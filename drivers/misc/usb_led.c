/*
 * Copyright (C) 2011
 * Bai <fudongbai@gmail.com>
 *
 * Based on driver/serial/usbtty.c
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
#include <usbdevice.h>

#include "usb_led.h"

/* FIXME no gpio implemetation on S3C2440 */
#include <status_led.h>
//#include <asm/gpio.h>

#define NUM_CONFIGS	1
#define MAX_INTERFACES	2
#define NUM_ENDPOINTS	2

static struct usb_device_instance device_instance[1];
static struct usb_bus_instance bus_instance[1];
static struct usb_configuration_instance config_instance[NUM_CONFIGS];
static struct usb_interface_instance interface_instance[MAX_INTERFACES];
/* one extra for control endpoint */
static struct usb_endpoint_instance endpoint_instance[NUM_ENDPOINTS+1];


/* Standard USB Data Structures */
static struct usb_interface_descriptor interface_descriptors[MAX_INTERFACES];
static struct usb_endpoint_descriptor *ep_descriptor_ptrs[NUM_ENDPOINTS];
static struct usb_device_descriptor device_descriptor = {
	.bLength = sizeof(struct usb_device_descriptor),
	.bDescriptorType =	USB_DT_DEVICE,
	.bcdUSB =		cpu_to_le16(USB_BCD_VERSION),
	.bDeviceSubClass =	0x00,
	.bDeviceProtocol =	0x00,
	.bMaxPacketSize0 =	EP0_MAX_PACKET_SIZE,
	.idVendor =		cpu_to_le16(CONFIG_USBD_VENDORID),
	.idProduct =		cpu_to_le16(CONFIG_USBD_PRODUCTID_USBLED),
	.bcdDevice =		cpu_to_le16(USBLED_BCD_DEVICE),
	.iManufacturer =	STR_MANUFACTURER,
	.iProduct =		STR_PRODUCT,
	.iSerialNumber =	STR_SERIAL,
	.bNumConfigurations =	NUM_CONFIGS
};

static struct usb_configuration_descriptor config_descriptor = {
	.bLength = sizeof(struct usb_configuration_descriptor),
	.bDescriptorType =	USB_DT_CONFIG,
	.wTotalLength =		cpu_to_le16(sizeof(struct usb_configuration_descriptor)),
	.bNumInterfaces =	0,
	.bConfigurationValue =	1,
	.iConfiguration	=	0,
	.bmAttributes =		BMATTRIBUTE_SELF_POWERED | BMATTRIBUTE_RESERVED,
	.bMaxPower =		USBLED_MAXPOWER,	/* unit: 2mA */
};

/*
 * Static Function Prototypes
 */
static void usb_led_init_strings(void);
static void usb_led_init_instances(void);
static void usb_led_init_endpoints(void);

int usb_led_init(void)
{
	int rc;

	udc_init();	/* Basic USB initialization */
	usb_led_init_strings();
	usb_led_init_instances();
	udc_startup_events(device_instance);	/* Enable dev, init udc pointers */
	udc_connect();	/* Enable pullup for host detection */
#if 0
	usb_led_init_endpoints();
#endif

	return 0;
}

static void usb_led_init_strings(void)
{

}

static void usb_led_init_instances(void)
{
	int i;

	/* initialize device instance */
	memset (device_instance, 0, sizeof (struct usb_device_instance));
	device_instance->device_state = STATE_INIT;
	device_instance->device_descriptor = &device_descriptor;
	//device_instance->event = usbled_event_handler;
	//device_instance->cdc_recv_setup = usbled_cdc_setup;
	device_instance->bus = bus_instance;
	device_instance->configurations = NUM_CONFIGS;
	device_instance->configuration_instance_array = config_instance;

	/* initialize bus instance */
	memset (bus_instance, 0, sizeof (struct usb_bus_instance));
	bus_instance->device = device_instance;
	bus_instance->endpoint_array = endpoint_instance;
	bus_instance->max_endpoints = 1;
	bus_instance->maxpacketsize = 64;
	//bus_instance->serial_number_str = serial_number;

	/* endpoint instances */
	memset (&endpoint_instance[0], 0,
		sizeof (struct usb_endpoint_instance));
	endpoint_instance[0].endpoint_address = 0;
	endpoint_instance[0].rcv_packetSize = EP0_MAX_PACKET_SIZE;
	endpoint_instance[0].rcv_attributes = USB_ENDPOINT_XFER_CONTROL;
	endpoint_instance[0].tx_packetSize = EP0_MAX_PACKET_SIZE;
	endpoint_instance[0].tx_attributes = USB_ENDPOINT_XFER_CONTROL;
	udc_setup_ep (device_instance, 0, &endpoint_instance[0]);

	/* configuration instance */
	memset (config_instance, 0,
		sizeof (struct usb_configuration_instance));
	//config_instance->interfaces = interface_count;
	config_instance->configuration_descriptor = &config_descriptor;
	config_instance->interface_instance_array = interface_instance;
}

static void usb_led_init_endpoints(void)
{
	int i;

	bus_instance->max_endpoints = NUM_ENDPOINTS + 1;
	for (i = 1; i <= NUM_ENDPOINTS; i++) {
		udc_setup_ep (device_instance, i, &endpoint_instance[i]);
	}
}
