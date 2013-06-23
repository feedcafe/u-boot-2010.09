/*
 * g_fastboot.c -- Android Fastboot Gadget Driver
 *
 * Based on
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

#define DEBUG

#ifdef DEBUG
#undef debug
#define debug(fmt,args...)	serial_printf (fmt ,##args)
#endif

#undef debugX
#define debugX(fmt,args...)	serial_printf (fmt ,##args)

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

struct fastboot_dev {
	struct usb_gadget	*gadget;
	struct usb_request	*req;

	u8			config;
	struct usb_ep		*out_ep;
	struct usb_ep		*in_ep;

	struct usb_request	*rx_req;
	struct usb_request	*tx_req;
};

static char		manufacturer[64] = DRIVER_MANUFACTURER;
static char		product_desc[30] = DRIVER_DESC;
static char		serial[20] = "20120115";

static unsigned rx_addr;
static unsigned rx_length;

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

	//req->buf = cmdbuf;
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

	debug("%s\n", __func__);

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

	/*
	 * TODO: need to add erase, boot, flash, flashall, update command
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
	int			result = 0;

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
					wValue &0xff, req->buf);
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

	if (value >=0) {
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

	return 0;
}
