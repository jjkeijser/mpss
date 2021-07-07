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

#include "mic/micscif.h"
#include "mic/micscif_smpt.h"
#include "mic/micscif_kmem_cache.h"
#include "mic/micscif_rma_list.h"
#ifndef _MIC_SCIF_
#include "mic_common.h"
#endif
#include "mic/mic_dma_api.h"
#include "mic/micscif_map.h"

bool mic_reg_cache_enable = 0;

bool mic_huge_page_enable = 1;

#ifdef _MIC_SCIF_
mic_dma_handle_t mic_dma_handle;
#endif
static inline
void micscif_rma_destroy_tcw(struct rma_mmu_notifier *mmn,
						struct endpt *ep, bool inrange,
						uint64_t start, uint64_t len);
#ifdef CONFIG_MMU_NOTIFIER
static void scif_mmu_notifier_release(struct mmu_notifier *mn,
			struct mm_struct *mm);
static void scif_mmu_notifier_invalidate_page(struct mmu_notifier *mn,
				struct mm_struct *mm,
				unsigned long address);
static void scif_mmu_notifier_invalidate_range_start(struct mmu_notifier *mn,
				       struct mm_struct *mm,
				       unsigned long start, unsigned long end);
static void scif_mmu_notifier_invalidate_range_end(struct mmu_notifier *mn,
				     struct mm_struct *mm,
				     unsigned long start, unsigned long end);
static const struct mmu_notifier_ops scif_mmu_notifier_ops = {
	.release = scif_mmu_notifier_release,
	.clear_flush_young = NULL,
	.change_pte = NULL,/*TODO*/
	.invalidate_page = scif_mmu_notifier_invalidate_page,
	.invalidate_range_start = scif_mmu_notifier_invalidate_range_start,
	.invalidate_range_end = scif_mmu_notifier_invalidate_range_end};

static void scif_mmu_notifier_release(struct mmu_notifier *mn,
			struct mm_struct *mm)
{
	struct endpt *ep;
	struct rma_mmu_notifier	*mmn;
	mmn = container_of(mn, struct rma_mmu_notifier, ep_mmu_notifier);
	ep = mmn->ep;
	micscif_rma_destroy_tcw(mmn, ep, false, 0, 0);
	pr_debug("%s\n", __func__);
	return;
}

static void scif_mmu_notifier_invalidate_page(struct mmu_notifier *mn,
				struct mm_struct *mm,
				unsigned long address)
{
	struct endpt *ep;
	struct rma_mmu_notifier	*mmn;
	mmn = container_of(mn, struct rma_mmu_notifier, ep_mmu_notifier);
	ep = mmn->ep;
	micscif_rma_destroy_tcw(mmn, ep, true, address, PAGE_SIZE);
	pr_debug("%s address 0x%lx\n", __func__, address);
	return;
}

static void scif_mmu_notifier_invalidate_range_start(struct mmu_notifier *mn,
				       struct mm_struct *mm,
				       unsigned long start, unsigned long end)
{
	struct endpt *ep;
	struct rma_mmu_notifier	*mmn;
	mmn = container_of(mn, struct rma_mmu_notifier, ep_mmu_notifier);
	ep = mmn->ep;
	micscif_rma_destroy_tcw(mmn, ep, true, (uint64_t)start, (uint64_t)(end - start));
	pr_debug("%s start=%lx, end=%lx\n", __func__, start, end);
	return;
}

static void scif_mmu_notifier_invalidate_range_end(struct mmu_notifier *mn,
				     struct mm_struct *mm,
				     unsigned long start, unsigned long end)
{
	/* Nothing to do here, everything needed was done in invalidate_range_start */
	pr_debug("%s\n", __func__);
	return;
}
#endif

#ifdef CONFIG_MMU_NOTIFIER
void ep_unregister_mmu_notifier(struct endpt *ep)
{
	struct endpt_rma_info *rma = &ep->rma_info;
	struct rma_mmu_notifier *mmn = NULL;
	struct list_head *item, *tmp;
	mutex_lock(&ep->rma_info.mmn_lock);
	list_for_each_safe(item, tmp, &rma->mmn_list) {
		mmn = list_entry(item, 
			struct rma_mmu_notifier, list_member);
		mmu_notifier_unregister(&mmn->ep_mmu_notifier, mmn->mm);
#ifdef RMA_DEBUG
		BUG_ON(atomic_long_sub_return(1, &ms_info.mmu_notif_cnt) < 0);
#endif
		list_del(item);
		kfree(mmn);
	}
	mutex_unlock(&ep->rma_info.mmn_lock);
}

static void init_mmu_notifier(struct rma_mmu_notifier *mmn, struct mm_struct *mm, struct endpt *ep)
{
	mmn->ep = ep;
	mmn->mm = mm;
	mmn->ep_mmu_notifier.ops = &scif_mmu_notifier_ops;
	INIT_LIST_HEAD(&mmn->list_member);
	INIT_LIST_HEAD(&mmn->tc_reg_list);
}

static struct rma_mmu_notifier *find_mmu_notifier(struct mm_struct *mm, struct endpt_rma_info *rma)
{
	struct rma_mmu_notifier *mmn;
	struct list_head *item;
	list_for_each(item, &rma->mmn_list) {
		mmn = list_entry(item, 
			struct rma_mmu_notifier, list_member);
		if (mmn->mm == mm)
			return mmn;
	}
	return NULL;
}
#endif

/**
 * micscif_rma_ep_init:
 * @ep: end point
 *
 * Initialize RMA per EP data structures.
 */
int micscif_rma_ep_init(struct endpt *ep)
{
	int ret;
	struct endpt_rma_info *rma = &ep->rma_info;

	mutex_init (&rma->rma_lock);
	if ((ret = va_gen_init(&rma->va_gen,
		VA_GEN_MIN, VA_GEN_RANGE)) < 0)
		goto init_err;
	spin_lock_init(&rma->tc_lock);
	mutex_init (&rma->mmn_lock);
	mutex_init (&rma->va_lock);
	INIT_LIST_HEAD(&rma->reg_list);
	INIT_LIST_HEAD(&rma->remote_reg_list);
	atomic_set(&rma->tw_refcount, 0);
	atomic_set(&rma->tw_total_pages, 0);
	atomic_set(&rma->tcw_refcount, 0);
	atomic_set(&rma->tcw_total_pages, 0);
	init_waitqueue_head(&rma->fence_wq);
	rma->fence_refcount = 0;
	rma->async_list_del = 0;
	rma->dma_chan = NULL;
	INIT_LIST_HEAD(&rma->mmn_list);
	INIT_LIST_HEAD(&rma->task_list);
init_err:
	return ret;
}

/**
 * micscif_rma_ep_can_uninit:
 * @ep: end point
 *
 * Returns 1 if an endpoint can be uninitialized and 0 otherwise.
 */
int micscif_rma_ep_can_uninit(struct endpt *ep)
{
	int ret = 0;

	/* Destroy RMA Info only if both lists are empty */
	if (list_empty(&ep->rma_info.reg_list) &&
		list_empty(&ep->rma_info.remote_reg_list) &&
#ifdef CONFIG_MMU_NOTIFIER
		list_empty(&ep->rma_info.mmn_list) &&
#endif
		!atomic_read(&ep->rma_info.tw_refcount) &&
		!atomic_read(&ep->rma_info.tcw_refcount))
		ret = 1;
	return ret;
}

#ifdef _MIC_SCIF_
/**
 * __micscif_setup_proxy_dma:
 * @ep: SCIF endpoint descriptor.
 *
 * Sets up data structures for P2P Proxy DMAs.
 */
static int __micscif_setup_proxy_dma(struct endpt *ep)
{
	struct endpt_rma_info *rma = &ep->rma_info;
	int err = 0;
	uint64_t *tmp = NULL;

	mutex_lock(&rma->rma_lock);
	if (is_p2p_scifdev(ep->remote_dev) && !rma->proxy_dma_va) {
		if (!(tmp = scif_zalloc(PAGE_SIZE))) {
			err = -ENOMEM;
			goto error;
		}
		if ((err = map_virt_into_aperture(&rma->proxy_dma_phys,
						tmp,
						ep->remote_dev, PAGE_SIZE))) {
			scif_free(tmp, PAGE_SIZE);
			goto error;
		}
		*tmp = OP_IDLE;
		rma->proxy_dma_va = tmp;
	}
error:
	mutex_unlock(&rma->rma_lock);
	return err;
}

static __always_inline int micscif_setup_proxy_dma(struct endpt *ep)
{
	if (ep->rma_info.proxy_dma_va)
		return 0;

	return __micscif_setup_proxy_dma(ep);
}

/**
 * micscif_teardown_proxy_dma:
 * @ep: SCIF endpoint descriptor.
 *
 * Tears down data structures setup for P2P Proxy DMAs.
 */
void micscif_teardown_proxy_dma(struct endpt *ep)
{
	struct endpt_rma_info *rma = &ep->rma_info;
	mutex_lock(&rma->rma_lock);
	if (rma->proxy_dma_va) {
		unmap_from_aperture(rma->proxy_dma_phys, ep->remote_dev, PAGE_SIZE);
		scif_free(rma->proxy_dma_va, PAGE_SIZE);
		rma->proxy_dma_va = NULL;
	}
	mutex_unlock(&rma->rma_lock);
}

/**
 * micscif_proxy_dma:
 * @ep: SCIF endpoint descriptor.
 * @copy_work: DMA copy work information.
 *
 * This API does the following:
 * 1) Sends the peer a SCIF Node QP message with the information
 * required to program a proxy DMA to covert a P2P Read to a Write
 * which will initiate a DMA transfer from the peer card to self.
 * The reason for this special code path is KNF and KNC P2P read
 * performance being much lower than P2P write performance on Crown
 * Pass platforms.
 * 2) Poll for an update of the known proxy dma VA to OP_COMPLETED
 * via a SUD by the peer.
 */
static int micscif_proxy_dma(scif_epd_t epd, struct mic_copy_work *work)
{
	struct endpt *ep = (struct endpt *)epd;
	struct nodemsg msg;
	unsigned long ts = jiffies;
	struct endpt_rma_info *rma = &ep->rma_info;
	int err;
	volatile uint64_t *proxy_dma_va = rma->proxy_dma_va;

	mutex_lock(&ep->rma_info.rma_lock);
	/*
	 * Bail out if there is a Proxy DMA already in progress
	 * for this endpoint. The callee will fallback on self
	 * DMAs upon an error.
	 */
	if (*proxy_dma_va != OP_IDLE) {
		mutex_unlock(&ep->rma_info.rma_lock);
		err = -EBUSY;
		goto error;
	}
	*proxy_dma_va = OP_IN_PROGRESS;
	mutex_unlock(&ep->rma_info.rma_lock);

	msg.src = ep->port;
	msg.uop = work->ordered ? SCIF_PROXY_ORDERED_DMA : SCIF_PROXY_DMA;
	msg.payload[0] = ep->remote_ep;
	msg.payload[1] = work->src_offset;
	msg.payload[2] = work->dst_offset;
	msg.payload[3] = work->len;

	if ((err = micscif_nodeqp_send(ep->remote_dev, &msg, ep)))
		goto error_init_va;

	while (*proxy_dma_va != OP_COMPLETED) {
		schedule();
		if (time_after(jiffies,
			ts + NODE_ALIVE_TIMEOUT)) {
			err = -EBUSY;
			goto error_init_va;
		}
	}
	err = 0;
error_init_va:
	*proxy_dma_va = OP_IDLE;
error:
	return err;
}
#endif

/**
 * micscif_create_pinned_pages:
 * @nr_pages: number of pages in window
 * @prot: read/write protection
 *
 * Allocate and prepare a set of pinned pages.
 */
struct scif_pinned_pages *micscif_create_pinned_pages(int nr_pages, int prot)
{
	struct scif_pinned_pages *pinned_pages;

	might_sleep();
	if (!(pinned_pages = scif_zalloc(sizeof(*pinned_pages))))
		goto error;

	if (!(pinned_pages->pages = scif_zalloc(nr_pages *
			sizeof(*(pinned_pages->pages)))))
		goto error_free_pinned_pages;

	if (!(pinned_pages->num_pages = scif_zalloc(nr_pages *
			sizeof(*(pinned_pages->num_pages)))))
		goto error_free_pages;

#if !defined(CONFIG_TRANSPARENT_HUGEPAGE) && defined(CONFIG_HUGETLB_PAGE) && !defined(_MIC_SCIF_)
	if (!(pinned_pages->vma = scif_zalloc(nr_pages *
			sizeof(*(pinned_pages->vma)))))
		goto error_free_num_pages;
#endif

	pinned_pages->prot = prot;
	pinned_pages->magic = SCIFEP_MAGIC;
	pinned_pages->nr_contig_chunks = 0;
	return pinned_pages;

#if !defined(CONFIG_TRANSPARENT_HUGEPAGE) && defined(CONFIG_HUGETLB_PAGE) && !defined(_MIC_SCIF_)
error_free_num_pages:
	scif_free(pinned_pages->num_pages,
		pinned_pages->nr_pages * sizeof(*(pinned_pages->num_pages)));
#endif
error_free_pages:
	scif_free(pinned_pages->pages,
		pinned_pages->nr_pages * sizeof(*(pinned_pages->pages)));
error_free_pinned_pages:
	scif_free(pinned_pages, sizeof(*pinned_pages));
error:
	return NULL;
}

/**
 * micscif_destroy_pinned_pages:
 * @pinned_pages: A set of pinned pages.
 *
 * Deallocate resources for pinned pages.
 */
int micscif_destroy_pinned_pages(struct scif_pinned_pages *pinned_pages)
{
	int j;
	int writeable = pinned_pages->prot & SCIF_PROT_WRITE;
	int kernel = SCIF_MAP_KERNEL & pinned_pages->map_flags;

	for (j = 0; j < pinned_pages->nr_pages; j++) {
		if (pinned_pages->pages[j]) {
			if (!kernel) {
				if (writeable)
					SetPageDirty(pinned_pages->pages[j]);
#ifdef RMA_DEBUG
				BUG_ON(!page_count(pinned_pages->pages[j]));
				BUG_ON(atomic_long_sub_return(1, &ms_info.rma_pin_cnt) < 0);
#endif
				page_cache_release(pinned_pages->pages[j]);
			}
		}
	}

#if !defined(CONFIG_TRANSPARENT_HUGEPAGE) && defined(CONFIG_HUGETLB_PAGE) && !defined(_MIC_SCIF_)
	scif_free(pinned_pages->vma,
		pinned_pages->nr_pages * sizeof(*(pinned_pages->vma)));
#endif
	scif_free(pinned_pages->pages,
		pinned_pages->nr_pages * sizeof(*(pinned_pages->pages)));
	scif_free(pinned_pages->num_pages,
		pinned_pages->nr_pages * sizeof(*(pinned_pages->num_pages)));
	scif_free(pinned_pages, sizeof(*pinned_pages));
	return 0;
}

/*
 * micscif_create_window:
 * @ep: end point
 * @pinned_pages: Set of pinned pages which wil back this window.
 * @offset: offset hint
 *
 * Allocate and prepare a self registration window.
 */
struct reg_range_t *micscif_create_window(struct endpt *ep,
			int64_t nr_pages, uint64_t offset, bool temp)
{
	struct reg_range_t *window;

	might_sleep();
	if (!(window = scif_zalloc(sizeof(struct reg_range_t))))
		goto error;

#ifdef CONFIG_ML1OM
	if (!temp) {
		if (!(window->phys_addr = scif_zalloc(nr_pages *
			sizeof(*(window->phys_addr)))))
			goto error_free_window;

		if (!(window->temp_phys_addr = scif_zalloc(nr_pages *
			sizeof(*(window->temp_phys_addr)))))
			goto error_free_window;
	}
#endif

	if (!(window->dma_addr = scif_zalloc(nr_pages *
			sizeof(*(window->dma_addr)))))
		goto error_free_window;

	if (!(window->num_pages = scif_zalloc(nr_pages *
			sizeof(*(window->num_pages)))))
		goto error_free_window;

	window->offset = offset;
	window->ep = (uint64_t)ep;
	window->magic = SCIFEP_MAGIC;
	window->reg_state = OP_IDLE;
	init_waitqueue_head(&window->regwq);
	window->unreg_state = OP_IDLE;
	init_waitqueue_head(&window->unregwq);
	INIT_LIST_HEAD(&window->list_member);
	window->type = RMA_WINDOW_SELF;
	window->temp = temp;
#ifdef _MIC_SCIF_
	micscif_setup_proxy_dma(ep);
#endif
	return window;

error_free_window:
	if (window->dma_addr)
		scif_free(window->dma_addr, nr_pages * sizeof(*(window->dma_addr)));
#ifdef CONFIG_ML1OM
	if (window->temp_phys_addr)
		scif_free(window->temp_phys_addr, nr_pages * sizeof(*(window->temp_phys_addr)));
	if (window->phys_addr)
		scif_free(window->phys_addr, nr_pages * sizeof(*(window->phys_addr)));
#endif
	scif_free(window, sizeof(*window));
error:
	return NULL;
}

/**
 * micscif_destroy_incomplete_window:
 * @ep: end point
 * @window: registration window
 *
 * Deallocate resources for self window.
 */
int micscif_destroy_incomplete_window(struct endpt *ep, struct reg_range_t *window)
{
	int err;
	int64_t nr_pages = window->nr_pages;
	struct allocmsg *alloc = &window->alloc_handle;
	struct nodemsg msg;

	RMA_MAGIC(window);
retry:
	err = wait_event_timeout(alloc->allocwq, alloc->state != OP_IN_PROGRESS, NODE_ALIVE_TIMEOUT);
	if (!err && scifdev_alive(ep))
		goto retry;

	if (OP_COMPLETED == alloc->state) {
		msg.uop = SCIF_FREE_VIRT;
		msg.src = ep->port;
		msg.payload[0] = ep->remote_ep;
		msg.payload[1] = (uint64_t)window->alloc_handle.vaddr;
		msg.payload[2] = (uint64_t)window;
		msg.payload[3] = SCIF_REGISTER;
		micscif_nodeqp_send(ep->remote_dev, &msg, ep);
	}

	micscif_free_window_offset(ep, window->offset,
		window->nr_pages << PAGE_SHIFT);
	if (window->dma_addr)
		scif_free(window->dma_addr, nr_pages *
			sizeof(*(window->dma_addr)));
	if (window->num_pages)
		scif_free(window->num_pages, nr_pages *
			sizeof(*(window->num_pages)));
#ifdef CONFIG_ML1OM
	if (window->phys_addr)
		scif_free(window->phys_addr, window->nr_pages *
			sizeof(*(window->phys_addr)));
	if (window->temp_phys_addr)
		scif_free(window->temp_phys_addr, nr_pages *
			sizeof(*(window->temp_phys_addr)));
#endif
	scif_free(window, sizeof(*window));
	return 0;
}

/**
 * micscif_destroy_window:
 * @ep: end point
 * @window: registration window
 *
 * Deallocate resources for self window.
 */
int micscif_destroy_window(struct endpt *ep, struct reg_range_t *window)
{
	int j;
	struct scif_pinned_pages *pinned_pages = window->pinned_pages;
	int64_t nr_pages = window->nr_pages;

	might_sleep();
	RMA_MAGIC(window);
	if (!window->temp && window->mm) {
		__scif_dec_pinned_vm_lock(window->mm, window->nr_pages, 0);
		__scif_release_mm(window->mm);
		window->mm = NULL;
	}

	if (!window->offset_freed)
		micscif_free_window_offset(ep, window->offset,
			window->nr_pages << PAGE_SHIFT);
	for (j = 0; j < window->nr_contig_chunks; j++) {
		if (window->dma_addr[j]) {
			unmap_from_aperture(
				window->dma_addr[j],
				ep->remote_dev,
				window->num_pages[j] << PAGE_SHIFT);
		}
	}

	/*
	 * Decrement references for this set of pinned pages from
	 * this window.
	 */
	j = atomic_sub_return((int32_t)pinned_pages->nr_pages, 
				&pinned_pages->ref_count);
	BUG_ON(j < 0);
	/*
	 * If the ref count for pinned_pages is zero then someone
	 * has already called scif_unpin_pages() for it and we should
	 * destroy the page cache.
	 */
	if (!j)
		micscif_destroy_pinned_pages(window->pinned_pages);
	if (window->dma_addr)
		scif_free(window->dma_addr, nr_pages *
			sizeof(*(window->dma_addr)));
	if (window->num_pages)
		scif_free(window->num_pages, nr_pages *
			sizeof(*(window->num_pages)));
#ifdef CONFIG_ML1OM
	if (window->phys_addr)
		scif_free(window->phys_addr, window->nr_pages *
			sizeof(*(window->phys_addr)));
	if (window->temp_phys_addr)
		scif_free(window->temp_phys_addr, nr_pages *
			sizeof(*(window->temp_phys_addr)));
#endif
	window->magic = 0;
	scif_free(window, sizeof(*window));
	return 0;
}

/**
 * micscif_create_remote_lookup:
 * @ep: end point
 * @window: remote window
 *
 * Allocate and prepare lookup entries for the remote
 * end to copy over the physical addresses.
 * Returns 0 on success and appropriate errno on failure.
 */
int micscif_create_remote_lookup(struct endpt *ep, struct reg_range_t *window)
{
	int i, j, err = 0;
	int64_t nr_pages = window->nr_pages;
	bool vmalloc_dma_phys;
#ifdef CONFIG_ML1OM
	bool vmalloc_temp_phys = false;
	bool vmalloc_phys = false;
#endif
	might_sleep();

	/* Map window */
	err = map_virt_into_aperture(&window->mapped_offset,
		window, ep->remote_dev, sizeof(*window));
	if (err)
		goto error_window;

	/* Compute the number of lookup entries. 21 == 2MB Shift */
	window->nr_lookup = ALIGN(nr_pages * PAGE_SIZE, 
					((2) * 1024 * 1024)) >> 21;

	if (!(window->dma_addr_lookup.lookup =
		scif_zalloc(window->nr_lookup *
		sizeof(*(window->dma_addr_lookup.lookup)))))
		goto error_window;

	/* Map DMA physical addess lookup array */
	err = map_virt_into_aperture(&window->dma_addr_lookup.offset,
		window->dma_addr_lookup.lookup, ep->remote_dev,
		window->nr_lookup *
		sizeof(*window->dma_addr_lookup.lookup));
	if (err)
		goto error_window;

	vmalloc_dma_phys = is_vmalloc_addr(&window->dma_addr[0]);

#ifdef CONFIG_ML1OM
	if (ep->remote_dev != &scif_dev[SCIF_HOST_NODE] && !is_self_scifdev(ep->remote_dev)) {
		if (!(window->temp_phys_addr_lookup.lookup =
			scif_zalloc(window->nr_lookup *
				sizeof(*(window->temp_phys_addr_lookup.lookup)))))
			goto error_window;

		/* Map physical addess lookup array */
		err = map_virt_into_aperture(&window->temp_phys_addr_lookup.offset,
			window->temp_phys_addr_lookup.lookup, ep->remote_dev,
			window->nr_lookup *
			sizeof(*window->temp_phys_addr_lookup.lookup));
		if (err)
			goto error_window;

		if (!(window->phys_addr_lookup.lookup =
			scif_zalloc(window->nr_lookup *
				sizeof(*(window->phys_addr_lookup.lookup)))))
			goto error_window;

		/* Map physical addess lookup array */
		err = map_virt_into_aperture(&window->phys_addr_lookup.offset,
			window->phys_addr_lookup.lookup, ep->remote_dev,
			window->nr_lookup *
			sizeof(*window->phys_addr_lookup.lookup));
		if (err)
			goto error_window;

		vmalloc_phys = is_vmalloc_addr(&window->phys_addr[0]);
		vmalloc_temp_phys = is_vmalloc_addr(&window->temp_phys_addr[0]);
	}
#endif

	/* Now map each of the pages containing physical addresses */
	for (i = 0, j = 0; i < nr_pages; i += NR_PHYS_ADDR_IN_PAGE, j++) {
#ifdef CONFIG_ML1OM
		if (ep->remote_dev != &scif_dev[SCIF_HOST_NODE] && !is_self_scifdev(ep->remote_dev)) {
			err = map_page_into_aperture(
				&window->temp_phys_addr_lookup.lookup[j],
				vmalloc_temp_phys ?
					vmalloc_to_page(&window->temp_phys_addr[i]) :
				virt_to_page(&window->temp_phys_addr[i]),
				ep->remote_dev);
			if (err)
				goto error_window;

			err = map_page_into_aperture(
				&window->phys_addr_lookup.lookup[j],
				vmalloc_phys ?
				vmalloc_to_page(&window->phys_addr[i]) :
				virt_to_page(&window->phys_addr[i]),
				ep->remote_dev);
			if (err)
				goto error_window;
		}
#endif
		err = map_page_into_aperture(
			&window->dma_addr_lookup.lookup[j],
			vmalloc_dma_phys ?
			vmalloc_to_page(&window->dma_addr[i]) :
			virt_to_page(&window->dma_addr[i]),
			ep->remote_dev);
		if (err)
			goto error_window;
	}
	return 0;
error_window:
	return err;
}

/**
 * micscif_destroy_remote_lookup:
 * @ep: end point
 * @window: remote window
 *
 * Destroy lookup entries used for the remote
 * end to copy over the physical addresses.
 */
void micscif_destroy_remote_lookup(struct endpt *ep, struct reg_range_t *window)
{
	int i, j;

	RMA_MAGIC(window);
	if (window->nr_lookup) {
		for (i = 0, j = 0; i < window->nr_pages;
			i += NR_PHYS_ADDR_IN_PAGE, j++) {
			if (window->dma_addr_lookup.lookup &&
				window->dma_addr_lookup.lookup[j]) {
				unmap_from_aperture(
				window->dma_addr_lookup.lookup[j],
				ep->remote_dev, PAGE_SIZE);
			}
		}
		if (window->dma_addr_lookup.offset) {
			unmap_from_aperture(
				window->dma_addr_lookup.offset,
				ep->remote_dev, window->nr_lookup *
				sizeof(*window->dma_addr_lookup.lookup));
		}
		if (window->dma_addr_lookup.lookup)
			scif_free(window->dma_addr_lookup.lookup, window->nr_lookup *
				sizeof(*(window->dma_addr_lookup.lookup)));
		if (window->mapped_offset) {
			unmap_from_aperture(window->mapped_offset,
				ep->remote_dev, sizeof(*window));
		}
		window->nr_lookup = 0;
	}
}

/**
 * micscif_create_remote_window:
 * @ep: end point
 * @nr_pages: number of pages in window
 *
 * Allocate and prepare a remote registration window.
 */
struct reg_range_t *micscif_create_remote_window(struct endpt *ep, int nr_pages)
{
	struct reg_range_t *window;

	might_sleep();
	if (!(window = scif_zalloc(sizeof(struct reg_range_t))))
		goto error_ret;

	window->magic = SCIFEP_MAGIC;
	window->nr_pages = nr_pages;

#if !defined(_MIC_SCIF_) && defined(CONFIG_ML1OM)
	if (!(window->page_ref_count = scif_zalloc(nr_pages *
			sizeof(*(window->page_ref_count)))))
		goto error_window;
#endif

	if (!(window->dma_addr = scif_zalloc(nr_pages *
			sizeof(*(window->dma_addr)))))
		goto error_window;

	if (!(window->num_pages = scif_zalloc(nr_pages *
			sizeof(*(window->num_pages)))))
		goto error_window;

#ifdef CONFIG_ML1OM
	if (!(window->phys_addr = scif_zalloc(nr_pages *
			sizeof(*(window->phys_addr)))))
		goto error_window;

	if (!(window->temp_phys_addr = scif_zalloc(nr_pages *
			sizeof(*(window->temp_phys_addr)))))
		goto error_window;
#endif

	if (micscif_create_remote_lookup(ep, window))
		goto error_window;

	window->ep = (uint64_t)ep;
	window->type = RMA_WINDOW_PEER;
	set_window_ref_count(window, nr_pages);
	window->get_put_ref_count = 0;
	window->unreg_state = OP_IDLE;
#if !defined(_MIC_SCIF_) && defined(CONFIG_ML1OM)
	window->gttmap_state = OP_IDLE;
	init_waitqueue_head(&window->gttmapwq);
#endif
#ifdef _MIC_SCIF_
	micscif_setup_proxy_dma(ep);
	window->proxy_dma_phys = ep->rma_info.proxy_dma_phys;
#endif
	return window;
error_window:
	micscif_destroy_remote_window(ep, window);
error_ret:
	return NULL;
}

/**
 * micscif_destroy_remote_window:
 * @ep: end point
 * @window: remote registration window
 *
 * Deallocate resources for remote window.
 */
void micscif_destroy_remote_window(struct endpt *ep, struct reg_range_t *window)
{
	RMA_MAGIC(window);
	micscif_destroy_remote_lookup(ep, window);
	if (window->dma_addr)
		scif_free(window->dma_addr, window->nr_pages *
				sizeof(*(window->dma_addr)));
	if (window->num_pages)
		scif_free(window->num_pages, window->nr_pages *
				sizeof(*(window->num_pages)));
#ifdef CONFIG_ML1OM
	if (window->phys_addr)
		scif_free(window->phys_addr, window->nr_pages *
				sizeof(*(window->phys_addr)));
	if (window->temp_phys_addr)
		scif_free(window->temp_phys_addr, window->nr_pages *
				sizeof(*(window->temp_phys_addr)));
#endif

#if !defined(_MIC_SCIF_) && defined(CONFIG_ML1OM)
	if (window->page_ref_count)
		scif_free(window->page_ref_count, window->nr_pages *
				sizeof(*(window->page_ref_count)));
#endif
	window->magic = 0;
	scif_free(window, sizeof(*window));
}

/**
 * micscif_map_window_pages:
 * @ep: end point
 * @window: self registration window
 * @tmp_wnd: is a temporary window?
 *
 * Map pages of a window into the aperture/PCI.
 * Also compute physical addresses required for DMA.
 */
int micscif_map_window_pages(struct endpt *ep, struct reg_range_t *window, bool tmp_wnd)
{
	int j, i, err = 0, nr_pages;
	scif_pinned_pages_t pinned_pages;

	might_sleep();
	RMA_MAGIC(window);

	pinned_pages = window->pinned_pages;
	for (j = 0, i = 0; j < window->nr_contig_chunks; j++, i += nr_pages) {
		nr_pages = pinned_pages->num_pages[i];
#ifdef _MIC_SCIF_
#ifdef CONFIG_ML1OM
		/* phys_addr[] holds addresses as seen from the remote node
		 * these addressed are then copied into the remote card's
		 * window structure
		 * when the remote node is the host and the card is knf
		 * these addresses are only created at the point of mapping
		 * the card physical address into gtt (for the KNC the
		 * the gtt code path returns the local address)
		 * when the remote node is loopback - the address remains
		 * the same
		 * when the remote node is a kn* - the base address of the local
		 * card as seen from the remote node is added in
		 */
		if (!tmp_wnd) {
			if(ep->remote_dev != &scif_dev[SCIF_HOST_NODE]) {
				if ((err = map_virt_into_aperture(
					&window->temp_phys_addr[j],
					phys_to_virt(page_to_phys(pinned_pages->pages[i])),
					ep->remote_dev,
					nr_pages << PAGE_SHIFT))) {
					int k,l;

					for (l = k = 0; k < i; l++) {
						nr_pages = pinned_pages->num_pages[k];
						window->temp_phys_addr[l]
							&= ~RMA_HUGE_NR_PAGE_MASK;
						unmap_from_aperture(
							window->temp_phys_addr[l],
							ep->remote_dev,
							nr_pages << PAGE_SHIFT);
						k += nr_pages;
						window->temp_phys_addr[l] = 0;
					}
					return err;
				}
				if (!tmp_wnd)
					RMA_SET_NR_PAGES(window->temp_phys_addr[j], nr_pages);
			}
		}
#endif
		window->dma_addr[j] =
			page_to_phys(pinned_pages->pages[i]);
		if (!tmp_wnd)
			RMA_SET_NR_PAGES(window->dma_addr[j], nr_pages);
#else
		err = map_virt_into_aperture(&window->dma_addr[j],
			phys_to_virt(page_to_phys(pinned_pages->pages[i])),
			ep->remote_dev, nr_pages << PAGE_SHIFT);
		if (err)
			return err;
		if (!tmp_wnd)
			RMA_SET_NR_PAGES(window->dma_addr[j], nr_pages);
#endif
		window->num_pages[j] = nr_pages;
	}
	return err;
}


/**
 * micscif_unregister_window:
 * @window: self registration window
 *
 * Send an unregistration request and wait for a response.
 */
int micscif_unregister_window(struct reg_range_t *window)
{
	int err = 0;
	struct endpt *ep = (struct endpt *)window->ep;
	bool send_msg = false;

	might_sleep();
	BUG_ON(!mutex_is_locked(&ep->rma_info.rma_lock));

	switch (window->unreg_state) {
	case OP_IDLE:
	{
		window->unreg_state = OP_IN_PROGRESS;
		send_msg = true;
		/* fall through */
	}
	case OP_IN_PROGRESS:
	{
		get_window_ref_count(window, 1);
		mutex_unlock(&ep->rma_info.rma_lock);
		if (send_msg && (err = micscif_send_scif_unregister(ep, window))) {
			window->unreg_state = OP_COMPLETED;
			goto done;
		}
retry:
		err = wait_event_timeout(window->unregwq, 
			window->unreg_state != OP_IN_PROGRESS, NODE_ALIVE_TIMEOUT);
		if (!err && scifdev_alive(ep))
			goto retry;
		if (!err) {
			err = -ENODEV;
			window->unreg_state = OP_COMPLETED;
			printk(KERN_ERR "%s %d err %d\n", __func__, __LINE__, err);
		}
		if (err > 0)
			err = 0;
done:
		mutex_lock(&ep->rma_info.rma_lock);
		put_window_ref_count(window, 1);
		break;
	}
	case OP_FAILED:
	{
		if (!scifdev_alive(ep)) {
			err = -ENODEV;
			window->unreg_state = OP_COMPLETED;
		}
		break;
	}
	case OP_COMPLETED:
		break;
	default:
		/* Invalid opcode? */
		BUG_ON(1);
	}

	if (OP_COMPLETED == window->unreg_state &&
			window->ref_count)
		put_window_ref_count(window, window->nr_pages);

	if (!window->ref_count) {
		atomic_inc(&ep->rma_info.tw_refcount);
		atomic_add_return((int32_t)window->nr_pages, &ep->rma_info.tw_total_pages);
		list_del(&window->list_member);
		micscif_free_window_offset(ep, window->offset,
				window->nr_pages << PAGE_SHIFT);
		window->offset_freed = true;
		mutex_unlock(&ep->rma_info.rma_lock);
		if ((!!(window->pinned_pages->map_flags & SCIF_MAP_KERNEL))
			&& scifdev_alive(ep)) {
			drain_dma_intr(ep->rma_info.dma_chan);
		} else {
			if (!__scif_dec_pinned_vm_lock(window->mm,
						  window->nr_pages, 1)) {
				__scif_release_mm(window->mm);
				window->mm = NULL;
			}
		}
		micscif_queue_for_cleanup(window, &ms_info.mi_rma);
		mutex_lock(&ep->rma_info.rma_lock);
	}
	return err;
}

/**
 * micscif_send_alloc_request:
 * @ep: end point
 * @window: self registration window
 *
 * Send a remote window allocation request
 */
int micscif_send_alloc_request(struct endpt *ep, struct reg_range_t *window)
{
	struct nodemsg msg;
	struct allocmsg *alloc = &window->alloc_handle;

	/* Set up the Alloc Handle */
	alloc->uop = SCIF_REGISTER;
	alloc->state = OP_IN_PROGRESS;
	init_waitqueue_head(&alloc->allocwq);

	/* Send out an allocation request */
	msg.uop = SCIF_ALLOC_REQ;
	msg.src = ep->port;
	msg.payload[0] = ep->remote_ep;
	msg.payload[1] = window->nr_pages;
	msg.payload[2] = (uint64_t)&window->alloc_handle;
	msg.payload[3] = SCIF_REGISTER;
	return micscif_nodeqp_send(ep->remote_dev, &msg, ep);
}

/**
 * micscif_prep_remote_window:
 * @ep: end point
 * @window: self registration window
 *
 * Send a remote window allocation request, wait for an allocation response,
 * prepare the remote window and notify the peer to unmap it once done.
 */
int micscif_prep_remote_window(struct endpt *ep, struct reg_range_t *window)
{
	struct nodemsg msg;
	struct reg_range_t *remote_window;
	struct allocmsg *alloc = &window->alloc_handle;
	dma_addr_t *dma_phys_lookup, *tmp;
	int i = 0, j = 0;
	int nr_contig_chunks, loop_nr_contig_chunks, remaining_nr_contig_chunks, nr_lookup;
#if defined(_MIC_SCIF_) && defined(CONFIG_ML1OM)
	dma_addr_t *phys_lookup = 0;
#endif
	int err, map_err;

	nr_contig_chunks = remaining_nr_contig_chunks = (int)window->nr_contig_chunks;

	if ((map_err = micscif_map_window_pages(ep, window, false))) {
		printk(KERN_ERR "%s %d map_err %d\n", __func__, __LINE__, map_err);
	}
retry:
	/* Now wait for the response */
	err = wait_event_timeout(alloc->allocwq, alloc->state != OP_IN_PROGRESS, NODE_ALIVE_TIMEOUT);
	if (!err && scifdev_alive(ep))
		goto retry;

	if (!err)
		err = -ENODEV;

	if (err > 0)
		err = 0;
	else
		return err;

	/* Bail out. The remote end rejected this request */
	if (OP_FAILED == alloc->state)
		return -ENOMEM;

	if (map_err) {
		printk(KERN_ERR "%s %d err %d\n", __func__, __LINE__, map_err);
		msg.uop = SCIF_FREE_VIRT;
		msg.src = ep->port;
		msg.payload[0] = ep->remote_ep;
		msg.payload[1] = (uint64_t)window->alloc_handle.vaddr;
		msg.payload[2] = (uint64_t)window;
		msg.payload[3] = SCIF_REGISTER;
		if (!(err = micscif_nodeqp_send(ep->remote_dev, &msg, ep)))
			err = -ENOTCONN;
		else
			err = map_err;
		return err;
	}


	remote_window = scif_ioremap(alloc->phys_addr,
		sizeof(*window), ep->remote_dev);

	RMA_MAGIC(remote_window);

	/* Compute the number of lookup entries. 21 == 2MB Shift */
	nr_lookup = ALIGN(nr_contig_chunks * PAGE_SIZE, ((2) * 1024 * 1024)) >> 21;
#if defined(_MIC_SCIF_) && defined(CONFIG_ML1OM)
	if (is_p2p_scifdev(ep->remote_dev))
		phys_lookup = scif_ioremap(remote_window->temp_phys_addr_lookup.offset,
			nr_lookup *
			sizeof(*remote_window->temp_phys_addr_lookup.lookup),
			ep->remote_dev);
#endif

	dma_phys_lookup = scif_ioremap(remote_window->dma_addr_lookup.offset,
		nr_lookup *
		sizeof(*remote_window->dma_addr_lookup.lookup),
		ep->remote_dev);

	while (remaining_nr_contig_chunks) {
		loop_nr_contig_chunks = min(remaining_nr_contig_chunks, (int)NR_PHYS_ADDR_IN_PAGE);
		/* #1/2 - Copy  physical addresses over to the remote side */

#if defined(_MIC_SCIF_) && defined(CONFIG_ML1OM)
		/*  If the remote dev is self or is any node except the host
		 * its OK to copy the bus address to the remote window
		 * in the case of the host (for KNF only) the bus address
		 * is generated at the time of mmap(..) into card memory
		 * and does not exist at this time
		 */
		/* Note:
		 * the phys_addr[] holds MIC address for remote cards
		 * -> GTT offset  for the host (KNF)
		 * -> local address for the host (KNC)
		 * -> local address for loopback
		 * this is done in map_window_pages(..) except for GTT
		 * offset for KNF
		 */
		if (is_p2p_scifdev(ep->remote_dev)) {
			tmp = scif_ioremap(phys_lookup[j],
				loop_nr_contig_chunks * sizeof(*window->temp_phys_addr),
				ep->remote_dev);
			memcpy_toio(tmp, &window->temp_phys_addr[i], 
				loop_nr_contig_chunks * sizeof(*window->temp_phys_addr));
			serializing_request(tmp);
			smp_mb();
			scif_iounmap(tmp, PAGE_SIZE, ep->remote_dev);
		}
#endif
		/* #2/2 - Copy DMA addresses (addresses that are fed into the DMA engine)
		 * We transfer bus addresses which are then converted into a MIC physical
		 * address on the remote side if it is a MIC, if the remote node is a host
		 * we transfer the MIC physical address
		 */
		tmp = scif_ioremap(
			dma_phys_lookup[j],
			loop_nr_contig_chunks * sizeof(*window->dma_addr),
			ep->remote_dev);
#ifdef _MIC_SCIF_
		if (is_p2p_scifdev(ep->remote_dev)) {
			/* knf:
			 * send the address as mapped through the GTT (the remote node's
			 * base address for this node is already added in)
			 * knc:
			 * add remote node's base address for this node to convert it
			 * into a MIC address
			 */
			int m;
			dma_addr_t dma_addr;
			for (m = 0; m < loop_nr_contig_chunks; m++) {
#ifdef CONFIG_ML1OM
				dma_addr = window->temp_phys_addr[i + m];
#else
				dma_addr = window->dma_addr[i + m] +
					ep->remote_dev->sd_base_addr;
#endif
				writeq(dma_addr, &tmp[m]);
			}
		} else
			/* Host node or loopback - transfer DMA addresses as is, this is
			 * the same as a MIC physical address (we use the dma_addr
			 * and not the phys_addr array since the phys_addr is only setup
			 * if there is a mmap() request from the host)
			 */
			memcpy_toio(tmp, &window->dma_addr[i], 
				loop_nr_contig_chunks * sizeof(*window->dma_addr));
#else
		/* Transfer the physical address array - this is the MIC address
		 * as seen by the card
		 */
		memcpy_toio(tmp, &window->dma_addr[i], 
			loop_nr_contig_chunks * sizeof(*window->dma_addr));
#endif
		remaining_nr_contig_chunks -= loop_nr_contig_chunks;
		i += loop_nr_contig_chunks;
		j++;
		serializing_request(tmp);
		smp_mb();
		scif_iounmap(tmp, PAGE_SIZE, ep->remote_dev);
	}

	/* Prepare the remote window for the peer */
	remote_window->peer_window = (uint64_t)window;
	remote_window->offset = window->offset;
	remote_window->prot = window->prot;
	remote_window->nr_contig_chunks = nr_contig_chunks;
#ifdef _MIC_SCIF_
	if (!ep->rma_info.proxy_dma_peer_phys)
		ep->rma_info.proxy_dma_peer_phys = remote_window->proxy_dma_phys;
#endif
#if defined(_MIC_SCIF_) && defined(CONFIG_ML1OM)
	if (is_p2p_scifdev(ep->remote_dev))
		scif_iounmap(phys_lookup,
			nr_lookup *
			sizeof(*remote_window->temp_phys_addr_lookup.lookup),
			ep->remote_dev);
#endif
	scif_iounmap(dma_phys_lookup,
		nr_lookup *
		sizeof(*remote_window->dma_addr_lookup.lookup),
		ep->remote_dev);
	scif_iounmap(remote_window, sizeof(*remote_window), ep->remote_dev);
	window->peer_window = (uint64_t)alloc->vaddr;
	return err;
}

/**
 * micscif_send_scif_register:
 * @ep: end point
 * @window: self registration window
 *
 * Send a SCIF_REGISTER message if EP is connected and wait for a
 * SCIF_REGISTER_(N)ACK message else send a SCIF_FREE_VIRT
 * message so that the peer can free its remote window allocated earlier.
 */
int micscif_send_scif_register(struct endpt *ep, struct reg_range_t *window)
{
	int err = 0;
	struct nodemsg msg;

	msg.src = ep->port;
	msg.payload[0] = ep->remote_ep;
	msg.payload[1] = (uint64_t)window->alloc_handle.vaddr;
	msg.payload[2] = (uint64_t)window;
	if (SCIFEP_CONNECTED == ep->state) {
		msg.uop = SCIF_REGISTER;
		window->reg_state = OP_IN_PROGRESS;
		if (!(err = micscif_nodeqp_send(ep->remote_dev, &msg, ep))) {
			micscif_set_nr_pages(ep->remote_dev, window);
retry:
			err = wait_event_timeout(window->regwq, 
				window->reg_state != OP_IN_PROGRESS, NODE_ALIVE_TIMEOUT);
			if (!err && scifdev_alive(ep))
				goto retry;
			if (!err)
				err = -ENODEV;
			if (err > 0)
				err = 0;
			if (OP_FAILED == window->reg_state)
				err = -ENOTCONN;
		} else {
			micscif_set_nr_pages(ep->remote_dev, window);
		}
	} else {
		msg.uop = SCIF_FREE_VIRT;
		msg.payload[3] = SCIF_REGISTER;
		if (!(err = micscif_nodeqp_send(ep->remote_dev, &msg, ep)))
			err = -ENOTCONN;
		micscif_set_nr_pages(ep->remote_dev, window);
	}
	return err;
}

/**
 * micscif_send_scif_unregister:
 * @ep: end point
 * @window: self registration window
 *
 * Send a SCIF_UNREGISTER message.
 */
int micscif_send_scif_unregister(struct endpt *ep, struct reg_range_t *window)
{
	struct nodemsg msg;

	RMA_MAGIC(window);
	msg.uop = SCIF_UNREGISTER;
	msg.src = ep->port;
	msg.payload[0] = (uint64_t)window->alloc_handle.vaddr;
	msg.payload[1] = (uint64_t)window;
	return micscif_nodeqp_send(ep->remote_dev, &msg, ep);
}

/**
 * micscif_get_window_offset:
 * @epd: end point descriptor
 * @flags: flags
 * @offset: offset hint
 * @len: length of range
 * @out_offset: computed offset returned by reference.
 *
 * Compute/Claim a new offset for this EP. The callee is supposed to grab
 * the RMA mutex before calling this API.
 */
int micscif_get_window_offset(struct endpt *ep, int flags,
		uint64_t offset, size_t len, uint64_t *out_offset)
{
	uint64_t computed_offset;
	int err = 0;

	might_sleep();
	mutex_lock(&ep->rma_info.va_lock);
	if (flags & SCIF_MAP_FIXED) {
		computed_offset = va_gen_claim(&ep->rma_info.va_gen,
						(uint64_t)offset, len);
		if (INVALID_VA_GEN_ADDRESS == computed_offset)
			err = -EADDRINUSE;
	} else {
		computed_offset = va_gen_alloc(&ep->rma_info.va_gen,
						len, PAGE_SIZE);
		if (INVALID_VA_GEN_ADDRESS == computed_offset)
			err = -ENOMEM;
	}
	*out_offset = computed_offset;
	mutex_unlock(&ep->rma_info.va_lock);
	return err;
}

/**
 * micscif_free_window_offset:
 * @offset: offset hint
 * @len: length of range
 *
 * Free offset for this EP. The callee is supposed to grab
 * the RMA mutex before calling this API.
 */
void micscif_free_window_offset(struct endpt *ep,
		uint64_t offset, size_t len)
{
	mutex_lock(&ep->rma_info.va_lock);
	va_gen_free(&ep->rma_info.va_gen, offset, len);
	mutex_unlock(&ep->rma_info.va_lock);
}

/**
 * scif_register_temp:
 * @epd: End Point Descriptor.
 * @addr: virtual address to/from which to copy
 * @len: length of range to copy
 * @out_offset: computed offset returned by reference.
 * @out_window: allocated registered window returned by reference.
 *
 * Create a temporary registered window. The peer will not know about this
 * window. This API is used for scif_vreadfrom()/scif_vwriteto() API's.
 */
static int
micscif_register_temp(scif_epd_t epd, void *addr, size_t len, int prot,
		off_t *out_offset, struct reg_range_t **out_window)
{
	struct endpt *ep = (struct endpt *)epd;
	int err;
	scif_pinned_pages_t pinned_pages;
	size_t aligned_len;

	aligned_len = ALIGN(len, PAGE_SIZE);

	if ((err = __scif_pin_pages((void *)((uint64_t)addr &
			PAGE_MASK),
			aligned_len, &prot, 0, &pinned_pages)))
		return err;

	pinned_pages->prot = prot;

	/* Compute the offset for this registration */
	if ((err = micscif_get_window_offset(ep, 0, 0,
		aligned_len, (uint64_t *)out_offset)))
		goto error_unpin;

	/* Allocate and prepare self registration window */
	if (!(*out_window = micscif_create_window(ep, aligned_len >> PAGE_SHIFT,
					*out_offset, true))) {
		micscif_free_window_offset(ep, *out_offset, aligned_len);
		err = -ENOMEM;
		goto error_unpin;
	}

	(*out_window)->pinned_pages = pinned_pages;
	(*out_window)->nr_pages = pinned_pages->nr_pages;
	(*out_window)->nr_contig_chunks = pinned_pages->nr_contig_chunks;
	(*out_window)->prot = pinned_pages->prot;

	(*out_window)->va_for_temp = (void*)((uint64_t)addr & PAGE_MASK);
	if ((err = micscif_map_window_pages(ep, *out_window, true))) {
		/* Something went wrong! Rollback */
		micscif_destroy_window(ep, *out_window);
		*out_window = NULL;
	} else
		*out_offset |= ((uint64_t)addr & ~PAGE_MASK);

	return err;
error_unpin:
	if (err)
		printk(KERN_ERR "%s %d err %d\n", __func__, __LINE__, err);
	scif_unpin_pages(pinned_pages);
	return err;
}

/**
 * micscif_rma_completion_cb:
 * @data: RMA cookie
 *
 * RMA interrupt completion callback.
 */
void micscif_rma_completion_cb(uint64_t data)
{
	struct dma_completion_cb *comp_cb = (struct dma_completion_cb *)data;
#ifndef _MIC_SCIF_
	struct pci_dev *pdev;
#endif

	/* Free DMA Completion CB. */
	if (comp_cb && comp_cb->temp_buf) {
		if (comp_cb->dst_window) {
			micscif_rma_local_cpu_copy(comp_cb->dst_offset,
				comp_cb->dst_window, comp_cb->temp_buf + comp_cb->header_padding,
				comp_cb->len, false);
		}
#ifndef _MIC_SCIF_
		micscif_pci_dev(comp_cb->remote_node, &pdev);
		mic_ctx_unmap_single(get_per_dev_ctx(comp_cb->remote_node - 1), 
			comp_cb->temp_phys, KMEM_UNALIGNED_BUF_SIZE);
#endif
		if (comp_cb->is_cache)
			micscif_kmem_cache_free(comp_cb->temp_buf_to_free);
		else
			kfree(comp_cb->temp_buf_to_free);
	}
	kfree(comp_cb);
}

static void __micscif_rma_destroy_tcw_ep(struct endpt *ep);
static
bool micscif_rma_tc_can_cache(struct endpt *ep, size_t cur_bytes)
{
	if ((cur_bytes >> PAGE_SHIFT) > ms_info.mi_rma_tc_limit)
		return false;
	if ((atomic_read(&ep->rma_info.tcw_total_pages)
			+ (cur_bytes >> PAGE_SHIFT)) >
			ms_info.mi_rma_tc_limit) {
		printk(KERN_ALERT "%s %d total=%d, current=%zu reached max\n", 
				__func__, __LINE__, 
				atomic_read(&ep->rma_info.tcw_total_pages), 
				(1 + (cur_bytes >> PAGE_SHIFT)));
		micscif_rma_destroy_tcw_invalid(&ms_info.mi_rma_tc);
		__micscif_rma_destroy_tcw_ep(ep);
	}
	return true;
}

/**
 * micscif_rma_copy:
 * @epd: end point descriptor.
 * @loffset: offset in local registered address space to/from which to copy
 * @addr: user virtual address to/from which to copy
 * @len: length of range to copy
 * @roffset: offset in remote registered address space to/from which to copy
 * @flags: flags
 * @dir: LOCAL->REMOTE or vice versa.
 *
 * Validate parameters, check if src/dst registered ranges requested for copy
 * are valid and initiate either CPU or DMA copy.
 */
int micscif_rma_copy(scif_epd_t epd, off_t loffset, void *addr, size_t len,
		off_t roffset, int flags, enum rma_direction dir, bool last_chunk)
{
	struct endpt *ep = (struct endpt *)epd;
	struct micscif_rma_req remote_req;
	struct micscif_rma_req req;
	struct reg_range_t *window = NULL;
	struct reg_range_t *remote_window = NULL;
	struct mic_copy_work copy_work;
	bool loopback;
	int err = 0;
	struct dma_channel *chan;
	struct rma_mmu_notifier *mmn = NULL;
	bool insert_window = false;
	bool cache = false;

	if ((err = verify_epd(ep)))
		return err;

	if (flags && !(flags & (SCIF_RMA_USECPU | SCIF_RMA_USECACHE | SCIF_RMA_SYNC | SCIF_RMA_ORDERED)))
		return -EINVAL;

	if (!len)
		return -EINVAL;
	loopback = is_self_scifdev(ep->remote_dev) ? true : false;
	copy_work.fence_type = ((flags & SCIF_RMA_SYNC) && last_chunk) ? DO_DMA_POLLING : 0;
	copy_work.ordered = !!((flags & SCIF_RMA_ORDERED) && last_chunk);

#ifdef CONFIG_MMU_NOTIFIER
	if (!mic_reg_cache_enable)
		flags &= ~SCIF_RMA_USECACHE;
#else
	flags &= ~SCIF_RMA_USECACHE;
#endif
#ifndef _MIC_SCIF_
#ifdef CONFIG_ML1OM
	/* Use DMA Copies even if CPU copy is requested on KNF MIC from Host */
	if (flags & SCIF_RMA_USECPU) {
		flags &= ~SCIF_RMA_USECPU;
		if (last_chunk)
			copy_work.fence_type = DO_DMA_POLLING;
	}
#endif
	/* Use CPU for Host<->Host Copies */
	if (loopback) {
		flags |= SCIF_RMA_USECPU;
		copy_work.fence_type = 0x0;
	}
#endif

	cache = flags & SCIF_RMA_USECACHE;

	/* Trying to wrap around */
	if ((loffset && (loffset + (off_t)len < loffset)) ||
		(roffset + (off_t)len < roffset))
		return -EINVAL;

	remote_req.out_window = &remote_window;
	remote_req.offset = roffset;
	remote_req.nr_bytes = len;
	/*
	 * If transfer is from local to remote then the remote window
	 * must be writeable and vice versa.
	 */
	remote_req.prot = LOCAL_TO_REMOTE == dir ? VM_WRITE : VM_READ;
	remote_req.type = WINDOW_PARTIAL;
	remote_req.head = &ep->rma_info.remote_reg_list;

#ifdef CONFIG_MMU_NOTIFIER
	if (addr && cache) {
		mutex_lock(&ep->rma_info.mmn_lock);
		mmn = find_mmu_notifier(current->mm, &ep->rma_info);
		if (!mmn) {
			mmn = kzalloc(sizeof(*mmn), GFP_KERNEL);
			if (!mmn) {
				mutex_unlock(&ep->rma_info.mmn_lock);
				return -ENOMEM;
			}
			init_mmu_notifier(mmn, current->mm, ep);
			if (mmu_notifier_register(&mmn->ep_mmu_notifier, current->mm)) {
				mutex_unlock(&ep->rma_info.mmn_lock);
				kfree(mmn);
				return -EBUSY;
			}
#ifdef RMA_DEBUG
			atomic_long_add_return(1, &ms_info.mmu_notif_cnt);
#endif
			list_add(&mmn->list_member, &ep->rma_info.mmn_list);
		}
		mutex_unlock(&ep->rma_info.mmn_lock);
	}
#endif

	micscif_inc_node_refcnt(ep->remote_dev, 1);
#ifdef _MIC_SCIF_
	if (!(flags & SCIF_RMA_USECPU)) {
		/*
		 * Proxy the DMA only for P2P reads with transfer size
		 * greater than proxy DMA threshold. scif_vreadfrom(..)
		 * and scif_vwriteto(..) is not supported since the peer
		 * does not have the page lists required to perform the
		 * proxy DMA.
		 */
		if (ep->remote_dev->sd_proxy_dma_reads &&
			!addr && dir == REMOTE_TO_LOCAL &&
			ep->rma_info.proxy_dma_va &&
			len >= ms_info.mi_proxy_dma_threshold) {
			copy_work.len = len;
			copy_work.src_offset = roffset;
			copy_work.dst_offset = loffset;
			/* Fall through if there were errors */
			if (!(err = micscif_proxy_dma(epd, &copy_work)))
				goto error;
		}
	}
#endif
	mutex_lock(&ep->rma_info.rma_lock);
	if (addr) {
		req.out_window = &window;
		req.nr_bytes = ALIGN(len + ((uint64_t)addr & ~PAGE_MASK), PAGE_SIZE);
		if (mmn)
			req.head = &mmn->tc_reg_list;
		req.va_for_temp = (void*)((uint64_t)addr & PAGE_MASK);
		req.prot = (LOCAL_TO_REMOTE == dir ? VM_READ : VM_WRITE | VM_READ);
		/* Does a valid local window exist? */

		pr_debug("%s %d req.va_for_temp %p addr %p req.nr_bytes 0x%lx len 0x%lx\n", 
			__func__, __LINE__, req.va_for_temp, addr, req.nr_bytes, len);
		spin_lock(&ep->rma_info.tc_lock);
		if (!mmn || (err = micscif_query_tcw(ep, &req))) {
			pr_debug("%s %d err %d req.va_for_temp %p addr %p req.nr_bytes 0x%lx len 0x%lx\n", 
				__func__, __LINE__, err, req.va_for_temp, addr, req.nr_bytes, len);
			spin_unlock(&ep->rma_info.tc_lock);
			mutex_unlock(&ep->rma_info.rma_lock);
			if (cache)
				if (!micscif_rma_tc_can_cache(ep, req.nr_bytes))
					cache = false;
			if ((err = micscif_register_temp(epd, req.va_for_temp, req.nr_bytes,
					req.prot,
					&loffset, &window))) {
				goto error;
			}
			mutex_lock(&ep->rma_info.rma_lock);
			pr_debug("New temp window created addr %p\n", addr);
			if (cache) {
				atomic_inc(&ep->rma_info.tcw_refcount);
				atomic_add_return((int32_t)window->nr_pages, &ep->rma_info.tcw_total_pages);
				if (mmn) {
					spin_lock(&ep->rma_info.tc_lock);
					micscif_insert_tcw(window, &mmn->tc_reg_list);
					spin_unlock(&ep->rma_info.tc_lock);
				}
			}
			insert_window = true;
		} else {
			spin_unlock(&ep->rma_info.tc_lock);
			pr_debug("window found for addr %p\n", addr);
			BUG_ON(window->va_for_temp > addr);
		}
		loffset = window->offset + ((uint64_t)addr - (uint64_t)window->va_for_temp);
		pr_debug("%s %d addr %p loffset 0x%lx window->nr_pages 0x%llx"
			" window->va_for_temp %p\n", __func__, __LINE__, 
			addr, loffset, window->nr_pages, window->va_for_temp);
		RMA_MAGIC(window);
	}

	/* Does a valid remote window exist? */
	if ((err = micscif_query_window(&remote_req))) {
		pr_debug("%s %d err %d roffset 0x%lx len 0x%lx\n", 
				__func__, __LINE__, err, roffset, len);
		mutex_unlock(&ep->rma_info.rma_lock);
		goto error;
	}
	RMA_MAGIC(remote_window);
	if (!addr) {
		req.out_window = &window;
		req.offset = loffset;
		/*
		 * If transfer is from local to remote then the self window
		 * must be readable and vice versa.
		 */
		req.prot = LOCAL_TO_REMOTE == dir ? VM_READ : VM_WRITE;
		req.nr_bytes = len;
		req.type = WINDOW_PARTIAL;
		req.head = &ep->rma_info.reg_list;
		/* Does a valid local window exist? */
		if ((err = micscif_query_window(&req))) {
			printk(KERN_ERR "%s %d err %d\n", __func__, __LINE__, err);
			mutex_unlock(&ep->rma_info.rma_lock);
			goto error;
		}
		RMA_MAGIC(window);
	}

	/*
	 * Preprare copy_work for submitting work to the DMA kernel thread
	 * or CPU copy routine.
	 */
	copy_work.len = len;
	copy_work.loopback = loopback;
	copy_work.remote_dev = ep->remote_dev;
	copy_work.dma_chan_released = false;
	if (LOCAL_TO_REMOTE == dir) {
		copy_work.src_offset = loffset;
		copy_work.src_window = window;
		copy_work.dst_offset = roffset;
		copy_work.dst_window = remote_window;
	} else {
		copy_work.src_offset = roffset;
		copy_work.src_window = remote_window;
		copy_work.dst_offset = loffset;
		copy_work.dst_window = window;
	}

	if (!(flags & SCIF_RMA_USECPU)) {
		chan = ep->rma_info.dma_chan;
		if ((err = request_dma_channel(chan))) {
			mutex_unlock(&ep->rma_info.rma_lock);
			goto error;
		}
		err = micscif_rma_list_dma_copy_wrapper(epd, &copy_work,
							chan, loffset);
		if (!copy_work.dma_chan_released)
			free_dma_channel(chan);
	}
	if (flags & SCIF_RMA_USECPU) {
		/* Initiate synchronous CPU copy */
		micscif_rma_list_cpu_copy(&copy_work);
	}
	if (insert_window && !cache) {
		atomic_inc(&ep->rma_info.tw_refcount);
		atomic_add_return((int32_t)window->nr_pages, &ep->rma_info.tw_total_pages);
	}

	mutex_unlock(&ep->rma_info.rma_lock);

	if (last_chunk) {
		if (DO_DMA_POLLING == copy_work.fence_type)
			err = drain_dma_poll(ep->rma_info.dma_chan);
		else if (DO_DMA_INTR == copy_work.fence_type)
			err = drain_dma_intr(ep->rma_info.dma_chan);
	}

	micscif_dec_node_refcnt(ep->remote_dev, 1);
	if (insert_window && !cache)
		micscif_queue_for_cleanup(window, &ms_info.mi_rma);
	return err;
error:
	if (err) {
		if (addr && window && !cache)
			micscif_destroy_window(ep, window);
		printk(KERN_ERR "%s %d err %d len 0x%lx\n", __func__, __LINE__, err, len);
	}
	micscif_dec_node_refcnt(ep->remote_dev, 1);
	return err;
}

/**
 * micscif_send_fence_mark:
 * @epd: end point descriptor.
 * @out_mark: Output DMA mark reported by peer.
 *
 * Send a remote fence mark request.
 */
int micscif_send_fence_mark(scif_epd_t epd, int *out_mark)
{
	int err;
	struct nodemsg msg;
	struct fence_info *fence_req;
	struct endpt *ep = (struct endpt *)epd;

	if (!(fence_req = kmalloc(sizeof(*fence_req), GFP_KERNEL))) {
		err = -ENOMEM;
		goto error;
	}

	fence_req->state = OP_IN_PROGRESS;
	init_waitqueue_head(&fence_req->wq);

	msg.src = ep->port;
	msg.uop = SCIF_MARK;
	msg.payload[0] = ep->remote_ep;
	msg.payload[1] = (uint64_t)fence_req;

	if ((err = micscif_nodeqp_send(ep->remote_dev, &msg, ep)))
		goto error;

retry:
	err = wait_event_timeout(fence_req->wq, 
		(OP_IN_PROGRESS != fence_req->state), NODE_ALIVE_TIMEOUT);
	if (!err && scifdev_alive(ep))
		goto retry;
	if (!err)
		err = -ENODEV;
	if (err > 0)
		err = 0;
	if (err < 0) {
		mutex_lock(&ep->rma_info.rma_lock);
		if (OP_IN_PROGRESS == fence_req->state)
			fence_req->state = OP_FAILED;
		mutex_unlock(&ep->rma_info.rma_lock);
	}
	if (OP_COMPLETED == fence_req->state)
		*out_mark = SCIF_REMOTE_FENCE | fence_req->dma_mark;

	if (OP_FAILED == fence_req->state && !err)
		err = -ENOMEM;
	mutex_lock(&ep->rma_info.rma_lock);
	mutex_unlock(&ep->rma_info.rma_lock);
	kfree(fence_req);
error:
	return err;
}

/**
 * micscif_send_fence_wait:
 * @epd: end point descriptor.
 * @mark: DMA mark to wait for.
 *
 * Send a remote fence wait request.
 */
int micscif_send_fence_wait(scif_epd_t epd, int mark)
{
	int err;
	struct nodemsg msg;
	struct fence_info *fence_req;
	struct endpt *ep = (struct endpt *)epd;

	if (!(fence_req = kmalloc(sizeof(*fence_req), GFP_KERNEL))) {
		err = -ENOMEM;
		goto error;
	}

	fence_req->state = OP_IN_PROGRESS;
	init_waitqueue_head(&fence_req->wq);

	msg.src = ep->port;
	msg.uop = SCIF_WAIT;
	msg.payload[0] = ep->remote_ep;
	msg.payload[1] = (uint64_t)fence_req;
	msg.payload[2] = mark;

	if ((err = micscif_nodeqp_send(ep->remote_dev, &msg, ep)))
		goto error;
retry:
	err = wait_event_timeout(fence_req->wq, 
		(OP_IN_PROGRESS != fence_req->state), NODE_ALIVE_TIMEOUT);
	if (!err && scifdev_alive(ep))
		goto retry;
	if (!err)
		err = -ENODEV;
	if (err > 0)
		err = 0;
	if (err < 0) {
		mutex_lock(&ep->rma_info.rma_lock);
		if (OP_IN_PROGRESS == fence_req->state)
			fence_req->state = OP_FAILED;
		mutex_unlock(&ep->rma_info.rma_lock);
	}
	if (OP_FAILED == fence_req->state && !err)
		err = -ENOMEM;
	mutex_lock(&ep->rma_info.rma_lock);
	mutex_unlock(&ep->rma_info.rma_lock);
	kfree(fence_req);
error:
	return err;
}

/**
 * micscif_send_fence_signal:
 * @epd - endpoint descriptor
 * @loff - local offset
 * @lval - local value to write to loffset
 * @roff - remote offset
 * @rval - remote value to write to roffset
 * @flags - flags
 *
 * Sends a remote fence signal request
 */
int micscif_send_fence_signal(scif_epd_t epd, off_t roff, uint64_t rval,
		off_t loff, uint64_t lval, int flags)
{
	int err = 0;
	struct nodemsg msg;
	struct fence_info *fence_req;
	struct endpt *ep = (struct endpt *)epd;

	if (!(fence_req = kmalloc(sizeof(*fence_req), GFP_KERNEL))) {
		err = -ENOMEM;
		goto error;
	}

	fence_req->state = OP_IN_PROGRESS;
	init_waitqueue_head(&fence_req->wq);

	msg.src = ep->port;
	if (flags & SCIF_SIGNAL_LOCAL) {
		msg.uop = SCIF_SIG_LOCAL;
		msg.payload[0] = ep->remote_ep;
		msg.payload[1] = roff;
		msg.payload[2] = rval;
		msg.payload[3] = (uint64_t)fence_req;
		if ((err = micscif_nodeqp_send(ep->remote_dev, &msg, ep)))
			goto error_free;
retry1:
		err = wait_event_timeout(fence_req->wq, 
			(OP_IN_PROGRESS != fence_req->state), NODE_ALIVE_TIMEOUT);
		if (!err && scifdev_alive(ep))
			goto retry1;
		if (!err)
			err = -ENODEV;
		if (err > 0)
			err = 0;
		if (err < 0) {
			mutex_lock(&ep->rma_info.rma_lock);
			if (OP_IN_PROGRESS == fence_req->state)
				fence_req->state = OP_FAILED;
			mutex_unlock(&ep->rma_info.rma_lock);
		}
		if (OP_FAILED == fence_req->state && !err) {
			err = -ENXIO;
			goto error_free;
		}
	}
	fence_req->state = OP_IN_PROGRESS;

	if (flags & SCIF_SIGNAL_REMOTE) {
		msg.uop = SCIF_SIG_REMOTE;
		msg.payload[0] = ep->remote_ep;
		msg.payload[1] = loff;
		msg.payload[2] = lval;
		msg.payload[3] = (uint64_t)fence_req;
		if ((err = micscif_nodeqp_send(ep->remote_dev, &msg, ep)))
			goto error_free;
retry2:
		err = wait_event_timeout(fence_req->wq, 
			(OP_IN_PROGRESS != fence_req->state), NODE_ALIVE_TIMEOUT);
		if (!err && scifdev_alive(ep))
			goto retry2;
		if (!err)
			err = -ENODEV;
		if (err > 0)
			err = 0;
		if (err < 0) {
			mutex_lock(&ep->rma_info.rma_lock);
			if (OP_IN_PROGRESS == fence_req->state)
				fence_req->state = OP_FAILED;
			mutex_unlock(&ep->rma_info.rma_lock);
		}
		if (OP_FAILED == fence_req->state && !err) {
			err = -ENXIO;
			goto error_free;
		}
	}
error_free:
	mutex_lock(&ep->rma_info.rma_lock);
	mutex_unlock(&ep->rma_info.rma_lock);
	kfree(fence_req);
error:
	return err;
}

/*
 * micscif_fence_mark:
 *
 * @epd - endpoint descriptor
 * Set up a mark for this endpoint and return the value of the mark.
 */
int micscif_fence_mark(scif_epd_t epd)
{
	int mark = 0;
	struct endpt *ep = (struct endpt *)epd;
	struct dma_channel *chan = ep->rma_info.dma_chan;

	if ((mark = request_dma_channel(chan)))
		goto error;

	mark = program_dma_mark(chan);

	free_dma_channel(chan);
error:
	return mark;
}

/**
 * micscif_rma_destroy_temp_windows:
 *
 * This routine destroys temporary registered windows created
 * by scif_vreadfrom() and scif_vwriteto().
 */
void micscif_rma_destroy_temp_windows(void)
{
	struct list_head *item, *tmp;
	struct reg_range_t *window;
	struct endpt *ep;
	struct dma_channel *chan;
	might_sleep();
restart:
	spin_lock(&ms_info.mi_rmalock);
	list_for_each_safe(item, tmp, &ms_info.mi_rma) {
		window = list_entry(item, 
			struct reg_range_t, list_member);
		ep = (struct endpt *)window->ep;
		chan = ep->rma_info.dma_chan;

		list_del(&window->list_member);
		spin_unlock(&ms_info.mi_rmalock);
		micscif_inc_node_refcnt(ep->remote_dev, 1);
		if (!chan ||
			!scifdev_alive(ep) ||
			(!is_current_dma_mark(chan, window->dma_mark) &&
			is_dma_mark_processed(chan, window->dma_mark)) ||
			!drain_dma_intr(chan)) {
			micscif_dec_node_refcnt(ep->remote_dev, 1);
			/* Remove window from global list */
			window->unreg_state = OP_COMPLETED;
		} else {
			micscif_dec_node_refcnt(ep->remote_dev, 1);
			/* DMA engine hung ?? */
			printk(KERN_ERR "%s %d DMA channel %d hung ep->state %d "
				"window->dma_mark 0x%x channel_mark 0x%x\n", 
				__func__, __LINE__, get_chan_num(chan), 
				ep->sd_state, window->dma_mark, get_dma_mark(chan));
			WARN_ON(1);
			micscif_queue_for_cleanup(window, &ms_info.mi_rma);
			goto restart;
		}

		if (OP_COMPLETED == window->unreg_state) {
			BUG_ON(atomic_sub_return((int32_t)window->nr_pages, 
					&ep->rma_info.tw_total_pages) < 0);
			if (RMA_WINDOW_SELF == window->type)
				micscif_destroy_window(ep, window);
			else
				micscif_destroy_remote_window(ep, window);
			BUG_ON(atomic_dec_return(
				&ep->rma_info.tw_refcount) < 0);
		}
		goto restart;
	}
	spin_unlock(&ms_info.mi_rmalock);
}

/**
 * micscif_rma_destroy_tcw:
 *
 * This routine destroys temporary registered windows created
 * by scif_vreadfrom() and scif_vwriteto().
 */
static
void __micscif_rma_destroy_tcw(struct rma_mmu_notifier *mmn,
						struct endpt *ep, bool inrange,
						uint64_t start, uint64_t len)
{
	struct list_head *item, *tmp;
	struct reg_range_t *window;
	uint64_t start_va, end_va;
	uint64_t end = start + len;
	list_for_each_safe(item, tmp, &mmn->tc_reg_list) {
		window = list_entry(item, 
			struct reg_range_t, list_member);
		ep = (struct endpt *)window->ep;
		if (inrange) {
			if (0 == len)
				break;
			start_va = (uint64_t)window->va_for_temp;
			end_va = start_va+ (window->nr_pages << PAGE_SHIFT);
			if (start < start_va) {
				if (end <= start_va) {
					break;
				} else {
				}

			} else {
				if (start >= end_va) {
					continue;
				} else {
				}
			}
		}
		__micscif_rma_destroy_tcw_helper(window);
	}
}

static inline
void micscif_rma_destroy_tcw(struct rma_mmu_notifier *mmn,
						struct endpt *ep, bool inrange,
						uint64_t start, uint64_t len)
{
	unsigned long sflags;

	spin_lock_irqsave(&ep->rma_info.tc_lock, sflags);
	__micscif_rma_destroy_tcw(mmn, ep, inrange, start, len);
	spin_unlock_irqrestore(&ep->rma_info.tc_lock, sflags);
}

static void __micscif_rma_destroy_tcw_ep(struct endpt *ep)
{
	struct list_head *item, *tmp;
	struct rma_mmu_notifier *mmn;
	spin_lock(&ep->rma_info.tc_lock);
	list_for_each_safe(item, tmp, &ep->rma_info.mmn_list) {
		mmn = list_entry(item, 
			struct rma_mmu_notifier, list_member);
		__micscif_rma_destroy_tcw(mmn, ep, false, 0, 0);
	}
	spin_unlock(&ep->rma_info.tc_lock);
}

void micscif_rma_destroy_tcw_ep(struct endpt *ep)
{
	struct list_head *item, *tmp;
	struct rma_mmu_notifier *mmn;
	list_for_each_safe(item, tmp, &ep->rma_info.mmn_list) {
		mmn = list_entry(item, 
			struct rma_mmu_notifier, list_member);
		micscif_rma_destroy_tcw(mmn, ep, false, 0, 0);
	}
}

/**
 * micscif_rma_destroy_tcw:
 *
 * This routine destroys temporary registered windows created
 * by scif_vreadfrom() and scif_vwriteto().
 */
void micscif_rma_destroy_tcw_invalid(struct list_head *list)
{
	struct list_head *item, *tmp;
	struct reg_range_t *window;
	struct endpt *ep;
	struct dma_channel *chan;
	might_sleep();
restart:
	spin_lock(&ms_info.mi_rmalock);
	list_for_each_safe(item, tmp, list) {
		window = list_entry(item, 
			struct reg_range_t, list_member);
		ep = (struct endpt *)window->ep;
		chan = ep->rma_info.dma_chan;
		list_del(&window->list_member);
		spin_unlock(&ms_info.mi_rmalock);
		micscif_inc_node_refcnt(ep->remote_dev, 1);
		mutex_lock(&ep->rma_info.rma_lock);
		if (!chan ||
			!scifdev_alive(ep) ||
			(!is_current_dma_mark(chan, window->dma_mark) &&
			is_dma_mark_processed(chan, window->dma_mark)) ||
			!drain_dma_intr(chan)) {
			micscif_dec_node_refcnt(ep->remote_dev, 1);
			BUG_ON(atomic_sub_return((int32_t)window->nr_pages, 
						&ep->rma_info.tcw_total_pages) < 0);
			micscif_destroy_window(ep, window);
			BUG_ON(atomic_dec_return(
						&ep->rma_info.tcw_refcount) < 0);
		} else {
			/* DMA engine hung ?? */
			printk(KERN_ERR "%s %d DMA channel %d hung ep->state %d "
				"window->dma_mark 0x%x channel_mark 0x%x\n", 
				__func__, __LINE__, get_chan_num(chan), 
				ep->sd_state, window->dma_mark, get_dma_mark(chan));
			WARN_ON(1);
			mutex_unlock(&ep->rma_info.rma_lock);
			micscif_dec_node_refcnt(ep->remote_dev, 1);
			micscif_queue_for_cleanup(window, &ms_info.mi_rma);
			goto restart;
		}
		mutex_unlock(&ep->rma_info.rma_lock);
		goto restart;
	}
	spin_unlock(&ms_info.mi_rmalock);
}

/**
 * micscif_rma_handle_remote_fences:
 *
 * This routine services remote fence requests.
 */
void micscif_rma_handle_remote_fences(void)
{
	struct list_head *item, *tmp;
	struct remote_fence_info *fence;
	struct endpt *ep;
	int mark;

	might_sleep();
	mutex_lock(&ms_info.mi_fencelock);
	list_for_each_safe(item, tmp, &ms_info.mi_fence) {
		fence = list_entry(item, 
			struct remote_fence_info, list_member);
		/* Remove fence from global list */
		list_del(&fence->list_member);

		/* Initiate the fence operation */
		ep = (struct endpt *)fence->msg.payload[0];
		mark = (int)fence->msg.payload[2];
		BUG_ON(!(mark & SCIF_REMOTE_FENCE));
		if (dma_mark_wait(ep->rma_info.dma_chan,
				mark & ~SCIF_REMOTE_FENCE, false)) {
			printk(KERN_ERR "%s %d err\n", __func__, __LINE__);
			fence->msg.uop = SCIF_WAIT_NACK;
		} else {
			fence->msg.uop = SCIF_WAIT_ACK;
		}
		micscif_inc_node_refcnt(ep->remote_dev, 1);
		fence->msg.payload[0] = ep->remote_ep;
		/* No error handling for Notification messages. */
		micscif_nodeqp_send(ep->remote_dev, &fence->msg, ep);
		micscif_dec_node_refcnt(ep->remote_dev, 1);
		kfree(fence);
		/*
		 * Decrement ref count and wake up
		 * any thread blocked in the EP close routine waiting
		 * for all such remote fence requests to complete.
		 */
		ep->rma_info.fence_refcount--;
		wake_up(&ep->rma_info.fence_wq);
	}
	mutex_unlock(&ms_info.mi_fencelock);
}

#ifdef CONFIG_MMU_NOTIFIER
void micscif_mmu_notif_handler(struct work_struct *work)
{
	struct list_head *pos, *tmpq;
	struct endpt *ep;
restart:
	micscif_rma_destroy_tcw_invalid(&ms_info.mi_rma_tc);
	spin_lock(&ms_info.mi_rmalock);
	list_for_each_safe(pos, tmpq, &ms_info.mi_mmu_notif_cleanup) {
		ep = list_entry(pos, struct endpt, mmu_list);
		list_del(&ep->mmu_list);
		spin_unlock(&ms_info.mi_rmalock);
		BUG_ON(list_empty(&ep->rma_info.mmn_list));

		micscif_rma_destroy_tcw_ep(ep);
		ep_unregister_mmu_notifier(ep);
		queue_work(ms_info.mi_misc_wq, &ms_info.mi_misc_work);
		goto restart;
	}
	spin_unlock(&ms_info.mi_rmalock);
}
#endif

/**
 * micscif_reserve_dma_chan:
 * @ep: Endpoint Descriptor.
 *
 * This routine reserves a DMA channel for a particular
 * endpoint. All DMA transfers for an endpoint are always
 * programmed on the same DMA channel.
 */
int micscif_reserve_dma_chan(struct endpt *ep)
{
	int err = 0;
#ifndef _MIC_SCIF_
	/*
	 * Host Loopback cannot use DMA by design and hence
	 * reserving DMA channels is a nop.
	 */
	if (is_self_scifdev(ep->remote_dev))
		return 0;
#endif
	mutex_lock(&ep->rma_info.rma_lock);
	if (!ep->rma_info.dma_chan) {
		struct dma_channel **chan = &ep->rma_info.dma_chan;
		unsigned long ts = jiffies;
#ifndef _MIC_SCIF_
		mic_ctx_t *mic_ctx =
			get_per_dev_ctx(ep->remote_dev->sd_node - 1);
		BUG_ON(!ep->remote_dev->sd_node);
#endif
		while (true) {
			if (!(err = allocate_dma_channel((struct mic_dma_ctx_t *)
#ifdef _MIC_SCIF_
				mic_dma_handle,
#else
				mic_ctx->dma_handle,
#endif
				chan)))
				break;
			schedule();
			if (time_after(jiffies,
				ts + NODE_ALIVE_TIMEOUT)) {
				err = -EBUSY;
				goto error;
			}
		}
		mic_dma_thread_free_chan(*chan);
	}
error:
	mutex_unlock(&ep->rma_info.rma_lock);
	return err;
}

/*
 * micscif_prog_signal:
 * @epd - Endpoint Descriptor
 * @offset - registered address
 * @val - Value to be programmed in SUD.
 * @type - Type of the window.
 *
 * Program a status update descriptor adter ensuring that the offset
 * provided is indeed valid.
 */
int micscif_prog_signal(scif_epd_t epd, off_t offset, uint64_t val,
		enum rma_window_type type)
{
	struct endpt *ep = (struct endpt *)epd;
	struct dma_channel *chan = ep->rma_info.dma_chan;
	struct reg_range_t *window = NULL;
	struct micscif_rma_req req;
	int err;
	dma_addr_t phys;

	mutex_lock(&ep->rma_info.rma_lock);
	req.out_window = &window;
	req.offset = offset;
	req.nr_bytes = sizeof(uint64_t);
	req.prot = SCIF_PROT_WRITE;
	req.type = WINDOW_SINGLE;
	if (RMA_WINDOW_SELF == type)
		req.head = &ep->rma_info.reg_list;
	else
		req.head = &ep->rma_info.remote_reg_list;
	/* Does a valid window exist? */
	if ((err = micscif_query_window(&req))) {
		printk(KERN_ERR "%s %d err %d\n", 
				__func__, __LINE__, err);
		goto unlock_ret;
	}
	RMA_MAGIC(window);

#ifndef _MIC_SCIF_
	if (unlikely(is_self_scifdev(ep->remote_dev))) {
		void *dst_virt;
		if (RMA_WINDOW_SELF == type)
			dst_virt = get_local_va(offset, window,
						sizeof(uint32_t));
		else {
			struct page **pages = ((struct reg_range_t *)
				(window->peer_window))->pinned_pages->pages;
			int page_nr = (int) ( (offset - window->offset) >> PAGE_SHIFT );
			off_t page_off = offset & ~PAGE_MASK;
			dst_virt = (void *)((uint64_t)phys_to_virt(page_to_phys(
				pages[page_nr])) | page_off);
		}
		*(uint64_t*)dst_virt = val;
		goto unlock_ret;
	}
#endif
	phys = micscif_get_dma_addr(window, offset, NULL, NULL, NULL);
	if ((err = request_dma_channel(chan)))
		goto unlock_ret;
	err = do_status_update(chan, phys, val);
	free_dma_channel(chan);
unlock_ret:
	mutex_unlock(&ep->rma_info.rma_lock);
	return err;
}

/*
 * __micscif_kill_apps_with_mmaps:
 * @ep - The SCIF endpoint
 *
 * Kill the applications which have valid remote memory mappings
 * created via scif_mmap(..).
 */
static void __micscif_kill_apps_with_mmaps(struct endpt *ep)
{
	struct list_head *item;
	struct rma_task_info *info;

	spin_lock(&ep->lock);
	list_for_each(item, &ep->rma_info.task_list) {
		info = list_entry(item, struct rma_task_info, list_member);
		kill_pid(info->pid, SIGKILL, 1);
		pr_debug("%s ep %p pid %p ref %d\n", 
			__func__, ep, info->pid, info->ref_count);
	}
	spin_unlock(&ep->lock);
}

/*
 * _micscif_kill_apps_with_mmaps:
 * @node - remote node id.
 * @head - head of the list of endpoints to kill.
 *
 * Traverse the list of endpoints for a particular remote node and
 * kill applications with valid remote memory mappings.
 */
static void _micscif_kill_apps_with_mmaps(int node, struct list_head *head)
{
	struct endpt *ep;
	unsigned long sflags;
	struct list_head *item;

	spin_lock_irqsave(&ms_info.mi_connlock, sflags);
	list_for_each(item, head) {
		ep = list_entry(item, struct endpt, list);
		if (ep->remote_dev->sd_node == node)
			__micscif_kill_apps_with_mmaps(ep);
	}
	spin_unlock_irqrestore(&ms_info.mi_connlock, sflags);
}

/*
 * micscif_kill_apps_with_mmaps:
 * @node - remote node id.
 *
 * Wrapper for killing applications with valid remote memory mappings
 * for a particular node. This API is called by peer nodes as part of
 * handling a lost node.
 */
void micscif_kill_apps_with_mmaps(int node)
{
	_micscif_kill_apps_with_mmaps(node, &ms_info.mi_connected);
	_micscif_kill_apps_with_mmaps(node, &ms_info.mi_disconnected);
}

/*
 * micscif_query_apps_with_mmaps:
 * @node - remote node id.
 * @head - head of the list of endpoints to query.
 *
 * Query if any applications for a remote node have valid remote memory
 * mappings.
 */
static bool micscif_query_apps_with_mmaps(int node, struct list_head *head)
{
	struct endpt *ep;
	unsigned long sflags;
	struct list_head *item;
	bool ret = false;

	spin_lock_irqsave(&ms_info.mi_connlock, sflags);
	list_for_each(item, head) {
		ep = list_entry(item, struct endpt, list);
		if (ep->remote_dev->sd_node == node &&
			!list_empty(&ep->rma_info.task_list)) {
			ret = true;
			break;
		}
	}
	spin_unlock_irqrestore(&ms_info.mi_connlock, sflags);
	return ret;
}

/*
 * micscif_rma_do_apps_have_mmaps:
 * @node - remote node id.
 *
 * Wrapper for querying if any applications have remote memory mappings
 * for a particular node.
 */
bool micscif_rma_do_apps_have_mmaps(int node)
{
	return (micscif_query_apps_with_mmaps(node, &ms_info.mi_connected) ||
		micscif_query_apps_with_mmaps(node, &ms_info.mi_disconnected));
}

/*
 * __micscif_cleanup_rma_for_zombies:
 * @ep - The SCIF endpoint
 *
 * This API is only called while handling a lost node:
 * a) Remote node is dead.
 * b) All endpoints with remote memory mappings have been killed.
 * So we can traverse the remote_reg_list without any locks. Since
 * the window has not yet been unregistered we can drop the ref count
 * and queue it to the cleanup thread.
 */
static void __micscif_cleanup_rma_for_zombies(struct endpt *ep)
{
	struct list_head *pos, *tmp;
	struct reg_range_t *window;

	list_for_each_safe(pos, tmp, &ep->rma_info.remote_reg_list) {
		window = list_entry(pos, struct reg_range_t, list_member);
		/* If unregistration is complete then why is it on the list? */
		WARN_ON(window->unreg_state == OP_COMPLETED);
		if (window->ref_count)
			put_window_ref_count(window, window->nr_pages);
		if (!window->ref_count) {
			atomic_inc(&ep->rma_info.tw_refcount);
			atomic_add_return((int32_t)window->nr_pages, 
				&ep->rma_info.tw_total_pages);
			list_del(&window->list_member);
			micscif_queue_for_cleanup(window, &ms_info.mi_rma);
		}
	}
}

/*
 * micscif_cleanup_rma_for_zombies:
 * @node - remote node id.
 *
 * Cleanup remote registration lists for zombie endpoints.
 */
void micscif_cleanup_rma_for_zombies(int node)
{
	struct endpt *ep;
	unsigned long sflags;
	struct list_head *item;

	spin_lock_irqsave(&ms_info.mi_eplock, sflags);
	list_for_each(item, &ms_info.mi_zombie) {
		ep = list_entry(item, struct endpt, list);
		if (ep->remote_dev && ep->remote_dev->sd_node == node) {
			/*
			 * If the zombie endpoint remote node matches the lost
			 * node then the scifdev should not be alive.
			 */
			WARN_ON(scifdev_alive(ep));
			__micscif_cleanup_rma_for_zombies(ep);
		}
	}
	spin_unlock_irqrestore(&ms_info.mi_eplock, sflags);
}

/*
 * micscif_rma_get_task:
 *
 * Store the parent task struct and bump up the number of remote mappings.
 * If this is the first remote memory mapping for this endpoint then
 * create a new rma_task_info entry in the epd task list.
 */
int micscif_rma_get_task(struct endpt *ep, int nr_pages)
{
	struct list_head *item;
	struct rma_task_info *info;
	int err = 0;

	spin_lock(&ep->lock);
	list_for_each(item, &ep->rma_info.task_list) {
		info = list_entry(item, struct rma_task_info, list_member);
		if (info->pid == task_tgid(current)) {
			info->ref_count += nr_pages;
			pr_debug("%s ep %p existing pid %p ref %d\n", 
				__func__, ep, info->pid, info->ref_count);
			goto unlock;
		}
	}
	spin_unlock(&ep->lock);

	/* A new task is mapping this window. Create a new entry */
	if (!(info = kzalloc(sizeof(*info), GFP_KERNEL))) {
		err = -ENOMEM;
		goto done;
	}

	info->pid = get_pid(task_tgid(current));
	info->ref_count = nr_pages;
	pr_debug("%s ep %p new pid %p ref %d\n", 
		__func__, ep, info->pid, info->ref_count);
	spin_lock(&ep->lock);
	list_add_tail(&info->list_member, &ep->rma_info.task_list);
unlock:
	spin_unlock(&ep->lock);
done:
	return err;
}

/*
 * micscif_rma_put_task:
 *
 * Bump down the number of remote mappings. if the ref count for this
 * particular task drops to zero then remove the rma_task_info from
 * the epd task list.
 */
void micscif_rma_put_task(struct endpt *ep, int nr_pages)
{
	struct list_head *item;
	struct rma_task_info *info;

	spin_lock(&ep->lock);
	list_for_each(item, &ep->rma_info.task_list) {
		info = list_entry(item, struct rma_task_info, list_member);
		if (info->pid == task_tgid(current)) {
			info->ref_count -= nr_pages;
			pr_debug("%s ep %p pid %p ref %d\n", 
				__func__, ep, info->pid, info->ref_count);
			if (!info->ref_count) {
				list_del(&info->list_member);
				put_pid(info->pid);
				kfree(info);
			}
			goto done;
		}
	}
	/* Why was the task not found? This is a bug. */
	WARN_ON(1);
done:
	spin_unlock(&ep->lock);
	return;
}

/* Only debug API's below */
void micscif_display_window(struct reg_range_t *window, const char *s, int line)
{
	int j;

	printk("%s %d window %p type %d temp %d offset 0x%llx"
		" nr_pages 0x%llx nr_contig_chunks 0x%llx"
		" prot %d ref_count %d magic 0x%llx peer_window 0x%llx"
		" unreg_state 0x%x va_for_temp %p\n", 
		s, line, window, window->type, window->temp, 
		window->offset, window->nr_pages, window->nr_contig_chunks, 
		window->prot, window->ref_count, window->magic, 
		window->peer_window, window->unreg_state, window->va_for_temp);

	for (j = 0; j < window->nr_contig_chunks; j++)
		pr_debug("page[%d] = dma_addr 0x%llx num_pages 0x%x\n", 
			j, 
			window->dma_addr[j], 
			window->num_pages[j]);

	if (RMA_WINDOW_SELF == window->type && window->pinned_pages)
		for (j = 0; j < window->nr_pages; j++)
			pr_debug("page[%d] = pinned_pages %p address %p\n", 
				j, window->pinned_pages->pages[j], 
				page_address(window->pinned_pages->pages[j]));

#ifdef CONFIG_ML1OM
	if (window->temp_phys_addr)
		for (j = 0; j < window->nr_contig_chunks; j++)
			pr_debug("page[%d] = temp_phys_addr 0x%llx\n", 
				j, window->temp_phys_addr[j]);
	if (window->phys_addr)
		for (j = 0; j < window->nr_pages; j++)
			pr_debug("page[%d] = phys_addr 0x%llx\n", 
				j, window->phys_addr[j]);
#endif
	RMA_MAGIC(window);
}
