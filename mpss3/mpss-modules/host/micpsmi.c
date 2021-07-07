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

#include "micint.h"

bool mic_psmi_enable = 0;

extern struct bin_attribute mic_psmi_ptes_attr;

static __always_inline void
mic_psmi_free_pte(mic_ctx_t *mic_ctx, int i)
{
	pci_unmap_single(mic_ctx->bi_pdev, 
		mic_ctx->bi_psmi.dma_tbl[i].pa, MIC_PSMI_PAGE_SIZE, PCI_DMA_BIDIRECTIONAL);
	free_pages(mic_ctx->bi_psmi.va_tbl[i - 1].pa, MIC_PSMI_PAGE_ORDER);
}

static int mic_psmi_alloc_buffer(mic_ctx_t *mic_ctx)
{
	int i, j, ret;
	void *va;
	dma_addr_t dma_hndl;
	struct mic_psmi_ctx *psmi_ctx = &mic_ctx->bi_psmi;

	/* allocate psmi page tables */
	psmi_ctx->nr_dma_pages =
		ALIGN(psmi_ctx->dma_mem_size, 
				MIC_PSMI_PAGE_SIZE) / MIC_PSMI_PAGE_SIZE;
	if ((psmi_ctx->va_tbl =
		kmalloc(psmi_ctx->nr_dma_pages *
				sizeof(struct mic_psmi_pte), GFP_KERNEL)) == NULL) {
		printk("mic: psmi va table alloc failed\n");
		return -ENOMEM;
	}
	psmi_ctx->dma_tbl_size =
		(psmi_ctx->nr_dma_pages + 2) * sizeof(struct mic_psmi_pte);
	if ((psmi_ctx->dma_tbl =
			kmalloc(psmi_ctx->dma_tbl_size, GFP_KERNEL)) == NULL) {
		printk("mic: psmi dma table alloc failed\n");
		ret = -ENOMEM;
		goto free_va_tbl;
	}
	psmi_ctx->dma_tbl_hndl =
		pci_map_single(mic_ctx->bi_pdev, 
			psmi_ctx->dma_tbl, psmi_ctx->dma_tbl_size, PCI_DMA_BIDIRECTIONAL);
	if (pci_dma_mapping_error(mic_ctx->bi_pdev, 
						psmi_ctx->dma_tbl_hndl)) {
		printk("mic: psmi dma table mapping failed\n");
		ret = -ENOMEM;
		goto free_dma_tbl;
	}

	/* allocate psmi pages */
	for (i = 0; i < psmi_ctx->nr_dma_pages; i++) {
		if ((va = (void *)__get_free_pages(
				GFP_KERNEL | __GFP_HIGHMEM,
					MIC_PSMI_PAGE_ORDER)) == NULL) {
			printk("mic: psmi page alloc failed: %d\n", i);
			ret = -ENOMEM;
			goto free_ptes;
		}
		memset(va, 0, MIC_PSMI_PAGE_SIZE);
		dma_hndl = pci_map_single(mic_ctx->bi_pdev, va, 
						MIC_PSMI_PAGE_SIZE, PCI_DMA_BIDIRECTIONAL);
		if (pci_dma_mapping_error(mic_ctx->bi_pdev, dma_hndl)) {
			printk("mic: psmi page mapping failed: %d\n", i);
			free_pages((unsigned long)va, MIC_PSMI_PAGE_ORDER);
			ret = -ENOMEM;
			goto free_ptes;
		}
		psmi_ctx->dma_tbl[i + 1].pa = dma_hndl;
		psmi_ctx->va_tbl[i].pa = (uint64_t)va;
	}
	psmi_ctx->dma_tbl[0].pa = MIC_PSMI_SIGNATURE;
	psmi_ctx->dma_tbl[psmi_ctx->nr_dma_pages + 1].pa = MIC_PSMI_SIGNATURE;
	printk("mic: psmi #%d, %ld bytes, "
			"dma_tbl va=0x%lx hndl=0x%lx\n", mic_ctx->bi_id + 1, 
			(unsigned long)psmi_ctx->dma_mem_size, 
			(unsigned long)psmi_ctx->dma_tbl, 
			(unsigned long)psmi_ctx->dma_tbl_hndl);
	return 0;
free_ptes:
	for (j = 1; j < i; j++)
		mic_psmi_free_pte(mic_ctx, j);
	pci_unmap_single(mic_ctx->bi_pdev, 
		psmi_ctx->dma_tbl_hndl, psmi_ctx->dma_tbl_size, PCI_DMA_BIDIRECTIONAL);
free_dma_tbl:
	kfree(psmi_ctx->dma_tbl);
	psmi_ctx->dma_tbl = NULL;
free_va_tbl:
	kfree(psmi_ctx->va_tbl);
	psmi_ctx->va_tbl = NULL;
	return ret;
}

static void mic_psmi_free_buffer(mic_ctx_t *mic_ctx)
{
	struct mic_psmi_ctx *psmi_ctx = &mic_ctx->bi_psmi;
	int i;

	for (i = 1; i <= psmi_ctx->nr_dma_pages; i++)
		mic_psmi_free_pte(mic_ctx, i);
	pci_unmap_single(mic_ctx->bi_pdev, 
		psmi_ctx->dma_tbl_hndl, psmi_ctx->dma_tbl_size, PCI_DMA_BIDIRECTIONAL);
	kfree(psmi_ctx->dma_tbl);
	psmi_ctx->dma_tbl = NULL;
	kfree(psmi_ctx->va_tbl);
	psmi_ctx->va_tbl = NULL;
	printk("mic: psmi freed %ld bytes for board #%d\n", 
		(unsigned long)psmi_ctx->dma_mem_size, mic_ctx->bi_id + 1);
}

extern int usagemode_param;

int mic_psmi_init(mic_ctx_t *mic_ctx)
{
	int ret;
	int status = 0;
	uint32_t scratch0;
	struct mic_psmi_ctx * psmi_ctx = &mic_ctx->bi_psmi;

	psmi_ctx->enabled = 0;
	/* Only initialize psmi for the first board */
	if (!mic_psmi_enable || mic_ctx->bi_id)
		return 0;
	if(!(scratch0 = SBOX_READ(mic_ctx->mmio.va, SBOX_SCRATCH0))) {
		status = wait_for_bootstrap(mic_ctx->mmio.va);
		scratch0 = SBOX_READ(mic_ctx->mmio.va, SBOX_SCRATCH0);
	}
	/* Memory size includes 512K reserved for VGA & GTT table */
	psmi_ctx->dma_mem_size =
		SCRATCH0_MEM_SIZE_KB(scratch0) * ((1) * 1024) +
			MIC_PSMI_PAGE_SIZE;
	if (USAGE_MODE_NORMAL == usagemode_param) {
		if ((ret = mic_psmi_alloc_buffer(mic_ctx)))
			return ret;
		mic_psmi_ptes_attr.size = psmi_ctx->dma_tbl_size;
	}
	psmi_ctx->enabled = 1;
	return 0;
}

void mic_psmi_uninit(mic_ctx_t *mic_ctx)
{
	struct mic_psmi_ctx * psmi_ctx = &mic_ctx->bi_psmi;

	if (!psmi_ctx->enabled)
		return;
	if (USAGE_MODE_NORMAL == usagemode_param)
		mic_psmi_free_buffer(mic_ctx);
	psmi_ctx->enabled = 0;
}
