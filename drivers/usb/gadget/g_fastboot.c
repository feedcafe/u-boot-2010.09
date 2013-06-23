/*
 * Copyright (C) 2008 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * g_fastboot.c -- Android Fastboot Gadget Driver
 *
 * Based on
 * bootable_bootloader_legacy/usbloader/usbloader.c
 *
 * and
 *
 * drivers/usb/gadget/g_fastboot.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307	 USA
 *
 */

#include <common.h>
#include <malloc.h>
#include <asm/errno.h>
#include <asm/io.h>
#include <asm/arch/s3c24x0_cpu.h>

#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/usb/compat.h>
#include <linux/mtd/mtd.h>
#include <jffs2/load_kernel.h>

#include <nand.h>

//#define DEBUG

#ifdef DEBUG
#undef debug
#define debug(fmt, args...)	serial_printf(fmt, ##args)
#endif

#undef debugX
#define debugX(fmt, args...)	serial_printf(fmt, ##args)

#define STRING_MANUFACTURER	1
#define STRING_PRODUCT		2
#define STRING_SERIAL		3

#define FASTBOOT_VENDOR_ID	0x18d1
#define FASTBOOT_PRODUCT_ID	0xbabe

#define FASTBOOT_VERSION	"0.5"

#define DRIVER_DESC		"Android 1.0"
#define DRIVER_MANUFACTURER	"Google, Inc"

/* big enough to hold our biggest descriptor */
#define USB_BUFSIZ		256
#define USB_DATA_SIZE		4096

/* fastboot has only one configuration, it's 1 */
#define FASTBOOT_CONFIG		0x01

/* for do_reset() prototype */
extern int do_reset(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[]);

extern int do_bootm(cmd_tbl_t *, int, int, char * const []);

struct fastboot_dev {
	struct usb_gadget	*gadget;
	struct usb_request	*req;

	u8			config;
	struct usb_ep		*out_ep;
	struct usb_ep		*in_ep;

	struct usb_request	*rx_req;
	struct usb_request	*tx_req;
};

static char	manufacturer[64] = DRIVER_MANUFACTURER;
static char	product_desc[30] = DRIVER_DESC;
static char	serial[20] = "20120115";

static unsigned rx_addr;
static unsigned rx_length;

unsigned kernel_addr = CONFIG_SYS_LOAD_ADDR;
unsigned kernel_size = 0;

static u8 ctrl_req[USB_BUFSIZ];

/* Static strings, in UTF-8 (for simplicity we use only ASCII characters */
static struct usb_string strings[] = {
	{ STRING_MANUFACTURER,	manufacturer, },
	{ STRING_PRODUCT,	product_desc, },
	{ STRING_SERIAL,	serial, },
	{ }
};

static struct usb_gadget_strings stringtab = {
	.language	= 0x0409,	/* en_US */
	.strings	= strings,
};

static struct usb_device_descriptor device_desc = {
	.bLength		= sizeof device_desc,
	.bDescriptorType	= USB_DT_DEVICE,

	.bcdUSB			= __constant_cpu_to_le16(0x0110),

	.bDeviceClass		= USB_CLASS_VENDOR_SPEC,
	.bDeviceSubClass	= 0x00,
	.bDeviceProtocol	= 0x00,

	.idVendor		= __constant_cpu_to_le16(FASTBOOT_VENDOR_ID),
	.idProduct		= __constant_cpu_to_le16(FASTBOOT_PRODUCT_ID),

	.iManufacturer		= STRING_MANUFACTURER,
	.iProduct		= STRING_PRODUCT,
	.iSerialNumber		= STRING_SERIAL,

	.bNumConfigurations	= 1,
};

static struct usb_config_descriptor config_desc = {
	.bLength		= sizeof config_desc,
	.bDescriptorType	= USB_DT_CONFIG,

	/* compute wTotalLength on the fly */
	.bNumInterfaces		= 0x01,
	.bConfigurationValue	= FASTBOOT_CONFIG,
	.iConfiguration		= 0x00,
	.bmAttributes		= USB_CONFIG_ATT_ONE | USB_CONFIG_ATT_SELFPOWER,
	.bMaxPower		= 0x80,
};

static struct usb_interface_descriptor intf_desc = {
	.bLength		= sizeof intf_desc,
	.bDescriptorType	= USB_DT_INTERFACE,
	.bInterfaceNumber	= 0x00,
	.bNumEndpoints		= 0x02,
	.bInterfaceClass	= 0xFF,
	.bInterfaceSubClass	= 0x42,
	.bInterfaceProtocol	= 0x03,
	.iInterface		= 0x00,
};

static struct usb_endpoint_descriptor bulk_in_desc = {
	.bLength		= USB_DT_ENDPOINT_SIZE,
	.bDescriptorType	= USB_DT_ENDPOINT,

	.bEndpointAddress	= USB_DIR_IN,
	.bmAttributes		= USB_ENDPOINT_XFER_BULK,
	/* wMaxPacketSize set by autoconfiguration */
	.bInterval		= 0x00,
};

static struct usb_endpoint_descriptor bulk_out_desc = {
	.bLength		= USB_DT_ENDPOINT_SIZE,
	.bDescriptorType	= USB_DT_ENDPOINT,

	.bEndpointAddress	= USB_DIR_OUT,
	.bmAttributes		= USB_ENDPOINT_XFER_BULK,
	/* wMaxPacketSize set by autoconfiguration */
	.bInterval		= 0x01,
};

static const struct usb_descriptor_header *fs_function[] = {
	(struct usb_descriptor_header *) &intf_desc,
	(struct usb_descriptor_header *) &bulk_in_desc,
	/* bulk out endpoint */
	(struct usb_descriptor_header *) &bulk_out_desc,
	NULL,
};

static void usb_rx_cmd_complete(struct usb_ep *ep, struct usb_request *req);
static void usb_rx_data_complete(struct usb_ep *ep, struct usb_request *req);
static void usb_tx_status_complete(struct usb_ep *ep, struct usb_request *req);

static void rx_cmd(struct fastboot_dev *dev)
{
	struct usb_request *req = dev->rx_req;
	int err;

	debug("%s\n", __func__);

	req->length = USB_DATA_SIZE;
	req->complete = usb_rx_cmd_complete;
	err = usb_ep_queue(dev->out_ep, req, GFP_ATOMIC);
	if (err)
		error("%s queue req: %d\n", dev->out_ep->name, err);
}

static void rx_data(struct fastboot_dev *dev)
{
	struct usb_request *req = dev->rx_req;
	int err;

	debug("%s\n", __func__);

	req->buf = (void *) rx_addr;
	req->length = (rx_length > USB_DATA_SIZE) ? USB_DATA_SIZE : rx_length;
	req->complete = usb_rx_data_complete;
	err = usb_ep_queue(dev->out_ep, req, GFP_ATOMIC);
	if (err)
		error("%s queue req: %d\n", dev->out_ep->name, err);
}

static void tx_status(struct fastboot_dev *dev, const char *status)
{
	struct usb_request *req = dev->tx_req;
	int err;

	int len = strlen(status);

	memcpy(req->buf, status, len);
	req->length = len;
	req->complete = usb_tx_status_complete;
	err = usb_ep_queue(dev->in_ep, req, GFP_ATOMIC);
	if (err)
		error("%s queue req: %d\n", dev->out_ep->name, err);
}

static void usb_rx_data_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct fastboot_dev *dev = ep->driver_data;

	debug("%s, addr= %x len=%x\n", __func__, rx_addr, rx_length);

	if (req->status != 0)
		return;

	if (req->actual > rx_length)
		req->actual = rx_length;

	rx_addr += req->actual;
	rx_length -= req->actual;

	if (rx_length > 0) {
		rx_data(dev);
	} else {
		tx_status(dev, "OKAY");
		rx_cmd(dev);
	}
}

static void num_to_hex8(unsigned n, char *out)
{
	static char tohex[16] = "0123456789abcdef";
	int i;

	for (i = 7; i >= 0; i--) {
		out[i] = tohex[n & 15];
		n >>= 4;
	}
	out[8] = 0;
}

static int fastboot_nand_erase(const char *name)
{
	struct mtd_device	*dev;
	struct part_info	*part;
	u8			pnum;

	nand_erase_options_t opts;

	if (find_dev_and_part(name, &dev, &pnum, &part)) {
		printf("Partition %s not found!\n", name);
		return -ENODEV;
	}
	printf("erasing '%s'\n", part->name);

	memset(&opts, 0, sizeof(opts));
	opts.length = part->size;
	opts.offset = part->offset;
	opts.quiet = 0;
	opts.jffs2 = 0;
	opts.scrub = 0;

	if (nand_erase_opts(&nand_info[0], &opts))
		return -EINVAL;
	printf("partition '%s' erased\n", part->name);
	printf("from %x size: %x\n", part->offset, part->size);

	return 0;
}

static int fastboot_nand_write(const char *name)
{
	struct mtd_device	*dev;
	struct part_info	*part;
	u8			pnum;

	if (find_dev_and_part(name, &dev, &pnum, &part)) {
		printf("Partition %s not found!\n", name);
		return -ENODEV;
	}

	return nand_write_skip_bad(&nand_info[0], part->offset,
					&part->size, (u8 *)kernel_addr);
}

static int fastboot_image_is_boot(void)
{
	u8 *tmp;

	/* TODO find a better way to figure out image is U-Boot or uImage */
	tmp = (u8 *)kernel_addr;
	if ((tmp[0x800 + 3] == 0xea) && (tmp[0x800 + 5] == 0xf0))
		return 1;
	return 0;
}

static int fastboot_boot_linux(struct usb_gadget *gadget)
{
	if (fastboot_image_is_boot()) {
		printf("Starting at 0x%08x ...\n", kernel_addr + 0x800);
		do_go_exec((void *) kernel_addr + 0x800);
	} else {
		char addr[16];
		sprintf(addr, "%08x", kernel_addr + 0x800);

		printf("booting linux at 0x%s ...\n", addr);
		setenv("loadaddr", addr);

		usb_gadget_disconnect(gadget);

		do_bootm(NULL, 0, 0, NULL);
	}

	return 0;
}

static void usb_rx_cmd_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct fastboot_dev	*dev = ep->driver_data;
	char *cmdbuf;

	cmdbuf = req->buf;

	debug("%s, cmd: %s\n", __func__, cmdbuf);
	debug("%s, cmd: %p %p, %d\n", __func__, cmdbuf, dev, req->status);

	if (req->status != 0)
		return;

	if (req->actual > USB_DATA_SIZE)
		req->actual = USB_DATA_SIZE;
	cmdbuf[req->actual] = 0;

	if (memcmp(cmdbuf, "reboot", 6) == 0) {
		tx_status(dev, "OKAY");
		rx_cmd(dev);
		udelay(100000);
		do_reset(NULL, 0, 0, NULL);
	}

	if (memcmp(cmdbuf, "getvar:", 7) == 0) {
		char resp[64];

		strcpy(resp, "OKAY");

		if (!strcmp(cmdbuf + 7, "version"))
			strcpy(resp + 4, FASTBOOT_VERSION);
		else if (!strcmp(cmdbuf + 7, "product"))
			strcpy(resp + 4, product_desc);
		else if (!strcmp(cmdbuf + 7, "serialno"))
			strcpy(resp + 4, serial);
		else
			;
		tx_status(dev, resp);
		rx_cmd(dev);
		return;
	}

	if (memcmp(cmdbuf, "download:", 9) == 0) {
		char status[16];

		rx_addr = kernel_addr;
		rx_length = (unsigned)simple_strtoul(cmdbuf + 9, NULL, 16);
		if (rx_length > (64 * 1024 * 1024)) {
			tx_status(dev, "FAILdata too large");
			rx_cmd(dev);
			return;
		}
		kernel_size = rx_length;

		debugX("recv data: addr=%x size=%x\n", rx_addr, rx_length);
		strncpy(status, "DATA", 4);
		num_to_hex8(rx_length, status + 4);
		tx_status(dev, status);
		rx_data(dev);
		return;
	}

	if (memcmp(cmdbuf, "erase:", 6) == 0) {
		if (fastboot_nand_erase(cmdbuf + 6)) {
			tx_status(dev, "FAILfailed to erase partition");
			rx_cmd(dev);
			printf("- FAIL\n");
			return;
		}
		tx_status(dev, "OKAY");
		rx_cmd(dev);
		return;
	}

	if (memcmp(cmdbuf, "flash:", 6) == 0) {
		struct mtd_device	*mtddev;
		struct part_info	*part;
		u8 pnum;

		if (kernel_size == 0) {
			tx_status(dev, "FAILno image downloaded");
			rx_cmd(dev);
			return;
		}

		if (find_dev_and_part(cmdbuf + 6, &mtddev, &pnum, &part)) {
			tx_status(dev, "FAILpartition does not exist");
			rx_cmd(dev);
			return;
		}

#if 0
		if (!strcmp(part->name, "boot") || !strcmp(part->name, "recovery")) {
			if (memcmp((void *)kernel_addr, BOOT_MAGIC, BOOT_MAGIC_SIZE)) {
				tx_status(dev, "FAILimage is not a boot image");
				rx_cmd(dev);
				return;
			}
		}


		if (!strcmp(part->name, "system") || !strcmp(part->name, "userdata"))
			extra = 64;
		else
			kernel_size = (kernel_size + 2047) & (~2047);
#endif

		printf("writing '%s' (%d bytes)", part->name, kernel_size);
		if (fastboot_nand_write(part->name)) {
			tx_status(dev, "FAILnand write failed");
			rx_cmd(dev);
			return;
		} else {
			printf(" - OKAY");
		}

		tx_status(dev, "OKAY");
		rx_cmd(dev);
		return;
	}

	if (memcmp(cmdbuf, "boot", 4) == 0) {
		debug("boot not working yet\n");
		tx_status(dev, "OKAY");
		udelay(10000);
		fastboot_boot_linux(dev->gadget);
		return;
	}

	/*
	 * TODO: need to add flashall, update command
	 */

	tx_status(dev, "FAILinvalid command");
	rx_cmd(dev);
}

static void usb_tx_status_complete(struct usb_ep *ep, struct usb_request *req)
{
	if (req->status || req->actual != req->length)
		debug("setup complete --> %d, %d/%d\n",
				req->status, req->actual, req->length);
}

static int config_buf(struct usb_gadget *gadget,
		u8 *buf, u8 type, unsigned index)
{
	int len;

	if (index >= device_desc.bNumConfigurations)
		return -EINVAL;

	len = usb_gadget_config_buf(&config_desc, buf, USB_BUFSIZ, fs_function);
	if (len < 0)
		return len;

	((struct usb_config_descriptor *) buf)->bDescriptorType = type;

	return len;
}

static void fastboot_setup_complete(struct usb_ep *ep, struct usb_request *req)
{
	if (req->status || req->actual != req->length)
		debug("setup complete --> %d, %d/%d\n",
				req->status, req->actual, req->length);
}

#if 0
static void fastboot_rx_complete(struct usb_ep *ep, struct usb_request *req)
{
	int status = req->status;

	debug("%s, status: %d\n", __func__, status);

	switch (status) {
	case 0:
		/* bulk data received from host, deal with it and queue
		 * later for next transfer, see below
		 */
		fastboot_handle_bulk_out(ep, req);
		break;
	case -ECONNABORTED:	/* hardware forced ep reset */
	case -ECONNRESET:	/* request dequeued */
	case -ESHUTDOWN:	/* disconnected from host */
		debug("%s ep shutdown --> %d, %d/%d\n", ep->name,
				status, req->actual, req->length);
		kfree(req->buf);
		usb_ep_free_request(ep, req);
		return;

	case -EOVERFLOW:	/* buffer overrun on read means that
				 * we did not provide a big enough buffer
				 */
		break;

	default:
		debug("%s complete --> %d, %d/%d\n", ep->name,
				status, req->actual, req->length);
	}

	status = usb_ep_queue(ep, req, GFP_ATOMIC);
	if (status) {
		error("kill %s: resubmit %d bytes --> %d\n",
				ep->name, req->length, status);
		usb_ep_set_halt(ep);
		/* FIXME recover later ... somehow */
	}

}
#endif

static void fastboot_reset_config(struct fastboot_dev *dev)
{
	if (dev->config == 0)
		return;
	debug("%s\n", __func__);

	/* disable endpoints, forcing completion of pending i/o */
	usb_ep_disable(dev->out_ep);
	if (dev->rx_req) {
		usb_ep_free_request(dev->out_ep, dev->rx_req);
		dev->rx_req = NULL;
	}

	dev->config = 0;
}

static struct usb_request *
fastboot_alloc_req(struct usb_ep *ep, unsigned len, gfp_t gfp_flags)
{
	struct usb_request	*req;
	req = usb_ep_alloc_request(ep, gfp_flags);
	if (req != NULL) {
		req->length = len;
		req->buf = kzalloc(len, gfp_flags);
		if (req->buf == NULL) {
			usb_ep_free_request(ep, req);
			return NULL;
		}
	}

	return req;
}

static int fastboot_set_cfg(struct fastboot_dev *dev)
{
	int err;

	err = usb_ep_enable(dev->in_ep, &bulk_in_desc);
	if (err) {
		error("enable ep %s failed(%d)\n", dev->in_ep->name, err);
		return err;
	}
	dev->in_ep->driver_data = dev;

	err = usb_ep_enable(dev->out_ep, &bulk_out_desc);
	if (err) {
		error("enable ep %s failed(%d)\n", dev->out_ep->name, err);
		return err;
	}
	dev->out_ep->driver_data = dev;

	/* allocate buffer for bulk in/out endpoint */

	dev->rx_req = fastboot_alloc_req(dev->out_ep, USB_DATA_SIZE, GFP_KERNEL);
	if (!dev->rx_req)
		return -ENOMEM;

	dev->tx_req = fastboot_alloc_req(dev->in_ep, USB_BUFSIZ, GFP_KERNEL);
	if (!dev->tx_req)
		return -ENOMEM;

	dev->rx_req->complete = usb_rx_cmd_complete;
	err = usb_ep_queue(dev->out_ep, dev->rx_req, GFP_ATOMIC);
	if (err)
		error("%s queue req: %d\n", dev->out_ep->name, err);

	return 0;
}

static int fastboot_set_config(struct fastboot_dev *dev, unsigned number)
{
	int	result = 0;

	fastboot_reset_config(dev);

	switch (number) {
	case FASTBOOT_CONFIG:
		result = fastboot_set_cfg(dev);
		break;
	default:
		result = -EINVAL;
	case 0:
		break;
	}

	if (result) {
		usb_gadget_vbus_draw(dev->gadget,
				dev->gadget->is_otg ? 8 : 100);
	} else {
		unsigned power;

		power = 2 * config_desc.bMaxPower;
		usb_gadget_vbus_draw(dev->gadget, power);
	}

	return result;
}

static int
fastboot_setup(struct usb_gadget *gadget, const struct usb_ctrlrequest *ctrl)
{
	struct fastboot_dev	*dev = get_gadget_data(gadget);
	struct usb_request	*req = dev->req;
	int			value = -EOPNOTSUPP;
	u16			wIndex = le16_to_cpu(ctrl->wIndex);
	u16			wValue = le16_to_cpu(ctrl->wValue);
	u16			wLength = le16_to_cpu(ctrl->wLength);

	debug("%s, dev: %p, req: %p\n", __func__, dev, req);
	debug("usb control req %02x.%02x v%04x i%04x l%02x\n",
			ctrl->bRequestType, ctrl->bRequest,
			wValue, wIndex, wLength);

	switch (ctrl->bRequest) {
	case USB_REQ_GET_DESCRIPTOR:
		if (ctrl->bRequestType != USB_DIR_IN)
			break;

		switch (wValue >> 8) {
		case USB_DT_DEVICE:
			value = min(wLength, (u16) sizeof(device_desc));
			memcpy(req->buf, &device_desc, value);
			break;

		case USB_DT_CONFIG:
			value = config_buf(gadget, req->buf,
					wValue >> 8,
					wValue & 0xff);
			if (value >= 0)
				value = min(wLength, (u16)value);
			break;

		case USB_DT_STRING:
			value = usb_gadget_get_string(&stringtab,
					wValue & 0xff, req->buf);
			if (value >= 0)
				value = min(wLength, (u16)value);
			break;
		}
		break;

	case USB_REQ_SET_CONFIGURATION:
		if (ctrl->bRequestType != 0)
			break;
		value = fastboot_set_config(dev, wValue);
		break;

	case USB_REQ_GET_CONFIGURATION:
		if (ctrl->bRequestType != USB_DIR_IN)
			break;
		*(u8 *)req->buf = dev->config;
		value = min(wLength, (u16) 1);
		break;

	default:
		debugX("usb control req %02x.%02x v%04x i%04x l%02x\n",
				ctrl->bRequestType, ctrl->bRequest,
				wValue, wIndex, wLength);
	}

	if (value >= 0) {
		debug("respond with data transfer before status phase\n");
		req->length = value;
		req->zero = value < wLength
			&& (value % gadget->ep0->maxpacket) == 0;
		value = usb_ep_queue(gadget->ep0, req, GFP_ATOMIC);
		if (value < 0) {
			debugX("ep_queue --> %d\n", value);
			req->status = 0;
			fastboot_setup_complete(gadget->ep0, req);
		}
	}

	return value;
}

static int fastboot_bind(struct usb_gadget *gadget)
{
	struct fastboot_dev	*dev;
	struct usb_ep		*in_ep, *out_ep;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	debug("%s dev: %p\n", __func__, dev);
	debug("%s %d\n", __func__, gadget->ep0->maxpacket);

	device_desc.bMaxPacketSize0 = gadget->ep0->maxpacket;
	usb_gadget_set_selfpowered(gadget);

	usb_ep_autoconfig_reset(gadget);

	in_ep = usb_ep_autoconfig(gadget, &bulk_in_desc);
	if (!in_ep) {
		error("autoconfig on %s failed\n", gadget->name);
		return -ENODEV;
	}
	in_ep->driver_data = in_ep;	/* claim */
	dev->in_ep = in_ep;


	out_ep = usb_ep_autoconfig(gadget, &bulk_out_desc);
	if (!out_ep) {
		error("autoconfig on %s failed\n", gadget->name);
		return -ENODEV;
	}
	out_ep->driver_data = out_ep;	/* claim */
	dev->out_ep = out_ep;


	/* pre-allocate control message data and buffer */
	dev->req = usb_ep_alloc_request(gadget->ep0, GFP_KERNEL);
	if(!dev->req)
		return -ENOMEM;
	dev->req->buf = ctrl_req;
	dev->req->complete = fastboot_setup_complete;

	dev->gadget = gadget;
	set_gadget_data(gadget, dev);

	return 0;
}

static void fastboot_unbind(struct usb_gadget *gadget)
{
	struct fastboot_dev *dev = get_gadget_data(gadget);

	debug("%s\n", __func__);

	if (dev->rx_req) {
		usb_ep_free_request(dev->out_ep, dev->rx_req);
		dev->req = NULL;
	}

	dev->gadget = NULL;
	kfree(dev);
	set_gadget_data(gadget, NULL);
}

static void fastboot_disconnect(struct usb_gadget *gadget)
{
	struct fastboot_dev *dev = get_gadget_data(gadget);

	debug("%s\n", __func__);

	if (dev->config == 0)
		return;

	dev->config = 0;
}

static struct usb_gadget_driver fastboot_driver = {
#ifdef CONFIG_USB_GADGET_DUALSPEED
	.speed		= USB_SPEED_HIGH,
#else
	.speed		= USB_SPEED_FULL,
#endif
	.bind		= fastboot_bind,
	.unbind		= fastboot_unbind,

	.disconnect	= fastboot_disconnect,
	.setup		= fastboot_setup,
};

int fastboot_init(void)
{
	int ret;

	ret = usb_gadget_register_driver(&fastboot_driver);
	if (ret)
		return ret;

	ret = mtdparts_init();
	if (ret != 0) {
		printf("Error initializing mtdparts!\n");
		return ret;
	}

	return 0;
}
