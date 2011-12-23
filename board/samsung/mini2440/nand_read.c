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
 *    - �ɰŶ�� ���𿡰� ��.
 *
 * 2002-05-xx: Chan Gyun Jeong <cgjeong@mizi.com>
 *    - ������ ��Ź�� �ް� ����� �����ϵ��� ����.
 *
 * 2002-08-10: Yong-iL Joh <tolkien@mizi.com>
 *    - SECTOR_SIZE�� 512�� ���� �� �е��� ����
 *
 */

#include <config.h>

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

#define NAND_SECTOR_SIZE	512
#define NAND_BLOCK_MASK		(NAND_SECTOR_SIZE - 1)

/* low level nand read function */
	int
nand_read_ll(unsigned char *buf, unsigned long start_addr, int size)
{
	int i, j;

	if ((start_addr & NAND_BLOCK_MASK) || (size & NAND_BLOCK_MASK)) {
		return -1;	/* invalid alignment */
	}

	/* chip Enable */
	NFCONT &= ~(1<<1);
	for(i=0; i<10; i++);

	for(i=start_addr; i < (start_addr + size);) {
		/* READ0 */
		NFSTAT |= (1<<2);
		NFCMD = 0;

		/* Write Address */
		NFADDR = i & 0xff;
		NFADDR = (i >> 9) & 0xff;
		NFADDR = (i >> 17) & 0xff;
		NFADDR = (i >> 25) & 0xff;

		//wait_idle();
		NAND_DETECT_RB;

		for(j=0; j < NAND_SECTOR_SIZE; j++, i++) {
			*buf = (NFDATA & 0xff);
			buf++;
		}
	}

	/* chip Disable */
	NFCONF |= 0x800;	/* chip disable */

	return 0;
}
#endif	/* CONFIG_S3C2440 */
