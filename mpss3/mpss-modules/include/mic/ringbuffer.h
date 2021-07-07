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

/*
Description: This is a generic ring buffer implementation to be used by
anyone who needs a ring buffer.  The ring buffer is maipulated
using Read and Write functions.  These functions perform all of
the necessary space checks and only complete the operation if
if the requested number of items can be read or written.  A
return value of false indicates that either the ring buffer
contains less then the requested number of items (for Read) or
there isn't enough space left in the ring buffer (for Write).
*/

#ifndef _MICHOST_RING_BUFFER_DEFINE

#define _MICHOST_RING_BUFFER_DEFINE

//
// Requirements:
// Ring base should be already aligned properly
// Ring size should be just multiple of the alignment size
// All packets should be at least multiple of 4 bytes for the purpose of padding
//

#define RINGBUFFER_ALIGNMENT_SIZE 64 // in byte

typedef struct _ringbuffer
{
	uint8_t *ringbuff_ptr;
	volatile uint32_t *readptr;	// Points to the read offset
	volatile uint32_t *writeptr;	// Points to the write offset
	uint32_t ringbuffsize;
	uint32_t curr_readoffset;	// cache it to improve performance.
	uint32_t curr_writeoffset;	// cache it to improve performance.
	uint32_t old_readoffset;
	uint32_t old_writeoffset;
} ringbuffer;

// Commands common across all ring buffers
typedef enum _rb_cmdopcode
{
	// note: don't use 0, because the ring buffer
	//   is initialized to a bunch of 0's that aren't really commands.
	MIC_RBCT_ERROR = 0x0,		// an error has occurred if encountered
	MIC_RBCT_NOP,			// Used to skip empty space in the ringbuffer.
	MIC_RBCT_DMAEXEC,		// DMA buffer to transfer/execute
	MIC_RBCT_SHUTDOWN,		// bus power-down eminent
	MIC_RBCT_CREATESTDPROCESS,	// Launches an executable on the ramdisk.
	MIC_RBCT_CREATENATIVEPROCESS,	// Launches a native process.
	  // NRFIX : not implemented. If native apps are launched by loading shared
	  // libraries(DLLs) into a standard stub app then this command goes away.
	MIC_RBCT_DESTROYPROCESS,	// Destroys a process.
	MIC_RBCT_VIRTUALALLOC,		// Creates a uOS virtual address range
	MIC_RBCT_MAPHOSTMEMORY,		// Used by implement host kernel mode driver services
	MIC_RBCT_UNMAPHOSTMEMORY,	// Unmaps host memory
	MIC_RBCT_UOSESCAPE,		// Used to pass uOS escapes from the host
	MIC_RBCT_RESERVED1,		// Reserved for future use
	MIC_RBCT_RESERVED2,		// Reserved for future use
	MIC_RBCT_UPLOADSTDAPPLICATION,	// Uploads a standard application to the uOS
	MIC_RBCT_CREATEUOSRESOURCE,	// Creates a DPT page cache
	MIC_RBCT_DESTROYUOSRESOURCE,	// Destroys a DPT page cache
	MIC_RBCT_RESERVE_RING_BANDWIDTH_DBOX_TRAFFIC, // Reserves a ring bandwidth for DBOX traffic

	// Following commands are from MIC->Host (CRBT => CPU ring buffer.)
	MIC_CRBT_LOG_INFO,		// Host logs information sent by the uOS.

	// Always make these the last ones in the list
#if defined(MIC_DEBUG) || defined(ENABLE_GFXDEBUG)
	MIC_RBCT_READPHYSICALMEMORY = 0x8000, // Used by debug tools to read memory on the device
	MIC_RBCT_WRITEPHYSICALMEMORY,	// Used by debug tools to write memory on the device
#endif // defined(MIC_DEBUG) || defined(ENABLE_GFXDEBUG)
	MIC_RBCT_CMD_MAX		// No valid OpCodes above this one
}ringbuff_cmdop;

typedef struct _ringbuff_cmdhdr
{
	ringbuff_cmdop	opcode:16;
	uint32_t	size:16;
}ringbuff_cmdhdr;

#ifdef __cplusplus
extern "C" {
#endif

//---------------------------------
// methods used by both
//---------------------------------
// initialize cached ring buffer structure
void rb_initialize(ringbuffer* ringbuff, volatile uint32_t* readptr,
		   volatile uint32_t* writeptr, void *buff, const uint32_t size);

//---------------------------------
// writer-only methods
//---------------------------------
// write a new command.  Must follow with fence/MMIO, then RingBufferCommit()
int rb_write(ringbuffer* ringbuff, ringbuff_cmdhdr* cmd_header);
// After write(), do an mfence(), an MMIO write to serialize, then Commit()
void rb_commit(ringbuffer* ringbuff);
// used on power state change to reset cached pointers
void rb_reset(ringbuffer* ringbuff);
// used to determine the largest possible command that could be sent next
uint32_t rb_get_available_space(ringbuffer* ringbuff);

// TODO:  It may be more optimal to have "Reserve" function exposed to the client
//        instead of requiring it to create a command that will be copied into the ring buffer.


//---------------------------------
// reader-only methods
//---------------------------------
// uses (updates) the cached read pointer to get the next command, so writer doesn't
// see the command as consumed
ringbuff_cmdhdr* rb_get_next_cmd(ringbuffer* ringbuff);
// updates the control block read pointer, which will be visible to the writer so it
// can re-use the space
void  rb_update_readptr(ringbuffer* ringbuff, ringbuff_cmdhdr* cmd_header);
// reader skips all commands, updating its next read offset
void rb_skip_to_offset(ringbuffer* ringbuff, uint32_t new_readptr);

// uOS used this method to determine if RingBuffer is empty or not before attempting
// to fetch command out of ring buffer If ringbuffer is empty, means uOS would have
// fetched it earlier.
uint32_t rb_empty(ringbuffer* ringbuff);

// only used by host simulator
void rb_sync(ringbuffer* ringbuff);

#ifdef __cplusplus
}
#endif

#ifdef __LINUX_GPL__
//==============================================================================
//  FUNCTION: AlignLow
//
//  DESCRIPTION: Returns trunk(in_data / in_granularity) * in_granularity
//
//  PARAMETERS:
//      in_data - Data to be aligned
//      in_granularity - Alignment chunk size - must be a power of 2
#if defined(__cplusplus)
template <typename TData>
#else // no C++
#define TData uint64_t
#endif // if C++

static inline TData AlignLow(TData in_data, uintptr_t in_granularity)
{
	TData mask = (TData)(in_granularity-1); // 64 -> 0x3f

	// floor to granularity
	TData low = in_data & ~mask;

	return low;
}

#if !defined(__cplusplus)
#undef TData
#endif // if no C++
#endif // __LINUX_GPL_

#endif //_MICHOST_RING_BUFFER_DEFINE
