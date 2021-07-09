/*
 * Intel MIC Platform Software Stack (MPSS)
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2016 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * BSD LICENSE
 *
 * Copyright(c) 2016 Intel Corporation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Intel MIC X200 DMA driver.
 */
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/vmalloc.h>
#include <linux/freezer.h>
#include <linux/version.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>

#include "dmaengine.h"
#include "../mic/common/mic_dev.h"

/* TBD Remove for upstream */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 13, 0))
#ifndef DMA_COMPLETE
#define DMA_COMPLETE DMA_SUCCESS
#endif
#endif

#define MIC_DMA_DESC_RX_SIZE	(4 * 1024)
#define MIC_DMA_ALIGN_SHIFT	6
#define MIC_DMA_ALIGN_BYTES	(1 << MIC_DMA_ALIGN_SHIFT)
#define MIC_DMA_POLL_TIMEOUT	500000
#define MIC_DMA_ABORT_TO_MS	3000

/* DMA Descriptor related flags */
#define MIC_DMA_DESC_VALID		(1UL << 31)
#define MIC_DMA_DESC_INTR_ENABLE	(1UL << 30)
#define MIC_DMA_DESC_SRC_LINK_ERR	(1UL << 29)
#define MIC_DMA_DESC_DST_LINK_ERR	(1UL << 28)
#define MIC_DMA_DESC_STD_DESC		(0UL << 27)
#define MIC_DMA_DESC_EXTD_DESC		(1UL << 27)
#define MIC_DMA_DESC_LOW_MASK		(0xffffffffUL)
#define MIC_DMA_DESC_SRC_LOW_SHIFT	32
#define MIC_DMA_DESC_BITS_32_TO_47	(0xffffUL << 32)
#define MIC_DMA_DESC_SIZE_MASK		((1UL << 27) - 1)

#define MIC_DMA_PCI_ID				0x000
#define MIC_DMA_SUBSYSTEM_ID			0x02C
#define MIC_DMA_GLOBAL_CTL			0x1F8
#define MIC_DMA_CHANNEL_ASSOC			0x1FC

#define MIC_DMA_BLK_SRC_LOWER			0x200
#define MIC_DMA_BLK_SRC_UPPER			0x204
#define MIC_DMA_BLK_DST_LOWER			0x208
#define MIC_DMA_BLK_DST_UPPER			0x20C
#define MIC_DMA_BLK_TRSFR_SIZE			0x210
#define MIC_DMA_DESC_RING_ADDR_LOW		0x214
#define MIC_DMA_DESC_RING_ADDR_HIGH		0x218
#define MIC_DMA_NEXT_DESC_ADDR_LOW		0x21C
#define MIC_DMA_DESC_RING_SIZE			0x220
#define MIC_DMA_LAST_DESC_ADDR_LOW		0x224
#define MIC_DMA_LAST_DESC_XFER_SIZE		0x228
#define MIC_DMA_CHAN_BANDWIDTH			0x22C
#define MIC_DMA_MAX_PREFETCH			0x234

#define MIC_DMA_CTRL_STATUS			0x238
#define MIC_DMA_CTRL_PAUSE			(1UL << 0)
#define MIC_DMA_CTRL_ABORT			(1UL << 1)
#define MIC_DMA_CTRL_WB_ENABLE			(1UL << 2)
#define MIC_DMA_CTRL_START			(1UL << 3)
#define MIC_DMA_CTRL_RING_STOP			(1UL << 4)
#define MIC_DMA_CTRL_BLOCK_MODE			(0UL << 5)
#define MIC_DMA_CTRL_ONCHIP_MODE		(1UL << 5)
#define MIC_DMA_CTRL_OFFCHIP_MODE		(2UL << 5)
#define MIC_DMA_CTRL_DESC_INVLD_STATUS		(1UL << 8)
#define MIC_DMA_CTRL_PAUSE_DONE_STATUS		(1UL << 9)
#define MIC_DMA_CTRL_ABORT_DONE_STATUS		(1UL << 10)
#define MIC_DMA_CTRL_FACTORY_TEST		(1UL << 11)
#define MIC_DMA_CTRL_IMM_PAUSE_DONE_STATUS	(1UL << 12)
#define MIC_DMA_CTRL_IN_PROGRESS		(1UL << 30)
#define MIC_DMA_CTRL_HEADER_LOG			(1UL << 31)
#define MIC_DMA_CTRL_MODE_BITS			(3UL << 5)
#define MIC_DMA_CTRL_STATUS_BITS		(0x1FUL << 8)

#define MIC_DMA_INTR_CTRL_STATUS		0x23C
#define MIC_DMA_ERROR_INTR_EN			(1UL << 0)
#define MIC_DMA_INVLD_DESC_INTR_EN		(1UL << 1)
#define MIC_DMA_ABORT_DONE_INTR_EN		(1UL << 3)
#define MIC_DMA_GRACE_PAUSE_INTR_EN		(1UL << 4)
#define MIC_DMA_IMM_PAUSE_INTR_EN		(1UL << 5)
#define MIC_DMA_IRQ_PIN_INTR_EN			(1UL << 15)
#define MIC_DMA_ERROR_INTR_STATUS		(1UL << 16)
#define MIC_DMA_INVLD_DESC_INTR_STATUS		(1UL << 17)
#define MIC_DMA_DESC_DONE_INTR_STATUS		(1UL << 18)
#define MIC_DMA_ABORT_DONE_INTR_STATUS		(1UL << 19)
#define MIC_DMA_GRACE_PAUSE_INTR_STATUS		(1UL << 20)
#define MIC_DMA_IMM_PAUSE_INTR_STATUS		(1UL << 21)
#define MIC_DMA_ALL_INTR_EN	(MIC_DMA_ERROR_INTR_EN |		\
				 MIC_DMA_INVLD_DESC_INTR_EN |		\
				 MIC_DMA_ABORT_DONE_INTR_EN |		\
				 MIC_DMA_GRACE_PAUSE_INTR_EN |		\
				 MIC_DMA_IMM_PAUSE_INTR_EN)

#define MIC_DMA_ERROR_STATUS	(MIC_DMA_ERROR_INTR_STATUS |		\
				 MIC_DMA_ABORT_DONE_INTR_STATUS |	\
				 MIC_DMA_GRACE_PAUSE_INTR_STATUS |	\
				 MIC_DMA_IMM_PAUSE_INTR_STATUS)

#define MIC_DMA_ALL_INTR_STATUS	(MIC_DMA_ERROR_INTR_STATUS |		\
				 MIC_DMA_INVLD_DESC_INTR_STATUS |	\
				 MIC_DMA_DESC_DONE_INTR_STATUS |	\
				 MIC_DMA_ABORT_DONE_INTR_STATUS |	\
				 MIC_DMA_GRACE_PAUSE_INTR_STATUS |	\
				 MIC_DMA_IMM_PAUSE_INTR_STATUS)

#define MIC_DMA_PCIE_DEVCTL2			0x90
#define MIC_DMA_PCIE_DEVCTL2_COMP_TO_MASK	0xf
#define MIC_DMA_PCIE_DEVCTL2_COMP_TO		0x9 /* 512 ms */

#define MIC_DMA_STA_RAM_THRESH			0x258
#define MIC_DMA_STA0_HDR_RAM			0x25C
#define MIC_DMA_STA0_PLD_RAM			0x260
#define MIC_DMA_STA1_HDR_RAM			0x2A4
#define MIC_DMA_STA1_PLD_RAM			0x2A8


/* HW DMA descriptor */
struct mic_dma_desc {
	u64 qw0;
	u64 qw1;
};

/*
 * mic_dma_chan - MIC X200 DMA channel specific data structures
 *
 * @ch_num: channel number
 * @last_tail: cached value of descriptor ring tail
 * @head: index of next descriptor in desc_ring
 * @chan: dma engine api channel
 * @desc_ring: dma descriptor ring
 * @desc_ring_da: DMA address of desc_ring
 * @tx_array: array of async_tx
 * @prep_lock: lock held in prep_memcpy & released in tx_submit
 * @issue_lock: lock used to synchronize writes to head
 * @notify_hw: flag used to notify HW that new descriptors are available
 * @dbg_flush: flag used for "flush" DMA's from debugfs
 * @abort_tail: descriptor ring position at which last abort occurred
 * @abort_counter: number of aborts at the last position in desc ring
 */
struct mic_dma_chan {
	int ch_num;
	u32 last_tail;
	u32 head;
	struct dma_chan chan;
	struct mic_dma_desc *desc_ring;
	dma_addr_t desc_ring_da;
	struct dma_async_tx_descriptor *tx_array;
	spinlock_t cleanup_lock;
	spinlock_t prep_lock;
	spinlock_t issue_lock;
	bool notify_hw;
	bool dbg_flush;
	u32 abort_tail;
	u32 abort_counter;
};

/*
 * mic_dma_device - Per MIC X200 DMA device driver specific data structures
 *
 * @pdev: PCIe device
 * @dma_dev: underlying dma device
 * @reg_base: virtual address of the mmio space
 * @mic_chan: Array of MIC X200 DMA channels
 * @max_xfer_size: maximum transfer size per dma descriptor
 * @dbg_dir: debugfs directory
 */
struct mic_dma_device {
	struct pci_dev *pdev;
	struct dma_device dma_dev;
	void __iomem *reg_base;
	struct mic_dma_chan mic_chan;
	size_t max_xfer_size;
	struct dentry *dbg_dir;
};

static inline struct mic_dma_device *to_mic_dma_dev(struct mic_dma_chan *ch)
{
	return
	container_of((const typeof(((struct mic_dma_device *)0)->mic_chan)*)
			 (ch - ch->ch_num), struct mic_dma_device, mic_chan);
}

static inline struct mic_dma_chan *to_mic_dma_chan(struct dma_chan *ch)
{
	return container_of(ch, struct mic_dma_chan, chan);
}

static inline struct device *mic_dma_ch_to_device(struct mic_dma_chan *ch)
{
	return to_mic_dma_dev(ch)->dma_dev.dev;
}

static inline u32 mic_dma_reg_read(struct mic_dma_device *pdma, u32 offset)
{
	return ioread32(pdma->reg_base + offset);
}

static inline void mic_dma_reg_write(struct mic_dma_device *pdma,
			   u32 offset, u32 value)
{
	iowrite32(value, pdma->reg_base + offset);
}

static inline u32 mic_dma_ch_reg_read(struct mic_dma_chan *ch, u32 offset)
{
	return mic_dma_reg_read(to_mic_dma_dev(ch), offset);
}

static inline void mic_dma_ch_reg_write(struct mic_dma_chan *ch,
				 u32 offset, u32 value)
{
	mic_dma_reg_write(to_mic_dma_dev(ch), offset, value);
}

static void mic_dma_debugfs_init(struct mic_dma_device *mic_dma_dev);

/* #define MIC_DMA_DEBUG 1 */
#ifdef MIC_DMA_DEBUG
struct mic_dma_desc_debug {
	struct mic_dma_desc desc;
	u32 head;
	u32 tail;
	u32 hw_tail;
	u32 space;
};

static struct mic_dma_desc_debug *mic_dma_desc_dbg;
static  int mic_dma_desc_dbg_count;
#define MIC_DMA_DESC_DEBUG_ENTRIES 2000

static inline void
mic_dma_desc_debug(struct mic_dma_chan *ch, struct mic_dma_desc *desc, int space)
{
	if (mic_dma_desc_dbg_count >= MIC_DMA_DESC_DEBUG_ENTRIES)
		mic_dma_desc_dbg_count = 0;
	mic_dma_desc_dbg[mic_dma_desc_dbg_count].desc.qw0 = desc->qw0;
	mic_dma_desc_dbg[mic_dma_desc_dbg_count].desc.qw1 = desc->qw1;
	mic_dma_desc_dbg[mic_dma_desc_dbg_count].head = ch->head;
	mic_dma_desc_dbg[mic_dma_desc_dbg_count].hw_tail =
			mic_dma_ch_reg_read(ch, MIC_DMA_LAST_DESC_ADDR_LOW);
	mic_dma_desc_dbg[mic_dma_desc_dbg_count].space = space;
	mic_dma_desc_dbg[mic_dma_desc_dbg_count++].tail = ch->last_tail;
}

static inline void mic_dma_debug_init(void)
{
	mic_dma_desc_dbg = vzalloc(MIC_DMA_DESC_DEBUG_ENTRIES * sizeof(*mic_dma_desc_dbg));
	mic_dma_desc_dbg_count = 0x0;
}

static inline void mic_dma_debug_destroy(void)
{
	vfree(mic_dma_desc_dbg);
	mic_dma_desc_dbg = NULL;
}
#else
static inline void
mic_dma_desc_debug(struct mic_dma_chan *ch, struct mic_dma_desc *desc, int s)
{
}

static inline void mic_dma_debug_init(void)
{
}

static inline void mic_dma_debug_destroy(void)
{
}
#endif /* MIC_DMA_DEBUG */


/* high-water mark for pushing dma descriptors */
static int mic_dma_pending_level = 16;


static inline u32 mic_dma_ring_inc(u32 val)
{
	return (val + 1) & (MIC_DMA_DESC_RX_SIZE - 1);
}

static inline u32 mic_dma_ring_dec(u32 val)
{
	return val ? val - 1 : MIC_DMA_DESC_RX_SIZE - 1;
}

static inline void mic_dma_inc_head(struct mic_dma_chan *ch)
{
	ch->head = mic_dma_ring_inc(ch->head);
}

static int mic_dma_alloc_desc_ring(struct mic_dma_chan *ch)
{
	/* Desc size must be >= depth of ring + prefetch size + 1 */
	u64 desc_ring_size = MIC_DMA_DESC_RX_SIZE * sizeof(*ch->desc_ring);
	struct device *dev = to_mic_dma_dev(ch)->dma_dev.dev;

	desc_ring_size = ALIGN(desc_ring_size, MIC_DMA_ALIGN_BYTES);

	ch->desc_ring = dma_alloc_coherent(dev, desc_ring_size,
					   &ch->desc_ring_da,
					   GFP_KERNEL|__GFP_ZERO);
	if (!ch->desc_ring)
		return -ENOMEM;

	dev_dbg(dev, "DMA desc ring VA = %p PA 0x%llx size 0x%llx\n",
		ch->desc_ring, ch->desc_ring_da, desc_ring_size);
	ch->tx_array = vzalloc(MIC_DMA_DESC_RX_SIZE * sizeof(*ch->tx_array));
	if (!ch->tx_array)
		goto tx_error;
	return 0;
tx_error:
	dma_free_coherent(dev, desc_ring_size, ch->desc_ring, ch->desc_ring_da);
	return -ENOMEM;
}

static inline void mic_dma_disable_chan(struct mic_dma_chan *ch)
{
	struct device *dev = mic_dma_ch_to_device(ch);
	u32 ctrl_reg = mic_dma_ch_reg_read(ch, MIC_DMA_CTRL_STATUS);
	int i;

	/* set abort bit and clear start bit */
	ctrl_reg |= MIC_DMA_CTRL_ABORT;
	ctrl_reg &= ~MIC_DMA_CTRL_START;
	mic_dma_ch_reg_write(ch, MIC_DMA_CTRL_STATUS, ctrl_reg);

	for (i = 0; i < MIC_DMA_ABORT_TO_MS; i++) {
		ctrl_reg = mic_dma_ch_reg_read(ch, MIC_DMA_CTRL_STATUS);

		if (ctrl_reg & MIC_DMA_CTRL_ABORT_DONE_STATUS) {
			ch->notify_hw = 0;
			return;
		}

		mdelay(1);
	}
	dev_err(dev, "%s abort timed out 0x%x head %d tail %d\n",
		__func__, ctrl_reg, ch->head, ch->last_tail);
}

static inline void mic_dma_enable_chan(struct mic_dma_chan *ch)
{
	u32 ctrl_reg = mic_dma_ch_reg_read(ch, MIC_DMA_CTRL_STATUS);

	ctrl_reg |= MIC_DMA_CTRL_WB_ENABLE;
	/* Clear any active status bits ([31,12:8]) */
	ctrl_reg |= MIC_DMA_CTRL_HEADER_LOG | MIC_DMA_CTRL_STATUS_BITS;
	ctrl_reg &= ~MIC_DMA_CTRL_MODE_BITS;
	/* Enable SGL off-chip mode */
	ctrl_reg |= MIC_DMA_CTRL_OFFCHIP_MODE;
	/* Set start and clear abort and abort done bits */
	ctrl_reg |= MIC_DMA_CTRL_START;
	ctrl_reg &= ~MIC_DMA_CTRL_ABORT;
	mic_dma_ch_reg_write(ch, MIC_DMA_CTRL_STATUS, ctrl_reg);
	ch->notify_hw = 1;
}

static inline void mic_dma_chan_mask_intr(struct mic_dma_chan *ch)
{
	u32 intr_reg = mic_dma_ch_reg_read(ch, MIC_DMA_INTR_CTRL_STATUS);

	intr_reg &= ~(MIC_DMA_ALL_INTR_EN);
	mic_dma_ch_reg_write(ch, MIC_DMA_INTR_CTRL_STATUS, intr_reg);
}

static inline void mic_dma_chan_unmask_intr(struct mic_dma_chan *ch)
{
	u32 intr_reg = mic_dma_ch_reg_read(ch, MIC_DMA_INTR_CTRL_STATUS);

	intr_reg |= (MIC_DMA_ALL_INTR_EN);
	mic_dma_ch_reg_write(ch, MIC_DMA_INTR_CTRL_STATUS, intr_reg);
}

static void mic_dma_chan_set_desc_ring(struct mic_dma_chan *ch)
{
	/* Set up the descriptor ring base address and size */
	mic_dma_ch_reg_write(ch, MIC_DMA_DESC_RING_ADDR_LOW,
			     ch->desc_ring_da & 0xffffffff);
	mic_dma_ch_reg_write(ch, MIC_DMA_DESC_RING_ADDR_HIGH,
			     (ch->desc_ring_da >> 32) & 0xffffffff);
	mic_dma_ch_reg_write(ch, MIC_DMA_DESC_RING_SIZE, MIC_DMA_DESC_RX_SIZE);
	/* Set up the next descriptor to the base of the descriptor ring */
	mic_dma_ch_reg_write(ch, MIC_DMA_NEXT_DESC_ADDR_LOW,
			     ch->desc_ring_da & 0xffffffff);
}

static void mic_dma_cleanup(struct mic_dma_chan *ch)
{
	struct dma_async_tx_descriptor *tx;
	u32 last_tail;
	u32 cached_head;

	spin_lock(&ch->cleanup_lock);
	/*
	 * Use a cached head rather than using ch->head directly, since a
	 * different thread can be updating ch->head potentially leading to an
	 * infinite loop below.
	 */
	cached_head = ACCESS_ONCE(ch->head);
	/*
	 * This is the barrier pair for smp_wmb() in fn.
	 * mic_dma_tx_submit_unlock. It's required so that we read the
	 * updated cookie value from tx->cookie.
	 */
	smp_rmb();

	for (last_tail = ch->last_tail; last_tail != cached_head;) {
		/* Break out of the loop if this desc is not yet complete */
		if (ch->desc_ring[last_tail].qw0 & MIC_DMA_DESC_VALID)
			break;

		tx = &ch->tx_array[last_tail];
		if (tx->cookie) {
			dma_cookie_complete(tx);
			if (tx->callback) {
				tx->callback(tx->callback_param);
				tx->callback = NULL;
			}
		}
		last_tail = mic_dma_ring_inc(last_tail);
	}
	/* finish all completion callbacks before incrementing tail */
	smp_mb();
	ch->last_tail = last_tail;
	spin_unlock(&ch->cleanup_lock);
}

static int mic_dma_chan_setup(struct mic_dma_chan *ch)
{
	mic_dma_chan_set_desc_ring(ch);
	ch->last_tail = 0;
	ch->head = 0;
	mic_dma_chan_unmask_intr(ch);
	mic_dma_enable_chan(ch);
	return 0;
}

static void mic_dma_free_desc_ring(struct mic_dma_chan *ch)
{
	u64 desc_ring_size = MIC_DMA_DESC_RX_SIZE * sizeof(*ch->desc_ring);
	struct device *dev = to_mic_dma_dev(ch)->dma_dev.dev;

	vfree(ch->tx_array);
	desc_ring_size = ALIGN(desc_ring_size, MIC_DMA_ALIGN_BYTES);
	dma_free_coherent(dev, desc_ring_size, ch->desc_ring, ch->desc_ring_da);
	ch->desc_ring = NULL;
	ch->desc_ring_da = 0;

	/* Reset description ring registers */
	mic_dma_chan_set_desc_ring(ch);
}

static int mic_dma_chan_init(struct mic_dma_chan *ch)
{
	struct device *dev = mic_dma_ch_to_device(ch);
	int rc;
#ifdef MIC_DMA_FIX
	struct mic_dma_device *mic_dma_dev = to_mic_dma_dev(ch);

	/*
	 * MIC_DMA_FIX: This reset helps to clear out some state in the
	 * PEX DMA device which results in dmatest to run without issues.
	 * Remove this reset for now since it nukes the EEPROM
	 * performance settings. Investigate the following options:
	 * a) Can the reset can be removed completely?
	 * b) Can the reset added back and performance settings be
	 *    restored by the driver.
	 */
	pci_reset_function(mic_dma_dev->pdev);
#endif
	rc = mic_dma_alloc_desc_ring(ch);
	if (rc)
		goto ring_error;
	rc = mic_dma_chan_setup(ch);
	if (rc)
		goto chan_error;
	mic_dma_debug_init();
	return rc;
chan_error:
	mic_dma_free_desc_ring(ch);
ring_error:
	dev_err(dev, "%s ret %d\n", __func__, rc);
	return rc;
}

static int mic_dma_alloc_chan_resources(struct dma_chan *ch)
{
	struct mic_dma_chan *mic_ch = to_mic_dma_chan(ch);
	struct device *dev = mic_dma_ch_to_device(mic_ch);
	int rc = mic_dma_chan_init(mic_ch);

	if (rc) {
		dev_err(dev, "%s ret %d\n", __func__, rc);
		return rc;
	}
	return MIC_DMA_DESC_RX_SIZE;
}

static inline u32 desc_ring_da_to_index(struct mic_dma_chan *ch, u32 descr)
{
	dma_addr_t last_dma_off = descr - (ch->desc_ring_da & 0xffffffffUL);

	return (last_dma_off / sizeof(struct mic_dma_desc)) &
		(MIC_DMA_DESC_RX_SIZE - 1);
}

static inline u32 desc_ring_index_to_da(struct mic_dma_chan *ch, u32 index)
{
	dma_addr_t addr = ch->desc_ring_da +
			sizeof(struct mic_dma_desc) * index;

	return (addr & 0xffffffffUL);
}

static u32 mic_dma_get_ring_hw_last(struct mic_dma_chan *ch)
{
	u32 last_desc = mic_dma_ch_reg_read(ch, MIC_DMA_LAST_DESC_ADDR_LOW);

	return desc_ring_da_to_index(ch, last_desc);
}

static u32 mic_dma_get_ring_hw_next(struct mic_dma_chan *ch)
{
	u32 last_desc = mic_dma_ch_reg_read(ch, MIC_DMA_NEXT_DESC_ADDR_LOW);

	return desc_ring_da_to_index(ch, last_desc);
}

static void mic_dma_free_chan_resources(struct dma_chan *ch)
{
	struct mic_dma_chan *mic_ch = to_mic_dma_chan(ch);

	mic_dma_chan_mask_intr(mic_ch);
	mic_dma_disable_chan(mic_ch);
	mic_dma_cleanup(mic_ch);
	mic_dma_free_desc_ring(mic_ch);
	mic_dma_debug_destroy();
}

static enum dma_status
mic_dma_tx_status(struct dma_chan *ch, dma_cookie_t cookie,
		  struct dma_tx_state *txstate)
{
	struct mic_dma_chan *mic_ch = to_mic_dma_chan(ch);

	if (DMA_COMPLETE != dma_cookie_status(ch, cookie, txstate))
		mic_dma_cleanup(mic_ch);

	return dma_cookie_status(ch, cookie, txstate);
}

static int mic_dma_ring_count(u32 head, u32 tail)
{
	int count;

	if (head >= tail)
		count = (tail - 0) + (MIC_DMA_DESC_RX_SIZE - head);
	else
		count = tail - head;
	return count - 1;
}

static void
mic_dma_memcpy_desc(struct mic_dma_desc *desc, dma_addr_t src_phys,
		    dma_addr_t dst_phys, u64 size, int flags)
{
	u64 qw0, qw1;

	qw0 = (src_phys & MIC_DMA_DESC_BITS_32_TO_47) << 16;
	qw0 |= (dst_phys & MIC_DMA_DESC_BITS_32_TO_47);
	qw0 |= size & MIC_DMA_DESC_SIZE_MASK;
	qw0 |=  MIC_DMA_DESC_VALID;
	if (flags & DMA_PREP_INTERRUPT)
		qw0 |=  MIC_DMA_DESC_INTR_ENABLE;

	qw1 = (src_phys & MIC_DMA_DESC_LOW_MASK) << MIC_DMA_DESC_SRC_LOW_SHIFT;
	qw1 |= dst_phys & MIC_DMA_DESC_LOW_MASK;

	desc->qw1 = qw1;
	/*
	 * Update QW1 before QW0 since QW0 has the valid bit and the
	 * descriptor might be picked up by the hardware DMA engine
	 * immediately if it finds a valid descriptor.
	 */
	wmb();
	desc->qw0 = qw0;
	/*
	 * This is not smp_wmb() on purpose since we are also publishing the
	 * descriptor updates to a dma device
	 */
	wmb();
}

static void mic_dma_fence_intr_desc(struct mic_dma_chan *ch, int flags)
{
	/*
	* Zero-length DMA memcpy needs valid source and destination addresses
	*/
	mic_dma_memcpy_desc(&ch->desc_ring[ch->head],
		ch->desc_ring_da, ch->desc_ring_da, 0, flags);
	mic_dma_inc_head(ch);
}

static int mic_dma_prog_memcpy_desc(struct mic_dma_chan *ch, dma_addr_t src,
				    dma_addr_t dst, size_t len, int flags)
{
	struct device *dev = mic_dma_ch_to_device(ch);
	size_t current_transfer_len;
	size_t max_xfer_size = to_mic_dma_dev(ch)->max_xfer_size;
	int num_desc = len / max_xfer_size;
	int ret;

	if (len % max_xfer_size)
		num_desc++;
	/*
	 * A single additional descriptor is programmed for fence/interrupt
	 * during submit(..)
	 */
	if (flags)
		num_desc++;

	if (!num_desc) {
		dev_err(dev, "%s nothing to do: rc %d\n", __func__, -EINVAL);
		return -EINVAL;
	}

	if (mic_dma_ring_count(ch->head, ch->last_tail) < num_desc)
		return -EBUSY;

	while (len > 0) {
		current_transfer_len = min(len, max_xfer_size);
		/*
		 * Set flags to 0 here, we don't want memcpy descriptors to
		 * generate interrupts, a separate descriptor will be programmed
		 * for fence/interrupt during submit, after a callback is
		 * guaranteed to have been assigned
		 */
		mic_dma_memcpy_desc(&ch->desc_ring[ch->head],
				    src, dst, current_transfer_len, 0);
		mic_dma_desc_debug(ch, &ch->desc_ring[ch->head], ret);
		mic_dma_inc_head(ch);
		len -= current_transfer_len;
		dst = dst + current_transfer_len;
		src = src + current_transfer_len;
	}
	return 0;
}

static int mic_dma_do_dma(struct mic_dma_chan *ch, int flags, dma_addr_t src,
			  dma_addr_t dst, size_t len)
{
	return mic_dma_prog_memcpy_desc(ch, src, dst, len, flags);
}

static inline void mic_dma_issue_pending(struct dma_chan *ch)
{
	struct mic_dma_chan *mic_ch = to_mic_dma_chan(ch);
	unsigned long flags;
	u32 ctrl_reg;

	spin_lock_irqsave(&mic_ch->issue_lock, flags);
	/*
	 * The notify_hw is introduced because occasionally we were seeing an
	 * issue where the HW tail seemed stuck, i.e. the DMA engine would not
	 * pick up new descriptors available in the ring. We found that
	 * serializing operations between SW and HW made this issue
	 * disappear. We believe this issue arises because of a race between SW
	 * and HW. SW adds new descriptors to the ring and clears
	 * DESC_INVLD_STATUS to ask HW to pick them up. However HW has
	 * prefetched descriptors and writes DESC_INVLD_STATUS to let SW know it
	 * has fetched an invalid descriptor. It appears that in this way HW can
	 * overwrite the clearing of DESC_INVLD_STATUS by SW (though we never
	 * saw DESC_INVLD_STATUS set to 1 when the DMA engine had stopped) and
	 * therefore HW never picks up these descriptors.
	 *
	 * Therefore it is necessary for SW to clear DESC_INVLD_STATUS only
	 * after HW has set it, and the notify_hw flag accomplishes this
	 * between issue_pending(..) and the invld_desc interrupt handler.
	 */
	if (mic_ch->notify_hw) {
		ctrl_reg = mic_dma_ch_reg_read(mic_ch, MIC_DMA_CTRL_STATUS);
		if (ctrl_reg & MIC_DMA_CTRL_DESC_INVLD_STATUS) {
			mic_ch->notify_hw = 0;
			/* FIXME: Setting START is only required for Simics */
			ctrl_reg |= MIC_DMA_CTRL_START;
			/* Clear DESC_INVLD_STATUS flag */
			mic_dma_ch_reg_write(mic_ch, MIC_DMA_CTRL_STATUS,
					     ctrl_reg);
		} else {
			dev_err(mic_dma_ch_to_device(mic_ch),
				"%s: DESC_INVLD_STATUS not set\n", __func__);
		}
	}
	spin_unlock_irqrestore(&mic_ch->issue_lock, flags);
}

static inline void mic_dma_update_pending(struct mic_dma_chan *ch)
{
	if (mic_dma_ring_count(ch->head, ch->last_tail)
		> mic_dma_pending_level)
		mic_dma_issue_pending(&ch->chan);
}

static dma_cookie_t mic_dma_tx_submit_unlock(struct dma_async_tx_descriptor *tx)
{
	struct mic_dma_chan *mic_ch = to_mic_dma_chan(tx->chan);
	dma_cookie_t cookie;

	dma_cookie_assign(tx);
	cookie = tx->cookie;
	/*
	 * We need an smp write barrier here because another CPU might see
	 * an update to submitted even before we assigned a cookie to this tx.
	 */
	smp_wmb();

	/*
	 * The final fence/interrupt desc is now programmed in submit after the
	 * cookie has been assigned
	 */
	if (tx->flags) {
		mic_dma_fence_intr_desc(mic_ch, tx->flags);
		tx->flags = 0;
	}

	spin_unlock(&mic_ch->prep_lock);
	mic_dma_update_pending(mic_ch);
	return cookie;
}

static inline struct dma_async_tx_descriptor *
allocate_tx(struct mic_dma_chan *ch, unsigned long flags)
{
	/* index of the final desc which is or will be programmed */
	u32 idx = flags ? ch->head : mic_dma_ring_dec(ch->head);
	struct dma_async_tx_descriptor *tx = &ch->tx_array[idx];

	dma_async_tx_descriptor_init(tx, &ch->chan);
	tx->tx_submit = mic_dma_tx_submit_unlock;
	tx->flags = flags;
	return tx;
}

static u32 mic_dma_ring_timeout_ms = 30000;

static struct dma_async_tx_descriptor *
mic_dma_prep_memcpy_lock(struct dma_chan *ch, dma_addr_t dma_dest,
			 dma_addr_t dma_src, size_t len, unsigned long flags)
{
	struct mic_dma_chan *mic_ch = to_mic_dma_chan(ch);
	struct device *dev = mic_dma_ch_to_device(mic_ch);
	int result;
	unsigned long timeout = jiffies +
				msecs_to_jiffies(mic_dma_ring_timeout_ms);

retry:
	spin_lock(&mic_ch->prep_lock);
	result = mic_dma_do_dma(mic_ch, flags, dma_src, dma_dest, len);
	if (result == -EBUSY) {
		mic_dma_cleanup(mic_ch);
		result = mic_dma_do_dma(mic_ch, flags, dma_src, dma_dest, len);
	}
	if ((result == -EBUSY) && time_before(jiffies, timeout)) {
		spin_unlock(&mic_ch->prep_lock);
		usleep_range(1000, 2000);
		mic_dma_cleanup(mic_ch);
		goto retry;
	}
	if (result >= 0)
		return allocate_tx(mic_ch, flags);
	dev_err(dev, "Error enqueueing dma, error=%d\n", result);
	spin_unlock(&mic_ch->prep_lock);
	return NULL;
}

static struct dma_async_tx_descriptor *
mic_dma_prep_interrupt_lock(struct dma_chan *ch, unsigned long flags)
{
	return mic_dma_prep_memcpy_lock(ch, 0, 0, 0, flags);
}

static irqreturn_t mic_dma_thread_fn(int irq, void *data)
{
	struct mic_dma_device *mic_dma_dev = ((struct mic_dma_device *)data);

	mic_dma_cleanup(&mic_dma_dev->mic_chan);
	return IRQ_HANDLED;
}

static void mic_dma_invld_desc_interrupt(struct mic_dma_chan *ch)
{
	u32 ctrl_reg;

	spin_lock(&ch->issue_lock);
	if (!ch->notify_hw) {
		ctrl_reg = mic_dma_ch_reg_read(ch, MIC_DMA_CTRL_STATUS);
		if (ctrl_reg & MIC_DMA_CTRL_DESC_INVLD_STATUS) {
			/*
			 * Check the descriptor ring for valid descriptors. If
			 * valid descriptors are available clear the
			 * DESC_INVLD_STATUS bit to signal to the hardware to
			 * pick them up. If no valid descriptors are available,
			 * when issue_pending is called, it will clear the
			 * INVLD_STATUS bit to signal to the hardware to pick up
			 * the posted descriptors.
			 */
			if (ch->desc_ring[mic_dma_ring_dec(ch->head)].qw0 &
				MIC_DMA_DESC_VALID) {
				/* FIXME: Setting START is only required for Simics */
				ctrl_reg |= MIC_DMA_CTRL_START;
				mic_dma_ch_reg_write(ch, MIC_DMA_CTRL_STATUS,
						     ctrl_reg);
			} else {
				ch->notify_hw = 1;
			}
		} else {
			dev_err(mic_dma_ch_to_device(ch),
				"%s: DESC_INVLD_STATUS not set\n", __func__);
		}
	}
	spin_unlock(&ch->issue_lock);
}

static void mic_dma_abort_interrupt(struct mic_dma_chan *ch, u32 intr_reg)
{
	u32 last_reg;
	u32 last_idx;
	u32 new_reg;
	u32 new_idx;

	spin_lock(&ch->issue_lock);

	last_reg = mic_dma_ch_reg_read(ch, MIC_DMA_LAST_DESC_ADDR_LOW);
	last_idx = desc_ring_da_to_index(ch, last_reg);
	new_reg = desc_ring_index_to_da(ch, last_idx);

	/* Check if last descriptor is inside the ring */
	if (last_reg != new_reg) {
		dev_dbg(mic_dma_ch_to_device(ch),
				 "DMA engine outside of ring, 0x%08X != 0x%08X, index = %d, head %d, tail %d\n",
				 last_reg, new_reg, last_idx,
				 ch->head, ch->last_tail);
	}

	/* Make sure next reg points to the first valid entry or head */
	new_idx = ch->last_tail;
	while (new_idx != ch->head) {
		if (ch->desc_ring[new_idx].qw0 & MIC_DMA_DESC_VALID)
			break;
		new_idx = mic_dma_ring_inc(new_idx);
	}
	new_reg = desc_ring_index_to_da(ch, new_idx);
	mic_dma_ch_reg_write(ch, MIC_DMA_NEXT_DESC_ADDR_LOW, new_reg);

	/* Check if we are not stuck at same descriptor */
	if (ch->abort_tail != last_idx) {
		ch->abort_tail = last_idx;
		ch->abort_counter = 0;
	} else {
		ch->abort_counter++;
	}

	if (ch->abort_counter < 5) {
		u32 reg = mic_dma_ch_reg_read(ch, MIC_DMA_CTRL_STATUS);

		if (!(reg & MIC_DMA_CTRL_ABORT))
			mic_dma_enable_chan(ch);
		else
			ch->abort_counter = 0;
	} else {
		/* We are stuck at same descriptor, give up */
		dev_err(mic_dma_ch_to_device(ch),
				"Abort at %d, retry %d, give up.\n",
				ch->abort_tail, ch->abort_counter);
	}

	dev_warn(mic_dma_ch_to_device(ch),
		"Abort at %d (0x%08X) (cnt = %d), new %d (0x%08X), head %d, tail %d, AER UE = 0x%08X\n",
			 last_idx, last_reg, ch->abort_counter,
			 new_idx, new_reg,
			 ch->head, ch->last_tail,
			 mic_dma_ch_reg_read(ch, 0xFB8));

	spin_unlock(&ch->issue_lock);
}

static u32 mic_dma_ack_interrupt(struct mic_dma_chan *ch)
{
	u32 intr_reg = mic_dma_ch_reg_read(ch, MIC_DMA_INTR_CTRL_STATUS);

	mic_dma_ch_reg_write(ch, MIC_DMA_INTR_CTRL_STATUS, intr_reg);
	return intr_reg;
}

static irqreturn_t mic_dma_intr_handler(int irq, void *data)
{
	struct mic_dma_device *mic_dma_dev = ((struct mic_dma_device *)data);
	bool wake_thread = false, error = false;
	struct mic_dma_chan *ch = &mic_dma_dev->mic_chan;
	struct device *dev = mic_dma_ch_to_device(ch);
	u32 reg;

	reg = mic_dma_ack_interrupt(&mic_dma_dev->mic_chan);
	wake_thread = !!(reg & MIC_DMA_DESC_DONE_INTR_STATUS);
	error = !!(reg & MIC_DMA_ERROR_STATUS);
	if (error)
		dev_info(dev, "%s error status 0x%x\n", __func__, reg);

	if (ch->dbg_flush) {
		dev_info(dev, "%s %d: flush interrupt!\n", __func__, __LINE__);
		ch->dbg_flush = false;
	}

	if (reg & MIC_DMA_INVLD_DESC_INTR_STATUS)
		mic_dma_invld_desc_interrupt(&mic_dma_dev->mic_chan);

	if (reg & MIC_DMA_ABORT_DONE_INTR_STATUS)
		mic_dma_abort_interrupt(&mic_dma_dev->mic_chan, reg);

	if (wake_thread)
		return IRQ_WAKE_THREAD;

	if (error || reg & MIC_DMA_INVLD_DESC_INTR_STATUS ||
		reg & MIC_DMA_ABORT_DONE_INTR_STATUS)
		return IRQ_HANDLED;

	return IRQ_NONE;
}

static int mic_dma_setup_irq(struct mic_dma_device *mic_dma_dev)
{
	struct pci_dev *pdev = mic_dma_dev->pdev;
	struct device *dev = &pdev->dev;
	int rc = pci_enable_msi(pdev);

	if (rc)
		pci_intx(pdev, 1);

	return devm_request_threaded_irq(dev, pdev->irq, mic_dma_intr_handler,
					 mic_dma_thread_fn, IRQF_SHARED,
					 "mic-x200-dma-intr", mic_dma_dev);
}

static inline void mic_dma_free_irq(struct mic_dma_device *mic_dma_dev)
{
	struct pci_dev *pdev = mic_dma_dev->pdev;
	struct device *dev = &pdev->dev;

	devm_free_irq(dev, pdev->irq, mic_dma_dev);
	if (pci_dev_msi_enabled(pdev))
		pci_disable_msi(pdev);
}

static void mic_dma_hw_init(struct mic_dma_device *mic_dma_dev)
{
	struct mic_dma_chan *ch = &mic_dma_dev->mic_chan;
	u32 reg;

	dev_dbg(mic_dma_dev->dma_dev.dev, "Ctrl reg 0x%0x, Intr reg 0x%0x\n",
		mic_dma_reg_read(mic_dma_dev, MIC_DMA_CTRL_STATUS),
		mic_dma_reg_read(mic_dma_dev, MIC_DMA_INTR_CTRL_STATUS));

	reg = mic_dma_ch_reg_read(ch, MIC_DMA_INTR_CTRL_STATUS);
	if (reg & MIC_DMA_ALL_INTR_EN) {
		/*
		 * Interrupts are enabled - that means card has been reset
		 * without resetting DMA HW.
		 * Disable interrupts and issue abort to stop DMA HW.
		 */
		reg &= ~(MIC_DMA_ALL_INTR_EN);
		mic_dma_ch_reg_write(ch, MIC_DMA_INTR_CTRL_STATUS, reg);
		mic_dma_disable_chan(ch);
	}

	/*
	 * Configure hw for best DMA speed. For explanation of the values
	 * used please refer to mic x200 dma hardware specification.
	 */
	mic_dma_reg_write(mic_dma_dev, MIC_DMA_CHAN_BANDWIDTH, 0x02000000);
	mic_dma_reg_write(mic_dma_dev, MIC_DMA_STA_RAM_THRESH, 0x003d002e);
	mic_dma_reg_write(mic_dma_dev, MIC_DMA_STA0_HDR_RAM, 0x009800a0);
	mic_dma_reg_write(mic_dma_dev, MIC_DMA_STA0_PLD_RAM, 0x00f80100);
	mic_dma_reg_write(mic_dma_dev, MIC_DMA_STA1_HDR_RAM, 0x009800a0);
	mic_dma_reg_write(mic_dma_dev, MIC_DMA_STA1_PLD_RAM, 0x00f80100);

	mic_dma_reg_write(mic_dma_dev, MIC_DMA_CTRL_STATUS, 0x03C30000);

	reg = mic_dma_ch_reg_read(ch, MIC_DMA_PCIE_DEVCTL2);
	reg &= ~MIC_DMA_PCIE_DEVCTL2_COMP_TO_MASK;
	reg |= MIC_DMA_PCIE_DEVCTL2_COMP_TO;
	mic_dma_reg_write(mic_dma_dev, MIC_DMA_PCIE_DEVCTL2, reg);
}

static int mic_dma_init(struct mic_dma_device *mic_dma_dev)
{
	struct mic_dma_chan *ch;
	int rc = 0;

	mic_dma_hw_init(mic_dma_dev);

	rc = mic_dma_setup_irq(mic_dma_dev);
	if (rc) {
		dev_err(mic_dma_dev->dma_dev.dev,
			"%s %d func error line %d\n", __func__, __LINE__, rc);
		goto error;
	}
	ch = &mic_dma_dev->mic_chan;
	spin_lock_init(&ch->cleanup_lock);
	spin_lock_init(&ch->prep_lock);
	spin_lock_init(&ch->issue_lock);


error:
	return rc;
}

static void mic_dma_uninit(struct mic_dma_device *mic_dma_dev)
{
	mic_dma_free_irq(mic_dma_dev);
}

static int mic_register_dma_device(struct mic_dma_device *mic_dma_dev)
{
	struct dma_device *dma_dev = &mic_dma_dev->dma_dev;
	struct mic_dma_chan *ch;

	dma_cap_zero(dma_dev->cap_mask);

	/* MIC DMA FIX: Remove private flag from caps if running dmatest */
	dma_cap_set(DMA_PRIVATE, dma_dev->cap_mask);
	dma_cap_set(DMA_MEMCPY, dma_dev->cap_mask);

	dma_dev->device_alloc_chan_resources = mic_dma_alloc_chan_resources;
	dma_dev->device_free_chan_resources = mic_dma_free_chan_resources;
	dma_dev->device_tx_status = mic_dma_tx_status;
	dma_dev->device_prep_dma_memcpy = mic_dma_prep_memcpy_lock;
	dma_dev->device_prep_dma_interrupt = mic_dma_prep_interrupt_lock;
	dma_dev->device_issue_pending = mic_dma_issue_pending;
	INIT_LIST_HEAD(&dma_dev->channels);

	/* Associate dma_dev with dma_chan */
	ch = &mic_dma_dev->mic_chan;
	ch->chan.device = dma_dev;
	dma_cookie_init(&ch->chan);
	list_add_tail(&ch->chan.device_node, &dma_dev->channels);
	return dma_async_device_register(dma_dev);
}

static int mic_dma_probe(struct mic_dma_device *mic_dma_dev)
{
	struct dma_device *dma_dev;
	int rc = -EINVAL;

	dma_dev = &mic_dma_dev->dma_dev;
	dma_dev->dev = &mic_dma_dev->pdev->dev;

	/* MIC X200 has one DMA channel per function */
	dma_dev->chancnt = 1;

	mic_dma_dev->max_xfer_size = MIC_DMA_DESC_SIZE_MASK;

	rc = mic_dma_init(mic_dma_dev);
	if (rc) {
		dev_err(&mic_dma_dev->pdev->dev,
			"%s %d rc %d\n", __func__, __LINE__, rc);
		goto init_error;
	}

	rc = mic_register_dma_device(mic_dma_dev);
	if (rc) {
		dev_err(&mic_dma_dev->pdev->dev,
			"%s %d rc %d\n", __func__, __LINE__, rc);
		goto reg_error;
	}
	mic_dma_debugfs_init(mic_dma_dev);
	return rc;
reg_error:
	mic_dma_uninit(mic_dma_dev);
init_error:
	return rc;
}

static void mic_dma_unregister_dma_device(struct mic_dma_device *mic_dma_dev)
{
	dma_async_device_unregister(&mic_dma_dev->dma_dev);
}

static void mic_dma_remove(struct mic_dma_device *mic_dma_dev)
{
	mic_dma_unregister_dma_device(mic_dma_dev);
	mic_dma_uninit(mic_dma_dev);
}

static void mic_dma_dump_ring_pos(struct seq_file *s, struct mic_dma_chan *ch)
{
	int ring_count = mic_dma_ring_count(ch->head, ch->last_tail) + 1;

	mic_dma_cleanup(ch);
	seq_printf(s, "Head %d tail %d hw %d hw next %d space %d todo %d\n",
			ch->head, ch->last_tail,
			mic_dma_get_ring_hw_last(ch),
			mic_dma_get_ring_hw_next(ch),
			ring_count,
			(MIC_DMA_DESC_RX_SIZE - ring_count));
}

static int mic_dma_reg_seq_show(struct seq_file *s, void *pos)
{
	struct mic_dma_device *mic_dma_dev = s->private;
	struct mic_dma_chan *ch = &mic_dma_dev->mic_chan;
	u32 reg;

	mic_dma_dump_ring_pos(s, ch);

#define _DUMP_REG(regAddr)	{\
	reg = mic_dma_reg_read(mic_dma_dev, regAddr);\
	seq_printf(s, #regAddr "(0x%x) \t0x%-10X\n", regAddr, reg); }

#define _DUMP_REG_BITS(bit) {\
	seq_printf(s, "\t" #bit "(1<<%d) \t%d\n", \
		ffs(bit) - 1, !!(bit & reg)); }

	_DUMP_REG(MIC_DMA_INTR_CTRL_STATUS);
	_DUMP_REG_BITS(MIC_DMA_ERROR_INTR_EN);
	_DUMP_REG_BITS(MIC_DMA_INVLD_DESC_INTR_EN);
	_DUMP_REG_BITS(MIC_DMA_ABORT_DONE_INTR_EN);
	_DUMP_REG_BITS(MIC_DMA_GRACE_PAUSE_INTR_EN);
	_DUMP_REG_BITS(MIC_DMA_IMM_PAUSE_INTR_EN);
	_DUMP_REG_BITS(MIC_DMA_IRQ_PIN_INTR_EN);
	_DUMP_REG_BITS(MIC_DMA_ERROR_INTR_STATUS);
	_DUMP_REG_BITS(MIC_DMA_INVLD_DESC_INTR_STATUS);
	_DUMP_REG_BITS(MIC_DMA_DESC_DONE_INTR_STATUS);
	_DUMP_REG_BITS(MIC_DMA_ABORT_DONE_INTR_STATUS);
	_DUMP_REG_BITS(MIC_DMA_GRACE_PAUSE_INTR_STATUS);
	_DUMP_REG_BITS(MIC_DMA_IMM_PAUSE_INTR_STATUS);

	_DUMP_REG(MIC_DMA_CTRL_STATUS);
	_DUMP_REG_BITS(MIC_DMA_CTRL_PAUSE);
	_DUMP_REG_BITS(MIC_DMA_CTRL_ABORT);
	_DUMP_REG_BITS(MIC_DMA_CTRL_WB_ENABLE);
	_DUMP_REG_BITS(MIC_DMA_CTRL_START);

	_DUMP_REG_BITS(MIC_DMA_CTRL_RING_STOP);
	_DUMP_REG_BITS(MIC_DMA_CTRL_ONCHIP_MODE);
	_DUMP_REG_BITS(MIC_DMA_CTRL_OFFCHIP_MODE);

	_DUMP_REG_BITS(MIC_DMA_CTRL_DESC_INVLD_STATUS);
	_DUMP_REG_BITS(MIC_DMA_CTRL_PAUSE_DONE_STATUS);
	_DUMP_REG_BITS(MIC_DMA_CTRL_ABORT_DONE_STATUS);
	_DUMP_REG_BITS(MIC_DMA_CTRL_IN_PROGRESS);

	seq_puts(s, "Ring status:\n");
	_DUMP_REG(MIC_DMA_BLK_SRC_LOWER);
	_DUMP_REG(MIC_DMA_BLK_SRC_UPPER);
	_DUMP_REG(MIC_DMA_BLK_DST_LOWER);
	_DUMP_REG(MIC_DMA_BLK_DST_UPPER);
	_DUMP_REG(MIC_DMA_BLK_TRSFR_SIZE);
	_DUMP_REG(MIC_DMA_NEXT_DESC_ADDR_LOW);
	_DUMP_REG(MIC_DMA_LAST_DESC_ADDR_LOW);
	_DUMP_REG(MIC_DMA_LAST_DESC_XFER_SIZE);

	seq_puts(s, "Ring config:\n");
	_DUMP_REG(MIC_DMA_DESC_RING_ADDR_LOW);
	_DUMP_REG(MIC_DMA_DESC_RING_ADDR_HIGH);
	_DUMP_REG(MIC_DMA_DESC_RING_SIZE);

	seq_puts(s, "HW info/settings:\n");
	_DUMP_REG(MIC_DMA_PCI_ID);
	_DUMP_REG(MIC_DMA_SUBSYSTEM_ID);
	_DUMP_REG(MIC_DMA_GLOBAL_CTL);
	_DUMP_REG(MIC_DMA_CHANNEL_ASSOC);
	_DUMP_REG(MIC_DMA_CHAN_BANDWIDTH);
	_DUMP_REG(MIC_DMA_MAX_PREFETCH);
	_DUMP_REG(MIC_DMA_STA_RAM_THRESH);
	_DUMP_REG(MIC_DMA_STA0_HDR_RAM);
	_DUMP_REG(MIC_DMA_STA0_PLD_RAM);
	_DUMP_REG(MIC_DMA_STA1_HDR_RAM);
	_DUMP_REG(MIC_DMA_STA1_PLD_RAM);

	return 0;
}

static int mic_dma_reg_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, mic_dma_reg_seq_show, inode->i_private);
}

static int mic_dma_reg_debug_release(struct inode *inode, struct file *file)
{
	return single_release(inode, file);
}

static const struct file_operations mic_dma_reg_ops = {
	.owner   = THIS_MODULE,
	.open    = mic_dma_reg_debug_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = mic_dma_reg_debug_release
};

static int mic_dma_desc_ring_seq_show(struct seq_file *s, void *pos)
{
	struct mic_dma_device *mic_dma_dev = s->private;
	int i;
	struct mic_dma_chan *ch;
	struct mic_dma_desc *desc;
	u64 src, dst;

	ch = &mic_dma_dev->mic_chan;

	mic_dma_dump_ring_pos(s, ch);

	if (!ch->desc_ring)
		return 0;

	for (i = 0; i < MIC_DMA_DESC_RX_SIZE; i++) {
		desc = &ch->desc_ring[i];
		/*
		 * Descriptor ring is too big to show all descriptors,
		 * for now only display valid ones.
		 * If you need to see all descriptors change
		 * MIC_DMA_DESC_RX_SIZE to 32K or smaller.
		 */
		if (!!(desc->qw0 & MIC_DMA_DESC_VALID)) {
			src = (desc->qw1 >> 32) | (desc->qw0 >> 48) << 32;
			dst = (desc->qw1 & MIC_DMA_DESC_LOW_MASK);
			dst |= (((desc->qw0 >> 32) & 0xffff) << 32);
			seq_printf(s, "%d: v %d i %d se %d de %d l %7xh",
				   i, !!(desc->qw0 & MIC_DMA_DESC_VALID),
				   !!(desc->qw0 & MIC_DMA_DESC_INTR_ENABLE),
				   !!(desc->qw0 & MIC_DMA_DESC_SRC_LINK_ERR),
				   !!(desc->qw0 & MIC_DMA_DESC_DST_LINK_ERR),
				   (int)(desc->qw0 & MIC_DMA_DESC_SIZE_MASK));
			seq_printf(s, " src 0x%llx dst 0x%llx\n",  src, dst);
		}
	}
	mic_dma_dump_ring_pos(s, ch);

	return 0;
}

static int mic_dma_desc_ring_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, mic_dma_desc_ring_seq_show, inode->i_private);
}

static int
mic_dma_desc_ring_debug_release(struct inode *inode, struct file *file)
{
	return single_release(inode, file);
}

static const struct file_operations mic_dma_desc_ring_ops = {
	.owner   = THIS_MODULE,
	.open    = mic_dma_desc_ring_debug_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = mic_dma_desc_ring_debug_release
};

struct dmatest_done {
	bool                    done;
	wait_queue_head_t       *wait;
};

static void dmatest_callback(void *arg)
{
	struct dmatest_done *done = arg;

	done->done = true;
	wake_up_all(done->wait);
}

static int mic_dma_intr_test_seq_show(struct seq_file *s, void *pos)
{
	dma_cap_mask_t mask;
	struct dma_chan *chan;
	struct dma_device *ddev;
	struct dma_async_tx_descriptor *tx = NULL;
	dma_cookie_t cookie;

	DECLARE_WAIT_QUEUE_HEAD_ONSTACK(done_wait);
	struct dmatest_done done = { .wait = &done_wait };
	enum dma_status status;

	dma_cap_zero(mask);
	dma_cap_set(DMA_MEMCPY, mask);

	chan = dma_request_channel(mask, NULL, NULL);
	if (!chan)
		goto done;
	ddev = chan->device;
	tx = ddev->device_prep_dma_interrupt(chan, DMA_PREP_INTERRUPT);
	if (!tx)
		goto release;
	done.done = false;
	tx->callback = dmatest_callback;
	tx->callback_param = &done;
	cookie = tx->tx_submit(tx);
	if (dma_submit_error(cookie))
		goto release;
	dma_async_issue_pending(chan);
	wait_event_freezable_timeout(done_wait, done.done,
					 msecs_to_jiffies(1000));
	status = dma_async_is_tx_complete(chan, cookie, NULL, NULL);
	if (!done.done)
		goto release;
	else if (status != DMA_COMPLETE)
		goto release;
	tx = ddev->device_prep_dma_memcpy(chan, 0, 0, 0, DMA_PREP_FENCE);
	if (!tx)
		goto release;
	cookie = tx->tx_submit(tx);
	if (dma_submit_error(cookie))
		goto release;
	dma_async_issue_pending(chan);
	msleep(100);
	status = dma_async_is_tx_complete(chan, cookie, NULL, NULL);
	if (!done.done)
		goto release;
	else if (status != DMA_COMPLETE)
		goto release;
	seq_puts(s, "Interrupt Test passed!\n");
	dma_release_channel(chan);
	return 0;
release:
	seq_puts(s, "Interrupt Test failed\n");
	dma_release_channel(chan);
done:
	return 0;
}

static int mic_dma_intr_test_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, mic_dma_intr_test_seq_show, inode->i_private);
}

static int
mic_dma_intr_test_debug_release(struct inode *inode, struct file *file)
{
	return single_release(inode, file);
}

static const struct file_operations mic_dma_intr_test_ops = {
	.owner   = THIS_MODULE,
	.open    = mic_dma_intr_test_debug_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = mic_dma_intr_test_debug_release
};

#ifdef MIC_DMA_DEBUG
static int mic_dma_dbg_trace_seq_show(struct seq_file *s, void *pos)
{
	int i;
	struct mic_dma_desc *desc;
	u64 src, dst;

	if (!mic_dma_desc_dbg)
		return 0;

	for (i = 0; i < mic_dma_desc_dbg_count; i++) {
		struct mic_dma_desc_debug *dbg = &mic_dma_desc_dbg[i];

		desc = &dbg->desc;
		src = (desc->qw1 >> 32) | (desc->qw0 >> 48) << 32;
		dst = (desc->qw1 & MIC_DMA_DESC_LOW_MASK);
		dst |= (((desc->qw0 >> 32) & 0xffff) << 32);
		seq_printf(s, "[%d] v 0x%x in 0x%x df 0x%x sz 0x%llx",
			   i, !!(desc->qw0 & MIC_DMA_DESC_VALID),
			   !!(desc->qw0 & MIC_DMA_DESC_INTR_ENABLE),
			   !!(desc->qw0 & MIC_DMA_DESC_STD_DESC),
			   desc->qw0 & MIC_DMA_DESC_SIZE_MASK);
		seq_printf(s, " src 0x%llx dst 0x%llx head %d tail %d",
			   src, dst, dbg->head, dbg->tail);
		seq_printf(s, " hw_tail 0x%x space %d\n",
			   dbg->hw_tail, dbg->space);
	}
	return 0;
}

static int
mic_dma_dbg_trace_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, mic_dma_dbg_trace_seq_show, inode->i_private);
}

static int
mic_dma_dbg_trace_debug_release(struct inode *inode, struct file *file)
{
	return single_release(inode, file);
}

static const struct file_operations mic_dma_dbg_trace_ops = {
	.owner   = THIS_MODULE,
	.open    = mic_dma_dbg_trace_debug_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = mic_dma_dbg_trace_debug_release
};
#endif

static int mic_dma_flush_seq_show(struct seq_file *s, void *pos)
{
	struct mic_dma_device *mic_dma_dev = s->private;
	struct dma_chan *chan = &mic_dma_dev->mic_chan.chan;
	struct dma_device *ddev = chan->device;
	struct dma_async_tx_descriptor *tx = NULL;
	DECLARE_WAIT_QUEUE_HEAD_ONSTACK(done_wait);
	struct dmatest_done done = { .done = false, .wait = &done_wait };
	dma_cookie_t cookie;
	enum dma_status status;

	tx = ddev->device_prep_dma_interrupt(chan, DMA_PREP_INTERRUPT);
	if (!tx)
		goto err;
	mic_dma_dev->mic_chan.dbg_flush = true;
	tx->callback = dmatest_callback;
	tx->callback_param = &done;
	cookie = tx->tx_submit(tx);
	if (dma_submit_error(cookie))
		goto err;
	dma_async_issue_pending(chan);
	msleep(1000);
	status = dma_async_is_tx_complete(chan, cookie, NULL, NULL);
	if (!done.done)
		seq_puts(s, "Flush DMA interrupt pending\n");
	else
		seq_puts(s, "Flush DMA interrupt received\n");
	if (status == DMA_COMPLETE)
		seq_puts(s, "Flush DMA tx complete!\n");
	else
		seq_puts(s, "Flush DMA tx pending!\n");
	return 0;
err:
	seq_puts(s, "Flush attempt failed\n");
	return 0;
}

static int mic_dma_flush_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, mic_dma_flush_seq_show, inode->i_private);
}

static int mic_dma_flush_debug_release(struct inode *inode, struct file *file)
{
	return single_release(inode, file);
}

static const struct file_operations mic_dma_flush_ops = {
	.owner   = THIS_MODULE,
	.open    = mic_dma_flush_debug_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = mic_dma_flush_debug_release
};

static int mic_dma_cleanup_seq_show(struct seq_file *s, void *pos)
{
	struct mic_dma_device *mic_dma_dev = s->private;
	struct mic_dma_chan *mic_chan = &mic_dma_dev->mic_chan;

	mic_dma_cleanup(mic_chan);
	return 0;
}

static int mic_dma_cleanup_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, mic_dma_cleanup_seq_show, inode->i_private);
}

static int mic_dma_cleanup_debug_release(struct inode *inode, struct file *file)
{
	return single_release(inode, file);
}

static const struct file_operations mic_dma_cleanup_ops = {
	.owner   = THIS_MODULE,
	.open    = mic_dma_cleanup_debug_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = mic_dma_cleanup_debug_release
};

static int mic_dma_stop_dbg_show(struct seq_file *s, void *pos)
{
	seq_puts(s, "Write to stop DMA engine\n");
	return 0;
}

static int mic_dma_stop_dbg_open(struct inode *inode, struct file *file)
{
	return single_open(file, mic_dma_stop_dbg_show, inode->i_private);
}

static ssize_t mic_dma_stop_dbg_write(struct file *file,
			const char __user *user_buf,
			size_t size, loff_t *ppos)
{
	struct mic_dma_device *mic_dma_dev = file->f_inode->i_private;
	struct mic_dma_chan *ch = &mic_dma_dev->mic_chan;

	mic_dma_disable_chan(ch);

	return size;
}

static const struct file_operations mic_dma_stop_dbg_ops = {
	.owner	 = THIS_MODULE,
	.open	 = mic_dma_stop_dbg_open,
	.read	 = seq_read,
	.write	 = mic_dma_stop_dbg_write,
	.release = single_release
};

static int mic_dma_start_dbg_show(struct seq_file *s, void *pos)
{
	seq_puts(s, "Write to start DMA engine\n");
	return 0;
}

static int mic_dma_start_dbg_open(struct inode *inode, struct file *file)
{
	return single_open(file, mic_dma_start_dbg_show, inode->i_private);
}

static ssize_t mic_dma_start_dbg_write(struct file *file,
			const char __user *user_buf,
			size_t size, loff_t *ppos)
{
	struct mic_dma_device *mic_dma_dev = file->f_inode->i_private;
	struct mic_dma_chan *ch = &mic_dma_dev->mic_chan;

	mic_dma_enable_chan(ch);

	return size;
}

static const struct file_operations mic_dma_start_dbg_ops = {
	.owner	 = THIS_MODULE,
	.open	 = mic_dma_start_dbg_open,
	.read	 = seq_read,
	.write	 = mic_dma_start_dbg_write,
	.release = single_release
};

/* Debufs driver root directory */
static struct dentry *mic_dma_dbg_dir;

static void mic_dma_debugfs_init(struct mic_dma_device *mic_dma_dev)
{
	struct dma_device *dma_dev = &mic_dma_dev->dma_dev;

	if (!mic_dma_dbg_dir)
		return;

	mic_dma_dev->dbg_dir =	debugfs_create_dir(dev_name(dma_dev->dev),
						   mic_dma_dbg_dir);
	if (!mic_dma_dev->dbg_dir)
		return;

	debugfs_create_file("dma_reg", 0444,
				mic_dma_dev->dbg_dir, mic_dma_dev,
				&mic_dma_reg_ops);
	debugfs_create_file("desc_ring", 0444,
				mic_dma_dev->dbg_dir, mic_dma_dev,
				&mic_dma_desc_ring_ops);
	debugfs_create_file("intr_test", 0444,
				mic_dma_dev->dbg_dir, mic_dma_dev,
				&mic_dma_intr_test_ops);
#ifdef MIC_DMA_DEBUG
	debugfs_create_file("debug_trace", 0444,
				mic_dma_dev->dbg_dir, mic_dma_dev,
				&mic_dma_dbg_trace_ops);
#endif
	debugfs_create_file("flush", 0444,
				mic_dma_dev->dbg_dir, mic_dma_dev,
				&mic_dma_flush_ops);
	debugfs_create_file("cleanup", 0444,
				mic_dma_dev->dbg_dir, mic_dma_dev,
				&mic_dma_cleanup_ops);
	/* MIC_DMA_FIX remove timeout before upstreaming */
	debugfs_create_u32("dma_ring_timeout_ms", 0644,
				mic_dma_dev->dbg_dir,
				&mic_dma_ring_timeout_ms);
	debugfs_create_file("start", 0644,
				mic_dma_dev->dbg_dir, mic_dma_dev,
				&mic_dma_start_dbg_ops);
	debugfs_create_file("stop", 0644,
				mic_dma_dev->dbg_dir, mic_dma_dev,
				&mic_dma_stop_dbg_ops);
}


#define PLX_PCI_VENDOR_ID_PLX   0x10B5
#define INTEL_PCI_DEVICE_2264	0x2264
#define MIC_DMA_DRV_NAME		"mic_x200_dma"

static struct pci_device_id mic_x200_dma_pci_id_table[] = {
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, INTEL_PCI_DEVICE_2264)},
	{PCI_DEVICE(PLX_PCI_VENDOR_ID_PLX, 0x87D0)},
	{PCI_DEVICE(PLX_PCI_VENDOR_ID_PLX, 0x87E0)},
	{ 0 } /* Required last entry */
};

MODULE_DEVICE_TABLE(pci, mic_x200_dma_pci_id_table);

static struct mic_dma_device *
mic_x200_alloc_dma(struct pci_dev *pdev, void __iomem *iobase)
{
	struct mic_dma_device *d = kzalloc(sizeof(*d), GFP_KERNEL);

	if (!d)
		return NULL;
	d->pdev = pdev;
	d->reg_base = iobase;
	return d;
}

static int
mic_x200_dma_probe(struct pci_dev *pdev, const struct pci_device_id *pdev_id)
{
	int rc;
	struct mic_dma_device *dma_dev;
	void __iomem * const *iomap;

	rc = pcim_enable_device(pdev);
	if (rc)
		goto done;
	rc = pcim_iomap_regions(pdev, 0x1, MIC_DMA_DRV_NAME);
	if (rc)
		goto done;
	iomap = pcim_iomap_table(pdev);
	if (!iomap) {
		rc = -ENOMEM;
		goto done;
	}
	rc = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(39));
	if (rc)
		goto done;
	dma_dev = mic_x200_alloc_dma(pdev, iomap[0]);
	if (!dma_dev) {
		rc = -ENOMEM;
		goto done;
	}
	pci_set_master(pdev);
	pci_set_drvdata(pdev, dma_dev);
	rc = mic_dma_probe(dma_dev);
	if (rc)
		goto probe_err;
	return 0;
probe_err:
	kfree(dma_dev);
done:
	dev_err(&pdev->dev, "probe error rc %d\n", rc);
	return rc;
}

static void  mic_x200_dma_remove(struct pci_dev *pdev)
{
	struct mic_dma_device *dma_dev = pci_get_drvdata(pdev);

	mic_dma_remove(dma_dev);
	kfree(dma_dev);
}

static struct pci_driver mic_x200_dma_driver = {
	.name     = MIC_DMA_DRV_NAME,
	.id_table = mic_x200_dma_pci_id_table,
	.probe    = mic_x200_dma_probe,
	.remove   = mic_x200_dma_remove
};

static int __init mic_x200_dma_init(void)
{
	int rc;

	mic_dma_dbg_dir = debugfs_create_dir(KBUILD_MODNAME, NULL);
	rc = pci_register_driver(&mic_x200_dma_driver);
	if (rc)
		pr_err("%s %d rc %x\n", __func__, __LINE__, rc);
	return rc;
}

static void __exit mic_x200_dma_exit(void)
{
	pci_unregister_driver(&mic_x200_dma_driver);
	debugfs_remove_recursive(mic_dma_dbg_dir);
}

module_init(mic_x200_dma_init);
module_exit(mic_x200_dma_exit);

MODULE_AUTHOR("Intel Corporation");
MODULE_DESCRIPTION("Intel(R) MIC X200 DMA Driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(MIC_VERSION);
