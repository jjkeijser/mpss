 /*
  * Copyright (C) 2009 Red Hat, Inc.
  * Author: Michael S. Tsirkin <mst@redhat.com>
  *
  * This work is licensed under the terms of the GNU GPL, version 2.

  * (C) Badari Pulavarty pbadari@us.ibm.com 2010 with the following comment.
  * He posted on http://lwn.net/Articles/382543/

  * virtio-block server in host kernel.
  * Inspired by vhost-net and shamlessly ripped code from it :)

  * For adapting to MIC
  * (C) Copyright 2012 Intel Corporation
  * Author: Caz Yokoyama <Caz.Yokoyama@intel.com>
  */
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,34) || \
	defined(RHEL_RELEASE_CODE)

#include <linux/compat.h>
#include <linux/eventfd.h>
#include <linux/vhost.h>
#include <linux/virtio_ring.h>
#include <linux/virtio_net.h>
#include <linux/virtio_blk.h>
#include <linux/mmu_context.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/rcupdate.h>
#include <linux/file.h>
#include <linux/fdtable.h>

#ifndef VIRTIO_RING_F_EVENT_IDX  /* virtio_ring.h of rhel6.0 does not define */
#define VIRTIO_RING_F_EVENT_IDX		29
#endif
#include "mic_common.h"
#include "mic/micveth_dma.h"
#include "vhost.h"
#include "mic/mic_virtio.h"

#define SECTOR_SHIFT		9
#define SECTOR_SIZE		(1UL << SECTOR_SHIFT)
#define VIRTIO_BLK_QUEUE_SIZE		128
#define DISK_SEG_MAX			(VIRTIO_BLK_QUEUE_SIZE - 2)

#define VHOST_BLK_VQ_MAX 1
#define WQNAME_SIZE 16

struct vhost_blk {
	struct vhost_dev dev;
	struct vhost_virtqueue vqs[VHOST_BLK_VQ_MAX];
	struct vhost_poll poll[VHOST_BLK_VQ_MAX];
	struct workqueue_struct *vb_wq;
	char vb_wqname[WQNAME_SIZE];
	struct work_struct vb_ws_bh;
	struct workqueue_struct *vblk_workqueue;
	struct board_info *bd_info;
	char *file_name;
	struct file *virtblk_file;
};

struct vhost_blk_io {
	struct list_head list;
	struct work_struct work;
	struct vhost_blk *blk;
	struct file *file;
	int head;
	uint32_t type;
	uint32_t nvecs;
	uint64_t sector;
	uint64_t len;
	struct iovec iov[0];
};

#define mic_addr_in_host(va, pa) ((u8 *)(va) + (u64)(pa))

static LIST_HEAD(write_queue);
static LIST_HEAD(read_queue);

static void
cleanup_vblk_workqueue(struct vhost_blk_io *vbio, struct vhost_virtqueue *vq)
{
	struct list_head single, *head, *node, *tmp;
	int need_free;
	struct vhost_blk_io *entry;

	if (vbio->head != -1) {
		INIT_LIST_HEAD(&single);
		list_add(&vbio->list, &single);
		head = &single;
		need_free = 0;
	} else {
		head = &vbio->list;
		need_free = 1;
	}

	mutex_lock(&vq->mutex);
	list_for_each_safe(node, tmp, head) {
		entry = list_entry(node, struct vhost_blk_io, list);
		list_del(node);
		kfree(entry);
	}
	mutex_unlock(&vq->mutex);

	if (need_free)
		kfree(vbio);
}

static void handle_io_work(struct work_struct *work)
{
	struct vhost_blk_io *vbio, *entry;
	struct vhost_virtqueue *vq;
	struct vhost_blk *blk;
	struct list_head single, *head, *node, *tmp;
	struct iovec *iov;
	uint8_t *aper_va;
	struct vring *vring;
	unsigned int num;

	int need_free, ret = 0;
	loff_t pos;
	uint8_t status = 0;

	vbio = container_of(work, struct vhost_blk_io, work);
	blk = vbio->blk;
	vq = &blk->dev.vqs[0];
	pos = vbio->sector << SECTOR_SHIFT;
	aper_va = blk->bd_info->bi_ctx.aper.va;

	vring = &((struct mic_virtblk *)blk->bd_info->bi_virtio)->vb_shared.vring;
	num = readl(&vring->num);
	if (num == 0 || micpm_get_reference(&blk->bd_info->bi_ctx, true)) {
		cleanup_vblk_workqueue(vbio, vq);
		return;
	}

	if (atomic64_read(&vbio->file->f_count) == 0) {  /* file is closed */
		ret = -1;
	} else if (vbio->type & VIRTIO_BLK_T_FLUSH)  {
#ifdef RHEL_RELEASE_CODE
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
		ret = vfs_fsync(vbio->file, 1);
#else
		ret = vfs_fsync(vbio->file, vbio->file->f_path.dentry, 1);
#endif
#else
		ret = vfs_fsync(vbio->file, 1);
#endif
	} else if (vbio->type & VIRTIO_BLK_T_OUT) {
	  for (iov = vbio->iov; iov < &vbio->iov[vbio->nvecs]; iov++) {
		iov->iov_base = mic_addr_in_host(aper_va, iov->iov_base);
	  }
		ret = vfs_writev(vbio->file, vbio->iov, vbio->nvecs, &pos);
	} else {
	  for (iov = vbio->iov; iov < &vbio->iov[vbio->nvecs]; iov++) {
		iov->iov_base = mic_addr_in_host(aper_va, iov->iov_base);
	  }
		ret = vfs_readv(vbio->file, vbio->iov, vbio->nvecs, &pos);
	}
	status = (ret < 0) ? VIRTIO_BLK_S_IOERR : VIRTIO_BLK_S_OK;
	if (vbio->head != -1) {
		INIT_LIST_HEAD(&single);
		list_add(&vbio->list, &single);
		head = &single;
		need_free = 0;
	} else {
		head = &vbio->list;
		need_free = 1;
	}
	list_for_each_entry(entry, head, list) {
		memcpy_toio(mic_addr_in_host(aper_va, entry->iov[entry->nvecs].iov_base), &status, sizeof(status));
	}
	mutex_lock(&vq->mutex);
	list_for_each_safe(node, tmp, head) {
		entry = list_entry(node, struct vhost_blk_io, list);
		vhost_add_used_and_signal(&blk->dev, vq, entry->head, ret);
		list_del(node);
		kfree(entry);
	}
	mutex_unlock(&vq->mutex);
	if (need_free)
		kfree(vbio);
	micpm_put_reference(&blk->bd_info->bi_ctx);
}

static struct vhost_blk_io *allocate_vbio(int nvecs)
{
	struct vhost_blk_io *vbio;
	int size = sizeof(struct vhost_blk_io) + nvecs * sizeof(struct iovec);
	vbio = kmalloc(size, GFP_KERNEL);
	if (vbio) {
		INIT_WORK(&vbio->work, handle_io_work);
		INIT_LIST_HEAD(&vbio->list);
	}
	return vbio;
}

static void merge_and_handoff_work(struct list_head *queue)
{
	struct vhost_blk_io *vbio, *entry;
	int nvecs = 0;
	int entries = 0;

	list_for_each_entry(entry, queue, list) {
		nvecs += entry->nvecs;
		entries++;
	}

	if (entries == 1) {
		vbio = list_first_entry(queue, struct vhost_blk_io, list);
		list_del(&vbio->list);
		queue_work(vbio->blk->vblk_workqueue, &vbio->work);
		return;
	}

	vbio = allocate_vbio(nvecs);
	if (!vbio) {
		/* Unable to allocate memory - submit IOs individually */
		list_for_each_entry(vbio, queue, list) {
			queue_work(vbio->blk->vblk_workqueue, &vbio->work);
		}
		INIT_LIST_HEAD(queue);
		return;
	}

	entry = list_first_entry(queue, struct vhost_blk_io, list);
	vbio->nvecs = nvecs;
	vbio->blk = entry->blk;
	vbio->file = entry->file;
	vbio->type = entry->type;
	vbio->sector = entry->sector;
	vbio->head = -1;
	vbio->len = 0;
	nvecs = 0;

	list_for_each_entry(entry, queue, list) {
		memcpy(&vbio->iov[nvecs], entry->iov, entry->nvecs * sizeof(struct iovec));
		nvecs += entry->nvecs;
		vbio->len += entry->len;
	}
	list_replace_init(queue, &vbio->list);
	queue_work(vbio->blk->vblk_workqueue, &vbio->work);
}

static void start_io(struct list_head *queue)
{
	struct list_head start;
	struct vhost_blk_io *vbio = NULL, *entry;

	if (list_empty(queue))
                return;

	list_for_each_entry(entry, queue, list) {
		if (!vbio) {
			vbio = entry;
			continue;
		}
		if (vbio->sector + (vbio->len >> SECTOR_SHIFT) == entry->sector) {
			vbio = entry;
		} else {
			INIT_LIST_HEAD(&start);
			list_cut_position(&start, queue, &vbio->list);
			merge_and_handoff_work(&start);
			vbio = entry;
		}
	}
	if (!list_empty(queue))
		merge_and_handoff_work(queue);
}

static uint64_t calculate_len(struct iovec *iov, int nvecs)
{
	uint64_t len = 0;
	int i;

	for (i=0; i<nvecs; i++)
		len += iov[i].iov_len;
	return len;
}

static void insert_to_queue(struct vhost_blk_io *vbio,
			struct list_head *queue)
{
	struct vhost_blk_io *entry;

	list_for_each_entry(entry, queue, list) {
		if (entry->sector > vbio->sector)
			break;
	}
	list_add_tail(&vbio->list, &entry->list);
}

static int handoff_io(struct vhost_blk *blk, int head,
			uint32_t type, uint64_t sector,
			struct iovec *iov, int nvecs)
{
	struct vhost_virtqueue *vq = &blk->dev.vqs[0];
	struct vhost_blk_io *vbio;

	vbio = allocate_vbio(nvecs+1);
	if (!vbio) {
		return -ENOMEM;
	}
	vbio->blk = blk;
	vbio->head = head;
	vbio->file = vq->private_data;
	vbio->type = type;
	vbio->sector = sector;
	vbio->nvecs = nvecs;
	vbio->len = calculate_len(iov, nvecs);
	memcpy(vbio->iov, iov, (nvecs + 1) * sizeof(struct iovec));

	if (vbio->type & VIRTIO_BLK_T_FLUSH) {
#if 0
		/* Sync called - do I need to submit IOs in the queue ? */
		start_io(&read_queue);
		start_io(&write_queue);
#endif
		queue_work(blk->vblk_workqueue, &vbio->work);
	} else if (vbio->type & VIRTIO_BLK_T_OUT) {
		insert_to_queue(vbio, &write_queue);
	} else {
		insert_to_queue(vbio, &read_queue);
	}
	return 0;
}

static void handle_blk(struct vhost_blk *blk)
{
	struct vhost_virtqueue *vq = &blk->dev.vqs[0];
	unsigned head, out, in;
	struct virtio_blk_outhdr hdr;
	int nvecs;
	struct board_info *bd_info = blk->bd_info;
	struct vring *vring;

	vring = &((struct mic_virtblk *)bd_info->bi_virtio)->vb_shared.vring;
	if (vring == 0 || readl(&vring->num) == 0) {
		printk("request comes in while card side driver is not loaded yet. Ignore\n");
		return;
	}
	/* the first time since the card side driver becomes ready */
	if (vq->desc == NULL || readb(&((struct mic_virtblk *)bd_info->bi_virtio)->vb_shared.update)) {
	  vq->num = readl(&vring->num);
	  vq->desc = (struct vring_desc *)readq(&vring->desc);
	  vq->avail = (struct vring_avail *)readq(&vring->avail);
	  vq->used = (struct vring_used *)readq(&vring->used);
	  vq->last_avail_idx = 0;
	  vq->avail_idx = 0;
	  vq->last_used_idx = 0;
	  vq->signalled_used = 0;
	  vq->signalled_used_valid = false;
	  vq->done_idx = 0;
	  writeb(false, &((struct mic_virtblk *)bd_info->bi_virtio)->vb_shared.update);
	}

	if (micpm_get_reference(&blk->bd_info->bi_ctx,  true))
		return;

	mutex_lock(&vq->mutex);

	vhost_disable_notify(&blk->dev, vq);

	for (;;) {
		head = vhost_get_vq_desc(&blk->dev, vq, vq->iov,
					 ARRAY_SIZE(vq->iov),
					 &out, &in, NULL, NULL);
		if ((head == vq->num) || (head == -EFAULT) || (head == -EINVAL)) {
			if (unlikely(vhost_enable_notify(&blk->dev, vq))) {
				vhost_disable_notify(&blk->dev, vq);
				continue;
			}
			start_io(&read_queue);
			start_io(&write_queue);
			break;
		}

		BUG_ON(vq->iov[0].iov_len != 16);

		memcpy_fromio(&hdr, mic_addr_in_host(bd_info->bi_ctx.aper.va, vq->iov[0].iov_base),
					  sizeof(hdr));

		nvecs = out - 1;
		if (hdr.type == VIRTIO_BLK_T_IN)
			nvecs = in - 1;

		BUG_ON(vq->iov[nvecs+1].iov_len != 1);
		if (handoff_io(blk, head, hdr.type, hdr.sector, &vq->iov[1], nvecs) < 0) {
		  vhost_discard_vq_desc(vq, 1);
			continue;
		}
	}
	mutex_unlock(&vq->mutex);
	micpm_put_reference(&blk->bd_info->bi_ctx);
}

static void handle_blk_kick(struct work_struct *work)
{
	struct vhost_blk *vblk;

	vblk = container_of(work, struct vhost_blk, vb_ws_bh);
	handle_blk(vblk);
}

#if 0
static void handle_rq_blk(struct vhost_work *work)
{
	struct vhost_blk *blk;

	blk = container_of(work, struct vhost_blk, poll[0].work);
	handle_blk(blk);
}
#endif

static int
vhost_doorbell_intr_handler(mic_ctx_t *mic_ctx, int doorbell)
{
	struct board_info *bi;
	struct vhost_blk *vblk;

	bi = container_of(mic_ctx, struct board_info, bi_ctx);
	vblk = ((struct mic_virtblk *)bi->bi_virtio)->vblk;
	queue_work(vblk->vb_wq, &vblk->vb_ws_bh);

	return 0;
}

static long vhost_blk_set_backend(struct vhost_blk *vblk)
{
	struct vhost_virtqueue *vq;
	struct board_info *bd_info = vblk->bd_info;
	unsigned index = bd_info->bi_ctx.bi_id;
	struct vb_shared *vb_shared;
	int ret = 0;
	struct kstat stat;
	unsigned int virtio_blk_features = (1U << VIRTIO_BLK_F_SEG_MAX) |
					   (1U << VIRTIO_BLK_F_BLK_SIZE);

	if (index >= MAX_BOARD_SUPPORTED) {
		ret = -ENOBUFS;
		goto _exit_;
	}
	if (vblk->virtblk_file == NULL) {
		ret = -EBADF;
		goto _exit_;
	}

	vq = &vblk->vqs[0];
	mutex_lock(&vq->mutex);
	rcu_assign_pointer(vq->private_data, vblk->virtblk_file);
	mutex_unlock(&vq->mutex);

	snprintf(vblk->vb_wqname, sizeof(vblk->vb_wqname),
		 "virtblk wq %d", index);
	vblk->vb_wq = __mic_create_singlethread_workqueue(vblk->vb_wqname);
	if (vblk->vb_wq == NULL) {
		ret = -ENOMEM;
		goto _exit_;
	}
	INIT_WORK(&vblk->vb_ws_bh, handle_blk_kick);

	/* They have to be accessed from "struct vhost_virtqueue *vq" in mic_vhost.c.
	   They are not used in vhost block. I don't modify vhost.h. */
	vq->log_base = (void __user *)&bd_info->bi_ctx;
	vq->log_addr = (u64)bd_info->bi_ctx.aper.va;

	vb_shared = &((struct mic_virtblk *)bd_info->bi_virtio)->vb_shared;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,6,0))
	virtio_blk_features |= (1U << VIRTIO_BLK_F_FLUSH);
#endif
	writel(virtio_blk_features, &vb_shared->host_features);
	writel(DISK_SEG_MAX, &vb_shared->blk_config.seg_max);
	writel(SECTOR_SIZE, &vb_shared->blk_config.blk_size);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0))
	ret = vfs_getattr(&vblk->virtblk_file->f_path, &stat);
#else
	ret = vfs_getattr(vblk->virtblk_file->f_path.mnt,
					  vblk->virtblk_file->f_path.dentry, &stat);
#endif
	if (ret < 0)
		goto _exit_;

	if (S_ISBLK(stat.mode)) {
	  writel(i_size_read(I_BDEV(vblk->virtblk_file->f_mapping->host)->bd_inode) / SECTOR_SIZE,
			 &vb_shared->blk_config.capacity);
	} else {
	  writel(stat.size / SECTOR_SIZE, &vb_shared->blk_config.capacity);
	}

	ret = mic_reg_irqhandler(&bd_info->bi_ctx, MIC_IRQ_DB2, "Host DoorBell 2",
				 vhost_doorbell_intr_handler);

_exit_:
	return ret;
}

void
mic_vhost_blk_stop(bd_info_t *bd_info)
{
	struct vring *vring = &((struct mic_virtblk *)bd_info->bi_virtio)->vb_shared.vring;

	writel(0, &vring->num);  /* reject subsequent request from MIC card */
}

extern bd_info_t *dev_to_bdi(struct device *dev);

ssize_t
show_virtblk_file(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct board_info *bd_info = dev_to_bdi(dev);
	struct mic_virtblk *mic_virtblk;
	struct vhost_blk *vblk;

	BUG_ON(bd_info == NULL);
	mic_virtblk = bd_info->bi_virtio;
	BUG_ON(mic_virtblk == NULL);
	vblk = mic_virtblk->vblk;
	BUG_ON(vblk == NULL);

	if (vblk->file_name != NULL)
		return snprintf(buf, PAGE_SIZE, "%s\n", vblk->file_name);
	else
		return 0;
}

ssize_t
store_virtblk_file(struct device *dev, struct device_attribute *attr,
				   const char *buf, size_t count)
{
	int ret = 0;
	struct board_info *bd_info = dev_to_bdi(dev);
	struct mic_virtblk *mic_virtblk;
	struct vhost_blk *vblk;
	struct vhost_virtqueue *vq;
	char *p;
	struct file *virtblk_file;

	BUG_ON(bd_info == NULL);
	mic_virtblk = bd_info->bi_virtio;
	BUG_ON(mic_virtblk == NULL);
	vblk = mic_virtblk->vblk;
	BUG_ON(vblk == NULL);
	vq = &vblk->vqs[0];
	BUG_ON(vq == NULL);

	if (buf == NULL) {
		ret = -EINVAL;
		goto _return_;
	}
	if (count <= 1) {
		ret = -EINVAL;
		goto _return_;
	}

	p = strchr(buf, '\n');
	if (p != NULL)
		*p = '\0';

	mutex_lock(&vq->mutex);
	if (vblk->virtblk_file != NULL) {  /* if virtblk file is already assigned */
		printk(KERN_ALERT "you are changing virtblk file: %s -> %s.\n", vblk->file_name, buf);
		kfree(vblk->file_name);
		vblk->file_name = NULL;
		filp_close(vblk->virtblk_file, current->files);
		vblk->virtblk_file = NULL;
	}

	vblk->file_name = kmalloc(count + 1, GFP_KERNEL);
	strcpy(vblk->file_name, buf);
	virtblk_file = filp_open(vblk->file_name, O_RDWR|O_LARGEFILE, 0);
	if (IS_ERR(virtblk_file)) {
		ret = PTR_ERR(virtblk_file);
		mutex_unlock(&vq->mutex);
		goto free_file_name;
	}
	vblk->virtblk_file = virtblk_file;
	mutex_unlock(&vq->mutex);

	ret = vhost_blk_set_backend(vblk);
	if (ret < 0)
		goto close_virtblk_file;

	return count;

 close_virtblk_file:
	filp_close(vblk->virtblk_file, current->files);
 free_file_name:
	kfree(vblk->file_name);
 _return_:
	return ret;
}

int mic_vhost_blk_probe(bd_info_t *bd_info)
{
	int ret = 0;
	char wq_name[8];
	struct mic_virtblk *mic_virtblk;
	struct vhost_blk *vblk;

	mic_virtblk = kzalloc(sizeof(*mic_virtblk), GFP_KERNEL);
	if (mic_virtblk == NULL) {
		ret = -ENOMEM;
		goto err_vblk;
	}
	bd_info->bi_virtio = mic_virtblk;

	vblk = kzalloc(sizeof *vblk, GFP_KERNEL);
	if (vblk == NULL) {
		ret = -ENOMEM;
		goto free_mic_virtblk;
	}
	mic_virtblk->vblk = vblk;
	vblk->bd_info = bd_info;

	ret = vhost_dev_init(&vblk->dev, vblk->vqs, VHOST_BLK_VQ_MAX);
	if (ret < 0)
		goto free_vblk;

#if 0
	vhost_poll_init(vblk->poll, handle_rq_blk, POLLOUT|POLLIN, &vblk->dev);
#endif

	BUG_ON(bd_info->bi_ctx.bi_id >= 1000);
	snprintf(wq_name, ARRAY_SIZE(wq_name), "vblk%03d", bd_info->bi_ctx.bi_id);
	vblk->vblk_workqueue = __mic_create_singlethread_workqueue(wq_name);
	if (vblk->vblk_workqueue == NULL) {
		ret = -ENOMEM;
		goto free_vblk;
	}

	return ret;

 free_vblk:
	kfree(vblk);
 free_mic_virtblk:
	kfree(mic_virtblk);
 err_vblk:
	return ret;
}

void mic_vhost_blk_remove(bd_info_t *bd_info)
{
	struct mic_virtblk *mic_virtblk = bd_info->bi_virtio;
	struct vhost_blk *vblk = mic_virtblk->vblk;
	struct vb_shared *vb_shared = &mic_virtblk->vb_shared;

	if (vblk->virtblk_file != NULL) {
		mic_unreg_irqhandler(&bd_info->bi_ctx, MIC_IRQ_DB2, "Host DoorBell 2");
		memset(&vb_shared->blk_config, 0, sizeof(vb_shared->blk_config));
		destroy_workqueue(vblk->vb_wq);
		if (vblk->vqs[0].private_data != NULL)
			fput(vblk->vqs[0].private_data);
		kfree(vblk->file_name);
		filp_close(vblk->virtblk_file, current->files);
	}
	vhost_dev_cleanup(&vblk->dev);
	destroy_workqueue(vblk->vblk_workqueue);
	kfree(vblk);
	kfree(mic_virtblk);
}
#endif
