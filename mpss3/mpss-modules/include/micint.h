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

#ifndef MICINT_H
#define MICINT_H

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/fs.h>
#include <linux/major.h>
#include <linux/blkdev.h>
#include <linux/capability.h>
#include <linux/uio.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/pci.h>
#include <linux/version.h>
#include <asm/io.h>
#include <asm/ioctl.h>
#include <asm/uaccess.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/rtnetlink.h>
#include <net/rtnetlink.h>
#include <linux/pm.h>
#include <linux/notifier.h>
#include <linux/suspend.h>
#include <linux/ctype.h>
#include <linux/sysfs.h>

#include "mic_common.h"
#include <mic/micscif.h>

#define MAX_DLDR_MINORS		68
typedef struct mic_lindata {
    dev_t			dd_dev;
	struct cdev		dd_cdev;
	struct device		*dd_hostdev;
	struct device		*dd_scifdev;
	struct class		*dd_class;
	struct pci_driver	dd_pcidriver;
}mic_lindata_t;

typedef struct board_info {
	struct device		*bi_sysfsdev;
#ifdef CONFIG_PCI_MSI
	struct msix_entry 	bi_msix_entries[MIC_NUM_MSIX_ENTRIES];
#endif
#ifdef USE_VCONSOLE
	micvcons_port_t		*bi_port;
#endif
	void			*bi_virtio;  /* for virtio */

	struct list_head	bi_list;
	mic_ctx_t 		bi_ctx;
} bd_info_t;

extern mic_lindata_t mic_lindata;

#ifdef USE_VCONSOLE
int micvcons_create(int num_bds);
void micvcons_destroy(int num_bds);
#endif

int micpm_suspend(struct device *pdev);
int micpm_resume(struct device *pdev);
int micpm_suspend_noirq(struct device *pdev);
int micpm_resume_noirq(struct device *pdev);
int micpm_notifier_block(struct notifier_block *nb, unsigned long event, void *dummy);
irqreturn_t mic_irq_isr(int irq, void *data);

int mic_psmi_init(mic_ctx_t *mic_ctx);
void mic_psmi_uninit(mic_ctx_t *mic_ctx);

void set_sysfs_entries(mic_ctx_t *mic_ctx);
void free_sysfs_entries(mic_ctx_t *mic_ctx);
#endif // MICINT_H
