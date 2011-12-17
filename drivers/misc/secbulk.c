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

#include "secbulk.h"

#ifdef DEBUG
#undef debug
#define debug(fmt,args...)	serial_printf (fmt ,##args)
#endif
#define info(fmt,args...)	serial_printf (fmt ,##args)

#define NUM_CONFIGS	1
#define MAX_INTERFACES	1
#define NUM_ENDPOINTS	2

/*
 * Instance variables
 */
static struct usb_device_instance device_instance[1];
static struct usb_bus_instance bus_instance[1];
static struct usb_configuration_instance config_instance[NUM_CONFIGS];
static struct usb_interface_instance interface_instance[MAX_INTERFACES];
/* one extra for control endpoint */
static struct usb_endpoint_instance endpoint_instance[NUM_ENDPOINTS+1];

/*
 * Descriptors, Strings, Local variables.
 */

/* defined and used by gadget/ep0.c */
extern struct usb_string_descriptor **usb_strings;

static struct usb_string_descriptor *secbulk_string_tbl[STR_COUNT];

/* USB Descriptor Strings */
static u8 wstrLang[4] = {4, USB_DT_STRING, 0x9, 0x4};
static u8 wstrManufacturer[2 + 2*(sizeof(CONFIG_USBD_MANUFACTURER)-1)];
static u8 wstrProduct[2 + 2*(sizeof(CONFIG_USBD_PRODUCT_NAME)-1)];


/* Standard USB Data Structures */
static struct usb_endpoint_descriptor *ep_descriptor_ptrs[NUM_ENDPOINTS];
static struct usb_device_descriptor device_descriptor = {
	.bLength = sizeof(struct usb_device_descriptor),
	.bDescriptorType =	USB_DT_DEVICE,
	.bcdUSB =		cpu_to_le16(USB_BCD_VERSION),
	.bDeviceClass	=	0xff,
	.bDeviceSubClass =	0x00,
	.bDeviceProtocol =	0x00,
	.bMaxPacketSize0 =	EP0_MAX_PACKET_SIZE,
	.idVendor =		cpu_to_le16(CONFIG_USBD_VENDORID),
	.idProduct =		cpu_to_le16(CONFIG_USBD_PRODUCTID_SECBULK),
	.bcdDevice =		cpu_to_le16(SECBULK_BCD_DEVICE),
	.iManufacturer =	STR_MANUFACTURER,
	.iProduct =		STR_PRODUCT,
	.iSerialNumber =	0,
	.bNumConfigurations =	NUM_CONFIGS
};

struct secbulk_config_desc {
	struct usb_configuration_descriptor config_desc;

	struct usb_interface_descriptor interface_desc;
	struct usb_endpoint_descriptor data_endpoints[NUM_ENDPOINTS];
} __attribute__((packed));

static struct secbulk_config_desc secbulk_configuration_descriptors[NUM_CONFIGS] = {
	{
		.config_desc = {
			.bLength = sizeof(struct usb_configuration_descriptor),
			.bDescriptorType	= USB_DT_CONFIG,
			.wTotalLength		=
				cpu_to_le16(sizeof(struct secbulk_config_desc)),
			.bNumInterfaces		= 1,
			.bConfigurationValue	= 1,
			.iConfiguration		= 0,
			.bmAttributes		=
				BMATTRIBUTE_SELF_POWERED | BMATTRIBUTE_RESERVED,
			.bMaxPower		= SECBULK_MAXPOWER,	/* unit: 2mA */
		},
		.interface_desc = {
			.bLength = sizeof(struct usb_interface_descriptor),
			.bDescriptorType	= USB_DT_INTERFACE,
			.bInterfaceNumber	= 0,
			.bAlternateSetting	= 0,
			.bNumEndpoints		= 0x2,
			.bInterfaceClass	= 0xFF,
			.bInterfaceSubClass	= 0,
			.bInterfaceProtocol	= 0,
			.iInterface		= 0,
		},
		.data_endpoints = {
			{
				.bLength		=
					sizeof(struct usb_endpoint_descriptor),
				.bDescriptorType	= USB_DT_ENDPOINT,
				.bEndpointAddress	= UDC_IN_ENDPOINT | USB_DIR_IN,
				.bmAttributes		= USB_ENDPOINT_XFER_BULK,
				.wMaxPacketSize		=
					cpu_to_le16(CONFIG_SECBULK_PKTSIZE),
				.bInterval		= 0,
			},
			{
				.bLength		=
					sizeof(struct usb_endpoint_descriptor),
				.bDescriptorType	= USB_DT_ENDPOINT,
				.bEndpointAddress	= UDC_OUT_ENDPOINT | USB_DIR_OUT,
				.bmAttributes		= USB_ENDPOINT_XFER_BULK,
				.wMaxPacketSize		=
					cpu_to_le16(CONFIG_SECBULK_PKTSIZE),
				.bInterval		= 0,
			},
		
		},
	},
};

/*
 * Static Function Prototypes
 */
static void secbulk_init_strings(void);
static void secbulk_init_instances(void);
static void secbulk_init_endpoints(void);
static void secbulk_event_handler(struct usb_device_instance *device,
				usb_device_event_t event, int data);
static int secbulk_cdc_setup(struct usb_device_request *request,
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

int secbulk_init(void)
{
	udc_init();	/* Basic USB initialization */
	secbulk_init_strings();
	secbulk_init_instances();
	udc_startup_events(device_instance);	/* Enable dev, init udc pointers */
	udc_connect();	/* Enable pullup for host detection */
	secbulk_init_endpoints();

	return 0;
}

static void secbulk_init_strings(void)
{
	struct usb_string_descriptor *string;

	secbulk_string_tbl[STR_LANG] =
		(struct usb_string_descriptor*)wstrLang;

	string = (struct usb_string_descriptor *) wstrManufacturer;
	string->bLength = sizeof(wstrManufacturer);
	string->bDescriptorType = USB_DT_STRING;
	str2wide(CONFIG_USBD_MANUFACTURER, string->wData);
	secbulk_string_tbl[STR_MANUFACTURER] = string;

	string = (struct usb_string_descriptor *) wstrProduct;
	string->bLength = sizeof(wstrProduct);
	string->bDescriptorType = USB_DT_STRING;
	str2wide(CONFIG_USBD_PRODUCT_NAME, string->wData);
	secbulk_string_tbl[STR_PRODUCT] = string;

	/* Now, initialize the string table for ep0 handling */
	usb_strings = secbulk_string_tbl;

}

static void secbulk_init_instances(void)
{
	int i;

	/* initialize device instance */
	memset (device_instance, 0, sizeof (struct usb_device_instance));
	device_instance->device_state = STATE_INIT;
	device_instance->device_descriptor = &device_descriptor;
	device_instance->event = secbulk_event_handler;
	device_instance->cdc_recv_setup = secbulk_cdc_setup;
	device_instance->bus = bus_instance;
	device_instance->configurations = NUM_CONFIGS;
	device_instance->configuration_instance_array = config_instance;

	/* initialize bus instance */
	memset (bus_instance, 0, sizeof (struct usb_bus_instance));
	bus_instance->device = device_instance;
	bus_instance->endpoint_array = endpoint_instance;
	bus_instance->max_endpoints = 1;
	bus_instance->maxpacketsize = 64;

	/* configuration instance */
	memset (config_instance, 0,
		sizeof (struct usb_configuration_instance));
	//config_instance->interfaces = interface_count;
	config_instance->configuration_descriptor =
			(struct usb_configuration_descriptor *)
			&secbulk_configuration_descriptors;
	config_instance->interface_instance_array = interface_instance;

	/* endpoint instances */
	memset (&endpoint_instance[0], 0,
		sizeof (struct usb_endpoint_instance));
	endpoint_instance[0].endpoint_address = 0;
	endpoint_instance[0].rcv_packetSize = EP0_MAX_PACKET_SIZE;
	endpoint_instance[0].rcv_attributes = USB_ENDPOINT_XFER_CONTROL;
	endpoint_instance[0].tx_packetSize = EP0_MAX_PACKET_SIZE;
	endpoint_instance[0].tx_attributes = USB_ENDPOINT_XFER_CONTROL;
	udc_setup_ep (device_instance, 0, &endpoint_instance[0]);

	ep_descriptor_ptrs[0] =
		&secbulk_configuration_descriptors[0].data_endpoints[0];
	ep_descriptor_ptrs[1] =
		&secbulk_configuration_descriptors[0].data_endpoints[1];

	for (i = 1; i <= NUM_ENDPOINTS; i++) {
		memset (&endpoint_instance[i], 0,
			sizeof (struct usb_endpoint_instance));

		endpoint_instance[i].endpoint_address =
			ep_descriptor_ptrs[i - 1]->bEndpointAddress;

		endpoint_instance[i].rcv_attributes =
			ep_descriptor_ptrs[i - 1]->bmAttributes;

		endpoint_instance[i].rcv_packetSize =
			le16_to_cpu(ep_descriptor_ptrs[i - 1]->wMaxPacketSize);

		endpoint_instance[i].tx_attributes =
			ep_descriptor_ptrs[i - 1]->bmAttributes;

		endpoint_instance[i].tx_packetSize =
			le16_to_cpu(ep_descriptor_ptrs[i - 1]->wMaxPacketSize);

		endpoint_instance[i].tx_attributes =
			ep_descriptor_ptrs[i - 1]->bmAttributes;

		urb_link_init (&endpoint_instance[i].rcv);
		urb_link_init (&endpoint_instance[i].rdy);
		urb_link_init (&endpoint_instance[i].tx);
		urb_link_init (&endpoint_instance[i].done);

		if (endpoint_instance[i].endpoint_address & USB_DIR_IN)
			endpoint_instance[i].tx_urb =
				usbd_alloc_urb (device_instance,
						&endpoint_instance[i]);
		else
			endpoint_instance[i].rcv_urb =
				usbd_alloc_urb (device_instance,
						&endpoint_instance[i]);
	}

}

static void secbulk_init_endpoints(void)
{
	int i;

	bus_instance->max_endpoints = NUM_ENDPOINTS + 1;
	for (i = 1; i <= NUM_ENDPOINTS; i++) {
		udc_setup_ep(device_instance, i, &endpoint_instance[i]);
	}
}

/*************************************************************************/

static void secbulk_event_handler(struct usb_device_instance *device,
				  usb_device_event_t event, int data)
{
	switch (event) {
	default:
		debug("%s: event: %d\n", __func__, event);
		break;
	}
}

/*************************************************************************/

/* We handle Vendor request here, is there a better way to do this? */
int secbulk_cdc_setup(struct usb_device_request *request, struct urb *urb)
{
	switch (request->bRequest) {
	case 0:
		break;
	default:
		debug("%s: bRequest: %d\n", __func__, request->bRequest);
		return 1;
	}
	return 0;
}

/*************************************************************************/
