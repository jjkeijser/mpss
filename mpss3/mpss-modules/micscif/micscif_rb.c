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
#include "mic/micscif_rb.h"

#include <linux/circ_buf.h>
#include <linux/module.h>
#define count_in_ring(head, tail, size)    CIRC_CNT(head, tail, size)
#define space_in_ring(head, tail, size)    CIRC_SPACE(head, tail, size)

MODULE_LICENSE("GPL");

static void *micscif_rb_get(struct micscif_rb *rb, uint32_t size);

/**
 * micscif_rb_init - To Initialize the RingBuffer
 * @rb: The RingBuffer context
 * @read_ptr: A pointer to the memory location containing
 * the updated read pointer
 * @write_ptr: A pointer to the memory location containing
 * the updated write pointer
 * @rb_base: The pointer to the ring buffer
 * @size: The size of the ring buffer
 */
void micscif_rb_init(struct micscif_rb *rb,
		volatile uint32_t *read_ptr,
		volatile uint32_t *write_ptr,
		volatile void *rb_base,
		const uint32_t size)
{
	/* Size must be a power of two -- all logic assoicated with
	 * incrementing the read and write pointers relies on the size
	 * being a power of 2
	 */
	BUG_ON((size & (size-1)) != 0);
	rb->rb_base = rb_base;
	rb->size = size;
	rb->read_ptr = read_ptr;
	rb->write_ptr = write_ptr;
	rb->current_read_offset = *read_ptr;
	rb->current_write_offset = *write_ptr;
}
EXPORT_SYMBOL(micscif_rb_init);

/**
 * micscif_rb_reset - To reset the RingBuffer
 * @rb - The RingBuffer context
 */
void micscif_rb_reset(struct micscif_rb *rb)
{
	/*
	 * XPU_RACE_CONDITION: write followed by read
	 * MFENCE after write
	 * Read should take care of SBOX sync
	 * Ponters are volatile (see RingBuffer declaration)
	 */
	*rb->read_ptr = 0x0;
	*rb->write_ptr = 0x0;
	smp_mb();
	rb->current_write_offset = *rb->write_ptr;
	rb->current_read_offset = *rb->read_ptr;
}
EXPORT_SYMBOL(micscif_rb_reset);

/* Copies a message to the ring buffer -- handles the wrap around case */
static int memcpy_torb(struct micscif_rb *rb, void *header,
			void *msg, uint32_t size)
{
	/* Need to call two copies if it wraps around */
	uint32_t size1, size2;
	if ((char*)header + size >= (char*)rb->rb_base + rb->size) {
		size1 = (uint32_t) ( ((char*)rb->rb_base + rb->size) - (char*)header);
		size2 = size - size1;
		memcpy_toio(header, msg, size1);
		memcpy_toio(rb->rb_base, (char*)msg+size1, size2);
	} else {
		memcpy_toio(header, msg, size);
	}
	return 0;
}

/* Copies a message from the ring buffer -- handles the wrap around case */
static int memcpy_fromrb(struct micscif_rb *rb, void *header,
			void *msg, uint32_t size)
{
	/* Need to call two copies if it wraps around */
	uint32_t size1, size2;
	if ((char*)header + size >= (char*)rb->rb_base + rb->size) {
		size1 = (uint32_t) ( ((char*)rb->rb_base + rb->size) - (char*)header );
		size2 = size - size1;
		memcpy_fromio(msg, header, size1);
		memcpy_fromio((char*)msg+size1, rb->rb_base, size2);
	} else {
		memcpy_fromio(msg, header, size);
	}
	return 0;
}

/**
 * micscif_rb_space -
 * Query space available for writing to the given RB.
 *
 * @rb - The RingBuffer context
 *
 * Returns: size available for writing to RB in bytes.
 */
int micscif_rb_space(struct micscif_rb *rb)
{
	rb->old_current_read_offset = rb->current_read_offset;

	rb->current_read_offset = *rb->read_ptr;
	return space_in_ring(rb->current_write_offset,
		rb->current_read_offset, rb->size);
}
EXPORT_SYMBOL(micscif_rb_space);

/**
 * micscif_rb_write - Write one package to the given ring buffer
 * @rb - The RingBuffer context
 * @msg - The package to be put in the ring buffer
 * @size - the size (in bytes) you want to copy
 *
 * This API does not block if there isn't enough space in the RB.
 */
int micscif_rb_write(struct micscif_rb *rb,
			void *msg,
			uint32_t size)
{
	void *header;
	int ret = 0;
	if ((uint32_t)micscif_rb_space(rb) < size)
		return -ENOMEM;
	header = (char*)rb->rb_base + rb->current_write_offset;
	ret = memcpy_torb(rb, header, msg, size);
	if (!ret) {
		/*
		 * XPU_RACE_CONDITION: Don't do anything here!
		 * Wait until micscif_rb_commit()
		 * Update the local ring buffer data, not the shared data until commit.
		 */
		rb->old_current_write_offset = rb->current_write_offset;
		rb->current_write_offset = (rb->current_write_offset + size) & (rb->size - 1);
	}
	return ret;
}
EXPORT_SYMBOL(micscif_rb_write);

/*
 * micscif_rb_get_next
 * Read from ring buffer.
 * @rb - The RingBuffer context
 * @msg - buffer to hold the message.  Must be at least size bytes long
 * @size - Size to be read out passed in, actual bytes read
 *          is returned.
 * RETURN:
 * Returns the number of bytes possible to read -- if retVal != size, then
 * the read does not occur.
 */
int micscif_rb_get_next (struct micscif_rb *rb, void *msg, uint32_t size)
{
	void *header = NULL;
	int read_size = 0;
	/*
	 * warning:  RingBufferGet() looks at the shared write pointer
	 */
	header = micscif_rb_get(rb, size);
	if (header) {
		uint32_t next_cmd_offset =
			(rb->current_read_offset + size) & (rb->size - 1);
		read_size = size;
		rb->old_current_read_offset = rb->current_read_offset;
		rb->current_read_offset = next_cmd_offset;
		if (memcpy_fromrb(rb, header, msg, size))  // add check here
			return -EFAULT;
	}
	return read_size;
}
EXPORT_SYMBOL(micscif_rb_get_next);

/**
 * micscif_rb_update_read_ptr
 * @rb - The RingBuffer context
 */
void micscif_rb_update_read_ptr(struct micscif_rb *rb)
{
	uint32_t old_offset;
	uint32_t new_offset;
	smp_mb();
	old_offset = rb->old_current_read_offset;
	new_offset = rb->current_read_offset;

	/*
	 * XPU_RACE_CONDITION:
	 * pReadPointer is ready to move
	 * Moving read pointer transfers ownership to MIC
	 * What if MICCPU starts writing to buffer before all
	 * writes were flushed?
	 * Need to flush out all pending writes before pointer update
	 */
	smp_mb();

#ifdef CONFIG_ML1OM
	serializing_request((volatile uint8_t*) rb->rb_base+old_offset);
#endif

	*rb->read_ptr = new_offset;
#ifdef CONFIG_ML1OM
	/*
	 * Readback since KNF doesn't guarantee that PCI ordering is maintained.
	 * Need a memory barrier on the host before the readback so the readback
	 * doesn't load from the write combining buffer but will go across to the
	 * PCI bus that will then flush the posted write to the device.
	 */
	smp_mb();
	serializing_request(rb->read_ptr);
#endif
#if defined(CONFIG_MK1OM) && defined(_MIC_SCIF_)
	/*
	 * KNC Si HSD 3853952: For the case where a Core is performing an EXT_WR
	 * followed by a Doorbell Write, the Core must perform two EXT_WR to the
	 * same address with the same data before it does the Doorbell Write.
	 * This way, if ordering is violate for the Interrupt Message, it will
	 * fall just behind the first Posted associated with the first EXT_WR.
	 */
	*rb->read_ptr = new_offset;
#endif
	smp_mb();
}
EXPORT_SYMBOL(micscif_rb_update_read_ptr);

/**
 * micscif_rb_count
 * @rb - The RingBuffer context
 * RETURN: number of empty slots in the RB
 */
uint32_t micscif_rb_count(struct micscif_rb *rb, uint32_t size)
{
	if (count_in_ring(rb->current_write_offset,
			rb->current_read_offset,
			rb->size) < size) {
		/*
		 * Update from the HW write pointer if empty
		 */
		rb->old_current_write_offset = rb->current_write_offset;
		rb->current_write_offset = *rb->write_ptr;
	}
	return count_in_ring(rb->current_write_offset,
			rb->current_read_offset,
			rb->size);
}
EXPORT_SYMBOL(micscif_rb_count);

/**
 * micscif_rb_commit
 * To submit the buffer to let the uOS to fetch it
 * @rb - The RingBuffer context
 */
void micscif_rb_commit(struct micscif_rb *rb)
{
	/*
	 * XPU_RACE_CONDITION:
	 * Writing to ringbuffer memory before updating the pointer
	 * can be out-of-order and write combined.
	 * This is the point where we start to care about
	 * consistency of the data.
	 * There are two race conditions below:
	 * (1) Ring buffer pointer moves before all data is flushed:
	 * if uOS is late taking the interrupt for the previous transaction,
	 * it may take the new write pointer immediately
	 * and start accessing data in the ringbuffer.
	 * Ring buffer data must be consistent before we update the write
	 * pointer. We read back the address at oldCurrentWriteOffset
	 * -- this is the location in memory written during the last
	 * ring buffer operation; keep in mind that ring buffers and ring buffer
	 * pointers can be in different kinds of memory (host vs MIC,
	 * depending on currently active workaround flags.
	 * (2) If uOS takes interrupt while write pointer value is still
	 * in-flight may result in uOS reading old value, message being lost,
	 * and the deadlock. Must put another memory barrier after readback --
	 * revents read-passing-read from later read
	 */
	smp_mb();
#ifdef CONFIG_ML1OM
	/*
	 * Also makes sure the following read is not reordered
	 */
	serializing_request((char*)rb->rb_base + rb->current_write_offset);
#endif
	*rb->write_ptr = rb->current_write_offset;
#ifdef CONFIG_ML1OM
	/*
	 * Readback since KNF doesn't guarantee that PCI ordering is maintained.
	 * Need a memory barrier on the host before the readback so the readback
	 * doesn't load from the write combining buffer but will go across to the
	 * PCI bus that will then flush the posted write to the device.
	 */
	smp_mb();
	serializing_request(rb->write_ptr);
#endif
#if defined(CONFIG_MK1OM) && defined(_MIC_SCIF_)
	/*
	 * KNC Si HSD 3853952: For the case where a Core is performing an EXT_WR
	 * followed by a Doorbell Write, the Core must perform two EXT_WR to the
	 * same address with the same data before it does the Doorbell Write.
	 * This way, if ordering is violate for the Interrupt Message, it will
	 * fall just behind the first Posted associated with the first EXT_WR.
	 */
	*rb->write_ptr = rb->current_write_offset;
#endif
	smp_mb();
}
EXPORT_SYMBOL(micscif_rb_commit);

/**
 * micscif_rb_get
 * To get next packet from the ring buffer
 * @rb - The RingBuffer context
 * RETURN:
 * NULL if no packet in the ring buffer
 * Otherwise The pointer of the next packet
 */
static void *micscif_rb_get(struct micscif_rb *rb, uint32_t size)
{
	void *header = NULL;

	if (micscif_rb_count(rb, size) >= size)
		header = (char*)rb->rb_base + rb->current_read_offset;
	return header;
}

/**
 * micscif_rb_get_version
 * Return the ring buffer module version
 */
uint16_t micscif_rb_get_version(void)
{
	return RING_BUFFER_VERSION;
}
EXPORT_SYMBOL(micscif_rb_get_version);
