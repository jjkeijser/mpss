/*
 * Intel MIC Platform Software Stack (MPSS)
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2016 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * BSD LICENSE
 *
 * Copyright(c) 2016 Intel Corporation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Intel SCIF driver.
 */
#include <linux/module.h>
#include <linux/version.h>
#include <linux/idr.h>

#ifdef MIC_IN_KERNEL_BUILD
#include <linux/mic_common.h>
#else
#include "../common/mic_common.h"
#endif
#include "../common/mic_dev.h"
#include "../bus/scif_bus.h"
#include "scif_peer_bus.h"
#include "scif_main.h"
#include "scif_map.h"

#if LINUX_VERSION_CODE > KERNEL_VERSION(4, 2, 0)
#ifdef IOMMU_IOVA
#define HAS_IOMMU_IOVA_CACHE
#endif
#endif

struct scif_info scif_info = {
	.mdev = {
		.minor = MISC_DYNAMIC_MINOR,
		.name = "scif",
		.fops = &scif_fops,
	}
};

struct scif_dev *scif_dev;
struct kmem_cache *unaligned_cache;
static atomic_t g_loopb_cnt;
static DEFINE_MUTEX(g_loopb_mutex);

/* Runs in the context of intr_wq */
static void scif_intr_bh_handler(struct work_struct *work)
{
	struct scif_dev *scifdev =
			container_of(work, struct scif_dev, intr_bh);

	scif_nodeqp_intrhandler(scifdev, scifdev->qpairs);
}

int scif_setup_intr_wq(struct scif_dev *scifdev)
{
	if (!scifdev->intr_wq) {
		scifdev->intr_wq =
			alloc_ordered_workqueue("SCIF INTR %d", 0, scifdev->node);
		if (!scifdev->intr_wq)
			return -ENOMEM;
		INIT_WORK(&scifdev->intr_bh, scif_intr_bh_handler);
	}
	return 0;
}

void scif_destroy_intr_wq(struct scif_dev *scifdev)
{
	if (scifdev->intr_wq) {
		destroy_workqueue(scifdev->intr_wq);
		scifdev->intr_wq = NULL;
	}
}

irqreturn_t scif_intr_handler(int irq, void *data)
{
	struct scif_dev *scifdev = data;
	struct scif_hw_dev *sdev = scifdev->sdev;

	sdev->hw_ops->ack_interrupt(sdev, scifdev->db);
	queue_work(scifdev->intr_wq, &scifdev->intr_bh);
	return IRQ_HANDLED;
}

static void scif_qp_setup_handler(struct work_struct *work)
{
	struct scif_dev *scifdev = container_of(work, struct scif_dev,
						qp_dwork.work);
	struct scif_hw_dev *sdev = scifdev->sdev;
	dma_addr_t da = 0;
	int err;

	if (scif_is_mgmt_node()) {
		struct mic_bootparam *bp = sdev->dp;

		da = bp->scif_card_dma_addr;
		scifdev->rdb = bp->h2c_scif_db;
	} else {
		struct mic_bootparam __iomem *bp = sdev->rdp;

		da = readq(&bp->scif_host_dma_addr);
		scifdev->rdb = ioread8(&bp->c2h_scif_db);
	}

	if (da) {
		err = scif_qp_response(da, scifdev);
		if (err)
			dev_err(&scifdev->sdev->dev,
				"scif_qp_response err %d\n", err);
	} else {
		schedule_delayed_work(&scifdev->qp_dwork,
				      msecs_to_jiffies(1000));
	}
}

static int scif_setup_scifdev(void)
{
	/* We support a maximum of 129 SCIF nodes including the mgmt node */
#define MAX_SCIF_NODES 129
	int i;
	u8 num_nodes = MAX_SCIF_NODES;

	scif_dev = kcalloc(num_nodes, sizeof(*scif_dev), GFP_KERNEL);
	if (!scif_dev)
		return -ENOMEM;
	for (i = 0; i < num_nodes; i++) {
		struct scif_dev *scifdev = &scif_dev[i];

		scifdev->node = i;
		scifdev->exit = OP_IDLE;
		init_waitqueue_head(&scifdev->disconn_wq);
		mutex_init(&scifdev->lock);
		INIT_WORK(&scifdev->peer_add_work, scif_add_peer_device);
		INIT_DELAYED_WORK(&scifdev->p2p_dwork,
				  scif_poll_qp_state);
		INIT_DELAYED_WORK(&scifdev->qp_dwork,
				  scif_qp_setup_handler);
		INIT_LIST_HEAD(&scifdev->p2p);
	}
	return 0;
}

static void scif_destroy_scifdev(void)
{
	kfree(scif_dev);
}

static int scif_probe(struct scif_hw_dev *sdev)
{
	struct scif_dev *scifdev = &scif_dev[sdev->dnode];
	int rc = 0;

	log_scif_info(scifdev->node, "scifdev %p sdev %p", scifdev, sdev);

	dev_set_drvdata(&sdev->dev, sdev);
	scifdev->sdev = sdev;

	mutex_lock(&g_loopb_mutex);
	if (1 == atomic_add_return(1, &g_loopb_cnt)) {
		struct scif_dev *loopb_dev = &scif_dev[sdev->snode];

		loopb_dev->sdev = sdev;
		rc = scif_setup_loopback_qp(loopb_dev);
	}
	mutex_unlock(&g_loopb_mutex);
	if (rc)
		goto exit;

	rc = scif_setup_intr_wq(scifdev);
	if (rc)
		goto destroy_loopb;
	rc = scif_setup_qp(scifdev);
	if (rc)
		goto destroy_intr;
	scifdev->db = sdev->hw_ops->next_db(sdev);
	scifdev->cookie = sdev->hw_ops->request_irq(sdev, scif_intr_handler,
						    "SCIF_INTR", scifdev,
						    scifdev->db);
	if (IS_ERR(scifdev->cookie)) {
		rc = PTR_ERR(scifdev->cookie);
		scifdev->cookie = NULL;
		goto free_qp;
	}

	if (scif_is_mgmt_node()) {
		struct mic_bootparam *bp = sdev->dp;

		bp->c2h_scif_db = scifdev->db;
		bp->scif_host_dma_addr = scifdev->qp_dma_addr;
	} else {
		struct mic_bootparam __iomem *bp = sdev->rdp;

		iowrite8(scifdev->db, &bp->h2c_scif_db);
		writeq(scifdev->qp_dma_addr, &bp->scif_card_dma_addr);
	}

	schedule_delayed_work(&scifdev->qp_dwork, msecs_to_jiffies(1000));
	return rc;
free_qp:
	scif_free_qp(scifdev);
destroy_intr:
	scif_destroy_intr_wq(scifdev);
destroy_loopb:
	mutex_lock(&g_loopb_mutex);
	if (atomic_dec_and_test(&g_loopb_cnt))
		scif_destroy_loopback_qp(&scif_dev[sdev->snode]);
	mutex_unlock(&g_loopb_mutex);
exit:
	return rc;
}

void scif_stop(struct scif_dev *scifdev)
{
	struct scif_dev *dev;
	int i;

	for (i = scif_info.maxid; i >= 0; i--) {
		dev = &scif_dev[i];
		if (scifdev_self(dev))
			continue;
		scif_handle_remove_node(i);
	}
}

static void scif_remove(struct scif_hw_dev *sdev)
{
	struct scif_dev *scifdev = &scif_dev[sdev->dnode];

	log_scif_info(scifdev->node, "scifdev %p sdev %p", scifdev, sdev);

	if (scif_is_mgmt_node()) {
		struct mic_bootparam *bp = sdev->dp;

		bp->c2h_scif_db = -1;
		bp->scif_host_dma_addr = 0x0;
	} else {
		struct mic_bootparam __iomem *bp = sdev->rdp;

		iowrite8(-1, &bp->h2c_scif_db);
		writeq(0x0, &bp->scif_card_dma_addr);
	}
	if (scif_is_mgmt_node()) {
		scif_disconnect_node(scifdev->node, true);
	} else {
		scif_info.card_initiated_exit = true;
		scif_stop(scifdev);
	}

	mutex_lock(&g_loopb_mutex);
	if (atomic_dec_and_test(&g_loopb_cnt))
		scif_destroy_loopback_qp(&scif_dev[sdev->snode]);
	mutex_unlock(&g_loopb_mutex);

	if (scifdev->cookie) {
		sdev->hw_ops->free_irq(sdev, scifdev->cookie, scifdev);
		scifdev->cookie = NULL;
	}
	scif_destroy_intr_wq(scifdev);
	cancel_delayed_work_sync(&scifdev->qp_dwork);
	scif_free_qp(scifdev);
	scifdev->rdb = -1;
	scifdev->sdev = NULL;
}

static struct scif_hw_dev_id id_table[] = {
	{ MIC_SCIF_DEV, SCIF_DEV_ANY_ID },
	{ 0 },
};

static struct scif_driver scif_driver = {
	.driver.name =	KBUILD_MODNAME,
	.driver.owner =	THIS_MODULE,
	.id_table = id_table,
	.probe = scif_probe,
	.remove = scif_remove,
};

static int _scif_init(void)
{
	int rc;

	mutex_init(&scif_info.eplock);
	spin_lock_init(&scif_info.rmalock);
	spin_lock_init(&scif_info.nb_connect_lock);
	spin_lock_init(&scif_info.port_lock);
	mutex_init(&scif_info.conflock);
	mutex_init(&scif_info.connlock);
	mutex_init(&scif_info.fencelock);
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
	INIT_LIST_HEAD(&scif_info.ports_in_use);
#endif
	INIT_LIST_HEAD(&scif_info.uaccept);
	INIT_LIST_HEAD(&scif_info.listen);
	INIT_LIST_HEAD(&scif_info.zombie);
	INIT_LIST_HEAD(&scif_info.connected);
	INIT_LIST_HEAD(&scif_info.disconnected);
	INIT_LIST_HEAD(&scif_info.rma);
	INIT_LIST_HEAD(&scif_info.rma_tc);
	INIT_LIST_HEAD(&scif_info.mmu_notif_cleanup);
	INIT_LIST_HEAD(&scif_info.fence);
	INIT_LIST_HEAD(&scif_info.nb_connect_list);
	init_waitqueue_head(&scif_info.exitwq);
	scif_info.rma_tc_limit = SCIF_RMA_TEMP_CACHE_LIMIT;
	scif_info.en_msg_log = 0;
	scif_info.p2p_enable = 1;
	rc = scif_setup_scifdev();
	if (rc)
		goto error;
	unaligned_cache = kmem_cache_create("Unaligned_DMA",
					    SCIF_KMEM_UNALIGNED_BUF_SIZE,
					    0, SLAB_HWCACHE_ALIGN, NULL);
	if (!unaligned_cache) {
		rc = -ENOMEM;
		goto free_sdev;
	}
	INIT_WORK(&scif_info.misc_work, scif_misc_handler);
	INIT_WORK(&scif_info.mmu_notif_work, scif_mmu_notif_handler);
	INIT_WORK(&scif_info.conn_work, scif_conn_handler);
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
	ida_init(&scif_ports);
#else
	idr_init(&scif_ports);
#endif
	return 0;
free_sdev:
	scif_destroy_scifdev();
error:
	return rc;
}

static void _scif_exit(void)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
	ida_destroy(&scif_ports);
#else
	idr_destroy(&scif_ports);
#endif
	kmem_cache_destroy(unaligned_cache);
	scif_destroy_scifdev();
}

static int __init scif_init(void)
{
	struct miscdevice *mdev = &scif_info.mdev;
	int rc;

	_scif_init();
#ifdef HAS_IOMMU_IOVA_CACHE
	iova_cache_get();
#endif
	rc = scif_peer_bus_init();
	if (rc)
		goto exit;
	rc = scif_register_driver(&scif_driver);
	if (rc)
		goto peer_bus_exit;
	rc = misc_register(mdev);
	if (rc)
		goto unreg_scif;
	scif_init_debugfs();
	return 0;
unreg_scif:
	scif_unregister_driver(&scif_driver);
peer_bus_exit:
	scif_peer_bus_exit();
exit:
	_scif_exit();
	return rc;
}

static void __exit scif_exit(void)
{
	scif_exit_debugfs();
	misc_deregister(&scif_info.mdev);
	scif_unregister_driver(&scif_driver);
	scif_peer_bus_exit();
#ifdef HAS_IOMMU_IOVA_CACHE
	iova_cache_put();
#endif
	_scif_exit();
}

module_init(scif_init);
module_exit(scif_exit);

MODULE_DEVICE_TABLE(scif, id_table);
MODULE_AUTHOR("Intel Corporation");
MODULE_DESCRIPTION("Intel(R) SCIF driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(MIC_VERSION);
