/*
 * Copyright (C) 2011
 *
 * Fudong Bai <fudongbai@gmail.com>
 *
 * Our four on-board LEDs are controlled by host-side usbled driver
 * through endpoint 0, the original driver is:
 * kernel/drivers/usb/misc/usbled.c
 *
 * Written by Greg Kroah-Hartman (greg@kroah.com)
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

#define USB_LED_VENDOR_ID	0x1987
#define USB_LED_PRODUCT_ID	0x0827

#define DRIVER_DESC		"usb led gadget"
#define DRIVER_MANUFACTURER	"U-Boot"

/* big enough to hold our biggest descriptor */
#define USB_BUFSIZ		256

#define GFP_ATOMIC ((gfp_t) 0)
#define GFP_KERNEL ((gfp_t) 0)

struct usb_led_dev {
	struct usb_gadget	*gadget;
	struct usb_request	*req;

	u8			config;
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

	.bDeviceClass		= USB_CLASS_PER_INTERFACE,
	.bDeviceSubClass	= 0,
	.bDeviceProtocol	= 0,

	.idVendor		= __constant_cpu_to_le16(USB_LED_VENDOR_ID),
	.idProduct		= __constant_cpu_to_le16(USB_LED_PRODUCT_ID),

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
	.bConfigurationValue	= 1,
	.iConfiguration		= 0,
	.bmAttributes		= USB_CONFIG_ATT_ONE | USB_CONFIG_ATT_SELFPOWER,
	.bMaxPower		= 1,
};

static struct usb_interface_descriptor intf_desc = {
	.bLength		= sizeof intf_desc,
	.bDescriptorType	= USB_DT_INTERFACE,
	.bInterfaceNumber	= 0,
	.bNumEndpoints		= 0,
	.bInterfaceClass	= 0,
	.bInterfaceSubClass	= 0,
	.bInterfaceProtocol	= 0,
	.iInterface		= 0,
};

static const struct usb_descriptor_header *fs_function[] = {
	(struct usb_descriptor_header *) &intf_desc,
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

static void usb_led_setup_complete(struct usb_ep *ep, struct usb_request *req)
{
	if (req->status || req->actual != req->length)
		debug("setup complete --> %d, %d/%d\n",
				req->status, req->actual, req->length);
}

static int
usb_led_setup(struct usb_gadget *gadget, const struct usb_ctrlrequest *ctrl)
{
	struct usb_led_dev	*dev = get_gadget_data(gadget);
	struct usb_request	*req = dev->req;
	int			value = -EOPNOTSUPP;
	u16			wIndex = le16_to_cpu(ctrl->wIndex);
	u16			wValue = le16_to_cpu(ctrl->wValue);
	u16			wLength = le16_to_cpu(ctrl->wLength);

	debug("%s, dev: %p, req: %p\n", __func__, dev, req);
	debugX("usb control req %02x.%02x v%04x i%04x l%d\n",
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
		debugX("not available!\n");
		break;

	case USB_REQ_GET_CONFIGURATION:
		if (ctrl->bRequestType != USB_DIR_IN)
			break;
		*(u8 *)req->buf = dev->config;
		value = min(wLength, (u16) 1);
		break;

	default:
		debugX("usb control req %02x.%02x v%04x i%04x l%d\n",
				ctrl->bRequestType, ctrl->bRequest,
				wValue, wIndex, wLength);
	}

	if (value >=0) {
		debugX("respond with data transfer before status phase\n");
		req->length = value;
		req->zero = value < wLength
			&& (value % gadget->ep0->maxpacket) == 0;
		value = usb_ep_queue(gadget->ep0, req, GFP_ATOMIC);
		if (value < 0) {
			debugX("ep_queue --> %d\n", value);
			req->status = 0;
			usb_led_setup_complete(gadget->ep0, req);
		}
	}

	return value;
}

static int usb_led_bind(struct usb_gadget *gadget)
{
	struct usb_led_dev *dev;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	debug("%s dev: %p\n", __func__, dev);
	debug("%s %d\n", __func__, gadget->ep0->maxpacket);

	device_desc.bMaxPacketSize0 = gadget->ep0->maxpacket;
	usb_gadget_set_selfpowered(gadget);

	device_desc.bNumConfigurations = 1;

	/* pre-allocate control message data and buffer */
	dev->req = usb_ep_alloc_request(gadget->ep0, GFP_KERNEL);
	if(!dev->req)
		return -ENOMEM;
	dev->req->buf = ctrl_req;
	dev->req->complete = usb_led_setup_complete;

	dev->gadget = gadget;
	set_gadget_data(gadget, dev);
	//gadget->ep0->driver_data = dev;

	return 0;
}

static void usb_led_unbind(struct usb_gadget *gadget)
{
	struct usb_led_dev *dev = get_gadget_data(gadget);

	debug("%s\n", __func__);

	kfree(dev);
	set_gadget_data(gadget, NULL);
}

static void usb_led_disconnect(struct usb_gadget *gadget)
{

}

static struct usb_gadget_driver usb_led_driver = {
#ifdef CONFIG_USB_GADGET_DUALSPEED
	.speed		= USB_SPEED_HIGH,
#else
	.speed		= USB_SPEED_FULL,
#endif
	.bind		= usb_led_bind,
	.unbind		= usb_led_unbind,

	.disconnect	= usb_led_disconnect,
	.setup		= usb_led_setup,
};

int usb_led_init(void)
{
	int ret;

	ret = usb_gadget_register_driver(&usb_led_driver);
	if (ret)
		return ret;

	return 0;
}
