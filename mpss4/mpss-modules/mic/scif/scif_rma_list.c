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
 * Intel SCIF driver.
 */
#include "scif_main.h"
#include <linux/mmu_notifier.h>
#include <linux/highmem.h>

#ifndef MIC_IN_KERNEL_BUILD
#ifndef list_last_entry
#define list_last_entry(ptr, type, member) \
	list_entry((ptr)->prev, type, member)
#endif
#endif

/*
 * scif_insert_tcw:
 *
 * Insert a temp window to the temp registration list sorted by va_for_temp.
 * RMA lock must be held.
 */
void scif_insert_tcw(struct scif_window *window, struct list_head *head)
{
	struct scif_window *curr = NULL;
	struct scif_window *prev = list_entry(head, struct scif_window, list);
	struct list_head *item;

	INIT_LIST_HEAD(&window->list);
	/* Compare with tail and if the entry is new tail add it to the end */
	if (!list_empty(head)) {
		curr = list_entry(head->prev, struct scif_window, list);
		if (curr->va_for_temp < window->va_for_temp) {
			list_add_tail(&window->list, head);
			return;
		}
	}
	list_for_each(item, head) {
		curr = list_entry(item, struct scif_window, list);
		if (curr->va_for_temp > window->va_for_temp)
			break;
		prev = curr;
	}
	list_add(&window->list, &prev->list);
}

/*
 * scif_insert_window:
 *
 * Insert a window to the self registration list sorted by offset.
 * RMA lock must be held.
 */
void scif_insert_window(struct scif_window *window, struct list_head *head)
{
	struct scif_window *curr = NULL, *prev = NULL;
	struct list_head *item;

	INIT_LIST_HEAD(&window->list);
	list_for_each(item, head) {
		curr = list_entry(item, struct scif_window, list);
		if (curr->offset > window->offset)
			break;
		prev = curr;
	}
	if (!prev)
		list_add(&window->list, head);
	else
		list_add(&window->list, &prev->list);
}

/*
 * scif_query_tcw:
 *
 * Query the temp cached registration list of ep for an overlapping window
 * in case of permission mismatch, destroy the previous window. if permissions
 * match and overlap is partial, destroy the window but return the new range
 * RMA lock must be held.
 */
int scif_query_tcw(struct scif_endpt *ep, struct scif_rma_req *req)
{
	struct list_head *item, *temp, *head = req->head;
	struct scif_window *window;
	u64 start_va_window, start_va_req = req->va_for_temp;
	u64 end_va_window, end_va_req = start_va_req + req->nr_bytes;

	if (!req->nr_bytes)
		return -EINVAL;
	/*
	 * Avoid traversing the entire list to find out that there
	 * is no entry that matches
	 */
	if (!list_empty(head)) {
		window = list_last_entry(head, struct scif_window, list);
		end_va_window = window->va_for_temp +
			(window->nr_pages << PAGE_SHIFT);
		if (start_va_req > end_va_window)
			return -ENXIO;
	}
	list_for_each_safe(item, temp, head) {
		window = list_entry(item, struct scif_window, list);
		start_va_window = window->va_for_temp;
		end_va_window = window->va_for_temp +
			(window->nr_pages << PAGE_SHIFT);
		if (start_va_req < start_va_window &&
		    end_va_req < start_va_window)
			break;
		if (start_va_req >= end_va_window)
			continue;
		if ((window->prot & req->prot) == req->prot) {
			if (start_va_req >= start_va_window &&
			    end_va_req <= end_va_window) {
				*req->out_window = window;
				return 0;
			}
			/* expand window */
			if (start_va_req < start_va_window) {
				req->nr_bytes +=
					start_va_window - start_va_req;
				req->va_for_temp = start_va_window;
			}
			if (end_va_req >= end_va_window)
				req->nr_bytes += end_va_window - end_va_req;
		}
		/* Destroy the old window to create a new one */
		__scif_rma_destroy_tcw_helper(window);
		break;
	}
	return -ENXIO;
}

/*
 * scif_query_window:
 *
 * Query the registration list and check if a valid contiguous
 * range of windows exist.
 * RMA lock must be held.
 */
int scif_query_window(struct scif_rma_req *req)
{
	struct list_head *item;
	struct scif_window *window;
	s64 end_offset, offset = req->offset;
	u64 tmp_min, nr_bytes_left = req->nr_bytes;

	if (!req->nr_bytes)
		return -EINVAL;

	list_for_each(item, req->head) {
		window = list_entry(item, struct scif_window, list);
		end_offset = window->offset +
			(window->nr_pages << PAGE_SHIFT);
		/* Offset not found! */
		if (offset < window->offset)
			return -ENXIO;
		if (offset >= end_offset)
			continue;
		/* Check read/write protections. */
		if ((window->prot & req->prot) != req->prot)
			return -EPERM;
		/* Store the first window */
		if (nr_bytes_left == req->nr_bytes)
			*req->out_window = window;
		tmp_min = min((u64)end_offset - offset, nr_bytes_left);
		nr_bytes_left -= tmp_min;
		offset += tmp_min;
		/*
		 * Range requested encompasses
		 * multiple windows contiguously.
		 */
		if (!nr_bytes_left) {
			/* Done for partial window */
			if (req->type == SCIF_WINDOW_PARTIAL ||
			    req->type == SCIF_WINDOW_SINGLE)
				return 0;
			/* Extra logic for full windows */
			/* Spanning multiple whole windows */
			if (offset == end_offset)
				return 0;
			/* Not spanning multiple whole windows */
			return -ENXIO;
		}
		if (req->type == SCIF_WINDOW_SINGLE)
			break;
	}
	dev_err(scif_info.mdev.this_device,
		"%s %d ENXIO\n", __func__, __LINE__);
	return -ENXIO;
}

/*
 * scif_rma_list_unregister:
 *
 * Traverse the self registration list starting from window:
 * 1) Call scif_unregister_window(..)
 * RMA lock must be held.
 */
int scif_rma_list_unregister(scif_epd_t epd,
				s64 offset, int nr_pages)
{
	struct scif_endpt *ep = (struct scif_endpt *)epd;
	struct list_head *head = &ep->rma_info.reg_list;
	struct list_head *item;
	s64 end_offset;
	int err = 0;
	struct scif_window *window;
	s64 end_req_offset = offset + ((s64)nr_pages << PAGE_SHIFT);

	list_for_each(item, head) {
		window = list_entry(item, struct scif_window, list);
		if (window->offset >= end_req_offset)
			break;

		end_offset = window->offset + (window->nr_pages << PAGE_SHIFT);
		if (end_offset <= offset)
			continue;

		/* does window intersect 'offset' */
		if (window->offset < offset && end_offset > offset) {
			err = -EINVAL;
			goto exit;
		}

		/* does window intersect 'end_req_offset' */
		if (end_offset > end_req_offset) {
			err = -EINVAL;
			goto exit;
		}

		err = scif_unregister_window(window);
		/*
		 * ENXIO means that scif_unregister_window is in progress
		 * for this window
		 */
		if (err != -ENXIO && err != 0) {
			dev_err(scif_info.mdev.this_device,
				"%s %d err: %d\n", __func__, __LINE__, err);
		}
	}

exit:
	return err;
}

/*
 * scif_unmap_all_window:
 *
 * Traverse all the windows in the self registration list and:
 * 1) Delete any DMA mappings created
 */
void scif_unmap_all_windows(scif_epd_t epd)
{
	struct list_head *item, *tmp;
	struct scif_window *window;
	struct scif_endpt *ep = (struct scif_endpt *)epd;
	struct list_head *head = &ep->rma_info.reg_list;

	mutex_lock(&ep->rma_info.rma_lock);
	list_for_each_safe(item, tmp, head) {
		window = list_entry(item, struct scif_window, list);
		scif_unmap_window(ep->remote_dev, window);
	}
	mutex_unlock(&ep->rma_info.rma_lock);
}

/*
 * scif_unregister_all_window:
 *
 * Traverse all the windows in the self registration list and:
 * 1) Call scif_unregister_window(..)
 * RMA lock must be held.
 */
int scif_unregister_all_windows(scif_epd_t epd)
{
	struct list_head *item, *tmp;
	struct scif_window *window;
	struct scif_endpt *ep = (struct scif_endpt *)epd;
	struct list_head *head = &ep->rma_info.reg_list;
	int err = 0;

	mutex_lock(&ep->rma_info.rma_lock);
retry:
	item = NULL;
	tmp = NULL;
	list_for_each_safe(item, tmp, head) {
		window = list_entry(item, struct scif_window, list);
		ep->rma_info.async_list_del = 0;
		err = scif_unregister_window(window);
		if (err)
			dev_err(scif_info.mdev.this_device,
				"%s %d err %d\n",
				__func__, __LINE__, err);
		/*
		 * Need to restart list traversal if there has been
		 * an asynchronous list entry deletion.
		 */
		if (ACCESS_ONCE(ep->rma_info.async_list_del))
			goto retry;
	}
	mutex_unlock(&ep->rma_info.rma_lock);
	if (!list_empty(&ep->rma_info.mmn_list)) {
		spin_lock(&scif_info.rmalock);
		list_add_tail(&ep->mmu_list, &scif_info.mmu_notif_cleanup);
		spin_unlock(&scif_info.rmalock);
		schedule_work(&scif_info.mmu_notif_work);
	}
	return err;
}
