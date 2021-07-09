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
#include <linux/pci.h>
#include <linux/interrupt.h>

#include "../common/mic_dev.h"
#include "mic_device.h"
#include "mic_hw.h"

static irqreturn_t mic_thread_fn(int irq, void *dev)
{
	struct mic_device *xdev = dev;
	struct mic_intr_info *intr_info = xdev->intr_info;
	struct mic_irq_info *irq_info = &xdev->irq_info;
	struct mic_intr_cb *intr_cb;
	struct pci_dev *pdev = container_of(&xdev->pdev->dev,
					    struct pci_dev, dev);
	int i;

	spin_lock(&irq_info->mic_thread_lock);
	for (i = intr_info->intr_start_idx;
			i < intr_info->intr_len; i++)
		if (test_and_clear_bit(i, &irq_info->mask)) {
			list_for_each_entry(intr_cb, &irq_info->cb_list[i],
					    list)
				if (intr_cb->thread_fn)
					intr_cb->thread_fn(pdev->irq,
							 intr_cb->data);
		}
	spin_unlock(&irq_info->mic_thread_lock);
	return IRQ_HANDLED;
}

/**
 * mic_interrupt - Generic interrupt handler for
 * MSI and INTx based interrupts.
 */
static irqreturn_t mic_interrupt(int irq, void *dev)
{
	struct mic_device *xdev = dev;
	struct mic_intr_info *intr_info = xdev->intr_info;
	struct mic_irq_info *irq_info = &xdev->irq_info;
	struct mic_intr_cb *intr_cb;
	struct pci_dev *pdev = container_of(&xdev->pdev->dev,
					    struct pci_dev, dev);
	u32 mask;
	int i;

	mask = mic_ack_interrupt(xdev);
	if (!mask)
		return IRQ_NONE;

	spin_lock(&irq_info->mic_intr_lock);
	for (i = intr_info->intr_start_idx;
			i < intr_info->intr_len; i++)
		if (mask & BIT(i)) {
			list_for_each_entry(intr_cb, &irq_info->cb_list[i],
					    list)
				if (intr_cb->handler)
					intr_cb->handler(pdev->irq,
							 intr_cb->data);
			set_bit(i, &irq_info->mask);
		}
	spin_unlock(&irq_info->mic_intr_lock);
	return IRQ_WAKE_THREAD;
}

/* Return the interrupt offset from the index. Index is 0 based. */
static u16 mic_map_src_to_offset(struct mic_device *xdev, int intr_src)
{
	if (intr_src >= xdev->intr_info->intr_len)
		return MIC_NUM_OFFSETS;

	return xdev->intr_info->intr_start_idx + intr_src;
}

/**
 * mic_register_intr_callback - Register a callback handler for the
 * given source id.
 *
 * @xdev: pointer to the mic_device instance
 * @idx: The source id to be registered.
 * @handler: The function to be called when the source id receives
 * the interrupt.
 * @thread_fn: thread fn. corresponding to the handler
 * @data: Private data of the requester.
 * Return the callback structure that was registered or an
 * appropriate error on failure.
 */
static struct mic_intr_cb *
mic_register_intr_callback(struct mic_device *xdev,
			   u8 idx, irq_handler_t handler,
			   irq_handler_t thread_fn, void *data)
{
	struct mic_intr_cb *intr_cb;
	unsigned long flags;
	int rc;

	intr_cb = kmalloc(sizeof(*intr_cb), GFP_KERNEL);
	if (!intr_cb)
		return ERR_PTR(-ENOMEM);

	intr_cb->handler = handler;
	intr_cb->thread_fn = thread_fn;
	intr_cb->data = data;
	intr_cb->cb_id = ida_simple_get(&xdev->irq_info.cb_ida,
		1, 0, GFP_KERNEL);
	if (intr_cb->cb_id < 0) {
		rc = intr_cb->cb_id;
		goto ida_fail;
	}

	spin_lock(&xdev->irq_info.mic_thread_lock);
	spin_lock_irqsave(&xdev->irq_info.mic_intr_lock, flags);
	list_add_tail(&intr_cb->list, &xdev->irq_info.cb_list[idx]);
	spin_unlock_irqrestore(&xdev->irq_info.mic_intr_lock, flags);
	spin_unlock(&xdev->irq_info.mic_thread_lock);

	return intr_cb;
ida_fail:
	kfree(intr_cb);
	return ERR_PTR(rc);
}

/**
 * mic_unregister_intr_callback - Unregister the callback handler
 * identified by its callback id.
 *
 * @xdev: pointer to the mic_device instance
 * @cb_id: The callback structure id to be unregistered.
 * Return the source id that was unregistered or MIC_NUM_OFFSETS if no
 * such callback handler was found.
 */
static u8 mic_unregister_intr_callback(struct mic_device *xdev, u32 cb_id)
{
	struct list_head *pos, *tmp;
	struct mic_intr_cb *intr_cb;
	unsigned long flags;
	int i;

	spin_lock(&xdev->irq_info.mic_thread_lock);
	spin_lock_irqsave(&xdev->irq_info.mic_intr_lock, flags);
	for (i = 0;  i < MIC_NUM_OFFSETS; i++) {
		list_for_each_safe(pos, tmp, &xdev->irq_info.cb_list[i]) {
			intr_cb = list_entry(pos, struct mic_intr_cb, list);
			if (intr_cb->cb_id == cb_id) {
				list_del(pos);
				ida_simple_remove(&xdev->irq_info.cb_ida,
						  intr_cb->cb_id);
				kfree(intr_cb);
				spin_unlock_irqrestore(
					&xdev->irq_info.mic_intr_lock, flags);
				spin_unlock(&xdev->irq_info.mic_thread_lock);
				return i;
			}
		}
	}
	spin_unlock_irqrestore(&xdev->irq_info.mic_intr_lock, flags);
	spin_unlock(&xdev->irq_info.mic_thread_lock);
	return MIC_NUM_OFFSETS;
}

/**
 * mic_setup_callbacks - Initialize data structures needed
 * to handle callbacks.
 *
 * @xdev: pointer to mic_device instance
 */
static int mic_setup_callbacks(struct mic_device *xdev)
{
	int i;

	xdev->irq_info.cb_list = kmalloc_array(MIC_NUM_OFFSETS,
					       sizeof(*xdev->irq_info.cb_list),
					       GFP_KERNEL);
	if (!xdev->irq_info.cb_list)
		return -ENOMEM;

	for (i = 0; i < MIC_NUM_OFFSETS; i++)
		INIT_LIST_HEAD(&xdev->irq_info.cb_list[i]);
	ida_init(&xdev->irq_info.cb_ida);
	spin_lock_init(&xdev->irq_info.mic_intr_lock);
	spin_lock_init(&xdev->irq_info.mic_thread_lock);
	return 0;
}

/**
 * mic_release_callbacks - Uninitialize data structures needed
 * to handle callbacks.
 *
 * @xdev: pointer to mic_device instance
 */
static void mic_release_callbacks(struct mic_device *xdev)
{
	unsigned long flags;
	struct list_head *pos, *tmp;
	struct mic_intr_cb *intr_cb;
	int i;

	spin_lock(&xdev->irq_info.mic_thread_lock);
	spin_lock_irqsave(&xdev->irq_info.mic_intr_lock, flags);
	for (i = 0; i < MIC_NUM_OFFSETS; i++) {
		list_for_each_safe(pos, tmp, &xdev->irq_info.cb_list[i]) {
			intr_cb = list_entry(pos, struct mic_intr_cb, list);
			list_del(pos);
			ida_simple_remove(&xdev->irq_info.cb_ida,
					  intr_cb->cb_id);
			kfree(intr_cb);
		}
	}
	spin_unlock_irqrestore(&xdev->irq_info.mic_intr_lock, flags);
	spin_unlock(&xdev->irq_info.mic_thread_lock);
	ida_destroy(&xdev->irq_info.cb_ida);
	kfree(xdev->irq_info.cb_list);
	xdev->irq_info.cb_list = NULL;
}

/**
 * mic_setup_msi - Initializes MSI interrupts.
 *
 * @xdev: pointer to mic_device instance
 * @pdev: PCI device structure
 *
 * RETURNS: An appropriate -ERRNO error value on error, or zero for success.
 */
static int mic_setup_msi(struct mic_device *xdev, struct pci_dev *pdev)
{
	int rc;

	rc = pci_enable_msi(pdev);
	if (rc) {
		log_mic_err(xdev->id, "error enabling MSI (rc %d)", rc);
		return rc;
	}

	rc = mic_setup_callbacks(xdev);
	if (rc) {
		log_mic_err(xdev->id, "error setting up callbacks (rc %d)", rc);
		goto err_msi;
	}

	rc = request_threaded_irq(pdev->irq, mic_interrupt, mic_thread_fn,
				  0, "mic-msi", xdev);
	if (rc) {
		log_mic_err(xdev->id, "request_threaded_irq failed (rc %d)", rc);
		goto err_irq_req_fail;
	}

	log_mic_dbg(xdev->id, "irq setup completed");
	return 0;

err_irq_req_fail:
	mic_release_callbacks(xdev);
err_msi:
	pci_disable_msi(pdev);
	return rc;
}

/**
 * mic_setup_intx - Initializes legacy interrupts.
 *
 * @xdev: pointer to mic_device instance
 * @pdev: PCI device structure
 *
 * RETURNS: An appropriate -ERRNO error value on error, or zero for success.
 */
static int mic_setup_intx(struct mic_device *xdev, struct pci_dev *pdev)
{
	int rc;

	/* Enable intx */
	pci_intx(pdev, 1);

	rc = mic_setup_callbacks(xdev);
	if (rc) {
		log_mic_err(xdev->id, "error setting up callbacks (rc %d)", rc);
		goto err_nomem;
	}

	rc = request_threaded_irq(pdev->irq, mic_interrupt, mic_thread_fn,
				  IRQF_SHARED, "mic-intx", xdev);
	if (rc) {
		log_mic_err(xdev->id, "request_threaded_irq failed (rc %d)", rc);
		goto err_irq_req_fail;
	}

	log_mic_dbg(xdev->id, "irq setup completed");
	return 0;
err_irq_req_fail:
	mic_release_callbacks(xdev);
err_nomem:
	return rc;
}

/**
 * mic_next_db - Retrieve the next doorbell interrupt source id.
 * The id is picked sequentially from the available pool of
 * doorlbell ids.
 *
 * @xdev: pointer to the mic_device instance.
 *
 * Returns the next doorbell interrupt source.
 */
int mic_next_db(struct mic_device *xdev)
{
	int next_db;

	mutex_lock(&xdev->mic_mutex);
	next_db = xdev->irq_info.next_avail_src % xdev->intr_info->intr_len;
	xdev->irq_info.next_avail_src++;
	mutex_unlock(&xdev->mic_mutex);

	log_mic_dbg(xdev->id, "db %d", next_db);

	return next_db;
}

#define COOKIE_ID_SHIFT 16
#define COOKIE_TAG 0xC00E
#define GET_CB_ID(cookie) ((u32) ((cookie) >> COOKIE_ID_SHIFT))
#define MK_COOKIE(x) (COOKIE_TAG | (unsigned long)(x) << COOKIE_ID_SHIFT)

/**
 * mic_request_threaded_irq - request an irq. mic_mutex needs
 * to be held before calling this function.
 *
 * @xdev: pointer to mic_device instance
 * @handler: The callback function that handles the interrupt.
 * The function needs to call ack_interrupts
 * (mic_ack_interrupt(xdev)) when handling the interrupts.
 * @thread_fn: thread fn required by request_threaded_irq.
 * @name: The ASCII name of the callee requesting the irq.
 * @data: private data that is returned back when calling the
 * function handler.
 * @intr_src: The source id of the requester. Its the doorbell id
 * for Doorbell interrupts and DMA channel id for DMA interrupts.
 *
 * returns: The cookie that is transparent to the caller. Passed
 * back when calling mic_free_irq. An appropriate error code
 * is returned on failure. Caller needs to use IS_ERR(return_val)
 * to check for failure and PTR_ERR(return_val) to obtained the
 * error code.
 *
 */
struct mic_irq *
mic_request_threaded_irq(struct mic_device *xdev,
			 irq_handler_t handler, irq_handler_t thread_fn,
			 const char *name, void *data, int intr_src)
{
	u16 offset;
	int rc = 0;
	unsigned long cookie = 0;
	struct mic_intr_cb *intr_cb;

	offset = mic_map_src_to_offset(xdev, intr_src);
	if (offset >= MIC_NUM_OFFSETS) {
		log_mic_err(xdev->id,
			"error mapping index %d to a valid source id",
			intr_src);
		rc = -EINVAL;
		goto err;
	}

	intr_cb = mic_register_intr_callback(xdev, offset, handler,
					     thread_fn, data);
	if (IS_ERR(intr_cb)) {
		log_mic_err(xdev->id, "no available callback entries for use");
		rc = PTR_ERR(intr_cb);
		goto err;
	}

	cookie = MK_COOKIE(intr_cb->cb_id);
	log_mic_dbg(xdev->id, "callback %d registered for src %d",
		intr_cb->cb_id, intr_src);
	return (struct mic_irq *)cookie;
err:
	return ERR_PTR(rc);
}

/**
 * mic_free_irq - free irq. mic_mutex
 *  needs to be held before calling this function.
 *
 * @xdev: pointer to mic_device instance
 * @cookie: cookie obtained during a successful call to mic_request_threaded_irq
 * @data: private data specified by the calling function during the
 * mic_request_threaded_irq
 *
 * returns: none.
 */
void mic_free_irq(struct mic_device *xdev,
		  struct mic_irq *cookie, void *data)
{
	u32 cb_id = GET_CB_ID((unsigned long) cookie);
	u8 src_id = mic_unregister_intr_callback(xdev, cb_id);
	if (src_id >= MIC_NUM_OFFSETS) {
		log_mic_err(xdev->id, "error unregistering callback (cb_id %u)",
			cb_id);
		return;
	}
	log_mic_dbg(xdev->id, "callback %d unregistered for src %d",
		cb_id, src_id);
}

/**
 * mic_setup_interrupts - Initializes interrupts.
 *
 * @xdev: pointer to mic_device instance
 * @pdev: PCI device structure
 *
 * RETURNS: An appropriate -ERRNO error value on error, or zero for success.
 */
int mic_setup_interrupts(struct mic_device *xdev, struct pci_dev *pdev)
{
	int rc;

	/*
	 * Disable MSI for now as it is resulting in hangs while running
	 * SCIF unit tests.
	 */
	rc = mic_setup_msi(xdev, pdev);
	if (!rc)
		goto done;

	rc = mic_setup_intx(xdev, pdev);
	if (rc) {
		log_mic_err(xdev->id, "no usable interrupts (rc %d)", rc);
		return rc;
	}
done:
	mic_enable_interrupts(xdev);
	return 0;
}

/**
 * mic_free_interrupts - Frees interrupts setup by mic_setup_interrupts
 *
 * @xdev: pointer to mic_device instance
 * @pdev: PCI device structure
 *
 * returns none.
 */
void mic_free_interrupts(struct mic_device *xdev, struct pci_dev *pdev)
{
	mic_disable_interrupts(xdev);
	if (pci_dev_msi_enabled(pdev)) {
		free_irq(pdev->irq, xdev);
		pci_disable_msi(pdev);
	} else {
		free_irq(pdev->irq, xdev);
	}
	mic_release_callbacks(xdev);
}
