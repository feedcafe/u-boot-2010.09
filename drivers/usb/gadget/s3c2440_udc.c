/*
 * drivers/usb/gadget/s3c2410_udc.c
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
#include <asm/errno.h>
#include <linux/list.h>

#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>


/*------------------------------ s3c2410_ep_ops -------------------------*/
static int s3c2410_ep_enable(struct usb_ep *ep,
		const struct usb_endpoint_descriptor *desc)
{
	return 0;
}

static int s3c2410_ep_disable(struct usb_ep *ep)
{
	return 0;
}

static struct usb_request *
s3c2410_ep_alloc_request(struct usb_ep *ep, gfp_t gfp_flags)
{
	return NULL;
}

static void
s3c2410_ep_free_request(struct usb_ep *ep, struct usb_request *req)
{

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

static int s3c2410_ep_set_halt(struct usb_ep *ep, int value)
{
	return 0;
}

static int s3c2410_ep_fifo_status(struct usb_ep *ep)
{
	return 0;
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
	return 0;
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

static int s3c2410_udc_pullup(struct usb_gadget *gadget, int enable)
{
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

int usb_gadget_handle_interrupts(void)
{
	return 0;
}

int usb_gadget_register_driver(struct usb_gadget_driver *driver)
{
	return 0;
}

int usb_gadget_unregister_driver(struct usb_gadget_driver *driver)
{
	return 0;
}
