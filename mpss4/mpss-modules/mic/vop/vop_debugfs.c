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
#include <linux/version.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

#include "vop.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 19, 0)
#define vringh16_to_cpu(vrh, n) (n)
#define vringh32_to_cpu(vrh, n) (n)
#endif

/* Debugfs parent dir */
static struct dentry *vop_dbg;

static int vop_dp_show(struct seq_file *s, void *pos)
{
	struct mic_device_desc *d;
	struct mic_device_ctrl *dc;
	struct mic_vqconfig *vqconfig;
	__u32 *features;
	__u8 *config;
	struct vop_info *vi = s->private;
	struct vop_device *vpdev = vi->vpdev;
	struct mic_bootparam *bootparam;
	int j, k;

	if (vpdev->dnode)
		bootparam = vpdev->hw_ops->get_dp(vpdev);
	else
		bootparam = vpdev->hw_ops->get_remote_dp(vpdev);

	seq_printf(s, "bootparam:\n");
	seq_printf(s, "\t" "magic %#x\n",
		   bootparam->magic);
	seq_printf(s, "\t" "h2c_config_db %d\n",
		   bootparam->h2c_config_db);
	seq_printf(s, "\t" "node_id %d\n",
		   bootparam->node_id);
	seq_printf(s, "\t" "c2h_scif_db %d\n",
		   bootparam->c2h_scif_db);
	seq_printf(s, "\t" "h2c_scif_db %d\n",
		   bootparam->h2c_scif_db);
	seq_printf(s, "\t" "scif_host_dma_addr %#llx\n",
		   bootparam->scif_host_dma_addr);
	seq_printf(s, "\t" "scif_card_dma_addr %#llx\n",
		   bootparam->scif_card_dma_addr);

	for (j = sizeof(*bootparam);
		j < MIC_DP_SIZE; j += mic_total_desc_size(d)) {
		d = (void *)bootparam + j;
		dc = (void *)d + mic_aligned_desc_size(d);

		/* end of list */
		if (d->type == 0)
			break;

		if (d->type == -1)
			continue;
		seq_printf(s, "device_desc %d\n", j / mic_total_desc_size(d));
		seq_printf(s, "\t" "type        %d\n", d->type);
		seq_printf(s, "\t" "num_vq      %d\n", d->num_vq);
		seq_printf(s, "\t" "feature_len %d\n", d->feature_len);
		seq_printf(s, "\t" "config_len  %d\n", d->config_len);
		seq_printf(s, "\t" "status      %d\n", d->status);

		for (k = 0; k < d->num_vq; k++) {
			vqconfig = mic_vq_config(d) + k;
			seq_printf(s, "\t" "vqconfig[%3d]:\n", k);
			seq_printf(s, "\t\t" "address      %#llx\n",
				   vqconfig->address);
			seq_printf(s, "\t\t" "num          %d\n",
				   vqconfig->num);
			seq_printf(s, "\t\t" "used_address %#llx\n",
				   vqconfig->used_address);
		}

		features = (__u32 *)mic_vq_features(d);
		seq_printf(s, "\t" "features:\n");
		seq_printf(s, "\t\t" "host  %#x\n", features[0]);
		seq_printf(s, "\t\t" "guest %#x\n", features[1]);

		config = mic_vq_configspace(d);
		seq_printf(s, "\t" "config:\n");
		for (k = 0; k < d->config_len; k++)
			seq_printf(s, "\t" "\t" "[%3d] = %d\n", k, config[k]);

		seq_puts(s, "\t" "device control:\n");
		seq_printf(s, "\t\t" "config_change        %d\n",
			   dc->config_change);
		seq_printf(s, "\t\t" "vdev_reset           %d\n",
			   dc->vdev_reset);
		seq_printf(s, "\t\t" "guest_ack            %d\n",
			   dc->guest_ack);
		seq_printf(s, "\t\t" "host_ack             %d\n",
			   dc->host_ack);
		seq_printf(s, "\t\t" "used_address_updated %d\n",
			   dc->used_address_updated);
		seq_printf(s, "\t\t" "vdev                 %#llx\n",
			   dc->vdev);
		seq_printf(s, "\t\t" "c2h_vdev_db          %d\n",
			   dc->c2h_vdev_db);
		seq_printf(s, "\t\t" "h2c_vdev_db          %d\n",
			   dc->h2c_vdev_db);
	}
	return 0;
}

static int vop_dp_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, vop_dp_show, inode->i_private);
}

static const struct file_operations dp_ops = {
	.owner   = THIS_MODULE,
	.open    = vop_dp_debug_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release
};

static int vop_vdev_info_show(struct seq_file *s, void *unused)
{
	struct vop_info *vi = s->private;
	struct list_head *pos, *tmp;
	struct vop_vdev *vdev;
	int i, j;

	mutex_lock(&vop_mutex);
	list_for_each_safe(pos, tmp, &vi->vdev_list) {
		vdev = list_entry(pos, struct vop_vdev, list);

		seq_printf(s, "VDEV \n\ttype %d\n\tstate %s\n\tin_bytes %ld\n"
			   "\tout_bytes %ld\n\tin_bytes_dma %ld\n"
			   "\tout_bytes_dma %ld\n",
			   vdev->virtio_id, vop_vdevup(vdev) ? "UP" : "DOWN",
			   vdev->in_bytes, vdev->out_bytes,
			   vdev->in_bytes_dma, vdev->out_bytes_dma);

		for (i = 0; i < MIC_MAX_VRINGS; i++) {
			struct vring_desc *desc;
			struct vring_avail *avail;
			struct vring_used *used;
			struct vop_vringh *vvr = &vdev->vvr[i];
			struct vringh *vrh = &vvr->vrh;
			int num = vrh->vring.num;

			if (!num)
				continue;

			desc = vrh->vring.desc;
			seq_printf(s, "\t" "vring %d, avail_idx %d,",
				   i, vvr->vring.info->avail_idx & (num - 1));
			seq_printf(s, " vring %d, avail_idx %d\n",
				   i, vvr->vring.info->avail_idx);

			seq_printf(s, "\t" "vrh   %d, weak_barriers %d,",
				   i, vrh->weak_barriers);
			seq_printf(s, " last_avail_idx %d, last_used_idx %d,",
				   vrh->last_avail_idx, vrh->last_used_idx);
			seq_printf(s, " completed %d\n", vrh->completed);

			seq_printf(s, "\t" "desc:\n");
			for (j = 0; j < num; j++) {
				seq_printf(s, "\t\t" "[%3d] addr %#18llx, len %10d,",
					   j, desc->addr, desc->len);
				seq_printf(s, " flags %#5x, next %d\n",
					   desc->flags, desc->next);
				desc++;
			}

			avail = vrh->vring.avail;
			seq_printf(s, "\t" "avail flags %#x, idx %d\n",
				   vringh16_to_cpu(vrh, avail->flags),
				   vringh16_to_cpu(vrh, avail->idx) & (num - 1));
			seq_printf(s, "\t" "avail flags %#x, idx %d\n",
				   vringh16_to_cpu(vrh, avail->flags),
				   vringh16_to_cpu(vrh, avail->idx));

			seq_printf(s, "\t" "avail_ring:\n");
			for (j = 0; j < num; j++)
				seq_printf(s, "\t\t" "[%3d] %d\n",
					   j, avail->ring[j]);
			used = vrh->vring.used;
			seq_printf(s, "\t" "used flags %#x, idx %d\n",
				   vringh16_to_cpu(vrh, used->flags),
				   vringh16_to_cpu(vrh, used->idx) & (num - 1));
			seq_printf(s, "\t" "used flags %#x, idx %d\n",
				   vringh16_to_cpu(vrh, used->flags),
				   vringh16_to_cpu(vrh, used->idx));

			seq_printf(s, "\t" "used ring:\n");
			for (j = 0; j < num; j++)
				seq_printf(s, "\t\t" "[%3d] id %4d, len %d\n",
					   j, vringh32_to_cpu(vrh,
							      used->ring[j].id),
					   vringh32_to_cpu(vrh,
							   used->ring[j].len));
		}
	}
	mutex_unlock(&vop_mutex);

	return 0;
}

static int vop_vdev_info_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, vop_vdev_info_show, inode->i_private);
}

static const struct file_operations vdev_info_ops = {
	.owner   = THIS_MODULE,
	.open    = vop_vdev_info_debug_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release
};

void vop_create_debug_dir(struct vop_info *vi)
{
	char name[16];

	if (!vop_dbg)
		return;

	scnprintf(name, sizeof(name), "mic%d", vi->vpdev->dnode - 1);
	vi->dbg = debugfs_create_dir(name, vop_dbg);
	if (!vi->dbg) {
		log_mic_err(vop_get_id(vi->vpdev),
			    "can't create debugfs dir vop");
		return;
	}
	debugfs_create_file("dp", 0444, vi->dbg, vi, &dp_ops);
	if (vi->vpdev->dnode)
		debugfs_create_file("vdev_info", 0444, vi->dbg,
				    vi, &vdev_info_ops);
}

void vop_delete_debug_dir(struct vop_info *vi)
{
	debugfs_remove_recursive(vi->dbg);
}

/**
 * vop_init_debugfs - Initialize global debugfs entry.
 */
void __init vop_init_debugfs(void)
{
	vop_dbg = debugfs_create_dir(KBUILD_MODNAME, NULL);
	if (!vop_dbg)
		pr_err("can't create debugfs dir\n");
}

/**
 * vop_exit_debugfs - Uninitialize global debugfs entry
 */
void vop_exit_debugfs(void)
{
	debugfs_remove(vop_dbg);
}
