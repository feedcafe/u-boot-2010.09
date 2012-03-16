/*
 * drivers/usb/gadget/s3c2410_udc.c
 *
 * Based on gadget driver:
 * linux/drivers/usb/gadget/omap_udc.c
 * linux/drivers/usb/gadget/pxa25x_udc.c
 *
 * Samsung S3C24xx series on-chip full speed USB device controllers
 *
 * Copyright (C) 2011 Fudong Bai <fudongbai@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <common.h>
#include <malloc.h>
#include <asm/errno.h>
#include <linux/list.h>

#include <linux/usb.h>
#include <linux/usb/gadget.h>
#include <linux/usb/compat.h>

#include <asm/io.h>
#include <asm/arch/udc.h>
#include <asm/arch/regs-udc.h>
#include <asm/arch/s3c24x0_cpu.h>

#define	DEBUG

#ifdef	DEBUG
#undef debug
#undef debugX
#define debug(fmt, args...)	serial_printf(fmt, ##args)
#define debugX(level, fmt, args...) \
	if (DEBUG >= level) serial_printf(fmt, ##args)
#endif
#define usbinfo(fmt, args...)	serial_printf(fmt, ##args)
#define usberr(fmt, args...)	serial_printf("ERROR: " fmt, ##args)


#define S3C2410_UDC_NUM_ENDPOINTS	5

struct s3c2410_req {
	struct usb_request		req;
	struct list_head		queue;
};

struct s3c2410_ep {
	struct usb_ep			ep;
	struct s3c2410_udc		*dev;
	struct list_head		queue;
	const struct usb_endpoint_descriptor *desc;
	char				name[14];
	u16				maxpacket;
	u8				bEndpointAddress;
	u8				bmAttributes;

	u8				idx;
};

struct s3c2410_udc {
	struct usb_gadget		gadget;
	struct usb_gadget_driver	*driver;
	struct s3c2410_ep		ep[S3C2410_UDC_NUM_ENDPOINTS];
	struct s3c2410_udc_mach_info	*udc_info;
};

static struct s3c2410_udc *the_controller;

static const char driver_name[] = "s3c2410_udc";
static const char ep0name[] = "ep0";

/*------------------------------ s3c2410_ep_ops -------------------------*/
static int s3c2410_ep_enable(struct usb_ep *_ep,
		const struct usb_endpoint_descriptor *desc)
{
	struct s3c2410_ep *ep;

	ep = container_of(_ep, struct s3c2410_ep, ep);
	if (!_ep || !desc || ep->desc || _ep->name == ep0name
			|| desc->bDescriptorType != USB_DT_ENDPOINT) {
		usberr("%s, bad ep or descriptor\n", __func__);
		return -EINVAL;
	}

	ep->desc = desc;
	ep->ep.maxpacket = le16_to_cpu(desc->wMaxPacketSize);

	usbinfo("%s enabled\n", _ep->name);

	return 0;
}

static int s3c2410_ep_disable(struct usb_ep *_ep)
{
	struct s3c2410_ep *ep;

	ep = container_of(_ep, struct s3c2410_ep, ep);
	if (!_ep || !ep->desc) {
		usberr("%s, %s not enabled\n", __func__,
				_ep ? ep->ep.name : NULL);
		return -EINVAL;
	}

	ep->desc = NULL;

	usbinfo("%s disabled\n", _ep->name);

	return 0;
}

static struct usb_request *
s3c2410_ep_alloc_request(struct usb_ep *ep, gfp_t gfp_flags)
{
	struct s3c2410_req *req;

	req = kzalloc(sizeof(*req), gfp_flags);
	if (!req)
		return NULL;

	INIT_LIST_HEAD(&req->queue);

	return &req->req;
}

static void
s3c2410_ep_free_request(struct usb_ep *ep, struct usb_request *_req)
{
	struct s3c2410_req *req;

	req = container_of(_req, struct s3c2410_req, req);

	kfree(req);
}

static int s3c2410_ep_queue(struct usb_ep *ep, struct usb_request *req,
				gfp_t gfp_flags)
{
	return 0;
}

static int s3c2410_ep_dequeue(struct usb_ep *ep, struct usb_request *req)
{
	return 0;
}

/**
 * set or clear endpoint halt feature
 * @ep: the non-isochronous endpoint being stalled
 * @value: 1--set halt 0--clear halt
 * Return zero, or a negative error code.
 */
static int s3c2410_ep_set_halt(struct usb_ep *ep, int value)
{
	return 0;
}

static int s3c2410_ep_fifo_status(struct usb_ep *_ep)
{
	struct s3c2410_ep	*ep;
	int			tmp;

	if (!_ep)
		return -ENODEV;

	ep = container_of(_ep, struct s3c2410_ep, ep);

	if (usb_endpoint_dir_in(ep->desc))
		return -EOPNOTSUPP;

	writeb(ep->idx, S3C2410_UDC_INDEX_REG);

	tmp = readb(S3C2410_UDC_OUT_FIFO_CNT2_REG) << 8;
	tmp |= readb(S3C2410_UDC_OUT_FIFO_CNT1_REG);

	return tmp & 0xffff;
}

static void s3c2410_ep_fifo_flush(struct usb_ep *ep)
{

}

static const struct usb_ep_ops s3c2410_ep_ops = {
	.enable		= s3c2410_ep_enable,
	.disable	= s3c2410_ep_disable,

	.alloc_request	= s3c2410_ep_alloc_request,
	.free_request	= s3c2410_ep_free_request,

	.queue		= s3c2410_ep_queue,
	.dequeue	= s3c2410_ep_dequeue,

	.set_halt	= s3c2410_ep_set_halt,
	.fifo_status	= s3c2410_ep_fifo_status,
	.fifo_flush	= s3c2410_ep_fifo_flush,
};


/*------------------------------ usb_gadget_ops -------------------------*/

static int s3c2410_udc_get_frame(struct usb_gadget *gadget)
{
	int tmp;

	tmp = readb(S3C2410_UDC_FRAME_NUM2_REG) << 8;
	tmp |= readb(S3C2410_UDC_FRAME_NUM1_REG);

	return tmp & 0xffff;
}

static int s3c2410_udc_wakeup(struct usb_gadget *gadget)
{
	return 0;
}

static int
s3c2410_udc_set_selfpowered(struct usb_gadget *gadget, int is_selfpowerd)
{
	return 0;
}

static int
s3c2410_udc_vbus_session(struct usb_gadget *gadget, int is_active)
{
	return 0;
}

static int s3c2410_udc_vbus_draw(struct usb_gadget *gadget, unsigned mA)
{
	return 0;
}

static void s3c2410_udc_enable(struct s3c2410_udc *dev);
static void s3c2410_udc_disable(struct s3c2410_udc *dev);

static int s3c2410_udc_pullup(struct usb_gadget *_gadget, int enable)
{
	struct s3c2410_udc	*udc;

	udc = container_of(_gadget, struct s3c2410_udc, gadget);

	if (!udc->udc_info->udc_command)
		return -EOPNOTSUPP;

	if (enable) {
		s3c2410_udc_enable(udc);
		udc->udc_info->udc_command(S3C2410_UDC_CMD_CONNECT);
	} else {
		udc->udc_info->udc_command(S3C2410_UDC_CMD_DISCONNECT);
		s3c2410_udc_disable(udc);
	}

	return 0;
}

static const struct usb_gadget_ops s3c2410_ops = {
	.get_frame		= s3c2410_udc_get_frame,
	.wakeup			= s3c2410_udc_wakeup,
	.set_selfpowered	= s3c2410_udc_set_selfpowered,
	.vbus_session		= s3c2410_udc_vbus_session,
	.vbus_draw		= s3c2410_udc_vbus_draw,
	.pullup			= s3c2410_udc_pullup,
};

static void s3c2410_udc_enable(struct s3c2410_udc *dev)
{
	struct s3c24x0_clock_power * const clk_power = s3c24x0_get_base_clock_power();
	struct s3c24x0_interrupt *irq = s3c24x0_get_base_interrupt();
	struct s3c24x0_gpio * const gpio = s3c24x0_get_base_gpio();

	/* Configure USB pads to device mode */
	gpio->MISCCR &= ~(S3C2410_MISCCR_USBHOST|S3C2410_MISCCR_USBSUSPND1);

	/* don't disable USB clock */
	clk_power->CLKSLOW &= ~S3C2410_CLKSLOW_UCLK_OFF;

	clk_power->CLKCON |= (1 << 7);

	/* clear interrupt registers before enable udc */
	writeb(0xff, S3C2410_UDC_EP_INT_REG);
	writeb(0xff, S3C2410_UDC_USB_INT_REG);

	irq->INTMSK &= ~BIT_USBD;

	/* Enable USB interrrupts for RESET and SUSPEND/RESUME */
	writeb(S3C2410_UDC_USBINT_RESET | S3C2410_UDC_USBINT_SUSPEND,
			S3C2410_UDC_USB_INT_EN_REG);

	dev->gadget.speed = USB_SPEED_UNKNOWN;
}

static void s3c2410_udc_disable(struct s3c2410_udc *dev)
{
	struct s3c24x0_interrupt *irq = s3c24x0_get_base_interrupt();

	irq->INTMSK |= BIT_USBD;
}

static int s3c2410_handle_ep0(struct s3c2410_udc *udc)
{
	return 0;
}

int s3c2410_udc_irq(void)
{
	struct s3c2410_udc *dev = the_controller;
	u8 usb_status;
	u8 usbd_status;

	usb_status = readb(S3C2410_UDC_USB_INT_REG);
	usbd_status = readb(S3C2410_UDC_EP_INT_REG);

	writeb(usb_status, S3C2410_UDC_USB_INT_REG);

	//debug("usb_status: %02x, usbd_status: %02x\n", usb_status, usbd_status);

	if (usb_status & S3C2410_UDC_USBINT_RESET) {
		dev->gadget.speed = USB_SPEED_FULL;
		debug("usb reset\n");
	}

	if (usb_status & S3C2410_UDC_USBINT_RESUME) {
		debug("usb resume\n");
	}

	if (usb_status & S3C2410_UDC_USBINT_SUSPEND) {
		debug("usb suspend\n");
	}

	if (usbd_status & S3C2410_UDC_INT_EP0) {
		writeb(S3C2410_UDC_INT_EP0, S3C2410_UDC_EP_INT_REG);
		s3c2410_handle_ep0(dev);
		debug("ep0 irq\n");
	}

	//usbinfo("%s\n", __func__);

	return 0;
}

int usb_gadget_handle_interrupts(void)
{
	return 0;
}

int usb_gadget_register_driver(struct usb_gadget_driver *driver)
{
	struct s3c2410_udc	*dev = the_controller;
	int			ret;

	if (!driver
			|| driver->speed < USB_SPEED_FULL
			|| !driver->bind
			|| !driver->disconnect
			|| !driver->setup)
		return -EINVAL;
	if (!dev)
		return -ENODEV;
	if (dev->driver)
		return -EBUSY;

	/* first hook up the driver ... */
	dev->driver = driver;

	ret = driver->bind(&dev->gadget);
	if (ret) {
		usberr("bind gadget: %s failed\n", dev->gadget.name);
		goto exit_bind_failed;
	}

	usbinfo("registered gadget driver '%s'\n", dev->gadget.name);

	return 0;

exit_bind_failed:
	dev->driver = NULL;

	return ret;
}

int usb_gadget_unregister_driver(struct usb_gadget_driver *driver)
{
	struct s3c2410_udc	*dev = the_controller;

	if (!dev)
		return -ENODEV;
	if (!driver || driver != dev->driver || !driver->unbind)
		return -EINVAL;

	driver->unbind(&dev->gadget);
	dev->driver = NULL;

	usbinfo("unregistered gadget driver '%s'\n", dev->gadget.name);

	return 0;
}

static struct s3c2410_udc memory = {
	.gadget = {
		.ops	= &s3c2410_ops,
		.ep0	= &memory.ep[0].ep,
		.name	= driver_name,
	},

	.ep[0] = {
		.ep = {
			.name		= ep0name,
			.ops		= &s3c2410_ep_ops,
			.maxpacket	= EP0_FIFO_SIZE,
		},
	},

	.ep[1] = {
		.ep = {
			.name		= "ep1-bulk",
			.ops		= &s3c2410_ep_ops,
			.maxpacket	= EP_FIFO_SIZE,
		},
		.dev = &memory,
		.bEndpointAddress	= 1,
		.bmAttributes		= USB_ENDPOINT_XFER_BULK,
	},

	.ep[2] = {
		.ep = {
			.name		= "ep2-bulk",
			.ops		= &s3c2410_ep_ops,
			.maxpacket	= EP_FIFO_SIZE,
		},
		.dev = &memory,
		.bEndpointAddress	= 2,
		.bmAttributes		= USB_ENDPOINT_XFER_BULK,
	},

	.ep[3] = {
		.ep = {
			.name		= "ep3-bulk",
			.ops		= &s3c2410_ep_ops,
			.maxpacket	= EP_FIFO_SIZE,
		},
		.dev = &memory,
		.bEndpointAddress	= 3,
		.bmAttributes		= USB_ENDPOINT_XFER_BULK,
	},

	.ep[4] = {
		.ep = {
			.name		= "ep4-bulk",
			.ops		= &s3c2410_ep_ops,
			.maxpacket	= EP_FIFO_SIZE,
		},
		.dev = &memory,
		.bEndpointAddress	= 4,
		.bmAttributes		= USB_ENDPOINT_XFER_BULK,
	},
};

static void s3c2410_udc_reinit(struct s3c2410_udc *dev)
{
	u32	i;

	INIT_LIST_HEAD(&dev->gadget.ep_list);
	INIT_LIST_HEAD(&dev->gadget.ep0->ep_list);

	for (i = 0; i < S3C2410_UDC_NUM_ENDPOINTS; i++) {
		struct s3c2410_ep *ep = &dev->ep[i];

		if (i != 0)
			list_add_tail(&ep->ep.ep_list, &dev->gadget.ep_list);
		ep->desc = NULL;
		INIT_LIST_HEAD(&ep->queue);
	}

	/* the rest was statically initialized, and is read-only */
}

int s3c2410_udc_probe(struct s3c2410_plat_udc_data *pdata)
{
	struct s3c2410_udc *dev = &memory;

	dev->udc_info = pdata->udc_info;

	s3c2410_udc_reinit(dev);

	the_controller = dev;

	usbinfo("%s\n", __func__);

	return 0;
}
