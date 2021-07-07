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

#ifndef MIC_DMA_MD_H
#define MIC_DMA_MD_H

#include "mic_sbox_md.h"
#include "micsboxdefine.h"

#define MAX_NUM_DMA_CHAN 8
/*
 * WE ASSUME 0 to __LAST_HOST_CHAN_NUM are owned by host
 * Keep this in mind when changing this value
 */
#define __LAST_HOST_CHAN_NUM	3

#ifdef _MIC_SCIF_
static inline int first_dma_chan(void)
{
	return __LAST_HOST_CHAN_NUM + 1;
}

static inline int last_dma_chan(void)
{
	return MAX_NUM_DMA_CHAN - 1;
}
#else
static inline int first_dma_chan(void)
{
	return 0;
}

static inline int last_dma_chan(void)
{
	return __LAST_HOST_CHAN_NUM;
}
#endif
enum md_mic_dma_chan_reg {
	REG_DCAR = 0,
	REG_DHPR,
	REG_DTPR,
	REG_DAUX_HI,
	REG_DAUX_LO,
	REG_DRAR_HI,
	REG_DRAR_LO,
	REG_DITR,
	REG_DSTAT,
	REG_DSTATWB_LO,
	REG_DSTATWB_HI,
	REG_DCHERR,
	REG_DCHERRMSK,
};


/* Pre-defined L1_CACHE_SHIFT is 6 on RH and 7 on Suse */
#undef L1_CACHE_SHIFT
#define L1_CACHE_SHIFT	6
#undef L1_CACHE_BYTES
#define L1_CACHE_BYTES	(1 << L1_CACHE_SHIFT)

enum dma_chan_flags {
	CHAN_AVAILABLE = 2,
	CHAN_INUSE = 3
};

/* Maximum DMA transfer size for a single memory copy descriptor */
#define MIC_MAX_DMA_XFER_SIZE  (((1U) * 1024 * 1024) - L1_CACHE_BYTES)

/* TODO:
 * I think it should be 128K - 64 (even 128k - 4 may work).
 * SIVA: Check this in the end
 */
/*
 * The maximum number of descriptors in the DMA descriptor queue is
 * 128K - 1 but since it needs to be a multiple of cache lines it is 128K - 64
 */
#define MIC_MAX_NUM_DESC_PER_RING	((128 * 1024) - L1_CACHE_BYTES)

/**
 * enum md_mic_dma_chan_owner - Memory copy DMA channels can be Host or MIC owned.
 *			AES channel can only be MIC owned.
 */
enum md_mic_dma_chan_owner {
	MIC_DMA_CHAN_MIC_OWNED = 0,
	MIC_DMA_CHAN_HOST_OWNED
};

/**
 * enum md_mic_dma_aes_endianness - Endianness needs to be provided
 *				only for the AES channel
 */
enum md_mic_dma_aes_endianness {
	/*
	 * The following two bits are opposite of what is given in
	 * content protection HAS but this is how it is implemented in RTL.
	 */
	MIC_BIG_ENDIAN = 0,
	MIC_LITTLE_ENDIAN
};


/**
 * struct md_mic_dma_chan - Opaque data structure for DMA channel specific fields.
 */
/*
 * struct md_mic_dma_chan: DMA channel specific structure
 * @in_use - true if the channel is in use and false otherwise
 * @owner - host or MIC required for masking/unmasking
 *		interrupts and enabling channels
 * @endianness - required for enabling AES channel
 * @cookie - Debug cookie to identify this structure
 * @num_desc_in_ring - Number of descriptors in the descriptor
 *                     ring for this channel.
 */
struct md_mic_dma_chan {
	int ch_num;
	atomic_t in_use;
	enum md_mic_dma_chan_owner owner;
	enum md_mic_dma_aes_endianness endianness;
	int cookie;
	uint32_t num_desc_in_ring;
	uint32_t cached_tail;
	uint32_t completion_count;
	void *dstat_wb_loc;
	dma_addr_t dstat_wb_phys;
	/* Add debug/profiling stats here */
};


/*
 * struct mic_dma_device - MIC DMA Device specific structure
 * @chan_info - static array of MIC DMA channel specific structures
 * @lock - MTX_DEF lock to synchronize allocation/deallocation of DMA channels
 */
struct mic_dma_device {
	struct md_mic_dma_chan chan_info[MAX_NUM_DMA_CHAN];
	void *mm_sbox;
};


/**
 * union md_mic_dma_desc - Opaque data structure for DMA descriptor format.
 */
/* TODO: Change bitfields to portable masks */
union md_mic_dma_desc {
	union {
		struct {
			uint64_t rsvd0;
			uint64_t rsvd1:60;
			uint64_t type:4;
		} nop;
		struct {
			uint64_t sap:40;
			uint64_t index:3;
			uint64_t rsvd0:3;
			uint64_t length:14;
			uint64_t rsvd1:4;
			uint64_t dap:40;
			uint64_t resd:15;
			uint64_t twb:1;
			uint64_t intr:1;
			uint64_t c:1;
			uint64_t co:1;
			uint64_t ecy:1;
			uint64_t type:4;
		} memcopy;
		struct {
			uint64_t data;
			uint64_t dap:40;
			uint64_t rsvdr0:19;
			uint64_t intr:1;
			uint64_t type:4;
		} status;
		struct {
			uint64_t data:32;
			uint64_t rsvd0:32;
			uint64_t dap:40;
			uint64_t rsvd1:20;
			uint64_t type:4;
		} general;
		struct {
			uint64_t data;
			uint64_t rsvd0:53;
			uint64_t cs:1;
			uint64_t index:3;
			uint64_t h:1;
			uint64_t sel:2;
			uint64_t type:4;
		} keynoncecnt;
		struct {
			uint64_t skap:40;
			uint64_t ski:3;
			uint64_t rsvd0:21;
			uint64_t rsvd1:51;
			uint64_t di:3;
			uint64_t rsvd2:6;
			uint64_t type:4;
		} key;
	} desc;
	struct {
		uint64_t qw0;
		uint64_t qw1;
	} qwords;
};

/* Initialization functions */
void md_mic_dma_init(struct mic_dma_device *dma_dev, uint8_t *mmio_va_base);
void md_mic_dma_uninit(struct mic_dma_device *dma_dev);
void md_mic_dma_chan_init_attr(struct mic_dma_device *dma_dev,
				      struct md_mic_dma_chan *chan);
void md_mic_dma_chan_mask_intr(struct mic_dma_device *dma_dev, struct md_mic_dma_chan *chan);
void md_mic_dma_chan_unmask_intr(struct mic_dma_device *dma_dev, struct md_mic_dma_chan *chan);
void md_mic_dma_chan_set_desc_ring(struct mic_dma_device *dma_dev,
				   struct md_mic_dma_chan *chan,
				   phys_addr_t desc_ring_phys_addr,
				   uint32_t num_desc);
void md_mic_dma_enable_chan(struct mic_dma_device *dma_dev, uint32_t chan_num, bool enable);
/* API */
struct md_mic_dma_chan *md_mic_dma_request_chan(struct mic_dma_device *dma_dev,
						enum md_mic_dma_chan_owner owner);
void md_mic_dma_free_chan(struct mic_dma_device *dma_dev, 
			  struct md_mic_dma_chan *chan);

static uint32_t mic_dma_reg[8][13] = {
	{SBOX_DCAR_0, SBOX_DHPR_0, SBOX_DTPR_0, SBOX_DAUX_HI_0, SBOX_DAUX_LO_0, SBOX_DRAR_HI_0,
	 SBOX_DRAR_LO_0, SBOX_DITR_0, SBOX_DSTAT_0,
	 SBOX_DSTATWB_LO_0, SBOX_DSTATWB_HI_0, SBOX_DCHERR_0, SBOX_DCHERRMSK_0},
	{SBOX_DCAR_1, SBOX_DHPR_1, SBOX_DTPR_1, SBOX_DAUX_HI_1, SBOX_DAUX_LO_1, SBOX_DRAR_HI_1,
	 SBOX_DRAR_LO_1, SBOX_DITR_1, SBOX_DSTAT_1,
	 SBOX_DSTATWB_LO_1, SBOX_DSTATWB_HI_1, SBOX_DCHERR_1, SBOX_DCHERRMSK_1},
	{SBOX_DCAR_2, SBOX_DHPR_2, SBOX_DTPR_2, SBOX_DAUX_HI_2, SBOX_DAUX_LO_2, SBOX_DRAR_HI_2,
	 SBOX_DRAR_LO_2, SBOX_DITR_2, SBOX_DSTAT_2,
	 SBOX_DSTATWB_LO_2, SBOX_DSTATWB_HI_2, SBOX_DCHERR_2, SBOX_DCHERRMSK_2},
	{SBOX_DCAR_3, SBOX_DHPR_3, SBOX_DTPR_3, SBOX_DAUX_HI_3, SBOX_DAUX_LO_3, SBOX_DRAR_HI_3,
	 SBOX_DRAR_LO_3, SBOX_DITR_3, SBOX_DSTAT_3,
	 SBOX_DSTATWB_LO_3, SBOX_DSTATWB_HI_3, SBOX_DCHERR_3, SBOX_DCHERRMSK_3},
	{SBOX_DCAR_4, SBOX_DHPR_4, SBOX_DTPR_4, SBOX_DAUX_HI_4, SBOX_DAUX_LO_4, SBOX_DRAR_HI_4,
	 SBOX_DRAR_LO_4, SBOX_DITR_4, SBOX_DSTAT_4,
	 SBOX_DSTATWB_LO_4, SBOX_DSTATWB_HI_4, SBOX_DCHERR_4, SBOX_DCHERRMSK_4},
	{SBOX_DCAR_5, SBOX_DHPR_5, SBOX_DTPR_5, SBOX_DAUX_HI_5, SBOX_DAUX_LO_5, SBOX_DRAR_HI_5,
	 SBOX_DRAR_LO_5, SBOX_DITR_5, SBOX_DSTAT_5,
	 SBOX_DSTATWB_LO_5, SBOX_DSTATWB_HI_5, SBOX_DCHERR_5, SBOX_DCHERRMSK_5},
	{SBOX_DCAR_6, SBOX_DHPR_6, SBOX_DTPR_6, SBOX_DAUX_HI_6, SBOX_DAUX_LO_6, SBOX_DRAR_HI_6,
	 SBOX_DRAR_LO_6, SBOX_DITR_6, SBOX_DSTAT_6,
	 SBOX_DSTATWB_LO_6, SBOX_DSTATWB_HI_6, SBOX_DCHERR_6, SBOX_DCHERRMSK_6},
	{SBOX_DCAR_7, SBOX_DHPR_7, SBOX_DTPR_7, SBOX_DAUX_HI_7, SBOX_DAUX_LO_7, SBOX_DRAR_HI_7,
	 SBOX_DRAR_LO_7, SBOX_DITR_7, SBOX_DSTAT_7,
	 SBOX_DSTATWB_LO_7, SBOX_DSTATWB_HI_7, SBOX_DCHERR_7, SBOX_DCHERRMSK_7}
};

static __always_inline uint32_t
md_mic_dma_read_mmio(struct mic_dma_device *dma_dev,
		int chan, enum md_mic_dma_chan_reg reg)
{
	return mic_sbox_read_mmio(dma_dev->mm_sbox, mic_dma_reg[chan][reg]);
}

static __always_inline void
md_mic_dma_write_mmio(struct mic_dma_device *dma_dev, int chan,
		enum md_mic_dma_chan_reg reg, uint32_t value)
{
	mic_sbox_write_mmio(dma_dev->mm_sbox, mic_dma_reg[chan][reg], value);
}

#ifdef DEBUG
#ifndef KASSERT
#define KASSERT(x, y, ...)		\
	do {				\
		if(!x)			\
			printk(y, ##__VA_ARGS__);\
		BUG_ON(!x);		\
	} while(0)
#endif
#define CHECK_CHAN(chan)							\
	do {									\
		KASSERT((chan), "NULL DMA channel\n");				\
		KASSERT((DMA_CHAN_COOKIE == chan->cookie),			\
			"Bad DMA channel cookie 0x%x\n", chan->cookie);		\
		KASSERT(atomic_read(&(chan->in_use)), "DMA Channel not in use\n");	\
	} while(0)
#else // DEBUG
#ifndef KASSERT
#define KASSERT(x, y, ...)		\
	do {				\
		if(!x)			\
			printk(y, ##__VA_ARGS__);\
		BUG_ON(!x);		\
	} while(0)
#endif
#define CHECK_CHAN(chan)

#endif // DEBUG

struct mic_dma_ctx_t;
void md_mic_dma_chan_set_dstat_wb(struct mic_dma_device *dma_dev,
					struct md_mic_dma_chan *chan);

void md_mic_dma_chan_set_dcherr_msk(struct mic_dma_device *dma_dev,
				struct md_mic_dma_chan *chan, uint32_t mask);

static __always_inline void
md_mic_dma_chan_write_head(struct mic_dma_device *dma_dev,
				struct md_mic_dma_chan *chan, uint32_t head)
{
	uint32_t chan_num;
	CHECK_CHAN(chan);
	chan_num = chan->ch_num;
	KASSERT((head < chan->num_desc_in_ring),
		"head 0x%x > num_desc_in_ring 0x%x chan_num %d\n",
		head, chan->num_desc_in_ring, chan_num);
	md_mic_dma_write_mmio(dma_dev, chan_num, REG_DHPR, head);
}

uint32_t md_mic_dma_chan_read_head(struct mic_dma_device *dma_dev, struct md_mic_dma_chan *chan);
uint32_t md_mic_dma_chan_read_tail(struct mic_dma_device *dma_dev, struct md_mic_dma_chan *chan);

#define TAIL_PTR_READ_RETRIES   500000
#define HW_CMP_CNT_MASK			0x1ffff
static __always_inline uint32_t
md_avail_desc_ring_space(struct mic_dma_device *dma_dev, bool is_astep,
				  struct md_mic_dma_chan *chan, uint32_t head, uint32_t required)
{
	uint32_t count = 0, max_num_retries = TAIL_PTR_READ_RETRIES, num_retries = 0;
	uint32_t tail = chan->cached_tail;
retry:
	if (head > tail)
		count = (tail - 0) + (chan->num_desc_in_ring - head);
	else if (tail > head)
		count = tail - head;
	else
		return (chan->num_desc_in_ring - 1);

	if (count > required) {
		return count - 1;
	} else {
		if (is_astep)
			tail = md_mic_dma_chan_read_tail(dma_dev, chan);
		else
			tail = HW_CMP_CNT_MASK & md_mic_dma_read_mmio(dma_dev, chan->ch_num, REG_DSTAT);
	}
	chan->cached_tail = tail;
	num_retries++;
	if (num_retries == max_num_retries)
		return 0;
	cpu_relax();
	goto retry;
}

bool md_mic_dma_chan_intr_pending(struct mic_dma_device *dma_dev, struct md_mic_dma_chan *chan);
phys_addr_t md_mic_dma_chan_get_desc_ring_phys(struct mic_dma_device *dma_dev,
			struct md_mic_dma_chan *chan);
phys_addr_t md_mic_dma_chan_get_dstatwb_phys(struct mic_dma_device *dma_dev,
			struct md_mic_dma_chan *chan);
inline uint32_t md_mic_dma_read_mmio(struct mic_dma_device *dma_dev, 
					    int chan, enum md_mic_dma_chan_reg reg);

/* Descriptor programming helpers */
void md_mic_dma_prep_nop_desc(union md_mic_dma_desc *desc);

/**
 * md_mic_dma_memcpy_desc - Prepares a memory copy descriptor
 * @src_phys: Source Physical Address must be cache line aligned
 * @dst_phys: Destination physical address must be cache line aligned
 * @size: Size of the transfer should not be 0 and must be a multiple
 *        of cache line size
 */
static __always_inline void
md_mic_dma_memcpy_desc(union md_mic_dma_desc *desc,
		uint64_t src_phys,
		uint64_t dst_phys,
		uint64_t size)
{
	KASSERT((desc != 0), ("NULL desc"));
	KASSERT((ALIGN(src_phys - (L1_CACHE_BYTES - 1), L1_CACHE_BYTES) == src_phys),
			"src not cache line aligned 0x%llx\n", (unsigned long long)src_phys);
	KASSERT((ALIGN(dst_phys - (L1_CACHE_BYTES - 1), L1_CACHE_BYTES) == dst_phys),
			"dst not cache line aligned 0x%llx\n", (unsigned long long)dst_phys);
	KASSERT(((size != 0) && (size <= MIC_MAX_DMA_XFER_SIZE) &&
				(ALIGN(size - (L1_CACHE_BYTES - 1), L1_CACHE_BYTES) == size)),
			"size > MAX_DMA_XFER_SIZE size 0x%llx", (unsigned long long)size);

	desc->qwords.qw0 = 0;
	desc->qwords.qw1 = 0;
	desc->desc.memcopy.type = 1;
	desc->desc.memcopy.sap = src_phys;
	desc->desc.memcopy.dap = dst_phys;
	desc->desc.memcopy.length = (size >> L1_CACHE_SHIFT);
}

/**
 * md_mic_dma_prep_status_desc - Prepares a status descriptor
 * @data - Value to be updated by the DMA engine @ dst_phys
 * @dst_phys: Destination physical address
 * @generate_intr: Interrupt must be generated when the DMA HW
 *                 completes processing this descriptor
 */
static __always_inline void
md_mic_dma_prep_status_desc(union md_mic_dma_desc *desc, uint64_t data,
				 uint64_t dst_phys, bool generate_intr)
{
	KASSERT((desc != 0), ("NULL desc"));

	desc->qwords.qw0 = 0;
	desc->qwords.qw1 = 0;
	desc->desc.memcopy.type = 2;
	desc->desc.status.data = data;
	desc->desc.status.dap = dst_phys;
	if (generate_intr)
		desc->desc.status.intr = 1;
}

/**
 * md_mic_dma_prep_gp_desc - Prepares a general purpose descriptor
 * @data - Value to be updated by the DMA engine @ dst_phys
 * @dst_phys: Destination physical address
 */
static __always_inline void
md_mic_dma_prep_gp_desc(union md_mic_dma_desc *desc, uint32_t data, uint64_t dst_phys)
{
	KASSERT((desc != 0), ("NULL desc"));

	desc->qwords.qw0 = 0;
	desc->qwords.qw1 = 0;
	desc->desc.general.type = 3;
	desc->desc.general.data = data;
	desc->desc.general.dap = dst_phys;
}
/* Debug functions */
void md_mic_dma_print_debug(struct mic_dma_device *dma_dev, struct md_mic_dma_chan *chan);
#endif
