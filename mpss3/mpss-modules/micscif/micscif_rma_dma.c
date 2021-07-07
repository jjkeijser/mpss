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
#include "mic/micscif_rma.h"
#include "mic/micscif_rma_list.h"
#if !defined(WINDOWS) && !defined(CONFIG_PREEMPT)
#include <linux/sched.h>
#endif
#include <linux/highmem.h>
#ifndef _MIC_SCIF_
#include "mic_common.h"
#endif

static __always_inline
void *get_local_va(off_t off, struct reg_range_t *window, size_t len)
{
	uint64_t page_nr = (off - window->offset) >> PAGE_SHIFT;
	off_t page_off = off & ~PAGE_MASK;
	void *va;

	if (RMA_WINDOW_SELF == window->type) {
		struct page **pages = window->pinned_pages->pages;
		va = (void *)((uint64_t)
			(page_address(pages[page_nr])) | page_off);
	} else {
		dma_addr_t phys =	micscif_get_dma_addr(window, off, NULL, NULL, NULL);
#ifdef CONFIG_ML1OM
		if (RMA_ERROR_CODE == phys)
			return NULL;
#endif
		va = (void *)((uint64_t) (phys_to_virt(phys)));
	}
	return va;
}

#ifdef _MIC_SCIF_
static __always_inline
void *ioremap_remote(off_t off, struct reg_range_t *window,
				size_t len, bool loopback, struct micscif_dev *dev, int *index, uint64_t *start_off)
{
	void *ret;
	dma_addr_t phys =	micscif_get_dma_addr(window, off, NULL, index, start_off);

#ifdef CONFIG_ML1OM
	if (RMA_ERROR_CODE == phys)
		return NULL;
#endif
	if (!loopback)
		ret = ioremap_nocache(phys, len);
	else
		ret = (void *)((uint64_t)phys_to_virt(phys));
	return ret;
}

static __always_inline
void *ioremap_remote_gtt(off_t off, struct reg_range_t *window,
	size_t len, bool loopback, struct micscif_dev *dev, int ch_num, struct mic_copy_work *work)
{
	return ioremap_remote(off, window, len, loopback, dev, NULL, NULL);
}
#else
static __always_inline
void *ioremap_remote_gtt(off_t off, struct reg_range_t *window,
	size_t len, bool loopback, struct micscif_dev *dev, int ch_num, struct mic_copy_work *work)
{
	void *ret;
	uint64_t page_nr = (off - window->offset) >> PAGE_SHIFT;
	off_t page_off = off & ~PAGE_MASK;
	if (!loopback) {
		dma_addr_t phys =	micscif_get_dma_addr(window, off, NULL, NULL, NULL);
		/* Ideally there should be a helper to do the +/-1 */
		ret = get_per_dev_ctx(dev->sd_node - 1)->aper.va + phys;
	} else {
		struct page **pages = ((struct reg_range_t *)
			(window->peer_window))->pinned_pages->pages;
		ret = (void *)((uint64_t)phys_to_virt(page_to_phys(pages[page_nr]))
				| page_off);
	}
	return ret;
}

static __always_inline
void *ioremap_remote(off_t off, struct reg_range_t *window,
				size_t len, bool loopback, struct micscif_dev *dev, int *index, uint64_t *start_off)
{
	void *ret;
	int page_nr = (int)((off - window->offset) >> PAGE_SHIFT);
	off_t page_off = off & ~PAGE_MASK;

	if (!loopback) {
		dma_addr_t phys;
		mic_ctx_t *mic_ctx = get_per_dev_ctx(dev->sd_node - 1);
		phys = micscif_get_dma_addr(window, off, NULL, index, start_off);
		ret = mic_ctx->aper.va + phys;
	} else {
		struct page **pages = ((struct reg_range_t *)
			(window->peer_window))->pinned_pages->pages;
		ret = (void *)((uint64_t)phys_to_virt(page_to_phys(pages[page_nr]))
				| page_off);
	}
	return ret;
}
#endif

static __always_inline void
iounmap_remote(void *virt, size_t size, struct mic_copy_work *work)
{
#ifdef _MIC_SCIF_
	if (!work->loopback)
		iounmap(virt);
#endif
}

/*
 * Takes care of ordering issue caused by
 * 1. Hardware:  Only in the case of cpu copy from host to card because of WC memory.
 * 2. Software: If memcpy reorders copy instructions for optimization. This could happen
 * at both host and card.
 */
static inline void ordered_memcpy(volatile char *dst,
		const char *src, size_t count)
{
	if (!count)
		return;

	memcpy_toio(dst, src, --count);
	wmb();
	*(dst + count) = *(src + count);
}

static inline void micscif_unaligned_memcpy(volatile char *dst,
		const char *src, size_t count, bool ordered)
{
	if (unlikely(ordered))
		ordered_memcpy(dst, src, count);
	else
		memcpy_toio(dst, src, count);
}

/*
 * Copy between rma window and temporary buffer
 */
void micscif_rma_local_cpu_copy(uint64_t offset, struct reg_range_t *window, uint8_t *temp, size_t remaining_len, bool to_temp)
{
	void *window_virt;
	size_t loop_len;
	int offset_in_page;
	uint64_t end_offset;
	struct list_head *item;

	BUG_ON(RMA_WINDOW_SELF != window->type);

	offset_in_page = offset & ~PAGE_MASK;
	loop_len = PAGE_SIZE - offset_in_page;

	if (remaining_len < loop_len)
		loop_len = remaining_len;

	if (!(window_virt = get_local_va(offset, window, loop_len)))
		return;
	if (to_temp)
		memcpy(temp, window_virt, loop_len);
	else
		memcpy(window_virt, temp, loop_len);

	offset		+= loop_len;
	temp		+= loop_len;
	remaining_len	-= loop_len;

	end_offset = window->offset +
		(window->nr_pages << PAGE_SHIFT);
	while (remaining_len) {
		if (offset == end_offset) {
			item = (
				&window->list_member)->next;
			window = list_entry(item, 
					struct reg_range_t, 
					list_member);
			end_offset = window->offset +
				(window->nr_pages << PAGE_SHIFT);
		}

		loop_len = min(PAGE_SIZE, remaining_len);

		if (!(window_virt = get_local_va(offset, window, loop_len)))
			return;

		if (to_temp)
			memcpy(temp, window_virt, loop_len);
		else
			memcpy(window_virt, temp, loop_len);

		offset	+= loop_len;
		temp		+= loop_len;
		remaining_len	-= loop_len;
	}
}

/*
 * Comment this
 *
 */
static int micscif_rma_list_dma_copy_unaligned(struct mic_copy_work *work, uint8_t *temp, struct dma_channel *chan, bool src_local)
{
	struct dma_completion_cb *comp_cb = work->comp_cb;
	dma_addr_t window_dma_addr, temp_dma_addr;
#ifndef _MIC_SCIF_
	dma_addr_t temp_phys = comp_cb->temp_phys;
#endif
	size_t loop_len, nr_contig_bytes = 0, remaining_len = work->len;
	int offset_in_page;
	uint64_t end_offset = 0, offset = 0;
	struct reg_range_t *window = NULL;
	struct list_head *item = NULL;
	int ret = 0;
	void *window_virt_addr = NULL;
	size_t tail_len = 0;

	if (src_local) {
		offset = work->dst_offset;
		window = work->dst_window;
	} else {
		offset = work->src_offset;
		window = work->src_window;
	}

	offset_in_page	= offset & (L1_CACHE_BYTES - 1);
	if (offset_in_page) {
		loop_len = L1_CACHE_BYTES - offset_in_page;
		loop_len = min(loop_len, remaining_len);

		if (!(window_virt_addr = ioremap_remote_gtt(offset, window, loop_len,
				work->loopback, work->remote_dev,
				get_chan_num(chan), work)))
			return -ENOMEM;

		if (src_local) {
			micscif_unaligned_memcpy(window_virt_addr, temp, loop_len, work->ordered &&
							 !(remaining_len - loop_len));
			serializing_request(window_virt_addr);
		} else {
			memcpy_fromio(temp, window_virt_addr, loop_len);
			serializing_request(temp);
		}
#ifdef RMA_DEBUG
		atomic_long_add_return(loop_len, &ms_info.rma_unaligned_cpu_cnt);
#endif
		smp_mb();
		iounmap_remote(window_virt_addr, loop_len, work);

		offset += loop_len;
		temp += loop_len;
#ifndef _MIC_SCIF_
		temp_phys += loop_len;
#endif
		remaining_len -= loop_len;
	}

	offset_in_page = offset & ~PAGE_MASK;
	end_offset = window->offset +
		(window->nr_pages << PAGE_SHIFT);

	tail_len = remaining_len & (L1_CACHE_BYTES - 1);
	remaining_len -= tail_len;
	while (remaining_len) {
		if (offset == end_offset) {
			item = (&window->list_member)->next;
			window = list_entry(item, 
					struct reg_range_t, 
					list_member);
			end_offset = window->offset +
				(window->nr_pages << PAGE_SHIFT);
		}
#ifndef _MIC_SCIF_
		temp_dma_addr = temp_phys;
#else
		temp_dma_addr = (dma_addr_t)virt_to_phys(temp);
#endif
		window_dma_addr = micscif_get_dma_addr(window, offset, &nr_contig_bytes, NULL, NULL);

#ifdef CONFIG_ML1OM
		if (RMA_ERROR_CODE == window_dma_addr)
			return -ENXIO;
#endif
		loop_len = min(nr_contig_bytes, remaining_len);

		if (src_local) {
			if (unlikely(work->ordered && !tail_len &&
				!(remaining_len - loop_len) &&
				loop_len != L1_CACHE_BYTES)) {
				/*
				 * Break up the last chunk of the transfer into two steps
				 * if there is no tail to gurantee DMA ordering.
				 * Passing DO_DMA_POLLING inserts a status update descriptor
				 * in step 1 which acts as a double sided synchronization
				 * fence for the DMA engine to ensure that the last cache line
				 * in step 2 is updated last.
				 */
				/* Step 1) DMA: Body Length - L1_CACHE_BYTES. */
				ret = do_dma(chan, DO_DMA_POLLING, temp_dma_addr, window_dma_addr,
						loop_len - L1_CACHE_BYTES, NULL);
				if (ret < 0) {
					printk(KERN_ERR "%s %d Desc Prog Failed ret %d\n", 
							__func__, __LINE__, ret);
					return ret;
				}
				offset += (loop_len - L1_CACHE_BYTES);
				temp_dma_addr += (loop_len - L1_CACHE_BYTES);
				window_dma_addr += (loop_len - L1_CACHE_BYTES);
				remaining_len -= (loop_len - L1_CACHE_BYTES);
				loop_len = remaining_len;

				/* Step 2) DMA: L1_CACHE_BYTES */
				ret = do_dma(chan, 0, temp_dma_addr, window_dma_addr,
						loop_len, NULL);
				if (ret < 0) {
					printk(KERN_ERR "%s %d Desc Prog Failed ret %d\n", 
							__func__, __LINE__, ret);
					return ret;
				}
			} else {
				int flags = 0;
				if (remaining_len == loop_len + L1_CACHE_BYTES)
					flags = DO_DMA_POLLING;
				ret = do_dma(chan, flags, temp_dma_addr, window_dma_addr,
						loop_len, NULL);
			}
		} else {
			ret = do_dma(chan, 0, window_dma_addr, temp_dma_addr,
					loop_len, NULL);
		}
		if (ret < 0) {
			printk(KERN_ERR "%s %d Desc Prog Failed ret %d\n", 
					__func__, __LINE__, ret);
			return ret;
		}
		offset += loop_len;
		temp += loop_len;
#ifndef _MIC_SCIF_
		temp_phys += loop_len;
#endif
		remaining_len -= loop_len;
		offset_in_page = 0;
	}
	if (tail_len) {
		if (offset == end_offset) {
			item = (&window->list_member)->next;
			window = list_entry(item, 
					struct reg_range_t, 
					list_member);
			end_offset = window->offset +
				(window->nr_pages << PAGE_SHIFT);
		}
		if (!(window_virt_addr = ioremap_remote_gtt(offset, window, tail_len,
					work->loopback, work->remote_dev,
					get_chan_num(chan), work)))
			return -ENOMEM;

		/*
		 * The CPU copy for the tail bytes must be initiated only once previous
		 * DMA transfers for this endpoint have completed to guarantee
		 * ordering.
		 */
		if (unlikely(work->ordered)) {
			free_dma_channel(chan);
			work->dma_chan_released = true;
			if ((ret = drain_dma_intr(chan)))
				return ret;
		}

		if (src_local) {
			micscif_unaligned_memcpy(window_virt_addr, temp, tail_len, work->ordered);
			serializing_request(window_virt_addr);
		} else {
			memcpy_fromio(temp, window_virt_addr, tail_len);
			serializing_request(temp);
		}
#ifdef RMA_DEBUG
		atomic_long_add_return(tail_len, &ms_info.rma_unaligned_cpu_cnt);
#endif
		smp_mb();
		iounmap_remote(window_virt_addr, tail_len, work);
	}
	if (work->dma_chan_released) {
		if ((ret = request_dma_channel(chan)))
			return ret;
		/* Callee frees the DMA channel lock, if it is held */
		work->dma_chan_released = false;
	}
	ret = do_dma(chan, DO_DMA_INTR, 0, 0, 0, comp_cb);
	if (ret < 0) {
		printk(KERN_ERR "%s %d Desc Prog Failed ret %d\n", 
				__func__, __LINE__, ret);
		return ret;
	}
	return 0;
}

static inline bool is_local_dma_addr(uint64_t addr)
{
#ifdef _MIC_SCIF_
	return (addr >> PAGE_SHIFT < num_physpages);
#else
	return is_syspa(addr);
#endif
}

/*
 * micscif_rma_list_dma_copy_aligned:
 *
 * Traverse all the windows and perform DMA copy.
 */
static int micscif_rma_list_dma_copy_aligned(struct mic_copy_work *work, struct dma_channel *chan)
{
	dma_addr_t src_dma_addr, dst_dma_addr;
	size_t loop_len, remaining_len, tail_len, src_contig_bytes = 0, dst_contig_bytes = 0;
	int src_cache_off, dst_cache_off, src_last_index = 0, dst_last_index = 0;
	uint64_t end_src_offset, end_dst_offset;
	void *src_virt, *dst_virt;
	struct reg_range_t *src_window = work->src_window;
	struct reg_range_t *dst_window = work->dst_window;
	uint64_t src_offset = work->src_offset, dst_offset = work->dst_offset;
	uint64_t src_start_offset = src_window->offset, dst_start_offset = dst_window->offset;
	struct list_head *item;
	int ret = 0;

	remaining_len = work->len;

	src_cache_off = src_offset & (L1_CACHE_BYTES - 1);
	dst_cache_off = dst_offset & (L1_CACHE_BYTES - 1);
	if (src_cache_off != dst_cache_off) {
		BUG_ON(1);
	} else if (src_cache_off != 0) {
		/* Head */
		loop_len = L1_CACHE_BYTES - src_cache_off;
		loop_len = min(loop_len, remaining_len);
		src_dma_addr = micscif_get_dma_addr(src_window, src_offset, NULL, NULL, NULL);
		dst_dma_addr = micscif_get_dma_addr(dst_window, dst_offset, NULL, NULL, NULL);
#ifdef CONFIG_ML1OM
		if (RMA_ERROR_CODE == src_dma_addr)
			return -ENXIO;
		if (RMA_ERROR_CODE == dst_dma_addr)
			return -ENXIO;
		get_window_ref_count(src_window, 1);
		get_window_ref_count(dst_window, 1);
#endif
		if (is_local_dma_addr(src_dma_addr))
			src_virt = get_local_va(src_offset, src_window, loop_len);
		else
			src_virt = ioremap_remote_gtt(src_offset, src_window,
					loop_len, work->loopback,
					work->remote_dev, get_chan_num(chan), work);
		if (!src_virt) {
#ifdef CONFIG_ML1OM
			put_window_ref_count(src_window, 1);
			put_window_ref_count(dst_window, 1);
#endif
			return -ENOMEM;
		}
		if (is_local_dma_addr(dst_dma_addr))
			dst_virt = get_local_va(dst_offset, dst_window, loop_len);
		else
			dst_virt = ioremap_remote_gtt(dst_offset, dst_window,
						loop_len, work->loopback,
						work->remote_dev, get_chan_num(chan), work);
#ifdef CONFIG_ML1OM
		put_window_ref_count(src_window, 1);
		put_window_ref_count(dst_window, 1);
#endif
		if (!dst_virt) {
			if (!is_local_dma_addr(src_dma_addr))
				iounmap_remote(src_virt, loop_len, work);
			return -ENOMEM;
		}
		if (is_local_dma_addr(src_dma_addr)){
			micscif_unaligned_memcpy(dst_virt, src_virt, loop_len,
					remaining_len == loop_len ? work->ordered : false);
		}
		else{
			memcpy_fromio(dst_virt, src_virt, loop_len);
		}
		serializing_request(dst_virt);
		smp_mb();
		if (!is_local_dma_addr(src_dma_addr))
			iounmap_remote(src_virt, loop_len, work);
		if (!is_local_dma_addr(dst_dma_addr))
			iounmap_remote(dst_virt, loop_len, work);
		src_offset += loop_len;
		dst_offset += loop_len;
		remaining_len -= loop_len;
	}

	end_src_offset = src_window->offset +
		(src_window->nr_pages << PAGE_SHIFT);
	end_dst_offset = dst_window->offset +
		(dst_window->nr_pages << PAGE_SHIFT);
	tail_len = remaining_len & (L1_CACHE_BYTES - 1);
	remaining_len -= tail_len;
	while (remaining_len) {
		if (src_offset == end_src_offset) {
			item = (&src_window->list_member)->next;
			src_window = list_entry(item, 
					struct reg_range_t, 
					list_member);
			end_src_offset = src_window->offset +
				(src_window->nr_pages << PAGE_SHIFT);
			src_last_index = 0;
			src_start_offset = src_window->offset;
		}
		if (dst_offset == end_dst_offset) {
			item = (&dst_window->list_member)->next;
			dst_window = list_entry(item, struct reg_range_t, list_member);
			end_dst_offset = dst_window->offset +
				(dst_window->nr_pages << PAGE_SHIFT);
			dst_last_index = 0;
			dst_start_offset = dst_window->offset;
		}

		/* compute dma addresses for transfer */
		src_dma_addr = micscif_get_dma_addr(src_window, src_offset, &src_contig_bytes, &src_last_index, &src_start_offset);
		dst_dma_addr = micscif_get_dma_addr(dst_window, dst_offset, &dst_contig_bytes, &dst_last_index, &dst_start_offset);
#ifdef CONFIG_ML1OM
		if (RMA_ERROR_CODE == src_dma_addr)
			return -ENXIO;
		if (RMA_ERROR_CODE == dst_dma_addr)
			return -ENXIO;
#endif
		loop_len = min(src_contig_bytes, dst_contig_bytes);
		loop_len = min(loop_len, remaining_len);
		if (unlikely(work->ordered && !tail_len &&
			!(remaining_len - loop_len) &&
			loop_len != L1_CACHE_BYTES)) {
			/*
			 * Break up the last chunk of the transfer into two steps
			 * if there is no tail to gurantee DMA ordering.
			 * Passing DO_DMA_POLLING inserts a status update descriptor
			 * in step 1 which acts as a double sided synchronization
			 * fence for the DMA engine to ensure that the last cache line
			 * in step 2 is updated last.
			 */
			/* Step 1) DMA: Body Length - L1_CACHE_BYTES. */
			ret = do_dma(chan, DO_DMA_POLLING, src_dma_addr, dst_dma_addr,
					loop_len - L1_CACHE_BYTES, NULL);
			if (ret < 0) {
				printk(KERN_ERR "%s %d Desc Prog Failed ret %d\n", 
						__func__, __LINE__, ret);
				return ret;
			}
			src_offset += (loop_len - L1_CACHE_BYTES);
			dst_offset += (loop_len - L1_CACHE_BYTES);
			src_dma_addr += (loop_len - L1_CACHE_BYTES);
			dst_dma_addr += (loop_len - L1_CACHE_BYTES);
			remaining_len -= (loop_len - L1_CACHE_BYTES);
			loop_len = remaining_len;

			/* Step 2) DMA: L1_CACHE_BYTES */
			ret = do_dma(chan, 0, src_dma_addr, dst_dma_addr,
				loop_len, NULL);
			if (ret < 0) {
				printk(KERN_ERR "%s %d Desc Prog Failed ret %d\n", 
					__func__, __LINE__, ret);
				return ret;
			}
		} else {
			int flags = 0;
			if (remaining_len == loop_len + L1_CACHE_BYTES)
				flags = DO_DMA_POLLING;
			ret = do_dma(chan, flags, src_dma_addr, dst_dma_addr,
					loop_len, NULL);
			if (ret < 0) {
				printk(KERN_ERR "%s %d Desc Prog Failed ret %d\n", 
						__func__, __LINE__, ret);
				return ret;
			}
		}
		src_offset += loop_len;
		dst_offset += loop_len;
		remaining_len -= loop_len;
	}
#ifdef CONFIG_MK1OM
	BUG_ON(remaining_len != 0);
#endif
#ifdef CONFIG_ML1OM
	if (remaining_len)
		return - ENXIO;
#endif
	remaining_len = tail_len;
	if (remaining_len) {
		loop_len = remaining_len;
		if (src_offset == end_src_offset) {
			item = (&src_window->list_member)->next;
			src_window = list_entry(item, 
					struct reg_range_t, 
					list_member);
		}
		if (dst_offset == end_dst_offset) {
			item = (&dst_window->list_member)->next;
			dst_window = list_entry(item, struct reg_range_t, list_member);
		}

        src_dma_addr = micscif_get_dma_addr(src_window, src_offset, NULL, NULL, NULL);
		dst_dma_addr = micscif_get_dma_addr(dst_window, dst_offset, NULL, NULL, NULL);
#ifdef CONFIG_ML1OM
		if (RMA_ERROR_CODE == src_dma_addr)
			return -ENXIO;
		if (RMA_ERROR_CODE == dst_dma_addr)
			return -ENXIO;
#endif
		/*
		 * The CPU copy for the tail bytes must be initiated only once previous
		 * DMA transfers for this endpoint have completed to guarantee
		 * ordering.
		 */
		if (unlikely(work->ordered)) {
			free_dma_channel(chan);
			work->dma_chan_released = true;
			if ((ret = drain_dma_poll(chan)))
				return ret;
		}
#ifdef CONFIG_ML1OM
		get_window_ref_count(src_window, 1);
		get_window_ref_count(dst_window, 1);
#endif
		if (is_local_dma_addr(src_dma_addr))
			src_virt = get_local_va(src_offset, src_window, loop_len);
		else
			src_virt = ioremap_remote_gtt(src_offset, src_window,
						loop_len, work->loopback,
						work->remote_dev, get_chan_num(chan), work);
		if (!src_virt) {
#ifdef CONFIG_ML1OM
			put_window_ref_count(src_window, 1);
			put_window_ref_count(dst_window, 1);
#endif
			return -ENOMEM;
		}

		if (is_local_dma_addr(dst_dma_addr))
			dst_virt = get_local_va(dst_offset, dst_window, loop_len);
		else
			dst_virt = ioremap_remote_gtt(dst_offset, dst_window,
						loop_len, work->loopback,
						work->remote_dev, get_chan_num(chan), work);
#ifdef CONFIG_ML1OM
		put_window_ref_count(src_window, 1);
		put_window_ref_count(dst_window, 1);
#endif
		if (!dst_virt) {
			if (!is_local_dma_addr(src_dma_addr))
				iounmap_remote(src_virt, loop_len, work);
			return -ENOMEM;
		}

		if (is_local_dma_addr(src_dma_addr)){
			micscif_unaligned_memcpy(dst_virt, src_virt, loop_len, work->ordered);
		}
		else{
			memcpy_fromio(dst_virt, src_virt, loop_len);
		}	
		serializing_request(dst_virt);
		smp_mb();
		if (!is_local_dma_addr(src_dma_addr))
			iounmap_remote(src_virt, loop_len, work);

		if (!is_local_dma_addr(dst_dma_addr))
			iounmap_remote(dst_virt, loop_len, work);

		remaining_len -= loop_len;
#ifdef CONFIG_MK1OM
		BUG_ON(remaining_len != 0);
#endif
#ifdef CONFIG_ML1OM
		if (remaining_len)
			return - ENXIO;
#endif
	}

	return ret;
}

int micscif_rma_list_dma_copy_wrapper(struct endpt *epd, struct mic_copy_work *work, struct dma_channel *chan, off_t loffset)
{
	int src_cache_off, dst_cache_off;
	uint64_t src_offset = work->src_offset, dst_offset = work->dst_offset;
	uint8_t *temp = NULL;
	bool src_local = true, dst_local = false;
	struct dma_completion_cb *comp_cb;
	dma_addr_t src_dma_addr, dst_dma_addr;
#ifndef _MIC_SCIF_
	struct pci_dev *pdev;
#endif

	src_cache_off = src_offset & (L1_CACHE_BYTES - 1);
	dst_cache_off = dst_offset & (L1_CACHE_BYTES - 1);
	if (dst_cache_off == src_cache_off)
		return micscif_rma_list_dma_copy_aligned(work, chan);

	if (work->loopback) {
#ifdef _MIC_SCIF_
		BUG_ON(micscif_rma_list_cpu_copy(work));
		return 0;
#else
		BUG_ON(1);
#endif
	}

	src_dma_addr = micscif_get_dma_addr(work->src_window, src_offset, NULL, NULL, NULL);
	dst_dma_addr = micscif_get_dma_addr(work->dst_window, dst_offset, NULL, NULL, NULL);

	if (is_local_dma_addr(src_dma_addr))
		src_local = true;
	else
		src_local = false;

	if (is_local_dma_addr(dst_dma_addr))
		dst_local = true;
	else
		dst_local = false;

	dst_local = dst_local;
	BUG_ON(work->len + (L1_CACHE_BYTES << 1) > KMEM_UNALIGNED_BUF_SIZE);

	/* Allocate dma_completion cb */
	if (!(comp_cb = kzalloc(sizeof(*comp_cb), GFP_KERNEL)))
		goto error;

	work->comp_cb = comp_cb;
	comp_cb->cb_cookie = (uint64_t)comp_cb;
	comp_cb->dma_completion_func = &micscif_rma_completion_cb;

	if (work->len + (L1_CACHE_BYTES << 1) < KMEM_UNALIGNED_BUF_SIZE) {
		comp_cb->is_cache = false;
		if (!(temp = kmalloc(work->len + (L1_CACHE_BYTES << 1), GFP_KERNEL)))
			goto free_comp_cb;
		comp_cb->temp_buf_to_free = temp;
		/* kmalloc(..) does not guarantee cache line alignment */
		if ((uint64_t)temp & (L1_CACHE_BYTES - 1))
			temp = (uint8_t*)ALIGN((uint64_t)temp, L1_CACHE_BYTES);
	} else {
		comp_cb->is_cache = true;
		if (!(temp = micscif_kmem_cache_alloc()))
			goto free_comp_cb;
		comp_cb->temp_buf_to_free = temp;
	}

	if (src_local) {
		temp += dst_cache_off;
		comp_cb->tmp_offset = dst_cache_off;
		micscif_rma_local_cpu_copy(work->src_offset, work->src_window, temp, work->len, true);
	} else {
		comp_cb->dst_window = work->dst_window;
		comp_cb->dst_offset = work->dst_offset;
		work->src_offset = work->src_offset - src_cache_off;
		comp_cb->len = work->len;
		work->len = ALIGN(work->len + src_cache_off, L1_CACHE_BYTES);
		comp_cb->header_padding = src_cache_off;
	}
	comp_cb->temp_buf = temp;

#ifndef _MIC_SCIF_
	micscif_pci_dev(work->remote_dev->sd_node, &pdev);
	comp_cb->temp_phys = mic_map_single(work->remote_dev->sd_node - 1,
			pdev, temp, KMEM_UNALIGNED_BUF_SIZE);

	if (mic_map_error(comp_cb->temp_phys)) {
		goto free_temp_buf;
	}

	comp_cb->remote_node = work->remote_dev->sd_node;
#endif
	if (0 > micscif_rma_list_dma_copy_unaligned(work, temp, chan, src_local))
		goto free_temp_buf;
	if (!src_local)
		work->fence_type = DO_DMA_INTR;
	return 0;
free_temp_buf:
	if (comp_cb->is_cache)
		micscif_kmem_cache_free(comp_cb->temp_buf_to_free);
	else
		kfree(comp_cb->temp_buf_to_free);
free_comp_cb:
	kfree(comp_cb);
error:
	printk(KERN_ERR "Unable to malloc %s %d\n", __func__, __LINE__);
	return -ENOMEM;
}

#if !defined(WINDOWS) && !defined(CONFIG_PREEMPT)
static int softlockup_threshold = 60;
static void avert_softlockup(unsigned long data)
{
	*(unsigned long*)data = 1;
}

/*
 * Add a timer to handle the case of hogging the cpu for
 * time > softlockup_threshold.
 * Add the timer every softlockup_threshold / 3 so that even if
 * there is a huge delay in running our timer, we will still don't hit
 * the softlockup case.(softlockup_tick() is run in hardirq() context while
 * timers are run at softirq context)
 *
 */
static inline void add_softlockup_timer(struct timer_list *timer, unsigned long *data)
{
	setup_timer(timer, avert_softlockup, (unsigned long) data);
	timer->expires = jiffies + usecs_to_jiffies(softlockup_threshold * 1000000 / 3);
	add_timer(timer);
}

static inline void del_softlockup_timer(struct timer_list *timer)
{
	/* We need delete synchronously since the variable being touched by
	 * timer interrupt is on the stack
	 */
	del_timer_sync(timer);
}
#endif

/*
 * micscif_rma_list_cpu_copy:
 *
 * Traverse all the windows and perform CPU copy.
 */
int micscif_rma_list_cpu_copy(struct mic_copy_work *work)
{
	void *src_virt, *dst_virt;
	size_t loop_len, remaining_len;
	int src_cache_off, dst_cache_off;
	uint64_t src_offset = work->src_offset, dst_offset = work->dst_offset;
	struct reg_range_t *src_window = work->src_window;
	struct reg_range_t *dst_window = work->dst_window;
	uint64_t end_src_offset, end_dst_offset;
	struct list_head *item;
    int srcchunk_ind = 0;
    int dstchunk_ind = 0;
    uint64_t src_start_offset, dst_start_offset;
	int ret = 0;
#if !defined(WINDOWS) && !defined(CONFIG_PREEMPT)
	unsigned long timer_fired = 0;
	struct timer_list timer;
	int cpu = smp_processor_id();
	add_softlockup_timer(&timer, &timer_fired);
#endif

	remaining_len = work->len;
    src_start_offset = src_window->offset;
    dst_start_offset = dst_window->offset;

	while (remaining_len) {
#if !defined(WINDOWS) && !defined(CONFIG_PREEMPT)
		/* Ideally we should call schedule only if we didn't sleep
		 * in between. But there is no way to know that.
		 */
		if (timer_fired) {
			timer_fired = 0;
			if (smp_processor_id() == cpu)
				touch_softlockup_watchdog();
			else
				cpu = smp_processor_id();
			add_softlockup_timer(&timer, &timer_fired);
		}
#endif
		src_cache_off = src_offset & ~PAGE_MASK;
		dst_cache_off = dst_offset & ~PAGE_MASK;
		loop_len = PAGE_SIZE -
			((src_cache_off > dst_cache_off) ?
			src_cache_off : dst_cache_off);
		if (remaining_len < loop_len)
			loop_len = remaining_len;

		if (RMA_WINDOW_SELF == src_window->type)
			src_virt = get_local_va(src_offset, src_window, loop_len);
		else
			src_virt = ioremap_remote(src_offset,
					src_window, loop_len, work->loopback, work->remote_dev, &srcchunk_ind, &src_start_offset);
		if (!src_virt) {
			ret = -ENOMEM;
			goto error;
		}

		if (RMA_WINDOW_SELF == dst_window->type)
			dst_virt = get_local_va(dst_offset, dst_window, loop_len);
		else
			dst_virt = ioremap_remote(dst_offset,
					dst_window, loop_len, work->loopback, work->remote_dev, &dstchunk_ind, &dst_start_offset);
		if (!dst_virt) {
			if (RMA_WINDOW_PEER == src_window->type)
				iounmap_remote(src_virt, loop_len, work);
			ret = -ENOMEM;
			goto error;
		}

		if (work->loopback)
			memcpy(dst_virt, src_virt, loop_len);
		else {

			if (RMA_WINDOW_SELF == src_window->type){
				memcpy_toio(dst_virt, src_virt, loop_len);
			}
			else{
				memcpy_fromio(dst_virt, src_virt, loop_len);
			}
			serializing_request(dst_virt);
			smp_mb();
		}
		if (RMA_WINDOW_PEER == src_window->type)
			iounmap_remote(src_virt, loop_len, work);

		if (RMA_WINDOW_PEER == dst_window->type)
			iounmap_remote(dst_virt, loop_len, work);

		src_offset += loop_len;
		dst_offset += loop_len;
		remaining_len -= loop_len;
		if (remaining_len) {
			end_src_offset = src_window->offset +
				(src_window->nr_pages << PAGE_SHIFT);
			end_dst_offset = dst_window->offset +
				(dst_window->nr_pages << PAGE_SHIFT);
			if (src_offset == end_src_offset) {
				item = (
					&src_window->list_member)->next;
				src_window = list_entry(item, 
						struct reg_range_t, 
						list_member);
                srcchunk_ind = 0;
                src_start_offset = src_window->offset;
			}
			if (dst_offset == end_dst_offset) {
				item = (
						&dst_window->list_member)->next;
				dst_window = list_entry(item, 
						struct reg_range_t, 
						list_member);
                dstchunk_ind = 0;
                dst_start_offset = dst_window->offset;
			}
		}
	}
error:
#if !defined(WINDOWS) && !defined(CONFIG_PREEMPT)
	del_softlockup_timer(&timer);
#endif
	return ret;
}
