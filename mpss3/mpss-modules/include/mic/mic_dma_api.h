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

#ifndef MIC_DMA_API_H
#define MIC_DMA_API_H

struct dma_channel;
/* API exported by the DMA library */

/*
 * Per MIC device (per MIC card) DMA handle. The card opens the handle to its own device.
 * The host opens the handle to the DMA devices of one of the cards.
 */
typedef void * mic_dma_handle_t; 

/* DMA Library Init/Uninit Routines */
int open_dma_device(int device_num, uint8_t *mmio_va_base, mic_dma_handle_t* dma_handle);

void close_dma_device(int device_num, mic_dma_handle_t *dma_handle);

/*
 * reserve_dma_channel - reserve a given dma channel for exclusive use
 *
 * @dma_handle - handle to DMA device returned by open_dma_device
 * @chan_num   - Channel number to be reserved
 * @chan       - set to point to the dma channel reserved by the call
 *
 * Returns < 1 on error (errorno)
 * Returns 0 on success
 */
int reserve_dma_channel(mic_dma_handle_t dma_handle, int chan_num, struct dma_channel **chan);

/*
 * allocate_dma_channel - dynamically allocate a dma channel (for a short while). Will 
 *    search for, choose, and lock down one channel for use by the calling thread.
 *
 * @dma_handle - handle to DMA device returned by open_dma_device
 * @chan - Returns the dma_channel pointer that was allocated by the call
 *
 * Returns < 1 on error
 * Returns 0 on success
 *
 * NOTE:  This function grabs a lock before exiting -- the calling thread MUST NOT
 *  sleep, and must call free_dma_channel before returning to user-space or switching
 *  volantarily to another thread.  Similarly, this function cannot be called from 
 *  an interrupt context at this time.
 */
int allocate_dma_channel(mic_dma_handle_t dma_handle, struct dma_channel **chan);

/*
 * request_dma_channel - Request a specific DMA channel.
 *
 * @dma_handle - handle to DMA device returned by open_dma_device
 * @chan - Returns the dma_channel pointer that was requested
 *
 * Returns: 0 on success and -ERESTARTSYS if the wait was interrupted
 * or -EBUSY if the channel was not available.
 *
 * NOTE:  This function grabs a lock before exiting -- the calling thread MUST NOT
 * sleep, and must call free_dma_channel before returning to user-space or switching
 * volantarily to another thread.  Similarly, this function cannot be called from
 * an interrupt context at this time.
 */
int request_dma_channel(struct dma_channel *chan);

/*
 * free_dma_channel - after allocating a channel, used to 
 *                 free the channel after DMAs are submitted
 *
 * @chan       - pointer to the dma_channel struct that was allocated
 *
 * Returns 0 on success, < 1 on error (errorno) 
 *
 * NOTE: This function must be called after all do_dma calls are finished,
 *  but can be called before the DMAs actually complete (as long as the comp_cb()
 *  handler in do_dma don't refer to the dma_channel struct).  If called with a
 *  dynamically allocated dma_channel, the caller must be the thread that called
 *  allocate_dma_channel.  When operating on a dynamic channel, free unlocks the 
 *  mutex locked in allocate.  Statically allocated channels cannot be freed,
 *  and calling this function with that type of channel will return an error.
 */
int free_dma_channel(struct dma_channel *chan);

/*
 * drain_dma_poll - Drain all outstanding DMA operations for a particular
 * DMA channel via polling.
 * @chan - DMA channel
 * Return 0 on success and -errno on error.
 */
int drain_dma_poll(struct dma_channel *chan);

/*
 * drain_dma_intr - Drain all outstanding DMA operations for a particular
 * DMA channel via interrupt based blocking wait.
 * @chan - DMA channel
 * Return 0 on success and -errno on error.
 */
int drain_dma_intr(struct dma_channel *chan);

/*
 * drain_dma_global - Drain all outstanding DMA operations for
 * all online DMA channel.
 * @block - Is it okay to block while operations are drained?
 * Return 0 on success and -errno on error.
 */
int drain_dma_global(mic_dma_handle_t dma_handle);

#ifdef _MIC_SCIF_
/*
 * dma_suspend: DMA tasks before transition to low power state.
 * @dma_handle: Handle for a DMA driver context.
 */
void dma_suspend(mic_dma_handle_t dma_handle);

/*
 * dma_resume: DMA tasks after wake up from low power state.
 * @dma_handle: Handle for a DMA driver context.
 */
void dma_resume(mic_dma_handle_t dma_handle);
#else
/*
 * dma_prep_suspend: DMA tasks required on host before a device can transition
 * to a low power state.
 * @dma_handle: Handle for a DMA driver context.
 */
void dma_prep_suspend(mic_dma_handle_t dma_handle);
#endif

static inline void mic_dma_thread_free_chan(struct dma_channel *chan)
{
	free_dma_channel(chan);
}
#ifndef _MIC_SCIF_
//extern struct mutex lock_dma_dev_init;
void host_dma_interrupt_handler(mic_dma_handle_t dma_handle, uint32_t sboxSicr0Reg);
#endif

#endif /* MIC_DMA_API_H */
