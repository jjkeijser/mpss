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

#include <mic/micscif.h>
#include <mic/micscif_smpt.h>
#if defined(HOST) || defined(WINDOWS)
#include "mic_common.h"
#endif

struct _mic_ctx_t;
// Figure out which SMPT entry based on the host addr
#define SYSTEM_ADDR_TO_SMPT(sysaddr) 	((sysaddr) >> (MIC_SYSTEM_PAGE_SHIFT))
#define HOSTMIC_PA_TO_SMPT(hostmic_pa) (((hostmic_pa) - MIC_SYSTEM_BASE)\
						 >> MIC_SYSTEM_PAGE_SHIFT)

#define	NUM_SMPT_ENTRIES_IN_USE		32
#define SMPT_TO_MIC_PA(smpt_index) 	(MIC_SYSTEM_BASE + ((smpt_index) * \
						MIC_SYSTEM_PAGE_SIZE))
#define MAX_HOST_MEMORY 		((NUM_SMPT_ENTRIES_IN_USE) * MIC_SYSTEM_PAGE_SIZE)
#define MAX_SYSTEM_ADDR 		((MIC_SYSTEM_BASE) + (MAX_HOST_MEMORY) - (1))
#define IS_MIC_SYSTEM_ADDR(addr)	(((addr) >=  MIC_SYSTEM_BASE) && \
						 ((addr) <= MAX_SYSTEM_ADDR))

#define _PAGE_OFFSET(x) 	((x) & ((PAGE_SIZE) - (1ULL)))
#define SMPT_OFFSET(x) 		((x) & MIC_SYSTEM_PAGE_MASK)
#define PAGE_ALIGN_LOW(x)	ALIGN(((x) - ((PAGE_SIZE) - 1ULL)), (PAGE_SIZE))
#define PAGE_ALIGN_HIGH(x) 	ALIGN((x), (PAGE_SIZE))
#define SMPT_ALIGN_LOW(x) 	ALIGN(((x) - (MIC_SYSTEM_PAGE_MASK)), \
								(MIC_SYSTEM_PAGE_SIZE))
#define SMPT_ALIGN_HIGH(x) 	ALIGN((x), (MIC_SYSTEM_PAGE_SIZE))

#if defined(HOST)
#define SMPT_LOGGING			0
#if SMPT_LOGGING
static int64_t smpt_ref_count_g[MAX_BOARD_SUPPORTED];
static int64_t map_count_g;
static int64_t unmap_count_g;
#endif
#endif

void mic_smpt_set(volatile void *mm_sbox, uint64_t dma_addr, uint64_t index)
{
	uint32_t smpt_reg_val = BUILD_SMPT(SNOOP_ON, dma_addr >> MIC_SYSTEM_PAGE_SHIFT);
	writel(smpt_reg_val, (uint8_t*)mm_sbox + SBOX_SMPT00 + (4 * index));
}

#if defined(HOST)
/*
 * Called once per board as part of starting a MIC
 * to restore the SMPT state to the previous values
 * as stored in SMPT SW data structures.
 */
void mic_smpt_restore(mic_ctx_t *mic_ctx)
{
	int i;
	dma_addr_t dma_addr;
	uint32_t *smpt = (uint32_t*)(mic_ctx->mmio.va +
			 HOST_SBOX_BASE_ADDRESS + SBOX_SMPT00);
	uint32_t smpt_reg_val;

	for (i = 0; i < NUM_SMPT_ENTRIES_IN_USE; i++) {
		dma_addr = mic_ctx->mic_smpt[i].dma_addr;
		if (mic_ctx->bi_family == FAMILY_KNC) {
			smpt_reg_val = BUILD_SMPT(SNOOP_ON,
					dma_addr >> MIC_SYSTEM_PAGE_SHIFT);
			writel(smpt_reg_val, &smpt[i]);
		}
	}
}

/*
 * Called once per board as part of smpt init
 * This does a 0-512G smpt mapping, 
 */
void mic_smpt_init(mic_ctx_t *mic_ctx)
{
	int i;
	dma_addr_t dma_addr;
	uint32_t *smpt = (uint32_t*)(mic_ctx->mmio.va +
			 HOST_SBOX_BASE_ADDRESS + SBOX_SMPT00);
	uint32_t smpt_reg_val;
#if SMPT_LOGGING
	smpt_ref_count_g[mic_ctx->bi_id] = 0;
#endif

	spin_lock_init(&mic_ctx->smpt_lock);
	mic_ctx->mic_smpt = kmalloc(sizeof(mic_smpt_t)
					* NUM_SMPT_ENTRIES_IN_USE, GFP_KERNEL);

	for (i = 0; i < NUM_SMPT_ENTRIES_IN_USE; i++) {
		dma_addr = i * MIC_SYSTEM_PAGE_SIZE;
		mic_ctx->mic_smpt[i].dma_addr = dma_addr;
		mic_ctx->mic_smpt[i].ref_count = 0;
		if (mic_ctx->bi_family == FAMILY_KNC) {
			smpt_reg_val = BUILD_SMPT(SNOOP_ON,
					dma_addr >> MIC_SYSTEM_PAGE_SHIFT);
			writel(smpt_reg_val, &smpt[i]);
		}
	}
}

/*
 * Called during mic exit per ctx (i.e once for every board)
 * If ref count is non-zero, then it means that some module
 * did not call mic_unmap_single/mic_ctx_unmap_single correctly.
 */
void
mic_smpt_uninit(mic_ctx_t *mic_ctx)
{
#if SMPT_LOGGING
	printk("global ref count for node = %d is %lld\n", 
		mic_ctx->bi_id+1, smpt_ref_count_g[mic_ctx->bi_id]);
	printk("mic map calls = %lld, mic unmap calls = %lld \n", 
					map_count_g, unmap_count_g);

	for (i = 0; i < NUM_SMPT_ENTRIES_IN_USE; i++) {
		printk("[smpt_san%d] smpt_entry[%d]  dma_addr = 0x%llX"
			" ref_count = %lld \n", mic_ctx->bi_id+1, i, 
			mic_ctx->mic_smpt[i].dma_addr, 
			mic_ctx->mic_smpt[i].ref_count);
	}
#endif
#ifdef DEBUG
	{
		int i;
		for (i = 0; i < NUM_SMPT_ENTRIES_IN_USE; i++)
			WARN_ON(mic_ctx->mic_smpt[i].ref_count);
	}
#endif

	kfree(mic_ctx->mic_smpt);
	mic_ctx->mic_smpt = NULL;
	;
}

dma_addr_t mic_ctx_map_single(mic_ctx_t *mic_ctx, void *p, size_t size)
{
	struct pci_dev *hwdev = mic_ctx->bi_pdev;
	int bid = mic_ctx->bi_id;

	return mic_map_single(bid, hwdev, p, size);
}

void mic_unmap_single(int bid, struct pci_dev *hwdev, dma_addr_t mic_addr,
		size_t size)
{
	dma_addr_t dma_addr = mic_to_dma_addr(bid, mic_addr);
	mic_unmap(bid, mic_addr, size);
	pci_unmap_single(hwdev, dma_addr, size, PCI_DMA_BIDIRECTIONAL);
}

void mic_ctx_unmap_single(mic_ctx_t *mic_ctx, dma_addr_t dma_addr,
		size_t size)
{
	struct pci_dev *hwdev = mic_ctx->bi_pdev;
	int bid = mic_ctx->bi_id;
	mic_unmap_single(bid, hwdev, dma_addr, size);
}

dma_addr_t mic_map_single(int bid, struct pci_dev *hwdev, void *p,
		size_t size)
{
	dma_addr_t mic_addr = 0;
	dma_addr_t dma_addr;

	dma_addr = pci_map_single(hwdev, p, size, PCI_DMA_BIDIRECTIONAL);

	if (!pci_dma_mapping_error(hwdev, dma_addr))
		if (!(mic_addr = mic_map(bid, dma_addr, size))) {
				printk(KERN_ERR "mic_map failed board id %d\
					     addr %#016llx size %#016zx\n", 
					     bid, dma_addr, size);
				pci_unmap_single(hwdev, dma_addr, 
						       size, PCI_DMA_BIDIRECTIONAL);
	}
	return mic_addr;
}

void add_smpt_entry(int spt, int64_t *ref, uint64_t dma_addr, int entries, mic_ctx_t *mic_ctx)
{

	struct nodemsg msg;
	dma_addr_t addr = dma_addr;
	mic_smpt_t *mic_smpt = mic_ctx->mic_smpt;
	int dev_id = mic_ctx->bi_id + 1;
	void *mm_sbox = mic_ctx->mmio.va + HOST_SBOX_BASE_ADDRESS;
	int i;

	for (i = spt; i < spt + entries; i++, addr += MIC_SYSTEM_PAGE_SIZE) {
#ifdef CONFIG_ML1OM
		/*
		 * For KNF if the ref count is 0 and the entry number is greater
		 * than 16 then we must resend a SMPT_SET message in case the uOS
		 * was rebooted and lost SMPT register state (example during host
		 * suspend/hibernate.
		 */
		if (!mic_smpt[i].ref_count && i >= (NUM_SMPT_ENTRIES_IN_USE >> 1)) {
#else
		if (!mic_smpt[i].ref_count && (mic_smpt[i].dma_addr != addr)) {
#endif
			/*
 			 * ref count was zero and dma_addr requested did not
 			 * match the dma address in the table. So, this is a
 			 * new entry in the table. 
 			 * KNF: Send a message to the card
 			 * to update its smpt table with a new value.
 			 * KNC: write to the SMPT registers from host since
 			 * they are accessible. 
 			 */
			if (mic_ctx->bi_family == FAMILY_ABR) {
				msg.uop = SMPT_SET;
				msg.payload[0] = addr;
				msg.payload[1] = i;
				msg.dst.node  = scif_dev[dev_id].sd_node;
				msg.src.node  = 0;
#if SMPT_LOGGING
				printk("[smpt_node%d] ==> sending msg to "
					" node = %d dma_addr = 0x%llX, entry =" 
					"0x%llX\n" , mic_ctx->bi_id + 1, 
					scif_dev[dev_id].sd_node, 
					msg.payload[0], msg.payload[1]);
#endif
				micscif_inc_node_refcnt(&scif_dev[dev_id], 1);
				micscif_nodeqp_send(&scif_dev[dev_id], &msg, NULL);
				micscif_dec_node_refcnt(&scif_dev[dev_id], 1);
			}
			else
				mic_smpt_set(mm_sbox, addr, i);
			mic_smpt[i].dma_addr = addr;
		}
		mic_smpt[i].ref_count += ref[i - spt];
	}
}

dma_addr_t smpt_op(int bid, uint64_t dma_addr,
				int entries, int64_t *ref)
{
	int spt = -1;   /* smpt index */
	int ee = 0; 	/* existing entries */
	int fe = 0;     /* free entries */
	int i;
	unsigned long flags;
	dma_addr_t mic_addr = 0;
	dma_addr_t addr = dma_addr;
	mic_ctx_t *mic_ctx = get_per_dev_ctx(bid);
	mic_smpt_t *mic_smpt = mic_ctx->mic_smpt;

	if (micpm_get_reference(mic_ctx, true))
		goto exit;
	spin_lock_irqsave(&mic_ctx->smpt_lock, flags);

	/* find existing entries */
	for (i = 0; i < NUM_SMPT_ENTRIES_IN_USE; i++) {
		if (mic_smpt[i].dma_addr == addr) {
			ee++;
			addr += MIC_SYSTEM_PAGE_SIZE;
		}
		else if (ee) /* cannot find contiguous entries */
			goto not_found;

		if (ee == entries)
			goto found;
	}

	/* find free entry */
#ifdef CONFIG_ML1OM
	/*
	 * For KNF the SMPT registers are not host accessible so we maintain a
	 * 1:1 map for SMPT registers from 0-256GB i.e. the first 16 entries and
	 * look for SMPT entries for P2P and IB etc from the 16th entry onwards.
	 * This allows the KNF card to boot on Host systems with < 256GB system
	 * memory and access VNET/SCIF buffers without crashing. P2P and IB SMPT
	 * entries are setup after SCIF driver load/reload via SCIF Node QP
	 * SMPT_SET messages.
	 */
	for (i = NUM_SMPT_ENTRIES_IN_USE / 2 ; i < NUM_SMPT_ENTRIES_IN_USE; i++) {
#else
	for (i = 0 ; i < NUM_SMPT_ENTRIES_IN_USE; i++) {
#endif
		fe = (mic_smpt[i].ref_count == 0) ? fe + 1: 0;
		if (fe == entries)
			goto found;
	}

not_found:
	spin_unlock_irqrestore(&mic_ctx->smpt_lock, flags);
	micpm_put_reference(mic_ctx);
exit:
	return mic_addr;
found:
	spt = i - entries + 1;
	mic_addr = SMPT_TO_MIC_PA(spt);
	add_smpt_entry(spt, ref, dma_addr, entries, mic_ctx);
	spin_unlock_irqrestore(&mic_ctx->smpt_lock, flags);
	micpm_put_reference(mic_ctx);
	return mic_addr;
}


/*
 * Returns number of smpt entries needed for dma_addr to dma_addr + size
 * also returns the reference count array for each of those entries
 * and the starting smpt address
 */
int get_smpt_ref_count(int64_t *ref, dma_addr_t dma_addr, size_t size,
						uint64_t *smpt_start)
{
	uint64_t start =  dma_addr;
	uint64_t end = dma_addr + size;
	int i = 0;

	while (start < end) {
		ref[i++] = min(SMPT_ALIGN_HIGH(start + 1), end) - start;
		start = SMPT_ALIGN_HIGH(start + 1);
	}

	if (smpt_start)
		*smpt_start = SMPT_ALIGN_LOW(dma_addr);

	return i;
}

/*
 * Maps dma_addr to dma_addr + size memory in the smpt table
 * of board bid
 */
dma_addr_t mic_map(int bid, dma_addr_t dma_addr, size_t size)
{
	dma_addr_t mic_addr = 0;
	int entries;
	int64_t ref[NUM_SMPT_ENTRIES_IN_USE];
	uint64_t smpt_start;
#if SMPT_LOGGING
	unsigned long flags;
	mic_ctx_t *mic_ctx = get_per_dev_ctx(bid);
	spin_lock_irqsave(&mic_ctx->smpt_lock, flags);
	map_count_g++;
	smpt_ref_count_g[bid] += (int64_t)size;
	spin_unlock_irqrestore(&mic_ctx->smpt_lock, flags);
#endif
	if (!size)
		return mic_addr;

	/*
 	 * Get number of smpt entries to be mapped, ref count array
 	 * and the starting smpt address to start the search for
 	 * free or existing smpt entries.
 	 */
	entries = get_smpt_ref_count(ref, dma_addr, size, &smpt_start);

	/* Set the smpt table appropriately and get 16G aligned mic address */
	mic_addr =  smpt_op(bid, smpt_start, entries, ref);

	/*
 	 * If mic_addr is zero then its a error case
 	 * since mic_addr can never be zero.
 	 * else generate mic_addr by adding the 16G offset in dma_addr
 	 */
	if (!mic_addr) {
		WARN_ON(1);
		return mic_addr;
	}
	else
		return (mic_addr + (dma_addr & MIC_SYSTEM_PAGE_MASK));
}

/*
 * Unmaps mic_addr to mic_addr + size memory in the smpt table
 * of board bid
 */
void mic_unmap(int bid, dma_addr_t mic_addr, size_t size)
{
	mic_ctx_t *mic_ctx = get_per_dev_ctx(bid);
	mic_smpt_t *mic_smpt = mic_ctx->mic_smpt;
	int64_t ref[NUM_SMPT_ENTRIES_IN_USE];
	int num_smpt;
	int spt = HOSTMIC_PA_TO_SMPT(mic_addr);
	int i;
	unsigned long flags;

	if (!size)
		return;

	if (!IS_MIC_SYSTEM_ADDR(mic_addr)) {
		WARN_ON(1);
		return;
	}

 	/* Get number of smpt entries to be mapped, ref count array */
	num_smpt = get_smpt_ref_count(ref, mic_addr, size, NULL);

	spin_lock_irqsave(&mic_ctx->smpt_lock, flags);

#if SMPT_LOGGING
	unmap_count_g++;
	smpt_ref_count_g[bid] -= (int64_t)size;
#endif

	for (i = spt; i < spt + num_smpt; i++) {
         	mic_smpt[i].ref_count -= ref[i - spt];
		WARN_ON(mic_smpt[i].ref_count < 0);
	}
	spin_unlock_irqrestore(&mic_ctx->smpt_lock, flags);
}

dma_addr_t mic_to_dma_addr(int bid, dma_addr_t mic_addr)
{
	mic_ctx_t *mic_ctx = get_per_dev_ctx(bid);
	int spt = HOSTMIC_PA_TO_SMPT(mic_addr);
	dma_addr_t dma_addr;

	if (!IS_MIC_SYSTEM_ADDR(mic_addr)) {
		WARN_ON(1);
		return 0;
	}
	dma_addr = mic_ctx->mic_smpt[spt].dma_addr + SMPT_OFFSET(mic_addr);
	return dma_addr;
}

#endif

bool is_syspa(dma_addr_t pa)
{
	return IS_MIC_SYSTEM_ADDR(pa);
}
