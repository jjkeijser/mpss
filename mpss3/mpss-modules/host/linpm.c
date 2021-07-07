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

#include <linux/errno.h>
#include <linux/hardirq.h>
#include <linux/types.h>
#include <linux/capability.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/gfp.h>
#include <linux/vmalloc.h>
#include <asm/io.h>
#include <linux/kernel.h>
#include <linux/mm_types.h>
#include <linux/jiffies.h>
#include <linux/timer.h>
#include <linux/irqflags.h>
#include <linux/time.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/semaphore.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <asm/bug.h>
#include <linux/pci.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <asm/atomic.h>
#include <linux/netdevice.h>
#include <linux/debugfs.h>
#include "micint.h"
#include "mic/micveth.h"

/*
 * Retrieves the device context for a particular device
 */
mic_ctx_t *
get_device_context(struct pci_dev *dev) {
	int i = 0;
	mic_ctx_t *mic_ctx = NULL;
	for (i = (mic_data.dd_numdevs -1); i >= 0; i--) {
		mic_ctx = &mic_data.dd_bi[i]->bi_ctx;
		if (mic_ctx!= NULL) {
			//TODO: Is bus number enough to uniquely identify a
			//pci_dev struct in mic_ctx?
			if (mic_ctx->bi_pdev->bus->number ==
					dev->bus->number) {

				//Bus number matches
				break;
			}
		}
	}
	return mic_ctx;
}

/*
 * Notifier callback with event specifying the actual power management
 * event to have happened.Our events of Interest right now are:
 * PM_HIBERNATION_PREPARE and PM_POST_RESTORE
 */
int
micpm_notifier_block(struct notifier_block *nb, unsigned long event, void *dummy)
{
	int i;
	mic_ctx_t *mic_ctx;
	switch (event) {
	case PM_POST_RESTORE:
	case PM_POST_SUSPEND:
	case PM_POST_HIBERNATION:
		pr_debug("%s Calling MIC resume\n", __func__);
		for(i = 0; i < mic_data.dd_numdevs; i++) {
			mic_ctx = get_per_dev_ctx(i);
			if (mic_ctx && mic_ctx->micpm_ctx.resume.wq) {
				queue_work(mic_ctx->micpm_ctx.resume.wq, 
						&mic_ctx->micpm_ctx.resume.work);
			}
		}
		break;
	default:
		pr_debug("%s: Unrecognized event %lu\n", __func__, event);
		break;
	}
return 0;
}

/*
 * Called by the OS when going into suspend.
 * Puts our device to D3Cold.
 */
int
micpm_suspend(struct device *pdev)
{
	struct pci_dev *pci_dev = to_pci_dev(pdev);
	mic_ctx_t *mic_ctx = get_device_context(pci_dev);

	if (!pci_dev) {
		pr_debug("Not initialized, aborting suspend.\n");
		return -ENODEV;
	}

	pr_debug("pm_stop_device called for dev: %d:%d:%d\n", pci_dev->bus->number, 
		    PCI_SLOT(pci_dev->devfn), PCI_FUNC(pci_dev->devfn));
	pm_stop_device(mic_ctx);
	pci_save_state(pci_dev);
	pci_disable_device(pci_dev);
	if (pci_set_power_state(pci_dev, PCI_D3cold))
		pr_debug("Not able to set to D3Cold state\n");
	pr_debug("Returning from mic_suspend\n");
	return 0;
}

/*
 * Called by the OS when coming out of suspend.
 * Puts our device to D0 and starts driver components.
 */
int
micpm_resume(struct device *pdev)
{
	struct pci_dev *pci_dev = to_pci_dev(pdev);
	if (!pci_dev) {
		pr_debug("Device not initialized. aborting resume");
		return -ENODEV;
	}

	pci_set_power_state(pci_dev, PCI_D0);
	if (pci_enable_device(pci_dev)) {
		pr_debug("Failed to wake-up device.\n");
		return -EIO;
	}
	pci_restore_state(pci_dev);
	pci_set_master(pci_dev);
	pr_debug("pm_start_device called for dev: %d:%d:%d\n", pci_dev->bus->number, 
			PCI_SLOT(pci_dev->devfn), PCI_FUNC(pci_dev->devfn));
	return 0;
}

int micpm_suspend_noirq(struct device *pdev) {
	
	struct pci_dev *pci_dev = to_pci_dev(pdev);
	mic_ctx_t *mic_ctx;
	bd_info_t *bd_info;

	if (!pci_dev) {
		pr_debug("Device not initialized. aborting suspend");
		return -ENODEV;
	}
	
	mic_ctx = get_device_context(pci_dev);
	if(mic_ctx) {
		bd_info = mic_ctx->bd_info;
		/* MSI interrupts do not work on resume.
		 * See http://www.digipedia.pl/usenet/thread/18815/2513/
		 * for a discussion on this issue.
		 */
		if (mic_ctx->msie) {
			free_irq(bd_info->bi_msix_entries[0].vector, &bd_info->bi_ctx);
		}
	}
	return 0;
}

int micpm_resume_noirq(struct device *pdev) {
	
	struct pci_dev *pci_dev = to_pci_dev(pdev);
	mic_ctx_t *mic_ctx;
	bd_info_t *bd_info;
	int err;

	if (!pci_dev) {
		pr_debug("Device not initialized. aborting resume");
		return -ENODEV;
	}
	mic_ctx = get_device_context(pci_dev);
	if(mic_ctx) {
		bd_info = mic_ctx->bd_info;

		/* MSI interrupts do not work on resume.
		 * See http://www.digipedia.pl/usenet/thread/18815/2513/
		 * for a discussion on this issue.
		 */
		if (mic_ctx->msie) {
			err = request_irq(bd_info->bi_msix_entries[0].vector,
							  mic_irq_isr, 0, "mic", mic_ctx);
			if (err) {
				pr_debug("%s: %d Error inititalizing MSI interrupts\n", 
						__func__, __LINE__);
				return 0;
			}
		}

	}
	return 0;
}

