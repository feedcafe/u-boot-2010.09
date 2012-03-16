/* arch/arm/mach-s3c2410/include/mach/udc.h
 *
 * Robbed from openmoko's patch
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

#ifndef __ASM_ARCH_UDC_H
#define __ASM_ARCH_UDC_H

struct s3c2410_udc_mach_info {
	void (*udc_command)(int cmd);
#define S3C2410_UDC_CMD_CONNECT		1	/* enable pullup */
#define S3C2410_UDC_CMD_DISCONNECT	0	/* disable pullup */
};

struct s3c2410_plat_udc_data {
	struct s3c2410_udc_mach_info	*udc_info;
};

#endif
