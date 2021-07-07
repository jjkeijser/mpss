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

#ifndef MICSCIF_RMA_H
#define MICSCIF_RMA_H

#ifdef CONFIG_MMU_NOTIFIER
#include <linux/mmu_notifier.h>
#ifdef CONFIG_TRANSPARENT_HUGEPAGE
#include <linux/huge_mm.h>
#endif
#ifdef CONFIG_HUGETLB_PAGE
#include <linux/hugetlb.h>
#endif
#endif
#include "scif.h"
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
#include "mic/micscif_kmem_cache.h"

struct rma_mmu_notifier {
#ifdef CONFIG_MMU_NOTIFIER
	struct mmu_notifier	ep_mmu_notifier;
#endif
	bool			ep_mn_registered;
	/* List of temp registration windows for self */
	struct list_head	tc_reg_list;
	struct mm_struct	*mm;
	struct endpt		*ep;
	struct list_head	list_member;
};

/* Per Endpoint Remote Memory Access Information */
struct endpt_rma_info {
	/* List of registration windows for self */
	struct list_head	reg_list;
	/* List of registration windows for peer */
	struct list_head	remote_reg_list;
	/* Offset generator */
	struct va_gen_addr	va_gen;
	/*
	 * Synchronizes access to self/remote list and also
	 * protects the window from being destroyed while
	 * RMAs are in progress.
	 */
	struct mutex		rma_lock;
	/*
	 * Synchronizes access to temporary cached windows list
	 * for SCIF Registration Caching.
	 */
	spinlock_t		tc_lock;
	/*
	 * Synchronizes access to the list of MMU notifiers
	 * registered for this SCIF endpoint.
	 */
	struct mutex		mmn_lock;
	/*
	 * Synchronizes access to the SCIF registered address space
	 * offset generator.
	 */
	struct mutex		va_lock;
	/*
	 * Keeps track of number of outstanding temporary registered
	 * windows created by scif_vreadfrom/scif_vwriteto which have
	 * not been destroyed. tcw refers to the number of temporary
	 * cached windows and total number of pages pinned.
	 */
	atomic_t		tw_refcount;
	atomic_t		tw_total_pages;
	atomic_t		tcw_refcount;
	atomic_t		tcw_total_pages;
	/*
	 * MMU notifier so that we can destroy the windows when there is
	 * a change
	 */
	struct list_head		mmn_list;
	/*
	 * Keeps track of number of outstanding remote fence requests
	 * which have been received by the peer.
	 */
	int			fence_refcount;
	/*
	 * The close routine blocks on this wait queue to ensure that all
	 * remote fence requests have been serviced.
	 */
	wait_queue_head_t	fence_wq;
	/*
	 * DMA channel used for all DMA transfers for this endpoint.
	 */
	struct dma_channel	*dma_chan;
	/* Detect asynchronous list entry deletion */
	int			async_list_del;
#ifdef _MIC_SCIF_
	/* Local P2P proxy DMA virtual address for SUD updates by peer */
	void			*proxy_dma_va;
	/* Local P2P proxy DMA physical address location for SUD updates */
	dma_addr_t	proxy_dma_phys;
	/* Remote P2P proxy DMA physical address location for SUD updates */
	dma_addr_t	proxy_dma_peer_phys;
#endif
	/* List of tasks which have remote memory mappings */
	struct list_head	task_list;
};

/* Information used for tracking remote fence requests */
struct fence_info {
	/* State of this transfer */
	enum micscif_msg_state	state;

	/* Fences wait on this queue */
	wait_queue_head_t	wq;

	/* Used for storing the DMA mark */
	int			dma_mark;
};

/* Per remote fence wait request */
struct remote_fence_info {
	/* The SCIF_WAIT message */
	struct nodemsg msg;

	struct list_head list_member;
};

/* Self or Peer window */
enum rma_window_type {
	RMA_WINDOW_SELF = 0x1,
	RMA_WINDOW_PEER
};

/* The number of physical addresses that can be stored in a PAGE. */
#define NR_PHYS_ADDR_IN_PAGE	(PAGE_SIZE >> 3)

/*
 * Store an array of lookup offsets. Each offset in this array maps
 * one 4K page containing 512 physical addresses i.e. 2MB. 512 such
 * offsets in a 4K page will correspond to 1GB of registered address space.
 */
struct rma_lookup {
	/* Array of offsets */
	dma_addr_t	*lookup;
	/* Offset used to map lookup array */
	dma_addr_t	offset;
};


/*
 * A set of pinned pages obtained with scif_pin_pages() which could be part
 * of multiple registered windows across different end points.
 */
struct scif_pinned_pages {
	int64_t			nr_pages;
	int			prot;
	int			map_flags;
	atomic_t		ref_count;
	uint64_t		magic;
	/*
	 * Array of pointers to struct pages populated
	 * with get_user_pages(..)
	 */
	struct page		**pages;
	int			*num_pages;
	int64_t			nr_contig_chunks;
	/* Only for Hosts without THP but with Huge TLB FS Like SuSe11 SP1 */
	struct vm_area_struct         **vma;
};

/*
 * Information about a particular task which has remote memory mappings
 * created via scif_mmap(..).
 */
struct rma_task_info {
	/*
	 * Stores the pid struct of the grp_leader task structure which
	 * scif_mmap(..)'d the remote window.
	 */
	struct pid		*pid;
	int			ref_count;
	struct list_head       list_member;
};

/* Registration Window for Self */
struct reg_range_t {
	int64_t			nr_pages;
	/* Number of contiguous physical chunks */
	int64_t			nr_contig_chunks;
	int			prot;
	int			ref_count;
	/* Cookie to detect corruption */
	uint64_t		magic;
	uint64_t		offset;
	/* va address that this window represents
	 * Useful for only for temp windows*/
	void			*va_for_temp;
	/* Used for temporary windows*/
	int			dma_mark;
	/*
	 * Pointer to EP. Useful for passing EP around
	 * with messages to avoid expensive list
	 * traversals.
	 */
	uint64_t		ep;

	struct list_head	list_member;

	enum rma_window_type	type;

	/*
	 * Pointer to peer window. Useful for sending
	 * messages to peer without requiring an
	 * extra list traversal
	 */
	uint64_t		peer_window;

	/* Unregistration state */
	enum micscif_msg_state	unreg_state;

	/*
	 * True for temporary windows created via
	 * scif_vreadfrom/scif_vwriteto.
	 */
	bool			temp;

	bool			offset_freed;

	/* Local P2P proxy DMA physical address location for SUD updates */
	dma_addr_t	proxy_dma_phys;

	union {
		/* Self RAS */
		struct {
			/* The set of pinned_pages backing this window */
			struct scif_pinned_pages *pinned_pages;

			/* Handle for sending ALLOC_REQ */
			struct allocmsg		alloc_handle;

			/* Wait Queue for an registration (N)ACK */
			wait_queue_head_t	regwq;

			/* Registration state */
			enum micscif_msg_state	reg_state;

			/* Wait Queue for an unregistration (N)ACK */
			wait_queue_head_t	unregwq;
		};
		/* Peer RAS specific window elements */
		struct {
#ifdef CONFIG_ML1OM
			/* Lookup for physical addresses used for mmap */
			struct rma_lookup	phys_addr_lookup;

			/* Lookup for temp physical addresses used for mmap */
			struct rma_lookup	temp_phys_addr_lookup;

			/* Mmap state */
			enum micscif_msg_state	gttmap_state;

			/* Wait Queue for an unregistration (N)ACK */
			wait_queue_head_t	gttmapwq;

			/* Ref count per page */
			int			*page_ref_count;
#endif
			/* Lookup for physical addresses used for DMA */
			struct rma_lookup	dma_addr_lookup;

			/* Number of entries in lookup */
			int			nr_lookup;

			/* Offset used to map the window by the peer */
			dma_addr_t	mapped_offset;

			/* Ref count for tracking scif_get_pages */
			int			get_put_ref_count;
		};
	};
#ifdef CONFIG_ML1OM
	/* Array of physical addresses used for creating VtoP mappings */
	/* FIXME: these are phys_addr as seen by the peer node, node at the
	 * opposite end of the endpt
	 */
	dma_addr_t		*phys_addr;

	/* Temporary array for storing physical addresses for performance */
	dma_addr_t		*temp_phys_addr;
#endif

	/* Array of physical addresses used for Host & MIC initiated DMA */
	dma_addr_t		*dma_addr;

	/* Array specifying number of pages for each physical address */
	int				*num_pages;
	struct mm_struct		*mm;
} __attribute__ ((packed));


#define RMA_MAGIC(x) BUG_ON(x->magic != SCIFEP_MAGIC)

/* If this bit is set then the mark is a remote fence mark */
#define SCIF_REMOTE_FENCE_BIT		30
/* Magic value used to indicate a remote fence request */
#define SCIF_REMOTE_FENCE (1ULL << SCIF_REMOTE_FENCE_BIT)

enum rma_direction {
	LOCAL_TO_REMOTE,
	REMOTE_TO_LOCAL
};

/* Initialize RMA for this EP */
int micscif_rma_ep_init(struct endpt *ep);

/* Check if epd can be uninitialized */
int micscif_rma_ep_can_uninit(struct endpt *ep);

/* Obtain a new offset. Callee must grab RMA lock */
int micscif_get_window_offset(struct endpt *ep, int flags,
			      uint64_t offset, size_t len, uint64_t *out_offset);

/* Free offset. Callee must grab RMA lock */
void micscif_free_window_offset(struct endpt *ep,
				uint64_t offset, size_t len);

/* Create self registration window */
struct reg_range_t *micscif_create_window(struct endpt *ep,
	int64_t nr_pages, uint64_t offset, bool temp);

/* Create a set of pinned pages */
struct scif_pinned_pages *micscif_create_pinned_pages(int nr_pages, int prot);

/* Destroy a set of pinned pages */
int micscif_destroy_pinned_pages(struct scif_pinned_pages *pages);

/* Destroy self registration window.*/
int micscif_destroy_window(struct endpt *ep, struct reg_range_t *window);

int micscif_destroy_incomplete_window(struct endpt *ep, struct reg_range_t *window);

/* Map pages of self window to Aperture/PCI */
int micscif_map_window_pages(struct endpt *ep, struct reg_range_t *window, bool temp);

/* Unregister a self window */
int micscif_unregister_window(struct reg_range_t *window);

/* Create remote registration window */
struct reg_range_t *micscif_create_remote_window(struct endpt *ep, int nr_pages);

/* Destroy remote registration window */
void micscif_destroy_remote_window(struct endpt *ep, struct reg_range_t *window);

int micscif_send_alloc_request(struct endpt *ep, struct reg_range_t *window);

/* Prepare a remote registration window */
int micscif_prep_remote_window(struct endpt *ep, struct reg_range_t *window);

/* Create remote lookup entries for physical addresses */
int micscif_create_remote_lookup(struct endpt *ep, struct reg_range_t *window);

/* Destroy remote lookup entries for physical addresses */
void micscif_destroy_remote_lookup(struct endpt *ep, struct reg_range_t *window);

/* Send a SCIF_REGISTER message and wait for an ACK */
int micscif_send_scif_register(struct endpt *ep, struct reg_range_t *window);

/* Send a SCIF_UNREGISTER message */
int micscif_send_scif_unregister(struct endpt *ep, struct reg_range_t *window);

/* RMA copy API */
int micscif_rma_copy(scif_epd_t epd, off_t loffset, void *addr, size_t len,
	off_t roffset, int flags, enum rma_direction dir, bool last_chunk);

/* Sends a remote fence mark request */
int micscif_send_fence_mark(scif_epd_t epd, int *out_mark);

/* Sends a remote fence wait request */
int micscif_send_fence_wait(scif_epd_t epd, int mark);

/* Sends a remote fence signal request */
int micscif_send_fence_signal(scif_epd_t epd, off_t roff, uint64_t rval,
		off_t loff, uint64_t lval, int flags);

/* Setup a DMA mark for an endpoint */
int micscif_fence_mark(scif_epd_t epd);

void ep_unregister_mmu_notifier(struct endpt *ep);
#ifdef CONFIG_MMU_NOTIFIER
void micscif_mmu_notif_handler(struct work_struct *work);
#endif

void micscif_rma_destroy_temp_windows(void);
void micscif_rma_destroy_tcw_ep(struct endpt *ep);
void micscif_rma_destroy_tcw_invalid(struct list_head *list);

void micscif_rma_handle_remote_fences(void);

/* Reserve a DMA channel for a particular endpoint */
int micscif_reserve_dma_chan(struct endpt *ep);

/* Program DMA SUD's after verifying the registered offset */
int micscif_prog_signal(scif_epd_t epd, off_t offset, uint64_t val,
                enum rma_window_type type);

/* Kill any applications which have valid remote memory mappings */
void micscif_kill_apps_with_mmaps(int node);

/* Query if any applications have remote memory mappings */
bool micscif_rma_do_apps_have_mmaps(int node);

/* Get a reference to the current task which is creating a remote memory mapping */
int micscif_rma_get_task(struct endpt *ep, int nr_pages);

/* Release a reference to the current task which is destroying a remote memory mapping */
void micscif_rma_put_task(struct endpt *ep, int nr_pages);

/* Cleanup remote registration lists for zombie endpoints */
void micscif_cleanup_rma_for_zombies(int node);

#ifdef _MIC_SCIF_
void micscif_teardown_proxy_dma(struct endpt *ep);
#endif

static __always_inline
bool is_unaligned(off_t src_offset, off_t dst_offset)
{
        src_offset = src_offset & (L1_CACHE_BYTES - 1);
        dst_offset = dst_offset & (L1_CACHE_BYTES - 1);
        if (src_offset == dst_offset)
                return false;
        else
                return true;
}

static __always_inline
int __scif_readfrom(scif_epd_t epd, off_t loffset, size_t len,
		off_t roffset, int flags)
{
	int err;

	pr_debug("SCIFAPI readfrom: ep %p loffset 0x%lx len 0x%lx"
		" offset 0x%lx flags 0x%x\n", 
		epd, loffset, len, roffset, flags);

	if (is_unaligned(loffset, roffset)) {
		while(len > MAX_UNALIGNED_BUF_SIZE) {
			err = micscif_rma_copy(epd, loffset, NULL,
				MAX_UNALIGNED_BUF_SIZE,
				roffset, flags, REMOTE_TO_LOCAL, false);
			if (err)
				goto readfrom_err;
			loffset += MAX_UNALIGNED_BUF_SIZE;
			roffset += MAX_UNALIGNED_BUF_SIZE;
			len -=MAX_UNALIGNED_BUF_SIZE;
		}
	}
	err = micscif_rma_copy(epd, loffset, NULL, len,
		roffset, flags, REMOTE_TO_LOCAL, true);
readfrom_err:
	return err;
}

static __always_inline
int __scif_writeto(scif_epd_t epd, off_t loffset, size_t len,
				off_t roffset, int flags)
{
	int err;

	pr_debug("SCIFAPI writeto: ep %p loffset 0x%lx len 0x%lx"
		" roffset 0x%lx flags 0x%x\n", 
		epd, loffset, len, roffset, flags);

	if (is_unaligned(loffset, roffset)) {
		while(len > MAX_UNALIGNED_BUF_SIZE) {
			err = micscif_rma_copy(epd, loffset, NULL,
				MAX_UNALIGNED_BUF_SIZE,
				roffset, flags, LOCAL_TO_REMOTE, false);
			if (err)
				goto writeto_err;
			loffset += MAX_UNALIGNED_BUF_SIZE;
			roffset += MAX_UNALIGNED_BUF_SIZE;
			len -= MAX_UNALIGNED_BUF_SIZE;
		}
	}
	err = micscif_rma_copy(epd, loffset, NULL, len,
		roffset, flags, LOCAL_TO_REMOTE, true);
writeto_err:
	return err;
}

static __always_inline
int __scif_vreadfrom(scif_epd_t epd, void *addr, size_t len, off_t roffset, int flags)
{
	int err;

	pr_debug("SCIFAPI vreadfrom: ep %p addr %p len 0x%lx"
		" roffset 0x%lx flags 0x%x\n", 
		epd, addr, len, roffset, flags);

	if (is_unaligned((off_t)addr, roffset)) {
		if (len > MAX_UNALIGNED_BUF_SIZE)
			flags &= ~SCIF_RMA_USECACHE;

		while(len > MAX_UNALIGNED_BUF_SIZE) {
			err = micscif_rma_copy(epd, 0, addr,
				MAX_UNALIGNED_BUF_SIZE,
				roffset, flags, REMOTE_TO_LOCAL, false);
			if (err)
				goto vreadfrom_err;
			addr = (void *)((uint64_t)addr + MAX_UNALIGNED_BUF_SIZE);
			roffset += MAX_UNALIGNED_BUF_SIZE;
			len -= MAX_UNALIGNED_BUF_SIZE;
		}
	}
	err = micscif_rma_copy(epd, 0, addr, len,
		roffset, flags, REMOTE_TO_LOCAL, true);
vreadfrom_err:
	return err;
}

static __always_inline
int __scif_vwriteto(scif_epd_t epd, void *addr, size_t len, off_t roffset, int flags)
{
	int err;

	pr_debug("SCIFAPI vwriteto: ep %p addr %p len 0x%lx"
		" roffset 0x%lx flags 0x%x\n", 
		epd, addr, len, roffset, flags);

	if (is_unaligned((off_t)addr, roffset)) {
		if (len > MAX_UNALIGNED_BUF_SIZE)
			flags &= ~SCIF_RMA_USECACHE;

		while(len > MAX_UNALIGNED_BUF_SIZE) {
			err = micscif_rma_copy(epd, 0, addr,
				MAX_UNALIGNED_BUF_SIZE,
				roffset, flags, LOCAL_TO_REMOTE, false);
			if (err)
				goto vwriteto_err;
			addr = (void *)((uint64_t)addr + MAX_UNALIGNED_BUF_SIZE);
			roffset += MAX_UNALIGNED_BUF_SIZE;
			len -= MAX_UNALIGNED_BUF_SIZE;
		}
	}
	err = micscif_rma_copy(epd, 0, addr, len,
		roffset, flags, LOCAL_TO_REMOTE, true);
vwriteto_err:
	return err;
}

void micscif_rma_completion_cb(uint64_t data);

int micscif_pci_dev(uint16_t node, struct pci_dev **pdev);
#ifndef _MIC_SCIF_
int micscif_pci_info(uint16_t node, struct scif_pci_info *dev);
#endif

/*
 * nr_pages in a 2MB page is specified via the top 12 bits in the
 * physical address.
 */

/* Check all parenthesis in these macros. See if putting in bottom makes sense? */
#define RMA_HUGE_NR_PAGE_SHIFT ((52))
#define RMA_HUGE_NR_PAGE_MASK (((0xFFFULL) << RMA_HUGE_NR_PAGE_SHIFT))
#define RMA_GET_NR_PAGES(addr) ((addr) >> RMA_HUGE_NR_PAGE_SHIFT)
#define RMA_SET_NR_PAGES(addr, nr_pages) ((addr) = (((nr_pages) & 0xFFFULL) << RMA_HUGE_NR_PAGE_SHIFT) | ((uint64_t)(addr)))
#define RMA_GET_ADDR(addr) ((addr) & ~(RMA_HUGE_NR_PAGE_MASK))

extern bool mic_huge_page_enable;

#define SCIF_HUGE_PAGE_SHIFT	21

/*
 * micscif_is_huge_page:
 * @page: A physical page.
 */
static __always_inline int
micscif_is_huge_page(struct scif_pinned_pages *pinned_pages, int index)
{
	int huge = 0;
	struct page *page = pinned_pages->pages[index];

	if (compound_order(page) + PAGE_SHIFT == SCIF_HUGE_PAGE_SHIFT)
		huge = 1;
	if (huge)
		ms_info.nr_2mb_pages++;
	if (!mic_huge_page_enable)
		huge = 0;
#ifdef RMA_DEBUG
	WARN_ON(!page_count(page));
	WARN_ON(page_mapcount(page) < 0);
#endif
	return huge;
}

/*
 * micscif_detect_large_page:
 * @pinned_pages: A set of pinned pages.
 */
static __always_inline int
micscif_detect_large_page(struct scif_pinned_pages *pinned_pages, char *addr)
{
	int i = 0, nr_pages, huge;
	char *next_huge, *end;
	char *end_addr = addr + (pinned_pages->nr_pages << PAGE_SHIFT);

	while (addr < end_addr) {
		huge = micscif_is_huge_page(pinned_pages, i);
		if (huge) {
			next_huge = (char *)ALIGN(
				(unsigned long)(addr + 1), 
				PMD_SIZE);
			end = next_huge > end_addr ? end_addr : next_huge;
			nr_pages = (int)((end - addr) >> PAGE_SHIFT);
			pinned_pages->num_pages[i] = (int)nr_pages;   
			addr = end;
			i += (int)nr_pages;   
						
		} else {
			pinned_pages->num_pages[i] = 1;
			i++;
			addr += PAGE_SIZE;
			ms_info.nr_4k_pages++;
		}
		pinned_pages->nr_contig_chunks++;
	}
	return 0;
}

/**
 * micscif_set_nr_pages:
 * @ep: end point
 * @window: self registration window
 *
 * Set nr_pages in every entry of physical address/dma address array
 * and also remove nr_pages information from physical addresses.
 */
static __always_inline void
micscif_set_nr_pages(struct micscif_dev *dev, struct reg_range_t *window)
{
	int j;
#ifdef CONFIG_ML1OM
	int l = 0, k;
#endif

	for (j = 0; j < window->nr_contig_chunks; j++) {
		window->num_pages[j] = RMA_GET_NR_PAGES(window->dma_addr[j]);
		if (window->num_pages[j])
			window->dma_addr[j] = RMA_GET_ADDR(window->dma_addr[j]);
		else
			break;
#ifdef CONFIG_ML1OM
		for (k = 0; k < window->num_pages[j]; k++)
			if (window->temp_phys_addr[j])
				window->phys_addr[l + k] =
					RMA_GET_ADDR(window->temp_phys_addr[j]) + (k << PAGE_SHIFT);
		l += window->num_pages[j];
#endif
	}
}

#ifdef CONFIG_ML1OM
/*
 * micscif_get_phys_addr:
 * Obtain the phys_addr given the window and the offset.
 * @window: Registered window.
 * @off: Window offset.
 */
static __always_inline dma_addr_t
micscif_get_phys_addr(struct reg_range_t *window, uint64_t off)
{
	int page_nr = (off - window->offset) >> PAGE_SHIFT;
	off_t page_off = off & ~PAGE_MASK;
	return window->phys_addr[page_nr] | page_off;
}
#endif

#define RMA_ERROR_CODE (~(dma_addr_t)0x0)

/*
 * micscif_get_dma_addr:
 * Obtain the dma_addr given the window and the offset.
 * @window: Registered window.
 * @off: Window offset.
 * @nr_bytes: Return the number of contiguous bytes till next DMA addr index.
 * @index: Return the index of the dma_addr array found.
 * @start_off: start offset of index of the dma addr array found.
 * The nr_bytes provides the callee an estimate of the maximum possible
 * DMA xfer possible while the index/start_off provide faster lookups
 * for the next iteration.
 */
static __always_inline dma_addr_t
micscif_get_dma_addr(struct reg_range_t *window, uint64_t off, size_t *nr_bytes, int *index, uint64_t *start_off)
{
	if (window->nr_pages == window->nr_contig_chunks) {
		int page_nr = (int)((off - window->offset) >> PAGE_SHIFT);
		off_t page_off = off & ~PAGE_MASK;
		if (nr_bytes)
			*nr_bytes = PAGE_SIZE - page_off;
        if (page_nr >= window->nr_pages) {
            printk(KERN_ERR "%s dma_addr out of boundary\n", __FUNCTION__);
        }
		return window->dma_addr[page_nr] | page_off;
	} else {
		int i = index ? *index : 0;
		uint64_t end;
		uint64_t start = start_off ? *start_off : window->offset;
		for (; i < window->nr_contig_chunks; i++) {
			end = start + (window->num_pages[i] << PAGE_SHIFT);
			if (off >= start && off < end) {
				if (index)
					*index = i;
				if (start_off)
					*start_off = start;
				if (nr_bytes)
					*nr_bytes = end - off;
				return (window->dma_addr[i] + (off - start));
			}
			start += (window->num_pages[i] << PAGE_SHIFT);
		}
	}
#ifdef CONFIG_MK1OM
	printk(KERN_ERR "%s %d BUG. Addr not found? window %p off 0x%llx\n", __func__, __LINE__, window, off);
	BUG_ON(1);
#endif
	return RMA_ERROR_CODE;
}

/*
 * scif_memset:
 * @va: kernel virtual address
 * @c: The byte used to fill the memory
 * @size: Buffer size
 *
 * Helper API which fills size bytes of memory pointed to by va with the
 * constant byte c. This API fills the memory in chunks of 4GB - 1 bytes
 * for a single invocation of memset(..) to work around a kernel bug in
 * x86_64 @ https://bugzilla.kernel.org/show_bug.cgi?id=27732
 * where memset(..) does not do "ANY" work for size >= 4GB.
 * This kernel bug has been fixed upstream in v3.2 via the commit
 * titled "x86-64: Fix memset() to support sizes of 4Gb and above"
 * but has not been backported to distributions like RHEL 6.3 yet.
 */
static __always_inline void scif_memset(char *va, int c, size_t size)
{
	size_t loop_size;
	const size_t four_gb = 4 * 1024 * 1024 * 1024ULL;

	while (size) {
		loop_size = min(size, four_gb - 1);
		memset(va, c, loop_size);
		size -= loop_size;
		va += loop_size;
	}
}

/*
 * scif_zalloc:
 * @size: Size of the allocation request.
 *
 * Helper API which attempts to allocate zeroed pages via
 * __get_free_pages(..) first and then falls back on
 * vmalloc(..) if that fails. This is required because
 * vmalloc(..) is *slow*.
 */
static __always_inline void *scif_zalloc(size_t size)
{
	void *ret;
	size_t align = ALIGN(size, PAGE_SIZE);

	if (!align)
		return NULL;

	if (align <= (1 << (MAX_ORDER + PAGE_SHIFT - 1)))
		if ((ret = (void*)__get_free_pages(GFP_KERNEL | __GFP_ZERO,
						get_order(align))))
			goto done;
	if (!(ret = vmalloc(align)))
		return NULL;

	/* TODO: Use vzalloc once kernel supports it */
	scif_memset(ret, 0, size);
done:
#ifdef RMA_DEBUG
	atomic_long_add_return(align, &ms_info.rma_alloc_cnt);
#endif
	return ret;
}

/*
 * scif_free:
 * @addr: Address to be freed.
 * @size: Size of the allocation.
 * Helper API which frees memory allocated via scif_zalloc().
 */
static __always_inline void scif_free(void *addr, size_t size)
{
	size_t align = ALIGN(size, PAGE_SIZE);

	if (unlikely(is_vmalloc_addr(addr)))
		vfree(addr);
	else {
		free_pages((unsigned long)addr, get_order(align));
	}
#ifdef RMA_DEBUG
	WARN_ON(atomic_long_sub_return(align, &ms_info.rma_alloc_cnt) < 0);
#endif
}

static __always_inline void
get_window_ref_count(struct reg_range_t *window, int64_t nr_pages)
{
	window->ref_count += (int)nr_pages;
}

static __always_inline void
put_window_ref_count(struct reg_range_t *window, int64_t nr_pages)
{
	window->ref_count -= (int)nr_pages; 
	BUG_ON(window->nr_pages < 0);
}

static __always_inline void
set_window_ref_count(struct reg_range_t *window, int64_t nr_pages)
{
    window->ref_count = (int)nr_pages; 
}

/* Debug API's */
void micscif_display_window(struct reg_range_t *window, const char *s, int line);
static inline struct mm_struct *__scif_acquire_mm(void)
{
	if (mic_ulimit_check) {
#ifdef RMA_DEBUG
		atomic_long_add_return(1, &ms_info.rma_mm_cnt);
#endif
		return get_task_mm(current);
	}
	return NULL;
}

static inline void __scif_release_mm(struct mm_struct *mm)
{
	if (mic_ulimit_check && mm) {
#ifdef RMA_DEBUG
		WARN_ON(atomic_long_sub_return(1, &ms_info.rma_mm_cnt) < 0);
#endif
		mmput(mm);
	}
}

static inline int __scif_dec_pinned_vm_lock(struct mm_struct *mm,
					int64_t nr_pages, bool try_lock)
{
	if (mm && nr_pages && mic_ulimit_check) {
		if (try_lock) {
			if (!down_write_trylock(&mm->mmap_sem)) {
				return -1;
			}
		} else {
			down_write(&mm->mmap_sem);
		}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 1, 0))
		mm->pinned_vm -= nr_pages;
#else
		mm->locked_vm -= nr_pages;
#endif
		up_write(&mm->mmap_sem);
	}
	return 0;
}

static inline int __scif_check_inc_pinned_vm(struct mm_struct *mm,
					     int64_t nr_pages)
{
	if (mm && mic_ulimit_check && nr_pages) {
		unsigned long locked, lock_limit;
		locked = nr_pages;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 1, 0))
		locked += mm->pinned_vm;
#else
		locked += mm->locked_vm;
#endif
		lock_limit = rlimit(RLIMIT_MEMLOCK) >> PAGE_SHIFT;
		if ((locked > lock_limit) && !capable(CAP_IPC_LOCK)) {
			pr_debug("locked(%lu) > lock_limit(%lu)\n", 
				    locked, lock_limit);
			return -ENOMEM;
		} else {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 1, 0))
			mm->pinned_vm = locked;
#else
			mm->locked_vm = locked;
#endif
		}
	}
	return 0;
}
#endif
