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

#ifndef _SCIF_RING_BUFFER_DEFINE
#define _SCIF_RING_BUFFER_DEFINE

/*
 * This describes a general purpose, byte based
 * ring buffer.  It handles multiple readers or
 * writers using a lock -- it is lockless between
 * producer and consumer (so it can handle being
 * used across the PCIe bus).
 */
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

/**
 * This version is used to ensure component compatibility between the host and
 * card driver modules that use the ring buffer functions. This version should
 * be incremented whenever there is a change to the ring buffer module that
 * affects the functionality of the ring buffer.
 */
#define RING_BUFFER_VERSION	1

/* Two of these actually form a single queue -- one on each side of the PCIe
 * bus
 *
 * NOTE!  This only works if the queue (pointed to at rb_base) exists in the
 * consumer's memory.  The code does not do any wbinvd after writing to the
 * buffer, which assumes that the memory is not cached on the writers side.
 *
 * If this structure were to be used across the PCIe bus with the buffer
 * living on the other side of the bus, it wouldn't work (would require a
 * wbinvd or use the linux dma streaming buffer API)
 */
struct micscif_rb {
	volatile void *rb_base;
	volatile uint32_t *read_ptr;	/* Points to the read offset */
	volatile uint32_t *write_ptr;	/* Points to the write offset */
	uint32_t size;
	uint32_t current_read_offset;	/* cache it to improve performance */
	uint32_t current_write_offset;	/* cache it to improve performance */
	uint32_t old_current_read_offset;
	uint32_t old_current_write_offset;
};

/**
 * methods used by both
 */
void micscif_rb_init(struct micscif_rb *rb, volatile uint32_t *read_ptr,
		     volatile uint32_t *write_ptr, volatile void *rb_base,
		     const uint32_t size);

/**
 * writer-only methods
 */
/*
 * write a new command, then micscif_rb_commit()
 */
int micscif_rb_write(struct micscif_rb *rb, void *msg, uint32_t size);
/*
 * After write(), then micscif_rb_commit()
 */
void micscif_rb_commit(struct micscif_rb *rb);
/*
 * used on power state change to reset cached pointers
 */
void micscif_rb_reset(struct micscif_rb *rb);

/*
 * Query space available for writing to a RB.
 */
int micscif_rb_space(struct micscif_rb *rb);
/**
 * reader-only methods
 */
/*
 * uses (updates) the cached read pointer to get the next command,
 * so writer doesnt see the command as consumed.
 *
 * Returns number of bytes read
 *
 * Size is IN -- the caller passes in a size (the max size that
 * the function will read out)
 *
 * msg is OUT, but the caller is responsible for allocating space to
 * read into.  The max size this function will read is what is passed
 * into by size, so the buffer pointer to by msg MUST be at least size
 * bytes long.
 */
int micscif_rb_get_next (struct micscif_rb *rb, void *msg, uint32_t size);

/*
 * updates the control block read pointer,
 * which will be visible to the writer so it can re-use the space
 */
void micscif_rb_update_read_ptr(struct micscif_rb *rb);

/*
 * Count the number of empty slots in the RB
 */
uint32_t micscif_rb_count(struct micscif_rb *rb, uint32_t size);

/**
 *  Return the ring buffer module version.
 */
uint16_t micscif_rb_get_version(void);
#endif
