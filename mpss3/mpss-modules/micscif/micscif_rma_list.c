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
#include "mic/mic_dma_api.h"
#include "mic/micscif_kmem_cache.h"
#ifdef CONFIG_MMU_NOTIFIER
#include <linux/mmu_notifier.h>
#include <linux/highmem.h>
#endif
#ifndef _MIC_SCIF_
#include "mic_common.h"
#endif
#include "mic/micscif_map.h"

/*
 * micscif_insert_tcw:
 *
 * Insert a temp window to the temp registration list sorted by va_for_temp.
 * RMA lock must be held.
 */
void micscif_insert_tcw(struct reg_range_t *window,
					struct list_head *head)
{
	struct reg_range_t *curr = NULL, *prev = NULL;
	struct list_head *item;
	BUG_ON(!window);
	INIT_LIST_HEAD(&window->list_member);
	/*
	 * HSD 4845254
	 * Hack for worst case performance
	 * Compare with tail and if the new entry is new tail add it to the end
	 */
	if (!list_empty(head)) {
		curr = list_entry(head->prev, struct reg_range_t, list_member);
		if ((uint64_t) curr->va_for_temp < (uint64_t) window->va_for_temp) {
			list_add_tail(&window->list_member, head);
			return;
		}
	}
	/*
	 * We don't need the if(!prev) code but I am gonna leave it as
	 * is for now. If someone touches the above code it is likely that they
	 * will miss that they have to add if(!prev) block
	 */
	list_for_each(item, head) {
		curr = list_entry(item, struct reg_range_t, list_member);
		if ((uint64_t) curr->va_for_temp > (uint64_t) window->va_for_temp)
			break;
		prev = curr;
	}
	if (!prev)
		list_add(&window->list_member, head);
	else
		list_add(&window->list_member, &prev->list_member);
}
/*
 * micscif_insert_window:
 *
 * Insert a window to the self registration list sorted by offset.
 * RMA lock must be held.
 */
void micscif_insert_window(struct reg_range_t *window, struct list_head *head)
{
	struct reg_range_t *curr = NULL, *prev = NULL;
	struct list_head *item;
	BUG_ON(!window);
	INIT_LIST_HEAD(&window->list_member);
	list_for_each(item, head) {
		curr = list_entry(item, struct reg_range_t, list_member);
		if (curr->offset > window->offset)
			break;
		prev = curr;
	}
	if (!prev)
		list_add(&window->list_member, head);
	else
		list_add(&window->list_member, &prev->list_member);
}

/*
 * micscif_query_tcw:
 *
 * Query the temp cached registration list of ep and check if a valid contiguous
 * range of windows exist.
 * If there is a partial overlap, delete the existing window and create a new one
 * that encompasses the previous window and a new range
 * RMA lock must be held.
 */
int micscif_query_tcw(struct endpt *ep, struct micscif_rma_req *req)
{
	struct list_head *item, *temp;
	struct reg_range_t *window;
	uint64_t start_va_window, start_va_req = (uint64_t) req->va_for_temp;
	uint64_t end_va_window, end_va_req = start_va_req + req->nr_bytes;

	/*
	 * HSD 4845254
	 * Hack for the worst case scenario
	 * Avoid traversing the entire list to find out that there is no
	 * entry that matches
	 */
	if (!list_empty(req->head)) {
		temp = req->head->prev;
		window = list_entry(temp, 
			struct reg_range_t, list_member);
		end_va_window = (uint64_t) window->va_for_temp +
			(window->nr_pages << PAGE_SHIFT);
		if (start_va_req > end_va_window)
			return -ENXIO;
	}
	list_for_each_safe(item, temp, req->head) {
		window = list_entry(item, 
			struct reg_range_t, list_member);
		start_va_window = (uint64_t) window->va_for_temp;
		end_va_window = (uint64_t) window->va_for_temp +
			(window->nr_pages << PAGE_SHIFT);
		pr_debug("%s %d start_va_window 0x%llx end_va_window 0x%llx"
			" start_va_req 0x%llx end_va_req 0x%llx req->nr_bytes 0x%lx\n", 
			__func__, __LINE__, start_va_window, end_va_window, 
			start_va_req, end_va_req, req->nr_bytes);
		if (start_va_req < start_va_window) {
			if (end_va_req < start_va_window) {
				/* No overlap */
			} else {
				if ((window->prot & req->prot) != req->prot) {
					
				} else {
					req->nr_bytes += ((end_va_req > end_va_window) ? 0:(end_va_window - end_va_req));
					pr_debug("%s %d Extend req->va_for_temp %p req->nr_byte 0x%lx\n", 
						__func__, __LINE__, req->va_for_temp, req->nr_bytes);
				}
				__micscif_rma_destroy_tcw_helper(window);
			}
			break;
		} else {
			if (start_va_req > end_va_window) {
				/* No overlap */
				continue;
			} else {
				if ((window->prot & req->prot) != req->prot) {
					__micscif_rma_destroy_tcw_helper(window);
					break;
				}
				if (end_va_req > end_va_window) {
					req->va_for_temp = (void*) start_va_window;
					req->nr_bytes = end_va_req - start_va_window;
					pr_debug("%s %d Extend req->va_for_temp %p req->nr_byte 0x%lx\n", 
						__func__, __LINE__, req->va_for_temp, req->nr_bytes);
					__micscif_rma_destroy_tcw_helper(window);
					return -ENXIO;
				} else {
					*(req->out_window) = window;
					return 0;
				}
			}
		}
	}
	pr_debug("%s %d ENXIO\n", __func__, __LINE__);
	return -ENXIO;
}

/*
 * micscif_query_window:
 *
 * Query the registration list and check if a valid contiguous
 * range of windows exist.
 * RMA lock must be held.
 */
int micscif_query_window(struct micscif_rma_req *req)
{
	struct list_head *item;
	struct reg_range_t *window;
	uint64_t end_offset, offset = req->offset;
	uint64_t tmp_min, nr_bytes_left = req->nr_bytes;

	list_for_each(item, req->head) {
		window = list_entry(item, 
			struct reg_range_t, list_member);
		end_offset = window->offset +
			(window->nr_pages << PAGE_SHIFT);
		if (offset < window->offset)
			/* Offset not found! */
			return -ENXIO;
		if (offset < end_offset) {
			/* Check read/write protections. */
			if ((window->prot & req->prot) != req->prot)
				return -EPERM;
			if (nr_bytes_left == req->nr_bytes)
				/* Store the first window */
				*(req->out_window) = window;
			tmp_min = min(end_offset - offset, nr_bytes_left);
			nr_bytes_left -= tmp_min;
			offset += tmp_min;
			/*
			 * Range requested encompasses
			 * multiple windows contiguously.
			 */
			if (!nr_bytes_left) {
				/* Done for partial window */
				if (req->type == WINDOW_PARTIAL ||
					req->type == WINDOW_SINGLE)
					return 0;
				/* Extra logic for full windows */
				if (offset == end_offset)
					/* Spanning multiple whole windows */
					return 0;
				/* Not spanning multiple whole windows */
				return -ENXIO;
			}
			if (req->type == WINDOW_SINGLE)
				break;
		}
	}
	printk(KERN_ERR "%s %d ENXIO\n", __func__, __LINE__);
	return -ENXIO;
}

/*
 * micscif_rma_list_mmap:
 *
 * Traverse the remote registration list starting from start_window:
 * 1) Check read/write protections.
 * 2) Create VtoP mappings via remap_pfn_range(..)
 * 3) Once step 1) and 2) complete successfully then traverse the range of
 *    windows again and bump the reference count.
 * RMA lock must be held.
 */
int micscif_rma_list_mmap(struct reg_range_t *start_window,
	uint64_t offset, int nr_pages, struct vm_area_struct *vma)
{
	struct list_head *item, *head;
	uint64_t end_offset, loop_offset = offset;
	struct reg_range_t *window;
	int64_t start_page_nr, loop_nr_pages, nr_pages_left = nr_pages;
	struct endpt *ep = (struct endpt *)start_window->ep;
	int i, err = 0;
	uint64_t j =0;
	dma_addr_t phys_addr;

	might_sleep();
	BUG_ON(!mutex_is_locked(&ep->rma_info.rma_lock));

	/* Start traversing from the previous link in the list */
	head = ((&start_window->list_member))->prev;
	list_for_each(item, head) {
		window = list_entry(item, struct reg_range_t, 
				list_member);
		end_offset = window->offset +
			(window->nr_pages << PAGE_SHIFT);
		start_page_nr = (loop_offset - window->offset) >> PAGE_SHIFT;
		loop_nr_pages = min((int64_t)((end_offset - loop_offset) >> PAGE_SHIFT),
				nr_pages_left);
		for (i = (int)start_page_nr;
			i < ((int)start_page_nr + (int)loop_nr_pages); i++, j++) {

			phys_addr =
#if !defined(_MIC_SCIF_) && defined(CONFIG_ML1OM)
			is_self_scifdev(ep->remote_dev) ?
				micscif_get_dma_addr(window, loop_offset,
				NULL, NULL, NULL) : window->phys_addr[i];
#else
			get_phys_addr(micscif_get_dma_addr(window, loop_offset,
				NULL, NULL, NULL), ep->remote_dev);
#endif
			/*
			 * Note:
			 * 1) remap_pfn_rnage returns an error if there is an
			 * attempt to create MAP_PRIVATE COW mappings.
			 */
			if ((err = remap_pfn_range(vma,
				((vma)->vm_start) + (j * PAGE_SIZE),
				phys_addr >> PAGE_SHIFT,
				PAGE_SIZE,
				((vma)->vm_page_prot))))
				goto error;
			loop_offset += PAGE_SIZE;
		}
		nr_pages_left -= loop_nr_pages;
		if (!nr_pages_left)
			break;
	}
	BUG_ON(nr_pages_left);
	/*
	 * No more failures expected. Bump up the ref count for all
	 * the windows. Another traversal from start_window required
	 * for handling errors encountered across windows during
	 * remap_pfn_range(..).
	 */
	loop_offset = offset;
	nr_pages_left = nr_pages;
	head = (&(start_window->list_member))->prev;
	list_for_each(item, head) {
		window = list_entry(item, struct reg_range_t, 
				list_member);
		end_offset = window->offset +
			(window->nr_pages << PAGE_SHIFT);
		start_page_nr = (loop_offset - window->offset) >> PAGE_SHIFT;
		loop_nr_pages = min((int64_t)((end_offset - loop_offset) >> PAGE_SHIFT),
				nr_pages_left);
		get_window_ref_count(window, loop_nr_pages);
		nr_pages_left -= loop_nr_pages;
		loop_offset += (loop_nr_pages << PAGE_SHIFT);
		if (!nr_pages_left)
			break;
	}
	BUG_ON(nr_pages_left);
error:
	if (err)
		printk(KERN_ERR "%s %d err %d\n", __func__, __LINE__, err);
	return err;
}

/*
 * micscif_rma_list_munmap:
 *
 * Traverse the remote registration list starting from window:
 * 1) Decrement ref count.
 * 2) If the ref count drops to zero then send a SCIF_MUNMAP message to peer.
 * RMA lock must be held.
 */
void micscif_rma_list_munmap(struct reg_range_t *start_window,
				uint64_t offset, int nr_pages)
{
	struct list_head *item, *tmp, *head;
	struct nodemsg msg;
	uint64_t loop_offset = offset, end_offset;
	int64_t loop_nr_pages, nr_pages_left = nr_pages;
	struct endpt *ep = (struct endpt *)start_window->ep;
	struct reg_range_t *window;

	BUG_ON(!mutex_is_locked(&ep->rma_info.rma_lock));

	msg.uop = SCIF_MUNMAP;
	msg.src = ep->port;
	loop_offset = offset;
	nr_pages_left = nr_pages;
	/* Start traversing from the previous link in the list */
	head = (&(start_window->list_member))->prev;
	list_for_each_safe(item, tmp, head) {
		window = list_entry(item, struct reg_range_t, 
				list_member);
		RMA_MAGIC(window);
		end_offset = window->offset +
			(window->nr_pages << PAGE_SHIFT);
		loop_nr_pages = min((int64_t)((end_offset - loop_offset) >> PAGE_SHIFT),
				nr_pages_left);
		put_window_ref_count(window, loop_nr_pages);
		if (!window->ref_count) {
			if (scifdev_alive(ep))
				drain_dma_intr(ep->rma_info.dma_chan);
			/* Inform the peer about this munmap */
			msg.payload[0] = window->peer_window;
			/* No error handling for Notification messages. */
			micscif_nodeqp_send(ep->remote_dev, &msg, ep);
			list_del(&window->list_member);
			/* Destroy this window from the peer's registered AS */
			micscif_destroy_remote_window(ep, window);
		}
		nr_pages_left -= loop_nr_pages;
		loop_offset += (loop_nr_pages << PAGE_SHIFT);
		if (!nr_pages_left)
			break;
	}
	BUG_ON(nr_pages_left);
}

/*
 * micscif_rma_list_unregister:
 *
 * Traverse the self registration list starting from window:
 * 1) Call micscif_unregister_window(..)
 * RMA lock must be held.
 */
int micscif_rma_list_unregister(struct reg_range_t *window,
				uint64_t offset, int nr_pages)
{
	struct list_head *item, *tmp, *head;
	uint64_t end_offset;
	int err = 0;
	int64_t loop_nr_pages;
	struct endpt *ep = (struct endpt *)window->ep;

	BUG_ON(!mutex_is_locked(&ep->rma_info.rma_lock));
	/* Start traversing from the previous link in the list */
	head = (&window->list_member)->prev;
	list_for_each_safe(item, tmp, head) {
		window = list_entry(item, struct reg_range_t, 
				list_member);
		RMA_MAGIC(window);
		end_offset = window->offset +
			(window->nr_pages << PAGE_SHIFT);
		loop_nr_pages = min((int)((end_offset - offset) >> PAGE_SHIFT),
				nr_pages);
		if ((err = micscif_unregister_window(window)))
			return err;
		nr_pages -= (int)loop_nr_pages;
		offset += (loop_nr_pages << PAGE_SHIFT);
		if (!nr_pages)
			break;
	}
	BUG_ON(nr_pages);
	return 0;
}

/*
 * micscif_unregister_all_window:
 *
 * Traverse all the windows in the self registration list and:
 * 1) Call micscif_unregister_window(..)
 * RMA lock must be held.
 */
int micscif_unregister_all_windows(scif_epd_t epd)
{
	struct list_head *item, *tmp;
	struct reg_range_t *window;
	struct endpt *ep = (struct endpt *)epd;
	struct list_head *head = &ep->rma_info.reg_list;
	int err = 0;

	queue_work(ms_info.mi_misc_wq, &ms_info.mi_misc_work);
	mutex_lock(&ep->rma_info.rma_lock);
retry:
	item = NULL;
	tmp = NULL;
	list_for_each_safe(item, tmp, head) {
		window = list_entry(item, 
			struct reg_range_t, list_member);
		ep->rma_info.async_list_del = 0;
		if ((err = micscif_unregister_window(window)))
			pr_debug("%s %d err %d\n", 
				__func__, __LINE__, err);
		/*
		 * Need to restart list traversal if there has been
		 * an asynchronous list entry deletion.
		 */
		if (ep->rma_info.async_list_del)
			goto retry;
	}
	mutex_unlock(&ep->rma_info.rma_lock);

	/*
	 * The following waits cannot be interruptible since they are
	 * from the driver release() entry point.
	 */
	err = wait_event_timeout(ep->rma_info.fence_wq, 
			!ep->rma_info.fence_refcount, NODE_ALIVE_TIMEOUT);
	/* Timeout firing is unexpected. Is the DMA engine hung? */
	if (!err)
		printk(KERN_ERR "%s %d err %d\n", __func__, __LINE__, err);

#ifdef CONFIG_MMU_NOTIFIER
	if (!list_empty(&ep->rma_info.mmn_list)) {
		spin_lock(&ms_info.mi_rmalock);
		list_add_tail(&ep->mmu_list, &ms_info.mi_mmu_notif_cleanup);
		spin_unlock(&ms_info.mi_rmalock);
		queue_work(ms_info.mi_mmu_notif_wq, &ms_info.mi_mmu_notif_work);
	}
#endif
	return err;
}

/*
 * micscif_rma_list_get_pages_check:
 *
 * Traverse the remote registration list and return 0 if all the
 * scif_get_pages()/scif_put_pages() ref_counts are zero else return -1.
 */
int micscif_rma_list_get_pages_check(struct endpt *ep)
{
	struct list_head *item, *head = &ep->rma_info.remote_reg_list;
	struct reg_range_t *window;
	int err = 0;

	mutex_lock(&ep->rma_info.rma_lock);
	list_for_each(item, head) {
		window = list_entry(item, 
			struct reg_range_t, list_member);
		if (window->get_put_ref_count) {
			err = -1;
			break;
		}
	}
	mutex_unlock(&ep->rma_info.rma_lock);
	return err;
}

/* Only debug API's below */
void micscif_display_all_windows(struct list_head *head)
{
	struct list_head *item;
	struct reg_range_t *window;
	pr_debug("\nWindow List Start\n");
	list_for_each(item, head) {
		window = list_entry(item, 
			struct reg_range_t, list_member);
		micscif_display_window(window, __func__, __LINE__);
	}
	pr_debug("Window List End\n\n");
}
