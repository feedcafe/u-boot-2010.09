/*
 * (C) Copyright 2002
 * Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Marius Groeger <mgroeger@sysgo.de>
 * Gary Jennejohn <garyj@denx.de>
 * David Mueller <d.mueller@elsoft.ch>
 *
 * Configuation settings for the SAMSUNG SMDK2440 board.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
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

#ifndef __CONFIG_H
#define __CONFIG_H

/*
 * High Level Configuration Options
 * (easy to change)
 */
#define CONFIG_ARM920T	1	/* This is an ARM920T Core	*/
#define CONFIG_S3C24X0	1	/* in a SAMSUNG S3C24x0-type SoC	*/
#define CONFIG_S3C2440	1	/* specifically a SAMSUNG S3C2440 SoC	*/
#define CONFIG_SMDK2440	1	/* on a SAMSUNG SMDK2440 Board  */
#define	CONFIG_MINI2440_LED

/* input clock of PLL */
#define CONFIG_SYS_CLK_FREQ	12000000/* the SMDK2440 has 12MHz input clock */


#define USE_920T_MMU		1
//#undef CONFIG_USE_IRQ			/* we don't need IRQ/FIQ stuff */
#define CONFIG_USE_IRQ

#define CONFIG_CMDLINE_TAG	/* enable passing of ATAGs */
#define CONFIG_SETUP_MEMORY_TAGS
#define CONFIG_INITRD_TAG

/*
 * Size of malloc() pool
 */
#define CONFIG_SYS_MALLOC_LEN		(CONFIG_ENV_SIZE + 128*1024)
#define CONFIG_SYS_GBL_DATA_SIZE	128	/* size in bytes reserved for initial data */

/*
 * Hardware drivers
 */
#define CONFIG_NET_MULTI
#define	CONFIG_ARP_TIMEOUT	5000	/* Milliseconds before trying ARP again */

#define	CONFIG_DRIVER_DM9000		/* we have a DM9000 on-board */

#ifdef CONFIG_DRIVER_DM9000
# define CONFIG_DM9000_BASE	0x20000300
# define CONFIG_DM9000_NO_SROM
# define DM9000_IO		CONFIG_DM9000_BASE
# define DM9000_DATA		(CONFIG_DM9000_BASE + 4)
#endif

/*
 * select serial console configuration
 */
#define CONFIG_S3C24X0_SERIAL
#define CONFIG_SERIAL1          1	/* we use SERIAL 1 on SMDK2440 */

/************************************************************
 * RTC
 ************************************************************/
#define	CONFIG_RTC_S3C24X0	1

/* allow to overwrite serial and ethaddr */
#define CONFIG_ENV_OVERWRITE

#define CONFIG_BAUDRATE		115200


/*
 * BOOTP options
 */
#define CONFIG_BOOTP_BOOTFILESIZE
#define CONFIG_BOOTP_BOOTPATH
#define CONFIG_BOOTP_GATEWAY
#define CONFIG_BOOTP_HOSTNAME


/*
 * Command line configuration.
 */
#include <config_cmd_default.h>

#define CONFIG_CMD_CACHE
#define CONFIG_CMD_DATE
#define CONFIG_CMD_ELF
#define	CONFIG_CMD_PING

#define CONFIG_CMD_NAND
#define CONFIG_SYS_MAX_NAND_DEVICE	1
#define CONFIG_SYS_NAND_BASE		0x4E000000
#define CONFIG_NAND_S3C2410
#define CONFIG_MTD_NAND_VERIFY_WRITE

#define CONFIG_USB_GADGET
#define CONFIG_USB_GADGET_S3C2410

#define CONFIG_USB_ETHER
#define CONFIG_USB_ETH_RNDIS

#ifdef CONFIG_USB_ETHER
#define CONFIG_USBNET_DEV_ADDR	"03:19:87:07:27:15"
#define CONFIG_USBNET_HOST_ADDR	"03:19:80:09:08:15"
#endif

#if 0
#define	CONFIG_USB_DEVICE
#define	CONFIG_S3C24X0_SECBULK
#define	CONFIG_USB_LED
#define	CONFIG_USB_TTY
#define	CONFIG_SYS_CONSOLE_IS_IN_ENV
#define	CONFIG_USBD_VENDORID		0x1457	/* Linux/NetChip */
#define	CONFIG_USBD_PRODUCTID_GSERIAL	0x5120	/* gserial */
#define	CONFIG_USBD_PRODUCTID_CDCACM	0x5119	/* CDC ACM */
#endif
#define	CONFIG_USBD_MANUFACTURER	"OpenMoko, Inc"
#define	CONFIG_EXTRA_ENV_SETTINGS	\
		"usbtty=cdc_acm\0"	\
		"stderr=usbtty\0stdout=usbtty\0stdin=usbtty\0"

#define CONFIG_BOOTDELAY	9
#define CONFIG_BOOTARGS		"root=/dev/ram0 rdinit=linuxrc console=ttySAC0"
#define CONFIG_ETHADDR		00:19:87:07:27:15
#define CONFIG_NETMASK          255.255.255.0
#define CONFIG_IPADDR		192.168.1.15
#define CONFIG_SERVERIP		192.168.1.98
#define CONFIG_BOOTFILE		"uImage"
#define CONFIG_BOOTCOMMAND	"tftp 31000000 u-boot.bin; go 31000000"

#if defined(CONFIG_CMD_KGDB)
#define CONFIG_KGDB_BAUDRATE	115200		/* speed to run kgdb serial port */
/* what's this ? it's not used anywhere */
#define CONFIG_KGDB_SER_INDEX	1		/* which serial port to use */
#endif

/*
 * Miscellaneous configurable options
 */
#define	CONFIG_SYS_LONGHELP				/* undef to save memory		*/
#define	CONFIG_SYS_PROMPT		"SMDK2440 # "	/* Monitor Command Prompt	*/
#define	CONFIG_SYS_CBSIZE		256		/* Console I/O Buffer Size	*/
#define	CONFIG_SYS_PBSIZE (CONFIG_SYS_CBSIZE+sizeof(CONFIG_SYS_PROMPT)+16) /* Print Buffer Size */
#define	CONFIG_SYS_MAXARGS		16		/* max number of command args	*/
#define CONFIG_SYS_BARGSIZE		CONFIG_SYS_CBSIZE	/* Boot Argument Buffer Size	*/

#define CONFIG_SYS_MEMTEST_START	0x30000000	/* memtest works on	*/
#define CONFIG_SYS_MEMTEST_END		0x33F00000	/* 63 MB in DRAM	*/

#define	CONFIG_SYS_LOAD_ADDR		0x33000000	/* default load address	*/

#define	CONFIG_SYS_HZ			1000

/* valid baudrates */
#define CONFIG_SYS_BAUDRATE_TABLE	{ 9600, 19200, 38400, 57600, 115200 }

/* command history and autocompletion */
#define	CONFIG_CMDLINE_EDITING
#define	CONFIG_AUTO_COMPLETE

/*-----------------------------------------------------------------------
 * Stack sizes
 *
 * The stack sizes are set up in start.S using the settings below
 */
#define CONFIG_STACKSIZE	(128*1024)	/* regular stack */
#ifdef CONFIG_USE_IRQ
# define CONFIG_STACKSIZE_IRQ	(4*1024)	/* IRQ stack */
# define CONFIG_STACKSIZE_FIQ	(4*1024)	/* FIQ stack */
#endif

/*-----------------------------------------------------------------------
 * Physical Memory Map
 */
#define CONFIG_NR_DRAM_BANKS	1	   /* we have 1 bank of DRAM */
#define PHYS_SDRAM_1		0x30000000 /* SDRAM Bank #1 */
#define PHYS_SDRAM_1_SIZE	0x04000000 /* 64 MB */

#define PHYS_FLASH_1		0x00000000 /* Flash Bank #1 */

#define CONFIG_SYS_FLASH_BASE		PHYS_FLASH_1

/*-----------------------------------------------------------------------
 * FLASH and environment organization
 */

#define CONFIG_AMD_LV800	1	/* uncomment this if you have a LV800 flash */

#define CONFIG_SYS_MAX_FLASH_BANKS	1	/* max number of memory banks */
#ifdef CONFIG_AMD_LV800
# define PHYS_FLASH_SIZE		0x00100000 /* 1MB */
# define CONFIG_SYS_MAX_FLASH_SECT	(19)	/* max number of sectors on one chip */
# define CONFIG_ENV_ADDR		(CONFIG_SYS_FLASH_BASE + 0x0F0000) /* addr of environment */
#endif


/* timeout values are in ticks */
#define CONFIG_SYS_FLASH_ERASE_TOUT	(5*CONFIG_SYS_HZ) /* Timeout for Flash Erase */
#define CONFIG_SYS_FLASH_WRITE_TOUT	(5*CONFIG_SYS_HZ) /* Timeout for Flash Write */

#define	CONFIG_ENV_IS_IN_NAND
#define CONFIG_UBOOT_SIZE	0x30000			/* siez = n * nand page size */
#define CONFIG_ENV_OFFSET	CONFIG_UBOOT_SIZE
#define CONFIG_ENV_SIZE		0x10000	/* Total Size of Environment Sector */


#ifdef CONFIG_MINI2440_LED
/* GPIO */
# define GPIO_CTL_BASE	0x56000000
# define oGPIO_B	0x10
# define oGPIO_CON	0x0 /* R/W, Configures the pins of the port */
# define oGPIO_DAT	0x4 /* R/W, Data register for port */
# define oGPIO_UP	0x8 /* R/W, Pull-up disable register */
#endif

#define STACK_BASE	0x33f00000
#define STACK_SIZE	0x8000

/* NAND Flash Controller */

/* Offset */
#define oNFCONF	0x00
#define oNFCONT	0x04
#define oNFCMD	0x08
#define oNFADDR	0x0c
#define oNFDATA	0x10
#define oNFSTAT	0x20
#define oNFECC	0x2c

#define	CONFIG_LL_INIT_NAND_ONLY
#define CONFIG_S3C2440_NAND_BOOT

#endif	/* __CONFIG_H */
