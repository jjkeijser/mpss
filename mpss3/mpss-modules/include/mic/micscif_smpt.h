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

#ifndef MIC_SMPT_H
#define MIC_SMPT_H

#include <linux/errno.h>
#include <linux/hardirq.h>
#include <linux/types.h>
#include <linux/capability.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/gfp.h>
#include <linux/vmalloc.h>
#include <asm/io.h>
#include <linux/kernel.h>
#include <linux/mm_types.h>
#include <linux/jiffies.h>
#include <linux/timer.h>
#include <linux/irqflags.h>
#include <linux/time.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/semaphore.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <asm/bug.h>
#include <linux/pci.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <asm/atomic.h>
#include <linux/netdevice.h>
#include <linux/debugfs.h>

#define MAX_BOARD_SUPPORTED	256

#define SNOOP_ON  		(0 << 0)
#define SNOOP_OFF 		(1 << 0)
#define NUM_SMPT_REGISTERS 	32
#define	NUM_SMPT_ENTRIES_IN_USE	32
#define SMPT_MASK 		0x1F
#define MIC_SYSTEM_PAGE_SHIFT 	34ULL
#define MIC_SYSTEM_PAGE_MASK 	((1ULL << MIC_SYSTEM_PAGE_SHIFT) - 1ULL)

struct _mic_ctx_t;
struct pci_dev;

typedef struct mic_smpt {
	dma_addr_t dma_addr; 
	int64_t ref_count;
} mic_smpt_t;


/* Sbox Smpt Reg Bits:
 * Bits 	31:2	Host address
 * Bits 	1	RSVD
 * Bits		0	No snoop
 */
#define BUILD_SMPT(NO_SNOOP, HOST_ADDR)  \
	(uint32_t)(((((HOST_ADDR)<< 2) & (~0x03)) | ((NO_SNOOP) & (0x01))))

bool is_syspa(dma_addr_t hostmic_pa);

dma_addr_t mic_map(int bid, dma_addr_t dma_addr, size_t size);
void mic_unmap(int bid, dma_addr_t dma_addr, size_t size);

dma_addr_t mic_map_single(int bid, struct pci_dev *hwdev, void *p, size_t size);
void mic_unmap_single(int bid, struct pci_dev *hwdev, dma_addr_t mic_addr,
		size_t size);

dma_addr_t mic_ctx_map_single(struct _mic_ctx_t *mic_ctx, void *p, size_t size);
void mic_ctx_unmap_single(struct _mic_ctx_t *mic_ctx, dma_addr_t dma_addr,
		size_t size);

dma_addr_t mic_to_dma_addr(int bid, dma_addr_t mic_addr);
void mic_smpt_set(volatile void *mm_sbox, uint64_t dma_addr, uint64_t index);

static inline
bool mic_map_error(dma_addr_t mic_addr)
{
	return !mic_addr;
}
#endif // MIC_SMPT_H
