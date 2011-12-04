/*
 * Copyright (C) 2011
 * Bai <fudongbai@gmail.com>
 *
 * Based on driver/serial/usbtty.c which is
 *
 * (C) Copyright 2003
 * Gerry Hamel, geh@ti.com, Texas Instruments
 *
 * (C) Copyright 2006
 * Bryan O'Donoghue, bodonoghue@codehermit.ie
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
#include <asm/arch/s3c24x0_cpu.h>
#include <asm/io.h>

#include <usbdevice.h>

#include "usb_led.h"

#ifdef DEBUG
#undef debug
#define debug(fmt,args...)	serial_printf (fmt ,##args)
#endif

#define NUM_CONFIGS	1
#define MAX_INTERFACES	1
#define NUM_ENDPOINTS	1

static struct usb_device_instance device_instance[1];
static struct usb_bus_instance bus_instance[1];
static struct usb_configuration_instance config_instance[NUM_CONFIGS];
static struct usb_interface_instance interface_instance[MAX_INTERFACES];
/* one extra for control endpoint */
static struct usb_endpoint_instance endpoint_instance[NUM_ENDPOINTS+1];

/*
 * Serial number
 */
static char serial_number[16] = "usb led test";


/*
 * Descriptors, Strings, Local variables.
 */

/* defined and used by gadget/ep0.c */
extern struct usb_string_descriptor **usb_strings;

static struct usb_string_descriptor *usbled_string_tbl[STR_COUNT];

/* USB Descriptor Strings */
static u8 wstrLang[4] = {4, USB_DT_STRING, 0x9, 0x4};
static u8 wstrManufacturer[2 + 2*(sizeof(CONFIG_USBD_MANUFACTURER)-1)];
static u8 wstrProduct[2 + 2*(sizeof(CONFIG_USBD_PRODUCT_NAME)-1)];
static u8 wstrSerial[2 + 2*(sizeof(serial_number)-1)];


/* Standard USB Data Structures */
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

struct usbled_config_desc {
	struct usb_configuration_descriptor config_desc;

	struct usb_interface_descriptor interface_desc;
	struct usb_endpoint_descriptor endpoint_desc;
} __attribute__((packed));

static struct usbled_config_desc led_configuration_descriptors[NUM_CONFIGS] = {
	{
		.config_desc = {
			.bLength = sizeof(struct usb_configuration_descriptor),
			.bDescriptorType	= USB_DT_CONFIG,
			.wTotalLength		=
				cpu_to_le16(sizeof(struct usbled_config_desc)),
			.bNumInterfaces		= 1,
			.bConfigurationValue	= 1,
			.iConfiguration		= 0,
			.bmAttributes		=
				BMATTRIBUTE_SELF_POWERED | BMATTRIBUTE_RESERVED,
			.bMaxPower		= USBLED_MAXPOWER,	/* unit: 2mA */
		},
		.interface_desc = {
			.bLength = sizeof(struct usb_interface_descriptor),
			.bDescriptorType	= USB_DT_INTERFACE,
			.bInterfaceNumber	= 0,
			.bAlternateSetting	= 0,
			.bNumEndpoints		= 0x01,
			.bInterfaceClass	= 0,
			.bInterfaceSubClass	= 0,
			.bInterfaceProtocol	= 0,
			.iInterface		= 0,
		},
		.endpoint_desc = {
			.bLength		=
				sizeof(struct usb_endpoint_descriptor),
			.bDescriptorType	= USB_DT_ENDPOINT,
			.bEndpointAddress	= UDC_INT_ENDPOINT | USB_DIR_IN,
			.bmAttributes		= USB_ENDPOINT_XFER_INT,
			.wMaxPacketSize		=
				cpu_to_le16(CONFIG_USBD_LED_INT_PKTSIZE),
			.bInterval		= 0xFF,
		},
	},
};

/*
 * Static Function Prototypes
 */
static void usb_led_init_strings(void);
static void usb_led_init_instances(void);
static void usb_led_init_endpoints(void);
static void usb_led_event_handler(struct usb_device_instance *device,
				usb_device_event_t event, int data);
static int usb_led_cdc_setup(struct usb_device_request *request,
				struct urb *urb);

/* utility function for converting char* to wide string used by USB */
static void str2wide(char *str, u16 * wide)
{
	int i;
	for (i = 0; i < strlen (str) && str[i]; i++){
		#if defined(__LITTLE_ENDIAN)
			wide[i] = (u16) str[i];
		#elif defined(__BIG_ENDIAN)
			wide[i] = ((u16)(str[i])<<8);
		#else
			#error "__LITTLE_ENDIAN or __BIG_ENDIAN undefined"
		#endif
	}
}

int usb_led_init(void)
{
	udc_init();	/* Basic USB initialization */
	usb_led_init_strings();
	usb_led_init_instances();
	udc_startup_events(device_instance);	/* Enable dev, init udc pointers */
	udc_connect();	/* Enable pullup for host detection */
	usb_led_init_endpoints();

	return 0;
}

static void usb_led_init_strings(void)
{
	struct usb_string_descriptor *string;

	usbled_string_tbl[STR_LANG] =
		(struct usb_string_descriptor*)wstrLang;

	string = (struct usb_string_descriptor *) wstrManufacturer;
	string->bLength = sizeof(wstrManufacturer);
	string->bDescriptorType = USB_DT_STRING;
	str2wide(CONFIG_USBD_MANUFACTURER, string->wData);
	usbled_string_tbl[STR_MANUFACTURER] = string;

	string = (struct usb_string_descriptor *) wstrProduct;
	string->bLength = sizeof(wstrProduct);
	string->bDescriptorType = USB_DT_STRING;
	str2wide(CONFIG_USBD_PRODUCT_NAME, string->wData);
	usbled_string_tbl[STR_PRODUCT] = string;

	string = (struct usb_string_descriptor *) wstrSerial;
	string->bLength = sizeof(wstrSerial);
	string->bDescriptorType = USB_DT_STRING;
	str2wide(serial_number, string->wData);
	usbled_string_tbl[STR_SERIAL] = string;

	/* Now, initialize the string table for ep0 handling */
	usb_strings = usbled_string_tbl;

}

static void usb_led_init_instances(void)
{
	/* initialize device instance */
	memset (device_instance, 0, sizeof (struct usb_device_instance));
	device_instance->device_state = STATE_INIT;
	device_instance->device_descriptor = &device_descriptor;
	device_instance->event = usb_led_event_handler;
	device_instance->cdc_recv_setup = usb_led_cdc_setup;
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
	config_instance->configuration_descriptor =
			(struct usb_configuration_descriptor *)
			&led_configuration_descriptors;
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

/*************************************************************************/

static void usb_led_event_handler(struct usb_device_instance *device,
				  usb_device_event_t event, int data)
{
	switch (event) {
	case VR_CTRL_USB_LED:
		debug("%s: line %d\n", __func__, __LINE__);
		break;
	default:
		debug("%s: line %d\n", __func__, __LINE__);
		break;
	}
}

/*************************************************************************/

int usb_led_on(struct usb_device_request *request)
{
	struct s3c24x0_gpio * const gpio = s3c24x0_get_base_gpio();
	short val;

	val = le16_to_cpu(request->wIndex);

	debug("%s: data: %#x, %#x\n", __func__, val, (~val << 5));

	writel(~val << 5, &gpio->GPBDAT);

	debug("%s: read data: %#x\n", __func__, readl(&gpio->GPBDAT));

	return 0;
}

/*************************************************************************/

/* We handle Vendor request here, is there a better way to do this? */
int usb_led_cdc_setup(struct usb_device_request *request, struct urb *urb)
{
	switch (request->bRequest) {
	case VR_CTRL_USB_LED:
		usb_led_on(request);
		break;
	default:
		return 1;
	}
	return 0;
}

/*************************************************************************/
