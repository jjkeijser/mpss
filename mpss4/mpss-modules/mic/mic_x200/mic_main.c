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
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/poll.h>
#include <linux/version.h>
#include <linux/suspend.h>
#include <linux/iommu.h>
#include <linux/dma_remapping.h>

#ifdef MIC_IN_KERNEL_BUILD
#include <linux/mic_common.h>
#else
#include "../common/mic_common.h"
#endif
#include "../common/mic_dev.h"
#include "mic_device.h"
#include "mic_hw.h"
#include "mic_alut.h"


static const char mic_driver_name[] = "mic_x200";

static const struct pci_device_id mic_pci_tbl[] = {
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, INTEL_PCI_DEVICE_2260)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, INTEL_PCI_DEVICE_2262)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, INTEL_PCI_DEVICE_2263)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, INTEL_PCI_DEVICE_2265)},

	/* required last entry */
	{ 0, }
};

MODULE_DEVICE_TABLE(pci, mic_pci_tbl);

/* ID allocator for MIC devices */
static struct ida g_mic_ida;

/* Initialize the device page */
static int mic_dp_init(struct mic_device *xdev)
{
	xdev->dp = dma_alloc_coherent(&xdev->pdev->dev, MIC_DP_SIZE,
				      &xdev->dp_dma_addr, GFP_KERNEL);

	if (!xdev->dp) {
		log_mic_err(xdev->id, "err %d", -ENOMEM);
		return -ENOMEM;
	}

	mic_write_spad(xdev, MIC_SPAD_DPLO, xdev->dp_dma_addr);
	mic_write_spad(xdev, MIC_SPAD_DPHI, xdev->dp_dma_addr >> 32);
	return 0;
}

/* Uninitialize the device page */
static void mic_dp_uninit(struct mic_device *xdev)
{
	dma_free_coherent(&xdev->pdev->dev, MIC_DP_SIZE, xdev->dp,
			  xdev->dp_dma_addr);
}

static int __init mic_rdp_init(struct mic_device *xdev)
{
	struct mic_bootparam __iomem *bootparam;
	u64 lo, hi, dp_dma_addr;
	u32 magic;

	lo = mic_read_spad(xdev, MIC_SPAD_DPLO);
	hi = mic_read_spad(xdev, MIC_SPAD_DPHI);

	dp_dma_addr = lo | (hi << 32);
	xdev->rdp = xdev->aper.va + dp_dma_addr;
	bootparam = xdev->rdp;
	magic = ioread32(&bootparam->magic);
	if (MIC_MAGIC != magic) {
		log_mic_err(xdev->id,
			"bootparam magic mismatch 0x%x", magic);
		return -EIO;
	}
	return 0;
}

/**
 * mic_device_init - Allocates and initializes the MIC device structure
 *
 * @xdev: pointer to mic_device instance
 * @pdev: The pci device structure
 *
 * returns none.
 */
static void
mic_device_init(struct mic_device *xdev, struct pci_dev *pdev)
{
	xdev->pdev = pdev;
	xdev->irq_info.next_avail_src = 0;
}

/**
 * mic_request_dma_chan - Request DMA channel
 * @xdev: pointer to mic_device instance
 *
 * returns true if a DMA channel was acquired
 */
static int mic_request_dma_chan(struct mic_device *xdev)
{
	dma_cap_mask_t mask;

	dma_cap_zero(mask);
	dma_cap_set(DMA_MEMCPY, mask);
	xdev->dma_ch = dma_request_channel(mask, mic_dma_filter, xdev->pdev);
	if (!xdev->dma_ch)
		return -ENODEV;

	log_mic_info(xdev->id, "DMA channel %s", dma_chan_name(xdev->dma_ch));
	return 0;
}

/**
 * mic_free_dma_chan - release DMA channel
 * @xdev: pointer to mic_device instance
 *
 * returns none
 */
static void mic_free_dma_chan(struct mic_device *xdev)
{
	if (xdev->dma_ch) {
		dma_release_channel(xdev->dma_ch);
		xdev->dma_ch = NULL;
	}
}

/**
 * mic_probe - Device Initialization Routine
 *
 * @pdev: PCI device structure
 * @ent: entry in mic_pci_tbl
 *
 * returns 0 on success, < 0 on failure.
 */
static int mic_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	int rc;
	struct mic_device *xdev;
	u64 mask;

#if defined(IOMMU_SUPPORTS_KNL)
	/*
	 * Remove for upstream: pci quirks.c enables dma aliases only for PCI
	 * IDs 2260 and 2264. Force aliases for all PCI ID used by KNL.
	 */
	if (!pdev->DMA_ALIAS_MEMBER)
		pdev->DMA_ALIAS_MEMBER = kcalloc(BITS_TO_LONGS(U8_MAX),
						 sizeof(long), GFP_KERNEL);
	set_bit(PCI_DEVFN(0x10, 0x0), pdev->DMA_ALIAS_MEMBER);
	set_bit(PCI_DEVFN(0x11, 0x0), pdev->DMA_ALIAS_MEMBER);
	set_bit(PCI_DEVFN(0x12, 0x3), pdev->DMA_ALIAS_MEMBER);
#endif

	xdev = kzalloc(sizeof(*xdev), GFP_KERNEL);
	if (!xdev) {
		rc = -ENOMEM;
		dev_err(&pdev->dev, "xdev kmalloc failed rc %d\n", rc);
		goto xdev_alloc_fail;
	}
	xdev->id = ida_simple_get(&g_mic_ida, 0, MIC_MAX_NUM_DEVS, GFP_KERNEL);
	if (xdev->id < 0) {
		rc = xdev->id;
		log_mic_err(xdev->id, "ida_simple_get failed rc %d", rc);
		goto ida_fail;
	}

	mutex_init(&xdev->mic_mutex);
	mic_device_init(xdev, pdev);

	rc = pci_enable_device(pdev);
	if (rc) {
		log_mic_err(xdev->id, "failed to enable pci device");
		goto ida_remove;
	}

	pci_set_master(pdev);

	rc = pci_request_regions(pdev, mic_driver_name);
	if (rc) {
		log_mic_err(xdev->id, "failed to get pci regions");
		goto disable_device;
	}

	/*
	 * Set 48-bit mask, so driver can be loaded without crash when
	 * running on huge memory system without IOMMU
	 */
	mask = intel_iommu_enabled ? DMA_BIT_MASK(39) : DMA_BIT_MASK(48);
	rc = dma_set_mask_and_coherent(&pdev->dev, mask);
	if (rc) {
		log_mic_err(xdev->id, "cannot set DMA mask: 0x%llx", mask);
		goto release_regions;
	}

	xdev->mmio.pa = pci_resource_start(pdev, MIC_MMIO_BAR);
	xdev->mmio.len = pci_resource_len(pdev, MIC_MMIO_BAR);
	xdev->mmio.va = pci_ioremap_bar(pdev, MIC_MMIO_BAR);
	if (!xdev->mmio.va) {
		log_mic_err(xdev->id, "cannot remap MMIO BAR");
		rc = -EIO;
		goto release_regions;
	}

	log_mic_info(xdev->id, "mmio pa 0x%llx len 0x%llx va %p",
		 xdev->mmio.pa, xdev->mmio.len, xdev->mmio.va);
	rc = mic_hw_init(xdev, pdev);
	if (rc) {
		log_mic_err(xdev->id, "mic_hw_init failed %d", rc);
		goto unmap_mmio;
	}

	if (xdev->link_side) {
		rc = mic_alut_init(xdev);
		if (rc) {
			log_mic_err(xdev->id, "mic_alut_init failed %d", rc);
			goto unmap_mmio;
		}
	}

	xdev->aper.pa = pci_resource_start(pdev, MIC_APER_BAR);
	if (xdev->link_side)
		xdev->aper.len = MIC_APER_SIZE;
	else
		xdev->aper.len = pci_resource_len(pdev, MIC_APER_BAR);

	xdev->aper.va = ioremap_wc(xdev->aper.pa, xdev->aper.len);
	if (!xdev->aper.va) {
		log_mic_err(xdev->id, "cannot remap Aperture BAR");
		rc = -EIO;
		goto alut_uninit;
	}
	log_mic_info(xdev->id, "aper pa 0x%llx len 0x%llx va %p",
		 xdev->aper.pa, xdev->aper.len, xdev->aper.va);

	if (!xdev->link_side)
		mic_set_postcode(xdev, MIC_X200_POST_CODE_OS_READY);

	mic_intr_init(xdev);
	rc = mic_setup_interrupts(xdev, pdev);
	if (rc) {
		log_mic_err(xdev->id, "mic_setup_interrupts failed %d", rc);
		goto unmap_aper;
	}
	pci_set_drvdata(pdev, xdev);
	if (xdev->link_side) {
		rc = mic_dp_init(xdev);
		if (rc) {
			log_mic_err(xdev->id, "mic_dp_init failed rc %d", rc);
			goto free_interrupts;
		}
		mic_bootparam_init(xdev);
	} else {
		rc = mic_rdp_init(xdev);
		if (rc) {
			log_mic_err(xdev->id, "mic_rdp_init failed rc %d", rc);
			goto free_interrupts;
		}
	}

	mic_create_debug_dir(xdev);
	rc = mic_request_dma_chan(xdev);
	if (rc) {
		log_mic_err(xdev->id, "DMA channel request failed");
		goto cleanup_debug_dir;
	}

	if (xdev->link_side) {
		/* MIC Card OS management only required on host side */
		xdev->cosm_dev = cosm_register_device(&xdev->pdev->dev,
							  &cosm_mic_hw_ops);
		if (IS_ERR(xdev->cosm_dev)) {
			rc = PTR_ERR(xdev->cosm_dev);
			log_mic_err(xdev->id,
					"cosm_register_device failed rc %d", rc);
			cosm_unregister_device(xdev->cosm_dev);
			goto dma_remove;
		}
	} else {
		xdev->vpdev = vop_register_device(&xdev->pdev->dev,
						  VOP_DEV_TRNSP, &_mic_dma_ops,
						  &vop_hw_ops, 0, NULL,
						  xdev->dma_ch);
		if (IS_ERR(xdev->vpdev)) {
			rc = PTR_ERR(xdev->vpdev);
			xdev->vpdev = NULL;
			log_mic_err(xdev->id,
				"vop_register_device failed rc %d", rc);
			goto dma_remove;
		}
		rc = mic_scif_setup(xdev);
		if (rc) {
			vop_unregister_device(xdev->vpdev);
			xdev->vpdev = NULL;
			goto dma_remove;
		}
		mic_set_postcode(xdev, MIC_X200_POST_CODE_MIC_DRIVER_READY);
	}
	return 0;
dma_remove:
	mic_free_dma_chan(xdev);
cleanup_debug_dir:
	mic_delete_debug_dir(xdev);
	if (xdev->link_side)
		mic_dp_uninit(xdev);
free_interrupts:
	mic_free_interrupts(xdev, pdev);
unmap_aper:
	iounmap(xdev->aper.va);
alut_uninit:
	if (xdev->link_side)
		mic_alut_uninit(xdev);
unmap_mmio:
	iounmap(xdev->mmio.va);
release_regions:
	pci_release_regions(pdev);
disable_device:
	pci_disable_device(pdev);
ida_remove:
	ida_simple_remove(&g_mic_ida, xdev->id);
ida_fail:
	kfree(xdev);
xdev_alloc_fail:
	dev_err(&pdev->dev, "probe failed rc %d\n", rc);
	return rc;
}

/**
 * mic_remove - Device Removal Routine
 * mic_remove is called by the PCI subsystem to alert the driver
 * that it should release a PCI device.
 *
 * @pdev: PCI device structure
 */
static void mic_remove(struct pci_dev *pdev)
{
	struct mic_device *xdev;

	xdev = pci_get_drvdata(pdev);
	if (!xdev)
		return;
	if (xdev->link_side) {
		cosm_unregister_device(xdev->cosm_dev);
	} else {
		mic_stop(xdev);
	}
	mic_free_dma_chan(xdev);
	mic_delete_debug_dir(xdev);
	if (xdev->link_side) {
		mic_dp_uninit(xdev);
		mic_alut_uninit(xdev);
		mic_clear_rid_lut(xdev);
	}
	mic_free_interrupts(xdev, pdev);
	iounmap(xdev->mmio.va);
	iounmap(xdev->aper.va);
	pci_release_regions(pdev);
	pci_disable_device(pdev);
	ida_simple_remove(&g_mic_ida, xdev->id);
	kfree(xdev);
}

static void mic_shutdown(struct pci_dev *pdev)
{
	struct mic_device *xdev;

	xdev = pci_get_drvdata(pdev);
	if (!xdev)
		return;
	/* Call mic_remove only for the card or virtual side so as to not delay
	 * host shutdown in the event of unresponsive MIC cards */
	if (xdev->link_side)
		return;

	mic_remove(pdev);
}

static struct pci_driver mic_driver = {
	.name = mic_driver_name,
	.id_table = mic_pci_tbl,
	.probe = mic_probe,
	.remove = mic_remove,
	.shutdown = mic_shutdown
};

static int __init mic_x200_init(void)
{
	int ret;

#if !defined(IOMMU_SUPPORTS_KNL)
	if (intel_iommu_enabled) {
		pr_err("mic_x200 - unable to load the driver with IOMMU enabled. Please disable it.\n");
		return -EINVAL;
	}
#endif
	ret = request_module("mic_x200_dma");
	if (ret)
		pr_warn("mic_x200_dma request_module failed %d\n", ret);

	mic_init_debugfs();
	ida_init(&g_mic_ida);

	ret = pci_register_driver(&mic_driver);
	if (ret) {
		pr_err("pci_register_driver failed ret %d\n", ret);
		goto cleanup_debugfs;
	}
	return 0;

cleanup_debugfs:
	ida_destroy(&g_mic_ida);
	mic_exit_debugfs();
	return ret;
}

static void __exit mic_x200_exit(void)
{
	pci_unregister_driver(&mic_driver);
	ida_destroy(&g_mic_ida);
	mic_exit_debugfs();
}

module_init(mic_x200_init);
module_exit(mic_x200_exit);

MODULE_AUTHOR("Intel Corporation");
MODULE_DESCRIPTION("Intel(R) MIC X200 PCIe driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(MIC_VERSION);
