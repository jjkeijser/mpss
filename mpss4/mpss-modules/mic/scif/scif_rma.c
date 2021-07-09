/*
 * Intel MIC Platform Software Stack (MPSS)
 * Copyright (c) 2016, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * Intel SCIF driver.
 */
#include <linux/dma_remapping.h>
#include <linux/moduleparam.h>
#include <linux/pagemap.h>
#include "scif_main.h"
#include "scif_map.h"

#ifndef LEGACY_IOVA
/* IO virtual address start page frame number */
#define IOVA_START_PFN         (1)
#endif

static bool scif_ulimit_check = true;

module_param(scif_ulimit_check, bool, S_IRUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(scif_ulimit_check, "enable or disable scif ulimit check");

/*
 * The following functions come from iova.c functions and
 * got copied  in order to change the functionality of
 * reserve_iova() function.
 *
 * To be removed, when upstream accepts reserve_iova_exclusive.
 */
static void
iova_insert_rbtree(struct rb_root *root, struct iova *iova)
{
	struct rb_node **new = &(root->rb_node), *parent = NULL;
	/* Figure out where to put new node */
	while (*new) {
		struct iova *this = container_of(*new, struct iova, node);

		parent = *new;
		if (iova->pfn_lo < this->pfn_lo)
			new = &((*new)->rb_left);
		else if (iova->pfn_lo > this->pfn_lo)
			new = &((*new)->rb_right);
		else
			BUG(); /* this should not happen */
	}
	/* Add new node and rebalance tree. */
	rb_link_node(&iova->node, parent, new);
	rb_insert_color(&iova->node, root);
}

static struct iova *
__insert_new_range(struct iova_domain *iovad,
		unsigned long pfn_lo, unsigned long pfn_hi)
{
	struct iova *iova;

	iova = alloc_iova_mem();
	if (!iova)
		return iova;

	iova->pfn_hi = pfn_hi;
	iova->pfn_lo = pfn_lo;
	iova_insert_rbtree(&iovad->rbroot, iova);
	return iova;
}

static int
__is_range_overlap(struct rb_node *node,
		unsigned long pfn_lo, unsigned long pfn_hi)
{
	struct iova *iova = container_of(node, struct iova, node);

	if ((pfn_lo <= iova->pfn_hi) && (pfn_hi >= iova->pfn_lo))
		return 1;
	return 0;
}

/**
 * reserve_iova_exclusive - reserves an iova in the given range
 * @iovad: - iova domain pointer
 * @pfn_lo: - lower page frame address
 * @pfn_hi:- higher pfn adderss
 * This function allocates reserves the address range from pfn_lo to pfn_hi so
 * that this address is not dished out as part of alloc_iova.
 * Fails if any part of a requested range is already taken.
 */
static struct iova *
reserve_iova_exclusive(struct iova_domain *iovad,
		unsigned long pfn_lo, unsigned long pfn_hi)
{
	struct rb_node *node;
	unsigned long flags;
	struct iova *iova;

	spin_lock_irqsave(&iovad->iova_rbtree_lock, flags);
	for (node = rb_first(&iovad->rbroot); node; node = rb_next(node)) {
		if (__is_range_overlap(node, pfn_lo, pfn_hi)) {
			iova = NULL;
			goto finish;
		}
	}

	/* We are here either because this is the first reserver node
	 * or need to insert remaining non overlap addr range
	 */
	iova = __insert_new_range(iovad, pfn_lo, pfn_hi);
finish:

	spin_unlock_irqrestore(&iovad->iova_rbtree_lock, flags);
	return iova;
}

/**
 * scif_rma_ep_init:
 * @ep: end point
 *
 * Initialize RMA per EP data structures.
 */
void scif_rma_ep_init(struct scif_endpt *ep)
{
	struct scif_endpt_rma_info *rma = &ep->rma_info;

	mutex_init(&rma->rma_lock);
#ifndef LEGACY_IOVA
	init_iova_domain(&rma->iovad, PAGE_SIZE, SCIF_IOVA_START_PFN,
			 SCIF_DMA_64BIT_PFN);
#else
	init_iova_domain(&rma->iovad, SCIF_DMA_64BIT_PFN);
#endif
	spin_lock_init(&rma->tc_lock);
	mutex_init(&rma->mmn_lock);
	INIT_LIST_HEAD(&rma->reg_list);
	INIT_LIST_HEAD(&rma->remote_reg_list);
	atomic_set(&rma->tw_refcount, 0);
	atomic_set(&rma->tcw_refcount, 0);
	atomic_set(&rma->tcw_total_pages, 0);
	atomic_set(&rma->fence_refcount, 0);

	rma->async_list_del = 0;
	rma->dma_chan = NULL;
	INIT_LIST_HEAD(&rma->mmn_list);
	INIT_LIST_HEAD(&rma->vma_list);
	init_waitqueue_head(&rma->markwq);
}

/**
 * scif_rma_ep_can_uninit:
 * @ep: end point
 *
 * Returns 1 if an endpoint can be uninitialized and 0 otherwise.
 */
int scif_rma_ep_can_uninit(struct scif_endpt *ep)
{
	int ret = 0;

	mutex_lock(&ep->rma_info.rma_lock);
	/* Destroy RMA Info only if both lists are empty */
	if (list_empty(&ep->rma_info.reg_list) &&
	    list_empty(&ep->rma_info.remote_reg_list) &&
	    list_empty(&ep->rma_info.mmn_list) &&
	    !atomic_read(&ep->rma_info.tw_refcount) &&
	    !atomic_read(&ep->rma_info.tcw_refcount) &&
	    !atomic_read(&ep->rma_info.fence_refcount))
		ret = 1;
	mutex_unlock(&ep->rma_info.rma_lock);
	return ret;
}

/**
 * scif_create_pinned_pages:
 * @nr_pages: number of pages in window
 * @prot: read/write protection
 *
 * Allocate and prepare a set of pinned pages.
 */
static struct scif_pinned_pages *
scif_create_pinned_pages(int nr_pages, int prot)
{
	struct scif_pinned_pages *pin;

	might_sleep();
	pin = scif_zalloc(sizeof(*pin));
	if (!pin)
		goto error;

	pin->pages = scif_zalloc(nr_pages * sizeof(*pin->pages));
	if (!pin->pages)
		goto error_free_pinned_pages;

	pin->prot = prot;
	pin->magic = SCIFEP_MAGIC;
	return pin;

error_free_pinned_pages:
	scif_free(pin, sizeof(*pin));
error:
	return NULL;
}

/**
 * scif_destroy_pinned_pages:
 * @pin: A set of pinned pages.
 *
 * Deallocate resources for pinned pages.
 */
static int scif_destroy_pinned_pages(struct scif_pinned_pages *pin)
{
	int j;
	int writeable = pin->prot & SCIF_PROT_WRITE;
	int kernel = SCIF_MAP_KERNEL & pin->map_flags;

	for (j = 0; j < pin->nr_pages; j++) {
		if (pin->pages[j] && !kernel) {
			if (writeable)
				set_page_dirty_lock(pin->pages[j]);
			put_page(pin->pages[j]);
		}
	}

	scif_free(pin->pages,
		  pin->nr_pages * sizeof(*pin->pages));
	scif_free(pin, sizeof(*pin));
	return 0;
}

/*
 * scif_create_window:
 * @ep: end point
 * @nr_pages: number of pages
 * @offset: registration offset
 * @temp: true if a temporary window is being created
 *
 * Allocate and prepare a self registration window.
 */
struct scif_window *scif_create_window(struct scif_endpt *ep, int nr_pages,
				       s64 offset, bool temp)
{
	struct scif_window *window;

	might_sleep();
	window = scif_zalloc(sizeof(*window));
	if (!window)
		goto error;

	window->dma_addr = scif_zalloc(nr_pages * sizeof(*window->dma_addr));
	if (!window->dma_addr)
		goto error_free_window;

	window->num_pages = scif_zalloc(nr_pages * sizeof(*window->num_pages));
	if (!window->num_pages)
		goto error_free_window;

	window->offset = offset;
	window->ep = (u64)ep;
	window->magic = SCIFEP_MAGIC;
	window->reg_state = OP_IDLE;
	init_waitqueue_head(&window->regwq);
	INIT_LIST_HEAD(&window->list);
	window->type = SCIF_WINDOW_SELF;
	window->temp = temp;
	window->mapped_pages = 0;
	window->unreg_state = SCIF_UNREG_NOT_INITIATED;
	return window;

error_free_window:
	scif_free(window->dma_addr,
		  nr_pages * sizeof(*window->dma_addr));
	scif_free(window, sizeof(*window));
error:
	return NULL;
}

/**
 * scif_destroy_incomplete_window:
 * @ep: end point
 * @window: registration window
 *
 * Deallocate resources for self window.
 */
static void scif_destroy_incomplete_window(struct scif_endpt *ep,
					   struct scif_window *window)
{
	int err;
	int nr_pages = window->nr_pages;
	struct scif_allocmsg *alloc = &window->alloc_handle;
	struct scifmsg msg;

retry:
	/* Wait for a SCIF_ALLOC_GNT/REJ message */
	err = wait_event_timeout(alloc->allocwq,
				 alloc->state != OP_IN_PROGRESS,
				 SCIF_NODE_ALIVE_TIMEOUT);
	if (!err && scifdev_alive(ep))
		goto retry;

	mutex_lock(&ep->rma_info.rma_lock);
	if (alloc->state == OP_COMPLETED) {
		msg.uop = SCIF_FREE_VIRT;
		msg.src = ep->port;
		msg.payload[0] = ep->remote_ep;
		msg.payload[1] = window->alloc_handle.vaddr;
		msg.payload[2] = (u64)window;
		msg.payload[3] = SCIF_REGISTER;
		_scif_nodeqp_send(ep->remote_dev, &msg);
	}
	mutex_unlock(&ep->rma_info.rma_lock);

	scif_free_window_offset(ep, window, window->offset);
	scif_free(window->dma_addr, nr_pages * sizeof(*window->dma_addr));
	scif_free(window->num_pages, nr_pages * sizeof(*window->num_pages));
	scif_free(window, sizeof(*window));
}

/**
 * scif_unmap_window:
 * @remote_dev: SCIF remote device
 * @window: registration window
 *
 * Delete any DMA mappings created for a registered self window
 */
void scif_unmap_window(struct scif_dev *remote_dev, struct scif_window *window)
{
	int j;

	if (scif_is_iommu_enabled() && !scifdev_self(remote_dev)) {
		if (window->st) {
			int idx = remote_dev->dma_ch_idx;

			dma_unmap_sg(remote_dev->sdev->dma_ch[idx]->device->dev,
				     window->st->sgl, window->st->nents,
				     DMA_BIDIRECTIONAL);
			sg_free_table(window->st);
			kfree(window->st);
			window->st = NULL;
		}
	} else if (scif_is_mgmt_node()) {
		for (j = 0; j < window->nr_contig_chunks; j++) {
			if (window->dma_addr[j]) {
				scif_unmap_single(window->dma_addr[j],
						  remote_dev,
						  window->num_pages[j] <<
						  PAGE_SHIFT);
				window->dma_addr[j] = 0x0;
			}
		}
	}
}

/**
 * __scif_get_pid_mm
 * @pid: pid of task
 *
 * Returns %NULL if the pid has no mm.  Checks PF_KTHREAD is not set
 * and if so returns a reference to it, after bumping up the use count.
 * User must release the mm via mmput() after use.
 */
static inline struct mm_struct*
__scif_get_pid_mm(struct pid *pid)
{
	struct mm_struct *mm;
	struct task_struct *task;

	if (!pid)
		return NULL;

	task = get_pid_task(pid, PIDTYPE_PID);
	if (!task)
		return NULL;
	mm = get_task_mm(task);
	put_task_struct(task);

	return mm;
}

static inline int
__scif_dec_pinned_vm(struct mm_struct *mm, int nr_pages)
{
	if (!mm || !nr_pages)
		return 0;

	down_write(&mm->mmap_sem);
	mm->pinned_vm -= nr_pages;
	up_write(&mm->mmap_sem);
	return 0;
}

static inline int
__scif_try_inc_pinned_vm(struct mm_struct *mm, int nr_pages)
{
	unsigned long locked, lock_limit;

	if (!mm || !nr_pages)
		return 0;

	down_write(&mm->mmap_sem);
	locked = nr_pages;
	locked += mm->pinned_vm;
	lock_limit = rlimit(RLIMIT_MEMLOCK) >> PAGE_SHIFT;
	if ((locked > lock_limit) && !capable(CAP_IPC_LOCK)) {
		up_write(&mm->mmap_sem);
		dev_err_ratelimited(scif_info.mdev.this_device,
			"locked(%lu) > lock_limit(%lu) - " \
			"please increase memlock ulimit for process\n",
			locked, lock_limit);
		return -EAGAIN;
	}
	mm->pinned_vm = locked;
	up_write(&mm->mmap_sem);
	return 0;
}

/**
 * scif_destroy_window:
 * @ep: end point
 * @window: registration window
 *
 * Deallocate resources for self window.
 */
int scif_destroy_window(struct scif_endpt *ep, struct scif_window *window)
{
	int j;
	struct mm_struct *mm;
	struct scif_pinned_pages *pinned_pages = window->pinned_pages;
	int nr_pages = window->nr_pages;

	if (window->magic != SCIFEP_MAGIC) {
		dev_err(scif_info.mdev.this_device,
			"%s: Window %p corrupted, wrong magic = %llx\n",
			__func__, window, window->magic);
		return -1;
	}

	might_sleep();
	if (window->pid) {
		mm = __scif_get_pid_mm(window->pid);
		if (mm) {
			__scif_dec_pinned_vm(mm, window->nr_pages);
			mmput(mm);
		}
		put_pid(window->pid);
		window->pid = NULL;
	}

	scif_free_window_offset(ep, window, window->offset);
	scif_unmap_window(ep->remote_dev, window);
	/*
	 * Decrement references for this set of pinned pages from
	 * this window.
	 */
	j = atomic_sub_return(1, &pinned_pages->ref_count);
	if (j < 0) {
		dev_err(scif_info.mdev.this_device,
			"%s: Window %p corrupted, incorrect ref count %d\n",
			__func__, window, j);
		return -1;
	}

	/*
	 * If the ref count for pinned_pages is zero then someone
	 * has already called scif_unpin_pages() for it and we should
	 * destroy the page cache.
	 */
	if (!j)
		scif_destroy_pinned_pages(window->pinned_pages);

	scif_free(window->dma_addr, nr_pages * sizeof(*window->dma_addr));
	window->dma_addr = NULL;
	scif_free(window->num_pages, nr_pages * sizeof(*window->num_pages));
	window->num_pages = NULL;
	window->magic = 0;
	scif_free(window, sizeof(*window));
	return 0;
}

/**
 * scif_create_remote_lookup:
 * @remote_dev: SCIF remote device
 * @window: remote window
 *
 * Allocate and prepare lookup entries for the remote
 * end to copy over the physical addresses.
 * Returns 0 on success and appropriate errno on failure.
 */
static int scif_create_remote_lookup(struct scif_dev *remote_dev,
				     struct scif_window *window)
{
	int i, j, err = 0;
	int nr_pages = window->nr_pages;
	bool dma_phys_is_vmalloc, num_pages_is_vmalloc;

	might_sleep();
	/* Map window */
	err = scif_map_single(&window->mapped_offset,
			      window, remote_dev, sizeof(*window));
	if (err)
		goto error_window;

	/* Compute the number of lookup entries. 21 == 2MB Shift */
	window->nr_lookup = ALIGN(nr_pages * PAGE_SIZE,
					((2) * 1024 * 1024)) >> 21;

	window->dma_addr_lookup.lookup =
		scif_alloc_coherent(&window->dma_addr_lookup.offset,
				    remote_dev, window->nr_lookup *
				    sizeof(*window->dma_addr_lookup.lookup),
				    GFP_KERNEL | __GFP_ZERO);
	if (!window->dma_addr_lookup.lookup) {
		err = -ENOMEM;
		goto error_window;
	}

	window->num_pages_lookup.lookup =
		scif_alloc_coherent(&window->num_pages_lookup.offset,
				    remote_dev, window->nr_lookup *
				    sizeof(*window->num_pages_lookup.lookup),
				    GFP_KERNEL | __GFP_ZERO);
	if (!window->num_pages_lookup.lookup) {
		err = -ENOMEM;
		goto error_window;
	}

	dma_phys_is_vmalloc = is_vmalloc_addr(&window->dma_addr[0]);
	num_pages_is_vmalloc = is_vmalloc_addr(&window->num_pages[0]);

	/* Now map each of the pages containing physical addresses */
	for (i = 0, j = 0; i < nr_pages; i += SCIF_NR_ADDR_IN_PAGE, j++) {
		err = scif_map_page(&window->dma_addr_lookup.lookup[j],
				    dma_phys_is_vmalloc ?
				    vmalloc_to_page(&window->dma_addr[i]) :
				    virt_to_page(&window->dma_addr[i]),
				    remote_dev);
		if (err)
			goto error_window;
		err = scif_map_page(&window->num_pages_lookup.lookup[j],
				    num_pages_is_vmalloc ?
				    vmalloc_to_page(&window->num_pages[i]) :
				    virt_to_page(&window->num_pages[i]),
				    remote_dev);
		if (err)
			goto error_window;
	}
	return 0;
error_window:
	return err;
}

/**
 * scif_destroy_remote_lookup:
 * @remote_dev: SCIF remote device
 * @window: remote window
 *
 * Destroy lookup entries used for the remote
 * end to copy over the physical addresses.
 */
static void scif_destroy_remote_lookup(struct scif_dev *remote_dev,
				       struct scif_window *window)
{
	int i, j;

	if (window->nr_lookup) {
		struct scif_rma_lookup *lup = &window->dma_addr_lookup;
		struct scif_rma_lookup *npup = &window->num_pages_lookup;

		for (i = 0, j = 0; i < window->nr_pages;
			i += SCIF_NR_ADDR_IN_PAGE, j++) {
			if (lup->lookup && lup->lookup[j])
				scif_unmap_page(lup->lookup[j],
						  remote_dev,
						  PAGE_SIZE);
			if (npup->lookup && npup->lookup[j])
				scif_unmap_page(npup->lookup[j],
						  remote_dev,
						  PAGE_SIZE);
		}
		if (lup->lookup)
			scif_free_coherent(lup->lookup, lup->offset,
					   remote_dev, window->nr_lookup *
					   sizeof(*lup->lookup));
		if (npup->lookup)
			scif_free_coherent(npup->lookup, npup->offset,
					   remote_dev, window->nr_lookup *
					   sizeof(*npup->lookup));
		if (window->mapped_offset)
			scif_unmap_single(window->mapped_offset,
					  remote_dev, sizeof(*window));
		window->nr_lookup = 0;
	}
}

/**
 * scif_create_remote_window:
 * @ep: end point
 * @nr_pages: number of pages in window
 *
 * Allocate and prepare a remote registration window.
 */
static struct scif_window *
scif_create_remote_window(struct scif_dev *scifdev, int nr_pages)
{
	struct scif_window *window;

	might_sleep();
	window = scif_zalloc(sizeof(*window));
	if (!window)
		goto error_ret;

	window->magic = SCIFEP_MAGIC;
	window->nr_pages = nr_pages;

	window->dma_addr = scif_zalloc(nr_pages * sizeof(*window->dma_addr));
	if (!window->dma_addr)
		goto error_window;

	window->num_pages = scif_zalloc(nr_pages *
					sizeof(*window->num_pages));
	if (!window->num_pages)
		goto error_window;

	if (scif_create_remote_lookup(scifdev, window))
		goto error_window;

	window->type = SCIF_WINDOW_PEER;
	INIT_LIST_HEAD(&window->list);
	return window;
error_window:
	scif_destroy_remote_window(window, scifdev);
error_ret:
	return NULL;
}

/**
 * scif_destroy_remote_window:
 * @ep: end point
 * @window: remote registration window
 *
 * Deallocate resources for remote window.
 */
void
scif_destroy_remote_window(struct scif_window *window, struct scif_dev *remote_dev)
{
	scif_destroy_remote_lookup(remote_dev, window);
	scif_free(window->dma_addr, window->nr_pages *
		  sizeof(*window->dma_addr));
	scif_free(window->num_pages, window->nr_pages *
		  sizeof(*window->num_pages));
	window->magic = 0;
	scif_free(window, sizeof(*window));
}

/**
 * scif_dma_map: create DMA mappings if the IOMMU is enabled
 * @remote_dev: SCIF remote device
 * @window: remote registration window
 *
 * Map the physical pages using dma_map_sg(..) and then detect the number
 * of contiguous DMA mappings allocated
 */
static int scif_dma_map(struct scif_dev *remote_dev,
			  struct scif_window *window)
{
	struct scatterlist *sg;
	int i, err;
	int idx = remote_dev->dma_ch_idx;
	scif_pinned_pages_t pin = window->pinned_pages;

	window->st = kzalloc(sizeof(*window->st), GFP_KERNEL);
	if (!window->st)
		return -ENOMEM;

	err = sg_alloc_table(window->st, window->nr_pages, GFP_KERNEL);
	if (err)
		return err;

	for_each_sg(window->st->sgl, sg, window->st->nents, i)
		sg_set_page(sg, pin->pages[i], PAGE_SIZE, 0x0);

	err = dma_map_sg(remote_dev->sdev->dma_ch[idx]->device->dev,
			window->st->sgl, window->st->nents, DMA_BIDIRECTIONAL);
	if (!err)
		return -ENOMEM;
	/* Detect contiguous ranges of DMA mappings */
	sg = window->st->sgl;
	for (i = 0; sg; i++) {
		dma_addr_t last_da;

		window->dma_addr[i] = sg_dma_address(sg);
		window->num_pages[i] = sg_dma_len(sg) >> PAGE_SHIFT;
		last_da = sg_dma_address(sg) + sg_dma_len(sg);
		while ((sg = sg_next(sg)) && sg_dma_address(sg) == last_da) {
			window->num_pages[i] +=
				(sg_dma_len(sg) >> PAGE_SHIFT);
			last_da = window->dma_addr[i] +
				sg_dma_len(sg);
		}
		window->nr_contig_chunks++;
	}
	return 0;
}

/**
 * scif_map_window:
 * @remote_dev: SCIF remote device
 * @window: self registration window
 *
 * Map pages of a window into the aperture/PCI.
 * Also determine addresses required for DMA.
 */
int
scif_map_window(struct scif_dev *remote_dev, struct scif_window *window)
{
	int i, j, k, err = 0, nr_contig_pages;
	scif_pinned_pages_t pin;
	phys_addr_t phys_prev, phys_curr;

	might_sleep();

	pin = window->pinned_pages;

	if (intel_iommu_enabled && !scifdev_self(remote_dev))
		return scif_dma_map(remote_dev, window);

	for (i = 0, j = 0; i < window->nr_pages; i += nr_contig_pages, j++) {
		phys_prev = page_to_phys(pin->pages[i]);
		nr_contig_pages = 1;

		/* Detect physically contiguous chunks */
		for (k = i + 1; k < window->nr_pages; k++) {
			phys_curr = page_to_phys(pin->pages[k]);
			if (phys_curr != (phys_prev + PAGE_SIZE))
				break;
			phys_prev = phys_curr;
			nr_contig_pages++;
		}
		window->num_pages[j] = nr_contig_pages;
		window->nr_contig_chunks++;
		if (scif_is_mgmt_node()) {
			/*
			 * Management node has to deal with SMPT on X100 and
			 * hence the DMA mapping is required
			 */
			err = scif_map_single(&window->dma_addr[j],
					      phys_to_virt(page_to_phys(
							   pin->pages[i])),
					      remote_dev,
					      (s64)nr_contig_pages << PAGE_SHIFT);
			if (err)
				return err;
		} else {
			window->dma_addr[j] = page_to_phys(pin->pages[i]);
		}
	}
	return err;
}

/**
 * scif_send_scif_unregister:
 * @ep: end point
 * @window: self registration window
 *
 * Send a SCIF_UNREGISTER message.
 */
static int scif_send_scif_unregister(struct scif_endpt *ep,
				     struct scif_window *window)
{
	struct scifmsg msg;

	msg.uop = SCIF_UNREGISTER;
	msg.src = ep->port;
	msg.payload[0] = window->alloc_handle.vaddr;
	msg.payload[1] = (u64)window;
	return scif_nodeqp_send(ep->remote_dev, &msg);
}

/**
 * scif_unregister_window:
 * @window: self registration window
 *
 * Send an unregistration request and wait for a response.
 */
int scif_unregister_window(struct scif_window *window)
{
	int err = 0;
	struct scif_endpt *ep = (struct scif_endpt *)window->ep;

	if (window->unreg_state == SCIF_UNREG_NOT_INITIATED) {
		err = scif_send_scif_unregister(ep, window);
		if (err == 0)
			window->unreg_state = SCIF_UNREG_REQUESTED;
	}
	return err;
}

/**
 * scif_send_alloc_request:
 * @ep: end point
 * @window: self registration window
 *
 * Send a remote window allocation request
 */
static int scif_send_alloc_request(struct scif_endpt *ep,
				   struct scif_window *window)
{
	struct scifmsg msg;
	struct scif_allocmsg *alloc = &window->alloc_handle;

	/* Set up the Alloc Handle */
	alloc->state = OP_IN_PROGRESS;
	init_waitqueue_head(&alloc->allocwq);

	/* Send out an allocation request */
	msg.uop = SCIF_ALLOC_REQ;
	msg.payload[1] = window->nr_pages;
	msg.payload[2] = (u64)&window->alloc_handle;
	return _scif_nodeqp_send(ep->remote_dev, &msg);
}

/**
 * scif_prep_remote_window:
 * @ep: end point
 * @window: self registration window
 *
 * Send a remote window allocation request, wait for an allocation response,
 * and prepares the remote window by copying over the page lists
 */
static int scif_prep_remote_window(struct scif_endpt *ep,
				   struct scif_window *window)
{
	struct scifmsg msg;
	struct scif_window *remote_window;
	struct scif_allocmsg *alloc = &window->alloc_handle;
	dma_addr_t *dma_phys_lookup, *tmp, *num_pages_lookup, *tmp1;
	int i = 0, j = 0;
	int nr_contig_chunks, loop_nr_contig_chunks;
	int remaining_nr_contig_chunks, nr_lookup;
	int err, map_err;

	map_err = scif_map_window(ep->remote_dev, window);
	if (map_err)
		dev_err(&ep->remote_dev->sdev->dev,
			"%s %d map_err %d\n", __func__, __LINE__, map_err);
	remaining_nr_contig_chunks = window->nr_contig_chunks;
	nr_contig_chunks = window->nr_contig_chunks;
retry:
	/* Wait for a SCIF_ALLOC_GNT/REJ message */
	err = wait_event_timeout(alloc->allocwq,
				 alloc->state != OP_IN_PROGRESS,
				 SCIF_NODE_ALIVE_TIMEOUT);
	mutex_lock(&ep->rma_info.rma_lock);
	/* Synchronize with the thread waking up allocwq */
	mutex_unlock(&ep->rma_info.rma_lock);
	if (!err && scifdev_alive(ep))
		goto retry;

	if (!err)
		err = -ENODEV;

	if (err > 0)
		err = 0;
	else
		return err;

	/* Bail out. The remote end rejected this request */
	if (alloc->state == OP_FAILED)
		return -ENOMEM;

	if (map_err) {
		dev_err(&ep->remote_dev->sdev->dev,
			"%s %d err %d\n", __func__, __LINE__, map_err);
		msg.uop = SCIF_FREE_VIRT;
		msg.src = ep->port;
		msg.payload[0] = ep->remote_ep;
		msg.payload[1] = window->alloc_handle.vaddr;
		msg.payload[2] = (u64)window;
		msg.payload[3] = SCIF_REGISTER;
		spin_lock(&ep->lock);
		if (ep->state == SCIFEP_CONNECTED)
			err = _scif_nodeqp_send(ep->remote_dev, &msg);
		else
			err = -ENOTCONN;
		spin_unlock(&ep->lock);
		return err;
	}

	remote_window = scif_ioremap(alloc->phys_addr, sizeof(*window),
				     ep->remote_dev);

	/* Compute the number of lookup entries. 21 == 2MB Shift */
	nr_lookup = ALIGN(nr_contig_chunks, SCIF_NR_ADDR_IN_PAGE)
			  >> ilog2(SCIF_NR_ADDR_IN_PAGE);

	dma_phys_lookup =
		scif_ioremap(remote_window->dma_addr_lookup.offset,
			     nr_lookup *
			     sizeof(*remote_window->dma_addr_lookup.lookup),
			     ep->remote_dev);
	num_pages_lookup =
		scif_ioremap(remote_window->num_pages_lookup.offset,
			     nr_lookup *
			     sizeof(*remote_window->num_pages_lookup.lookup),
			     ep->remote_dev);

	while (remaining_nr_contig_chunks) {
		loop_nr_contig_chunks = min_t(int, remaining_nr_contig_chunks,
					      (int)SCIF_NR_ADDR_IN_PAGE);
		/* #1/2 - Copy  physical addresses over to the remote side */

		/* #2/2 - Copy DMA addresses (addresses that are fed into the
		 * DMA engine) We transfer bus addresses which are then
		 * converted into a MIC physical address on the remote
		 * side if it is a MIC, if the remote node is a mgmt node we
		 * transfer the MIC physical address
		 */
		tmp = scif_ioremap(dma_phys_lookup[j],
				   loop_nr_contig_chunks *
				   sizeof(*window->dma_addr),
				   ep->remote_dev);
		tmp1 = scif_ioremap(num_pages_lookup[j],
				    loop_nr_contig_chunks *
				    sizeof(*window->num_pages),
				    ep->remote_dev);
		if (scif_is_mgmt_node()) {
			memcpy_toio((void __force __iomem *)tmp,
				    &window->dma_addr[i], loop_nr_contig_chunks
				    * sizeof(*window->dma_addr));
			memcpy_toio((void __force __iomem *)tmp1,
				    &window->num_pages[i], loop_nr_contig_chunks
				    * sizeof(*window->num_pages));
		} else {
			if (scifdev_is_p2p(ep->remote_dev)) {
				/*
				 * add remote node's base address for this node
				 * to convert it into a MIC address
				 */
				int m;
				dma_addr_t dma_addr;

				for (m = 0; m < loop_nr_contig_chunks; m++) {
					dma_addr = window->dma_addr[i + m] +
						ep->remote_dev->base_addr;
					writeq(dma_addr,
					       (void __force __iomem *)&tmp[m]);
				}
				memcpy_toio((void __force __iomem *)tmp1,
					    &window->num_pages[i],
					    loop_nr_contig_chunks
					    * sizeof(*window->num_pages));
			} else {
				/* Mgmt node or loopback - transfer DMA
				 * addresses as is, this is the same as a
				 * MIC physical address (we use the dma_addr
				 * and not the phys_addr array since the
				 * phys_addr is only setup if there is a mmap()
				 * request from the mgmt node)
				 */
				memcpy_toio((void __force __iomem *)tmp,
					    &window->dma_addr[i],
					    loop_nr_contig_chunks *
					    sizeof(*window->dma_addr));
				memcpy_toio((void __force __iomem *)tmp1,
					    &window->num_pages[i],
					    loop_nr_contig_chunks *
					    sizeof(*window->num_pages));
			}
		}
		remaining_nr_contig_chunks -= loop_nr_contig_chunks;
		i += loop_nr_contig_chunks;
		j++;
		scif_iounmap(tmp, loop_nr_contig_chunks *
			     sizeof(*window->dma_addr), ep->remote_dev);
		scif_iounmap(tmp1, loop_nr_contig_chunks *
			     sizeof(*window->num_pages), ep->remote_dev);
	}

	/* Prepare the remote window for the peer */
	remote_window->peer_window = (u64)window;
	remote_window->offset = window->offset;
	remote_window->prot = window->prot;
	remote_window->nr_contig_chunks = nr_contig_chunks;
	remote_window->ep = ep->remote_ep;
	remote_window->mapped_pages = 0;
	remote_window->unreg_state = SCIF_UNREG_NOT_INITIATED;
	scif_iounmap(num_pages_lookup,
		     nr_lookup *
		     sizeof(*remote_window->num_pages_lookup.lookup),
		     ep->remote_dev);
	scif_iounmap(dma_phys_lookup,
		     nr_lookup *
		     sizeof(*remote_window->dma_addr_lookup.lookup),
		     ep->remote_dev);
	scif_iounmap(remote_window, sizeof(*remote_window), ep->remote_dev);
	window->peer_window = alloc->vaddr;
	return err;
}

/**
 * scif_send_scif_register:
 * @ep: end point
 * @window: self registration window
 *
 * Send a SCIF_REGISTER message if EP is connected and wait for a
 * SCIF_REGISTER_(N)ACK message else send a SCIF_FREE_VIRT
 * message so that the peer can free its remote window allocated earlier.
 */
static int scif_send_scif_register(struct scif_endpt *ep,
				   struct scif_window *window)
{
	int err = 0;
	struct scifmsg msg;

	msg.src = ep->port;
	msg.payload[0] = ep->remote_ep;
	msg.payload[1] = window->alloc_handle.vaddr;
	msg.payload[2] = (u64)window;
	spin_lock(&ep->lock);
	if (ep->state == SCIFEP_CONNECTED) {
		msg.uop = SCIF_REGISTER;
		window->reg_state = OP_IN_PROGRESS;
		err = _scif_nodeqp_send(ep->remote_dev, &msg);
		spin_unlock(&ep->lock);
		if (!err) {
retry:
			/* Wait for a SCIF_REGISTER_(N)ACK message */
			err = wait_event_timeout(window->regwq,
						 window->reg_state !=
						 OP_IN_PROGRESS,
						 SCIF_NODE_ALIVE_TIMEOUT);
			if (!err && scifdev_alive(ep))
				goto retry;
			err = !err ? -ENODEV : 0;
			if (window->reg_state == OP_FAILED)
				err = -ENOTCONN;
		}
	} else {
		msg.uop = SCIF_FREE_VIRT;
		msg.payload[3] = SCIF_REGISTER;
		err = _scif_nodeqp_send(ep->remote_dev, &msg);
		spin_unlock(&ep->lock);
		if (!err)
			err = -ENOTCONN;
	}
	return err;
}

/**
 * scif_get_window_offset:
 * @ep: end point descriptor
 * @flags: flags
 * @offset: offset hint
 * @num_pages: number of pages
 * @out_offset: computed offset returned by reference.
 *
 * Compute/Claim a new offset for this EP.
 */
int scif_get_window_offset(struct scif_endpt *ep, int flags, s64 offset,
			   int num_pages, s64 *out_offset)
{
	s64 page_index;
	struct iova *iova_ptr;
	int err = 0;

	if (flags & SCIF_MAP_FIXED) {
		page_index = SCIF_IOVA_PFN(offset);
		iova_ptr = reserve_iova_exclusive(&ep->rma_info.iovad,
				page_index, page_index + num_pages - 1);
		if (!iova_ptr)
			err = -EADDRINUSE;
	} else {
		iova_ptr = alloc_iova(&ep->rma_info.iovad, num_pages,
				      SCIF_DMA_63BIT_PFN - 1, 0);
		if (!iova_ptr)
			err = -ENOMEM;
	}
	if (!err)
		*out_offset = (iova_ptr->pfn_lo) << PAGE_SHIFT;
	return err;
}

/**
 * scif_free_window_offset:
 * @ep: end point descriptor
 * @window: registration window
 * @offset: Offset to be freed
 *
 * Free offset for this EP. The callee is supposed to grab
 * the RMA mutex before calling this API.
 */
void scif_free_window_offset(struct scif_endpt *ep,
			     struct scif_window *window, s64 offset)
{
	if ((window && !window->offset_freed) || !window) {
		free_iova(&ep->rma_info.iovad, offset >> PAGE_SHIFT);
		if (window)
			window->offset_freed = true;
	}
}

/**
 * scif_alloc_req: Respond to SCIF_ALLOC_REQ interrupt message
 * @msg:        Interrupt message
 *
 * Remote side is requesting a memory allocation.
 */
void scif_alloc_req(struct scif_dev *scifdev, struct scifmsg *msg)
{
	int err;
	struct scif_window *window = NULL;
	int nr_pages = msg->payload[1];

	window = scif_create_remote_window(scifdev, nr_pages);
	if (!window) {
		err = -ENOMEM;
		goto error;
	}

	/* The peer's allocation request is granted */
	msg->uop = SCIF_ALLOC_GNT;
	msg->payload[0] = (u64)window;
	msg->payload[1] = window->mapped_offset;
	err = scif_nodeqp_send(scifdev, msg);
	if (err)
		scif_destroy_remote_window(window, scifdev);
	return;
error:
	/* The peer's allocation request is rejected */
	dev_err(&scifdev->sdev->dev,
		"%s %d error %d alloc_ptr %p nr_pages 0x%x\n",
		__func__, __LINE__, err, window, nr_pages);
	msg->uop = SCIF_ALLOC_REJ;
	scif_nodeqp_send(scifdev, msg);
}

/**
 * scif_alloc_gnt_rej: Respond to SCIF_ALLOC_GNT/REJ interrupt message
 * @msg:        Interrupt message
 *
 * Remote side responded to a memory allocation.
 */
void scif_alloc_gnt_rej(struct scif_dev *scifdev, struct scifmsg *msg)
{
	struct scif_allocmsg *handle = (struct scif_allocmsg *)msg->payload[2];
	struct scif_window *window = container_of(handle, struct scif_window,
						  alloc_handle);
	struct scif_endpt *ep = (struct scif_endpt *)window->ep;

	mutex_lock(&ep->rma_info.rma_lock);
	handle->vaddr = msg->payload[0];
	handle->phys_addr = msg->payload[1];
	if (msg->uop == SCIF_ALLOC_GNT)
		handle->state = OP_COMPLETED;
	else
		handle->state = OP_FAILED;
	wake_up(&handle->allocwq);
	mutex_unlock(&ep->rma_info.rma_lock);
}

/**
 * scif_free_virt: Respond to SCIF_FREE_VIRT interrupt message
 * @msg:        Interrupt message
 *
 * Free up memory kmalloc'd earlier.
 */
void scif_free_virt(struct scif_dev *scifdev, struct scifmsg *msg)
{
	struct scif_window *window = (struct scif_window *)msg->payload[1];

	scif_destroy_remote_window(window, scifdev);
}

static void
scif_fixup_aper_base(struct scif_dev *dev, struct scif_window *window)
{
	int j;
	struct scif_hw_dev *sdev = dev->sdev;
	phys_addr_t apt_base = 0;

	/*
	 * Add the aperture base if the DMA address is not card relative
	 * since the DMA addresses need to be an offset into the bar
	 */
	if (!scifdev_self(dev) && window->type == SCIF_WINDOW_PEER &&
	    sdev->aper && !sdev->card_rel_da)
		apt_base = sdev->aper->pa;
	else
		return;

	for (j = 0; j < window->nr_contig_chunks; j++) {
		if (window->num_pages[j])
			window->dma_addr[j] += apt_base;
		else
			break;
	}
}

/**
 * scif_recv_reg: Respond to SCIF_REGISTER interrupt message
 * @msg:        Interrupt message
 *
 * Update remote window list with a new registered window.
 */
void scif_recv_reg(struct scif_dev *scifdev, struct scifmsg *msg)
{
	struct scif_endpt *ep = (struct scif_endpt *)msg->payload[0];
	struct scif_window *window =
		(struct scif_window *)msg->payload[1];

	mutex_lock(&ep->rma_info.rma_lock);
	spin_lock(&ep->lock);
	if (ep->state == SCIFEP_CONNECTED) {
		msg->uop = SCIF_REGISTER_ACK;
		scif_nodeqp_send(ep->remote_dev, msg);
		scif_fixup_aper_base(ep->remote_dev, window);
		/* No further failures expected. Insert new window */
		scif_insert_window(window, &ep->rma_info.remote_reg_list);
	} else {
		msg->uop = SCIF_REGISTER_NACK;
		scif_nodeqp_send(ep->remote_dev, msg);
	}
	spin_unlock(&ep->lock);
	mutex_unlock(&ep->rma_info.rma_lock);
	/* free up any lookup resources now that page lists are transferred */
	scif_destroy_remote_lookup(ep->remote_dev, window);
	/*
	 * We could not insert the window but we need to
	 * destroy the window.
	 */
	if (msg->uop == SCIF_REGISTER_NACK)
		scif_destroy_remote_window(window, scifdev);
}

/**
 * scif_recv_unreg: Respond to SCIF_UNREGISTER interrupt message
 * @msg:        Interrupt message
 *
 * Remove window from remote registration list;
 */
void scif_recv_unreg(struct scif_dev *scifdev, struct scifmsg *msg)
{
	struct scif_rma_req req;
	struct scif_window *window = NULL;
	struct scif_window *recv_window =
		(struct scif_window *)msg->payload[0];
	struct scif_endpt *ep;

	ep = (struct scif_endpt *)recv_window->ep;
	req.out_window = &window;
	req.offset = recv_window->offset;
	req.prot = 0;
	req.nr_bytes = recv_window->nr_pages << PAGE_SHIFT;
	req.type = SCIF_WINDOW_FULL;
	req.head = &ep->rma_info.remote_reg_list;

	mutex_lock(&ep->rma_info.rma_lock);
	/* Does a valid window exist? */
	if (scif_query_window(&req)) {
		dev_err(&scifdev->sdev->dev,
			"%s %d -ENXIO\n", __func__, __LINE__);
		goto error;
	}

	if (window) {
		window->unreg_state = SCIF_UNREG_REQUESTED;
		msg->payload[0] = ep->remote_ep;
		msg->payload[1] = window->peer_window;
		if (window->mapped_pages == 0) {
			msg->uop = SCIF_DELETE_WINDOW;
			atomic_inc(&ep->rma_info.tw_refcount);
			ep->rma_info.async_list_del = 1;
			list_del_init(&window->list);
			scif_drain_dma_intr(ep->remote_dev->sdev,
			    ep->rma_info.dma_chan);
			scif_nodeqp_send(ep->remote_dev, msg);
			scif_queue_for_cleanup(window, &scif_info.rma);
		} else {
			msg->uop = SCIF_USED_WINDOW;
			scif_nodeqp_send(ep->remote_dev, msg);
		}
	} else {
		/* The window did not make its way to the list at all. ACK */
		scif_destroy_remote_window(recv_window, scifdev);
	}

error:
	mutex_unlock(&ep->rma_info.rma_lock);
}

/**
 * scif_recv_reg_ack: Respond to SCIF_REGISTER_ACK interrupt message
 * @msg:        Interrupt message
 *
 * Wake up the window waiting to complete registration.
 */
void scif_recv_reg_ack(struct scif_dev *scifdev, struct scifmsg *msg)
{
	struct scif_window *window =
		(struct scif_window *)msg->payload[2];
	struct scif_endpt *ep = (struct scif_endpt *)window->ep;

	mutex_lock(&ep->rma_info.rma_lock);
	window->reg_state = OP_COMPLETED;
	wake_up(&window->regwq);
	mutex_unlock(&ep->rma_info.rma_lock);
}

/**
 * scif_recv_reg_nack: Respond to SCIF_REGISTER_NACK interrupt message
 * @msg:        Interrupt message
 *
 * Wake up the window waiting to inform it that registration
 * cannot be completed.
 */
void scif_recv_reg_nack(struct scif_dev *scifdev, struct scifmsg *msg)
{
	struct scif_window *window =
		(struct scif_window *)msg->payload[2];
	struct scif_endpt *ep = (struct scif_endpt *)window->ep;

	mutex_lock(&ep->rma_info.rma_lock);
	window->reg_state = OP_FAILED;
	wake_up(&window->regwq);
	mutex_unlock(&ep->rma_info.rma_lock);
}

/**
 * scif_recv_del_window: Respond to SCIF_DELETE_WINDOW interrupt message
 * @msg:        Interrupt message
 *
 * Wake up the window waiting to complete unregistration and delete it.
 */
void scif_recv_del_window(struct scif_dev *scifdev, struct scifmsg *msg)
{
	struct mm_struct *mm;
	struct scif_window *window =
		(struct scif_window *)msg->payload[1];
	struct scif_endpt *ep = (struct scif_endpt *)window->ep;

	mutex_lock(&ep->rma_info.rma_lock);
	window->unreg_state = SCIF_UNREG_ACCEPTED;
	atomic_inc(&ep->rma_info.tw_refcount);
	list_del_init(&window->list);
	scif_free_window_offset(ep, window, window->offset);
	mutex_unlock(&ep->rma_info.rma_lock);
	if ((!!(window->pinned_pages->map_flags & SCIF_MAP_KERNEL)) &&
	    scifdev_alive(ep)) {
		scif_drain_dma_intr(ep->remote_dev->sdev,
				    ep->rma_info.dma_chan);
	} else if (window->pid) {
		mm = __scif_get_pid_mm(window->pid);
		if (mm) {
			__scif_dec_pinned_vm(mm, window->nr_pages);
			mmput(mm);
		}
		put_pid(window->pid);
		window->pid = NULL;
	}
	scif_queue_for_cleanup(window, &scif_info.rma);
}

/**
 * scif_used_window: Respond to SCIF_USED_WINDOW interrupt message
 * @msg:        Interrupt message
 *
 * Wake up the window waiting to complete unregistration and delete it.
 */
void scif_recv_used_window(struct scif_dev *scifdev, struct scifmsg *msg)
{
	struct scif_window *window =
		(struct scif_window *)msg->payload[1];
	struct scif_endpt *ep = (struct scif_endpt *)window->ep;

	mutex_lock(&ep->rma_info.rma_lock);
	window->unreg_state = SCIF_UNREG_DENIED;
	mutex_unlock(&ep->rma_info.rma_lock);
}

int __scif_pin_pages(void *addr, size_t len, int *out_prot, int map_flags,
		     scif_pinned_pages_t *pages, struct scif_window *window)
{
	struct scif_pinned_pages *pinned_pages;
	int nr_pages, err = 0, i;
	bool vmalloc_addr = false;
	bool try_upgrade = false;
	int prot = *out_prot;
	struct pid *pid = NULL;

	/* Unsupported flags */
	if (map_flags & ~SCIF_MAP_KERNEL)
		return -EINVAL;

	/* Unsupported protection requested */
	if (prot & ~(SCIF_PROT_READ | SCIF_PROT_WRITE))
		return -EINVAL;

	/* addr/len must be page aligned. len should be non zero */
	if (!len ||
	    (ALIGN((u64)addr, PAGE_SIZE) != (u64)addr) ||
	    (ALIGN((u64)len, PAGE_SIZE) != (u64)len))
		return -EINVAL;

	/*
	 * Get pid if pinned pages shall be tracked by ulimit.
	 * Don't check ulimit if any of the following is true:
	 * - the ulimit check is disabled
	 * - no window is associated
	 * - it is a kernel thread
	 */
	if (scif_ulimit_check && window && !(map_flags & SCIF_MAP_KERNEL)
			&& !(current->flags & PF_KTHREAD))
		pid = get_task_pid(current, PIDTYPE_PID);

	might_sleep();

	nr_pages = len >> PAGE_SHIFT;

	/* Allocate a set of pinned pages */
	pinned_pages = scif_create_pinned_pages(nr_pages, prot);
	if (!pinned_pages)
		return -ENOMEM;

	if (map_flags & SCIF_MAP_KERNEL) {
		if (is_vmalloc_addr(addr))
			vmalloc_addr = true;

		for (i = 0; i < nr_pages; i++) {
			if (vmalloc_addr)
				pinned_pages->pages[i] =
					vmalloc_to_page(addr + (i * PAGE_SIZE));
			else
				pinned_pages->pages[i] =
					virt_to_page(addr + (i * PAGE_SIZE));
		}
		pinned_pages->nr_pages = nr_pages;
		pinned_pages->map_flags = SCIF_MAP_KERNEL;
	} else {
		if (pid) {
			err = __scif_try_inc_pinned_vm(current->mm, nr_pages);
			if (err)
				goto error_unmap;
		}

		/*
		 * SCIF supports registration caching. If a registration has
		 * been requested with read only permissions, then we try
		 * to pin the pages with RW permissions so that a subsequent
		 * transfer with RW permission can hit the cache instead of
		 * invalidating it. If the upgrade fails with RW then we
		 * revert back to R permission and retry
		 */
		if (prot == SCIF_PROT_READ)
			try_upgrade = true;
		prot |= SCIF_PROT_WRITE;
retry:
		pinned_pages->nr_pages = get_user_pages_fast(
				(u64)addr,
				nr_pages,
				!!(prot & SCIF_PROT_WRITE),
				pinned_pages->pages);

		if ((nr_pages != pinned_pages->nr_pages) && try_upgrade) {
			/* Roll back any pinned pages */
			for (i = 0; i < pinned_pages->nr_pages; i++) {
				if (pinned_pages->pages[i]) {
					put_page(pinned_pages->pages[i]);
					pinned_pages->pages[i] = NULL;
				}
			}

			prot &= ~SCIF_PROT_WRITE;
			try_upgrade = false;
			goto retry;
		}
		pinned_pages->map_flags = 0;

		if (pinned_pages->nr_pages < nr_pages) {
			if (pid)
				__scif_dec_pinned_vm(current->mm, nr_pages);
			err = -EFAULT;
			goto error_unmap;
		}
	}

	if (pid)
		window->pid = pid;
	*out_prot = prot;
	atomic_set(&pinned_pages->ref_count, 1);
	*pages = pinned_pages;
	return err;
	/* Something went wrong! Rollback */
error_unmap:
	if (pid)
		put_pid(pid);
	pinned_pages->nr_pages = nr_pages;
	scif_destroy_pinned_pages(pinned_pages);
	*pages = NULL;
	dev_dbg(scif_info.mdev.this_device,
		"%s %d err %d len 0x%lx\n", __func__, __LINE__, err, len);
	return err;
}

int scif_pin_pages(void *addr, size_t len, int prot,
		   int map_flags, scif_pinned_pages_t *pages)
{
	return __scif_pin_pages(addr, len, &prot, map_flags, pages, NULL);
}
EXPORT_SYMBOL_GPL(scif_pin_pages);

int scif_unpin_pages(scif_pinned_pages_t pinned_pages)
{
	int err = 0, ret;

	if (!pinned_pages || SCIFEP_MAGIC != pinned_pages->magic)
		return -EINVAL;

	ret = atomic_sub_return(1, &pinned_pages->ref_count);
	if (ret < 0) {
		dev_err(scif_info.mdev.this_device,
			"%s %d scif_unpin_pages called without pinning? rc %d\n",
			__func__, __LINE__, ret);
		return -EINVAL;
	}
	/*
	 * Destroy the window if the ref count for this set of pinned
	 * pages has dropped to zero. If it is positive then there is
	 * a valid registered window which is backed by these pages and
	 * it will be destroyed once all such windows are unregistered.
	 */
	if (!ret)
		err = scif_destroy_pinned_pages(pinned_pages);

	return err;
}
EXPORT_SYMBOL_GPL(scif_unpin_pages);

static inline void
scif_insert_local_window(struct scif_window *window, struct scif_endpt *ep)
{
	mutex_lock(&ep->rma_info.rma_lock);
	scif_insert_window(window, &ep->rma_info.reg_list);
	mutex_unlock(&ep->rma_info.rma_lock);
}

off_t scif_register_pinned_pages(scif_epd_t epd,
				 scif_pinned_pages_t pinned_pages,
				 off_t offset, int map_flags)
{
	struct scif_endpt *ep = (struct scif_endpt *)epd;
	s64 computed_offset;
	struct scif_window *window;
	int err;
	size_t len;
	struct device *spdev;

	/* Unsupported flags */
	if (map_flags & ~SCIF_MAP_FIXED)
		return -EINVAL;

	len = pinned_pages->nr_pages << PAGE_SHIFT;

	/*
	 * Offset is not page aligned/negative or offset+len
	 * wraps around with SCIF_MAP_FIXED.
	 */
	if ((map_flags & SCIF_MAP_FIXED) &&
	    ((ALIGN(offset, PAGE_SIZE) != offset) ||
	    (offset < 0) ||
	    (offset + (off_t)len < offset)))
		return -EINVAL;

	might_sleep();

	err = scif_verify_epd(ep);
	if (err)
		return err;
	/*
	 * It is an error to pass pinned_pages to scif_register_pinned_pages()
	 * after calling scif_unpin_pages().
	 */
	if (!atomic_add_unless(&pinned_pages->ref_count, 1, 0))
		return -EINVAL;

	/* Compute the offset for this registration */
	err = scif_get_window_offset(ep, map_flags, offset,
				     len, &computed_offset);
	if (err) {
		atomic_sub(1, &pinned_pages->ref_count);
		return err;
	}

	/* Allocate and prepare self registration window */
	window = scif_create_window(ep, pinned_pages->nr_pages,
				    computed_offset, false);
	if (!window) {
		atomic_sub(1, &pinned_pages->ref_count);
		scif_free_window_offset(ep, NULL, computed_offset);
		return -ENOMEM;
	}

	window->pinned_pages = pinned_pages;
	window->nr_pages = pinned_pages->nr_pages;
	window->prot = pinned_pages->prot;

	spdev = scif_get_peer_dev(ep->remote_dev);
	if (!spdev) {
		scif_destroy_window(ep, window);
		return -ENODEV;
	}
	err = scif_send_alloc_request(ep, window);
	if (err) {
		dev_err(&ep->remote_dev->sdev->dev,
			"%s %d err %d\n", __func__, __LINE__, err);
		goto error_unmap;
	}

	/* Prepare the remote registration window */
	err = scif_prep_remote_window(ep, window);
	if (err) {
		dev_err(&ep->remote_dev->sdev->dev,
			"%s %d err %d\n", __func__, __LINE__, err);
		goto error_unmap;
	}

	/* Tell the peer about the new window */
	err = scif_send_scif_register(ep, window);
	if (err) {
		dev_err(&ep->remote_dev->sdev->dev,
			"%s %d err %d\n", __func__, __LINE__, err);
		goto error_unmap;
	}

	scif_put_peer_dev(spdev);
	/* No further failures expected. Insert new window */
	scif_insert_local_window(window, ep);
	return computed_offset;
error_unmap:
	scif_destroy_window(ep, window);
	scif_put_peer_dev(spdev);
	dev_err(&ep->remote_dev->sdev->dev,
		"%s %d err %d\n", __func__, __LINE__, err);
	return err;
}
EXPORT_SYMBOL_GPL(scif_register_pinned_pages);

off_t scif_register(scif_epd_t epd, void *addr, size_t len, off_t offset,
		    int prot, int map_flags)
{
	scif_pinned_pages_t pinned_pages;
	off_t err;
	struct scif_endpt *ep = (struct scif_endpt *)epd;
	s64 computed_offset;
	struct scif_window *window = NULL;
	struct device *spdev;
	unsigned long timeout = 0;

	dev_dbg(scif_info.mdev.this_device,
		"SCIFAPI register: ep %p addr %p len 0x%lx offset 0x%lx prot 0x%x map_flags 0x%x\n",
		epd, addr, len, offset, prot, map_flags);
	/* Unsupported flags */
	if (map_flags & ~(SCIF_MAP_FIXED | SCIF_MAP_KERNEL))
		return -EINVAL;

	/*
	 * Offset is not page aligned/negative or offset+len
	 * wraps around with SCIF_MAP_FIXED.
	 */
	if ((map_flags & SCIF_MAP_FIXED) &&
	    ((ALIGN(offset, PAGE_SIZE) != offset) ||
	    (offset < 0) ||
	    (offset + (off_t)len < offset)))
		return -EINVAL;

	/* Unsupported protection requested */
	if (prot & ~(SCIF_PROT_READ | SCIF_PROT_WRITE))
		return -EINVAL;

	/* addr/len must be page aligned. len should be non zero */
	if ((!len) ||
		(ALIGN((u64)addr, PAGE_SIZE) != (u64)addr) ||
		(ALIGN((u64)len, PAGE_SIZE) != (u64)len))
			return -EINVAL;

	might_sleep();

	err = scif_verify_epd(ep);
	if (err)
		return err;

	timeout = jiffies + SCIF_NODE_ALIVE_TIMEOUT;
get_offset:
	/* Compute the offset for this registration */
	err = scif_get_window_offset(ep, map_flags, offset,
				     len >> PAGE_SHIFT, &computed_offset);
	if (err == -EADDRINUSE) {
		/* Validate whether this window/offset still exists on reg_list,
		 * if so, wait until it is removed */
		s64 end_offset;
		struct list_head *head = &ep->rma_info.reg_list;
		struct list_head *item;

		mutex_lock(&ep->rma_info.rma_lock);
		list_for_each(item, head) {
			window = list_entry(item, struct scif_window, list);
			end_offset = window->offset + (window->nr_pages << PAGE_SHIFT);
			if (offset >= window->offset && offset < end_offset &&
				window->type == SCIF_WINDOW_SELF) {
				if (window->unreg_state == SCIF_UNREG_REQUESTED) {
					break;
				} else if (window->unreg_state == SCIF_UNREG_ACCEPTED) {
					break;
				} else if (window->unreg_state == SCIF_UNREG_DENIED ||
						window->unreg_state == SCIF_UNREG_NOT_INITIATED) {
					mutex_unlock(&ep->rma_info.rma_lock);
					return -EADDRINUSE;
				}
			}
		}
		mutex_unlock(&ep->rma_info.rma_lock);

		if (time_before(jiffies, timeout)) {
			msleep(1);
			goto get_offset;
		}
		else {
			err = -ENODEV;
		}
	}

	if (err) {
		dev_err(&ep->remote_dev->sdev->dev,
			"%s %d err %ld\n", __func__, __LINE__, err);
		return err;
	}

	spdev = scif_get_peer_dev(ep->remote_dev);
	if (!spdev) {
		scif_free_window_offset(ep, NULL, computed_offset);
		return -ENODEV;
	}

	/* Allocate and prepare self registration window */
	window = scif_create_window(ep, len >> PAGE_SHIFT,
				    computed_offset, false);
	if (!window) {
		scif_free_window_offset(ep, NULL, computed_offset);
		scif_put_peer_dev(spdev);
		return -ENOMEM;
	}

	window->nr_pages = len >> PAGE_SHIFT;

	err = scif_send_alloc_request(ep, window);
	if (err) {
		scif_destroy_incomplete_window(ep, window);
		scif_put_peer_dev(spdev);
		return err;
	}

	/* Pin down the pages */
	err = __scif_pin_pages(addr, len, &prot,
			       map_flags & SCIF_MAP_KERNEL,
			       &pinned_pages, window);
	if (err) {
		scif_destroy_incomplete_window(ep, window);
		goto error;
	}

	window->pinned_pages = pinned_pages;
	window->prot = pinned_pages->prot;

	/* Prepare the remote registration window */
	err = scif_prep_remote_window(ep, window);
	if (err) {
		dev_err(&ep->remote_dev->sdev->dev,
			"%s %d err %ld\n", __func__, __LINE__, err);
		goto error_unmap;
	}

	/* Tell the peer about the new window */
	err = scif_send_scif_register(ep, window);
	if (err) {
		dev_err(&ep->remote_dev->sdev->dev,
			"%s %d err %ld\n", __func__, __LINE__, err);
		goto error_unmap;
	}

	scif_put_peer_dev(spdev);
	/* No further failures expected. Insert new window */
	scif_insert_local_window(window, ep);
	dev_dbg(&ep->remote_dev->sdev->dev,
		"SCIFAPI register: ep %p addr %p len 0x%lx computed_offset 0x%llx\n",
		epd, addr, len, computed_offset);
	return computed_offset;
error_unmap:
	scif_destroy_window(ep, window);
error:
	scif_put_peer_dev(spdev);
	dev_err(&ep->remote_dev->sdev->dev,
		"%s %d err %ld\n", __func__, __LINE__, err);
	return err;
}
EXPORT_SYMBOL_GPL(scif_register);

int
scif_unregister(scif_epd_t epd, off_t offset, size_t len)
{
	struct scif_endpt *ep = (struct scif_endpt *)epd;
	int nr_pages, err;
	struct device *spdev;

	dev_dbg(scif_info.mdev.this_device,
		"SCIFAPI unregister: ep %p offset 0x%lx len 0x%lx\n",
		ep, offset, len);
	/* len must be page aligned. len should be non zero */
	if (!len ||
	    (ALIGN((u64)len, PAGE_SIZE) != (u64)len))
		return -EINVAL;

	/* Offset is not page aligned or offset+len wraps around */
	if ((ALIGN(offset, PAGE_SIZE) != offset) ||
	    (offset + (off_t)len < offset))
		return -EINVAL;

	err = scif_verify_epd(ep);
	if (err)
		return err;

	might_sleep();
	nr_pages = len >> PAGE_SHIFT;
	spdev = scif_get_peer_dev(ep->remote_dev);
	if (!spdev)
		return -ENODEV;

	mutex_lock(&ep->rma_info.rma_lock);
	/* Unregister all the windows in this range */
	err = scif_rma_list_unregister(ep, offset, nr_pages);
	if (err)
		dev_err(&ep->remote_dev->sdev->dev,
			"%s %d err %d\n", __func__, __LINE__, err);
	mutex_unlock(&ep->rma_info.rma_lock);
	scif_put_peer_dev(spdev);
	return err;
}
EXPORT_SYMBOL_GPL(scif_unregister);
