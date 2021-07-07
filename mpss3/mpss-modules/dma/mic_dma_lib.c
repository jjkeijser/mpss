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

#include<linux/module.h>
#include<linux/init.h>
#include<linux/slab.h>
#include<asm/io.h>
#include<linux/mm.h>
#include<linux/kernel.h>
#include<linux/interrupt.h>
#include<linux/proc_fs.h>
#include<linux/bitops.h>
#include<linux/version.h>
#ifdef _MIC_SCIF_
#include <asm/mic/mic_common.h>
#ifdef CONFIG_PAGE_CACHE_DMA
#include <linux/mic_dma/mic_dma_callback.h>
#endif
#endif

#ifndef _MIC_SCIF_
#include <mic/micscif.h>
#include "mic_common.h"
#endif

#include <mic/mic_dma_lib.h>
#include <mic/micscif_smpt.h>
#include <mic/mic_dma_md.h>
#include <mic/mic_dma_api.h>
#include <mic/compl_buf_ring.h>
#include <mic/micscif_smpt.h>
#include <mic/micsboxdefine.h>

MODULE_LICENSE("GPL");

#ifdef MIC_IS_EMULATION
#define DMA_TO		(INT_MAX)
#define DMA_FENCE_TIMEOUT_CNT (INT_MAX)
#else
#define DMA_TO		(5 * HZ)
#define DMA_SLOWEST_BW  (300)  // 300Mbps
// the maximum size for each decriptor entry is 2M
#define DMA_FENCE_TIMEOUT_CNT (2 * MIC_MAX_NUM_DESC_PER_RING /DMA_SLOWEST_BW/ (DMA_TO/HZ))
#endif

#ifdef _MIC_SCIF_
#define MAX_DMA_XFER_SIZE  MIC_MAX_DMA_XFER_SIZE
#else
/* Use 512K as the maximum descriptor transfer size for Host */
#define MAX_DMA_XFER_SIZE  (((1U) * 1024 * 1024) >> 1)
#endif
#ifndef KASSERT
#define KASSERT(x, y, ...)		\
	do {				\
		if(!x)			\
			printk(y, ##__VA_ARGS__);\
		BUG_ON(!x);		\
	} while(0)
#endif
/*
 * Arrary of per device DMA contexts. The card only uses index 0. The host uses one
 * context per card starting from 0.
 */
static struct mic_dma_ctx_t *mic_dma_context[MAX_BOARD_SUPPORTED + 1];
static struct mutex lock_dma_dev_init[MAX_BOARD_SUPPORTED + 1];

enum mic_desc_format_type {
	NOP,
	MEMCOPY,
	STATUS,
	GENERAL,
	KEYNONCECNT,
	KEY
};
char proc_dma_reg[]="mic_dma_registers_";
char proc_dma_ring[]="mic_dma_ring_";

#define PR_PREFIX "DMA_LIB_MI:"
#define DMA_DESC_RING_SIZE MIC_MAX_NUM_DESC_PER_RING
#define MAX_POLLING_BUFFERS DMA_DESC_RING_SIZE

#define DMA_PROC
static void mic_dma_proc_init(struct mic_dma_ctx_t *dma_ctx);
static void mic_dma_proc_uninit(struct mic_dma_ctx_t *dma_ctx);

/*
 * TODO: This is size of s/w interrupt ring.
 * We need to figure out a value so that we don't run out of memory in
 * interrupt ring and at the same time don't waste memory
 */
#define NUM_COMP_BUFS (((PAGE_SIZE/sizeof(struct dma_completion_cb*)) - 10) * 10)

struct intr_compl_buf_ring {
	struct dma_completion_cb **comp_cb_array;
	struct compl_buf_ring ring;
	int old_tail;
};

struct mic_dma_ctx_t;			/* Forward Declaration */

struct dma_channel {
	int ch_num;/*Duplicated in md_mic_dma_chan struct too*/
	struct md_mic_dma_chan *chan;
	atomic_t flags;
	wait_queue_head_t intr_wq;
	wait_queue_head_t access_wq;
	union md_mic_dma_desc *desc_ring_bak;
	union md_mic_dma_desc *desc_ring;
	phys_addr_t desc_ring_phys;
	uint64_t next_write_index; /* next write index into desc ring */
	struct intr_compl_buf_ring intr_ring;
	struct compl_buf_ring poll_ring;
	struct mic_dma_ctx_t *dma_ctx;	  /* Pointer to parent DMA context */
};

/* Per MIC device (per MIC board) DMA context */
struct mic_dma_ctx_t {
	struct dma_channel	dma_channels[MAX_NUM_DMA_CHAN];
	int			last_allocated_dma_channel_num;
	struct mic_dma_device	dma_dev;
	int			device_num;
	atomic_t		ref_count;	/* Reference count */
	atomic_t		ch_num;
};

/* DMA Library Init/Uninit Routines */
static int mic_dma_lib_init(uint8_t *mmio_va_base, struct mic_dma_ctx_t *dma_ctx);
static void mic_dma_lib_uninit(struct mic_dma_ctx_t *dma_ctx);

int get_chan_num(struct dma_channel *chan)
{
	return chan->ch_num;
}
EXPORT_SYMBOL(get_chan_num);

void initdmaglobalvar(void)
{
	memset(mic_dma_context, 0, sizeof(struct mic_dma_ctx_t *) * (MAX_BOARD_SUPPORTED + 1));
}

static void
ack_dma_interrupt(struct dma_channel *ch)
{
	md_mic_dma_chan_mask_intr(&ch->dma_ctx->dma_dev, ch->chan);
	md_mic_dma_chan_unmask_intr(&ch->dma_ctx->dma_dev, ch->chan);
}

/* Returns true if the next write index is "within" bounds */
static inline bool verify_next_write_index(struct dma_channel *ch)
{
	bool ret = false;

	if (ch->next_write_index < DMA_DESC_RING_SIZE)
		ret = true;
	else
		printk(KERN_ERR "%s %d OOB ch_num 0x%x next_write_index 0x%llx\n", 
			__func__, __LINE__, 
			ch->ch_num, ch->next_write_index);
	return ret;
}

/* TODO:
 * See if we can use __get_free_pages or something similar
 * get_free_pages expects a power of 2 number of pages
 */
static void
alloc_dma_desc_ring_mem(struct dma_channel *ch, struct mic_dma_ctx_t *dma_ctx)
{
#ifndef _MIC_SCIF_
	struct pci_dev *pdev;
#endif
	/* Is there any kernel allocator which provides the
	 * option to give the alignment??
	 */
	ch->desc_ring = kzalloc(
			(DMA_DESC_RING_SIZE * sizeof(*ch->desc_ring)) + PAGE_SIZE, GFP_KERNEL);
	ch->desc_ring_bak = ch->desc_ring;
	ch->desc_ring = (union md_mic_dma_desc *)ALIGN(
			(uint64_t)ch->desc_ring, PAGE_SIZE);
#ifdef _MIC_SCIF_
	ch->desc_ring_phys = virt_to_phys(ch->desc_ring);
#else
	micscif_pci_dev(dma_ctx->device_num, &pdev);
	ch->desc_ring_phys = mic_map_single(dma_ctx->device_num - 1, pdev, (void *)ch->desc_ring,
			(DMA_DESC_RING_SIZE * sizeof(*ch->desc_ring)) + PAGE_SIZE);
	BUG_ON(pci_dma_mapping_error(pdev, ch->desc_ring_phys));
#endif
}

/*
 * Call completion cb functions:
 * Take care of case where we allocated temp buf
 */
static void
mic_dma_lib_interrupt_handler(struct dma_channel *chan)
{
	int i = 0;
	int ring_size = chan->intr_ring.ring.size;
	struct dma_completion_cb **temp = chan->intr_ring.comp_cb_array;
	struct dma_completion_cb *cb;
	int new_tail, old_tail;

	if (mic_hw_family(chan->dma_ctx->device_num) == FAMILY_KNC &&
		mic_hw_stepping(chan->dma_ctx->device_num) >= KNC_B0_STEP) {
		unsigned long error = *((uint32_t*)chan->chan->dstat_wb_loc);
		if (unlikely(test_bit(31, &error)))
			printk(KERN_ERR "DMA h/w error - %s %d, dstatwb=%lx\n", 
					__func__, __LINE__, error);
	}
	new_tail = read_tail(&chan->intr_ring.ring);
	old_tail = chan->intr_ring.old_tail;

	for (; i < ring_size && old_tail != new_tail;
		old_tail = incr_rb_index(old_tail, ring_size), i++) {
		cb = (struct dma_completion_cb *)xchg(&temp[old_tail], NULL);
		if (cb) {
			cb->dma_completion_func(cb->cb_cookie);
		}
	}
	chan->intr_ring.old_tail = new_tail;
	update_tail(&chan->intr_ring.ring, new_tail);
	wake_up(&chan->intr_wq);
	if (i == ring_size && old_tail != new_tail) {
		printk(KERN_ERR PR_PREFIX "Something went wrong, old tail = %d, new tail = %d\n", 
							old_tail, new_tail);
	}
}

#ifdef _MIC_SCIF_
/*
 * TODO;
 * Maybe move the logic into slow interrupt handler
 */
static irqreturn_t
dma_interrupt_handler(int irq, void *dev_id)
{
	struct dma_channel *chan = ((struct dma_channel*)dev_id);

	ack_dma_interrupt(chan);
	mic_dma_lib_interrupt_handler(chan);

	return IRQ_HANDLED;
}
#else

#define SBOX_SICR0_DMA(x)  (((x) >> 8) & 0xff)

/*
 * TODO;
 * Maybe move the logic into slow interrupt handler
 */
void
host_dma_interrupt_handler(mic_dma_handle_t dma_handle, uint32_t sboxSicr0reg)
{
	struct mic_dma_ctx_t *dma_ctx = (struct mic_dma_ctx_t *) dma_handle;
	uint32_t dma_chan_id;
	struct dma_channel *ch;

	for (dma_chan_id = 0; dma_chan_id < 8; dma_chan_id++) {
		if (SBOX_SICR0_DMA(sboxSicr0reg) & (0x1 << dma_chan_id)) {
			ch = &dma_ctx->dma_channels[dma_chan_id];
			if (ch->desc_ring)
				host_dma_lib_interrupt_handler(ch);
		}
	}
}

void
host_dma_lib_interrupt_handler(struct dma_channel *chan)
{
	ack_dma_interrupt(chan);
	mic_dma_lib_interrupt_handler(chan);
}
#endif

static void
mi_mic_dma_chan_setup(struct dma_channel *ch, struct mic_dma_ctx_t *dma_ctx)
{
	ch->next_write_index = ch->chan->cached_tail;

	init_ring(&ch->poll_ring, MAX_POLLING_BUFFERS, dma_ctx->device_num);

	ch->intr_ring.comp_cb_array =
		kzalloc(sizeof(*ch->intr_ring.comp_cb_array) * NUM_COMP_BUFS, GFP_KERNEL);
	init_ring(&ch->intr_ring.ring, NUM_COMP_BUFS, dma_ctx->device_num);
	ch->intr_ring.old_tail = 0;
}

static void
mi_mic_dma_chan_destroy(struct dma_channel *ch, struct mic_dma_ctx_t *dma_ctx)
{
	uninit_ring(&ch->intr_ring.ring, dma_ctx->device_num);
	kfree(ch->intr_ring.comp_cb_array);
	uninit_ring(&ch->poll_ring, dma_ctx->device_num);
}

int
open_dma_device(int device_num, uint8_t *mmio_va_base, mic_dma_handle_t* dma_handle)
{
	int result = 0;

	if (device_num >= MAX_BOARD_SUPPORTED)
		return -EINVAL;

	mutex_lock(&lock_dma_dev_init[device_num]);
	if (!mic_dma_context[device_num]) {
		mic_dma_context[device_num] = kzalloc(sizeof(struct mic_dma_ctx_t), GFP_KERNEL);
		BUG_ON(!mic_dma_context[device_num]);

		mic_dma_context[device_num]->device_num = device_num;

		result = mic_dma_lib_init(mmio_va_base, mic_dma_context[device_num]);
		BUG_ON(result);
	}

	atomic_inc(&mic_dma_context[device_num]->ref_count);
	*dma_handle = mic_dma_context[device_num];
	mutex_unlock(&lock_dma_dev_init[device_num]);

	return result;
}
EXPORT_SYMBOL(open_dma_device);

void
close_dma_device(int device_num, mic_dma_handle_t *dma_handle)
{
	struct mic_dma_ctx_t *dma_ctx;

	if (device_num >= MAX_BOARD_SUPPORTED)
		return;

	mutex_lock(&lock_dma_dev_init[device_num]);
	dma_ctx = (struct mic_dma_ctx_t *) *dma_handle;
	if (dma_ctx &&
		atomic_read(&dma_ctx->ref_count) &&
		atomic_dec_and_test(&dma_ctx->ref_count)) {
		mic_dma_lib_uninit(dma_ctx);
		mic_dma_context[dma_ctx->device_num] = 0;
		*dma_handle = NULL;
		kfree(dma_ctx);
	}
	mutex_unlock(&lock_dma_dev_init[device_num]);
}
EXPORT_SYMBOL(close_dma_device);

void mi_mic_dma_chan_set_dstat_wb(struct mic_dma_ctx_t *dma_ctx,
				struct md_mic_dma_chan *chan)
{
#ifndef _MIC_SCIF_
	struct pci_dev *pdev;
#endif
	if (!chan->dstat_wb_phys) {
		chan->dstat_wb_loc = kzalloc(sizeof(uint32_t), GFP_KERNEL);

#ifdef _MIC_SCIF_
		chan->dstat_wb_phys = virt_to_phys(chan->dstat_wb_loc);
#else
		micscif_pci_dev(dma_ctx->device_num, &pdev);
		chan->dstat_wb_phys = mic_map_single(dma_ctx->device_num - 1, pdev, chan->dstat_wb_loc,
			sizeof(uint32_t));
		BUG_ON(pci_dma_mapping_error(pdev, chan->dstat_wb_phys));
#endif
	}
	md_mic_dma_chan_set_dstat_wb(&dma_ctx->dma_dev, chan);
}

void
md_mic_dma_chan_setup(struct mic_dma_ctx_t *dma_ctx, struct dma_channel *ch)
{
	md_mic_dma_chan_unmask_intr(&dma_ctx->dma_dev, ch->chan);

	/*
	 * Disable the channel, update desc ring base and size, write new head
	 * and then enable the channel.
	 */
	if (mic_hw_family(ch->dma_ctx->device_num) == FAMILY_KNC &&
		mic_hw_stepping(ch->dma_ctx->device_num) >= KNC_B0_STEP) {
		mi_mic_dma_chan_set_dstat_wb(dma_ctx, ch->chan);
		md_mic_dma_chan_set_dcherr_msk(&dma_ctx->dma_dev, ch->chan, 0);
	}
	md_mic_dma_chan_set_desc_ring(&dma_ctx->dma_dev, ch->chan,
			ch->desc_ring_phys,
			DMA_DESC_RING_SIZE);

	wmb();

	md_mic_dma_chan_unmask_intr(&dma_ctx->dma_dev, ch->chan);
}

int
mic_dma_lib_init(uint8_t *mmio_va_base, struct mic_dma_ctx_t *dma_ctx)
{
	int i;
#ifdef _MIC_SCIF_
	int ret_value;
#endif
	struct dma_channel *ch;
	enum md_mic_dma_chan_owner owner, currentOwner;

	//pr_debug(PR_PREFIX "Initialized the dma mmio va=%p\n", mmio_va_base);
	// Using this to check where the DMA lib is at for now.
	currentOwner = mmio_va_base == 0 ? MIC_DMA_CHAN_MIC_OWNED : MIC_DMA_CHAN_HOST_OWNED;

	// TODO: multi-card support
	md_mic_dma_init(&dma_ctx->dma_dev, mmio_va_base);

	for (i = 0 ; i < MAX_NUM_DMA_CHAN; i++) {
		ch = &dma_ctx->dma_channels[i];

		/* Initialize pointer to parent */
		ch->dma_ctx = dma_ctx;

		owner = i > __LAST_HOST_CHAN_NUM ? MIC_DMA_CHAN_MIC_OWNED
						: MIC_DMA_CHAN_HOST_OWNED;

		// This has to be done from card side
		ch->chan = md_mic_dma_request_chan(&dma_ctx->dma_dev, owner);
		KASSERT((ch->chan != NULL), "dummy\n");
		ch->ch_num = ch->chan->ch_num;

#ifdef _MIC_SCIF_
		/*
		 * Host driver would have executed by now and thus setup the
		 * desc. ring
		 */
		if (ch->chan->owner == MIC_DMA_CHAN_HOST_OWNED)
			md_mic_dma_enable_chan(&dma_ctx->dma_dev, i, true);
#endif

		atomic_set(&(ch->flags), CHAN_INUSE); // Mark as used by default
		if (currentOwner == owner) {
			alloc_dma_desc_ring_mem(ch, dma_ctx);

#ifdef _MIC_SCIF_ // DMA now shares the IRQ handler with other system interrupts
			ret_value = request_irq(i, dma_interrupt_handler, IRQF_DISABLED,
						"dma channel", ch);
			ret_value = ret_value;
			//pr_debug(PR_PREFIX "Interrupt handler ret value for  chan %d = %d\n", i, ret_value);
#endif
			md_mic_dma_chan_setup(dma_ctx, ch);

			mi_mic_dma_chan_setup(ch, dma_ctx);

			init_waitqueue_head(&ch->intr_wq);
			init_waitqueue_head(&ch->access_wq);
			// Only mark owned channel to be available
			atomic_set(&(ch->flags), CHAN_AVAILABLE);
			md_mic_dma_print_debug(&dma_ctx->dma_dev, ch->chan);
		} else {
			ch->desc_ring = NULL;
		}
	}

	/* Initialize last_allocated_dma_channel */
	dma_ctx->last_allocated_dma_channel_num = -1;
	//pr_debug(PR_PREFIX "Initialized the dma channels\n");
	mic_dma_proc_init(dma_ctx);
	return 0;
}

void
mic_dma_lib_uninit(struct mic_dma_ctx_t *dma_ctx)
{
	int i;
	struct dma_channel *ch;
#ifndef _MIC_SCIF_
	struct pci_dev *pdev;
#endif

	mic_dma_proc_uninit(dma_ctx);
	for (i = 0 ; i < MAX_NUM_DMA_CHAN; i++) {
		ch = &dma_ctx->dma_channels[i];
		if (!ch->desc_ring)
			continue;
		drain_dma_intr(ch);
		/* Request the channel but don't free it. Errors are okay */
		request_dma_channel(ch);
#ifdef _MIC_SCIF_ // DMA now shares the IRQ handler with other system interrupts
		free_irq(i, ch);
#endif
		mi_mic_dma_chan_destroy(ch, dma_ctx);
#ifndef _MIC_SCIF_
		micscif_pci_dev(dma_ctx->device_num, &pdev);
		mic_unmap_single(dma_ctx->device_num - 1, pdev, ch->desc_ring_phys,
			(DMA_DESC_RING_SIZE * sizeof(*ch->desc_ring)) + PAGE_SIZE);
#endif

        kfree(ch->desc_ring_bak);
		ch->desc_ring_bak = NULL; 
		ch->desc_ring = NULL;
		if (mic_hw_family(ch->dma_ctx->device_num) == FAMILY_KNC &&
			mic_hw_stepping(dma_ctx->device_num) >= KNC_B0_STEP) {
#ifndef _MIC_SCIF_
			mic_unmap_single(dma_ctx->device_num - 1, pdev, ch->chan->dstat_wb_phys,
				sizeof(uint32_t));
#endif
			kfree(ch->chan->dstat_wb_loc);
			ch->chan->dstat_wb_loc = NULL;
			ch->chan->dstat_wb_phys = 0;
		}
		md_mic_dma_free_chan(&dma_ctx->dma_dev, ch->chan);
	}
#ifndef MIC_IS_EMULATION
	/* Ensure that all waiters for DMA channels time out */
	msleep(DMA_TO/HZ * 1000);
#endif
	md_mic_dma_uninit(&dma_ctx->dma_dev);
	//pr_debug(PR_PREFIX "Uninitialized the dma channels\n");
}

/*
 * reserve_dma_channel - reserve a given dma channel for exclusive use
 *
 * @dma_handle - handle to DMA device returned by open_dma_device
 * @chan_num - Channel number to be reserved
 * @chan     - set to point to the dma channel reserved by the call
 *
 * Returns < 1 on error (errorno)
 * Returns 0 on success
 *
 * NOTES: Should this function sleep waiting for the lock?
 * TODO:
 * Maybe there should be a blocking and non-blocking versions of this function
 */
int
reserve_dma_channel(mic_dma_handle_t dma_handle, int chan_num, struct dma_channel **chan)
{
	struct mic_dma_ctx_t *dma_ctx = (struct mic_dma_ctx_t *) dma_handle;

	/*
	 * Do we need to do acquire the lock for statically allocated channels?
	 * I am assuming we dont have to lock
	 */
	if (CHAN_AVAILABLE == atomic_cmpxchg(&(dma_ctx->dma_channels[chan_num].flags), 
						   CHAN_AVAILABLE, CHAN_INUSE)) {
		*chan = &dma_ctx->dma_channels[chan_num];
		return 0;
	}
	return -1;
}
EXPORT_SYMBOL(reserve_dma_channel);

/*
 * allocate_dma_channel - dynamically allocate a dma channel (for a short while). Will
 *    search for, choose, and lock down one channel for use by the calling thread.
 *
 * @dma_handle - handle to DMA device returned by open_dma_device
 * @chan - Returns the dma_channel pointer that was allocated by the call
 *
 * Returns < 1 on error
 * Returns 0 on success
 *
 * NOTE:  This function grabs a lock before exiting -- the calling thread MUST NOT
 *  sleep, and must call free_dma_channel before returning to user-space or switching
 *  volantarily to another thread.  Similarly, this function cannot be called from
 *  an interrupt context at this time.
 *
 *  TODO: How do we pick a dma channel?
 *  For now I am doing it in round robin fashion.
 */
int
allocate_dma_channel(mic_dma_handle_t dma_handle, struct dma_channel **chan)
{
	struct mic_dma_ctx_t *dma_ctx = (struct mic_dma_ctx_t *) dma_handle;
	int i, j;

	if (!dma_ctx)
		return -ENODEV;

	j = dma_ctx->last_allocated_dma_channel_num + 1;

	for (i = 0; i < MAX_NUM_DMA_CHAN; i++, j++) {
		if (CHAN_AVAILABLE == atomic_cmpxchg(&(dma_ctx->dma_channels[j %
							     MAX_NUM_DMA_CHAN].flags), 
							   CHAN_AVAILABLE, CHAN_INUSE)) {
			*chan = &(dma_ctx->dma_channels[j % MAX_NUM_DMA_CHAN]);
			dma_ctx->last_allocated_dma_channel_num = j % MAX_NUM_DMA_CHAN;
			return 0;
		}
	}
	return -1;
}
EXPORT_SYMBOL(allocate_dma_channel);

/*
 * request_dma_channel - Request a specific DMA channel.
 *
 * @chan - Returns the dma_channel pointer that was requested
 *
 * Returns: 0 on success and -ERESTARTSYS if the wait was interrupted
 * or -EBUSY if the channel was not available.
 *
 * NOTE:  This function must call free_dma_channel before returning to
 * user-space.
 */
int request_dma_channel(struct dma_channel *chan)
{
	int ret;

	ret = wait_event_interruptible_timeout(chan->access_wq, 
		CHAN_AVAILABLE == atomic_cmpxchg(&chan->flags, 
		CHAN_AVAILABLE, CHAN_INUSE), DMA_TO);
	if (!ret) {
		printk(KERN_ERR "%s %d TO chan 0x%x\n", __func__, __LINE__, chan->ch_num);
		ret = -EBUSY;
	}
	if (ret > 0)
		ret = 0;
	return ret;
}
EXPORT_SYMBOL(request_dma_channel);

/*
 * free_dma_channel - after allocating a channel, used to
 *                 free the channel after DMAs are submitted
 *
 * @chan - pointer to the dma_channel struct that was allocated
 *
 * Returns 0 on success, < 1 on error (errorno)
 *
 * NOTE: This function must be called after all do_dma calls are finished,
 *  but can be called before the DMAs actually complete (as long as the comp_cb()
 *  handler in do_dma don't refer to the dma_channel struct).  If called with a
 *  dynamically allocated dma_chan, the caller must be the thread that called
 *  allocate_dma_chan.  When operating on a dynamic channel, free unlocks the
 *  mutex locked in allocate.  Statically allocated channels cannot be freed,
 *  and calling this function with that type of channel will return an error.
 */
int
free_dma_channel(struct dma_channel *chan)
{
	/*
	 * Why can't we use this function with channels that were statically allocated??
	 */
	BUG_ON(CHAN_INUSE !=
		atomic_cmpxchg(&chan->flags, CHAN_INUSE, CHAN_AVAILABLE));
	wake_up(&chan->access_wq);
	return 0;
}
EXPORT_SYMBOL(free_dma_channel);

static __always_inline uint32_t
get_dma_tail_pointer(struct dma_channel *chan)
{
	struct mic_dma_device *dma_dev;
	dma_dev = &chan->dma_ctx->dma_dev;
	return md_mic_dma_chan_read_tail(dma_dev, chan->chan);
}
/*
 * Return -1 in case of error
 */
static int
program_memcpy_descriptors(struct dma_channel *chan, uint64_t src, uint64_t dst, size_t len)
{
	size_t current_transfer_len;
	bool is_astep = false;
	unsigned long ts = jiffies;

	if (mic_hw_family(chan->dma_ctx->device_num) == FAMILY_KNC) {
		if (mic_hw_stepping(chan->dma_ctx->device_num) == KNC_A_STEP)
			is_astep = true;
	} else {
		is_astep = true;
	}
	do {
		current_transfer_len = (len > MAX_DMA_XFER_SIZE) ?
					MAX_DMA_XFER_SIZE : len;

		ts = jiffies;
		while (!md_avail_desc_ring_space(&chan->dma_ctx->dma_dev, is_astep, chan->chan,
						  (uint32_t)chan->next_write_index, 1)) {
				if (time_after(jiffies,ts + DMA_TO)) {
					printk(KERN_ERR "%s %d TO chan 0x%x\n", __func__, __LINE__, chan->ch_num);
					return -ENOMEM;
				}
		}

		//pr_debug("src_phys=0x%llx, dst_phys=0x%llx, size=0x%zx\n", src_phys_addr, dst_phys_addr, current_transfer_len);
		md_mic_dma_memcpy_desc(&chan->desc_ring[chan->next_write_index],
					    src, dst, current_transfer_len);
		chan->next_write_index = incr_rb_index((int)chan->next_write_index,
						       chan->chan->num_desc_in_ring);
		len -= current_transfer_len;
		dst = dst + current_transfer_len;
		src = src + current_transfer_len;
	} while(len > 0);

	return 0;
}

/*
 * do_dma - main dma function:  perform a dma memcpy, len bytes from src to dst
 *
 * @chan    - DMA channel to use for the transfer.  The channel can be allocated
 *            dynamically by calling allocate_dma_chan, or statically by
 *            reserve_dma_chan.  Using a channel not allocated in this way will
 *            result in undefined behavior.
 * @flags   - ATOMIC, called from an interrupt context (no blocking)
 * @src     - src physical address
 * @dst     - dst physical address
 * @len     - Length of the dma
 * @comp_cb - When the DMA is complete, the struct's function will be called.  NOTE!
 *            comp_cb(cb_cookie) is called from an interrupt context, so the
 *            function must not sleep or block.
 *
 * TODO: Figure out proper value instead of -2
 * Return < 0 on error
 * Return = -2 copy was done successfully, no need to wait
 * Return >= 0: DMA has been queued.  Return value can be polled on for completion
 *   if DO_DMA_POLLING was sent in flags
 *   (poll cookie).  An example (simplified w/ no error handling).
 *              int cookie = do_dma(...);
 *              while (poll_dma_completion(cookie) == 0);
 *              printf("DMA now complete\n");
 */
int
do_dma(struct dma_channel *chan, int flags, uint64_t src,
       uint64_t dst, size_t len, struct dma_completion_cb *comp_cb)
{
	/*
	 * TODO:
	 * Do we need to assert the ownership of channel??
	 */
	int poll_ring_index = -1;
	int intr_ring_index = -1;
	uint32_t num_status_desc = 0;
	bool is_astep = false;
	unsigned long ts = jiffies;

	might_sleep();
	if (flags & DO_DMA_INTR && !comp_cb)
		return -EINVAL;

	if (!verify_next_write_index(chan))
		return -ENODEV;

	//pr_debug(PR_PREFIX "Current transfer src = 0x%llx,dst = 0x%llx, len = 0x%zx\n", src, dst, len);
	if (flags & DO_DMA_INTR) {
		int err;
		err = wait_event_interruptible_timeout(chan->intr_wq, 
		(-1 != (intr_ring_index = allocate_buffer(&chan->intr_ring.ring))), 
		DMA_TO);
		if (!err) {
			printk(KERN_ERR "%s %d TO chan 0x%x\n", __func__, __LINE__, chan->ch_num);
			err = -ENOMEM;
		}
		if (err > 0)
			err = 0;
		if (!err) {
			chan->intr_ring.comp_cb_array[intr_ring_index] = comp_cb;
			num_status_desc++;
#ifdef CONFIG_MK1OM
			num_status_desc++;
#endif
		} else {
			return err;
		}
		//pr_debug(PR_PREFIX "INTR intr_ring_index=%d, chan_num=%lx\n", intr_ring_index, (chan - dma_channels));
	}

	if (flags & DO_DMA_POLLING) {
		poll_ring_index = allocate_buffer(&chan->poll_ring);
		if (-1 == poll_ring_index)
			return -ENOMEM;
		num_status_desc++;
		//pr_debug(PR_PREFIX "polling poll_ring_index=%d\n", poll_ring_index);
	}
	if (len && -ENOMEM == program_memcpy_descriptors(chan, src, dst, len)) {
		//pr_debug(PR_PREFIX "ERROR: do_dma: No available space from program_memcpy_descriptors\n");
		return -ENOMEM;
	}

	if (mic_hw_family(chan->dma_ctx->device_num) == FAMILY_KNC) {
		if (mic_hw_stepping(chan->dma_ctx->device_num) == KNC_A_STEP)
			is_astep = true;
	} else {
		is_astep = true;
	}

	ts = jiffies;

	while (num_status_desc && num_status_desc > md_avail_desc_ring_space(&chan->dma_ctx->dma_dev,
						is_astep, chan->chan, (uint32_t)chan->next_write_index, num_status_desc)) {
		if (time_after(jiffies,ts + DMA_TO)) {
			printk(KERN_ERR "%s %d TO chan 0x%x\n", __func__, __LINE__, chan->ch_num);
			return -ENOMEM;
		}
		//pr_debug(PR_PREFIX "ERROR: do_dma: No available space from md_avail_desc_ring_space\n");
	}

	if (flags & DO_DMA_POLLING) {
		incr_head(&chan->poll_ring);
		md_mic_dma_prep_status_desc(&chan->desc_ring[chan->next_write_index],
				poll_ring_index,
				chan->poll_ring.tail_phys,
				false);
		chan->next_write_index = incr_rb_index((int)chan->next_write_index,
						       chan->chan->num_desc_in_ring);
	}

	if (flags & DO_DMA_INTR) {
		incr_head(&chan->intr_ring.ring);
#ifdef CONFIG_MK1OM
		md_mic_dma_prep_status_desc(&chan->desc_ring[chan->next_write_index],
				intr_ring_index,
				chan->intr_ring.ring.tail_phys,
				false);
		chan->next_write_index = incr_rb_index((int)chan->next_write_index,
						       chan->chan->num_desc_in_ring);
#endif
		md_mic_dma_prep_status_desc(&chan->desc_ring[chan->next_write_index],
				intr_ring_index,
				chan->intr_ring.ring.tail_phys,
				true);
		chan->next_write_index = incr_rb_index((int)chan->next_write_index,
						       chan->chan->num_desc_in_ring);
	}

	/*
	 * TODO:
	 * Maybe it is better if we update the head pointer for every descriptor??
	 */
	md_mic_dma_chan_write_head(&chan->dma_ctx->dma_dev, chan->chan, (uint32_t)chan->next_write_index);
	//pr_debug(PR_PREFIX "in HW chan->next_write_index=%lld\n", chan->next_write_index);

	if (DO_DMA_POLLING & flags)
		return poll_ring_index;
	return 0;
}
EXPORT_SYMBOL(do_dma);

/*
 * poll_dma_completion - check if a DMA is complete
 *
 * @poll_cookie - value returned from do_dma
 *
 * Returns
 * 0 -> DMA pending
 * 1 -> DMA completed
 *
 * Note: This is mostly useful after calling do_dma with a NULL comp_cb parameter, as
 *  it will allow the caller to wait for DMA completion.
 */
int
poll_dma_completion(int poll_cookie, struct dma_channel *chan)
{
	if (!chan)
		return -EINVAL;
	/*
	 * In case of interrupts the ISR runs and reads the value
	 * of the tail location. If we are polling then we need
	 * to read the value of the tail location before checking
	 * if the entry is processed.
	 */
	chan->poll_ring.tail = read_tail(&chan->poll_ring);
	return is_entry_processed(&chan->poll_ring, poll_cookie);
}
EXPORT_SYMBOL(poll_dma_completion);

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
 */
int do_status_update(struct dma_channel *chan, uint64_t phys, uint64_t value)
{
	unsigned long ts = jiffies;
	bool is_astep = false;

	if (!verify_next_write_index(chan))
		return -ENODEV;

	if (mic_hw_family(chan->dma_ctx->device_num) == FAMILY_KNC) {
		if (mic_hw_stepping(chan->dma_ctx->device_num) == KNC_A_STEP)
			is_astep = true;
	} else {
		is_astep = true;
	}
	/*
	 * TODO:
	 * Do we need to assert the ownership of channel??
	 */
	ts = jiffies;
	while (!md_avail_desc_ring_space(&chan->dma_ctx->dma_dev,
		is_astep, chan->chan, (uint32_t) chan->next_write_index, 1)) {
		cpu_relax();
		if (time_after(jiffies,ts + DMA_TO)) {
			printk(KERN_ERR "%s %d TO chan 0x%x\n", __func__, __LINE__, chan->ch_num);
			return -EBUSY;
		}
	}

	md_mic_dma_prep_status_desc(&chan->desc_ring[chan->next_write_index],
			value,
			phys,
			false);

	chan->next_write_index = incr_rb_index((int)chan->next_write_index,
			chan->chan->num_desc_in_ring);

	md_mic_dma_chan_write_head(&chan->dma_ctx->dma_dev,
			chan->chan, (uint32_t)chan->next_write_index);
	return 0;
}
EXPORT_SYMBOL(do_status_update);

/*
 * get_dma_mark: Obtain current value of DMA mark
 * @chan - DMA channel to use for the transfer. The channel can be allocated
 *            dynamically by calling allocate_dma_channel, or statically by
 *            reserve_dma_channel. Using a channel not allocated in this way will
 *            result in undefined behavior.
 */
int get_dma_mark(struct dma_channel *chan)
{
	if (chan)
		return chan->intr_ring.ring.head;
	else
		return -1;
}
EXPORT_SYMBOL(get_dma_mark);

/*
 * program_dma_mark: Increment the current value of the DMA mark for a DMA channel
 * and program an interrupt status update descriptor which ensures that all DMA
 * descriptors programmed uptil this point in time are completed.
 * @chan - DMA channel to use for the transfer. The channel can be allocated
 *            dynamically by calling allocate_dma_channel, or statically by
 *            reserve_dma_channel. Using a channel not allocated in this way will
 *            result in undefined behavior.
 */
int program_dma_mark(struct dma_channel *chan)
{
	/*
	 * TODO:
	 * Do we need to assert the ownership of channel??
	 */
	int intr_ring_index;
	int err;
	unsigned long ts = jiffies;
	uint32_t num_status_desc = 1;
	bool is_astep = false;

	if (!verify_next_write_index(chan))
		return -ENODEV;

	if (mic_hw_family(chan->dma_ctx->device_num) == FAMILY_KNC) {
		if (mic_hw_stepping(chan->dma_ctx->device_num) == KNC_A_STEP)
			is_astep = true;
	} else {
		is_astep = true;
	}
	might_sleep();
	err = wait_event_interruptible_timeout(chan->intr_wq, 
	(-1 != (intr_ring_index = allocate_buffer(&chan->intr_ring.ring))), 
	DMA_TO);
	if (!err)
		err = -EBUSY;
	if (err > 0)
		err = 0;
	if (err)
		return err;

#ifdef CONFIG_MK1OM
	num_status_desc++;
#endif
	ts = jiffies;
	while (num_status_desc > md_avail_desc_ring_space(&chan->dma_ctx->dma_dev,
			is_astep, chan->chan, (uint32_t)chan->next_write_index, num_status_desc)) {
		cpu_relax();
		if (time_after(jiffies,ts + DMA_TO)) {
			printk(KERN_ERR "%s %d TO chan 0x%x\n", __func__, __LINE__, chan->ch_num);
			return -EBUSY;
		}
	}

	chan->intr_ring.comp_cb_array[intr_ring_index] = NULL;

	incr_head(&chan->intr_ring.ring);
#ifdef CONFIG_MK1OM
	md_mic_dma_prep_status_desc(&chan->desc_ring[chan->next_write_index],
			intr_ring_index,
			chan->intr_ring.ring.tail_phys,
			false);
	chan->next_write_index = incr_rb_index((int)chan->next_write_index,
			chan->chan->num_desc_in_ring);
#endif
	md_mic_dma_prep_status_desc(&chan->desc_ring[chan->next_write_index],
			intr_ring_index,
			chan->intr_ring.ring.tail_phys,
			true);
	chan->next_write_index = incr_rb_index((int)chan->next_write_index,
			chan->chan->num_desc_in_ring);

	md_mic_dma_chan_write_head(&chan->dma_ctx->dma_dev, chan->chan, (uint32_t)chan->next_write_index);
	return intr_ring_index;
}
EXPORT_SYMBOL(program_dma_mark);

/*
 * is_current_dma_mark: Check if the dma mark provided is the current DMA mark.
 * @chan - DMA channel
 * @mark - DMA mark
 *
 * Return true on success and false on failure.
 */
bool is_current_dma_mark(struct dma_channel *chan, int mark)
{
	return (get_dma_mark(chan) == mark);
}
EXPORT_SYMBOL(is_current_dma_mark);

/*
 * is_dma_mark_processed: Check if the dma mark provided has been processed.
 * @chan - DMA channel
 * @mark - DMA mark
 *
 * Return true on success and false on failure.
 */
bool is_dma_mark_processed(struct dma_channel *chan, int mark)
{
	return is_entry_processed(&chan->intr_ring.ring, mark);
}
EXPORT_SYMBOL(is_dma_mark_processed);

/*
 * dma_mark_wait:
 * @chan - DMA channel
 * @mark - DMA mark
 * @is_interruptible - Use wait_event_interruptible() or not.
 *
 * Wait for the dma mark to complete.
 * Return 0 on success and appropriate error value on error.
 */
int dma_mark_wait(struct dma_channel *chan, int mark, bool is_interruptible)
{
	int err = 0;
	uint32_t prev_tail = 0, new_tail;
	uint32_t count = 0;

	if (chan) {
		might_sleep();
__retry:
		if (is_interruptible)
			err = wait_event_interruptible_timeout(
				chan->intr_wq, 
				is_dma_mark_processed(chan, mark), 
				DMA_TO);
		else
			err = wait_event_timeout(chan->intr_wq, 
				is_dma_mark_processed(chan, mark), DMA_TO);

		if (!err) { // 0 is timeout
			new_tail = get_dma_tail_pointer(chan);
			if ((count <= DMA_FENCE_TIMEOUT_CNT) &&
				(!count || new_tail != prev_tail)) {  // For performance, prev_tail is not read at the begining
					prev_tail = new_tail;
					count++;
					pr_debug("DMA fence wating is still ongoing, waiting for %d seconds\n", DMA_TO/HZ *count);
					goto __retry;
			} else {
				printk(KERN_ERR "%s %d TO chan 0x%x\n", __func__, __LINE__, chan->ch_num);
				err = -EBUSY;
			}
		}
		if (err > 0)
			err = 0;
	}
	return err;
}
EXPORT_SYMBOL(dma_mark_wait);

/*
 * drain_dma_poll - Drain all outstanding DMA operations for a particular
 * DMA channel via polling.
 * @chan - DMA channel
 * Return 0 on success and -errno on error.
 */
int drain_dma_poll(struct dma_channel *chan)
{
	int cookie, err;
	unsigned long ts;
	uint32_t prev_tail = 0, new_tail, count = 0;
	if (chan) {
		if ((err = request_dma_channel(chan)))
			goto error;
		if ((cookie = do_dma(chan,
			DO_DMA_POLLING, 0, 0, 0, NULL)) < 0) {
			err = cookie;
			free_dma_channel(chan);
			goto error;
		}
		free_dma_channel(chan);
		ts = jiffies;
		while (1 != poll_dma_completion(cookie, chan)) {
			cpu_relax();
			if (time_after(jiffies,ts + DMA_TO)) {
				new_tail = get_dma_tail_pointer(chan);
				if ((!count || new_tail != prev_tail) && (count <= DMA_FENCE_TIMEOUT_CNT)) {
					prev_tail = new_tail;
					ts = jiffies;
					count++;
					pr_debug("polling DMA is still ongoing,  wating for %d seconds\n", DMA_TO/HZ * count);
				} else {
					err = -EBUSY;
					break;
				}
			}
		}
error:
		if (err)
			printk(KERN_ERR "%s %d err %d\n", __func__, __LINE__, err);
	} else {
		err = -EINVAL;
	}
	return err;
}
EXPORT_SYMBOL(drain_dma_poll);

/*
 * drain_dma_intr - Drain all outstanding DMA operations for a particular
 * DMA channel via interrupt based blocking wait.
 * @chan - DMA channel
 * Return 0 on success and -errno on error.
 */
int drain_dma_intr(struct dma_channel *chan)
{
	int cookie, err;

	if (chan) {
		if ((err = request_dma_channel(chan)))
			goto error;
		if ((cookie = program_dma_mark(chan)) < 0) {
			err = cookie;
			free_dma_channel(chan);
			goto error;
		}
		free_dma_channel(chan);
		err = dma_mark_wait(chan, cookie, false);
error:
		if (err)
			printk(KERN_ERR "%s %d err %d\n", __func__, __LINE__, err);
	} else {
		err = -EINVAL;
	}
	return err;
}
EXPORT_SYMBOL(drain_dma_intr);

/*
 * drain_dma_global - Drain all outstanding DMA operations for
 * all online DMA channel.
 * Return none
 */
int drain_dma_global(mic_dma_handle_t dma_handle)
{
	int i, err = -EINVAL;
	struct dma_channel *chan;
	struct mic_dma_ctx_t *dma_ctx = (struct mic_dma_ctx_t *)dma_handle;

	if (!dma_ctx)
		return err;

	might_sleep();
	for (i = 0 ; i < MAX_NUM_DMA_CHAN; i++) {
		chan = &dma_ctx->dma_channels[i];
		if (chan->desc_ring == NULL)
			continue;
		if ((err = drain_dma_intr(chan)))
			break;
	}
	return err;
}
EXPORT_SYMBOL(drain_dma_global);

#ifdef _MIC_SCIF_
/*
 * dma_suspend: DMA tasks before transition to low power state.
 * @dma_handle: Handle for a DMA driver context.
 *
 * Perform the following tasks before the device transitions
 * to a low power state:
 * 1) Store away the DMA descriptor ring physical address base for
 * all DMA channels (both host/uOS owned) since the value would be
 * required to reinitialize the DMA channels upon transition from
 * low power to active state.
 *
 * Return: none
 * Notes: Invoked only on MIC.
 */
void dma_suspend(mic_dma_handle_t dma_handle)
{
	int i;
	struct dma_channel *ch;
	struct mic_dma_ctx_t *dma_ctx = (struct mic_dma_ctx_t *)dma_handle;
	struct mic_dma_device *dma_dev = &dma_ctx->dma_dev;

	for (i = 0; i < MAX_NUM_DMA_CHAN; i++) {
		ch = &dma_ctx->dma_channels[i];
		ch->desc_ring_phys =
			md_mic_dma_chan_get_desc_ring_phys(dma_dev, ch->chan);
		ch->chan->dstat_wb_phys =
			md_mic_dma_chan_get_dstatwb_phys(dma_dev, ch->chan);
	}
}
EXPORT_SYMBOL(dma_suspend);

/*
 * dma_resume: DMA tasks after wake up from low power state.
 * @dma_handle: Handle for a DMA driver context.
 *
 * Performs the following tasks before the device transitions
 * from a low power state to active state:
 * 1) As a test, reset the value in DMA configuration register.
 * 2) Reset the next_write_index for the DMA descriptor ring to 0
 * since the DMA channel will be reset shortly.
 * 3) Reinitialize the DMA MD layer for the channel.
 *
 * Return: none
 * Notes:
 * Notes: Invoked only on MIC.
 */
void dma_resume(mic_dma_handle_t dma_handle)
{
	int i;
	struct dma_channel *ch;
	struct mic_dma_ctx_t *dma_ctx = (struct mic_dma_ctx_t *)dma_handle;
	struct mic_dma_device *dma_dev = &dma_ctx->dma_dev;

	/* TODO: Remove test write to SBOX_DCR */
	mic_sbox_write_mmio(dma_dev->mm_sbox, SBOX_DCR, 0);
	for (i = 0; i < MAX_NUM_DMA_CHAN; i++) {
		ch = &dma_ctx->dma_channels[i];
		ch->next_write_index = 0;
		md_mic_dma_chan_init_attr(dma_dev, ch->chan);
		md_mic_dma_chan_setup(dma_ctx, ch);
	}
}
EXPORT_SYMBOL(dma_resume);

#else

/*
 * dma_prep_suspend: DMA tasks required on host before a device can transition
 * to a low power state.
 * @dma_handle: Handle for a DMA driver context.
 *
 * Performs the following tasks on the host before the device can be allowed
 * to transiti to a low power state.
 * 1) Reset the next_Write_index for the DMA descriptor ring to 0
 * since the DMA channel will be reset shortly. This is required primarily
 * for Host owned DMA channels since MIC does not have access to this
 * information.
 * Return: none
 * Invoked only on Host.
 */
void dma_prep_suspend(mic_dma_handle_t dma_handle)
{
	int i;
	struct dma_channel *ch;
	struct mic_dma_ctx_t *dma_ctx = (struct mic_dma_ctx_t *)dma_handle;

	for (i = 0; i < MAX_NUM_DMA_CHAN; i++) {
		ch = &dma_ctx->dma_channels[i];
		ch->next_write_index = 0;
	}
}
EXPORT_SYMBOL(dma_prep_suspend);
#endif

#ifdef CONFIG_PAGE_CACHE_DMA
#ifdef _MIC_SCIF_
static const struct dma_operations dma_operations_fast_copy = {
	.do_dma               = do_dma,
	.poll_dma_completion  = poll_dma_completion,
	.free_dma_channel     = free_dma_channel,
	.open_dma_device      = open_dma_device,
	.close_dma_device     = close_dma_device,
	.allocate_dma_channel = allocate_dma_channel,
	.program_descriptors  = program_memcpy_descriptors,
	.do_dma_polling				= DO_DMA_POLLING,
};

static const struct file_dma fdma_callback = {
	.dmaops = &dma_operations_fast_copy,
};
#endif
#endif

#ifdef _MIC_SCIF_
static int
#else
int
#endif
mic_dma_init(void)
{
	int i;

	for (i = 0; i < MAX_BOARD_SUPPORTED; i++)
		mutex_init (&lock_dma_dev_init[i]);
#ifdef CONFIG_PAGE_CACHE_DMA
#ifdef _MIC_SCIF_
	register_dma_for_fast_copy(&fdma_callback);
#endif
#endif
	return 0;
}

#ifdef _MIC_SCIF_
static void mic_dma_uninit(void)
{
#ifdef CONFIG_PAGE_CACHE_DMA
	unregister_dma_for_fast_copy();
#endif
}

module_init(mic_dma_init);
module_exit(mic_dma_uninit);
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
static int
mic_dma_proc_ring_show(struct seq_file *m, void *data)
{
	struct mic_dma_ctx_t *dma_ctx = m->private;
	mic_ctx_t *mic_ctx = get_per_dev_ctx(dma_ctx->device_num - 1);
	int i, err;
	struct compl_buf_ring *ring;

	if ((err = micpm_get_reference(mic_ctx, true))) {
		printk(KERN_ERR "%s %d: unable to get micpm reference: %d\n", 
				 __func__, __LINE__, err);
		return err;
	}

	seq_printf(m, "Intr rings\n");
	seq_printf(m, "%-10s%-12s%-12s%-12s%-25s%-18s%-25s\n",
		       "Chan", "Head", "Tail", "Size", "Tail loc", "Actual tail", "In Use");
	for (i = first_dma_chan(); i <= last_dma_chan(); i++) {
		ring = &dma_ctx->dma_channels[i].intr_ring.ring;
		seq_printf(m, "%-#10x%-#12x%-#12x%-#12x%-#25llx%-#18x%-#18x\n",
			i, ring->head, ring->tail, ring->size,
			ring->tail_location, *(int*)ring->tail_location,
			atomic_read(&dma_ctx->dma_channels[i].flags));
	}
	seq_printf(m, "Poll rings\n");
	seq_printf(m, "%-10s%-12s%-12s%-12s%-25s%-18s\n",
		       "Chan", "Head", "Tail", "Size", "Tail loc", "Actual tail");
	for (i = first_dma_chan(); i <= last_dma_chan(); i++) {
		ring = &dma_ctx->dma_channels[i].poll_ring;
		seq_printf(m, "%-#10x%-#12x%-#12x%-#12x%-#25llx%-#18x\n",
			       i, ring->head, ring->tail, ring->size,
			       ring->tail_location, *(int*)ring->tail_location);
	}
	seq_printf(m, "Next_Write_Index\n");
	seq_printf(m, "%-10s%-12s\n", "Chan", "Next_Write_Index");
	for (i = 0; i < MAX_NUM_DMA_CHAN; i++) {
		seq_printf(m, "%-#10x%-#12llx\n",
			       i, dma_ctx->dma_channels[i].next_write_index);
	}
	micpm_put_reference(mic_ctx);
	return 0;
}

static int
mic_dma_proc_ring_open(struct inode *inode, struct file *file)
{
	return single_open(file, mic_dma_proc_ring_show, PDE_DATA(inode));
}

static int
mic_dma_proc_reg_show(struct seq_file *m, void *data)
{
	int i, j, chan_num, size, dtpr, err;
	struct mic_dma_ctx_t *dma_ctx = m->private;
	mic_ctx_t *mic_ctx = get_per_dev_ctx(dma_ctx->device_num - 1);
	struct mic_dma_device *dma_dev = &dma_ctx->dma_dev;
	struct dma_channel *curr_chan;
	union md_mic_dma_desc desc;

	if ((err = micpm_get_reference(mic_ctx, true))) {
		printk(KERN_ERR "%s %d: unable to get micpm reference: %d\n", 
				 __func__, __LINE__, err);
		return err;
	}

	seq_printf(m, "========================================"
				"=======================================\n");
	seq_printf(m, "SBOX_DCR: %#x\n",
				mic_sbox_read_mmio(dma_dev->mm_sbox, SBOX_DCR));
	seq_printf(m, "DMA Channel Registers\n");
	seq_printf(m, "========================================"
				"=======================================\n");
	seq_printf(m, "%-10s| %-10s %-10s %-10s %-10s %-10s %-10s"
#ifdef CONFIG_MK1OM
				  " %-10s %-11s %-14s %-10s"
#endif
				"\n", "Channel", "DCAR", "DTPR", "DHPR",
					"DRAR_HI", "DRAR_LO",
#ifdef CONFIG_MK1OM
					"DSTATWB_LO", "DSTATWB_HI", "DSTAT_CHERR", "DSTAT_CHERRMSK",
#endif
					"DSTAT");
	seq_printf(m, "========================================"
				"=======================================\n");

#ifdef _MIC_SCIF_
	for (i = 0; i < MAX_NUM_DMA_CHAN; i++) {
#else
	for (i = first_dma_chan(); i <= last_dma_chan(); i++) {
#endif
		curr_chan = &dma_ctx->dma_channels[i];
		chan_num = curr_chan->ch_num;
		seq_printf(m, "%-10i| %-#10x %-#10x %-#10x %-#10x"
			" %-#10x"
#ifdef CONFIG_MK1OM
			" %-#10x %-#11x %-#10x %-#14x"
#endif
			" %-#10x\n", chan_num,
			md_mic_dma_read_mmio(dma_dev, chan_num, REG_DCAR),
			md_mic_dma_read_mmio(dma_dev, chan_num, REG_DTPR),
			md_mic_dma_read_mmio(dma_dev, chan_num, REG_DHPR),
			md_mic_dma_read_mmio(dma_dev, chan_num, REG_DRAR_HI),
			md_mic_dma_read_mmio(dma_dev, chan_num, REG_DRAR_LO),
#ifdef CONFIG_MK1OM
			md_mic_dma_read_mmio(dma_dev, chan_num, REG_DSTATWB_LO),
			md_mic_dma_read_mmio(dma_dev, chan_num, REG_DSTATWB_HI),
			md_mic_dma_read_mmio(dma_dev, chan_num, REG_DCHERR),
			md_mic_dma_read_mmio(dma_dev, chan_num, REG_DCHERRMSK),
#endif
			md_mic_dma_read_mmio(dma_dev, chan_num, REG_DSTAT));
	}

	seq_printf(m, "\nDMA Channel Descriptor Rings\n");
	seq_printf(m, "========================================"
				"=======================================\n");

	for (i = first_dma_chan(); i <= last_dma_chan(); i++) {
		curr_chan = &dma_ctx->dma_channels[i];
		chan_num = curr_chan->ch_num;
		dtpr = md_mic_dma_read_mmio(dma_dev, chan_num, REG_DTPR);
		seq_printf(m,  "Channel %i: [", chan_num);
		size = ((int) md_mic_dma_read_mmio(dma_dev, chan_num, REG_DHPR)
			- dtpr) % curr_chan->chan->num_desc_in_ring;
		/*
		 * In KNC B0, empty condition is tail = head -1
		 */
		if (mic_hw_family(dma_ctx->device_num) == FAMILY_KNC &&
			mic_hw_stepping(dma_ctx->device_num) >= KNC_B0_STEP)
			size -= 1;

		for (j = 0; j < size; j++) {
			desc = curr_chan->desc_ring[(j+dtpr) % 
				curr_chan->chan->num_desc_in_ring];	

			switch (desc.desc.nop.type){
			case NOP:
				seq_printf(m," {Type: NOP, 0x%#llx"
					" %#llx} ",  desc.qwords.qw0,
						   desc.qwords.qw1);
			case MEMCOPY:
				seq_printf(m," {Type: MEMCOPY, SAP:"
					" 0x%#llx, DAP: %#llx, length: %#llx} ",
					  (uint64_t) desc.desc.memcopy.sap,
					  (uint64_t) desc.desc.memcopy.dap,
					  (uint64_t) desc.desc.memcopy.length);
				break;
			case STATUS:
				seq_printf(m," {Type: STATUS, data:"
					" 0x%#llx, DAP: %#llx, intr: %lli} ",
					(uint64_t) desc.desc.status.data,
					(uint64_t) desc.desc.status.dap,
					(uint64_t) desc.desc.status.intr);
				break;
			case GENERAL:
				seq_printf(m," {Type: GENERAL, "
					"DAP: %#llx, dword: %#llx} ",
					(uint64_t) desc.desc.general.dap,
					(uint64_t) desc.desc.general.data);
				break;
			case KEYNONCECNT:
				seq_printf(m," {Type: KEYNONCECNT, sel: "
					"%lli, h: %lli, index: %lli, cs: %lli,"
					" value: %#llx} ",
						(uint64_t) desc.desc.keynoncecnt.sel,
						(uint64_t) desc.desc.keynoncecnt.h,
						(uint64_t) desc.desc.keynoncecnt.index,
						(uint64_t) desc.desc.keynoncecnt.cs,
						(uint64_t) desc.desc.keynoncecnt.data);
				break;
			case KEY:
				seq_printf(m," {Type: KEY, dest_ind"
					   "ex: %lli, ski: %lli, skap: %#llx ",
						(uint64_t) desc.desc.key.di,
						(uint64_t) desc.desc.key.ski,
						(uint64_t) desc.desc.key.skap);
				break;
			default:
				seq_printf(m," {Uknown Type=%lli ,"
				 "%#llx %#llx} ",(uint64_t)  desc.desc.nop.type,
						(uint64_t) desc.qwords.qw0,
						(uint64_t) desc.qwords.qw1);
			}
		}
		seq_printf(m,  "]\n");
		if (mic_hw_family(dma_ctx->device_num) == FAMILY_KNC &&
		    mic_hw_stepping(dma_ctx->device_num) >= KNC_B0_STEP &&
		    curr_chan->chan->dstat_wb_loc)
			seq_printf(m, "DSTAT_WB = 0x%x\n",
				*((uint32_t*)curr_chan->chan->dstat_wb_loc));
	}
	micpm_put_reference(mic_ctx);

	return 0;
}

static int
mic_dma_proc_reg_open(struct inode *inode, struct file *file)
{
	return single_open(file, mic_dma_proc_reg_show, PDE_DATA(inode));
}

struct file_operations micdma_ring_fops = {
	.open		= mic_dma_proc_ring_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
        .release 	= single_release,
};

struct file_operations micdma_reg_fops = {
	.open		= mic_dma_proc_reg_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
        .release 	= single_release,
};

static void
mic_dma_proc_init(struct mic_dma_ctx_t *dma_ctx)
{
	char name[64];

	snprintf(name, 63, "%s%d", proc_dma_ring, dma_ctx->device_num);
	if (!proc_create_data(name,  S_IFREG | S_IRUGO, NULL, &micdma_ring_fops, dma_ctx))
		printk("micdma: unable to register /proc/%s\n", name);

	snprintf(name, 63, "%s%d", proc_dma_reg, dma_ctx->device_num);
	if (!proc_create_data(name, S_IFREG | S_IRUGO, NULL, &micdma_reg_fops, dma_ctx))
		printk("micdma: unable to register /proc/%s\n", name);

}
#else // LINUX VERSION
static int
mic_dma_proc_read_fn(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
	struct mic_dma_ctx_t *dma_ctx = data;
	int i, len = 0;
	struct compl_buf_ring *ring;

	len += sprintf(buf + len, "Intr rings\n");
	len += sprintf(buf + len, "%-10s%-12s%-12s%-12s%-25s%-18s%-25s\n",
		       "Chan", "Head", "Tail", "Size", "Tail loc", "Actual tail", "In Use");
	for (i = first_dma_chan(); i <= last_dma_chan(); i++) {
		ring = &dma_ctx->dma_channels[i].intr_ring.ring;
		len += sprintf(buf + len, "%-#10x%-#12x%-#12x%-#12x%-#25llx%-#18x%-#18x\n",
			i, ring->head, ring->tail, ring->size,
			ring->tail_location, *(int*)ring->tail_location,
			atomic_read(&dma_ctx->dma_channels[i].flags));
	}
	len += sprintf(buf + len, "Poll rings\n");
	len += sprintf(buf + len, "%-10s%-12s%-12s%-12s%-25s%-18s\n",
		       "Chan", "Head", "Tail", "Size", "Tail loc", "Actual tail");
	for (i = first_dma_chan(); i <= last_dma_chan(); i++) {
		ring = &dma_ctx->dma_channels[i].poll_ring;
		len += sprintf(buf + len, "%-#10x%-#12x%-#12x%-#12x%-#25llx%-#18x\n",
			       i, ring->head, ring->tail, ring->size,
			       ring->tail_location, *(int*)ring->tail_location);
	}
	len += sprintf(buf + len, "Next_Write_Index\n");
	len += sprintf(buf + len, "%-10s%-12s\n", "Chan", "Next_Write_Index");
	for (i = 0; i < MAX_NUM_DMA_CHAN; i++) {
		len += sprintf(buf + len, "%-#10x%-#12llx\n",
			       i, dma_ctx->dma_channels[i].next_write_index);
	}
	return len;
}

static int
mic_dma_proc_read_registers_fn(char *buf, char **start, off_t offset, int count,
							   int *eof, void *data)
{
	int i, j, chan_num, size, dtpr, len = 0;
	struct mic_dma_ctx_t *dma_ctx = data;
	struct mic_dma_device *dma_dev = &dma_ctx->dma_dev;
	struct dma_channel *curr_chan;
	union md_mic_dma_desc desc;

	len += sprintf(buf + len, "========================================"
				"=======================================\n");
	len += sprintf(buf + len, "SBOX_DCR: %#x\n",
				mic_sbox_read_mmio(dma_dev->mm_sbox, SBOX_DCR));
	len += sprintf(buf + len, "DMA Channel Registers\n");
	len += sprintf(buf + len, "========================================"
				"=======================================\n");
	len += sprintf(buf + len, "%-10s| %-10s %-10s %-10s %-10s %-10s %-10s"
#ifdef CONFIG_MK1OM
				  " %-10s %-11s %-14s %-10s"
#endif
				"\n", "Channel", "DCAR", "DTPR", "DHPR",
					"DRAR_HI", "DRAR_LO",
#ifdef CONFIG_MK1OM
					"DSTATWB_LO", "DSTATWB_HI", "DSTAT_CHERR", "DSTAT_CHERRMSK",
#endif
					"DSTAT");
	len += sprintf(buf + len, "========================================"
				"=======================================\n");

#ifdef _MIC_SCIF_
	for (i = 0; i < MAX_NUM_DMA_CHAN; i++) {
#else
	for (i = first_dma_chan(); i <= last_dma_chan(); i++) {
#endif
		curr_chan = &dma_ctx->dma_channels[i];
		chan_num = curr_chan->ch_num;
		len += sprintf(buf + len, "%-10i| %-#10x %-#10x %-#10x %-#10x"
			" %-#10x"
#ifdef CONFIG_MK1OM
			" %-#10x %-#11x %-#10x %-#14x"
#endif
			" %-#10x\n", chan_num,
			md_mic_dma_read_mmio(dma_dev, chan_num, REG_DCAR),
			md_mic_dma_read_mmio(dma_dev, chan_num, REG_DTPR),
			md_mic_dma_read_mmio(dma_dev, chan_num, REG_DHPR),
			md_mic_dma_read_mmio(dma_dev, chan_num, REG_DRAR_HI),
			md_mic_dma_read_mmio(dma_dev, chan_num, REG_DRAR_LO),
#ifdef CONFIG_MK1OM
			md_mic_dma_read_mmio(dma_dev, chan_num, REG_DSTATWB_LO),
			md_mic_dma_read_mmio(dma_dev, chan_num, REG_DSTATWB_HI),
			md_mic_dma_read_mmio(dma_dev, chan_num, REG_DCHERR),
			md_mic_dma_read_mmio(dma_dev, chan_num, REG_DCHERRMSK),
#endif
			md_mic_dma_read_mmio(dma_dev, chan_num, REG_DSTAT));
	}

	len += sprintf(buf + len, "\nDMA Channel Descriptor Rings\n");
	len += sprintf(buf + len, "========================================"
				"=======================================\n");

	for (i = first_dma_chan(); i <= last_dma_chan(); i++) {
		curr_chan = &dma_ctx->dma_channels[i];
		chan_num = curr_chan->ch_num;
		dtpr = md_mic_dma_read_mmio(dma_dev, chan_num, REG_DTPR);
		len += sprintf(buf + len,  "Channel %i: [", chan_num);
		size = ((int) md_mic_dma_read_mmio(dma_dev, chan_num, REG_DHPR)
			- dtpr) % curr_chan->chan->num_desc_in_ring;
		/*
		 * In KNC B0, empty condition is tail = head -1
		 */
		if (mic_hw_family(dma_ctx->device_num) == FAMILY_KNC &&
			mic_hw_stepping(dma_ctx->device_num) >= KNC_B0_STEP)
			size -= 1;

		for (j = 0; j < size; j++) {
			desc = curr_chan->desc_ring[(j+dtpr) % 
				curr_chan->chan->num_desc_in_ring];	

			switch (desc.desc.nop.type){
			case NOP:
				len += sprintf(buf + len," {Type: NOP, 0x%#llx"
					" %#llx} ",  desc.qwords.qw0,
						   desc.qwords.qw1);
			case MEMCOPY:
				len += sprintf(buf + len," {Type: MEMCOPY, SAP:"
					" 0x%#llx, DAP: %#llx, length: %#llx} ",
					  (uint64_t) desc.desc.memcopy.sap,
					  (uint64_t) desc.desc.memcopy.dap,
					  (uint64_t) desc.desc.memcopy.length);
				break;
			case STATUS:
				len += sprintf(buf + len," {Type: STATUS, data:"
					" 0x%#llx, DAP: %#llx, intr: %lli} ",
					(uint64_t) desc.desc.status.data,
					(uint64_t) desc.desc.status.dap,
					(uint64_t) desc.desc.status.intr);
				break;
			case GENERAL:
				len += sprintf(buf + len," {Type: GENERAL, "
					"DAP: %#llx, dword: %#llx} ",
					(uint64_t) desc.desc.general.dap,
					(uint64_t) desc.desc.general.data);
				break;
			case KEYNONCECNT:
				len += sprintf(buf + len," {Type: KEYNONCECNT, sel: "
					"%lli, h: %lli, index: %lli, cs: %lli,"
					" value: %#llx} ",
						(uint64_t) desc.desc.keynoncecnt.sel,
						(uint64_t) desc.desc.keynoncecnt.h,
						(uint64_t) desc.desc.keynoncecnt.index,
						(uint64_t) desc.desc.keynoncecnt.cs,
						(uint64_t) desc.desc.keynoncecnt.data);
				break;
			case KEY:
				len += sprintf(buf + len," {Type: KEY, dest_ind"
					   "ex: %lli, ski: %lli, skap: %#llx ",
						(uint64_t) desc.desc.key.di,
						(uint64_t) desc.desc.key.ski,
						(uint64_t) desc.desc.key.skap);
				break;
			default:
				len += sprintf(buf + len," {Uknown Type=%lli ,"
				 "%#llx %#llx} ",(uint64_t)  desc.desc.nop.type,
						(uint64_t) desc.qwords.qw0,
						(uint64_t) desc.qwords.qw1);
			}
		}
		len += sprintf(buf + len,  "]\n");
		if (mic_hw_family(dma_ctx->device_num) == FAMILY_KNC &&
		    mic_hw_stepping(dma_ctx->device_num) >= KNC_B0_STEP &&
		    curr_chan->chan->dstat_wb_loc)
			len += sprintf(buf + len, "DSTAT_WB = 0x%x\n",
				*((uint32_t*)curr_chan->chan->dstat_wb_loc));
	}
	return len;
}

static void
mic_dma_proc_init(struct mic_dma_ctx_t *dma_ctx)
{
	struct proc_dir_entry *dma_proc;
	char name[64];

	snprintf(name, 63, "%s%d", proc_dma_ring, dma_ctx->device_num);
	if ((dma_proc = create_proc_entry(name,  S_IFREG | S_IRUGO, NULL)) != NULL) {
		dma_proc->read_proc = mic_dma_proc_read_fn;
		dma_proc->data      = dma_ctx;
	}
	snprintf(name, 63, "%s%d", proc_dma_reg, dma_ctx->device_num);
	if ((dma_proc = create_proc_entry(name, S_IFREG | S_IRUGO, NULL)) != NULL) {
		dma_proc->read_proc = mic_dma_proc_read_registers_fn;
		dma_proc->data      = dma_ctx;
	}

}
#endif // LINUX VERSION

static void
mic_dma_proc_uninit(struct mic_dma_ctx_t *dma_ctx)
{
	char name[64];

	snprintf(name, 63, "%s%d", proc_dma_reg, dma_ctx->device_num);
	remove_proc_entry(name, NULL);
	snprintf(name, 63, "%s%d", proc_dma_ring, dma_ctx->device_num);
	remove_proc_entry(name, NULL);
}
