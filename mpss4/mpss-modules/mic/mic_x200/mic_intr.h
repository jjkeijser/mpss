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
 * Intel MIC X200 PCIe driver.
 */
#ifndef _MIC_INTR_H_
#define _MIC_INTR_H_

#include <linux/bitops.h>
#include <linux/interrupt.h>

#define MIC_NUM_OFFSETS 32
/**
 * struct mic_intr_info - Contains h/w specific interrupt sources
 * information.
 *
 * @intr_start_idx: Contains the starting indexes of the
 * interrupt types.
 * @intr_len: Contains the length of the interrupt types.
 */
struct mic_intr_info {
	u16 intr_start_idx;
	u16 intr_len;
};

/**
 * struct mic_irq_info - OS specific irq information
 *
 * @next_avail_src: next available doorbell that can be assigned.
 * @cb_ida: callback ID allocator to track the callbacks registered.
 * @mic_intr_lock: spinlock to protect the interrupt callback list.
 * @mic_thread_lock: spinlock to protect the thread callback list.
 *		   This lock is used to protect against thread_fn while
 *		   mic_intr_lock is used to protect against interrupt handler.
 * @cb_list: Array of callback lists one for each source.
 * @mask: Mask used by the main thread fn to call the underlying thread fns.
 */
struct mic_irq_info {
	int next_avail_src;
	struct ida cb_ida;
	spinlock_t mic_intr_lock;
	spinlock_t mic_thread_lock;
	struct list_head *cb_list;
	unsigned long mask;
};

/**
 * struct mic_intr_cb - Interrupt callback structure.
 *
 * @handler: The callback function
 * @thread_fn: The thread_fn.
 * @data: Private data of the requester.
 * @cb_id: The callback id. Identifies this callback.
 * @list: list head pointing to the next callback structure.
 */
struct mic_intr_cb {
	irq_handler_t handler;
	irq_handler_t thread_fn;
	void *data;
	int cb_id;
	struct list_head list;
};

/* Forward declaration */
struct mic_device;

int mic_next_db(struct mic_device *xdev);
struct mic_irq *
mic_request_threaded_irq(struct mic_device *xdev,
			 irq_handler_t handler, irq_handler_t thread_fn,
			 const char *name, void *data, int intr_src);
void mic_free_irq(struct mic_device *xdev,
		  struct mic_irq *cookie, void *data);
int mic_setup_interrupts(struct mic_device *xdev, struct pci_dev *pdev);
void mic_free_interrupts(struct mic_device *xdev, struct pci_dev *pdev);
#endif
