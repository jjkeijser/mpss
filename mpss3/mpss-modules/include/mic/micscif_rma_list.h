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

#ifndef MICSCIF_RMA_LIST_H
#define MICSCIF_RMA_LIST_H

/*
 * RMA Linked List Manipulation API's.
 * Callee must Hold RMA lock to call the API's below.
 * When and if RMA uses RB trees for log(n) search,
 * similar API's should be implemented.
 */

/*
 * Specifies whether an RMA operation can span
 * across partial windows, a single window or multiple
 * contiguous windows.
 * Mmaps can span across parial windows.
 * Unregistration can span across complete windows.
 * scif_get_pages() can span a single window.
 */
enum range_request {
	WINDOW_PARTIAL,
	WINDOW_SINGLE,
	WINDOW_FULL
};

/* Self Registration list RMA Request query */
struct micscif_rma_req {
	struct reg_range_t **out_window;
	uint64_t offset;
	size_t nr_bytes;
	int prot;
	enum range_request type;
	struct list_head *head;
	void *va_for_temp;
};

/**
 * struct mic_copy_work:
 *
 * Work for DMA copy thread is provided by alloocating and preparing
 * struct mic_copy_work and calling mic_enqueue_copy_work.
 */
struct mic_copy_work {
	uint64_t src_offset;

	uint64_t dst_offset;

	/* Starting src registered window */
	struct reg_range_t *src_window;

	/* Starting dst registered window */
	struct reg_range_t *dst_window;

	/* Is this transfer a loopback transfer? */
	int loopback;

	size_t len;
	/* DMA copy completion callback. Details in mic_dma_lib.h */
	struct dma_completion_cb   *comp_cb;

	struct micscif_dev	*remote_dev;

	/* DO_DMA_POLLING or DO_DMA_INTR or none */
	int fence_type;

	bool ordered;

#ifdef CONFIG_ML1OM
	/* GTT map state */
	enum micscif_msg_state	gttmap_state;

	/* Wait Queue for a GTT map (N)ACK */
	wait_queue_head_t	gttmapwq;

	uint64_t		gtt_offset;

	uint64_t		gtt_length;

#endif
	bool			dma_chan_released;
	struct list_head list_member;
};

/* Insert */
void micscif_insert_window(struct reg_range_t *window, struct list_head *head);
void micscif_insert_tcw(struct reg_range_t *window,
					struct list_head *head);

/* Query */
int micscif_query_window(struct micscif_rma_req *request);
int micscif_query_tcw(struct endpt *ep, struct micscif_rma_req *request);

/* Called from close to unregister all self windows */
int micscif_unregister_all_windows(scif_epd_t epd);

/* Traverse list and munmap */
void micscif_rma_list_munmap(struct reg_range_t *window, uint64_t offset, int nr_pages);
/* Traverse list and mmap */
int micscif_rma_list_mmap(struct reg_range_t *start_window,
			  uint64_t offset, int nr_pages, struct vm_area_struct *vma);
/* Traverse list and unregister */
int micscif_rma_list_unregister(struct reg_range_t *window, uint64_t offset, int nr_pages);

/* CPU copy */
int micscif_rma_list_cpu_copy(struct mic_copy_work *work);

/* Traverse remote RAS and ensure none of the get_put_ref_counts are +ve */
int micscif_rma_list_get_pages_check(struct endpt *ep);

/* Debug API's */
void micscif_display_all_windows(struct list_head *head);

int micscif_rma_list_dma_copy_wrapper(struct endpt *epd, struct mic_copy_work *work, struct dma_channel *chan, off_t loffset);

void micscif_rma_local_cpu_copy(uint64_t offset, struct reg_range_t *window, uint8_t *temp, size_t remaining_len, bool to_temp);

#endif /* MICSCIF_RMA_LIST_H */
