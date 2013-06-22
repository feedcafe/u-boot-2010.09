/* 
 * vivi/s3c2410/nand_read.c: Simple NAND read functions for booting from NAND
 *
 * Copyright (C) 2002 MIZI Research, Inc.
 *
 * Author: Hwang, Chideok <hwang@mizi.com>
 * Date  : $Date: 2002/08/14 10:26:47 $
 *
 * $Revision: 1.6 $
 * $Id: param.c,v 1.9 2002/07/11 06:17:20 nandy Exp 
 *
 * History
 *
 * 2002-05-xx: Hwang, Chideok <hwang@mizi.com>
 *    - 될거라고 난디에게 줌.
 *
 * 2002-05-xx: Chan Gyun Jeong <cgjeong@mizi.com>
 *    - 난디의 부탁을 받고 제대로 동작하도록 수정.
 *
 * 2002-08-10: Yong-iL Joh <tolkien@mizi.com>
 *    - SECTOR_SIZE가 512인 놈은 다 읽도록 수정
 *
 */

#include <common.h>
#include <asm/arch/s3c24x0_cpu.h>

#define __REGb(x)	(*(volatile unsigned char *)(x))
#define __REGi(x)	(*(volatile unsigned int *)(x))
#define NF_BASE		0x4e000000

#if defined(CONFIG_S3C2440)
#define NFCONF		__REGi(NF_BASE + 0x0)
#define NFCONT		__REGi(NF_BASE + 0x4)
#define NFCMD		__REGb(NF_BASE + 0x8)
#define NFADDR		__REGb(NF_BASE + 0xc)
#define NFDATA		__REGb(NF_BASE + 0x10)
#define NFSTAT		__REGb(NF_BASE + 0x20)

#define NAND_CHIP_ENA	(NFCONT &= ~(1<<1))
#define NAND_CHIP_DIS	(NFCONT |= ~(1<<1))
#define NAND_CLEAR_RB	(NFSTAT |= (1<<2))
#define NAND_DETECT_RB	{ while (!(NFSTAT & (1<<2))); }

#define BUSY 4
inline void wait_idle(void) {
	int i;

	while(!(NFSTAT & BUSY))
		for(i=0; i<10; i++);
}

static void s3c2440_write_addr_page_2k(unsigned addr, unsigned mask, unsigned page_size)
{
	int col, page;

	col = addr & mask;
	page = addr / page_size;

	NFADDR = col & 0xff;		/* Column Address A0~A7 */
	NFADDR = (col >> 8) & 0x0f;	/* Column Address A8~A11 */
	NFADDR = page & 0xff;		/* Row Address A12~A19 */
	NFADDR = (page >> 8) & 0xff;	/* Row Address A20~A27 */
	NFADDR = (page >> 16) & 0x01;	/* Row Address A28 */
}

static void s3c2440_write_addr_page_512(unsigned int addr)
{
	/* Write Address */
	NFADDR = addr & 0xff;
	NFADDR = (addr >> 9) & 0xff;
	NFADDR = (addr >> 17) & 0xff;
	NFADDR = (addr >> 25) & 0xff;
}

/* low level nand read function */
int nand_read_ll(unsigned char *buf, unsigned long start_addr, int size)
{
	int i, j;
	struct s3c24x0_gpio * const gpio = s3c24x0_get_base_gpio();

	unsigned char ncon = (gpio->GSTATUS0 & 0x4) >> 2;
	unsigned char gpg13 = (gpio->GPGDAT & 0x2000) >> 13;
	unsigned page_size;
	unsigned nand_block_mask;

	if (ncon == 1)
		if (gpg13 == 1)
			page_size = 2048;
		else
			page_size = 1024;
	else
		if (gpg13 == 1)
			page_size = 512;
		else
			page_size = 256;

	nand_block_mask = page_size - 1;

	if ((start_addr & nand_block_mask) || (size & nand_block_mask)) {
		return -1;	/* invalid alignment */
	}

	NFCONF = (3 << 8);
	/* chip Enable */
	NFCONT &= ~(1<<1);
	for (i = 0; i < 10; i++);

	for (i = start_addr; i < (start_addr + size);) {
		/* READ0 */
		NFSTAT |= (1<<2);
		NFCMD = 0;

		if (page_size == 2048) {
			s3c2440_write_addr_page_2k(i, nand_block_mask, page_size);
			NFCMD = 0x30;
		} else if (page_size == 1024) {
			/* TODO */
		} else if (page_size == 512) {
			s3c2440_write_addr_page_512(i);
		} else if (page_size == 256) {
			/* TODO */
		}

		//wait_idle();
		NAND_DETECT_RB;

		for(j = 0; j < page_size; j++, i++) {
			*buf = (NFDATA & 0xff);
			buf++;
		}
	}

	/* chip Disable */
	NFCONT |= (1 << 1);	/* chip disable */

	return 0;
}
#endif	/* CONFIG_S3C2440 */
