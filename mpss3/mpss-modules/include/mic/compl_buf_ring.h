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

#ifndef COMPL_BUF_RING_H
#define COMPL_BUF_RING_H
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
#include "mic_dma_md.h"
#ifndef _MIC_SCIF_
#include "micscif.h"
#include "micscif_smpt.h"
#endif
#define MAX_POLL_TAIL_READ_RETRIES 20

/*
 * Assuming read/write to int is atomic
 * This can't be used as generic ring because of update_tail()
 * One entry is left in the ring to differentiate between ring being empty and
 * full
 */
struct compl_buf_ring {
	int head;
	int tail;
	int size;
	uint64_t tail_location;
	dma_addr_t tail_phys;
};

/* 
 * FIXME:
 * Function calls pci_map_single etc, return type needs to indicate
 * an error
 */
static __always_inline void init_ring(struct compl_buf_ring *ring, int size,
				int device_num)
{
#ifndef _MIC_SCIF_
	struct pci_dev *pdev;
#endif
	ring->head = 0;
	ring->tail = 0;
	ring->size = size;
	ring->tail_location = (uint64_t) kmalloc(sizeof(uint64_t), GFP_ATOMIC);
	BUG_ON(!ring->tail_location);
	*(int*)ring->tail_location = -1;
#ifdef _MIC_SCIF_
	ring->tail_phys = virt_to_phys((void*)ring->tail_location);
#else
	micscif_pci_dev(device_num, &pdev);

	ring->tail_phys = mic_map_single(device_num - 1, pdev, (void *)ring->tail_location,
					sizeof(uint64_t));
	if (mic_map_error(ring->tail_phys))
		printk(KERN_ERR "mic_map returned error please help\n");
#endif
}

static __always_inline void uninit_ring(struct compl_buf_ring *ring,
				int device_num)
{
#ifndef _MIC_SCIF_
	struct pci_dev *pdev;
#endif
	ring->head = 0;
	ring->tail = 0;
	ring->size = 0;
#ifndef _MIC_SCIF_
	micscif_pci_dev(device_num, &pdev);
	mic_unmap_single(device_num - 1, pdev, ring->tail_phys, sizeof(uint64_t));
#endif
	kfree((void *)ring->tail_location);
}

static __always_inline int incr_rb_index(int cur_index, int ring_size)
{
	return((cur_index + 1) % ring_size);
}

/*
 * Tail location has the index that has been recently processed by dma engine
 * But, tail has to point to the index that will be processed next
 * So increment the tail
 */
static __always_inline void update_tail(struct compl_buf_ring *ring, int new_tail)
{
	ring->tail = new_tail;
}

static __always_inline int read_tail(struct compl_buf_ring *ring)
{
	return incr_rb_index(*(volatile int*)ring->tail_location, ring->size);
}

/*
 * This fn. assumes no one else is updating head
 * Returns - avaliable space
 * 0 - if no space is available
 */
static __always_inline bool avail_space_in_ring(struct compl_buf_ring *ring)
{
	int count = 0, max_num_retries = MAX_POLL_TAIL_READ_RETRIES, num_retries = 0;
	int head = ring->head, tail = ring->tail;
retry:
	if (head > tail)
		count = (tail - 0) + (ring->size - head);
	else if (tail > head)
		count = tail - head;
	else
		return ring->size - 1;

	if (1 != count)
		return count - 1;

	num_retries++;
	if (num_retries == max_num_retries)
		return 0;
	cpu_relax();

	ring->tail = read_tail(ring);
	tail = ring->tail;

	goto retry;
}

/*
 * Used for polling
 */
static __always_inline bool is_entry_processed(struct compl_buf_ring *ring, int index)
{
	int head = ring->head, tail = ring->tail;
	if (head < tail) {
		if (index >= head && index < tail)
			return 1;
	} else {
		if (index >= head || index < tail)
			return 1;
	}
	return 0;
}

static __always_inline void incr_head(struct compl_buf_ring *ring)
{
	ring->head = incr_rb_index(ring->head, ring->size);
}

/*
 * This function is not reentrant
 * It is expected that the user of this func, will call incr_head() if allocated
 * buffer is used
 */
static __always_inline int allocate_buffer(struct compl_buf_ring *ring)
{
	if (avail_space_in_ring(ring))
		return ring->head;
	else
		return -1;
}
#endif
