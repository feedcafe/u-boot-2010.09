/*
 * (C) Copyright 2011
 * Bai fudongbai@gmail.com
 *
 * (C) Copyright 2000-2009
 * DENX Software Engineering
 * Wolfgang Denk, wd@denx.de
 *
 * Based on:
 * qt2410_boot_usb - Ram Loader for Armzone QT2410 Devel Boards
 * (C) 2006 by Harald Welte <hwelte@hmw-consulting.de>
 *
 * original file can be found at:
 * http://svn.openmoko.org/trunk/src/host/s3c2410_boot_usb/boot_usb.c
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */ 

#include <stdio.h>
#include <errno.h>
#include <usb.h>

#define USBD_VENDOR_ID	0x1987
#define USBD_PRODUCT_ID	0x0827

static struct usb_device *find_usbled_device(void)
{
	struct usb_bus *bus;

	for (bus = usb_busses; bus; bus = bus->next) {
		struct usb_device *dev;
		for (dev = bus->devices; dev; dev = dev->next) {
			if (dev->descriptor.idVendor == USBD_VENDOR_ID
				&&dev->descriptor.idProduct == USBD_PRODUCT_ID)
			return dev;

		}
	}
	return NULL;
}

static int usb_led_on(usb_dev_handle *hdl, int val)
{
	int ret;

	ret = usb_control_msg(hdl,
		/* bmRequestType */	USB_ENDPOINT_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
		/* bRequest	 */	0x12,
		/* wValue	 */	0,
		/* wIndex	 */	val,
		/* Data		 */	NULL,
		/* wLength	 */	0,
					2000);
	return 0;
}

int main()
{
	struct usb_device *dev;
	struct usb_dev_handle *hdl;
	int i;

	usb_init();

	if (!usb_find_busses())
		return -ENODEV;
	if (!usb_find_devices())
		return -ENODEV;

	dev = find_usbled_device();
	if (!dev) {
		printf("No usb led device found\n");
		return -ENODEV;
	}

	hdl = usb_open(dev);
	if (!hdl) {
		printf("Open usb device failed: %s\n", usb_strerror());
		return errno;
	}

	if (usb_claim_interface(hdl, 0) < 0) {
		printf("Unable to claim usb interface 1 of device: %s\n",
				usb_strerror());
		return errno;
	}

	printf("on-board LEDs is countting down\n");

	for (i = 1; i < 16; i++) {
		usb_led_on(hdl, i);
		sleep(1);
	}

	for (i = 15; i > 0; i--) {
		usb_led_on(hdl, i);
		sleep(1);
	}
	return 0;
}
