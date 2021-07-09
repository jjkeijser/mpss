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
#ifndef SCIF_RMA_LIST_H
#define SCIF_RMA_LIST_H

/*
 * struct scif_rma_req - Self Registration list RMA Request query
 *
 * @out_window - Returns the window if found
 * @offset: Starting offset
 * @nr_bytes: number of bytes
 * @prot: protection requested i.e. read or write or both
 * @type: Specify single, partial or multiple windows
 * @head: Head of list on which to search
 * @va_for_temp: VA for searching temporary cached windows
 */
struct scif_rma_req {
	struct scif_window **out_window;
	union {
		s64 offset;
		unsigned long va_for_temp;
	};
	size_t nr_bytes;
	int prot;
	enum scif_window_type type;
	struct list_head *head;
};

/* Insert */
void scif_insert_window(struct scif_window *window, struct list_head *head);
void scif_insert_tcw(struct scif_window *window,
		     struct list_head *head);
/* Query */
int scif_query_window(struct scif_rma_req *request);
int scif_query_tcw(struct scif_endpt *ep, struct scif_rma_req *request);
/* Called from close to unregister all self windows */
int scif_unregister_all_windows(scif_epd_t epd);
void scif_unmap_all_windows(scif_epd_t epd);
/* Traverse list and unregister */
int scif_rma_list_unregister(scif_epd_t epd, s64 offset, int nr_pages);
#endif /* SCIF_RMA_LIST_H */
