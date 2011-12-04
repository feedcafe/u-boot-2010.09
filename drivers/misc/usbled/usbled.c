/*
 * USB LED driver - 1.1
 *
 * Copyright (C) 2004 Greg Kroah-Hartman (greg@kroah.com)
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License as
 *	published by the Free Software Foundation, version 2.
 *
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/usb.h>


#define DRIVER_AUTHOR "Greg Kroah-Hartman, greg@kroah.com"
#define DRIVER_DESC "USB LED Driver"

#define VENDOR_ID	0x1987
#define PRODUCT_ID	0x0727

/* table of devices that work with this driver */
static struct usb_device_id id_table [] = {
	{ USB_DEVICE(VENDOR_ID, PRODUCT_ID) },
	{ },
};
MODULE_DEVICE_TABLE (usb, id_table);

struct usb_led {
	struct usb_device *	udev;
	unsigned char		value;
};

static void turn_on_led(struct usb_led *led)
{
	int retval;

	retval = usb_control_msg(led->udev,
				usb_sndctrlpipe(led->udev, 0),
	/* bRequest	 */	0x12,
	/* bmRequestType */	0x40,
	/* wVlaue	 */	(0x02 * 0x100) + 0x0a,
	/* wIndex	 */	(0x00 * 0x100) + led->value,
	/* Data		 */	NULL,	
	/* wLength	 */	8,
				2000);
	if (retval)
		dev_dbg(&led->udev->dev, "retval = %d\n", retval);
}

#define show_set(value)	\
static ssize_t show_##value(struct device *dev, struct device_attribute *attr, char *buf)		\
{									\
	struct usb_interface *intf = to_usb_interface(dev);		\
	struct usb_led *led = usb_get_intfdata(intf);			\
									\
	return sprintf(buf, "%d\n", led->value);			\
}									\
static ssize_t set_##value(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)	\
{									\
	struct usb_interface *intf = to_usb_interface(dev);		\
	struct usb_led *led = usb_get_intfdata(intf);			\
	int temp = simple_strtoul(buf, NULL, 10);			\
									\
	led->value = temp;						\
	turn_on_led(led);						\
	return count;							\
}									\
static DEVICE_ATTR(value, S_IRUGO | S_IWUSR, show_##value, set_##value);
show_set(value);

static int led_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
	struct usb_device *udev = interface_to_usbdev(interface);
	struct usb_led *dev = NULL;
	int retval = -ENOMEM;

	dev = kzalloc(sizeof(struct usb_led), GFP_KERNEL);
	if (dev == NULL) {
		dev_err(&interface->dev, "Out of memory\n");
		goto error_mem;
	}

	dev->udev = usb_get_dev(udev);

	usb_set_intfdata (interface, dev);

	retval = device_create_file(&interface->dev, &dev_attr_value);
	if (retval)
		goto error;

	dev_info(&interface->dev, "USB LED device now attached\n");
	return 0;

error:
	device_remove_file(&interface->dev, &dev_attr_value);
	usb_set_intfdata (interface, NULL);
	usb_put_dev(dev->udev);
	kfree(dev);
error_mem:
	return retval;
}

static void led_disconnect(struct usb_interface *interface)
{
	struct usb_led *dev;

	dev = usb_get_intfdata (interface);

	device_remove_file(&interface->dev, &dev_attr_value);

	/* first remove the files, then set the pointer to NULL */
	usb_set_intfdata (interface, NULL);

	usb_put_dev(dev->udev);

	kfree(dev);

	dev_info(&interface->dev, "USB LED now disconnected\n");
}

static struct usb_driver led_driver = {
	.name =		"usbled",
	.probe =	led_probe,
	.disconnect =	led_disconnect,
	.id_table =	id_table,
};

static int __init usb_led_init(void)
{
	int retval = 0;

	retval = usb_register(&led_driver);
	if (retval)
		err("usb_register failed. Error number %d", retval);
	return retval;
}

static void __exit usb_led_exit(void)
{
	usb_deregister(&led_driver);
}

module_init (usb_led_init);
module_exit (usb_led_exit);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
