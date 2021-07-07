/*
  virtio block device adapted for MIC.
  copied from drivers/block/virtio_blk.c of Linux kernel
  It is initially commited by
  Rusty Russell <rusty@rustcorp.com.au>  2007-10-21 18:03:38
  with SHA1 ID, e467cde238184d1b0923db2cd61ae1c5a6dc15aa

  drivers/block/virtio_blk.c of Linux kernel does not have copyright notice.

 * For adapting to MIC
 * (C) Copyright 2012 Intel Corporation
 * Author: Caz Yokoyama <Caz.Yokoyama@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * The full GNU General Public License is included in this distribution in
 * the file called "COPYING".
 */
//#define DEBUG
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/blkdev.h>
#include <linux/hdreg.h>
#include <linux/virtio.h>
#include <linux/virtio_ring.h>
#include <linux/virtio_blk.h>
#include <linux/scatterlist.h>
#include <linux/list.h>
#include "mic_common.h"
#include "mic/micveth_dma.h"
#include "mic/micscif_intr.h"
#include "mic/mic_virtio.h"

#define SBOX_MMIO_LENGTH	(64 * 1024)

#define PART_BITS 4

#define VIRTQUEUE_LENGTH 128
#define MIC_VRING_ALIGN PAGE_SIZE

#define INTERRUPT_ID_FOR_VIRTBLK 3

extern int get_sbox_irq(int index);

static int major, index = 0;
static long virtio_addr = 0;
static mic_data_t virtblk_mic_data;

struct virtio_blk
{
	spinlock_t lock;

	struct virtio_device *vdev;
	struct virtqueue *vq;

	/* The disk structure for the kernel. */
	struct gendisk *disk;

	/* Request tracking. */
	struct list_head reqs;

	mempool_t *pool;

	/* virtual address of blk_config */
	void __iomem *ioaddr;

	/* What host tells us, plus 2 for header & tailer. */
	unsigned int sg_elems;

	/* sbox va */
	u8 *sbox;

	/* Scatterlist: can be too big for stack. */
	struct scatterlist sg[/*sg_elems*/];
};

struct virtblk_req
{
	struct list_head list;
	struct request *req;
	struct virtio_blk_outhdr out_hdr;
	struct virtio_scsi_inhdr in_hdr;
	u8 status;
};

#define blk_pc_request(rq)     ((rq)->cmd_type == REQ_TYPE_BLOCK_PC)

/* The following vring_virtqueue and to_vvq() are copied from virtio_ring.c. Please name sure you have the same structure
   as in virtio_ring.c. The reason why they are copied is that I don't want to change virtio_ring.c which is a symbolic link.
*/
struct vring_virtqueue
{
	struct virtqueue vq;

	/* Actual memory layout for this queue */
	struct vring vring;

	/* Other side has made a mess, don't try any more. */
	bool broken;

	/* Host supports indirect buffers */
	bool indirect;

	/* Number of free buffers */
	unsigned int num_free;
	/* Head of free buffer list. */
	unsigned int free_head;
	/* Number we've added since last sync. */
	unsigned int num_added;

	/* Last used index we've seen. */
	u16 last_used_idx;

	/* How to notify other side. FIXME: commonalize hcalls! */
	void (*notify)(struct virtqueue *vq);

#ifdef DEBUG
	/* They're supposed to lock for us. */
	unsigned int in_use;
#endif

	struct _mic_ctx_t *mic_ctx;
	/* Tokens for callbacks. */
	void *data[];
};

#define to_vvq(_vq) container_of(_vq, struct vring_virtqueue, vq)

static void blk_done(struct virtqueue *vq)
{
	struct virtio_blk *vblk = vq->vdev->priv;
	struct virtblk_req *vbr;
	unsigned int len;
	unsigned long flags;

	spin_lock_irqsave(&vblk->lock, flags);
	while ((vbr = virtqueue_get_buf(vblk->vq, &len)) != NULL) {
		int error;

		switch (vbr->status) {
		case VIRTIO_BLK_S_OK:
			error = 0;
			break;
		case VIRTIO_BLK_S_UNSUPP:
			error = -ENOTTY;
			break;
		default:
			error = -EIO;
			break;
		}

		if (blk_pc_request(vbr->req)) {
			vbr->req->resid_len = vbr->in_hdr.residual;
			vbr->req->sense_len = vbr->in_hdr.sense_len;
			vbr->req->errors = vbr->in_hdr.errors;
		}

		__blk_end_request_all(vbr->req, error);
		list_del(&vbr->list);
		mempool_free(vbr, vblk->pool);
	}
	/* In case queue is stopped waiting for more buffers. */
	blk_start_queue(vblk->disk->queue);
	spin_unlock_irqrestore(&vblk->lock, flags);
}

static bool do_req(struct request_queue *q, struct virtio_blk *vblk,
		   struct request *req)
{
	unsigned long num, out = 0, in = 0;
	struct virtblk_req *vbr;

	vbr = mempool_alloc(vblk->pool, GFP_ATOMIC);
	if (!vbr)
		/* When another request finishes we'll try again. */
		return false;

	vbr->req = req;

	if (req->cmd_flags & REQ_FLUSH) {
		vbr->out_hdr.type = VIRTIO_BLK_T_FLUSH;
		vbr->out_hdr.sector = 0;
		vbr->out_hdr.ioprio = req_get_ioprio(vbr->req);
	} else {
		switch (req->cmd_type) {
		case REQ_TYPE_FS:
			vbr->out_hdr.type = 0;
			vbr->out_hdr.sector = blk_rq_pos(vbr->req);
			vbr->out_hdr.ioprio = req_get_ioprio(vbr->req);
			break;
		case REQ_TYPE_BLOCK_PC:
			vbr->out_hdr.type = VIRTIO_BLK_T_SCSI_CMD;
			vbr->out_hdr.sector = 0;
			vbr->out_hdr.ioprio = req_get_ioprio(vbr->req);
			break;
		case REQ_TYPE_SPECIAL:
			vbr->out_hdr.type = VIRTIO_BLK_T_GET_ID;
			vbr->out_hdr.sector = 0;
			vbr->out_hdr.ioprio = req_get_ioprio(vbr->req);
			break;
		default:
			/* We don't put anything else in the queue. */
			BUG();
		}
	}

	sg_set_buf(&vblk->sg[out++], &vbr->out_hdr, sizeof(vbr->out_hdr));

	/*
	 * If this is a packet command we need a couple of additional headers.
	 * Behind the normal outhdr we put a segment with the scsi command
	 * block, and before the normal inhdr we put the sense data and the
	 * inhdr with additional status information before the normal inhdr.
	 */
	if (blk_pc_request(vbr->req))
		sg_set_buf(&vblk->sg[out++], vbr->req->cmd, vbr->req->cmd_len);

	num = blk_rq_map_sg(q, vbr->req, vblk->sg + out);

	if (blk_pc_request(vbr->req)) {
		sg_set_buf(&vblk->sg[num + out + in++], vbr->req->sense, 96);
		sg_set_buf(&vblk->sg[num + out + in++], &vbr->in_hdr,
			   sizeof(vbr->in_hdr));
	}

	sg_set_buf(&vblk->sg[num + out + in++], &vbr->status,
		   sizeof(vbr->status));

	if (num) {
		if (rq_data_dir(vbr->req) == WRITE) {
			vbr->out_hdr.type |= VIRTIO_BLK_T_OUT;
			out += num;
		} else {
			vbr->out_hdr.type |= VIRTIO_BLK_T_IN;
			in += num;
		}
	}

	if (virtqueue_add_buf(vblk->vq, vblk->sg, out, in, vbr) < 0) {
		mempool_free(vbr, vblk->pool);
		return false;
	}

	list_add_tail(&vbr->list, &vblk->reqs);
	return true;
}

static void do_virtblk_request(struct request_queue *q)
{
	struct virtio_blk *vblk = q->queuedata;
	struct request *req;
	unsigned int issued = 0;

	while ((req = blk_peek_request(q)) != NULL) {
		BUG_ON(req->nr_phys_segments + 2 > vblk->sg_elems);

		/* If this request fails, stop queue and wait for something to
		   finish to restart it. */
		if (!do_req(q, vblk, req)) {
			blk_stop_queue(q);
			break;
		}
		blk_start_request(req);
		issued++;
	}

	if (issued)
		virtqueue_kick(vblk->vq);
}

static int
set_capacity_from_host(struct virtio_blk *vblk)
{
	struct virtio_device *vdev = vblk->vdev;
	u64 cap;

	/* Host must always specify the capacity. */
	vdev->config->get(vdev, offsetof(struct virtio_blk_config, capacity),
			  &cap, sizeof(cap));
	if (cap == 0) {
		printk(KERN_ERR "Have you set virtblk file?\n");
		return -ENXIO;
	}

	/* If capacity is too big, truncate with warning. */
	if ((sector_t)cap != cap) {
		dev_warn(&vdev->dev, "Capacity %llu too large: truncating\n",
			 (unsigned long long)cap);
		cap = (sector_t)-1;
	}
	set_capacity(vblk->disk, cap);

	return 0;
}

static int
virtblk_open(struct block_device *bdev, fmode_t mode)
{
	struct gendisk *disk = bdev->bd_disk;
	struct virtio_blk *vblk = disk->private_data;

	return set_capacity_from_host(vblk);
}

static int virtblk_ioctl(struct block_device *bdev, fmode_t mode,
			 unsigned cmd, unsigned long data)
{
	struct gendisk *disk = bdev->bd_disk;
	struct virtio_blk *vblk = disk->private_data;

	/*
	 * Only allow the generic SCSI ioctls if the host can support it.
	 */
	if (!virtio_has_feature(vblk->vdev, VIRTIO_BLK_F_SCSI))
		return -ENOTTY;

	return scsi_cmd_ioctl(disk->queue, disk, mode, cmd,
			      (void __user *)data);
}

/* We provide getgeo only to please some old bootloader/partitioning tools */
static int virtblk_getgeo(struct block_device *bd, struct hd_geometry *geo)
{
	struct virtio_blk *vblk = bd->bd_disk->private_data;
	struct virtio_blk_geometry vgeo;
	int err;

	/* see if the host passed in geometry config */
	err = virtio_config_val(vblk->vdev, VIRTIO_BLK_F_GEOMETRY,
				offsetof(struct virtio_blk_config, geometry),
				&vgeo);

	if (!err) {
		geo->heads = vgeo.heads;
		geo->sectors = vgeo.sectors;
		geo->cylinders = vgeo.cylinders;
	} else {
		/* some standard values, similar to sd */
		geo->heads = 1 << 6;
		geo->sectors = 1 << 5;
		geo->cylinders = get_capacity(bd->bd_disk) >> 11;
	}
	return 0;
}

static const struct block_device_operations virtblk_fops = {
	.open = virtblk_open,
	.ioctl = virtblk_ioctl,
	.owner  = THIS_MODULE,
	.getgeo = virtblk_getgeo,
};

static int index_to_minor(int index)
{
	return index << PART_BITS;
}
 
static inline bool more_used(const struct vring_virtqueue *vq)
{
  return vq->last_used_idx != vq->vring.used->idx;
}

static irqreturn_t
mic_virtblk_intr_handler(int irq, void *_vq)
{
	struct vring_virtqueue *vq = to_vvq(_vq);

	if (!more_used(vq)) {
		pr_debug("virtqueue interrupt with no work for %p\n", vq);
		goto _exit_;
	}

	if (unlikely(vq->broken))
		goto _exit_;

	pr_debug("virtqueue callback for %p (%p)\n", vq, vq->vq.callback);
	if (vq->vq.callback)
		vq->vq.callback(&vq->vq);

 _exit_:
	return IRQ_HANDLED;
}

static int __devinit virtblk_probe(struct virtio_device *vdev)
{
	struct virtio_blk *vblk;
	struct request_queue *q;
	int err;
	u32 v, blk_size, sg_elems, opt_io_size;
	u16 min_io_size;
	u8 physical_block_exp, alignment_offset;
	struct board_info *bd_info = virtblk_mic_data.dd_bi[0];
	struct vb_shared *vb_shared;

	if (index_to_minor(index) >= 1 << MINORBITS)
		return -ENOSPC;

	vb_shared = ((struct mic_virtblk *)bd_info->bi_virtio)->vb_shared;
	vdev->features[0] = readl(&vb_shared->host_features);

	/* We need to know how many segments before we allocate. */
	err = virtio_config_val(vdev, VIRTIO_BLK_F_SEG_MAX,
				offsetof(struct virtio_blk_config, seg_max),
				&sg_elems);
	if (err)
		sg_elems = 1;

	/* We need an extra sg elements at head and tail. */
	sg_elems += 2;
	vdev->priv = vblk = kmalloc(sizeof(*vblk) +
				    sizeof(vblk->sg[0]) * sg_elems, GFP_KERNEL);
	if (!vblk) {
		err = -ENOMEM;
		goto out;
	}

	INIT_LIST_HEAD(&vblk->reqs);
	spin_lock_init(&vblk->lock);
	vblk->vdev = vdev;
	vblk->sg_elems = sg_elems;
	sg_init_table(vblk->sg, vblk->sg_elems);

	/* map sbox */
	vblk->sbox = ioremap_nocache(SBOX_BASE, SBOX_MMIO_LENGTH);
	if (!vblk->sbox) {
		printk(KERN_ERR "%s: NULL SBOX ptr\n", __func__);
		err = -ENOMEM;
		goto out_free_vblk;
	}

	/* We expect one virtqueue, for output. */
	vblk->vq = virtio_find_single_vq(vdev, blk_done, "requests");
	if (IS_ERR(vblk->vq)) {
		err = PTR_ERR(vblk->vq);
		goto out_unmap_sbox;
	}

	if ((err = request_irq(get_sbox_irq(VIRTIO_SBOX_INT_IDX),
						   mic_virtblk_intr_handler, IRQF_DISABLED,
						   "virtio intr", vblk->vq))) {
		printk(KERN_ERR "%s: can't register interrupt: %d\n", __func__, err);
		goto out_free_vq;
	}

	vblk->pool = mempool_create_kmalloc_pool(1,sizeof(struct virtblk_req));
	if (!vblk->pool) {
		err = -ENOMEM;
		goto out_free_irq;
	}

	/* FIXME: How many partitions?  How long is a piece of string? */
	vblk->disk = alloc_disk(1 << PART_BITS);
	if (!vblk->disk) {
		err = -ENOMEM;
		goto out_mempool;
	}

	q = vblk->disk->queue = blk_init_queue(do_virtblk_request, &vblk->lock);
	if (!q) {
		err = -ENOMEM;
		goto out_put_disk;
	}

	q->queuedata = vblk;

	if (index < 26) {
		sprintf(vblk->disk->disk_name, "vd%c", 'a' + index % 26);
	} else if (index < (26 + 1) * 26) {
		sprintf(vblk->disk->disk_name, "vd%c%c",
			'a' + index / 26 - 1, 'a' + index % 26);
	} else {
		const unsigned int m1 = (index / 26 - 1) / 26 - 1;
		const unsigned int m2 = (index / 26 - 1) % 26;
		const unsigned int m3 =  index % 26;
		sprintf(vblk->disk->disk_name, "vd%c%c%c",
			'a' + m1, 'a' + m2, 'a' + m3);
	}

	vblk->disk->major = major;
	vblk->disk->first_minor = index_to_minor(index);
	vblk->disk->private_data = vblk;
	vblk->disk->fops = &virtblk_fops;
	vblk->disk->driverfs_dev = NULL;  // There is no parent device.
	index++;

	/* configure queue flush support */
	if (virtio_has_feature(vdev, VIRTIO_BLK_F_FLUSH))
		blk_queue_flush(q, REQ_FLUSH);

	/* If disk is read-only in the host, the guest should obey */
	if (virtio_has_feature(vdev, VIRTIO_BLK_F_RO)) {
	  if (vdev->config->get_features(vdev) & (1U << VIRTIO_BLK_F_RO)) {
		set_disk_ro(vblk->disk, 1);
	  }
	}

	err = set_capacity_from_host(vblk);
	if (err)
		goto out_put_disk;

	/* We can handle whatever the host told us to handle. */
	blk_queue_max_segments(q, vblk->sg_elems-2);

	/* No need to bounce any requests */
	blk_queue_bounce_limit(q, BLK_BOUNCE_ANY);

	/* No real sector limit. */
	blk_queue_max_hw_sectors(q, -1U);

	/* Host can optionally specify maximum segment size and number of
	 * segments. */
	err = virtio_config_val(vdev, VIRTIO_BLK_F_SIZE_MAX,
				offsetof(struct virtio_blk_config, size_max),
				&v);
	if (!err)
		blk_queue_max_segment_size(q, v);
	else
		blk_queue_max_segment_size(q, -1U);

	/* Host can optionally specify the block size of the device */
	err = virtio_config_val(vdev, VIRTIO_BLK_F_BLK_SIZE,
				offsetof(struct virtio_blk_config, blk_size),
				&blk_size);
	if (!err)
		blk_queue_logical_block_size(q, blk_size);
	else
		blk_size = queue_logical_block_size(q);

	/* Use topology information if available */
	err = virtio_config_val(vdev, VIRTIO_BLK_F_TOPOLOGY,
			offsetof(struct virtio_blk_config, physical_block_exp),
			&physical_block_exp);
	if (!err && physical_block_exp)
		blk_queue_physical_block_size(q,
				blk_size * (1 << physical_block_exp));

	err = virtio_config_val(vdev, VIRTIO_BLK_F_TOPOLOGY,
			offsetof(struct virtio_blk_config, alignment_offset),
			&alignment_offset);
	if (!err && alignment_offset)
		blk_queue_alignment_offset(q, blk_size * alignment_offset);

	err = virtio_config_val(vdev, VIRTIO_BLK_F_TOPOLOGY,
			offsetof(struct virtio_blk_config, min_io_size),
			&min_io_size);
	if (!err && min_io_size)
		blk_queue_io_min(q, blk_size * min_io_size);

	err = virtio_config_val(vdev, VIRTIO_BLK_F_TOPOLOGY,
			offsetof(struct virtio_blk_config, opt_io_size),
			&opt_io_size);
	if (!err && opt_io_size)
		blk_queue_io_opt(q, blk_size * opt_io_size);

	add_disk(vblk->disk);
	return 0;

out_put_disk:
	put_disk(vblk->disk);
out_mempool:
	mempool_destroy(vblk->pool);
out_free_irq:
	free_irq(get_sbox_irq(VIRTIO_SBOX_INT_IDX), vblk->vq);
out_free_vq:
	vdev->config->del_vqs(vdev);
out_unmap_sbox:
	iounmap(vblk->sbox);
out_free_vblk:
	kfree(vblk);
out:
	return err;
}

static void __devexit virtblk_remove(struct virtio_device *vdev)
{
	struct virtio_blk *vblk = vdev->priv;

	/* Nothing should be pending. */
	BUG_ON(!list_empty(&vblk->reqs));

	free_irq(get_sbox_irq(VIRTIO_SBOX_INT_IDX), vblk->vq);

	/* Stop all the virtqueues. */
	vdev->config->reset(vdev);

	del_gendisk(vblk->disk);
	blk_cleanup_queue(vblk->disk->queue);
	put_disk(vblk->disk);
	mempool_destroy(vblk->pool);
	vdev->config->del_vqs(vdev);
	iounmap(vblk->sbox);
	kfree(vblk);
}

/* config->get_features() implementation */
static u32 virtblk_get_features(struct virtio_device *vdev)
{
	/* When someone needs more than 32 feature bits, we'll need to
	 * steal a bit to indicate that the rest are somewhere else. */
	struct board_info *bd_info = virtblk_mic_data.dd_bi[0];
	struct vb_shared *vb_shared;

	vb_shared = ((struct mic_virtblk *)bd_info->bi_virtio)->vb_shared;
	return readl(&vb_shared->host_features);
}

/* virtio config->finalize_features() implementation */
static void virtblk_finalize_features(struct virtio_device *vdev)
{
	struct board_info *bd_info = virtblk_mic_data.dd_bi[0];
	struct vb_shared *vb_shared;

	/* Give virtio_ring a chance to accept features. */
	vring_transport_features(vdev);

	/* We only support 32 feature bits. */
	BUILD_BUG_ON(ARRAY_SIZE(vdev->features) != 1);

	vb_shared = ((struct mic_virtblk *)bd_info->bi_virtio)->vb_shared;
	writel(vdev->features[0], &vb_shared->client_features);
}

/* config->get() implementation */
static void virtblk_get(struct virtio_device *vdev, unsigned offset,
		   void *buf, unsigned len)
{
	struct board_info *bd_info = virtblk_mic_data.dd_bi[0];
	struct vb_shared *vb_shared;
	void *ioaddr;
	u8 *ptr = buf;
	int i;

	vb_shared = ((struct mic_virtblk *)bd_info->bi_virtio)->vb_shared;
	ioaddr = (void *)&vb_shared->blk_config + offset;
	for (i = 0; i < len; i++)
		ptr[i] = readb(ioaddr + i);
}

static void virtblk_reset(struct virtio_device *vdev)
{
}

/* the notify function used when creating a virt queue */
static void virtblk_notify(struct virtqueue *vq)
{
	const int doorbell = 2;
	struct virtio_blk *vblk = vq->vdev->priv;
	uint32_t db_reg;

	/* Ring host doorbell interrupt */
	db_reg = readl(vblk->sbox + (SBOX_SDBIC0 + (4 * doorbell)))
			| SBOX_SDBIC0_DBREQ_BIT;
	writel(db_reg, vblk->sbox + (SBOX_SDBIC0 + (4 * doorbell)));
}

/* the config->del_vqs() implementation */
static void virtblk_del_vqs(struct virtio_device *vdev)
{
	struct virtio_blk *vblk = vdev->priv;
	unsigned long size;

	size = PAGE_ALIGN(vring_size(VIRTQUEUE_LENGTH, MIC_VRING_ALIGN));
	free_pages_exact(vblk->vq->priv, size);

	vring_del_virtqueue(vblk->vq);
	vblk->vq = NULL;
}

/* the config->find_vqs() implementation */
static int virtblk_find_vqs(struct virtio_device *vdev, unsigned nvqs,
		       struct virtqueue *vqs[],
		       vq_callback_t *callbacks[],
		       const char *names[])
{
	struct virtio_blk *vblk = vdev->priv;
	struct virtqueue *vq;
	int err;
	unsigned long size;
	void *queue;  /* the virtual address of the ring queue */
	struct vring_virtqueue *vvq;
	struct vring *vring;
	struct board_info *bd_info = virtblk_mic_data.dd_bi[0];

	BUG_ON(nvqs != 1);
	BUG_ON(vblk == NULL);

	size = PAGE_ALIGN(vring_size(VIRTQUEUE_LENGTH, MIC_VRING_ALIGN));
	queue = alloc_pages_exact(size, GFP_KERNEL|__GFP_ZERO);
	if (queue == NULL) {
		err = -ENOMEM;
		goto out_info;
	}

	/* create the vring */
	vq = vring_new_virtqueue(VIRTQUEUE_LENGTH, MIC_VRING_ALIGN,
							 vdev, queue, virtblk_notify, callbacks[0], names[0]);
	if (vq == NULL) {
		err = -ENOMEM;
		goto out_activate_queue;
	}
	vq->priv = queue;

	vqs[0] = vblk->vq = vq;

	vvq = to_vvq(vq);
	vring = &((struct mic_virtblk *)bd_info->bi_virtio)->vb_shared->vring;
	writel(vvq->vring.num, &vring->num);
	writeq(virt_to_phys(vvq->vring.desc), &vring->desc);
	writeq(virt_to_phys(vvq->vring.avail), &vring->avail);
	writeq(virt_to_phys(vvq->vring.used), &vring->used);

	return 0;

out_activate_queue:
	free_pages_exact(queue, size);
out_info:
	return err;
}

static struct virtio_config_ops virtio_blk_config_ops = {
	.get		= virtblk_get,
	//	.set		= vp_set,
	//	.get_status	= vp_get_status,
	//	.set_status	= vp_set_status,
	.reset		= virtblk_reset,
	.find_vqs	= virtblk_find_vqs,
	.del_vqs	= virtblk_del_vqs,
	.get_features	= virtblk_get_features,
	.finalize_features = virtblk_finalize_features,
};

static unsigned int features[] = {
	VIRTIO_BLK_F_SEG_MAX, VIRTIO_BLK_F_SIZE_MAX, VIRTIO_BLK_F_GEOMETRY,
	VIRTIO_BLK_F_RO, VIRTIO_BLK_F_BLK_SIZE, VIRTIO_BLK_F_SCSI,
	VIRTIO_BLK_F_FLUSH, VIRTIO_BLK_F_TOPOLOGY
};

/*
 * virtio_blk causes spurious section mismatch warning by
 * simultaneously referring to a __devinit and a __devexit function.
 * Use __refdata to avoid this warning.
 */
static struct virtio_driver __refdata virtio_blk = {
	.feature_table = features,
	.feature_table_size = ARRAY_SIZE(features),
	.driver.name =	KBUILD_MODNAME,
	.driver.owner =	THIS_MODULE,
};

struct class block_class = {
	.name		= "block",
};

static struct device_type disk_type = {
	.name		= "disk",
	/*
	.groups		= disk_attr_groups,
	.release	= disk_release,
	.devnode	= block_devnode,
	*/
};

static int __init init(void)
{
	bd_info_t *bd_info;
	struct virtio_device *vdev;
	struct mic_virtblk *mic_virtblk;
	int ret;
	struct vb_shared *vb_shared;

#ifdef CONFIG_ML1OM
	printk(KERN_ERR "virtio block device is not available on KNF\n");
	ret = -ENODEV;
	goto error_return;
#endif
	major = register_blkdev(0, "virtblk");
	if (major < 0) {
		ret = major;
		goto error_return;
	}

	bd_info = kmalloc(sizeof(bd_info_t), GFP_KERNEL);
	if (bd_info == NULL) {
	  ret = -ENOMEM;
	  goto error_return;
	}
	memset(bd_info, 0, sizeof(*bd_info));
	virtblk_mic_data.dd_numdevs = 1;
	index = 0;
	virtblk_mic_data.dd_bi[0] = bd_info;
	bd_info->bi_ctx.bi_id = 0;

	mic_virtblk = kmalloc(sizeof(*mic_virtblk), GFP_KERNEL);
	if (mic_virtblk == NULL) {
	  ret = -ENOMEM;
	  goto free_bd_info;
	}
	memset(mic_virtblk, 0, sizeof(*mic_virtblk));
	bd_info->bi_virtio = (void *)mic_virtblk;

	if (virtio_addr == 0) {
	  printk(KERN_ERR "virtio address is not passed from host\n");
	  return -ENODEV;
	  goto free_mic_virtblk;
	}
	vb_shared = ioremap_nocache(virtio_addr, sizeof(*vb_shared));
	if (vb_shared == NULL) {
	  ret = -ENODEV;
	  goto free_mic_virtblk;
	}
	vb_shared->update = true;
	mic_virtblk->vb_shared = vb_shared;

	vdev = kmalloc(sizeof(*vdev), GFP_KERNEL);
	if (vdev == NULL) {
	  ret = -ENOMEM;
	  goto free_mic_virtblk;
	}
	memset(vdev, 0, sizeof(*vdev));
	vdev->config = &virtio_blk_config_ops;
	INIT_LIST_HEAD(&vdev->vqs);
	vdev->dev.driver = &virtio_blk.driver;
	vdev->dev.class = &block_class;
	vdev->dev.type = &disk_type;
	device_initialize(&vdev->dev);
	mic_virtblk->vdev = (void *)vdev;

	return virtblk_probe(vdev);

 free_mic_virtblk:
	kfree(bd_info->bi_virtio);
 free_bd_info:
	kfree(bd_info);
 error_return:
	return ret;
}

static void __exit fini(void)
{
	bd_info_t *bd_info = virtblk_mic_data.dd_bi[0];
	struct mic_virtblk *mic_virtblk = (struct mic_virtblk *)bd_info->bi_virtio;

	unregister_blkdev(major, "virtblk");
	virtblk_remove(mic_virtblk->vdev);
	iounmap(mic_virtblk->vb_shared);
	kfree(mic_virtblk->vdev);
	kfree(bd_info->bi_virtio);
	kfree(bd_info);
}
module_init(init);
module_exit(fini);

MODULE_DESCRIPTION("Virtio block driver");
MODULE_LICENSE("GPL");
MODULE_PARM_DESC(virtio_addr, "address of virtio related structure");
module_param(virtio_addr, long, S_IRUGO);
