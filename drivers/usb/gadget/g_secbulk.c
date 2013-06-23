/*
 * Copyright (C) 2011
 *
 * Fudong Bai <fudongbai@gmail.com>
 *
 * Based on drivers/usb/gadget/g_usbled.c
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

#ifdef DEBUG
#undef debug
#define debug(fmt,args...)	serial_printf (fmt ,##args)
#endif

#undef debugX
#define debugX(fmt,args...)	serial_printf (fmt ,##args)

#define STRING_MANUFACTURER	1
#define STRING_PRODUCT		2
#define STRING_SERIAL		3

#define SECBULK_VENDOR_ID	0x5345
#define SECBULK_PRODUCT_ID	0x1234

#define DRIVER_DESC		"s3c24x0 secbulk gadget"
#define DRIVER_MANUFACTURER	"U-Boot"

/* big enough to hold our biggest descriptor */
#define USB_BUFSIZ		256

/* secbulk has only one configuration, it's 1 */
#define SECBULK_CONFIG		1

#define GFP_ATOMIC ((gfp_t) 0)
#define GFP_KERNEL ((gfp_t) 0)

struct secbulk_dev {
	struct usb_gadget	*gadget;
	struct usb_request	*req;

	u8			config;
	struct usb_ep		*out_ep;

	struct usb_request	*rx_req;
};

static char		manufacturer[64] = DRIVER_MANUFACTURER;
static char		product_desc[30] = DRIVER_DESC;
static char		serial[15] = "1";

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

static u8 ctrl_req[USB_BUFSIZ];


static struct usb_device_descriptor device_desc = {
	.bLength		= sizeof device_desc,
	.bDescriptorType	= USB_DT_DEVICE,

	.bcdUSB			= __constant_cpu_to_le16(0x0110),

	.bDeviceClass		= USB_CLASS_VENDOR_SPEC,
	.bDeviceSubClass	= 0x00,
	.bDeviceProtocol	= 0x00,

	.idVendor		= __constant_cpu_to_le16(SECBULK_VENDOR_ID),
	.idProduct		= __constant_cpu_to_le16(SECBULK_PRODUCT_ID),

	.iManufacturer		= STRING_MANUFACTURER,
	.iProduct		= STRING_PRODUCT,
	.iSerialNumber		= STRING_SERIAL,

	.bNumConfigurations	= 1,
};

static struct usb_config_descriptor config_desc = {
	.bLength		= sizeof config_desc,
	.bDescriptorType	= USB_DT_CONFIG,

	/* compute wTotalLength on the fly */
	.bNumInterfaces		= 1,
	.bConfigurationValue	= SECBULK_CONFIG,
	.iConfiguration		= 0,
	.bmAttributes		= USB_CONFIG_ATT_ONE | USB_CONFIG_ATT_SELFPOWER,
	.bMaxPower		= 1,
};

static struct usb_interface_descriptor intf_desc = {
	.bLength		= sizeof intf_desc,
	.bDescriptorType	= USB_DT_INTERFACE,
	.bInterfaceNumber	= 0,
	.bNumEndpoints		= 1,
	.bInterfaceClass	= 0xFF,
	.bInterfaceSubClass	= 0,
	.bInterfaceProtocol	= 0,
	.iInterface		= 0,
};

static struct usb_endpoint_descriptor bulk_out_desc = {
	.bLength		= USB_DT_ENDPOINT_SIZE,
	.bDescriptorType	= USB_DT_ENDPOINT,

	.bEndpointAddress	= USB_DIR_OUT,
	.bmAttributes		= USB_ENDPOINT_XFER_BULK,
	/* wMaxPacketSize set by autoconfiguration */
};

static const struct usb_descriptor_header *fs_function[] = {
	(struct usb_descriptor_header *) &intf_desc,
	/* bulk out endpoint */
	(struct usb_descriptor_header *) &bulk_out_desc,
	NULL,
};

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

static void secbulk_setup_complete(struct usb_ep *ep, struct usb_request *req)
{
	if (req->status || req->actual != req->length)
		debug("setup complete --> %d, %d/%d\n",
				req->status, req->actual, req->length);
}

static void secbulk_reset_config(struct secbulk_dev *dev)
{
	if (dev->config == 0)
		return;
	debug("%s\n", __func__);

	/* disable endpoints, forcing completion of pending i/o */
	usb_ep_disable(dev->out_ep);

	dev->config = 0;
}

static int secbulk_set_cfg(struct secbulk_dev *dev)
{
	int err = 0;

	err = usb_ep_enable(dev->out_ep, &bulk_out_desc);
	if (err) {
		error("enable ep %s failed(%d)\n", dev->out_ep->name, err);
		return err;
	}
	dev->out_ep->driver_data = dev;

	/* allocate buffer for bulk out endpoint */
	dev->rx_req = usb_ep_alloc_request(dev->out_ep, 0);
	if (!dev->rx_req)
		return -ENOMEM;

	return 0;
}

static int secbulk_set_config(struct secbulk_dev *dev, unsigned number)
{
	int			result = 0;

	secbulk_reset_config(dev);

	switch (number) {
	case SECBULK_CONFIG:
		result = secbulk_set_cfg(dev);
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
secbulk_setup(struct usb_gadget *gadget, const struct usb_ctrlrequest *ctrl)
{
	struct secbulk_dev	*dev = get_gadget_data(gadget);
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
		value = secbulk_set_config(dev, wValue);
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
			secbulk_setup_complete(gadget->ep0, req);
		}
	}

	return value;
}

static int secbulk_bind(struct usb_gadget *gadget)
{
	struct secbulk_dev	*dev;
	struct usb_ep		*out_ep;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	debug("%s dev: %p\n", __func__, dev);
	debug("%s %d\n", __func__, gadget->ep0->maxpacket);

	device_desc.bMaxPacketSize0 = gadget->ep0->maxpacket;
	usb_gadget_set_selfpowered(gadget);

	usb_ep_autoconfig_reset(gadget);
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
	dev->req->complete = secbulk_setup_complete;

	dev->gadget = gadget;
	set_gadget_data(gadget, dev);

	return 0;
}

static void secbulk_unbind(struct usb_gadget *gadget)
{
	struct secbulk_dev *dev = get_gadget_data(gadget);

	debug("%s\n", __func__);

	if (dev->rx_req) {
		usb_ep_free_request(dev->out_ep, dev->rx_req);
		dev->req = NULL;
	}

	dev->gadget = NULL;
	kfree(dev);
	set_gadget_data(gadget, NULL);
}

static void secbulk_disconnect(struct usb_gadget *gadget)
{
	struct secbulk_dev *dev = get_gadget_data(gadget);

	debug("%s\n", __func__);

	if (dev->config == 0)
		return;

	dev->config = 0;
}

static struct usb_gadget_driver secbulk_driver = {
#ifdef CONFIG_USB_GADGET_DUALSPEED
	.speed		= USB_SPEED_HIGH,
#else
	.speed		= USB_SPEED_FULL,
#endif
	.bind		= secbulk_bind,
	.unbind		= secbulk_unbind,

	.disconnect	= secbulk_disconnect,
	.setup		= secbulk_setup,
};

int secbulk_init(void)
{
	int ret;

	ret = usb_gadget_register_driver(&secbulk_driver);
	if (ret)
		return ret;

	return 0;
}
