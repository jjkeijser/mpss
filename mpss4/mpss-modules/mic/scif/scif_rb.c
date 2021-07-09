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
#include <linux/circ_buf.h>
#include <linux/types.h>
#include <linux/io.h>
#include <linux/errno.h>

#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
#define ACCESS_ONCE(x) (*(volatile typeof(x) *)&(x))
#endif

#include "scif_rb.h"

static inline u32 scif_rb_ring_cnt(u32 head, u32 tail, u32 size)
{
	return CIRC_CNT(head, tail, size);
}

static inline u32 scif_rb_ring_space(u32 head, u32 tail, u32 size)
{
	return CIRC_SPACE(head, tail, size);
}

/**
 * scif_rb_init - Initializes the ring buffer
 * @rb: ring buffer
 * @read_ptr: A pointer to the read offset
 * @write_ptr: A pointer to the write offset
 * @rb_base: A pointer to the base of the ring buffer
 * @size: The size of the ring buffer in powers of two
 */
void scif_rb_init(struct scif_rb *rb, u32 *read_ptr, u32 *write_ptr,
		  void *rb_base, u8 size)
{
	rb->rb_base = rb_base;
	rb->size = (1 << size);
	rb->read_ptr = read_ptr;
	rb->write_ptr = write_ptr;
	rb->current_read_offset = *read_ptr;
	rb->current_write_offset = *write_ptr;
}

/* Copies a message to the ring buffer -- handles the wrap around case */
static void memcpy_torb(struct scif_rb *rb, void *header,
			void *msg, u32 size)
{
	u32 size1, size2;

	if (header + size >= rb->rb_base + rb->size) {
		/* Need to call two copies if it wraps around */
		size1 = (u32)(rb->rb_base + rb->size - header);
		size2 = size - size1;
		memcpy_toio((void __iomem __force *)header, msg, size1);
		memcpy_toio((void __iomem __force *)rb->rb_base,
			    msg + size1, size2);
	} else {
		memcpy_toio((void __iomem __force *)header, msg, size);
	}
}

/* Copies a message from the ring buffer -- handles the wrap around case */
static void memcpy_fromrb(struct scif_rb *rb, void *header,
			  void *msg, u32 size)
{
	u32 size1, size2;

	if (header + size >= rb->rb_base + rb->size) {
		/* Need to call two copies if it wraps around */
		size1 = (u32)(rb->rb_base + rb->size - header);
		size2 = size - size1;
		memcpy_fromio(msg, (void __iomem __force *)header, size1);
		memcpy_fromio(msg + size1,
			      (void __iomem __force *)rb->rb_base, size2);
	} else {
		memcpy_fromio(msg, (void __iomem __force *)header, size);
	}
}

/**
 * scif_rb_space - Query space available for writing to the RB
 * @rb: ring buffer
 *
 * Return: size available for writing to RB in bytes.
 */
u32 scif_rb_space(struct scif_rb *rb)
{
	return scif_rb_ring_space(rb->current_write_offset,
				  ACCESS_ONCE(*rb->read_ptr), rb->size);
}

/**
 * scif_rb_write - Write a message to the RB
 * @rb: ring buffer
 * @msg: buffer to send the message.  Must be at least size bytes long
 * @size: the size (in bytes) to be copied to the RB
 *
 * This API does not block if there isn't enough space in the RB.
 * Returns: 0 on success or -ENOMEM on failure
 */
int scif_rb_write(struct scif_rb *rb, void *msg, u32 size)
{
	void *header;

	if (scif_rb_space(rb) < size)
		return -ENOMEM;
	header = rb->rb_base + rb->current_write_offset;
	memcpy_torb(rb, header, msg, size);
	/*
	 * Wait until scif_rb_commit(). Update the local ring
	 * buffer data, not the shared data until commit.
	 */
	rb->current_write_offset =
		(rb->current_write_offset + size) & (rb->size - 1);
	return 0;
}

/**
 * scif_rb_commit - To submit the message to let the peer fetch it
 * @rb: ring buffer
 */
void scif_rb_commit(struct scif_rb *rb)
{
	/*
	 * This barrier ensures that all the data is written to the
	 * ring before the peer is notified by updating the ring's head
	 * (rb->write_ptr).
	 */
	wmb();
	ACCESS_ONCE(*rb->write_ptr) = rb->current_write_offset;
#ifdef CONFIG_INTEL_MIC_X100_CARD
	/*
	 * X100 Si bug: For the case where a Core is performing an EXT_WR
	 * followed by a Doorbell Write, the Core must perform two EXT_WR to the
	 * same address with the same data before it does the Doorbell Write.
	 * This way, if ordering is violated for the Interrupt Message, it will
	 * fall just behind the first Posted associated with the first EXT_WR.
	 */
	ACCESS_ONCE(*rb->write_ptr) = rb->current_write_offset;
#endif
	/*
	 * This barrier assures that ring's head updates are realized
	 * in correct order among local cores. This assumes correct
	 * synchronization via locking.
	 */
	wmb();
}

/**
 * scif_rb_get - To get next message from the ring buffer
 * @rb: ring buffer
 * @size: Number of bytes to be read
 *
 * Return: NULL if no bytes to be read from the ring buffer, otherwise the
 *	pointer to the next byte
 */
static void *scif_rb_get(struct scif_rb *rb, u32 size)
{
	void *header = NULL;

	if (scif_rb_count(rb) >= size)
		header = rb->rb_base + rb->current_read_offset;
	return header;
}

/*
 * scif_rb_get_next - Read from ring buffer.
 * @rb: ring buffer
 * @msg: buffer to hold the message.  Must be at least size bytes long
 * @size: Number of bytes to be read
 *
 * Return: number of bytes read if available bytes are >= size, otherwise
 * returns zero.
 */
u32 scif_rb_get_next(struct scif_rb *rb, void *msg, u32 size)
{
	void *header = NULL;
	int read_size = 0;

	header = scif_rb_get(rb, size);
	if (header) {
		u32 next_cmd_offset =
			(rb->current_read_offset + size) & (rb->size - 1);

		read_size = size;
		rb->current_read_offset = next_cmd_offset;
		memcpy_fromrb(rb, header, msg, size);
	}
	return read_size;
}

/**
 * scif_rb_update_read_ptr
 * @rb: ring buffer
 */
void scif_rb_update_read_ptr(struct scif_rb *rb)
{
	/*
	 * This barrier ensures that all the data from ring is
	 * read before the peer is notified by updating the ring's tail
	 * (rb->read_ptr).
	 */
	mb();
	ACCESS_ONCE(*rb->read_ptr) = rb->current_read_offset;
#ifdef CONFIG_INTEL_MIC_X100_CARD
	/*
	 * X100 Si Bug: For the case where a Core is performing an EXT_WR
	 * followed by a Doorbell Write, the Core must perform two EXT_WR to the
	 * same address with the same data before it does the Doorbell Write.
	 * This way, if ordering is violated for the Interrupt Message, it will
	 * fall just behind the first Posted associated with the first EXT_WR.
	 */
	ACCESS_ONCE(*rb->read_ptr) = rb->current_read_offset;
#endif
	/*
	 * This barrier assures that ring's tail updates are realized
	 * in correct order among local cores. This assumes correct
	 * synchronization via locking.
	 */
	wmb();
}

/**
 * scif_rb_count
 * @rb: ring buffer
 * @size: Number of bytes expected to be read
 *
 * Return: number of bytes that can be read from the RB
 */
u32 scif_rb_count(struct scif_rb *rb)
{
	return scif_rb_ring_cnt(ACCESS_ONCE(*rb->write_ptr),
				rb->current_read_offset,
				rb->size);
}
