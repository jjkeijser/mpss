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

#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/pci.h>
#include <linux/version.h>
#include <linux/module.h>

#ifdef MIC_IN_KERNEL_BUILD
#include <linux/mic_common.h>
#include <linux/vop_bus.h>
#include <linux/scif_bus.h>
#else
#include "../common/mic_common.h"
#include "../bus/vop_bus.h"
#include "../bus/scif_bus.h"
#endif
#include "../common/mic_dev.h"
#include "mic_device.h"
#include "mic_hw.h"

#define MIC_S5_TRANSITION_DELAY_MS 2000

static dma_addr_t
_mic_map_single(struct mic_device *xdev, void *va, size_t size)
{
	dma_addr_t dma_addr;

	if (xdev->link_side)
		return mic_map_single(xdev, va, size);

	dma_addr = pci_map_single(xdev->pdev, va, size, PCI_DMA_BIDIRECTIONAL);
	if (pci_dma_mapping_error(xdev->pdev, dma_addr)) {
		dev_err(&xdev->pdev->dev,
			"Cannot map va: %p, size: %zu\n", va, size);
		return 0;
	}
	return dma_addr;
}

static void
_mic_unmap_single(struct mic_device *xdev, dma_addr_t mic_addr,
			size_t size)
{
	if (xdev->link_side)
		mic_unmap_single(xdev, mic_addr, size);
	else
		pci_unmap_single(xdev->pdev, mic_addr, size,
			PCI_DMA_BIDIRECTIONAL);
}

static inline struct mic_device *scdev_to_xdev(struct scif_hw_dev *scdev)
{
	return dev_get_drvdata(scdev->dev.parent);
}

static void *__mic_dma_alloc(struct device *dev, size_t size,
			     dma_addr_t *dma_handle, gfp_t gfp,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
			     struct dma_attrs *attrs)
#else
			     unsigned long attrs)
#endif
{
	struct scif_hw_dev *scdev = dev_get_drvdata(dev);
	struct mic_device *xdev = scdev_to_xdev(scdev);

	dma_addr_t tmp;
	void *va;

	if (xdev->link_side) {
		va = kmalloc(size, gfp);

		if (va) {
			tmp = mic_map_single(xdev, va, size);
			if (dma_mapping_error(dev, tmp)) {
				kfree(va);
				va = NULL;
			} else {
				*dma_handle = tmp;
			}
		}
		return va;
	} else {
		return dma_alloc_coherent(&xdev->pdev->dev, size,
			dma_handle, gfp);
	}
}

static void __mic_dma_free(struct device *dev, size_t size, void *vaddr,
			   dma_addr_t dma_handle,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
			   struct dma_attrs *attrs)
#else
			   unsigned long attrs)
#endif
{
	struct scif_hw_dev *scdev = dev_get_drvdata(dev);
	struct mic_device *xdev = scdev_to_xdev(scdev);

	if (xdev->link_side) {
		mic_unmap_single(xdev, dma_handle, size);
		kfree(vaddr);
	} else {
		dma_free_coherent(&xdev->pdev->dev, size, vaddr,
			dma_handle);
	}
}

static dma_addr_t
__mic_dma_map_page(struct device *dev, struct page *page,
		   unsigned long offset, size_t size,
		   enum dma_data_direction dir,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
		   struct dma_attrs *attrs)
#else
		   unsigned long attrs)
#endif
{
	void *va = phys_to_virt(page_to_phys(page)) + offset;
	struct scif_hw_dev *scdev = dev_get_drvdata(dev);
	struct mic_device *xdev = scdev_to_xdev(scdev);

	return _mic_map_single(xdev, va, size);
}

static void
__mic_dma_unmap_page(struct device *dev, dma_addr_t dma_addr,
		     size_t size, enum dma_data_direction dir,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
		     struct dma_attrs *attrs)
#else
		     unsigned long attrs)
#endif
{
	struct scif_hw_dev *scdev = dev_get_drvdata(dev);
	struct mic_device *xdev = scdev_to_xdev(scdev);

	_mic_unmap_single(xdev, dma_addr, size);
}

static int __mic_dma_map_sg(struct device *dev, struct scatterlist *sg,
			    int nents, enum dma_data_direction dir,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
			    struct dma_attrs *attrs)
#else
			    unsigned long attrs)
#endif
{
	struct scif_hw_dev *scdev = dev_get_drvdata(dev);
	struct mic_device *xdev = scdev_to_xdev(scdev);
	struct scatterlist *s;
	int i, j, ret;
	dma_addr_t da;

	if (xdev->link_side) {
		ret = dma_map_sg(&xdev->pdev->dev, sg, nents, dir);
		if (ret <= 0)
			return 0;

		for_each_sg(sg, s, nents, i) {
			da = mic_map(xdev, sg_dma_address(s) + s->offset, s->length);
			if (!da)
				goto err;
			sg_dma_address(s) = da;
		}
		return nents;
err:
		for_each_sg(sg, s, i, j) {
			mic_unmap(xdev, sg_dma_address(s), s->length);
			sg_dma_address(s) = mic_to_dma_addr(xdev, sg_dma_address(s));
		}
		dma_unmap_sg(&xdev->pdev->dev, sg, nents, dir);
		return 0;
	} else {
		for_each_sg(sg, s, nents, i) {
				sg_dma_address(s) = dma_map_page(dev, sg_page(s),
				s->offset, s->length, dir);
			if (dma_mapping_error(dev, s->dma_address))
				goto err1;
			sg_dma_len(s) = s->length;
		}
		return nents;
err1:
		for_each_sg(sg, s, i, j)
			dma_unmap_page(dev, sg_dma_address(s), sg_dma_len(s), dir);
		return 0;
	}
}

static void __mic_dma_unmap_sg(struct device *dev,
			       struct scatterlist *sg, int nents,
			       enum dma_data_direction dir,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
			       struct dma_attrs *attrs)
#else
			       unsigned long attrs)
#endif
{
	struct scif_hw_dev *scdev = dev_get_drvdata(dev);
	struct mic_device *xdev = scdev_to_xdev(scdev);
	struct scatterlist *s;
	dma_addr_t da;
	int i;

	if (xdev->link_side) {
		for_each_sg(sg, s, nents, i) {
			da = mic_to_dma_addr(xdev, sg_dma_address(s));
			mic_unmap(xdev, sg_dma_address(s), s->length);
			sg_dma_address(s) = da;
		}
		dma_unmap_sg(&xdev->pdev->dev, sg, nents, dir);
	} else {
		for_each_sg(sg, s, nents, i) {
			dma_unmap_page(dev, sg_dma_address(s),
				sg_dma_len(s), dir);
		}
	}
}

static struct dma_map_ops __mic_dma_ops = {
	.alloc = __mic_dma_alloc,
	.free = __mic_dma_free,
	.map_page = __mic_dma_map_page,
	.unmap_page = __mic_dma_unmap_page,
	.map_sg = __mic_dma_map_sg,
	.unmap_sg = __mic_dma_unmap_sg,
};

static struct mic_irq *
___mic_request_irq(struct scif_hw_dev *scdev,
		   irqreturn_t (*func)(int irq, void *data),
		   const char *name, void *data, int intr_src)
{
	struct mic_device *xdev = scdev_to_xdev(scdev);

	return mic_request_threaded_irq(xdev, func, NULL, name, data, intr_src);
}

static void ___mic_free_irq(struct scif_hw_dev *scdev,
			    struct mic_irq *cookie, void *data)
{
	struct mic_device *xdev = scdev_to_xdev(scdev);

	return mic_free_irq(xdev, cookie, data);
}

static void ___mic_ack_interrupt(struct scif_hw_dev *scdev, int num)
{
}

static int ___mic_next_db(struct scif_hw_dev *scdev)
{
	struct mic_device *xdev = scdev_to_xdev(scdev);

	return mic_next_db(xdev);
}

static void ___mic_send_intr(struct scif_hw_dev *scdev, int db)
{
	struct mic_device *xdev = scdev_to_xdev(scdev);

	mic_send_intr(xdev, db);
}

static void ___mic_send_p2p_intr(struct scif_hw_dev *scdev, int db,
			struct mic_mw *mw)
{
	struct mic_device *xdev = scdev_to_xdev(scdev);

	mic_send_p2p_intr(xdev, db, mw);
}

static void __iomem *___mic_ioremap(struct scif_hw_dev *scdev,
				    dma_addr_t pa, size_t len)
{
	struct mic_device *xdev = scdev_to_xdev(scdev);

	return xdev->aper.va + pa;
}

static void ___mic_iounmap(struct scif_hw_dev *scdev, void __iomem *va)
{
	/* nothing to do */
}


static dma_addr_t ___map_peer_resource(struct scif_hw_dev *sdev,
				       dma_addr_t addr, u64 len)
{
	struct mic_device *xdev = scdev_to_xdev(sdev);
	dma_addr_t dma_addr;

	if (xdev->alut)
		return mic_map(xdev, addr, len);

	dma_addr = pci_map_page(xdev->pdev, pfn_to_page(addr >> PAGE_SHIFT), 0,
				len, PCI_DMA_BIDIRECTIONAL);
	if (pci_dma_mapping_error(xdev->pdev, dma_addr)) {
		dev_err(&xdev->pdev->dev,
			"Cannot map addr: 0x%llx, len: %lld\n", addr, len);
		return 0;
	}
	return dma_addr;
}

static void ___unmap_peer_resource(struct scif_hw_dev *sdev, dma_addr_t addr,
				   u64 len)
{
	struct mic_device *xdev = scdev_to_xdev(sdev);

	if (xdev->alut)
		mic_unmap(xdev, addr, len);
	else
		pci_unmap_page(xdev->pdev, addr, len, PCI_DMA_BIDIRECTIONAL);
}

static struct scif_hw_ops scif_hw_ops = {
	.request_irq = ___mic_request_irq,
	.free_irq = ___mic_free_irq,
	.ack_interrupt = ___mic_ack_interrupt,
	.next_db = ___mic_next_db,
	.send_intr = ___mic_send_intr,
	.send_p2p_intr = ___mic_send_p2p_intr,
	.ioremap = ___mic_ioremap,
	.iounmap = ___mic_iounmap,
	.map_peer_resource = ___map_peer_resource,
	.unmap_peer_resource = ___unmap_peer_resource,
};

int mic_scif_setup(struct mic_device *xdev)
{
	int rc = 0, snode, dnode;
	struct mic_bootparam __iomem *bootparam;

	if (xdev->link_side) {
		snode = 0;
		dnode = xdev->cosm_dev->index + 1;
	} else {
		bootparam = xdev->rdp;
		snode = ioread8(&bootparam->node_id);
		dnode = 0;
	}
	xdev->scdev = scif_register_device(&xdev->pdev->dev,
					   MIC_SCIF_DEV,
					   &__mic_dma_ops, &scif_hw_ops,
					   dnode, snode, &xdev->mmio,
					   &xdev->aper, xdev->dp, xdev->rdp,
					   &xdev->dma_ch, 1, false);
	if (IS_ERR(xdev->scdev)) {
		rc = PTR_ERR(xdev->scdev);
		xdev->scdev = NULL;
	}
	return rc;
}

static inline struct mic_device *vpdev_to_xdev(struct vop_device *vpdev)
{
	return dev_get_drvdata(vpdev->dev.parent);
}

static dma_addr_t
_mic_dma_map_page(struct device *dev, struct page *page,
		  unsigned long offset, size_t size,
		  enum dma_data_direction dir,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
		  struct dma_attrs *attrs)
#else
		  unsigned long attrs)
#endif
{
	void *va = phys_to_virt(page_to_phys(page)) + offset;
	struct vop_device *vpdev = dev_get_drvdata(dev);
	struct mic_device *xdev = vpdev_to_xdev(vpdev);

	return _mic_map_single(xdev, va, size);
}

static void
_mic_dma_unmap_page(struct device *dev, dma_addr_t dma_addr,
		    size_t size, enum dma_data_direction dir,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
		    struct dma_attrs *attrs)
#else
		    unsigned long attrs)
#endif
{
	struct vop_device *vpdev = dev_get_drvdata(dev);
	struct mic_device *xdev = vpdev_to_xdev(vpdev);

	_mic_unmap_single(xdev, dma_addr, size);
}

struct dma_map_ops _mic_dma_ops = {
	.map_page = _mic_dma_map_page,
	.unmap_page = _mic_dma_unmap_page,
};

static struct mic_irq *
__mic_request_irq(struct vop_device *vpdev,
		  irqreturn_t (*func)(int irq, void *data),
		  const char *name, void *data, int intr_src)
{
	struct mic_device *xdev = vpdev_to_xdev(vpdev);

	return mic_request_threaded_irq(xdev, func, NULL, name, data, intr_src);
}

static void __mic_free_irq(struct vop_device *vpdev,
			   struct mic_irq *cookie, void *data)
{
	struct mic_device *xdev = vpdev_to_xdev(vpdev);

	return mic_free_irq(xdev, cookie, data);
}

static void __mic_ack_interrupt(struct vop_device *vpdev, int num)
{
}

static int __mic_next_db(struct vop_device *vpdev)
{
	struct mic_device *xdev = vpdev_to_xdev(vpdev);

	return mic_next_db(xdev);
}

static void *__mic_get_dp(struct vop_device *vpdev)
{
	struct mic_device *xdev = vpdev_to_xdev(vpdev);

	return xdev->dp;
}

static void __iomem *__mic_get_remote_dp(struct vop_device *vpdev)
{
	struct mic_device *xdev = vpdev_to_xdev(vpdev);

	return xdev->rdp;
}

static void __mic_send_intr(struct vop_device *vpdev, int db)
{
	struct mic_device *xdev = vpdev_to_xdev(vpdev);

	mic_send_intr(xdev, db);
}

static void __iomem *__mic_ioremap(struct vop_device *vpdev,
				   dma_addr_t pa, size_t len)
{
	struct mic_device *xdev = vpdev_to_xdev(vpdev);

	return xdev->aper.va + pa;
}

static void __mic_iounmap(struct vop_device *vpdev, void __iomem *va)
{
	/* nothing to do */
}

struct vop_hw_ops vop_hw_ops = {
	.request_irq = __mic_request_irq,
	.free_irq = __mic_free_irq,
	.ack_interrupt = __mic_ack_interrupt,
	.next_db = __mic_next_db,
	.get_dp = __mic_get_dp,
	.get_remote_dp = __mic_get_remote_dp,
	.send_intr = __mic_send_intr,
	.ioremap = __mic_ioremap,
	.iounmap = __mic_iounmap,
};

/* Initialize the MIC bootparams */
void mic_bootparam_init(struct mic_device *xdev)
{
	struct mic_bootparam *bootparam = xdev->dp;

	if (!bootparam)
		return;

	memset(bootparam, 0, MIC_DP_SIZE);

	bootparam->magic = cpu_to_le32(MIC_MAGIC);
	bootparam->h2c_config_db = -1;
	bootparam->node_id = xdev->id + 1;
	bootparam->scif_host_dma_addr = 0x0;
	bootparam->scif_card_dma_addr = 0x0;
	bootparam->c2h_scif_db = -1;
	bootparam->h2c_scif_db = -1;
}

static inline struct mic_device *cosmdev_to_xdev(struct cosm_device *cdev)
{
	return dev_get_drvdata(cdev->dev.parent);
}

static struct mic_mw *_mic_aper(struct cosm_device *cdev)
{
	struct mic_device *xdev = cosmdev_to_xdev(cdev);

	return &xdev->aper;
}


/******************************************************************************
 * The cosm_hw_ops operations
 ******************************************************************************/

static int
mic_dev_init(struct cosm_device *cdev)
{
	struct mic_device *xdev = cosmdev_to_xdev(cdev);
	int err;

	/* Increasing module reference when COSM initializes the card. */
	if (!try_module_get(THIS_MODULE)) {
		log_mic_err(xdev->id, "can't get reference for module");
		err = -EBUSY;
		goto exit;
	}

	/* Initialize log buffer. */
	cdev->log_buf_size = MIC_HOST_LOG_BUFFER;

	cdev->log_buf_cpu_addr = kzalloc(cdev->log_buf_size, GFP_KERNEL);
	if (!cdev->log_buf_cpu_addr) {
		log_mic_err(xdev->id, "failed to allocate klog memory");
		err = -ENOMEM;
		goto put_module;
	}

	cdev->log_buf_dma_addr = dma_map_single(&xdev->pdev->dev,
						cdev->log_buf_cpu_addr,
						cdev->log_buf_size,
						DMA_BIDIRECTIONAL);
	if (dma_mapping_error(&xdev->pdev->dev, cdev->log_buf_dma_addr)) {
		log_mic_err(xdev->id, "failed map klog memory");
		err = -ENOMEM;
		goto free_buf;
	}

	log_mic_dbg(xdev->id, "allocated klog memory - address: %llx",
		    cdev->log_buf_dma_addr);

	err = mic_sysfs_init(cdev);
	if (err)
		goto unmap_buf;
	return 0;

unmap_buf:
	dma_unmap_single(&xdev->pdev->dev, cdev->log_buf_dma_addr,
			 cdev->log_buf_size, DMA_BIDIRECTIONAL);
free_buf:
	kfree(cdev->log_buf_cpu_addr);
put_module:
	module_put(THIS_MODULE);
exit:
	return err;
}

static int
mic_dev_update(struct cosm_device *cdev)
{
	struct mic_device *xdev = cosmdev_to_xdev(cdev);
	int rc;

	log_mic_info(xdev->id, "retrieve device info from DMI");
	rc = mic_dmi_scan(xdev);

	return rc;
}

static void
mic_dev_uninit(struct cosm_device *cdev)
{
	struct mic_device *xdev = cosmdev_to_xdev(cdev);

	mic_sysfs_uninit(cdev);

	mic_dmi_scan_uninit(xdev);

	/* Release log buffer. */
	dma_unmap_single(&xdev->pdev->dev, cdev->log_buf_dma_addr,
			 cdev->log_buf_size, DMA_BIDIRECTIONAL);
	kfree(cdev->log_buf_cpu_addr);

	/* Decreasing module reference when COSM releases the card. */
	module_put(THIS_MODULE);
}

static u64 mic_max_supported_address(struct cosm_device *cdev)
{
	struct mic_device *xdev = cosmdev_to_xdev(cdev);

	if (xdev->alut)
		return xdev->alut->identity_map_size;
	else
		return -1;
}

static bool
mic_in_ready_state(struct mic_device *xdev)
{
	if (mic_read_post_code(xdev) == MIC_X200_POST_CODE_BIOS_READY
			&& mic_is_fw_ready(xdev))
		return true;

	return false;
}

static bool
mic_in_online_firmware_state(struct mic_device *xdev)
{
	if (mic_read_post_code(xdev) == MIC_X200_POST_CODE_FW_READY)
		return true;

	return false;
}

static int
mic_detect_state(struct cosm_device *cdev)
{
	struct mic_device *xdev = cosmdev_to_xdev(cdev);

	log_mic_info(xdev->id, "post code: 0x%x", mic_read_post_code(xdev));

	if (mic_in_ready_state(xdev))
		return MIC_STATE_READY;

	if (mic_in_S5(xdev))
		return MIC_STATE_SHUTDOWN;

	if (mic_in_online_firmware_state(xdev))
		return MIC_STATE_ONLINE_FIRMWARE;

	if (mic_read_post_code(xdev) == MIC_X200_POST_CODE_RESETTING)
		return MIC_STATE_RESETTING;

	return MIC_STATE_UNKNOWN;
}

void
mic_stop(struct mic_device *xdev)
{
	if (xdev->scdev) {
		log_mic_info(xdev->id, "unregister scif device");
		scif_unregister_device(xdev->scdev);
		xdev->scdev = NULL;
	}

	if (xdev->vpdev) {
		log_mic_info(xdev->id, "unregister vop device");
		vop_unregister_device(xdev->vpdev);
		xdev->vpdev = NULL;
	}
}

static int
mic_boot_firmware(struct cosm_device *cdev)
{
	struct mic_device *xdev = cosmdev_to_xdev(cdev);
	int rc;

	log_mic_info(xdev->id, "start booting firmware");

	if (!mic_is_fw_ready(xdev)) {
		log_mic_err(xdev->id, "card is not in the ready state");
		return -EINVAL;
	}

	/* Make sure all devices (like scif or vop) are unregistered. */
	rc = mic_load_fw(xdev, true);
	if (rc) {
		log_mic_err(xdev->id, "mic_load_fw failed");
		return rc;
	}

	mic_enable_interrupts(xdev);
	mic_set_download_ready(xdev);
	return 0;
}

static int
mic_boot(struct cosm_device *cdev)
{
	struct mic_device *xdev = cosmdev_to_xdev(cdev);
	int rc;

	log_mic_info(xdev->id, "start booting");

	if (!mic_is_fw_ready(xdev)) {
		log_mic_err(xdev->id, "card is not in the ready state");
		return -EPERM;
	}

	mic_bootparam_init(xdev);

	xdev->vpdev = vop_register_device(&xdev->pdev->dev,
					  VOP_DEV_TRNSP, &_mic_dma_ops,
					  &vop_hw_ops, cdev->index + 1,
					  &xdev->aper, xdev->dma_ch);
	if (IS_ERR(xdev->vpdev)) {
		log_mic_err(xdev->id, "vop_register_device failed");
		rc = PTR_ERR(xdev->vpdev);
		xdev->vpdev = NULL;
		goto ret;
	}

	rc = mic_scif_setup(xdev);
	if (rc) {
		log_mic_err(xdev->id, "mic_scif_setup failed");
		goto vop_remove;
	}

	memset(cdev->log_buf_cpu_addr, 0, cdev->log_buf_size);

	rc = mic_load_fw(xdev, false);
	if (rc != 0) {
		log_mic_err(xdev->id, "mic_load_fw failed");
		goto scif_remove;
	}

	mic_enable_interrupts(xdev);

	mic_write_spad(xdev, MIC_SPAD_DPLO, xdev->dp_dma_addr);
	mic_write_spad(xdev, MIC_SPAD_DPHI, xdev->dp_dma_addr >> 32);

	mic_set_download_ready(xdev);
	goto ret;

scif_remove:
	scif_unregister_device(xdev->scdev);
	xdev->scdev = NULL;
vop_remove:
	vop_unregister_device(xdev->vpdev);
	xdev->vpdev = NULL;
ret:
	return rc;
}

/*
 * reset the card by powering it off and on again
 */
static int
mic_reset(struct cosm_device *cdev)
{
	struct mic_device *xdev = cosmdev_to_xdev(cdev);

	mic_stop(xdev);
	mic_reset_fw_status(xdev);

	log_mic_info(xdev->id, "sending long GPIO2 signal to power off");
	mic_hw_send_gpio_signal(xdev, MIC_GPIO2, MIC_GPIO_LONG);

	/* allow the card to complete transition to S5 */
	msleep(MIC_S5_TRANSITION_DELAY_MS);

	log_mic_info(xdev->id, "sending short GPIO2 signal to power on");
	mic_hw_send_gpio_signal(xdev, MIC_GPIO2, MIC_GPIO_SHORT);

	mic_set_postcode(xdev, MIC_X200_POST_CODE_RESETTING);
	return 0;
}

/*
 * reset the card by performing a hot reset
 */
static int
mic_reset_warm(struct cosm_device *cdev)
{
	struct mic_device *xdev = cosmdev_to_xdev(cdev);

	mic_stop(xdev);
	mic_reset_fw_status(xdev);

	log_mic_info(xdev->id, "sending short GPIO2 signal");
	mic_hw_send_gpio_signal(xdev, MIC_GPIO2, MIC_GPIO_SHORT);

	log_mic_info(xdev->id, "sending short GPIO1 signal");
	mic_hw_send_gpio_signal(xdev, MIC_GPIO1, MIC_GPIO_SHORT);

	mic_set_postcode(xdev, MIC_X200_POST_CODE_RESETTING);
	return 0;
}

static int
mic_shutdown(struct cosm_device *cdev)
{
	struct mic_device *xdev = cosmdev_to_xdev(cdev);

	mic_stop(xdev);
	mic_reset_fw_status(xdev);

	log_mic_info(xdev->id, "sending long GPIO2 signal to shutdown");
	mic_hw_send_gpio_signal(xdev, MIC_GPIO2, MIC_GPIO_LONG);

	/* TODO: remove after SMC update to v2.31 */
	msleep(MIC_S5_TRANSITION_DELAY_MS);
	mic_set_postcode(xdev, MIC_X200_POST_CODE_SHUTDOWN);
	return 0;
}

static void
mic_cleanup(struct cosm_device *cdev)
{
	struct mic_device *xdev = cosmdev_to_xdev(cdev);

	mic_stop(xdev);
}

struct cosm_hw_ops cosm_mic_hw_ops = {
	.boot = mic_boot,
	.boot_firmware = mic_boot_firmware,
	.reset = mic_reset,
	.reset_warm = mic_reset_warm,
	.shutdown = mic_shutdown,

	.reset_timeout = 60,
	.boot_firmware_timeout = 40,

	.detect_state = mic_detect_state,

	.cleanup = mic_cleanup,

	.dev_init = mic_dev_init,
	.dev_update = mic_dev_update,
	.dev_uninit = mic_dev_uninit,

	.aper = _mic_aper,
	.max_supported_address = mic_max_supported_address
};
