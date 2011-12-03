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

#ifndef __USB_LED_H__
#define __USB_LED_H___

#include <usbdevice.h>
#if defined(CONFIG_PPC)
#include <usb/mpc8xx_udc.h>
#elif defined(CONFIG_OMAP1510)
#include <usb/omap1510_udc.h>
#elif defined(CONFIG_MUSB_UDC)
#include <usb/musb_udc.h>
#elif defined(CONFIG_PXA27X)
#include <usb/pxa27x_udc.h>
#elif defined(CONFIG_S3C2440)
#include <usb/s3c2410_udc.h>
#elif defined(CONFIG_SPEAR3XX) || defined(CONFIG_SPEAR600)
#include <usb/spr_udc.h>
#endif

#ifndef CONFIG_USBD_VENDORID
#define CONFIG_USBD_VENDORID		0x1987
#endif
#ifndef CONFIG_USBD_PRODUCTID_USBLED
#define CONFIG_USBD_PRODUCTID_USBLED	0x0727
#endif
#ifndef CONFIG_USBD_MANUFACTURER
#define CONFIG_USBD_MANUFACTURER	"Das U-Boot"
#endif
#ifndef CONFIG_USBD_PRODUCT_NAME
#define CONFIG_USBD_PRODUCT_NAME	"usb led"
#endif

#define CONFIG_USBD_CONFIGURATION_STR	"USB LED TEST"


#define CONFIG_USBD_LED_INT_PKTSIZE	UDC_INT_PACKET_SIZE

#define USBLED_BCD_DEVICE	0x00
#define USBLED_MAXPOWER		0x00

#define STR_LANG		0x00
#define STR_MANUFACTURER	0x01
#define STR_PRODUCT		0x02
#define STR_SERIAL		0x03
#define STR_CONFIG		0x04
#define STR_DATA_INTERFACE	0x05
#define STR_CTRL_INTERFACE	0x06
#define STR_COUNT		0x07

#endif
