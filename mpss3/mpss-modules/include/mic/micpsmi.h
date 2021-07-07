/*
 * Copyright 2010-2017 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * Disclaimer: The codes contained in these modules may be specific to
 * the Intel Software Development Platform codenamed Knights Ferry,
 * and the Intel product codenamed Knights Corner, and are not backward
 * compatible with other Intel products. Additionally, Intel will NOT
 * support the codes or instruction set in future products.
 *
 * Intel offers no warranty of any kind regarding the code. This code is
 * licensed on an "AS IS" basis and Intel is not obligated to provide
 * any support, assistance, installation, training, or other services
 * of any kind. Intel is also not obligated to provide any updates,
 * enhancements or extensions. Intel specifically disclaims any warranty
 * of merchantability, non-infringement, fitness for any particular
 * purpose, and any other warranty.
 *
 * Further, Intel disclaims all liability of any kind, including but
 * not limited to liability for infringement of any proprietary rights,
 * relating to the use of the code, even if Intel is notified of the
 * possibility of such liability. Except as expressly stated in an Intel
 * license agreement provided with this code and agreed upon with Intel,
 * no license, express or implied, by estoppel or otherwise, to any
 * intellectual property rights is granted herein.
 */

#ifndef _MIC_PSMI_H
#define _MIC_PSMI_H

struct mic_psmi_pte {
	uint64_t pa;
};

struct mic_psmi_ctx
{
	unsigned char 		enabled;

	struct mic_psmi_pte	*dma_tbl;
	int 			dma_tbl_size;
	dma_addr_t 	dma_tbl_hndl;
	uint64_t		dma_mem_size;
	int 			nr_dma_pages;

	struct mic_psmi_pte 	*va_tbl;
};

#define MIC_PSMI_PAGE_ORDER (7)
#define MIC_PSMI_PAGE_SIZE (PAGE_SIZE << MIC_PSMI_PAGE_ORDER)
#define MIC_PSMI_SIGNATURE 0x4B434F52494D5350L

int mic_psmi_open(struct file *filp);

#endif /* _MIC_PSMI_H */
