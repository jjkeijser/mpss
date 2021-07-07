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

#include "mic_common.h"
#include "mic/micscif_smpt.h"
#include "mic/micscif_nodeqp.h"
#include "mic/micscif_intr.h"
#include "mic/micscif_nm.h"
#include "micint.h"

struct micscif_info ms_info;
struct micscif_dev scif_dev[MAX_BOARD_SUPPORTED + 1];

bool mic_watchdog_enable = 1;
bool mic_watchdog_auto_reboot = 1;
bool mic_crash_dump_enabled = 1;

int
micscif_init(void)
{
	int err;
	ms_info.mi_nodeid = 0;	// Host is node 0
	ms_info.mi_maxid = 0;	// Host is at start the max card ID
	ms_info.mi_total = 1;	// Host will know about this many MIC cards
	ms_info.mi_mask = 1;	// first bit in the mask is the host node

	mutex_init (&ms_info.mi_conflock);
	spin_lock_init(&ms_info.mi_eplock);
	spin_lock_init(&ms_info.mi_connlock);
	spin_lock_init(&ms_info.mi_rmalock);
	mutex_init (&ms_info.mi_fencelock);
	mutex_init (&ms_info.mi_event_cblock);
	spin_lock_init(&ms_info.mi_nb_connect_lock);
	INIT_LIST_HEAD(&ms_info.mi_uaccept);
	INIT_LIST_HEAD(&ms_info.mi_listen);
	INIT_LIST_HEAD(&ms_info.mi_zombie);
	INIT_LIST_HEAD(&ms_info.mi_connected);
	INIT_LIST_HEAD(&ms_info.mi_disconnected);
	INIT_LIST_HEAD(&ms_info.mi_rma);
	INIT_LIST_HEAD(&ms_info.mi_rma_tc);
#ifdef CONFIG_MMU_NOTIFIER
	INIT_LIST_HEAD(&ms_info.mi_mmu_notif_cleanup);
#endif
	INIT_LIST_HEAD(&ms_info.mi_fence);
	INIT_LIST_HEAD(&ms_info.mi_event_cb);
	INIT_LIST_HEAD(&ms_info.mi_nb_connect_list);
	ms_info.mi_watchdog_to = DEFAULT_WATCHDOG_TO;
#ifdef MIC_IS_EMULATION
	ms_info.mi_watchdog_enabled = 0;
	ms_info.mi_watchdog_auto_reboot = 0;
#else
	ms_info.mi_watchdog_enabled = mic_watchdog_enable;
	ms_info.mi_watchdog_auto_reboot = mic_watchdog_auto_reboot;
#endif
#ifdef RMA_DEBUG
	ms_info.rma_unaligned_cpu_cnt = (atomic_long_t) ATOMIC_LONG_INIT(0);
	ms_info.rma_alloc_cnt = (atomic_long_t) ATOMIC_LONG_INIT(0);
	ms_info.rma_pin_cnt = (atomic_long_t) ATOMIC_LONG_INIT(0);
#ifdef CONFIG_MMU_NOTIFIER
	ms_info.mmu_notif_cnt = (atomic_long_t) ATOMIC_LONG_INIT(0);
#endif
#endif
	ms_info.mi_misc_wq = __mic_create_singlethread_workqueue("SCIF_MISC");
	if (!ms_info.mi_misc_wq) {
		err = -ENOMEM;
		goto wq_error;
	}
	INIT_WORK(&ms_info.mi_misc_work, micscif_misc_handler);
#ifdef CONFIG_MMU_NOTIFIER
	ms_info.mi_mmu_notif_wq = create_singlethread_workqueue("SCIF_MMU");
	if (!ms_info.mi_mmu_notif_wq) {
		err = -ENOMEM;
		goto wq_error;
	}
	INIT_WORK(&ms_info.mi_mmu_notif_work, micscif_mmu_notif_handler);
#endif
	ms_info.mi_conn_wq = __mic_create_singlethread_workqueue("SCIF_NB_CONN");
	if (!ms_info.mi_conn_wq) {
		err = -ENOMEM;
		goto wq_error;
	}
	INIT_WORK(&ms_info.mi_conn_work, micscif_conn_handler);

	//pr_debug("micscif_create(%d) \n", num_bds);

	// Setup information for self aka loopback.
	scif_dev[SCIF_HOST_NODE].sd_node = SCIF_HOST_NODE;
	micscif_setup_loopback_qp(&scif_dev[SCIF_HOST_NODE]);
	scif_dev[SCIF_HOST_NODE].sd_state = SCIFDEV_RUNNING;
	scif_dev[SCIF_HOST_NODE].scif_ref_cnt =
		(atomic_long_t) ATOMIC_LONG_INIT(0);
	scif_dev[SCIF_HOST_NODE].scif_map_ref_cnt = 0;
	init_waitqueue_head(&scif_dev[SCIF_HOST_NODE].sd_wq);
	init_waitqueue_head(&scif_dev[SCIF_HOST_NODE].sd_mmap_wq);
	mutex_init (&scif_dev[SCIF_HOST_NODE].sd_lock);
	ms_info.mi_rma_tc_limit = SCIF_RMA_TEMP_CACHE_LIMIT;
	ms_info.en_msg_log = 0;
	scif_proc_init();
	return 0;
wq_error:
	if (ms_info.mi_misc_wq)
		destroy_workqueue(ms_info.mi_misc_wq);
#ifdef CONFIG_MMU_NOTIFIER
	if (ms_info.mi_mmu_notif_wq)
		destroy_workqueue(ms_info.mi_mmu_notif_wq);
#endif
	if (ms_info.mi_conn_wq)
		destroy_workqueue(ms_info.mi_conn_wq);
	return err;
}

void
micscif_destroy(void)
{
	struct list_head *pos, *unused;
	struct scif_callback *temp;
#ifdef CONFIG_MMU_NOTIFIER
	destroy_workqueue(ms_info.mi_mmu_notif_wq);
#endif
	destroy_workqueue(ms_info.mi_misc_wq);
	destroy_workqueue(ms_info.mi_conn_wq);
	micscif_destroy_loopback_qp(&scif_dev[SCIF_HOST_NODE]);
	scif_proc_cleanup();
	mic_debug_uninit();
	list_for_each_safe(pos, unused, &ms_info.mi_event_cb) {
		temp = list_entry(pos, struct scif_callback, list_member);
		list_del(pos);
		kfree(temp);
	}
	mutex_destroy(&ms_info.mi_event_cblock);
}

int
micscif_host_doorbell_intr_handler(mic_ctx_t *mic_ctx, int doorbell)
{
	struct micscif_dev *dev = &scif_dev[mic_ctx->bi_id + 1];

	queue_work(dev->sd_intr_wq, &dev->sd_intr_bh);
	return 0;
}

int micscif_setup_host_qp(mic_ctx_t *mic_ctx, struct micscif_dev *scifdev);

void
micscif_probe(mic_ctx_t *mic_ctx)
{
	struct micscif_dev *scifdev = &scif_dev[mic_ctx->bi_id + 1];

	// The host needs to keep track of scif_dev interfaces for all boards in
	// the system.  Host is node zero for MIC board 0 is SCIF node 1, etc.
	// This will need to become more dynamic if hot plug is supported

	scifdev->sd_node = mic_ctx->bi_id + 1;
	scifdev->sd_state = SCIFDEV_STOPPED;
	scifdev->mm_sbox = mic_ctx->mmio.va + HOST_SBOX_BASE_ADDRESS;

	/* This workqueue thread will handle all card->host interrupt processing. */
	micscif_setup_interrupts(scifdev);

	init_waitqueue_head(&scifdev->sd_mmap_wq);
	init_waitqueue_head(&scifdev->sd_wq);
	mutex_init (&scifdev->sd_lock);
	INIT_LIST_HEAD(&scifdev->sd_p2p);

	init_waitqueue_head(&scifdev->sd_watchdog_wq);
	snprintf(scifdev->sd_ln_wqname, sizeof(scifdev->sd_intr_wqname),
			"SCIF LOSTNODE %d", scifdev->sd_node);
	if (!(scifdev->sd_ln_wq =
		__mic_create_singlethread_workqueue(scifdev->sd_ln_wqname)))
		printk(KERN_ERR "%s %d wq creation failed\n", __func__, __LINE__);
	INIT_DELAYED_WORK(&scifdev->sd_watchdog_work, micscif_watchdog_handler);
	/*
	 * Register function for doorbell 0 which will
	 * basically kick off the workqueue.
	 */
	mic_reg_irqhandler(mic_ctx, 0, "SCIF DoorBell 0",
			   micscif_host_doorbell_intr_handler);
}

void
micscif_start(mic_ctx_t *mic_ctx)
{
	struct micscif_dev *scifdev = &scif_dev[mic_ctx->bi_id + 1];

	scifdev->scif_ref_cnt = (atomic_long_t) ATOMIC_LONG_INIT(0);
	scifdev->scif_map_ref_cnt = 0;

	scifdev->sd_state = SCIFDEV_INIT;


	/* Sets up bd_bs and the host side of the queuepair */
	pr_debug("micscif_probe: host setting up qp \n");
	micscif_setup_host_qp(mic_ctx, scifdev);
}

void micscif_removehost_respose(struct micscif_dev *scifdev, struct nodemsg *msg);

void
micscif_stop(mic_ctx_t *mic_ctx)
{
	struct micscif_dev *scifdev = &scif_dev[mic_ctx->bi_id + 1];

	if (scifdev->sd_state == SCIFDEV_STOPPED || scifdev->sd_state == SCIFDEV_INIT)
		return;

	micscif_disconnect_node(scifdev->sd_node, NULL, DISCONN_TYPE_LOST_NODE);
}

void
micscif_remove(mic_ctx_t *mic_ctx)
{
	struct micscif_dev *scifdev = &scif_dev[mic_ctx->bi_id + 1];
	struct micscif_qp *qp = &scifdev->qpairs[0];

	destroy_workqueue(scifdev->sd_intr_wq);
	scifdev->sd_intr_wq = 0;
	cancel_delayed_work_sync(&scifdev->sd_watchdog_work);
	if (scifdev->sd_ln_wq){
		destroy_workqueue(scifdev->sd_ln_wq);
		scifdev->sd_ln_wq = 0;
	}
	mic_unreg_irqhandler(mic_ctx, 0x0, "SCIF DoorBell 0");

	if (qp) {
		mic_ctx_unmap_single(mic_ctx, qp->local_buf, qp->inbound_q.size);
		mic_ctx_unmap_single(mic_ctx, qp->local_qp, sizeof(struct micscif_qp));
		kfree((void*)(qp->inbound_q.rb_base));
	}

	if (scifdev->qpairs) {
		kfree(scifdev->qpairs);
		scifdev->qpairs = NULL;
	}
}

int
scif_get_node_status(int node_id)
{
	struct micscif_dev *scifdev = &scif_dev[node_id];

	return scifdev->sd_state;
}

struct scatterlist *
micscif_p2p_mapsg(void *va, int page_size, int page_cnt)
{
	struct scatterlist *sg;
	struct page *page;
	int i;

	if ((sg = kcalloc(page_cnt, sizeof(struct scatterlist), GFP_KERNEL)) == NULL) {
		return NULL;
	}

	sg_init_table(sg, page_cnt);

	for (i = 0; i < page_cnt; i++) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0))
		phys_addr_t phys;
		phys = slow_virt_to_phys(va);

		if ((page = pfn_to_page(phys >> PAGE_SHIFT)) == NULL)
			goto p2p_sg_err;
#else
		if ((page = vmalloc_to_page(va)) == NULL) 
			goto p2p_sg_err;
#endif
		sg_set_page(&sg[i], page, page_size, 0);
		va += page_size;
	}

	return sg;

p2p_sg_err:
	kfree(sg);
	return NULL;
}

void
micscif_p2p_freesg(struct scatterlist *sg)
{
	kfree(sg);
}
