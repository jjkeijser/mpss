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
#include<linux/slab.h>
#include<asm/io.h>
#include<linux/kernel.h>

#include <mic/micscif_smpt.h>
#include <mic/mic_dma_md.h>
#include <mic/mic_dma_api.h>

#define PR_PREFIX "DMA_LIB_MD:"

#ifdef CONFIG_ML1OM
#define MIC_DMA_AES_CHAN_NUM	7
#define is_AES_channel(n) ((n) == MIC_DMA_AES_CHAN_NUM)
#else
#define is_AES_channel(n) ((void)(n), 0)
#endif

#define DMA_CHAN_COOKIE		0xdeadc0d

#define SBOX_DCAR_IM0		(0x1 << 24)	// APIC Interrupt mask bit
#define SBOX_DCAR_IM1		(0x1 << 25)	// MSI-X Interrupt mask bit
#define SBOX_DCAR_IS0		(0x1 << 26)	// Interrupt status

#define SBOX_DRARHI_SYS_MASK	(0x1 << 26)

#ifdef _MIC_SCIF_
static inline uint32_t chan_to_dcr_mask(uint32_t dcr, struct md_mic_dma_chan *chan, struct mic_dma_device *dma_dev)
{
	uint32_t chan_num = chan->ch_num;
	uint32_t owner;

	if (!is_AES_channel(chan_num))
		owner = chan->owner;
	else
		owner = chan->endianness;

	return ((dcr & ~(0x1 << (chan_num * 2))) | (owner << (chan_num * 2)));
}
#endif

static inline uint32_t drar_hi_to_ba_bits(uint32_t drar_hi)
{
	/*
	 * Setting bits 3:2 should generate a DESC_ADDR_ERR but the hardware ignores
	 * these bits currently and doesn't generate the error.
	 */
#ifdef _MIC_SCIF_
	return drar_hi & 0xf;
#else
	return drar_hi & 0x3;
#endif
}

static inline uint32_t physaddr_to_drarhi_ba(phys_addr_t phys_addr)
{
	return drar_hi_to_ba_bits((uint32_t)(phys_addr >> 32));
}

static inline uint32_t size_to_drar_hi_size(uint32_t size)
{
	return (size & 0x1ffff) << 4;
}

static inline uint32_t addr_to_drar_hi_smpt_bits(phys_addr_t mic_phys_addr)
{
	return ((mic_phys_addr >> MIC_SYSTEM_PAGE_SHIFT) & 0x1f) << 21;
}

static inline uint32_t drar_hi_to_smpt(uint32_t drar_hi, uint32_t chan_num)
{
	return ((drar_hi >> 21) & 0x1f);
}

void md_mic_dma_enable_chan(struct mic_dma_device *dma_dev, uint32_t chan_num, bool enable);


#ifdef _MIC_SCIF_
/**
 * md_mic_dma_chan_init_attr - Set channel attributes like owner and endianness
 * @chan: The DMA channel handle
 */
void md_mic_dma_chan_init_attr(struct mic_dma_device *dma_dev,
				      struct md_mic_dma_chan *chan)
{
	uint32_t dcr;

	CHECK_CHAN(chan);

	dcr = mic_sbox_read_mmio(dma_dev->mm_sbox, SBOX_DCR);
	dcr = chan_to_dcr_mask(dcr, chan, dma_dev);
	mic_sbox_write_mmio(dma_dev->mm_sbox, SBOX_DCR, dcr);
}
#endif

/* One time DMA Init API */
void md_mic_dma_init(struct mic_dma_device *dma_dev, uint8_t *mmio_va_base)
{
	int i;
#ifdef _MIC_SCIF_
	dma_dev->mm_sbox = mic_sbox_md_init();
#else
	dma_dev->mm_sbox = mmio_va_base;
#endif
	//pr_debug("sbox: va=%p\n", dma_dev.mm_sbox);

	for (i = 0; i < MAX_NUM_DMA_CHAN; i++) {
		atomic_set(&(dma_dev->chan_info[i].in_use), CHAN_AVAILABLE);
		dma_dev->chan_info[i].cookie = DMA_CHAN_COOKIE;
		dma_dev->chan_info[i].dstat_wb_phys = 0;
		dma_dev->chan_info[i].dstat_wb_loc = NULL;
	}
	return;
}

/* One time DMA Uninit API */
void md_mic_dma_uninit(struct mic_dma_device *dma_dev)
{
	return;
}

/**
 * md_mic_dma_request_chan
 * @owner: DMA channel owner: MIC or Host
 *
 * Return - The DMA channel handle or NULL if failed
 *
 * Note: Allocating a Host owned channel is not allowed currently
 */
struct md_mic_dma_chan *md_mic_dma_request_chan(struct mic_dma_device *dma_dev,
						enum md_mic_dma_chan_owner owner)
{
	struct md_mic_dma_chan *tmp = NULL;
	int i;

	for (i = 0; i < MAX_NUM_DMA_CHAN; i++) {
		if (CHAN_AVAILABLE == atomic_cmpxchg(&(dma_dev->chan_info[i].in_use), 
							   CHAN_AVAILABLE, CHAN_INUSE)) {
			tmp = &dma_dev->chan_info[i];
			tmp->owner = owner;
			tmp->ch_num = i;
			/*
			 * Setting endianness by default to MIC_LITTLE_ENDIAN
			 * in case the AES channel is used for clear transfers
			 * This is a don't care for clear transfers.
			 */
			tmp->endianness = MIC_LITTLE_ENDIAN;
#ifdef _MIC_SCIF_
			md_mic_dma_chan_init_attr(dma_dev, tmp);
#endif
			break;
		}
	}
	return tmp;
}

/**
 * md_mic_dma_free_chan - Frees up a DMA channel
 * @chan: The DMA channel handle
 */
void md_mic_dma_free_chan(struct mic_dma_device *dma_dev,
			  struct md_mic_dma_chan *chan)
{
	CHECK_CHAN(chan);
	atomic_set(&(chan->in_use), CHAN_AVAILABLE);
	md_mic_dma_enable_chan(dma_dev, chan->ch_num, false);
}

/**
 * md_mic_dma_enable_chan - Enable/disable the DMA channel
 * @chan_num: The DMA channel
 * @enable: enable/disable
 *
 * Must set desc ring and update head pointer only
 * after disabling the channel
 */
void md_mic_dma_enable_chan(struct mic_dma_device *dma_dev,
			    uint32_t chan_num, bool enable)
{
	uint32_t dcr = mic_sbox_read_mmio(dma_dev->mm_sbox, SBOX_DCR);

	/*
	 * There is a separate bit for every channel.
	 * Look up sboxDcrReg.
	 */
	if (enable) {
		dcr |= 2 << (chan_num << 1);
	} else {
		dcr &= ~(2 << (chan_num << 1));
	}
	mic_sbox_write_mmio(dma_dev->mm_sbox, SBOX_DCR, dcr);
}

#if 0
uint32_t md_mic_dma_chan_read_completion_count(struct mic_dma_device *dma_dev, struct md_mic_dma_chan *chan)
{
	CHECK_CHAN(chan);

	return (md_mic_dma_read_mmio(dma_dev, chan->ch_num, REG_DSTAT) & 0xffff);
}


/* This function needs to be used only in error case */
void update_compcount_and_tail(struct mic_dma_device *dma_dev, struct md_mic_dma_chan *chan)
{
	chan->completion_count = md_mic_dma_chan_read_completion_count(dma_dev, chan);
	chan->cached_tail = md_mic_dma_chan_read_tail(dma_dev, chan);
}
#endif
void md_mic_dma_chan_set_dstat_wb(struct mic_dma_device *dma_dev,
					struct md_mic_dma_chan *chan)
{
	uint32_t dstat_wb, dstat_wb_hi;
	CHECK_CHAN(chan);

	dstat_wb = (uint32_t)chan->dstat_wb_phys;
	dstat_wb_hi = chan->dstat_wb_phys >> 32;
	md_mic_dma_write_mmio(dma_dev, chan->ch_num, REG_DSTATWB_LO, dstat_wb);
	md_mic_dma_write_mmio(dma_dev, chan->ch_num, REG_DSTATWB_HI, dstat_wb_hi);
}

void md_mic_dma_chan_set_dcherr_msk(struct mic_dma_device *dma_dev,
				struct md_mic_dma_chan *chan, uint32_t mask)
{
	CHECK_CHAN(chan);
	md_mic_dma_write_mmio(dma_dev, chan->ch_num, REG_DCHERRMSK, mask);
}
#if 0
uint32_t md_mic_dma_chan_get_dcherr_msk(struct mic_dma_device *dma_dev,
				struct md_mic_dma_chan *chan)
{
	CHECK_CHAN(chan);
	return md_mic_dma_read_mmio(dma_dev, chan->ch_num, REG_DCHERRMSK);
}

uint32_t md_mic_dma_chan_get_dcherr(struct mic_dma_device *dma_dev,
				struct md_mic_dma_chan *chan)
{
	CHECK_CHAN(chan);
	return md_mic_dma_read_mmio(dma_dev, chan->ch_num, REG_DCHERR);
}

void md_mic_dma_chan_set_dcherr(struct mic_dma_device *dma_dev,
				struct md_mic_dma_chan *chan, uint32_t value)
{
	CHECK_CHAN(chan);
	md_mic_dma_write_mmio(dma_dev, chan->ch_num, REG_DCHERR, value);
	printk("dcherr = %d\n", md_mic_dma_read_mmio(dma_dev, chan->ch_num, REG_DCHERR));
}
#endif

/**
 * md_mic_dma_chan_set_desc_ring - Configures the DMA channel desc ring
 * @chan: The DMA channel handle
 * @desc_ring_phys_addr: Physical address of the desc ring base. Needs to be
 *                       physically contiguous and wired down memory.
 * @num_desc: Number of descriptors must be a multiple of cache line size.
 * Descriptor size should be determined using sizeof(union md_mic_dma_desc).
 *            The maximum number of descriptors is defined by
 *            MIC_MAX_NUM_DESC_PER_RING.
 */
void md_mic_dma_chan_set_desc_ring(struct mic_dma_device *dma_dev,
				   struct md_mic_dma_chan *chan,
				   phys_addr_t desc_ring_phys_addr,
				   uint32_t num_desc)
{
	uint32_t chan_num;
	uint32_t drar_lo = 0;
	uint32_t drar_hi = 0;

	CHECK_CHAN(chan);
	chan_num = chan->ch_num;
	/*
	 * TODO: Maybe the 2nd condition should be different considering the
	 * size of union md_mic_dma_desc?
	 */
	KASSERT((((num_desc) <= MIC_MAX_NUM_DESC_PER_RING) &&
		(ALIGN((num_desc - (L1_CACHE_BYTES - 1)), L1_CACHE_BYTES) == num_desc)),
		"num_desc > max or not multiple of cache line num 0x%x", num_desc);

	md_mic_dma_enable_chan(dma_dev, chan_num, false);

	drar_hi = size_to_drar_hi_size(num_desc);

	if (MIC_DMA_CHAN_HOST_OWNED == chan->owner) {
		drar_hi |= SBOX_DRARHI_SYS_MASK;
		drar_hi |= addr_to_drar_hi_smpt_bits(desc_ring_phys_addr);
	}
	drar_lo = (uint32_t)desc_ring_phys_addr;
	drar_hi |= physaddr_to_drarhi_ba(desc_ring_phys_addr);
	md_mic_dma_write_mmio(dma_dev, chan_num, REG_DRAR_LO, drar_lo);
	md_mic_dma_write_mmio(dma_dev, chan_num, REG_DRAR_HI, drar_hi);
	chan->num_desc_in_ring = num_desc;
	pr_debug("md_mic_dma_chan_set_desc_ring addr=0x%llx num=%d drar_hi.bits.pageno 0x%x\n", 
			desc_ring_phys_addr, num_desc, 
			(uint32_t)(desc_ring_phys_addr >> MIC_SYSTEM_PAGE_SHIFT));
	chan->cached_tail = md_mic_dma_chan_read_tail(dma_dev, chan);

	md_mic_dma_enable_chan(dma_dev, chan_num, true);
}

uint32_t md_mic_dma_chan_read_head(struct mic_dma_device *dma_dev, struct md_mic_dma_chan *chan)
{
	CHECK_CHAN(chan);

	return md_mic_dma_read_mmio(dma_dev, chan->ch_num, REG_DHPR);
}

uint32_t md_mic_dma_chan_read_tail(struct mic_dma_device *dma_dev, struct md_mic_dma_chan *chan)
{
	CHECK_CHAN(chan);

	return md_mic_dma_read_mmio(dma_dev, chan->ch_num, REG_DTPR);
}

/**
 * md_mic_dma_chan_intr_pending - Reads interrupt status to figure out
 *                              if an interrupt is pending.
 * @chan: The DMA channel handle.
 */
bool md_mic_dma_chan_intr_pending(struct mic_dma_device *dma_dev, struct md_mic_dma_chan *chan)
{
	uint32_t dcar;
	CHECK_CHAN(chan);

	dcar = md_mic_dma_read_mmio(dma_dev, chan->ch_num, REG_DCAR);
	return (dcar >> 26) & 0x1;
}

/**
 * md_mic_dma_chan_mask_intr - Mask or disable interrupts
 * @chan: The DMA channel handle
 *
 * Masking interrupts will also acknowledge any pending
 * interrupts on the channel.
 */
void md_mic_dma_chan_mask_intr(struct mic_dma_device *dma_dev, struct md_mic_dma_chan *chan)
{
	uint32_t dcar;
	uint32_t chan_num;
	CHECK_CHAN(chan);
	chan_num = chan->ch_num;

	dcar = md_mic_dma_read_mmio(dma_dev, chan_num, REG_DCAR);

	if (MIC_DMA_CHAN_MIC_OWNED == chan->owner)
		dcar |= SBOX_DCAR_IM0;
	else
		dcar |= SBOX_DCAR_IM1;

	md_mic_dma_write_mmio(dma_dev, chan_num, REG_DCAR, dcar);
	/*
	 * This read is completed only after previous write is completed.
	 * It guarantees that, interrupts has been acknowledged to SBOX DMA
	 * This read forces previous write to be commited in memory.
	 * This is the actual fix for HSD# 3497216 based on theoretical
	 * hypothesis that somehow previous write is not truly completed
	 * since for writes as long as transactions are accepted by SBOX
	 * ( not necessarily commited in memory) those write transactions
	 * reported as complete.
	 */
	dcar = md_mic_dma_read_mmio(dma_dev, chan_num, REG_DCAR);
}

/**
 * md_mic_dma_chan_unmask_intr - Unmask or enable interrupts
 * @chan: The DMA channel handle
 */
void md_mic_dma_chan_unmask_intr(struct mic_dma_device *dma_dev, struct md_mic_dma_chan *chan)
{
	uint32_t dcar;
	uint32_t chan_num;
	CHECK_CHAN(chan);
	chan_num = chan->ch_num;

	dcar = md_mic_dma_read_mmio(dma_dev, chan_num, REG_DCAR);

	if (MIC_DMA_CHAN_MIC_OWNED == chan->owner)
		dcar &= ~SBOX_DCAR_IM0;
	else
		dcar &= ~SBOX_DCAR_IM1;

	md_mic_dma_write_mmio(dma_dev, chan_num, REG_DCAR, dcar);
	/*
	 * This read is completed only after previous write is completed.
	 * It guarantees that, interrupts has been acknowledged to SBOX DMA
	 * This read forces previous write to be commited in memory.
	 * This is the actual fix for HSD# 3497216 based on theoretical
	 * hypothesis that somehow previous write is not truly completed
	 * since for writes as long as transactions are accepted by SBOX
	 * ( not necessarily commited in memory) those write transactions
	 * reported as complete.
	 */
	dcar = md_mic_dma_read_mmio(dma_dev, chan_num, REG_DCAR);
}

/**
 * md_mic_dma_chan_get_desc_ring_phys - Compute the value of the descriptor ring
 * base physical address from the descriptor ring attributes register.
 * @dma_dev: DMA device.
 * @chan: The DMA channel handle
 */
phys_addr_t
md_mic_dma_chan_get_desc_ring_phys(struct mic_dma_device *dma_dev, struct md_mic_dma_chan *chan)
{
	phys_addr_t phys, phys_hi;
	uint32_t phys_lo, chan_num, drar_hi;

	CHECK_CHAN(chan);
	chan_num = chan->ch_num;
	phys_lo = md_mic_dma_read_mmio(dma_dev, chan_num, REG_DRAR_LO);
	drar_hi = md_mic_dma_read_mmio(dma_dev, chan_num, REG_DRAR_HI);
	phys_hi = drar_hi_to_ba_bits(drar_hi);
	phys_hi |= drar_hi_to_smpt(drar_hi, chan_num) << 2;

	phys = phys_lo | (phys_hi << 32);
	return phys;
}

/**
 * md_mic_dma_chan_get_dstatwb_phys - Compute the value of the DSTAT write back
 * physical address.
 * @dma_dev: DMA device.
 * @chan: The DMA channel handle
 */
phys_addr_t md_mic_dma_chan_get_dstatwb_phys(struct mic_dma_device *dma_dev,
			struct md_mic_dma_chan *chan)
{
	uint32_t reg, chan_num;
	phys_addr_t phys;

	CHECK_CHAN(chan);
	chan_num = chan->ch_num;
	reg = md_mic_dma_read_mmio(dma_dev, chan_num, REG_DSTATWB_HI);
	phys = reg;
	reg = md_mic_dma_read_mmio(dma_dev, chan_num, REG_DSTATWB_LO);

	phys = phys << 32 | reg;
	return phys;
}

/**
 * md_mic_dma_prep_nop_desc - Prepares a NOP descriptor.
 * @desc: Descriptor to be populated.
 *
 * This descriptor is used to pad a cacheline if the previous
 * descriptor does not end on a cacheline boundary.
 */
void md_mic_dma_prep_nop_desc(union md_mic_dma_desc *desc)
{
	KASSERT((desc != 0), ("NULL desc"));

	desc->qwords.qw0 = 0;
	desc->qwords.qw1 = 0;
	desc->desc.nop.type = 0;
}

/* Only Debug Code Below */

/**
 * md_mic_dma_print_debug - Print channel debug information
 * @chan: The DMA channel handle
 * @sbuf: Print to an sbuf if not NULL else prints to console
 */
void md_mic_dma_print_debug(struct mic_dma_device *dma_dev, struct md_mic_dma_chan *chan)
{
	uint32_t dcr;
	uint32_t dcar;
	uint32_t dtpr;
	uint32_t dhpr;
	uint32_t drar_lo;
	uint32_t drar_hi;
	uint32_t dstat;
	uint32_t chan_num = chan->ch_num;

	dcr  = mic_sbox_read_mmio(dma_dev->mm_sbox, SBOX_DCR);
	dcar = md_mic_dma_read_mmio(dma_dev, chan_num, REG_DCAR);
	dtpr = md_mic_dma_read_mmio(dma_dev, chan_num, REG_DTPR);
	dhpr = md_mic_dma_read_mmio(dma_dev, chan_num, REG_DHPR);
	drar_lo = md_mic_dma_read_mmio(dma_dev, chan_num, REG_DRAR_LO);
	drar_hi = md_mic_dma_read_mmio(dma_dev, chan_num, REG_DRAR_HI);
	dstat = md_mic_dma_read_mmio(dma_dev, chan_num, REG_DSTAT);
	pr_debug(PR_PREFIX "Chan_Num 0x%x DCR 0x%x DCAR 0x%x DTPR 0x%x" 
			      "DHPR 0x%x DRAR_HI 0x%x DRAR_LO 0x%x DSTAT 0x%x\n", 
			      chan_num, dcr, dcar, dtpr, dhpr, drar_hi, drar_lo, dstat);
	pr_debug(PR_PREFIX "DCR 0x%x\n", dcr);
}
