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
 * Intel Virtio Over PCIe (VOP) driver.
 */
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/version.h>
#include <linux/dma-mapping.h>
#include <linux/iommu.h>
#include <linux/moduleparam.h>

#ifdef MIC_IN_KERNEL_BUILD
#include <linux/mic_common.h>
#else
#include "../common/mic_common.h"
#endif
#include "../common/mic_dev.h"

#include "mic_ioctl.h"
#include "vop.h"

// TODO: remove this param before upstreaming
static bool disable_dma = 0;

/* Helper API to obtain the VOP PCIe device */
static inline struct device *vop_dev(struct vop_vdev *vdev)
{
	return vdev->vpdev->dev.parent;
}

/* Helper API to check if a virtio device is initialized */
static inline int vop_vdev_get_status(struct vop_vdev *vdev)
{
	/* Device has not been created yet */
	if (!vdev || !vdev->dd || !vdev->dd->type)
		return -EINVAL;

	/* Device removed or failed */
	if (vdev->dd->type == -1)
		return -ENODEV;

	return 0;
}

static void _vop_notify(struct vringh *vrh)
{
	struct vop_vringh *vvrh = container_of(vrh, struct vop_vringh, vrh);
	struct vop_vdev *vdev = vvrh->vdev;
	struct vop_device *vpdev = vdev->vpdev;
	s8 db = vdev->dc->h2c_vdev_db;

	if (db != -1)
		vpdev->hw_ops->send_intr(vpdev, db);
}

static void vop_virtio_init_post(struct vop_vdev *vdev)
{
	struct mic_vqconfig *vqconfig = mic_vq_config(vdev->dd);
	struct vop_device *vpdev = vdev->vpdev;
	int i, used_size;

	for (i = 0; i < vdev->dd->num_vq; i++) {
		used_size = PAGE_ALIGN(sizeof(u16) * 3 +
				sizeof(struct vring_used_elem) *
				le16_to_cpu(vqconfig->num));
		if (!le64_to_cpu(vqconfig[i].used_address)) {
			log_mic_warn(vop_get_id(vdev->vpdev), "used_address zero??");
			continue;
		}
		vdev->vvr[i].vrh.vring.used =
			(void __force *)vpdev->hw_ops->ioremap(
			vpdev,
			le64_to_cpu(vqconfig[i].used_address),
			used_size);
	}

	vdev->dc->used_address_updated = 0;
	log_mic_info(vpdev->index, "device type %d LINKUP", vdev->virtio_id);
}

static inline void vop_virtio_device_reset(struct vop_vdev *vdev)
{
	log_mic_dbg(vop_get_id(vdev->vpdev), "status %d device type %d RESET",
		    vdev->dd->status, vdev->dd->type);

	vdev->dd->status = 0; /* 0 means "reset" */
	vdev->dc->vdev_reset = 0;
	vdev->dc->host_ack = 1;
}

static void vop_bh_handler(struct work_struct *work)
{
	struct vop_vdev *vdev = container_of(work, struct vop_vdev,
			virtio_bh_work);

	if (vdev->dc->used_address_updated)
		vop_virtio_init_post(vdev);

	if (vdev->dc->vdev_reset)
		vop_virtio_device_reset(vdev);

	vdev->poll_wake = 1;
	wake_up(&vdev->waitq);
}

static irqreturn_t _vop_virtio_intr_handler(int irq, void *data)
{
	struct vop_vdev *vdev = data;
	struct vop_device *vpdev = vdev->vpdev;

	vpdev->hw_ops->ack_interrupt(vpdev, vdev->virtio_db);
	schedule_work(&vdev->virtio_bh_work);
	return IRQ_HANDLED;
}

static int vop_copy_dp_entry(struct vop_vdev *vdev,
			     struct mic_device_desc *argp, __u8 *type,
			     struct mic_device_desc **devpage)
{
	struct vop_device *vpdev = vdev->vpdev;
	struct mic_device_desc *devp;
	struct mic_vqconfig *vqconfig;
	int ret = 0, i;
	bool slot_found = false;

	vqconfig = mic_vq_config(argp);
	for (i = 0; i < argp->num_vq; i++) {
		if (le16_to_cpu(vqconfig[i].num) > MIC_MAX_VRING_ENTRIES) {
			ret =  -EINVAL;
			log_mic_err(vop_get_id(vdev->vpdev),
				    "too much entries to copy, err %d", ret);
			goto exit;
		}
	}

	/* Find the first free device page entry */
	for (i = sizeof(struct mic_bootparam);
		i < MIC_DP_SIZE - mic_total_desc_size(argp);
		i += mic_total_desc_size(devp)) {
		devp = vpdev->hw_ops->get_dp(vpdev) + i;
		if (devp->type == 0 || devp->type == -1) {
			slot_found = true;
			break;
		}
	}
	if (!slot_found) {
		ret =  -EINVAL;
		log_mic_err(vop_get_id(vdev->vpdev),
			    "slot not found, err %d", ret);
		goto exit;
	}
	/*
	 * Save off the type before doing the memcpy. Type will be set in the
	 * end after completing all initialization for the new device.
	 */
	*type = argp->type;
	argp->type = 0;
	memcpy(devp, argp, mic_desc_size(argp));

	*devpage = devp;
exit:
	return ret;
}

static void vop_init_device_ctrl(struct vop_vdev *vdev,
				 struct mic_device_desc *devpage)
{
	struct mic_device_ctrl *dc;

	dc = (void *)devpage + mic_aligned_desc_size(devpage);

	dc->config_change = 0;
	dc->guest_ack = 0;
	dc->vdev_reset = 0;
	dc->host_ack = 0;
	dc->used_address_updated = 0;
	dc->c2h_vdev_db = -1;
	dc->h2c_vdev_db = -1;
	vdev->dc = dc;
}

static int vop_virtio_add_device(struct vop_vdev *vdev,
				 struct mic_device_desc *argp)
{
	struct vop_info *vi = vdev->vi;
	struct vop_device *vpdev = vi->vpdev;
	struct device *dma_dev = vdev->vi->dma_ch->device->dev;
	struct mic_device_desc *dd = NULL;
	struct mic_vqconfig *vqconfig;
	int vr_size, i, j, ret;
	u8 type = 0;
	s8 db = -1;
	char irqname[16];
	struct mic_bootparam *bootparam;
	u16 num;
	dma_addr_t vr_addr;

	bootparam = vpdev->hw_ops->get_dp(vpdev);
	init_waitqueue_head(&vdev->waitq);
	INIT_LIST_HEAD(&vdev->list);
	vdev->vpdev = vpdev;

	ret = vop_copy_dp_entry(vdev, argp, &type, &dd);
	if (ret) {
		kfree(vdev);
		log_mic_err(vop_get_id(vdev->vpdev),
			    "can't copy dp entries, err %d", ret);
		return ret;
	}

	vop_init_device_ctrl(vdev, dd);

	vdev->dd = dd;
	vdev->virtio_id = type;
	vqconfig = mic_vq_config(dd);
	INIT_WORK(&vdev->virtio_bh_work, vop_bh_handler);

	for (i = 0; i < dd->num_vq; i++) {
		struct vop_vringh *vvr = &vdev->vvr[i];
		struct mic_vring *vr = &vdev->vvr[i].vring;

		num = le16_to_cpu(vqconfig[i].num);
		mutex_init(&vvr->vr_mutex);

		vr_size = PAGE_ALIGN(vring_size(num, MIC_VIRTIO_RING_ALIGN) +
			sizeof(struct _mic_vring_info));

		vr->va = dma_zalloc_coherent(dma_dev, vr_size, &vr_addr,
					     GFP_KERNEL);
		if (!vr->va) {
			ret = -ENOMEM;
			log_mic_err(vop_get_id(vdev->vpdev),
				    "dma_alloc_coherent error, err %d", ret);
			goto err;
		}

		vr->len = vr_size;
		vr->info = vr->va + vring_size(num, MIC_VIRTIO_RING_ALIGN);
		vr->info->magic = cpu_to_le32(MIC_MAGIC + vdev->virtio_id + i);

		vqconfig[i].address = cpu_to_le64(vr_addr);

		vring_init(&vr->vr, num, vr->va, MIC_VIRTIO_RING_ALIGN);
		ret = vringh_init_kern(&vvr->vrh,
					*(u32 *)mic_vq_features(vdev->dd),
					num, false, vr->vr.desc, vr->vr.avail,
					vr->vr.used);
		if (ret) {
			log_mic_err(vop_get_id(vdev->vpdev),
				    "can't create vringh, err %d", ret);
			goto err;
		}
		vringh_kiov_init(&vvr->riov, NULL, 0);
		vringh_kiov_init(&vvr->wiov, NULL, 0);
		vvr->head = USHRT_MAX;
		vvr->vdev = vdev;
		vvr->vrh.notify = _vop_notify;
		log_mic_dbg(vop_get_id(vdev->vpdev),
			    "index %d va %p info %p vr_size 0x%x",
			i, vr->va, vr->info, vr_size);
		vvr->buf = dma_alloc_coherent(dma_dev,
				VOP_INT_DMA_BUF_SIZE, &vvr->buf_da,
				GFP_KERNEL|__GFP_ZERO);
		if (!vvr->buf) {
			ret = -ENOMEM;
			log_mic_err(vop_get_id(vdev->vpdev),
				    "dma allocation failed, err %d", ret);
			goto err;
		}
	}

	snprintf(irqname, sizeof(irqname), "vop%dvirtio%d", vpdev->index,
		 vdev->virtio_id);
	vdev->virtio_db = vpdev->hw_ops->next_db(vpdev);
	vdev->virtio_cookie = vpdev->hw_ops->request_irq(vpdev,
			_vop_virtio_intr_handler, irqname, vdev,
			vdev->virtio_db);
	if (IS_ERR(vdev->virtio_cookie)) {
		ret = PTR_ERR(vdev->virtio_cookie);
		log_mic_err(vop_get_id(vdev->vpdev), "request irq failed");
		goto err;
	}

	vdev->dc->c2h_vdev_db = vdev->virtio_db;

	/*
	 * Order the type update with previous stores. This write barrier
	 * is paired with the corresponding read barrier before the uncached
	 * system memory read of the type, on the card while scanning the
	 * device page.
	 */
	smp_wmb();
	dd->type = type;
	argp->type = type;

	if (bootparam) {
		db = bootparam->h2c_config_db;
		if (db != -1)
			vpdev->hw_ops->send_intr(vpdev, db);
	}
	log_mic_dbg(vop_get_id(vdev->vpdev),
		    "added virtio id %d db %d", dd->type, db);


	return 0;
err:
	vqconfig = mic_vq_config(dd);
	for (j = 0; j < i; j++) {
		struct vop_vringh *vvr = &vdev->vvr[j];
		struct mic_vring *vr = &vvr->vring;

		if (vvr->buf) {
			dma_free_coherent(dma_dev,
				VOP_INT_DMA_BUF_SIZE, vvr->buf, vvr->buf_da);
		}

		if (vr->va) {
			dma_free_coherent(dma_dev, (size_t)vr->len, (void*)vr->va,
					  (dma_addr_t)vqconfig[j].address);
			vr->va = NULL;
		}

	}
	return ret;
}

static void vop_dev_remove(struct mic_device_ctrl *devp,
			   struct vop_vdev *vdev)
{
	struct vop_device *vpdev = vdev->vpdev;
	struct mic_bootparam *bootparam = vpdev->hw_ops->get_dp(vpdev);
	s8 db;
	int ret, retry;
	DECLARE_WAIT_QUEUE_HEAD_ONSTACK(wake);
	devp->config_change = MIC_VIRTIO_PARAM_DEV_REMOVE;
	db = bootparam->h2c_config_db;
	if (db != -1)
		vpdev->hw_ops->send_intr(vpdev, db);

	for (retry = 5; retry--;) {
		ret = wait_event_timeout(wake, devp->vdev_reset,
					msecs_to_jiffies(1000));
		if (ret)
			break;
	}

	if (!ret)
		log_mic_warn(vop_get_id(vdev->vpdev),
			     "waiting for reset request finished with timeout");

	devp->config_change = 0;
	devp->guest_ack = 0;

	vop_virtio_device_reset(vdev);

	while (retry-- > 0) {
		ret = wait_event_timeout(wake, vdev->dd->type == -1,
					 msecs_to_jiffies(1000));
		if (ret)
			break;
	}

	if (!ret)
		log_mic_warn(vop_get_id(vdev->vpdev),
			     "waiting for release finished with timeout");

	vdev->dd->type = -1;

	wake_up(&vdev->waitq);
}

static void vop_virtio_del_device(struct vop_vdev *vdev)
{
	struct vop_device *vpdev = vdev->vpdev;
	struct device *dma_dev = vdev->vi->dma_ch->device->dev;
	struct mic_vqconfig *vqconfig;
	struct mic_bootparam *bootparam = vpdev->hw_ops->get_dp(vpdev);
	int i;

	vpdev->hw_ops->free_irq(vpdev, vdev->virtio_cookie, vdev);
	flush_work(&vdev->virtio_bh_work);

	if (bootparam && vdev->dd->type != -1)
		vop_dev_remove(vdev->dc, vdev);

	vqconfig = mic_vq_config(vdev->dd);
	for (i = 0; i < vdev->dd->num_vq; i++) {
		struct vop_vringh *vvr = &vdev->vvr[i];
		if (vvr->buf) {
			dma_free_coherent(dma_dev,
					  VOP_INT_DMA_BUF_SIZE, vvr->buf,
					  vvr->buf_da);
			vvr->buf = NULL;
		}
		vringh_kiov_cleanup(&vvr->riov);
		vringh_kiov_cleanup(&vvr->wiov);
		if (vvr->vring.va) {
			dma_free_coherent(dma_dev, (size_t)vvr->vring.len,
					  vvr->vring.va,
					  (dma_addr_t)vqconfig[i].address);

			vvr->vring.va = NULL;
		}
	}
	/*
	 * Order the type update with previous stores. This write barrier
	 * is paired with the corresponding read barrier before the uncached
	 * system memory read of the type, on the card while scanning the
	 * device page.
	 */
	smp_wmb();
	vdev->deleted = true;
	vdev->dd->type = -1;
}

/*
 * vop_sync_dma - Wrapper for synchronous DMAs.
 *
 * @dev - The address of the pointer to the device instance used
 * for DMA registration.
 * @dst - destination DMA address.
 * @src - source DMA address.
 * @len - size of the transfer.
 *
 * Return DMA_SUCCESS on success
 */
static int vop_sync_dma(struct vop_vdev *vdev, dma_addr_t dst, dma_addr_t src,
			size_t len)
{
	int err = 0;
	struct dma_device *ddev;
	struct dma_async_tx_descriptor *tx;
	struct vop_info *vi = vdev->vpdev->priv;
	struct dma_chan *vop_ch = vi->dma_ch;

	if (!vop_ch) {
		err = -EBUSY;
		log_mic_err(vop_get_id(vdev->vpdev),
			    "dma channel not available, err %d", err);
		goto error;
	}
	ddev = vop_ch->device;
	tx = ddev->device_prep_dma_memcpy(vop_ch, dst, src, len,
		DMA_PREP_FENCE);
	if (!tx) {
		err = -ENOMEM;
		log_mic_err(vop_get_id(vdev->vpdev),
			    "can't prepare dma memory, err %d", err);
		goto error;
	} else {
		dma_cookie_t cookie;

		cookie = tx->tx_submit(tx);
		if (dma_submit_error(cookie)) {
			err = -ENOMEM;
			log_mic_err(vop_get_id(vdev->vpdev),
				    "can't submit error, err %d", err);
			goto error;
		}
		err = dma_sync_wait(vop_ch, cookie);
	}
error:
	if (err)
		log_mic_err(vop_get_id(vdev->vpdev),
			    "error occurred, err %d", err);
	return err;
}

/**
 *  vop_setsg - Initialize a scatterlist array
 *  @pa: physical memory address
 *  @page_size: data length
 *  @page_cnt: the number of entries in sg table
 *
 *  returns pointer to sg allocated page array
 */
static struct scatterlist *
vop_setsg(phys_addr_t pa, int page_size, int page_cnt)
{
	struct scatterlist *sg;
	struct page *page;
	int i;

	sg = kcalloc(page_cnt, sizeof(struct scatterlist), GFP_KERNEL);
	if (!sg)
		return NULL;
	sg_init_table(sg, page_cnt);
	for (i = 0; i < page_cnt; i++) {
		page = pfn_to_page(pa >> PAGE_SHIFT);
		sg_set_page(&sg[i], page, page_size, 0);
		pa += page_size;
	}
	return sg;
}

/**
* vop_dma_map: create DMA mappings if the IOMMU is enabled
* @vop_vdev: VOP remote device
* @sg: scaterlist
* @daddr: offset address in a bar
* @dma_addr: mapped DMA address
* @num_pages: number of pages in the sg table
* @size: size of the data to be DMA maped
*
* Map the physical pages using dma_map_sg(..) and then detect the number
* of contiguous DMA mappings allocated
*/
static int vop_dma_map(struct vop_vdev *vdev, struct scatterlist **sg,
		       u64 daddr, u64 *dma_addr, size_t num_pages, size_t size)
{
	struct vop_device *vpdev = vdev->vpdev;
	struct vop_info *vi = vdev->vpdev->priv;
	struct dma_chan *vop_ch = vi->dma_ch;
	int rc = 0;

	if (iommu_present(vpdev->dev.bus)) {
		*sg = vop_setsg(vpdev->aper->pa + daddr, PAGE_SIZE, num_pages);
		if (!(*sg)) {
			log_mic_err(vop_get_id(vdev->vpdev),
				    "dma_setsg error, num_pages: 0x%zx",
				    num_pages);
			goto free_sg;
		}
		rc = dma_map_sg(vop_ch->device->dev, *sg, num_pages,
			DMA_BIDIRECTIONAL);
		if (rc != num_pages) {
			log_mic_err(vop_get_id(vdev->vpdev),
				    "dma_map_sg error, num_pages: 0x%zx",
				    num_pages);
			goto dma_unmap_sg;
		}
		*dma_addr = sg_dma_address(*sg) + (daddr & PAGE_MASK);
		log_mic_dbg(vop_get_id(vdev->vpdev),
			    "sg: daddr 0x%llx size 0x%zx dma 0x%llx",
			    vpdev->aper->pa, size, *dma_addr);
	} else {
		*dma_addr = vpdev->aper->pa + daddr;
	}
	return 0;
dma_unmap_sg:
	dma_unmap_sg(vop_ch->device->dev, *sg,
		num_pages, DMA_BIDIRECTIONAL);
free_sg:
	kfree(*sg);
	return rc;
}


/**
* vop_dma_unmap: remove DMA mappings if the IOMMU is enabled
* @vop_vdev: VOP remote device
* @sg: scaterlist
* @num_pages: number of pages in the sg table
*
* Remove the mapping that was created by the vop_dma_map() function
*/
static void vop_dma_unmap(struct vop_vdev *vdev, struct scatterlist *sg,
		   size_t num_pages)
{
	struct vop_device *vpdev = vdev->vpdev;
	struct vop_info *vi = vdev->vpdev->priv;

	if (iommu_present(vpdev->dev.bus)) {
		dma_unmap_sg(vi->dma_ch->device->dev, sg,
			num_pages, DMA_BIDIRECTIONAL);
	}
}


/*
 * Initiates the copies across the PCIe bus from card memory to a user
 * space buffer. When transfers are done using DMA, source/destination
 * addresses and transfer length must follow the alignment requirements of
 * the MIC DMA engine.
 */
static int vop_virtio_copy_to_user(struct vop_vdev *vdev, void __user *ubuf,
				   size_t len, u64 daddr, size_t dlen,
				   int vr_idx)
{
	struct vop_device *vpdev = vdev->vpdev;
	void __iomem *dbuf = vpdev->hw_ops->ioremap(vpdev, daddr, len);
	struct vop_vringh *vvr = &vdev->vvr[vr_idx];
	struct vop_info *vi = vdev->vpdev->priv;
	size_t dma_alignment = 1 << vi->dma_ch->device->copy_align;
	bool x200 = is_dma_copy_aligned(vi->dma_ch->device, 1, 1, 1);
	size_t dma_offset, partlen;
	size_t num_pages = 0;
	struct scatterlist *sg;
	int err;

	if (disable_dma) {
		if (copy_to_user(ubuf, (void __force *)dbuf, len)) {
			err = -EFAULT;
			log_mic_err(vop_get_id(vdev->vpdev),
				    "can't copy to user, "
				    "length %#zx, err %d", len, err);
			goto err;
		}
		vdev->in_bytes += len;
		err = 0;
		goto err;
	}

	dma_offset = daddr - round_down(daddr, dma_alignment);
	daddr -= dma_offset;
	len += dma_offset;
	/*
	 * X100 uses DMA addresses as seen by the card so adding
	 * the aperture base is not required for DMA. However x200
	 * requires DMA addresses to be an offset into the bar so
	 * add the aperture base for x200.
	 */
	if (x200) {
		num_pages = (daddr + len)/PAGE_SIZE - daddr/PAGE_SIZE + 1;
		err = vop_dma_map(vdev, &sg, daddr, &daddr, num_pages, len);
		if (err) {
			log_mic_err(vop_get_id(vdev->vpdev),
				    "dma map failure, err %d", err);
		}
	}
	while (len) {
		partlen = min_t(size_t, len, VOP_INT_DMA_BUF_SIZE);
		err = vop_sync_dma(vdev, vvr->buf_da, daddr,
				   ALIGN(partlen, dma_alignment));
		if (err) {
			log_mic_err(vop_get_id(vdev->vpdev),
				    "dma sync transfer failure, err %d", err);
			goto err;
		}
		if (copy_to_user(ubuf, vvr->buf + dma_offset,
				 partlen - dma_offset)) {
			err = -EFAULT;
			log_mic_err(vop_get_id(vdev->vpdev),
				    "copy to user failed, length %#zx, err %d",
				    partlen - dma_offset, err);
			goto err;
		}
		daddr += partlen;
		ubuf += partlen;
		dbuf += partlen;
		vdev->in_bytes_dma += partlen;
		vdev->in_bytes += partlen;
		len -= partlen;
		dma_offset = 0;
	}
	err = 0;
err:
	if (x200 && !disable_dma) {
		vop_dma_unmap(vdev, sg, num_pages);
	}
	vpdev->hw_ops->iounmap(vpdev, dbuf);
	return err;
}

/*
 * Initiates copies across the PCIe bus from a user space buffer to card
 * memory. When transfers are done using DMA, source/destination addresses
 * and transfer length must follow the alignment requirements of the MIC
 * DMA engine.
 */
static int vop_virtio_copy_from_user(struct vop_vdev *vdev, void __user *ubuf,
				     size_t len, u64 daddr, size_t dlen,
				     int vr_idx)
{
	struct vop_device *vpdev = vdev->vpdev;
	void __iomem *dbuf = vpdev->hw_ops->ioremap(vpdev, daddr, len);
	struct vop_vringh *vvr = &vdev->vvr[vr_idx];
	struct vop_info *vi = vdev->vpdev->priv;
	size_t dma_alignment = 1 << vi->dma_ch->device->copy_align;
	bool x200 = is_dma_copy_aligned(vi->dma_ch->device, 1, 1, 1);
	struct scatterlist *sg;
	size_t partlen, num_pages = 0;
	bool dma = !disable_dma;
	int err = 0;

	if (daddr & (dma_alignment - 1)) {
		vdev->tx_dst_unaligned += len;
		dma = false;
	} else if (ALIGN(len, dma_alignment) > dlen) {
		vdev->tx_len_unaligned += len;
		dma = false;
	}

	if (!dma)
		goto memcpy;

	/*
	 * X100 uses DMA addresses as seen by the card so adding
	 * the aperture base is not required for DMA. However x200
	 * requires DMA addresses to be an offset into the bar so
	 * add the aperture base for x200.
	 */
	if (x200) {
		num_pages = (daddr + len)/PAGE_SIZE - daddr/PAGE_SIZE + 1;
		err = vop_dma_map(vdev, &sg, daddr, &daddr, num_pages, len);
		if (err) {
			log_mic_err(vop_get_id(vdev->vpdev),
				    "dma map failed, err %d", err);
		}
	}
	while (len) {
		partlen = min_t(size_t, len, VOP_INT_DMA_BUF_SIZE);

		if (copy_from_user(vvr->buf, ubuf, partlen)) {
			err = -EFAULT;
			log_mic_err(vop_get_id(vdev->vpdev),
				    "copy from user failure, "
				    "length %#zx, err %d", partlen, err);
			goto err;
		}
		err = vop_sync_dma(vdev, daddr, vvr->buf_da,
				   ALIGN(partlen, dma_alignment));
		if (err) {
			log_mic_err(vop_get_id(vdev->vpdev),
				    "dma sync transfer failed, err %d", err);
			goto err;
		}
		daddr += partlen;
		ubuf += partlen;
		dbuf += partlen;
		vdev->out_bytes_dma += partlen;
		vdev->out_bytes += partlen;
		len -= partlen;
	}
memcpy:
	/*
	 * We are copying to IO below and should ideally use something
	 * like copy_from_user_toio(..) if it existed.
	 */
	if (copy_from_user((void __force *)dbuf, ubuf, len)) {
		err = -EFAULT;
		log_mic_err(vop_get_id(vdev->vpdev), "copy_from_user failed, "
			    "length %#zx, err %d", len, err);
		goto err;
	}
	vdev->out_bytes += len;
	err = 0;
err:
	if (x200 && dma) {
		vop_dma_unmap(vdev, sg, num_pages);
	}
	vpdev->hw_ops->iounmap(vpdev, dbuf);
	return err;
}

#define MIC_VRINGH_READ true

/* Determine the total number of bytes consumed in a VRINGH KIOV */
static inline u32 vop_vringh_iov_consumed(struct vringh_kiov *iov)
{
	int i;
	u32 total = iov->consumed;

	for (i = 0; i < iov->i; i++)
		total += iov->iov[i].iov_len;
	return total;
}

/*
 * Traverse the VRINGH KIOV and issue the APIs to trigger the copies.
 * This API is heavily based on the vringh_iov_xfer(..) implementation
 * in vringh.c. The reason we cannot reuse vringh_iov_pull_kern(..)
 * and vringh_iov_push_kern(..) directly is because there is no
 * way to override the VRINGH xfer(..) routines as of v3.10.
 */
static int vop_vringh_copy(struct vop_vdev *vdev, struct vringh_kiov *iov,
			   void __user *ubuf, size_t len, bool read, int vr_idx,
			   size_t *out_len)
{
	int ret = 0;
	size_t partlen, tot_len = 0;

	while (len && iov->i < iov->used) {
		struct kvec *kiov = &iov->iov[iov->i];

		partlen = min(kiov->iov_len, len);
		if (read)
			ret = vop_virtio_copy_to_user(vdev, ubuf, partlen,
						      (u64)kiov->iov_base,
						      kiov->iov_len,
						      vr_idx);
		else
			ret = vop_virtio_copy_from_user(vdev, ubuf, partlen,
							(u64)kiov->iov_base,
							kiov->iov_len,
							vr_idx);
		if (ret) {
			log_mic_err(vop_get_id(vdev->vpdev),
				    "dma transfer failed, err %d", ret);
			break;
		}
		len -= partlen;
		ubuf += partlen;
		tot_len += partlen;
		iov->consumed += partlen;
		kiov->iov_len -= partlen;
		kiov->iov_base += partlen;
		if (!kiov->iov_len) {
			/* Fix up old iov element then increment. */
			kiov->iov_len = iov->consumed;
			kiov->iov_base -= iov->consumed;

			iov->consumed = 0;
			iov->i++;
		}
	}
	*out_len = tot_len;
	return ret;
}

/*
 * Use the standard VRINGH infrastructure in the kernel to fetch new
 * descriptors, initiate the copies and update the used ring.
 */
static int _vop_virtio_copy(struct vop_vdev *vdev, struct mic_copy_desc *copy)
{
	int ret = 0;
	u32 iovcnt = copy->iovcnt;
	struct iovec iov;
	struct iovec __user *u_iov = copy->iov;
	void __user *ubuf = NULL;
	struct vop_vringh *vvr = &vdev->vvr[copy->vr_idx];
	struct vringh_kiov *riov = &vvr->riov;
	struct vringh_kiov *wiov = &vvr->wiov;
	struct vringh *vrh = &vvr->vrh;
	u16 *head = &vvr->head;
	struct mic_vring *vr = &vvr->vring;
	size_t len = 0, out_len;

	copy->out_len = 0;
	/* Fetch a new IOVEC if all previous elements have been processed */
	if (riov->i == riov->used && wiov->i == wiov->used) {
		ret = vringh_getdesc_kern(vrh, riov, wiov,
					  head, GFP_KERNEL);
		/* Check if there are available descriptors */
		if (ret <= 0) {
			log_mic_err(vop_get_id(vdev->vpdev),
				    "no descriptors available, err %d", ret);
			return ret;
		}
	}
	while (iovcnt) {
		if (!len) {
			/* Copy over a new iovec from user space. */
			ret = copy_from_user(&iov, u_iov, sizeof(*u_iov));
			if (ret) {
				ret = -EINVAL;
				log_mic_err(vop_get_id(vdev->vpdev),
					    "copy from user failed, "
					    "length %#lx, err %d",
					    sizeof(*u_iov), ret);
				break;
			}
			len = iov.iov_len;
			ubuf = iov.iov_base;
		}
		/* Issue all the read descriptors first */
		ret = vop_vringh_copy(vdev, riov, ubuf, len,
				      MIC_VRINGH_READ, copy->vr_idx, &out_len);
		if (ret) {
			log_mic_err(vop_get_id(vdev->vpdev),
				    "vringh copy failed, err %d", ret);
			break;
		}
		len -= out_len;
		ubuf += out_len;
		copy->out_len += out_len;
		/* Issue the write descriptors next */
		ret = vop_vringh_copy(vdev, wiov, ubuf, len,
				      !MIC_VRINGH_READ, copy->vr_idx, &out_len);
		if (ret) {
			log_mic_err(vop_get_id(vdev->vpdev),
				    "vringh copy failed, err %d", ret);
			break;
		}
		len -= out_len;
		ubuf += out_len;
		copy->out_len += out_len;
		if (!len) {
			/* One user space iovec is now completed */
			iovcnt--;
			u_iov++;
		}
		/* Exit loop if all elements in KIOVs have been processed. */
		if (riov->i == riov->used && wiov->i == wiov->used)
			break;
	}
	/*
	 * Update the used ring if a descriptor was available and some data was
	 * copied in/out and the user asked for a used ring update.
	 */
	if (*head != USHRT_MAX && copy->out_len && copy->update_used) {
		u32 total = 0;

		/* Determine the total data consumed */
		total += vop_vringh_iov_consumed(riov);
		total += vop_vringh_iov_consumed(wiov);
		vringh_complete_kern(vrh, *head, total);
		*head = USHRT_MAX;
		vringh_notify(vrh);

		vringh_kiov_cleanup(riov);
		vringh_kiov_cleanup(wiov);
		/* Update avail idx for user space */
		vr->info->avail_idx = vrh->last_avail_idx;
	}
	return ret;
}

static inline int vop_verify_copy_args(struct vop_vdev *vdev,
				       struct mic_copy_desc *copy)
{
	if (!vdev)
		return -EINVAL;

	if (copy->vr_idx >= vdev->dd->num_vq) {
		log_mic_err(vop_get_id(vdev->vpdev),
			    "copy args incorrect, err %d", -EINVAL);
		return -EINVAL;
	}
	return 0;
}

/* Copy a specified number of virtio descriptors in a chain */
static int vop_virtio_copy_desc(struct vop_vdev *vdev,
				struct mic_copy_desc *copy)
{
	int err;
	struct vop_vringh *vvr = &vdev->vvr[copy->vr_idx];

	err = vop_verify_copy_args(vdev, copy);
	if (err)
		return err;

	mutex_lock(&vvr->vr_mutex);
	if (!vop_vdevup(vdev)) {
		err = -ENODEV;
		goto err;
	}
	err = _vop_virtio_copy(vdev, copy);
	if (err) {
		log_mic_err(vop_get_id(vdev->vpdev),
			    "virtio copy failure, err %d", err);
	}
err:
	mutex_unlock(&vvr->vr_mutex);
	return err;
}

static int vop_open(struct inode *inode, struct file *f)
{
	struct vop_vdev *vdev;
	struct vop_info *vi = container_of(f->private_data,
		struct vop_info, miscdev);

	vdev = kzalloc(sizeof(*vdev), GFP_KERNEL);
	if (!vdev)
		return -ENOMEM;

	vdev->vi = vi;
	vdev->deleted = false;
	mutex_init(&vdev->vdev_mutex);
	f->private_data = vdev;
	return 0;
}

static int vop_release(struct inode *inode, struct file *f)
{
	struct vop_vdev *vdev = f->private_data;

	mutex_lock(&vdev->vdev_mutex);

	mutex_lock(&vop_mutex);
	if (!list_empty(&vdev->list)) {
		log_mic_info(vop_get_id(vdev->vpdev),
			     "removing list from vdev:%p type:%d",
			     vdev, vdev->dd->type);
		list_del_init(&vdev->list);
	}

	mutex_unlock(&vop_mutex);

	if (!vdev->deleted) {
		log_mic_info(vop_get_id(vdev->vpdev),
			     "deleting vdev:%p type:%d",
			     vdev, vdev->dd->type);
		vop_virtio_del_device(vdev);
	}

	mutex_unlock(&vdev->vdev_mutex);

	kfree(vdev);
	f->private_data = NULL;
	return 0;
}

static long vop_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	struct vop_vdev *vdev = f->private_data;
	struct vop_info *vi = vdev->vi;
	void __user *argp = (void __user *)arg;
	int ret;

	switch (cmd) {
	case MIC_VIRTIO_ADD_DEVICE:
	{
		struct mic_device_desc dd, *dd_config;

		if (copy_from_user(&dd, argp, sizeof(dd))) {
			log_mic_err(vop_get_id(vdev->vpdev),
				    "can't copy descriptor from user, "
				    "length %#lx, err %d",
				    sizeof(dd), -EFAULT);
			return -EFAULT;
		}

		if (mic_aligned_desc_size(&dd) > MIC_MAX_DESC_BLK_SIZE ||
		    dd.num_vq > MIC_MAX_VRINGS) {
			log_mic_err(vop_get_id(vdev->vpdev),
				    "mic descriptor too long, "
				    "length %#x, err %d",
				    mic_aligned_desc_size(&dd), -EINVAL);
			return -EINVAL;
		}

		dd_config = kzalloc(mic_desc_size(&dd), GFP_KERNEL);
		if (!dd_config)
			return -ENOMEM;

		if (copy_from_user(dd_config, argp, mic_desc_size(&dd))) {
			log_mic_err(vop_get_id(vdev->vpdev),
				    "can't copy config from user, "
				    "length %#x, err %d",
				    mic_desc_size(&dd), -EFAULT);
			ret = -EFAULT;
			goto free_ret;
		}
		mutex_lock(&vdev->vdev_mutex);
		mutex_lock(&vop_mutex);
		ret = vop_virtio_add_device(vdev, dd_config);
		if (ret)
			goto unlock_ret;
		list_add_tail(&vdev->list, &vi->vdev_list);
unlock_ret:
		mutex_unlock(&vop_mutex);
		mutex_unlock(&vdev->vdev_mutex);
free_ret:
		kfree(dd_config);
		return ret;
	}
	case MIC_VIRTIO_COPY_DESC:
	{
		struct mic_copy_desc copy;

		mutex_lock(&vdev->vdev_mutex);
		ret = vop_vdev_get_status(vdev);
		if (ret) {
			if (ret == -EINVAL)
				log_mic_host_err("invalid arguments "
						 "(vop device doesn't exist), "
						 "err %d", ret);
			else
				log_mic_host_dbg("device descriptor "
						 "not initialized, err %d", ret);
			goto _unlock_ret;
		}

		if (copy_from_user(&copy, argp, sizeof(copy))) {
			log_mic_err(vop_get_id(vdev->vpdev),
				    "can't copy copy_desc from user, "
				    "length %#lx, err %d",
				    sizeof(copy), -EFAULT);
			ret = -EFAULT;
			goto _unlock_ret;
		}

		ret = vop_virtio_copy_desc(vdev, &copy);
		if (ret < 0) {
			log_mic_host_dbg("device descriptor "
					 "not initialized, err %d", ret);
			goto _unlock_ret;
		}

		if (copy_to_user(
			&((struct mic_copy_desc __user *)argp)->out_len,
			&copy.out_len, sizeof(copy.out_len))) {
			log_mic_err(vop_get_id(vdev->vpdev),
				    "can't copy copy_desc to user, "
				    "length %#lx, err %d",
				    sizeof(copy.out_len), -EFAULT);
			ret = -EFAULT;
		}
_unlock_ret:
		mutex_unlock(&vdev->vdev_mutex);
		return ret;
	}
	default:
		return -ENOIOCTLCMD;
	};
	return 0;
}

/*
 * We return POLLIN | POLLOUT from poll when new buffers are enqueued, and
 * not when previously enqueued buffers may be available. This means that
 * in the card->host (TX) path, when userspace is unblocked by poll it
 * must drain all available descriptors or it can stall.
 */
static unsigned int vop_poll(struct file *f, poll_table *wait)
{
	struct vop_vdev *vdev = f->private_data;
	int mask = 0;
	int err;

	if (!vdev) {
		log_mic_host_err("vop_poll ERROR: vdev is NULL");
		return 0;
	}

	mutex_lock(&vdev->vdev_mutex);

	err = vop_vdev_get_status(vdev);
	if (err)
		goto error;

	poll_wait(f, &vdev->waitq, wait);

	err = vop_vdev_get_status(vdev);
	if (err)
		goto error;

	if (vdev->poll_wake) {
		mask = POLLIN | POLLOUT;
		vdev->poll_wake = 0;
	}
	goto unlock;

error:
	/*
	 * ENODEV - device already removed
	 * EINVAL - device not created yet
	 */
	switch (err) {
	case -ENODEV:
		log_mic_dbg(vdev->vi->vpdev->dnode - 1, "vop_poll: %d", err);
		mask = POLLHUP;
		break;
	case -EINVAL:
		log_mic_err(vdev->vi->vpdev->dnode - 1, "vop_poll ERROR: %d", err);
		mask = POLLERR;
		break;
	default:
		log_mic_host_err("unknown error");
	}

unlock:
	mutex_unlock(&vdev->vdev_mutex);
	return mask;
}

static inline int
vop_query_offset(struct vop_vdev *vdev, unsigned long offset,
		 unsigned long *size, unsigned long *pa)
{
	struct vop_device *vpdev = vdev->vpdev;
	unsigned long start = MIC_DP_SIZE;
	int i;

	/*
	 * MMAP interface is as follows:
	 * offset				region
	 * 0x0					virtio device_page
	 * 0x1000				first vring
	 * 0x1000 + size of 1st vring		second vring
	 * ....
	 */
	if (!offset) {
		*pa = virt_to_phys(vpdev->hw_ops->get_dp(vpdev));
		*size = MIC_DP_SIZE;
		return 0;
	}

	for (i = 0; i < vdev->dd->num_vq; i++) {
		struct vop_vringh *vvr = &vdev->vvr[i];

		if (offset == start) {
			*pa = virt_to_phys(vvr->vring.va);
			*size = vvr->vring.len;
			return 0;
		}
		start += vvr->vring.len;
	}
	return -1;
}

/*
 * Maps the device page and virtio rings to user space for readonly access.
 */
static int vop_mmap(struct file *f, struct vm_area_struct *vma)
{
	struct vop_vdev *vdev = f->private_data;
	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long pa = 0;
	unsigned long size = vma->vm_end - vma->vm_start, size_rem = size;
	int i, err;

	err = vop_vdev_get_status(vdev);
	if (err) {
		if (printk_ratelimit())
			log_mic_host_err("vop device not initialized, err %d",
					 err);
		goto ret;
	}
	if (vma->vm_flags & VM_WRITE) {
		err = -EACCES;
		log_mic_err(vop_get_id(vdev->vpdev),
			    "readonly access, err %d", err);
		goto ret;
	}
	while (size_rem) {
		i = vop_query_offset(vdev, offset, &size, &pa);
		if (i < 0) {
			err = -EINVAL;
			log_mic_err(vop_get_id(vdev->vpdev),
				    "can't retrieve offset, offset %#lx, "
				    "size %#lx, pa %#lx, err %d",
				    offset, size, pa, err);
			goto ret;
		}
		err = remap_pfn_range(vma, vma->vm_start + offset,
				      pa >> PAGE_SHIFT, size,
				      vma->vm_page_prot);
		if (err) {
			log_mic_err(vop_get_id(vdev->vpdev),
				    "can't remap range, "
				    "addr %#lx, pfn %#lx, size %#lx, err %d",
				    vma->vm_start + offset, pa >> PAGE_SHIFT,
				    size, -err);
			goto ret;
		}
		size_rem -= size;
		offset += size;
	}
ret:
	return err;
}

static const struct file_operations vop_fops = {
	.open = vop_open,
	.release = vop_release,
	.unlocked_ioctl = vop_ioctl,
	.poll = vop_poll,
	.mmap = vop_mmap,
	.owner = THIS_MODULE,
};

int vop_init(struct vop_info *vi)
{
	int rc;
	struct miscdevice *mdev;
	struct vop_device *vpdev = vi->vpdev;

	INIT_LIST_HEAD(&vi->vdev_list);
	vi->dma_ch = vpdev->dma_ch;
	mdev = &vi->miscdev;
	mdev->minor = MISC_DYNAMIC_MINOR;
	snprintf(vi->name, sizeof(vi->name), "vop_virtio%d", vpdev->index);
	mdev->name = vi->name;
	mdev->fops = &vop_fops;
	mdev->parent = &vpdev->dev;

	rc = misc_register(mdev);
	if (rc)
		log_mic_err(vpdev->index,
			    "failed to register miscdevice, rc %d", rc);
	return rc;
}

void vop_uninit(struct vop_info *vi)
{
	struct vop_vdev *vdev;

	while (true) {
		mutex_lock(&vop_mutex);
		if (list_empty(&vi->vdev_list)) {
			log_mic_host_info("list empty for vi:%p", vi);
			mutex_unlock(&vop_mutex);
			break;
		}
		vdev = list_first_entry(&vi->vdev_list, struct vop_vdev, list);
		log_mic_info(vop_get_id(vdev->vpdev),
		"removing list for vdev:%p\n", vdev);
		list_del_init(&vdev->list);
		mutex_unlock(&vop_mutex);

		mutex_lock(&vdev->vdev_mutex);
		if (!vdev->deleted) {
			log_mic_info(vop_get_id(vdev->vpdev),
				     "deleting vdev:%p", vdev);
			vop_virtio_del_device(vdev);
		}

		mutex_unlock(&vdev->vdev_mutex);
	}
	misc_deregister(&vi->miscdev);
}

module_param(disable_dma, bool, 0644);
MODULE_PARM_DESC(disable_dma, "Disable DMA engine for all transfers");
