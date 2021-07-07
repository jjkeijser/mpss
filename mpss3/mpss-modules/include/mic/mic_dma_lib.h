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

#ifndef MIC_DMA_LIB_H
#define MIC_DMA_LIB_H

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

/* Program SUD for poll ring */
#define DO_DMA_POLLING	(1<<0)
/* Program SUD for interrupt ring */
#define DO_DMA_INTR	(1<<1)

struct dma_channel;

struct dma_completion_cb {
	void (*dma_completion_func) (uint64_t cookie);
	uint64_t cb_cookie;
	uint8_t *temp_buf;
	uint8_t *temp_buf_to_free;
	bool is_cache;
	uint64_t dst_offset;
	uint64_t tmp_offset;
	struct reg_range_t *dst_window;
	size_t len;
	dma_addr_t temp_phys;
	int remote_node;
	int header_padding;
};

int get_chan_num(struct dma_channel *chan);
/*
 * do_dma - main dma function:  perform a dma memcpy, len bytes from src to dst
 *
 * @chan    - DMA channel to use for the transfer.  The channel can be allocated 
 *            dynamically by calling allocate_dma_channel, or statically by 
 *            reserve_dma_channel.  Using a channel not allocated in this way will
 *            result in undefined behavior.
 * @flags   - ATOMIC, called from an interrupt context (no blocking)
 * @src     - src physical address
 * @dst     - dst physical address
 * @len     - Length of the dma
 * @comp_cb - When the DMA is complete, the struct's function will be called.  NOTE!
 *            comp_cb(cb_cookie) is called from an interrupt context, so the
 *            function must not sleep or block.
 *
 * Return < 1 on error
 * Return 0 on success and DMA is completed
 * Return > 1: DMA has been queued.  Return value can be polled on for completion
 *   (poll cookie).  An example (simplified w/ no error handling).
 *              int cookie = do_dma(...);
 *              while (poll_dma_completion(cookie) == 0);
 *              printf("DMA now complete\n");
 */
int do_dma(struct dma_channel *chan, int flags,
		uint64_t src, uint64_t dst, size_t len,
		struct dma_completion_cb *comp_cb);
/*
 * poll_dma_completion - check if a DMA is complete
 *
 * @poll_cookie - value returned from do_dma
 *
 * Returns
 * < 0 -> error (e.g., invalid cookie)
 * 0 -> DMA pending 
 * 1 -> DMA completed 
 *
 * Note: This is mostly useful after calling do_dma with a NULL comp_cb parameter, as
 *  it will allow the caller to wait for DMA completion.
 */
int poll_dma_completion(int poll_cookie, struct dma_channel *chan);

/*
 * do_status_update: Update physical address location with the value provided.
 *		Ensures all previous DMA descriptors submitted on this DMA
 *		channel are executed.
 * @chan    - DMA channel to use for the transfer. The channel can be allocated
 *            dynamically by calling allocate_dma_channel, or statically by
 *            reserve_dma_channel. Using a channel not allocated in this way will
 *            result in undefined behavior.
 * @phys    - physical address
 * @value   - Value to be programmed
 *
 * Return 0 on success and appropriate error value on error.
 */
int do_status_update(struct dma_channel *chan, uint64_t phys, uint64_t value);

/*
 * get_dma_mark: Obtain current value of DMA mark
 * @chan - DMA channel to use for the transfer. The channel can be allocated
 *            dynamically by calling allocate_dma_channel, or statically by
 *            reserve_dma_channel. Using a channel not allocated in this way will
 *            result in undefined behavior.
 *
 * Return mark.
 */
int get_dma_mark(struct dma_channel *chan);

/*
 * is_current_dma_mark: Check if the dma mark provided is the current DMA mark.
 * @chan - DMA channel
 * @mark - DMA mark
 *
 * Return true on success and false on failure.
 */
bool is_current_dma_mark(struct dma_channel *chan, int mark);

/*
 * program_dma_mark: Increment the current value of the DMA mark for a DMA channel
 * and program an interrupt status update descriptor which ensures that all DMA
 * descriptors programmed until this point in time are completed.
 * @chan - DMA channel to use for the transfer. The channel can be allocated
 *            dynamically by calling allocate_dma_channel, or statically by
 *            reserve_dma_channel. Using a channel not allocated in this way will
 *            result in undefined behavior.
 *
 * Return mark upon success and appropriate negative error value on error.
 */
int program_dma_mark(struct dma_channel *chan);

/*
 * is_dma_mark_wait: Check if the dma mark provided has been processed.
 * @chan - DMA channel
 * @mark - DMA mark
 *
 * Return true on success and false on failure.
 */
bool is_dma_mark_processed(struct dma_channel *chan, int mark);

/*
 * dma_mark_wait: Wait for the dma mark to complete.
 * @chan - DMA channel
 * @mark - DMA mark
 * @is_interruptible - Use wait_event_interruptible() or not.
 *
 * Return 0 on success and appropriate error value on error.
 */
int dma_mark_wait(struct dma_channel *chan, int mark, bool is_interruptible);

#ifndef _MIC_SCIF_
void host_dma_lib_interrupt_handler(struct dma_channel *chan);
#endif

#endif /* MIC_DMA_LIB_H */
