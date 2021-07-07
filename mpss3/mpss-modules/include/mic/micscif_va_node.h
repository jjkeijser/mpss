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

/* generate a virtual address for a given size */
#ifndef MICSCIF_VA_NODE_H
#define MICSCIF_VA_NODE_H

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

#define invalid_va_node_index ((uint32_t)(-1))

struct va_node {
	uint32_t next;
	uint64_t base;
	uint64_t range;
};

struct va_node_allocator {
	/* Emulated variable-size array
	 * is implemented as a sequence of fixed-sized slabs.
	 * SlabDirectory keeps the sequence.
	 * Slab is a contiguous block of nodes -- saves number of allocations
	 * when allocing a new slab of nodes, alloc this size
	 */
	uint32_t slab_shift;
	uint32_t nodes_in_slab;
	uint32_t slab_mask;
	struct va_node **pp_slab_directory;
	uint32_t num_slabs;
	uint32_t num_free_slabs;
	uint32_t free_list;
};

int va_node_is_valid(uint32_t index);

/*
 * get the node corresponding to a NodePtr
 * We are emulating a variable-size array
 */
struct va_node *va_node_get(struct va_node_allocator *node, uint32_t index);

/* returns an NodePtr to a free node */
int va_node_alloc(struct va_node_allocator *node, uint32_t *out_alloc);

/* put a node back into the free pool, by NodePtr */
void va_node_free(struct va_node_allocator *node, uint32_t index);

void va_node_init(struct va_node_allocator *node);

void va_node_destroy(struct va_node_allocator *node);

#endif
