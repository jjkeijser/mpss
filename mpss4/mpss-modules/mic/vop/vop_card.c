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
 *
 * Adapted from:
 *
 * virtio for kvm on s390
 *
 * Copyright IBM Corp. 2008
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2 only)
 * as published by the Free Software Foundation.
 *
 *    Author(s): Christian Borntraeger <borntraeger@de.ibm.com>
 */
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/dma-mapping.h>

#include "vop.h"

#define VOP_MAX_VRINGS 4

#ifdef RHEL_RELEASE_VERSION
#if RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7, 2)
#define LEGACY_VIRTIO
#endif
#elif LINUX_VERSION_CODE < KERNEL_VERSION(3, 19, 0)
#define LEGACY_VIRTIO
#endif
/*
 * _vop_vdev - Allocated per virtio device instance injected by the peer.
 *
 * @vdev: Virtio device
 * @desc: Virtio device page descriptor
 * @dc: Virtio device control
 * @vpdev: VOP device which is the parent for this virtio device
 * @vr: Buffer for accessing the VRING
 * @used: Buffer for used
 * @used_size: Size of the used buffer
 * @virtio_cookie: Cookie returned upon requesting a interrupt
 * @c2h_vdev_db: The doorbell used by the guest to interrupt the host
 * @h2c_vdev_db: The doorbell used by the host to interrupt the guest
 * @dnode: The destination node
 */
struct _vop_vdev {
	struct virtio_device vdev;
	struct mic_device_desc __iomem *desc;
	struct mic_device_ctrl __iomem *dc;
	struct vop_device *vpdev;
	void __iomem *vr[VOP_MAX_VRINGS];
	dma_addr_t used[VOP_MAX_VRINGS];
	int used_size[VOP_MAX_VRINGS];
	struct mic_irq *virtio_cookie;
	int c2h_vdev_db;
	int h2c_vdev_db;
	int dnode;
};

#define to_vopvdev(vd) container_of(vd, struct _vop_vdev, vdev)

#define _vop_aligned_desc_size(d) __mic_align(_vop_desc_size(d), 8)

/* Helper API to obtain the parent of the virtio device */
static inline struct device *_vop_dev(struct _vop_vdev *vdev)
{
	return vdev->vdev.dev.parent;
}

static inline unsigned _vop_desc_size(struct mic_device_desc __iomem *desc)
{
	return sizeof(*desc)
		+ ioread8(&desc->num_vq) * sizeof(struct mic_vqconfig)
		+ ioread8(&desc->feature_len) * 2
		+ ioread8(&desc->config_len);
}

static inline struct mic_vqconfig __iomem *
_vop_vq_config(struct mic_device_desc __iomem *desc)
{
	return (struct mic_vqconfig __iomem *)(desc + 1);
}

static inline u8 __iomem *
_vop_vq_features(struct mic_device_desc __iomem *desc)
{
	return (u8 __iomem *)(_vop_vq_config(desc) + ioread8(&desc->num_vq));
}

static inline u8 __iomem *
_vop_vq_configspace(struct mic_device_desc __iomem *desc)
{
	return _vop_vq_features(desc) + ioread8(&desc->feature_len) * 2;
}

static inline unsigned
_vop_total_desc_size(struct mic_device_desc __iomem *desc)
{
	return _vop_aligned_desc_size(desc) + sizeof(struct mic_device_ctrl);
}

/* This gets the device's feature bits. */
#ifndef LEGACY_VIRTIO
static u64 vop_get_features(struct virtio_device *vdev)
#else
static u32 vop_get_features(struct virtio_device *vdev)
#endif
{
	unsigned int i, bits;
	u32 features = 0;
	struct mic_device_desc __iomem *desc = to_vopvdev(vdev)->desc;
	u8 __iomem *in_features = _vop_vq_features(desc);
	int feature_len = ioread8(&desc->feature_len);

	bits = min_t(unsigned, feature_len, sizeof(vdev->features)) * 8;
	for (i = 0; i < bits; i++)
		if (ioread8(&in_features[i / 8]) & (BIT(i % 8)))
			features |= BIT(i);

	return features;
}

#ifndef LEGACY_VIRTIO
static int vop_finalize_features(struct virtio_device *vdev)
#else
static void vop_finalize_features(struct virtio_device *vdev)
#endif
{
	unsigned int i, bits;
	struct mic_device_desc __iomem *desc = to_vopvdev(vdev)->desc;
	u8 feature_len = ioread8(&desc->feature_len);
	/* Second half of bitmap is features we accept. */
	u8 __iomem *out_features = _vop_vq_features(desc) + feature_len;

	/* Give virtio_ring a chance to accept features. */
	vring_transport_features(vdev);

#ifndef LEGACY_VIRTIO
	/* Make sure we don't have any features > 32 bits! */
	BUG_ON((u32)vdev->features != vdev->features);
#endif

	memset_io(out_features, 0, feature_len);
	bits = min_t(unsigned, feature_len,
		     sizeof(vdev->features)) * 8;
	for (i = 0; i < bits; i++) {
#ifndef LEGACY_VIRTIO
		if (__virtio_test_bit(vdev, i))
#else
		if (test_bit(i, vdev->features))
#endif
			iowrite8(ioread8(&out_features[i / 8]) | (1 << (i % 8)),
				 &out_features[i / 8]);
	}
#ifndef LEGACY_VIRTIO
	return 0;
#endif
}

/*
 * Reading and writing elements in config space
 */
static void vop_get(struct virtio_device *vdev, unsigned int offset,
		    void *buf, unsigned len)
{
	struct mic_device_desc __iomem *desc = to_vopvdev(vdev)->desc;
	struct _vop_vdev *vvdev = to_vopvdev(vdev);

	if (offset + len > ioread8(&desc->config_len)) {
		log_mic_err(vvdev->vpdev->index,
			    "offset + len to get is too large");
		return;
	}
	memcpy_fromio(buf, _vop_vq_configspace(desc) + offset, len);
}

static void vop_set(struct virtio_device *vdev, unsigned int offset,
		    const void *buf, unsigned len)
{
	struct mic_device_desc __iomem *desc = to_vopvdev(vdev)->desc;
	struct _vop_vdev *vvdev = to_vopvdev(vdev);

	if (offset + len > ioread8(&desc->config_len)) {
		log_mic_err(vvdev->vpdev->index,
			    "offset + len to set is too large");
		return;
	}
	memcpy_toio(_vop_vq_configspace(desc) + offset, buf, len);
}

/*
 * The operations to get and set the status word just access the status
 * field of the device descriptor. set_status also interrupts the host
 * to tell about status changes.
 */
static u8 vop_get_status(struct virtio_device *vdev)
{
	return ioread8(&to_vopvdev(vdev)->desc->status);
}

static void vop_set_status(struct virtio_device *dev, u8 status)
{
	struct _vop_vdev *vdev = to_vopvdev(dev);
	struct vop_device *vpdev = vdev->vpdev;

	if (!status) {
		log_mic_err(vop_get_id(vdev->vpdev),
			    "requested status not valid, status = %#x",
			    status);
		return;
	}

	iowrite8(status, &vdev->desc->status);
	vpdev->hw_ops->send_intr(vpdev, vdev->c2h_vdev_db);
}

/* Inform host on a virtio device reset and wait for ack from host */
static void vop_reset_inform_host(struct virtio_device *dev)
{
	struct _vop_vdev *vdev = to_vopvdev(dev);
	struct mic_device_ctrl __iomem *dc = vdev->dc;
	struct vop_device *vpdev = vdev->vpdev;
	int retry;

	iowrite8(0, &dc->host_ack);
	iowrite8(1, &dc->vdev_reset);
	vpdev->hw_ops->send_intr(vpdev, vdev->c2h_vdev_db);
	/* Wait till host completes all card accesses and acks the reset */
	for (retry = 100; retry--;) {
		if (ioread8(&dc->host_ack))
			break;
		msleep(100);
	};

	log_mic_dbg(vop_get_id(vdev->vpdev),
		    "after wait for host, retry == %d", retry);
}

static void vop_reset(struct virtio_device *dev)
{
	struct _vop_vdev *vdev = to_vopvdev(dev);

	log_mic_dbg(vop_get_id(vdev->vpdev), "reset virtio device, id %d",
		    dev->id.device);

	vop_reset_inform_host(dev);
}

/*
 * The virtio_ring code calls this API when it wants to notify the Host.
 */
#ifdef RHEL_RELEASE_VERSION
#if RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7, 1)
static bool vop_notify(struct virtqueue *vq)
#else
static void vop_notify(struct virtqueue *vq)
#endif
#elif (defined CONFIG_SUSE_KERNEL)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 12, 49)
static bool vop_notify(struct virtqueue *vq)
#else
static void vop_notify(struct virtqueue *vq)
#endif
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0)
static bool vop_notify(struct virtqueue *vq)
#else
static void vop_notify(struct virtqueue *vq)
#endif
#endif
{
	struct _vop_vdev *vdev = vq->priv;
	struct vop_device *vpdev = vdev->vpdev;

	vpdev->hw_ops->send_intr(vpdev, vdev->c2h_vdev_db);
#ifdef RHEL_RELEASE_VERSION
#if RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7, 1)
	return true;
#endif
#elif (defined CONFIG_SUSE_KERNEL)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 12, 49)
	return true;
#endif
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0)
	return true;
#endif
#endif
}

static void vop_del_vq(struct virtqueue *vq, int n)
{
	struct _vop_vdev *vdev = to_vopvdev(vq->vdev);
	struct vring *vr = (struct vring *)(vq + 1);
	struct vop_device *vpdev = vdev->vpdev;

	dma_unmap_single(&vpdev->dev, vdev->used[n],
			 vdev->used_size[n], DMA_BIDIRECTIONAL);
	free_pages((unsigned long) vr->used, get_order(vdev->used_size[n]));
	vring_del_virtqueue(vq);
	vpdev->hw_ops->iounmap(vpdev, vdev->vr[n]);
	vdev->vr[n] = NULL;
}

static void vop_del_vqs(struct virtio_device *dev)
{
	struct _vop_vdev *vdev = to_vopvdev(dev);
	struct virtqueue *vq, *n;
	int idx = 0;

	log_mic_dbg(vop_get_id(vdev->vpdev), "removing vqs from %p", dev);

	list_for_each_entry_safe(vq, n, &dev->vqs, list)
		vop_del_vq(vq, idx++);
}

/*
 * This routine will assign vring's allocated in host/io memory. Code in
 * virtio_ring.c however continues to access this io memory as if it were local
 * memory without io accessors.
 */
static struct virtqueue *vop_find_vq(struct virtio_device *dev,
				     unsigned index,
				     void (*callback)(struct virtqueue *vq),
				     const char *name)
{
	struct _vop_vdev *vdev = to_vopvdev(dev);
	struct vop_device *vpdev = vdev->vpdev;
	struct mic_vqconfig __iomem *vqconfig;
	struct mic_vqconfig config;
	struct virtqueue *vq;
	void __iomem *va;
	struct _mic_vring_info __iomem *info;
	void *used;
	int vr_size, _vr_size, err, magic;
	struct vring *vr;
	u8 type = ioread8(&vdev->desc->type);

	if (index >= ioread8(&vdev->desc->num_vq)) {
		log_mic_err(vop_get_id(vdev->vpdev),
			    "requested index %d too large", index);
		return ERR_PTR(-ENOENT);
	}

	if (!name) {
		log_mic_err(vop_get_id(vdev->vpdev),
			    "passed name == NULL");
		return ERR_PTR(-ENOENT);
	}

	/* First assign the vring's allocated in host memory */
	vqconfig = _vop_vq_config(vdev->desc) + index;
	memcpy_fromio(&config, vqconfig, sizeof(config));
	_vr_size = vring_size(le16_to_cpu(config.num), MIC_VIRTIO_RING_ALIGN);
	vr_size = PAGE_ALIGN(_vr_size + sizeof(struct _mic_vring_info));
	va = vpdev->hw_ops->ioremap(vpdev, le64_to_cpu(config.address),
			vr_size);
	if (!va) {
		log_mic_err(vop_get_id(vdev->vpdev),
			    "vop config remapping failed");
		return ERR_PTR(-ENOMEM);
	}
	vdev->vr[index] = va;
	memset_io(va, 0x0, _vr_size);
	vq = vring_new_virtqueue(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0)
				index,
#endif
				le16_to_cpu(config.num), MIC_VIRTIO_RING_ALIGN,
				dev,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 3, 0)
				false,
#endif
				(void __force *)va, vop_notify, callback, name);
	if (!vq) {
		log_mic_err(vop_get_id(vdev->vpdev),
			    "can't create new virtqueue");
		err = -ENOMEM;
		goto unmap;
	}
	info = va + _vr_size;
	magic = ioread32(&info->magic);

	if (WARN(magic != MIC_MAGIC + type + index, "magic mismatch")) {
		err = -EIO;
		log_mic_err(vop_get_id(vdev->vpdev),
			    "magic expected %d, retrieved %d, err %d",
			    MIC_MAGIC + type + index, magic, err);
		goto unmap;
	}

	/* Allocate and reassign used ring now */
	vdev->used_size[index] = PAGE_ALIGN(sizeof(__u16) * 3 +
					     sizeof(struct vring_used_elem) *
					     le16_to_cpu(config.num));
	used = (void *)__get_free_pages(GFP_KERNEL | __GFP_ZERO,
					get_order(vdev->used_size[index]));
	if (!used) {
		err = -ENOMEM;
		log_mic_err(vop_get_id(vdev->vpdev),
			    "no free pages available, err %d", err);
		goto del_vq;
	}
	vdev->used[index] = dma_map_single(&vpdev->dev, used,
					    vdev->used_size[index],
					    DMA_BIDIRECTIONAL);
	if (dma_mapping_error(&vpdev->dev, vdev->used[index])) {
		err = -ENOMEM;
		log_mic_err(vop_get_id(vdev->vpdev),
			    "dma mapping error, err %d", err);
		goto free_used;
	}
	writeq(vdev->used[index], &vqconfig->used_address);
	/*
	 * To reassign the used ring here we are directly accessing
	 * struct vring_virtqueue which is a private data structure
	 * in virtio_ring.c. At the minimum, a BUILD_BUG_ON() in
	 * vring_new_virtqueue() would ensure that
	 *  (&vq->vring == (struct vring *) (&vq->vq + 1));
	 */
	vr = (struct vring *)(vq + 1);
	vr->used = used;
	vq->priv = vdev;
	return vq;
free_used:
	free_pages((unsigned long)used,
		   get_order(vdev->used_size[index]));
del_vq:
	vring_del_virtqueue(vq);
unmap:
	vpdev->hw_ops->iounmap(vpdev, vdev->vr[index]);
	return ERR_PTR(err);
}

static int vop_find_vqs(struct virtio_device *dev, unsigned nvqs,
			struct virtqueue *vqs[],
			vq_callback_t *callbacks[],
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
			const char *names[])
#else
			const char * const names[])
#endif
{
	struct _vop_vdev *vdev = to_vopvdev(dev);
	struct vop_device *vpdev = vdev->vpdev;
	struct mic_device_ctrl __iomem *dc = vdev->dc;
	int i, err, retry;

	/* We must have this many virtqueues. */
	if (nvqs > ioread8(&vdev->desc->num_vq))
		return -ENOENT;

	for (i = 0; i < nvqs; ++i) {
		log_mic_dbg(vop_get_id(vdev->vpdev),
			    "nvq index %d, name %s", i, names[i]);

		vqs[i] = vop_find_vq(dev, i, callbacks[i], names[i]);
		if (IS_ERR(vqs[i])) {
			err = PTR_ERR(vqs[i]);
			log_mic_err(vop_get_id(vdev->vpdev),
				    "error occurred on vqs[%d]", i);
			goto error;
		}
	}

	iowrite8(1, &dc->used_address_updated);
	/*
	 * Send an interrupt to the host to inform it that used
	 * rings have been re-assigned.
	 */
	vpdev->hw_ops->send_intr(vpdev, vdev->c2h_vdev_db);
	for (retry = 100; retry--;) {
		if (!ioread8(&dc->used_address_updated))
			break;
		msleep(100);
	};

	log_mic_dbg(vop_get_id(vdev->vpdev), "retry: %d", retry);
	if (!retry) {
		err = -ENODEV;
		log_mic_err(vop_get_id(vdev->vpdev), "vq not found");
		goto error;
	}

	return 0;
error:
	vop_del_vqs(dev);
	return err;
}

/*
 * The config ops structure as defined by virtio config
 */
static struct virtio_config_ops vop_vq_config_ops = {
	.get_features = vop_get_features,
	.finalize_features = vop_finalize_features,
	.get = vop_get,
	.set = vop_set,
	.get_status = vop_get_status,
	.set_status = vop_set_status,
	.reset = vop_reset,
	.find_vqs = vop_find_vqs,
	.del_vqs = vop_del_vqs,
};

static irqreturn_t vop_virtio_intr_handler(int irq, void *data)
{
	struct _vop_vdev *vdev = data;
	struct vop_device *vpdev = vdev->vpdev;
	struct virtqueue *vq;

	vpdev->hw_ops->ack_interrupt(vpdev, vdev->h2c_vdev_db);
	list_for_each_entry(vq, &vdev->vdev.vqs, list)
		vring_interrupt(0, vq);

	return IRQ_HANDLED;
}

static void vop_virtio_release_dev(struct device *dev)
{
	struct virtio_device *_dev = dev_to_virtio(dev);
	struct _vop_vdev *vdev = to_vopvdev(_dev);

	vdev->vpdev->hw_ops->free_irq(vdev->vpdev, vdev->virtio_cookie, vdev);
	iowrite8(-1, &vdev->dc->h2c_vdev_db);

	iowrite8(-1, &vdev->desc->type);
	vdev->vpdev->hw_ops->send_intr(vdev->vpdev, vdev->c2h_vdev_db);

	log_mic_host_dbg("decreasing module refcount");
	module_put(THIS_MODULE);

	kfree(vdev);
}

/*
 * adds a new device and register it with virtio
 * appropriate drivers are loaded by the device model
 */
static int _vop_add_device(struct mic_device_desc __iomem *d,
			   unsigned int offset, struct vop_device *vpdev,
			   int dnode)
{
	struct _vop_vdev *vdev;
	int ret;
	u8 type = ioread8(&d->type);

	vdev = kzalloc(sizeof(*vdev), GFP_KERNEL);
	if (!vdev) {
		return -ENOMEM;
	}

	vdev->vpdev = vpdev;
	vdev->vdev.dev.parent = &vpdev->dev;
	vdev->vdev.dev.release = vop_virtio_release_dev;
	vdev->vdev.id.device = type;
	vdev->vdev.config = &vop_vq_config_ops;
	vdev->desc = d;
	vdev->dc = (void __iomem *)d + _vop_aligned_desc_size(d);
	vdev->dnode = dnode;
	vdev->vdev.priv = (void *)(u64)dnode;

	vdev->h2c_vdev_db = vpdev->hw_ops->next_db(vpdev);
	vdev->virtio_cookie = vpdev->hw_ops->request_irq(vpdev,
			vop_virtio_intr_handler, "virtio intr",
			vdev, vdev->h2c_vdev_db);
	if (IS_ERR(vdev->virtio_cookie)) {
		ret = PTR_ERR(vdev->virtio_cookie);
		log_mic_err(vop_get_id(vdev->vpdev),
			    "virtio_cookie error, code %d", ret);
		goto kfree;
	}
	iowrite8((u8)vdev->h2c_vdev_db, &vdev->dc->h2c_vdev_db);
	vdev->c2h_vdev_db = ioread8(&vdev->dc->c2h_vdev_db);

	ret = register_virtio_device(&vdev->vdev);
	if (ret) {
		log_mic_err(vop_get_id(vdev->vpdev),
			    "Failed to register vop device %u type %u",
			    offset, type);
		goto free_irq;
	}
	writeq((u64)vdev, &vdev->dc->vdev);
	log_mic_dbg(vop_get_id(vdev->vpdev),
		    "registered vop device %u type %u vdev %p",
		    offset, type, vdev);


	log_mic_host_dbg("increasing module refcount");
	if (!try_module_get(THIS_MODULE)) {
		log_mic_err(vop_get_id(vdev->vpdev),
			    "unable to increase module refcount");
		goto free_irq;
	}

	return 0;

free_irq:
	vpdev->hw_ops->free_irq(vpdev, vdev->virtio_cookie, vdev);
kfree:
	kfree(vdev);
	return ret;
}

/*
 * match for a vop device with a specific desc pointer
 */
static int vop_match_desc(struct device *dev, void *data)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 7, 0) && !defined(dev_to_virtio)
#define dev_to_virtio(dev) container_of(dev, struct virtio_device, dev)
#endif
	struct virtio_device *_dev = dev_to_virtio(dev);
	struct _vop_vdev *vdev = to_vopvdev(_dev);

	return vdev->desc == (void __iomem *)data;
}

static void _vop_handle_config_change(struct mic_device_desc __iomem *d,
				      unsigned int offset,
				      struct vop_device *vpdev)
{
	struct mic_device_ctrl __iomem *dc
		= (void __iomem *)d + _vop_aligned_desc_size(d);
	struct _vop_vdev *vdev = (struct _vop_vdev *)readq(&dc->vdev);
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 18, 0)
	struct virtio_driver *drv;
#endif

	if (ioread8(&dc->config_change) != MIC_VIRTIO_PARAM_CONFIG_CHANGED)
		return;

	log_mic_dbg(vop_get_id(vdev->vpdev), "config_change handling");
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 18, 0)
	drv = container_of(vdev->vdev.dev.driver,
			   struct virtio_driver, driver);
	if (drv->config_changed)
		drv->config_changed(&vdev->vdev);
#else
	virtio_config_changed(&vdev->vdev);
#endif
	iowrite8(1, &dc->guest_ack);
}

/*
 * removes a virtio device if a hot remove event has been
 * requested by the host.
 */
static int _vop_remove_device(struct mic_device_desc __iomem *d,
			      unsigned int offset, struct vop_device *vpdev)
{
	struct mic_device_ctrl __iomem *dc
		= (void __iomem *)d + _vop_aligned_desc_size(d);
	struct _vop_vdev *vdev = (struct _vop_vdev *)readq(&dc->vdev);
	int ret = -1;

	if (ioread8(&dc->config_change) == MIC_VIRTIO_PARAM_DEV_REMOVE) {
		log_mic_dbg(vop_get_id(vdev->vpdev),
			    "config_change %d type %d vdev %p",
			    ioread8(&dc->config_change), ioread8(&d->type),
			    vdev);
		unregister_virtio_device(&vdev->vdev);
		ret = 0;
	}
	return ret;
}

#define REMOVE_DEVICES true

static void _vop_scan_devices(void __iomem *dp, struct vop_device *vpdev,
			      bool remove, int dnode)
{
	s8 type;
	unsigned int i;
	struct mic_device_desc __iomem *d;
	struct mic_device_ctrl __iomem *dc;
	struct device *dev;
	int ret;

	for (i = sizeof(struct mic_bootparam);
			i < MIC_DP_SIZE; i += _vop_total_desc_size(d)) {
		d = dp + i;
		dc = (void __iomem *)d + _vop_aligned_desc_size(d);
		/*
		 * This read barrier is paired with the corresponding write
		 * barrier on the host which is inserted before adding or
		 * removing a virtio device descriptor, by updating the type.
		 */
		rmb();
		type = ioread8(&d->type);

		/* end of list */
		if (type == 0)
			break;

		if (type == -1)
			continue;

		/* device already exists */
		dev = device_find_child(&vpdev->dev, (void __force *)d,
					vop_match_desc);
		if (dev) {
			if (remove)
				iowrite8(MIC_VIRTIO_PARAM_DEV_REMOVE,
					 &dc->config_change);
			put_device(dev);
			_vop_handle_config_change(d, i, vpdev);
			ret = _vop_remove_device(d, i, vpdev);
			continue;
		}

		/* new device */
		log_mic_dbg(vpdev->index,
			    "Adding new virtio device %p", d);
		if (!remove)
			_vop_add_device(d, i, vpdev, dnode);
	}
}

static void vop_scan_devices(struct vop_info *vi,
			     struct vop_device *vpdev, bool remove)
{
	void __iomem *dp = vpdev->hw_ops->get_remote_dp(vpdev);

	if (!dp) {
		log_mic_err(vpdev->index, "remote dp == NULL");
		return;
	}
	mutex_lock(&vop_mutex);
	_vop_scan_devices(dp, vpdev, remove, vpdev->dnode);
	mutex_unlock(&vop_mutex);
}

/*
 * vop_hotplug_device tries to find changes in the device page.
 */
static void vop_hotplug_devices(struct work_struct *work)
{
	struct vop_info *vi = container_of(work, struct vop_info,
					     hotplug_work);

	vop_scan_devices(vi, vi->vpdev, !REMOVE_DEVICES);
}

/*
 * Interrupt handler for hot plug/config changes etc.
 */
static irqreturn_t vop_extint_handler(int irq, void *data)
{
	struct vop_info *vi = data;
	struct mic_bootparam __iomem *bp;
	struct vop_device *vpdev = vi->vpdev;

	bp = vpdev->hw_ops->get_remote_dp(vpdev);
	log_mic_dbg(vpdev->index, "hotplug work");
	vpdev->hw_ops->ack_interrupt(vpdev, ioread8(&bp->h2c_config_db));
	schedule_work(&vi->hotplug_work);
	return IRQ_HANDLED;
}

int vop_init(struct vop_info *vi)
{
	struct mic_bootparam __iomem *bootparam;
	int rc;

	INIT_WORK(&vi->hotplug_work, vop_hotplug_devices);
	vop_scan_devices(vi, vi->vpdev, !REMOVE_DEVICES);

	vi->h2c_config_db = vi->vpdev->hw_ops->next_db(vi->vpdev);
	vi->cookie = vi->vpdev->hw_ops->request_irq(vi->vpdev,
						vop_extint_handler,
						"virtio_config_intr",
						vi, vi->h2c_config_db);
	if (IS_ERR(vi->cookie)) {
		rc = PTR_ERR(vi->cookie);
		log_mic_err(vi->vpdev->index,
			    "irq cookie corrupted, PTR_ERR = %d", rc);
		goto exit;
	}
	bootparam = vi->vpdev->hw_ops->get_remote_dp(vi->vpdev);
	iowrite8(vi->h2c_config_db, &bootparam->h2c_config_db);

	return 0;
exit:
	return rc;
}

void vop_uninit(struct vop_info *vi)
{
	struct vop_device *vpdev = vi->vpdev;
	struct mic_bootparam __iomem *bootparam =
		vpdev->hw_ops->get_remote_dp(vpdev);

	if (bootparam)
		iowrite8(-1, &bootparam->h2c_config_db);

	vpdev->hw_ops->free_irq(vpdev, vi->cookie, vi);
	flush_work(&vi->hotplug_work);
	vop_scan_devices(vi, vpdev, REMOVE_DEVICES);
}
