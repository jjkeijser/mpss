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

#include <linux/string.h>

#include "mic/micscif_kmem_cache.h"
#include "micint.h"
#include "mic_common.h"
#include "mic/io_interface.h"
#include "mic/mic_pm.h"
#include "mic/micveth.h"

MODULE_LICENSE("GPL");
MODULE_INFO(build_number, BUILD_NUMBER);
MODULE_INFO(build_bywhom, BUILD_BYWHOM);
MODULE_INFO(build_ondate, BUILD_ONDATE);
MODULE_INFO(build_scmver, BUILD_SCMVER);

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,34))
#include <linux/pm_qos_params.h>
#endif

struct kmem_cache *unaligned_cache;
mic_lindata_t mic_lindata;

module_param_named(ulimit, mic_ulimit_check, bool, 0600);
MODULE_PARM_DESC(ulimit, "SCIF ulimit check");

module_param_named(reg_cache, mic_reg_cache_enable, bool, 0600);
MODULE_PARM_DESC(reg_cache, "SCIF registration caching");

module_param_named(huge_page, mic_huge_page_enable, bool, 0600);
MODULE_PARM_DESC(huge_page, "SCIF Huge Page Support");

extern bool mic_p2p_enable;
module_param_named(p2p, mic_p2p_enable, bool, 0600);
MODULE_PARM_DESC(p2p, "SCIF peer-to-peer");

extern bool mic_p2p_proxy_enable;
module_param_named(p2p_proxy, mic_p2p_proxy_enable, bool, 0600);
MODULE_PARM_DESC(p2p_proxy, "SCIF peer-to-peer proxy DMA support");

extern bool mic_watchdog_enable;
module_param_named(watchdog, mic_watchdog_enable, bool, 0600);
MODULE_PARM_DESC(watchdog, "SCIF Watchdog");

extern bool mic_watchdog_auto_reboot;
module_param_named(watchdog_auto_reboot, mic_watchdog_auto_reboot, bool, 0600);
MODULE_PARM_DESC(watchdog_auto_reboot, "SCIF Watchdog auto reboot");

bool mic_msi_enable = 1;
module_param_named(msi, mic_msi_enable, bool, 0600);
MODULE_PARM_DESC(mic_msi_enable, "To enable MSIx in the driver.");

int mic_pm_qos_cpu_dma_lat = -1;
module_param_named(pm_qos_cpu_dma_lat, mic_pm_qos_cpu_dma_lat, int, 0600);
MODULE_PARM_DESC(mic_pm_qos_cpu_dma_lat, "PM QoS CPU DMA latency in usecs.");

extern int ramoops_count;
module_param_named(ramoops_count, ramoops_count, int, 0600);
MODULE_PARM_DESC(ramoops_count, "Maximum frame count for the ramoops driver.");

extern bool mic_crash_dump_enabled;
module_param_named(crash_dump, mic_crash_dump_enabled, bool, 0600);
MODULE_PARM_DESC(mic_crash_dump_enabled, "MIC Crash Dump enabled.");

#define GET_FILE_SIZE_FROM_INODE(fp) i_size_read((fp)->f_path.dentry->d_inode)

int usagemode_param = 0;

static int
mic_open(struct inode *inode, struct file *filp)
{
	dev_t dev = inode->i_rdev;

	switch (MINOR(dev)) {
	case 0:
		return 0;
	case 1:
		return scif_fdopen(filp);
	case 2:
		return mic_psmi_open(filp);
	}

	return -EINVAL;
}

static int
mic_release(struct inode *inode, struct file *filp)
{
	dev_t dev = inode->i_rdev;
	int rc = 0;

	switch (MINOR(dev)) {
	case 0:
		if (filp->private_data == filp) {
			// Fasync is set
			rc = fasync_helper(-1, filp, 0, &mic_data.dd_fasync);
			mic_data.dd_fasync = NULL;
		}
		return rc;
	case 1:
		return scif_fdclose(filp);
	case 2:
		// psmi access to device
		return 0;
	}

	return -EINVAL;
}

extern ssize_t mic_psmi_read(struct file * filp, char __user *buf,
			size_t count, loff_t *pos);
static ssize_t
mic_read(struct file * filp, char __user *buf,
		size_t count, loff_t *pos)
{
	dev_t dev = filp->f_path.dentry->d_inode->i_rdev;
	if (MINOR(dev) == 2)
		return mic_psmi_read(filp, buf, count, pos);

	return -EINVAL;
}

static long
mic_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	dev_t dev;
	int status = 0;

	dev = filp->f_path.dentry->d_inode->i_rdev;
	if (MINOR(dev) == 1)
		return scif_process_ioctl(filp, cmd, arg);

	if (MINOR(dev) == 2)
		return -EINVAL;

	status = adapter_do_ioctl(cmd, arg);
	return status;
}

static int
mic_fasync(int fd, struct file *filp, int on)
{
	int rc;

	if ((rc = fasync_helper(fd, filp, on, &mic_data.dd_fasync)) < 0) {
		return rc;
	}

	if (on) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 18, 0))
		rc = __f_setown(filp, task_pid(current), PIDTYPE_PID, 0);
#else
		__f_setown(filp, task_pid(current), PIDTYPE_PID, 0);
#endif
		filp->private_data = filp;
	} else {
		filp->private_data = NULL;
	}

	return rc;
}

int
mic_mmap(struct file *f, struct vm_area_struct *vma)
{
	dev_t dev = f->f_path.dentry->d_inode->i_rdev;
	if (MINOR(dev) == 1)
		return micscif_mmap(f, vma);

	return -EINVAL;
}

unsigned int
mic_poll(struct file *f, poll_table *wait)
{
	dev_t dev = f->f_path.dentry->d_inode->i_rdev;
	if (MINOR(dev) == 1)
		return micscif_poll(f, wait);

	return -EINVAL;
}

int
mic_flush(struct file *f, fl_owner_t id)
{
	dev_t dev = f->f_path.dentry->d_inode->i_rdev;
	if (MINOR(dev) == 1)
		return micscif_flush(f, id);

	return -EINVAL;
}

irqreturn_t
mic_irq_isr(int irq, void *data)
{
	if (((mic_ctx_t *)data)->msie)
		adapter_imsr((mic_ctx_t *)data);
	else if (adapter_isr((mic_ctx_t *)data) < 0 ){
		return IRQ_NONE;
	}

	return IRQ_HANDLED;
}

extern struct attribute_group bd_attr_group;
extern struct attribute_group host_attr_group;
extern struct attribute_group scif_attr_group;
extern struct attribute_group psmi_attr_group;
extern struct bin_attribute mic_psmi_ptes_attr;

static int
mic_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	int brdnum = mic_data.dd_numdevs;
	int err = 0;
	bd_info_t *bd_info;
	mic_ctx_t *mic_ctx;
#ifdef CONFIG_PCI_MSI
	int i=0;
#endif
	if ((bd_info = (bd_info_t *)kzalloc(sizeof(bd_info_t), GFP_KERNEL)) == NULL) {
		printk("MIC: probe failed allocating memory for bd_info\n");
		return -ENOSPC;
	}

	mic_ctx = &bd_info->bi_ctx;
	mic_ctx->bd_info = bd_info;
	mic_ctx->bi_id = brdnum;
	mic_ctx->bi_pdev = pdev;
	mic_ctx->msie = 0;
	mic_data.dd_bi[brdnum] = bd_info;

	if ((err = pci_enable_device(pdev))) {
		printk("pci_enable failed board #%d\n", brdnum);
		goto probe_freebd;
	}

	pci_set_master(pdev);
	err = pci_reenable_device(pdev);
	err = pci_set_dma_mask(pdev, DMA_BIT_MASK(64));
	if (err) {
		printk("mic %d: ERROR DMA not available\n", brdnum);
		goto probe_freebd;
	}
	err = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(64));
	if (err) {
		printk("mic %d: ERROR pci_set_consistent_dma_mask(64) %d\n", brdnum, err);
		err = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(32));
		if (err) {
			printk("mic %d: ERROR pci_set_consistent_dma_mask(32) %d\n", brdnum, err);
			goto probe_freebd;
		}
	}

	// Allocate bar 4 for MMIO and GTT
	bd_info->bi_ctx.mmio.pa = pci_resource_start(pdev, DLDR_MMIO_BAR);
	bd_info->bi_ctx.mmio.len = pci_resource_len(pdev, DLDR_MMIO_BAR);
	if (request_mem_region(bd_info->bi_ctx.mmio.pa,
	    bd_info->bi_ctx.mmio.len, "mic") == NULL) {
		printk("mic %d: failed to reserve mmio space\n", brdnum);
		goto probe_freebd;
	}

	// Allocate bar 0 for access Aperture
	bd_info->bi_ctx.aper.pa = pci_resource_start(pdev, DLDR_APT_BAR);
	bd_info->bi_ctx.aper.len = pci_resource_len(pdev, DLDR_APT_BAR);
	if (request_mem_region(bd_info->bi_ctx.aper.pa,
	    bd_info->bi_ctx.aper.len, "mic") == NULL) {
		printk("mic %d: failed to reserve aperture space\n", brdnum);
		goto probe_relmmio;
	}

#ifdef CONFIG_PCI_MSI
	if (mic_msi_enable){
		for (i = 0; i < MIC_NUM_MSIX_ENTRIES; i ++)
			bd_info->bi_msix_entries[i].entry = i;
		err = pci_enable_msix(mic_ctx->bi_pdev, bd_info->bi_msix_entries,
				      MIC_NUM_MSIX_ENTRIES);
		if (err == 0 ) {
			// Only support 1 MSIx for now
			err = request_irq(bd_info->bi_msix_entries[0].vector,
					  mic_irq_isr, 0, "mic", mic_ctx);
			if (err != 0) {
				printk("MIC: Error in request_irq %d\n", err);
				goto probe_relaper;
			}
			mic_ctx->msie = 1;
		}
	}
#endif

	// TODO: this needs to be hardened and actually return errors
	if ((err = adapter_init_device(mic_ctx)) != 0) {
		printk("MIC: Adapter init device failed %d\n", err);
		goto probe_relaper;
	}

	// Adding sysfs entries
	set_sysfs_entries(mic_ctx);

	bd_info->bi_sysfsdev = device_create(mic_lindata.dd_class, &pdev->dev,
			mic_lindata.dd_dev + 2 + mic_ctx->bd_info->bi_ctx.bi_id,
			NULL, "mic%d", mic_ctx->bd_info->bi_ctx.bi_id);
	err = sysfs_create_group(&mic_ctx->bd_info->bi_sysfsdev->kobj, &bd_attr_group);
	mic_ctx->sysfs_state = sysfs_get_dirent(mic_ctx->bd_info->bi_sysfsdev->kobj.sd,
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,35) && LINUX_VERSION_CODE < KERNEL_VERSION(3,14,0))
				NULL,
#endif
				"state");

	dev_set_drvdata(mic_ctx->bd_info->bi_sysfsdev, mic_ctx);

	if (!mic_ctx->msie)
		if ((err = request_irq(mic_ctx->bi_pdev->irq, mic_irq_isr,
				       IRQF_SHARED, "mic", mic_ctx)) != 0) {
			printk("MIC: Error in request_irq %d\n", err);
			goto probe_unmapaper;
		}

	adapter_probe(&bd_info->bi_ctx);

	if (mic_ctx->bi_psmi.enabled) {
		err = sysfs_create_group(&mic_ctx->bd_info->bi_sysfsdev->kobj,
						&psmi_attr_group);
		err = device_create_bin_file(mic_ctx->bd_info->bi_sysfsdev,
						&mic_psmi_ptes_attr);
	}

	adapter_wait_reset(mic_ctx);

	// Adding a board instance so increment the total number of MICs in the system.
	list_add_tail(&bd_info->bi_list, &mic_data.dd_bdlist);
	mic_data.dd_numdevs++;
	printk("mic_probe %d:%d:%d as board #%d\n", pdev->bus->number,
	       PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn), brdnum);
	return 0;

probe_unmapaper:
	wait_event(mic_ctx->ioremapwq, mic_ctx->aper.va || mic_ctx->state == MIC_RESETFAIL);
	if (mic_ctx->aper.va)
		iounmap((void *)bd_info->bi_ctx.aper.va);
	iounmap((void *)bd_info->bi_ctx.mmio.va);

probe_relaper:
	release_mem_region(bd_info->bi_ctx.aper.pa, bd_info->bi_ctx.aper.len);

probe_relmmio:
	release_mem_region(bd_info->bi_ctx.mmio.pa, bd_info->bi_ctx.mmio.len);

probe_freebd:
	kfree(bd_info);
	return err;
}

static void
mic_remove(struct pci_dev *pdev)
{
	int32_t brdnum;
	bd_info_t *bd_info;

	if (mic_data.dd_numdevs - 1 < 0)
		return;
	mic_data.dd_numdevs--;
	brdnum = mic_data.dd_numdevs;

	/* Make sure boards are shutdown and not available. */
	bd_info = mic_data.dd_bi[brdnum];

	spin_lock_bh(&bd_info->bi_ctx.sysfs_lock);
	sysfs_put(bd_info->bi_ctx.sysfs_state);
	bd_info->bi_ctx.sysfs_state = NULL;
	spin_unlock_bh(&bd_info->bi_ctx.sysfs_lock);

	if (bd_info->bi_ctx.bi_psmi.enabled) {
		device_remove_bin_file(bd_info->bi_sysfsdev, &mic_psmi_ptes_attr);
		sysfs_remove_group(&bd_info->bi_sysfsdev->kobj, &psmi_attr_group);
	}
	sysfs_remove_group(&bd_info->bi_sysfsdev->kobj, &bd_attr_group);

	free_sysfs_entries(&bd_info->bi_ctx);
	device_destroy(mic_lindata.dd_class,
		       mic_lindata.dd_dev + 2 + bd_info->bi_ctx.bi_id);

	adapter_stop_device(&bd_info->bi_ctx, 1, 0);
	/*
	 * Need to wait for reset since accessing the card while GDDR training
	 * is ongoing by adapter_remove(..) below for example can be fatal.
	 */
	wait_for_reset(&bd_info->bi_ctx);

	mic_disable_interrupts(&bd_info->bi_ctx);

	if (!bd_info->bi_ctx.msie) {
		free_irq(bd_info->bi_ctx.bi_pdev->irq, &bd_info->bi_ctx);
#ifdef CONFIG_PCI_MSI
	} else {
		free_irq(bd_info->bi_msix_entries[0].vector, &bd_info->bi_ctx);
		pci_disable_msix(bd_info->bi_ctx.bi_pdev);
#endif
	}
	adapter_remove(&bd_info->bi_ctx);
	release_mem_region(bd_info->bi_ctx.aper.pa, bd_info->bi_ctx.aper.len);
	release_mem_region(bd_info->bi_ctx.mmio.pa, bd_info->bi_ctx.mmio.len);
	pci_disable_device(bd_info->bi_ctx.bi_pdev);
	kfree(bd_info);
}

static void
mic_shutdown(struct pci_dev *pdev) {
	mic_ctx_t *mic_ctx;
	mic_ctx = get_device_context(pdev);

	if(!mic_ctx)
		return;

	adapter_stop_device(mic_ctx, !RESET_WAIT , !RESET_REATTEMPT);
	return;
}
static const struct file_operations mic_fops = {
	.open   = mic_open,
	.release = mic_release,
	.read	= mic_read,
	.unlocked_ioctl = mic_ioctl,
	.fasync = mic_fasync,
	.mmap	= mic_mmap,
	.poll	= mic_poll,
	.flush	= mic_flush,
	.owner  = THIS_MODULE,
};

static const struct dev_pm_ops pci_dev_pm_ops = {
	.suspend = micpm_suspend,
	.resume = micpm_resume,
	.freeze = micpm_suspend,
	.restore = micpm_resume,
	.suspend_noirq = micpm_suspend_noirq,
	.resume_noirq = micpm_resume_noirq,
	.freeze_noirq = micpm_suspend_noirq,
	.restore_noirq = micpm_resume_noirq,
};

static struct notifier_block mic_pm_notifer = {
	.notifier_call = micpm_notifier_block,
};

static struct pci_device_id mic_pci_tbl[] = {
#ifdef CONFIG_ML1OM
	{ PCI_VENDOR_ID_INTEL,  PCI_DEVICE_ABR_2249, PCI_ANY_ID, PCI_ANY_ID,
	  0, 0, 0 },
	{ PCI_VENDOR_ID_INTEL,  PCI_DEVICE_ABR_224a, PCI_ANY_ID, PCI_ANY_ID,
	  0, 0, 0 },
#endif
#ifdef CONFIG_MK1OM
	{ PCI_VENDOR_ID_INTEL, PCI_DEVICE_KNC_2250, PCI_ANY_ID, PCI_ANY_ID,
	  0, 0, 0 },
	{ PCI_VENDOR_ID_INTEL, PCI_DEVICE_KNC_2251, PCI_ANY_ID, PCI_ANY_ID,
	  0, 0, 0 },
	{ PCI_VENDOR_ID_INTEL, PCI_DEVICE_KNC_2252, PCI_ANY_ID, PCI_ANY_ID,
	  0, 0, 0 },
	{ PCI_VENDOR_ID_INTEL, PCI_DEVICE_KNC_2253, PCI_ANY_ID, PCI_ANY_ID,
	  0, 0, 0 },
	{ PCI_VENDOR_ID_INTEL, PCI_DEVICE_KNC_2254, PCI_ANY_ID, PCI_ANY_ID,
	  0, 0, 0 },
	{ PCI_VENDOR_ID_INTEL, PCI_DEVICE_KNC_2255, PCI_ANY_ID, PCI_ANY_ID,
	  0, 0, 0 },
	{ PCI_VENDOR_ID_INTEL, PCI_DEVICE_KNC_2256, PCI_ANY_ID, PCI_ANY_ID,
	  0, 0, 0 },
	{ PCI_VENDOR_ID_INTEL, PCI_DEVICE_KNC_2257, PCI_ANY_ID, PCI_ANY_ID,
	  0, 0, 0 },
	{ PCI_VENDOR_ID_INTEL, PCI_DEVICE_KNC_2258, PCI_ANY_ID, PCI_ANY_ID,
	  0, 0, 0 },
	{ PCI_VENDOR_ID_INTEL, PCI_DEVICE_KNC_2259, PCI_ANY_ID, PCI_ANY_ID,
	  0, 0, 0 },
	{ PCI_VENDOR_ID_INTEL, PCI_DEVICE_KNC_225a, PCI_ANY_ID, PCI_ANY_ID,
	  0, 0, 0 },
	{ PCI_VENDOR_ID_INTEL, PCI_DEVICE_KNC_225b, PCI_ANY_ID, PCI_ANY_ID,
	  0, 0, 0 },
	{ PCI_VENDOR_ID_INTEL, PCI_DEVICE_KNC_225c, PCI_ANY_ID, PCI_ANY_ID,
	  0, 0, 0 },
	{ PCI_VENDOR_ID_INTEL, PCI_DEVICE_KNC_225d, PCI_ANY_ID, PCI_ANY_ID,
	  0, 0, 0 },
	{ PCI_VENDOR_ID_INTEL, PCI_DEVICE_KNC_225e, PCI_ANY_ID, PCI_ANY_ID,
          0, 0, 0 },

#endif
	{ 0, }
};

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,31)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)
#define MODE_T umode_t
#else
#define MODE_T mode_t
#endif
static char *
mic_devnode(struct device *dev, MODE_T *mode)
{
	return kasprintf(GFP_KERNEL, "mic/%s", dev_name(dev));
}
#undef MODE_T
#endif

static int __init
mic_init(void)
{
	int ret, i;

	adapter_init();

	unaligned_cache = micscif_kmem_cache_create();
	if (!unaligned_cache) {
		ret = -ENOMEM;
		goto init_free_ports;
	}

	mic_lindata.dd_pcidriver.name = "mic";
	mic_lindata.dd_pcidriver.id_table = mic_pci_tbl;
	mic_lindata.dd_pcidriver.probe = mic_probe;
	mic_lindata.dd_pcidriver.remove = mic_remove;
	mic_lindata.dd_pcidriver.driver.pm = &pci_dev_pm_ops;
	mic_lindata.dd_pcidriver.shutdown = mic_shutdown;


	if ((ret = alloc_chrdev_region(&mic_lindata.dd_dev,
				       0, MAX_DLDR_MINORS, "mic") != 0)) {
		printk("Error allocating device nodes: %d\n", ret);
		goto init_free_ports;
	}

	cdev_init(&mic_lindata.dd_cdev, &mic_fops);
	mic_lindata.dd_cdev.owner = THIS_MODULE;
	mic_lindata.dd_cdev.ops = &mic_fops;

	if ((ret = cdev_add(&mic_lindata.dd_cdev,
			    mic_lindata.dd_dev, MAX_DLDR_MINORS) != 0)) {
		kobject_put(&mic_lindata.dd_cdev.kobj);
		goto init_free_region;
	}

	mic_lindata.dd_class = class_create(THIS_MODULE, "mic");
	if (IS_ERR(mic_lindata.dd_class)) {
		printk("MICDLDR: Error createing mic class\n");
		cdev_del(&mic_lindata.dd_cdev);
		ret = PTR_ERR(mic_lindata.dd_class);
		goto init_free_region;
	}

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,31)
	mic_lindata.dd_class->devnode = mic_devnode;
#endif

	mic_lindata.dd_hostdev = device_create(mic_lindata.dd_class, NULL,
					    mic_lindata.dd_dev, NULL, "ctrl");
	mic_lindata.dd_scifdev = device_create(mic_lindata.dd_class, NULL,
					    mic_lindata.dd_dev + 1, NULL, "scif");
	ret = sysfs_create_group(&mic_lindata.dd_hostdev->kobj, &host_attr_group);
	ret = sysfs_create_group(&mic_lindata.dd_scifdev->kobj, &scif_attr_group);

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,31)
	mic_lindata.dd_class->devnode = NULL;
#endif

	if (micveth_init(mic_lindata.dd_hostdev))
		printk(KERN_ERR "%s: micveth_init failed\n", __func__);

	ret = pci_register_driver(&mic_lindata.dd_pcidriver);
	if (ret) {
		micscif_destroy();
		printk("mic: failed to register pci driver %d\n", ret);
		goto clean_unregister;
	}

	if (!mic_data.dd_numdevs) {
		printk("mic: No MIC boards present.  SCIF available in loopback mode\n");
	} else {
		printk("mic: number of devices detected %d \n", mic_data.dd_numdevs);
	}

	for (i = 0; i < mic_data.dd_numdevs; i++) {
		mic_ctx_t *mic_ctx = get_per_dev_ctx(i);
		wait_event(mic_ctx->ioremapwq,
			mic_ctx->aper.va || mic_ctx->state == MIC_RESETFAIL);
		destroy_workqueue(mic_ctx->ioremapworkq);
	}

	micveth_init_legacy(mic_data.dd_numdevs, mic_lindata.dd_hostdev);

	ret = acptboot_init();

#ifdef USE_VCONSOLE
	micvcons_create(mic_data.dd_numdevs);
#endif

	/* Initialize Data structures for PM Disconnect */
	ret = micpm_disconn_init(mic_data.dd_numdevs + 1);
	if (ret)
		printk(KERN_ERR "%s: Failed to initialize PM disconnect"
			" data structures. PM may not work as expected."
			" ret = %d\n", __func__, ret);
	register_pm_notifier(&mic_pm_notifer);
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,34))
	ret = pm_qos_add_requirement(PM_QOS_CPU_DMA_LATENCY, "mic", mic_pm_qos_cpu_dma_lat);
	if (ret) {
		printk(KERN_ERR "%s %d mic_pm_qos_cpu_dma_lat %d ret %d\n", 
			__func__, __LINE__, mic_pm_qos_cpu_dma_lat, ret);
		ret = 0;
		/* Dont fail driver load due to PM QoS API. Fall through */
	}
#endif
	return 0;

clean_unregister:
	device_destroy(mic_lindata.dd_class, mic_lindata.dd_dev + 1);
	device_destroy(mic_lindata.dd_class, mic_lindata.dd_dev);
	class_destroy(mic_lindata.dd_class);
	cdev_del(&mic_lindata.dd_cdev);
	unregister_pm_notifier(&mic_pm_notifer);
init_free_region:
	unregister_chrdev_region(mic_lindata.dd_dev, MAX_DLDR_MINORS);
init_free_ports:
	micpm_uninit();
	return ret;
}

static void __exit
mic_exit(void)
{
	/* Close endpoints related to reverse registration */
	acptboot_exit();

#ifdef USE_VCONSOLE
	micvcons_destroy(mic_data.dd_numdevs);
#endif

	pci_unregister_driver(&mic_lindata.dd_pcidriver);
	micpm_uninit();

	/* Uninit data structures for PM disconnect */
	micpm_disconn_uninit(mic_data.dd_numdevs + 1);

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,34))
	pm_qos_remove_requirement(PM_QOS_CPU_DMA_LATENCY, "mic");
#endif
	micscif_kmem_cache_destroy();
	vmcore_exit();
	micveth_exit();
	micscif_destroy();
	ramoops_exit();

	device_destroy(mic_lindata.dd_class, mic_lindata.dd_dev + 1);
	device_destroy(mic_lindata.dd_class, mic_lindata.dd_dev);
	class_destroy(mic_lindata.dd_class);
	cdev_del(&mic_lindata.dd_cdev);
	unregister_chrdev_region(mic_lindata.dd_dev, MAX_DLDR_MINORS);
	unregister_pm_notifier(&mic_pm_notifer);
	return;
}

void
set_sysfs_entries(mic_ctx_t *mic_ctx)
{
	memset(&mic_ctx->sysfs_info, 0, sizeof(mic_ctx->sysfs_info));
}

void
free_sysfs_entries(mic_ctx_t *mic_ctx)
{
	if (mic_ctx->image != NULL)
		kfree(mic_ctx->image); /* mic_ctx->initramfs points into this buffer */
	if (mic_ctx->sysfs_info.cmdline != NULL)
		kfree(mic_ctx->sysfs_info.cmdline);
	if (mic_ctx->sysfs_info.kernel_cmdline != NULL)
		kfree(mic_ctx->sysfs_info.kernel_cmdline);
}

mic_ctx_t *
get_per_dev_ctx(uint16_t node)
{
	/* TODO: Its important to check the upper bound of the dd_bi array as well.
	 * Cannot be done currently since not all calling functions to get_per_dev_ctx
	 * has the dd_numdevs set correctly. (See mic_ctx_map_single call in adapter_init_device
	 * thats callled even before dd_numdevs is incremented. */
	return &mic_data.dd_bi[node]->bi_ctx;
}

int
get_num_devs(mic_ctx_t *mic_ctx, uint32_t *num_devs)
{
	if (num_devs == NULL)
		return -EINVAL;
	if (copy_to_user(num_devs, &mic_data.dd_numdevs, sizeof(uint32_t)))
		return -EFAULT;
	return 0;
}

int
mic_get_file_size(const char* fn, uint32_t* file_len)
{
	struct file *filp;
	loff_t filp_size;
	uint32_t status = 0;
	mm_segment_t fs = get_fs();

	set_fs(get_ds());

	if (!fn || IS_ERR(filp = filp_open(fn, 0, 0))) {
		status = EINVAL;
		goto cleanup_fs;
	}

	filp_size = GET_FILE_SIZE_FROM_INODE(filp);
	if (filp_size <= 0) {
		status = EINVAL;
		goto cleanup_filp;
	}

	*file_len = filp_size;
cleanup_filp:
	filp_close(filp, current->files);
cleanup_fs:
	set_fs(fs);
	return status;
}

// loads file from hdd into pci physical memory
int
mic_load_file(const char* fn, uint8_t* buffer, uint32_t max_size)
{
	long c;
	int status = 0;
	struct file *filp;
	loff_t filp_size, pos = 0;

	mm_segment_t fs = get_fs();
	set_fs(get_ds());

	if (!fn || IS_ERR(filp = filp_open(fn, 0, 0))) {
		status = EINVAL;
		goto cleanup_fs;
	}

	filp_size = GET_FILE_SIZE_FROM_INODE(filp);
	if (filp_size <= 0) {
		goto cleanup_filp;
	}

	c = vfs_read(filp, buffer, filp_size, &pos);
	if(c != (long)filp_size) {
		status = -1; //FIXME
		goto cleanup_filp;
	}

cleanup_filp:
	filp_close(filp, current->files);
cleanup_fs:
	set_fs(fs);

	return status;
}

module_init(mic_init);
module_exit(mic_exit);
