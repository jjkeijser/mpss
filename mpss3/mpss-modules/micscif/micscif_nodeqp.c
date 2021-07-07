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

#include "mic/micscif.h"
#include "mic/micscif_smpt.h"
#include "mic/micscif_nodeqp.h"
#include "mic/micscif_intr.h"
#include "mic/micscif_nm.h"
#include "mic_common.h"
#include "mic/micscif_map.h"

#define SBOX_MMIO_LENGTH	0x10000
/* FIXME: HW spefic, define someplace else */
/* SBOX Offset in MMIO space */
#define SBOX_OFFSET			0x10000

#ifdef ENABLE_TEST
static void micscif_qp_testboth(struct micscif_dev *scifdev);
#endif

bool mic_p2p_enable = 1;
bool mic_p2p_proxy_enable = 1;

void micscif_teardown_ep(void *endpt)
{
	struct endpt *ep = (struct endpt *)endpt;
	struct micscif_qp *qp = ep->qp_info.qp;
	if (qp) {
		if (qp->outbound_q.rb_base)
			scif_iounmap((void *)qp->outbound_q.rb_base,
				qp->outbound_q.size, ep->remote_dev);
		if (qp->remote_qp)
			scif_iounmap((void *)qp->remote_qp,
				sizeof(struct micscif_qp), ep->remote_dev);
		if (qp->local_buf) {
			unmap_from_aperture(
				qp->local_buf,
				ep->remote_dev, ENDPT_QP_SIZE);
		}
		if (qp->local_qp) {
			unmap_from_aperture(qp->local_qp, ep->remote_dev,
					sizeof(struct micscif_qp));
		}
		if (qp->inbound_q.rb_base)
			kfree((void *)qp->inbound_q.rb_base);
		kfree(qp);
#ifdef _MIC_SCIF_
		micscif_teardown_proxy_dma(endpt);
#endif
		WARN_ON(!list_empty(&ep->rma_info.task_list));
	}
}

/*
 * Enqueue the endpoint to the zombie list for cleanup.
 * The endpoint should not be accessed once this API returns.
 */
void micscif_add_epd_to_zombie_list(struct endpt *ep, bool mi_eplock_held)
{
	unsigned long sflags = 0;

	/*
	 * It is an error to call scif_close() on an endpoint on which a
	 * scif_range structure of that endpoint has not been returned
	 * after a call to scif_get_pages() via scif_put_pages().
	 */
	if (SCIFEP_CLOSING == ep->state ||
		SCIFEP_CLOSED == ep->state ||
		SCIFEP_DISCONNECTED == ep->state)
		BUG_ON(micscif_rma_list_get_pages_check(ep));

	if (list_empty(&ep->rma_info.task_list) && ep->remote_dev)
		wake_up(&ep->remote_dev->sd_mmap_wq);
	if (!mi_eplock_held)
		spin_lock_irqsave(&ms_info.mi_eplock, sflags);
	spin_lock(&ep->lock);
	ep->state = SCIFEP_ZOMBIE;
	spin_unlock(&ep->lock);
	list_add_tail(&ep->list, &ms_info.mi_zombie);
	ms_info.mi_nr_zombies++;
	if (!mi_eplock_held)
		spin_unlock_irqrestore(&ms_info.mi_eplock, sflags);
	queue_work(ms_info.mi_misc_wq, &ms_info.mi_misc_work);
}

/* Initializes "local" data structures for the QP
 *
 * Allocates the QP ring buffer (rb), initializes the "in bound" queue
 * For the host generate bus addresses for QP rb & qp, in the card's case
 * map these into the pci aperture
 */
int micscif_setup_qp_connect(struct micscif_qp *qp, dma_addr_t *qp_offset,
				int local_size, struct micscif_dev *scifdev)
{
	void *local_q = NULL;
	int err = 0;
	volatile uint32_t tmp_rd;

	spin_lock_init(&qp->qp_send_lock);
	spin_lock_init(&qp->qp_recv_lock);

	if (!qp->inbound_q.rb_base) {
		/* we need to allocate the local buffer for the incoming queue */
		local_q = kzalloc(local_size, GFP_ATOMIC);
		if (!local_q) {
			printk(KERN_ERR "Ring Buffer Allocation Failed\n");
			err = -ENOMEM;
			return err;
		}
		/* to setup the inbound_q, the buffer lives locally (local_q),
		 * the read pointer is remote (in remote_qp's local_read)
		 * the write pointer is local (in local_write)
		 */
		tmp_rd = 0;
		micscif_rb_init(&qp->inbound_q,
				&tmp_rd, /* No read ptr right now ... */
				&(scifdev->qpairs[0].local_write),
				(volatile void *) local_q,
				local_size);
		qp->inbound_q.read_ptr = NULL; /* it is unsafe to use the ring buffer until this changes! */
	}

	if (!qp->local_buf) {
		err = map_virt_into_aperture(&qp->local_buf, local_q, scifdev, local_size);
		if (err) {
			printk(KERN_ERR "%s %d error %d\n", 
					__func__, __LINE__, err);
			return err;
		}
	}

	if (!qp->local_qp) {
		err = map_virt_into_aperture(qp_offset, qp, scifdev, sizeof(struct micscif_qp));
		if (err) {
			printk(KERN_ERR "%s %d error %d\n", 
					__func__, __LINE__, err);
			return err;
		}
		qp->local_qp = *qp_offset;
	} else {
		*qp_offset = qp->local_qp;
	}
	return err;
}

/* When the other side has already done it's allocation, this is called */
/* TODO: Replace reads that go across the bus somehow ... */
int micscif_setup_qp_accept(struct micscif_qp *qp, dma_addr_t *qp_offset, dma_addr_t phys, int local_size, struct micscif_dev *scifdev)
{
	void *local_q;
	volatile void *remote_q;
	struct micscif_qp *remote_qp;
	int remote_size;
	int err = 0;

	spin_lock_init(&qp->qp_send_lock);
	spin_lock_init(&qp->qp_recv_lock);
	/* Start by figuring out where we need to point */
	remote_qp = scif_ioremap(phys, sizeof(struct micscif_qp), scifdev);
	qp->remote_qp = remote_qp;
	qp->remote_buf = remote_qp->local_buf;
	/* To setup the outbound_q, the buffer lives in remote memory (at scifdev->bs->buf phys),
	 * the read pointer is local (in local's local_read)
	 * the write pointer is remote (In remote_qp's local_write)
	 */
	remote_size = qp->remote_qp->inbound_q.size; /* TODO: Remove this read for p2p */
	remote_q = scif_ioremap(qp->remote_buf, remote_size, scifdev);

	BUG_ON(qp->remote_qp->magic != SCIFEP_MAGIC);

	qp->remote_qp->local_write = 0;
	micscif_rb_init(&(qp->outbound_q),
			&(qp->local_read), /*read ptr*/
			&(qp->remote_qp->local_write), /*write ptr*/
			remote_q, /*rb_base*/
			remote_size);
	/* to setup the inbound_q, the buffer lives locally (local_q),
	 * the read pointer is remote (in remote_qp's local_read)
	 * the write pointer is local (in local_write)
	 */
	local_q = kzalloc(local_size, GFP_KERNEL);
	if (!local_q) {
		printk(KERN_ERR "Ring Buffer Allocation Failed\n");
		err = -ENOMEM;
		return err;
	}

	qp->remote_qp->local_read = 0;
	micscif_rb_init(&(qp->inbound_q),
			&(qp->remote_qp->local_read),
			&(qp->local_write),
			local_q,
			local_size);
	err = map_virt_into_aperture(&qp->local_buf, local_q, scifdev, local_size);
	if (err) {
		printk(KERN_ERR "%s %d error %d\n", 
				__func__, __LINE__, err);
		return err;
	}
	err = map_virt_into_aperture(qp_offset, qp, scifdev, sizeof(struct micscif_qp));
	if (err) {
		printk(KERN_ERR "%s %d error %d\n", 
				__func__, __LINE__, err);
		return err;
	}
	qp->local_qp = *qp_offset;
	return err;
}

int micscif_setup_qp_connect_response(struct micscif_dev *scifdev, struct micscif_qp *qp, uint64_t payload)
{
	int err = 0;
	void *r_buf;
	int remote_size;
	phys_addr_t tmp_phys;

	qp->remote_qp = scif_ioremap(payload, sizeof(struct micscif_qp), scifdev);

	if (!qp->remote_qp) {
		err = -ENOMEM;
		goto error;
	}

	if (qp->remote_qp->magic != SCIFEP_MAGIC) {
		printk(KERN_ERR "SCIFEP_MAGIC doesnot match between node %d "
			"(self) and %d (remote)\n", scif_dev[ms_info.mi_nodeid].sd_node, 
					scifdev->sd_node);
		WARN_ON(1);
		err = -ENODEV;
		goto error;
	}

	tmp_phys = readq(&(qp->remote_qp->local_buf));
	remote_size = readl(&qp->remote_qp->inbound_q.size);
	r_buf = scif_ioremap(tmp_phys, remote_size, scifdev);

#if 0
	pr_debug("payload = 0x%llx remote_qp = 0x%p tmp_phys=0x%llx \
			remote_size=%d r_buf=%p\n", payload, qp->remote_qp, 
			tmp_phys, remote_size, r_buf);
#endif

	micscif_rb_init(&(qp->outbound_q),
			&(qp->local_read),
			&(qp->remote_qp->local_write),
			r_buf,
			remote_size);
	/* resetup the inbound_q now that we know where the inbound_read really is */
	micscif_rb_init(&(qp->inbound_q),
			&(qp->remote_qp->local_read),
			&(qp->local_write),
			qp->inbound_q.rb_base,
			qp->inbound_q.size);
error:
	return err;
}

#ifdef _MIC_SCIF_
extern int micscif_send_host_intr(struct micscif_dev *, uint32_t);

int micscif_send_host_intr(struct micscif_dev *dev, uint32_t doorbell)
{
	uint32_t db_reg;

	if (doorbell > 3)
		return -EINVAL;

	db_reg = readl(dev->mm_sbox +
			(SBOX_SDBIC0 + (4 * doorbell))) | SBOX_SDBIC0_DBREQ_BIT;
	writel(db_reg, dev->mm_sbox + (SBOX_SDBIC0 + (4 * doorbell)));
	return 0;
}
#endif

/*
 * Interrupts remote mic
 */
static void
micscif_send_mic_intr(struct micscif_dev *dev)
{
	/* Writes to RDMASR triggers the interrupt */
	writel(0, (uint8_t *)dev->mm_sbox + dev->sd_rdmasr);
}

/* scifdev - remote scif device
 * also needs the local scif device so that we can decide which RMASR
 * to target on the remote mic
 */
static __always_inline void
scif_send_msg_intr(struct micscif_dev *scifdev)
{
#ifdef _MIC_SCIF_
	if (scifdev == &scif_dev[0])
		micscif_send_host_intr(scifdev, 0);
	else
#endif
		micscif_send_mic_intr(scifdev);
}

#ifdef _MIC_SCIF_
int micscif_setup_card_qp(phys_addr_t host_phys, struct micscif_dev *scifdev)
{
	int local_size;
	dma_addr_t qp_offset;
	int err = 0;
	struct nodemsg tmp_msg;
	uint16_t host_scif_ver;

	pr_debug("Got 0x%llx from the host\n", host_phys);

	local_size = NODE_QP_SIZE;

	/* FIXME: n_qpairs is always 1 OK to get rid of it ? */
	scifdev->n_qpairs = 1;
	scifdev->qpairs = kzalloc(sizeof(struct micscif_qp), GFP_KERNEL);
	if (!scifdev->qpairs) {
		printk(KERN_ERR "Node QP Allocation failed\n");
		err = -ENOMEM;
		return err;
	}

	scifdev->qpairs->magic = SCIFEP_MAGIC;
	pr_debug("micscif_card(): called qp_accept\n");
	err = micscif_setup_qp_accept(&scifdev->qpairs[0], &qp_offset, host_phys, local_size, scifdev);

	if (!err) {
		host_scif_ver = readw(&(&scifdev->qpairs[0])->remote_qp->scif_version);
		if (host_scif_ver != SCIF_VERSION) {
			printk(KERN_ERR "Card and host SCIF versions do not match. \n");
			printk(KERN_ERR "Card version: %u, Host version: %u \n", 
						SCIF_VERSION, host_scif_ver);
			err = -ENXIO;
			goto error_exit;
		}
		/* now that everything is setup and mapped, we're ready to tell the
		 * host where our queue's location
		 */
		tmp_msg.uop = SCIF_INIT;
		tmp_msg.payload[0] = qp_offset;
		tmp_msg.payload[1] = get_rdmasr_offset(scifdev->sd_intr_handle);
		tmp_msg.dst.node = 0; /* host */

		pr_debug("micscif_setup_card_qp: micscif_setup_qp_accept, INIT message\n");
		err = micscif_nodeqp_send(scifdev, &tmp_msg, NULL);
	}
error_exit:
	if (err)
		printk(KERN_ERR "%s %d error %d\n", 
				__func__, __LINE__, err);
	return err;
}


void micscif_send_exit(void)
{
	struct nodemsg msg;
	struct micscif_dev *scifdev = &scif_dev[SCIF_HOST_NODE];

	init_waitqueue_head(&ms_info.mi_exitwq);

	msg.uop = SCIF_EXIT;
	msg.src.node = ms_info.mi_nodeid;
	msg.dst.node = scifdev->sd_node;
	/* No error handling for Host SCIF device */
	micscif_nodeqp_send(scifdev, &msg, NULL);
}

#else /* !_MIC_SCIF_ */
static uint32_t tmp_r_ptr;
int micscif_setup_host_qp(mic_ctx_t *mic_ctx, struct micscif_dev *scifdev)
{
	int err = 0;
	int local_size;

	/* Bail out if the node QP is already setup */
	if (scifdev->qpairs)
		return err;

	local_size = NODE_QP_SIZE;

	/* for now, assume that we only have one queue-pair -- with the host */
	scifdev->n_qpairs = 1;
	scifdev->qpairs = (struct micscif_qp *)kzalloc(sizeof(struct micscif_qp), GFP_ATOMIC);
	if (!scifdev->qpairs) {
		printk(KERN_ERR "Node QP Allocation failed\n");
		err = -ENOMEM;
		return err;
	}

	scifdev->qpairs->magic = SCIFEP_MAGIC;
	scifdev->qpairs->scif_version = SCIF_VERSION;
	err = micscif_setup_qp_connect(&scifdev->qpairs[0], &(mic_ctx->bi_scif.si_pa), local_size, scifdev);
	/* fake the read pointer setup so we can use the inbound q */
	scifdev->qpairs[0].inbound_q.read_ptr = &tmp_r_ptr;

	/* We're as setup as we can be ... the inbound_q is setup, w/o
	 * a usable outbound q.  When we get a message, the read_ptr will
	 * be updated, so we know there's something here.  When that happens,
	 * we finish the setup (just point the write pointer to the real
	 * write pointer that lives on the card), and pull the message off
	 * the card.
	 * Tell the card where we are.
	 */
	printk("My Phys addrs: 0x%llx and scif_addr 0x%llx\n", scifdev->qpairs[0].local_buf, 
		    mic_ctx->bi_scif.si_pa);

	if (err) printk(KERN_ERR "%s %d error %d\n", __func__, __LINE__, err);
	return err;
}


/* FIXME: add to header */
struct scatterlist * micscif_p2p_mapsg(void *va, int page_size, int page_cnt);
void micscif_p2p_freesg(struct scatterlist *);
mic_ctx_t* get_per_dev_ctx(uint16_t node);

/* Init p2p mappings required to access peerdev from scifdev */
static  struct scif_p2p_info *
init_p2p_info(struct micscif_dev *scifdev, struct micscif_dev *peerdev)
{
	struct _mic_ctx_t *mic_ctx_peer;
	struct _mic_ctx_t *mic_ctx;
	struct scif_p2p_info *p2p;
	int num_mmio_pages;
	int num_aper_pages;

	mic_ctx = get_per_dev_ctx(scifdev->sd_node - 1);
	mic_ctx_peer = get_per_dev_ctx(peerdev->sd_node - 1);

	num_mmio_pages = (int) (mic_ctx_peer->mmio.len >> PAGE_SHIFT);
	num_aper_pages = (int) (mic_ctx_peer->aper.len >> PAGE_SHIFT);

	// First map the peer board addresses into the new board
	p2p = kzalloc(sizeof(struct scif_p2p_info), GFP_KERNEL);

	if (p2p){
		int sg_page_shift = get_order(min(mic_ctx_peer->aper.len,(uint64_t)(1 << 30)));		
		/* FIXME: check return codes below */
		p2p->ppi_sg[PPI_MMIO] = micscif_p2p_mapsg(mic_ctx_peer->mmio.va, PAGE_SIZE,
							num_mmio_pages);
		p2p->sg_nentries[PPI_MMIO] = num_mmio_pages;
		p2p->ppi_sg[PPI_APER] = micscif_p2p_mapsg(mic_ctx_peer->aper.va, 1 << sg_page_shift,
							num_aper_pages >> (sg_page_shift - PAGE_SHIFT));
		p2p->sg_nentries[PPI_APER] = num_aper_pages >> (sg_page_shift - PAGE_SHIFT);

		pci_map_sg(mic_ctx->bi_pdev, p2p->ppi_sg[PPI_MMIO], num_mmio_pages, PCI_DMA_BIDIRECTIONAL);
		pci_map_sg(mic_ctx->bi_pdev, p2p->ppi_sg[PPI_APER], 
							num_aper_pages >> (sg_page_shift - PAGE_SHIFT), PCI_DMA_BIDIRECTIONAL);

		p2p->ppi_pa[PPI_MMIO] = sg_dma_address(p2p->ppi_sg[PPI_MMIO]);
		p2p->ppi_pa[PPI_APER] = sg_dma_address(p2p->ppi_sg[PPI_APER]);
		p2p->ppi_len[PPI_MMIO] = num_mmio_pages;
		p2p->ppi_len[PPI_APER] = num_aper_pages;
		p2p->ppi_disc_state = SCIFDEV_RUNNING;
		p2p->ppi_peer_id = peerdev->sd_node;

	}
	return (p2p);
}


int micscif_setuphost_response(struct micscif_dev *scifdev, uint64_t payload)
{
	int read_size;
	struct nodemsg msg;
	int err = 0;

	pr_debug("micscif_setuphost_response: scif node  %d\n", scifdev->sd_node);
	err = micscif_setup_qp_connect_response(scifdev, &scifdev->qpairs[0], payload);
	if (err) {
		printk(KERN_ERR "%s %d error %d\n", __func__, __LINE__, err);
		return err;
	}
	/* re-recieve the bootstrap message after re-init call */
	pr_debug("micscif_host(): reading INIT message after re-init call\n");
	read_size = micscif_rb_get_next(&(scifdev->qpairs[0].inbound_q), &msg,
		sizeof(struct nodemsg));
	micscif_rb_update_read_ptr(&(scifdev->qpairs[0].inbound_q));

	scifdev->sd_rdmasr = (uint32_t)msg.payload[1];

	/* for testing, send a message back to the card */
	msg.uop = SCIF_INIT;
	msg.payload[0] = 0xdeadbeef;
	msg.dst.node = scifdev->sd_node; /* card */
	if ((err = micscif_nodeqp_send(scifdev, &msg, NULL))) {
		printk(KERN_ERR "%s %d error %d\n", __func__, __LINE__, err);
		return err;
	}

#ifdef ENABLE_TEST
	/* Launch the micscif_rb test */
	pr_debug("micscif_host(): starting TEST\n");
	micscif_qp_testboth(scifdev);
#endif

	/*
	 * micscif_nodeqp_intrhandler(..) increments the ref_count before calling
	 * this API hence clamp the scif_ref_cnt to 1. This is required to
	 * handle the SCIF module load/unload case on MIC. The SCIF_EXIT message
	 * keeps the ref_cnt clamped to SCIF_NODE_IDLE during module unload.
	 * Setting the ref_cnt to 1 during SCIF_INIT ensures that the ref_cnt
	 * returns back to 0 once SCIF module load completes.
	 */
#ifdef SCIF_ENABLE_PM
	scifdev->scif_ref_cnt = (atomic_long_t) ATOMIC_LONG_INIT(1);
#endif
	mutex_lock(&ms_info.mi_conflock);
	ms_info.mi_mask |= 0x1 << scifdev->sd_node;
	ms_info.mi_maxid = SCIF_MAX(scifdev->sd_node, ms_info.mi_maxid);
	ms_info.mi_total++;
	scifdev->sd_state = SCIFDEV_RUNNING;
	mutex_unlock(&ms_info.mi_conflock);

	micscif_node_add_callback(scifdev->sd_node);
	return err;
}

void
micscif_removehost_respose(struct micscif_dev *scifdev, struct nodemsg *msg)
{
	mic_ctx_t *mic_ctx = get_per_dev_ctx(scifdev->sd_node -1);
	int err;

	if (scifdev->sd_state != SCIFDEV_RUNNING)
		return;

	micscif_stop(mic_ctx);

	if ((err = micscif_nodeqp_send(scifdev, msg, NULL)))
		printk(KERN_ERR "%s %d error %d\n", __func__, __LINE__, err);

	scifdev->sd_state = SCIFDEV_INIT;
}
#endif

/* TODO: Fix the non-symmetric use of micscif_dev on the host and the card.  Right
 * now, the card's data structures are shaping up such that there is a single
 * micscif_dev structure with multiple qp's.  The host ends up with multiple
 * micscif_devs (one per card).  We should unify the way this will work.
 */
static struct micscif_qp *micscif_nodeqp_find(struct micscif_dev *scifdev, uint8_t node)
{
	struct micscif_qp *qp = NULL;
#ifdef _MIC_SCIF_
	/* This is also a HACK.  Even though the code is identical with the host right
	 * now, I broke it into two parts because they will likely not be identical
	 * moving forward
	 */
	qp = scifdev->qpairs;
#else
	/* HORRIBLE HACK!  Since we only have one card, and one scifdev, we
	 * can just grab the scifdev->qp to find the qp.  We don't actually have to
	 * do any kind of looking for it
	 */
	qp = scifdev->qpairs;
#endif /*  !_MIC_SCIF_ */
	return qp;
}

static char *scifdev_state[] = {"SCIFDEV_NOTPRESENT",
				"SCIFDEV_INIT",
				"SCIFDEV_RUNNING",
				"SCIFDEV_SLEEPING",
				"SCIFDEV_STOPPING",
				"SCIFDEV_STOPPED"};

static char *message_types[] = {"BAD",
				"INIT",
				"EXIT",
				"SCIF_NODE_ADD",
				"SCIF_NODE_ADD_ACK",
				"CNCT_REQ",
				"CNCT_GNT",
				"CNCT_GNTACK",
				"CNCT_GNTNACK",
				"CNCT_REJ",
				"CNCT_TERM",
				"TERM_ACK",
				"DISCNCT",
				"DISCNT_ACK",
				"REGISTER",
				"REGISTER_ACK",
				"REGISTER_NACK",
				"UNREGISTER",
				"UNREGISTER_ACK",
				"UNREGISTER_NACK",
				"ALLOC_REQ",
				"ALLOC_GNT",
				"ALLOC_REJ",
				"FREE_PHYS",
				"FREE_VIRT",
				"CLIENT_SENT",
				"CLIENT_RCVD",
				"MUNMAP",
				"MARK",
				"MARK_ACK",
				"MARK_NACK",
				"WAIT",
				"WAIT_ACK",
				"WAIT_NACK",
				"SIGNAL_LOCAL",
				"SIGNAL_REMOTE",
				"SIG_ACK",
				"SIG_NACK",
				"MAP_GTT",
				"MAP_GTT_ACK",
				"MAP_GTT_NACK",
				"UNMAP_GTT",
				"CREATE_NODE_DEP",
				"DESTROY_NODE_DEP",
				"REMOVE_NODE",
				"REMOVE_NODE_ACK",
				"WAKE_UP_NODE",
				"WAKE_UP_NODE_ACK",
				"WAKE_UP_NODE_NACK",
				"SCIF_NODE_ALIVE",
				"SCIF_NODE_ALIVE_ACK",
				"SCIF_SMPT",
				"SCIF_GTT_DMA_MAP",
				"SCIF_GTT_DMA_ACK",
				"SCIF_GTT_DMA_NACK",
				"SCIF_GTT_DMA_UNMAP",
				"SCIF_PROXY_DMA",
				"SCIF_PROXY_ORDERED_DMA",
				"SCIF_NODE_CONNECT",
				"SCIF_NODE_CONNECT_NACK",
				"SCIF_NODE_ADD_NACK",
				"SCIF_GET_NODE_INFO",
				"TEST"};

static void
micscif_display_message(struct micscif_dev *scifdev, struct nodemsg *msg,
						const char *label)
{
	if (!ms_info.en_msg_log)
		return;
	if (msg->uop > SCIF_MAX_MSG) {
		pr_debug("%s: unknown msg type %d\n", label, msg->uop);
		return;
	}
	if (msg->uop == SCIF_TEST)
		return;

	printk("%s: %s msg type %s, src %d:%d, dest %d:%d "
		"payload 0x%llx:0x%llx:0x%llx:0x%llx\n", 
		label, scifdev_state[scifdev->sd_state], 
		message_types[msg->uop], msg->src.node, msg->src.port, 
		msg->dst.node, msg->dst.port, msg->payload[0], msg->payload[1], 
		msg->payload[2], msg->payload[3]);
}

/**
 * micscif_nodeqp_send - Send a message on the Node Qp.
 * @scifdev: Scif Device.
 * @msg: The message to be sent.
 *
 * This function will block till a message is not sent to the destination
 * scif device.
 */
int micscif_nodeqp_send(struct micscif_dev *scifdev,
			struct nodemsg *msg, struct endpt *ep)
{
	struct micscif_qp *qp;
	int err = -ENOMEM, loop_cnt = 0;

	if (oops_in_progress ||
		(SCIF_INIT != msg->uop &&
		SCIF_EXIT != msg->uop &&
		SCIFDEV_RUNNING != scifdev->sd_state &&
		SCIFDEV_SLEEPING != scifdev->sd_state) ||
		(ep && SCIFDEV_STOPPED == ep->sd_state)) {
		err = -ENODEV;
		goto error;
	}

	micscif_display_message(scifdev, msg, "Sent");

	qp = micscif_nodeqp_find(scifdev, (uint8_t)msg->dst.node);
	if (!qp) {
		err = -EINVAL;
		goto error;
	}
	spin_lock(&qp->qp_send_lock);

	while ((err = micscif_rb_write(&qp->outbound_q,
			msg, sizeof(struct nodemsg)))) {
		cpu_relax();
		mdelay(1);
		if (loop_cnt++ > (NODEQP_SEND_TO_MSEC)) {
			err = -ENODEV;
			break;
		}
	}
	if (!err)
		micscif_rb_commit(&qp->outbound_q);
	spin_unlock(&qp->qp_send_lock);
	if (!err) {
		if (is_self_scifdev(scifdev))
			/*
			 * For loopback we need to emulate an interrupt by queueing
			 * work for the queue handling real Node Qp interrupts.
			 */

			queue_work(scifdev->sd_intr_wq, &scifdev->sd_intr_bh);
		else
			scif_send_msg_intr(scifdev);
	}
error:
	if (err)
		pr_debug("%s %d error %d uop %d\n", 
			__func__, __LINE__, err, msg->uop);
	return err;
}

/* TODO: Make this actually figure out where the interrupt came from.  For host, it can
 * be a little easier (one "vector" per board).  For the cards, we'll have to do some
 * scanning, methinks
 */
struct micscif_qp *micscif_nodeqp_nextmsg(struct micscif_dev *scifdev)
{
	return &scifdev->qpairs[0];
}

/*
 * micscif_misc_handler:
 *
 * Work queue handler for servicing miscellaneous SCIF tasks.
 * Examples include:
 * 1) Remote fence requests.
 * 2) Destruction of temporary registered windows
 *    created during scif_vreadfrom()/scif_vwriteto().
 * 3) Cleanup of zombie endpoints.
 */
void micscif_misc_handler(struct work_struct *work)
{
	micscif_rma_handle_remote_fences();
	micscif_rma_destroy_temp_windows();
#ifdef _MIC_SCIF_
	vm_unmap_aliases();
#endif
	micscif_rma_destroy_tcw_invalid(&ms_info.mi_rma_tc);
	micscif_cleanup_zombie_epd();
}

/**
 * scif_init_resp() - Respond to SCIF_INIT interrupt message
 * @scifdev:    Other node device to respond to
 * @msg:        Interrupt message
 *
 * Loading the driver on the MIC card sends an INIT message to the host
 * with the PCI bus memory information it needs.  This function receives
 * that message, finishes its intialization and echoes it back to the card.
 *
 * When the card receives the message this function starts a connection test.
 */
static __always_inline void
scif_init_resp(struct micscif_dev *scifdev, struct nodemsg *msg)
{
#ifdef _MIC_SCIF_
	if (msg->payload[0] != 0xdeadbeef)
		printk(KERN_ERR "Bad payload 0x%llx\n", msg->payload[0]);
#ifdef ENABLE_TEST
	else
		micscif_qp_testboth(scifdev);
#endif
#else
	pr_debug("scifhost(): sending response to INIT\n");
	micscif_setuphost_response(scifdev, msg->payload[0]);
	atomic_set(&scifdev->sd_node_alive, 0);
	if (scifdev->sd_ln_wq)
		queue_delayed_work(scifdev->sd_ln_wq, 
			&scifdev->sd_watchdog_work, NODE_ALIVE_TIMEOUT);
#endif
}

/**
 * scif_exit_resp() - Respond to SCIF_EXIT interrupt message
 * @scifdev:    Other node device to respond to
 * @msg:        Interrupt message
 *
 * Loading the driver on the MIC card sends an INIT message to the host
 * with the PCI bus memory information it needs.  This function receives
 * that message, finishes its intialization and echoes it back to the card.
 *
 * When the card receives the message this function starts a connection test.
 */
static __always_inline void
scif_exit_resp(struct micscif_dev *scifdev, struct nodemsg *msg)
{
#ifdef _MIC_SCIF_
	printk("card: scif node %d exiting\n", ms_info.mi_nodeid);
	scif_dev[ms_info.mi_nodeid].sd_state = SCIFDEV_STOPPED;
	wake_up(&ms_info.mi_exitwq);
#else
	printk("host: scif node %d exiting\n", msg->src.node);
	/* The interrupt handler that received the message would have
	 * bumped up the ref_cnt by 1. micscif_removehost_response
	 * calls micscif_cleanup_scifdev which loops forever for the ref_cnt
	 * to drop to 0 thereby leading to a soft lockup. To prevent
	 * that, decrement the ref_cnt here.
	 */
	micscif_dec_node_refcnt(scifdev, 1);
	micscif_removehost_respose(scifdev, msg);
	/* increment the ref_cnt here. The interrupt handler will now
	 * decrement it, leaving the ref_cnt to 0 if everything
	 * works as expected. Note that its not absolutely necessary
	 * to do this execpt to make sure ref_cnt is 0 and to catch
	 * errors that may happen if ref_cnt drops to a negative value.
	 */
	micscif_inc_node_refcnt(scifdev, 1);

#endif
}

/**
 * scif_nodeadd_resp() - Respond to SCIF_NODE_ADD interrupt message
 * @scifdev:    Other node device to respond to
 * @msg:        Interrupt message
 *
 * When the host driver has finished initializing a MIC node queue pair it
 * marks the board as online.  It then looks for all currently online MIC
 * cards and send a SCIF_NODE_ADD message to identify the ID of the new card for
 * peer to peer initialization
 *
 * The local node allocates its incoming queue and sends its address in the
 * SCIF_NODE_ADD_ACK message back to the host, the host "reflects" this message
 * to the new node
 */
static __always_inline void
scif_nodeadd_resp(struct micscif_dev *scifdev, struct nodemsg *msg)
{
#ifdef _MIC_SCIF_
	struct micscif_dev *newdev;
	dma_addr_t qp_offset;
	int qp_connect;

	pr_debug("Scifdev %d:%d received NODE_ADD msg for node %d\n", 
	scifdev->sd_node, msg->dst.node, msg->src.node);
	pr_debug("Remote address for this node's aperture %llx\n", 
	msg->payload[0]);
	printk("Remote node's sbox %llx\n", msg->payload[1]);

	newdev = &scif_dev[msg->src.node];
	newdev->sd_node = msg->src.node;

	if (micscif_setup_interrupts(newdev)) {
		printk(KERN_ERR "failed to setup interrupts for %d\n", msg->src.node);
		goto interrupt_setup_error;
	}

	newdev->mm_sbox = ioremap_nocache(msg->payload[1] + SBOX_OFFSET, SBOX_MMIO_LENGTH);

	if (!newdev->mm_sbox) {
		printk(KERN_ERR "failed to map mmio for %d\n", msg->src.node);
		goto mmio_map_error;
	}

	if (!(newdev->qpairs = kzalloc(sizeof(struct micscif_qp), GFP_KERNEL))) {
		printk(KERN_ERR "failed to allocate qpair for %d\n", msg->src.node);
		goto qp_alloc_error;
	}

	/* Set the base address of the remote node's memory since it gets
	 * added to qp_offset
	 */
	newdev->sd_base_addr = msg->payload[0];

	if ((qp_connect = micscif_setup_qp_connect(newdev->qpairs, &qp_offset,
		NODE_QP_SIZE, newdev))) {
		printk(KERN_ERR "failed to setup qp_connect %d\n", qp_connect);
		goto qp_connect_error;
	}

	if (register_scif_intr_handler(newdev))
		goto qp_connect_error;

	newdev->scif_ref_cnt = (atomic_long_t) ATOMIC_LONG_INIT(0);
	micscif_node_add_callback(msg->src.node);
	newdev->qpairs->magic = SCIFEP_MAGIC;
	newdev->qpairs->qp_state = QP_OFFLINE;
	wmb();

	msg->uop = SCIF_NODE_ADD_ACK;
	msg->dst.node = msg->src.node;
	msg->src.node = ms_info.mi_nodeid;
	msg->payload[0] = qp_offset;
	msg->payload[2] = get_rdmasr_offset(newdev->sd_intr_handle);
	msg->payload[3] = scif_dev[ms_info.mi_nodeid].sd_numa_node;
	micscif_nodeqp_send(&scif_dev[SCIF_HOST_NODE], msg, NULL);
	return;

qp_connect_error:
	kfree(newdev->qpairs);
	newdev->qpairs = NULL;
qp_alloc_error:
	iounmap(newdev->mm_sbox);
	newdev->mm_sbox = NULL;
mmio_map_error:
interrupt_setup_error:
	printk(KERN_ERR "node add failed for node %d\n", msg->src.node);
	/*
	 * Update self with NODE ADD failure and send
	 * nack to update the peer.
	 */
	mutex_lock(&newdev->sd_lock);
	newdev->sd_state = SCIFDEV_NOTPRESENT;
	mutex_unlock(&newdev->sd_lock);
	wake_up_interruptible(&newdev->sd_p2p_wq);
	msg->uop = SCIF_NODE_ADD_NACK;
	msg->dst.node = msg->src.node;
	msg->src.node = ms_info.mi_nodeid;
	micscif_nodeqp_send(&scif_dev[SCIF_HOST_NODE], msg, NULL);
#endif
}

#ifdef _MIC_SCIF_
static inline void scif_p2pdev_uninit(struct micscif_dev *peerdev)
{
	deregister_scif_intr_handler(peerdev);
	iounmap(peerdev->mm_sbox);
	mutex_lock(&peerdev->sd_lock);
	peerdev->sd_state = SCIFDEV_NOTPRESENT;
	mutex_unlock(&peerdev->sd_lock);
}

void scif_poll_qp_state(struct work_struct *work)
{
#define NODE_QP_RETRY 100
	struct micscif_dev *peerdev = container_of(work, struct micscif_dev,
							sd_p2p_dwork.work);
	struct micscif_qp *qp = &peerdev->qpairs[0];

	if (SCIFDEV_RUNNING != peerdev->sd_state)
		return;
	if (qp->qp_state == QP_OFFLINE) {
		if (peerdev->sd_p2p_retry++ == NODE_QP_RETRY) {
			printk(KERN_ERR "Warning: QP check timeout with "
				"state %d\n", qp->qp_state);
			goto timeout;
		}
		schedule_delayed_work(&peerdev->sd_p2p_dwork,
			msecs_to_jiffies(NODE_QP_TIMEOUT));
		return;
	}
	wake_up(&peerdev->sd_p2p_wq);
	return;
timeout:
	printk(KERN_ERR "%s %d remote node %d offline,  state = 0x%x\n", 
		__func__, __LINE__, peerdev->sd_node, qp->qp_state);
	micscif_inc_node_refcnt(peerdev, 1);
	qp->remote_qp->qp_state = QP_OFFLINE;
	micscif_dec_node_refcnt(peerdev, 1);
	scif_p2pdev_uninit(peerdev);
	wake_up(&peerdev->sd_p2p_wq);
}
#endif

/**
 * scif_nodeaddack_resp() - Respond to SCIF_NODE_ADD_ACK interrupt message
 * @scifdev:    Other node device to respond to
 * @msg:        Interrupt message
 *
 * After a MIC node receives the SCIF_LINK_ADD_ACK message it send this
 * message to the host to confirm the sequeuce is finished.
 *
 */
static __always_inline void
scif_nodeaddack_resp(struct micscif_dev *scifdev, struct nodemsg *msg)
{
#ifdef _MIC_SCIF_
	struct micscif_dev *peerdev;
	struct micscif_qp *qp;
#else
	struct micscif_dev *dst_dev = &scif_dev[msg->dst.node];
#endif
	pr_debug("Scifdev %d received SCIF_NODE_ADD_ACK msg for src %d dst %d\n", 
		scifdev->sd_node, msg->src.node, msg->dst.node);
	pr_debug("payload %llx %llx %llx %llx\n", msg->payload[0], msg->payload[1], 
		msg->payload[2], msg->payload[3]);
#ifndef _MIC_SCIF_

	/* the lock serializes with micscif_setuphost_response
	* The host is forwarding the NODE_ADD_ACK message from src to dst
	* we need to make sure that the dst has already received a NODE_ADD
	* for src and setup its end of the qp to dst
	*/
	mutex_lock(&ms_info.mi_conflock);
	msg->payload[1] = ms_info.mi_maxid;
	mutex_unlock(&ms_info.mi_conflock);
	micscif_inc_node_refcnt(dst_dev, 1);
	micscif_nodeqp_send(dst_dev, msg, NULL);
	micscif_dec_node_refcnt(dst_dev, 1);
#else
	peerdev = &scif_dev[msg->src.node];
	peerdev->sd_node = msg->src.node;

	if (peerdev->sd_state == SCIFDEV_NOTPRESENT)
		return;

	qp = &peerdev->qpairs[0];

	if ((micscif_setup_qp_connect_response(peerdev, &peerdev->qpairs[0],
				msg->payload[0])))
		goto local_error;

	mutex_lock(&peerdev->sd_lock);
	peerdev->sd_numa_node = msg->payload[3];
	/*
	 * Proxy the DMA only for P2P reads with transfer size
	 * greater than proxy DMA threshold. Proxying reads to convert
	 * them into writes is only required for host jaketown platforms
	 * when the two MIC devices are connected to the same
	 * QPI/IOH/numa node. The host will not pass the numa node
	 * information for non Intel Jaketown platforms and it will
	 * be -1 in that case.
	 */
	peerdev->sd_proxy_dma_reads =
		mic_p2p_proxy_enable &&
		scif_dev[ms_info.mi_nodeid].sd_numa_node != -1 &&
		(peerdev->sd_numa_node ==
		 scif_dev[ms_info.mi_nodeid].sd_numa_node);
	peerdev->sd_state = SCIFDEV_RUNNING;
	mutex_unlock(&peerdev->sd_lock);

	mutex_lock(&ms_info.mi_conflock);
	ms_info.mi_maxid = msg->payload[1];
	peerdev->sd_rdmasr = msg->payload[2];
	mutex_unlock(&ms_info.mi_conflock);

	/* accessing the peer qp. Make sure the peer is awake*/
	micscif_inc_node_refcnt(peerdev, 1);
	qp->remote_qp->qp_state = QP_ONLINE;
	micscif_dec_node_refcnt(peerdev, 1);
	schedule_delayed_work(&peerdev->sd_p2p_dwork,
		msecs_to_jiffies(NODE_QP_TIMEOUT));
	return;
local_error:
	scif_p2pdev_uninit(peerdev);
	wake_up(&peerdev->sd_p2p_wq);
#endif
}

/**
 * scif_cnctreq_resp() - Respond to SCIF_CNCT_REQ interrupt message
 * @msg:        Interrupt message
 *
 * This message is initiated by the remote node to request a connection
 * to the local node.  This function looks for an end point in the
 * listen state on the requested port id.
 *
 * If it finds a listening port it places the connect request on the
 * listening end points queue and wakes up any pending accept calls.
 *
 * If it does not find a listening end point it sends a connection
 * reject message to the remote node.
 */
static __always_inline void
scif_cnctreq_resp(struct micscif_dev *scifdev, struct nodemsg *msg)
{
	struct endpt *ep = NULL;
	struct conreq *conreq;
	unsigned long sflags;

	if ((conreq = (struct conreq *)kmalloc(sizeof(struct conreq), GFP_KERNEL)) == NULL) {
		// Lack of resources so reject the request.
		goto conreq_sendrej;
	}

	if ((ep = micscif_find_listen_ep(msg->dst.port, &sflags)) == NULL) {
		//  Send reject due to no listening ports
		goto conreq_sendrej_free;
	}

	if (ep->backlog <= ep->conreqcnt) {
		//  Send reject due to too many pending requests
		spin_unlock_irqrestore(&ep->lock, sflags);
		goto conreq_sendrej_free;
	}

	conreq->msg = *msg;
	list_add_tail(&conreq->list, &ep->conlist);
	ep->conreqcnt++;
	spin_unlock_irqrestore(&ep->lock, sflags);

	wake_up_interruptible(&ep->conwq);
	return;

conreq_sendrej_free:
	kfree(conreq);
conreq_sendrej:
	msg->uop = SCIF_CNCT_REJ;
	micscif_nodeqp_send(&scif_dev[msg->src.node], msg, NULL);
}

/**
 * scif_cnctgnt_resp() - Respond to SCIF_CNCT_GNT interrupt message
 * @msg:        Interrupt message
 *
 * An accept() on the remote node has occured and sent this message
 * to indicate success.  Place the end point in the MAPPING state and
 * save the remote nodes memory information.  Then wake up the connect
 * request so it can finish.
 */
static __always_inline void
scif_cnctgnt_resp(struct micscif_dev *scifdev, struct nodemsg *msg)
{
	unsigned long sflags;
	struct endpt *ep = (struct endpt *)msg->payload[0];

	spin_lock_irqsave(&ep->lock, sflags);
	if (SCIFEP_CONNECTING == ep->state) {
		ep->peer.node = msg->src.node;
		ep->peer.port = msg->src.port;
		ep->qp_info.cnct_gnt_payload = msg->payload[1];
		ep->remote_ep = msg->payload[2];
		ep->state = SCIFEP_MAPPING;

		wake_up_interruptible(&ep->conwq);
		wake_up(&ep->diswq);
	}
	spin_unlock_irqrestore(&ep->lock, sflags);
}

/**
 * scif_cnctgntack_resp() - Respond to SCIF_CNCT_GNTACK interrupt message
 * @msg:        Interrupt message
 *
 * The remote connection request has finished mapping the local memmory.
 * Place the connection in the connected state and wake up the pending
 * accept() call.
 */
static __always_inline void
scif_cnctgntack_resp(struct micscif_dev *scifdev, struct nodemsg *msg)
{
	unsigned long sflags;
	struct endpt *ep = (struct endpt *)msg->payload[0];

	spin_lock_irqsave(&ms_info.mi_connlock, sflags);
	spin_lock(&ep->lock);
	// New ep is now connected with all resouces set.
	ep->state = SCIFEP_CONNECTED;
	list_add_tail(&ep->list, &ms_info.mi_connected);
	get_conn_count(scifdev);
	wake_up(&ep->conwq);
	spin_unlock(&ep->lock);
	spin_unlock_irqrestore(&ms_info.mi_connlock, sflags);
}

/**
 * scif_cnctgntnack_resp() - Respond to SCIF_CNCT_GNTNACK interrupt message
 * @msg:        Interrupt message
 *
 * The remote connection request failed to map the local memory it was sent.
 * Place the end point in the CLOSING state to indicate it and wake up
 * the pending accept();
 */
static __always_inline void
scif_cnctgntnack_resp(struct micscif_dev *scifdev, struct nodemsg *msg)
{
	struct endpt *ep = (struct endpt *)msg->payload[0];
	unsigned long sflags;

	spin_lock_irqsave(&ep->lock, sflags);
	ep->state = SCIFEP_CLOSING;
	wake_up(&ep->conwq);
	spin_unlock_irqrestore(&ep->lock, sflags);
}

/**
 * scif_cnctrej_resp() - Respond to SCIF_CNCT_REJ interrupt message
 * @msg:        Interrupt message
 *
 * The remote end has rejected the connection request.  Set the end
 * point back to the bound state and wake up the pending connect().
 */
static __always_inline void
scif_cnctrej_resp(struct micscif_dev *scifdev, struct nodemsg *msg)
{
	struct endpt *ep = (struct endpt *)msg->payload[0];
	unsigned long sflags;

	spin_lock_irqsave(&ep->lock, sflags);
	if (SCIFEP_CONNECTING == ep->state) {
		ep->state = SCIFEP_BOUND;
		wake_up_interruptible(&ep->conwq);
	}
	spin_unlock_irqrestore(&ep->lock, sflags);
}

/**
 * scif_cnctterm_resp() - Respond to SCIF_CNCT_TERM interrupt message
 * @msg:        Interrupt message
 *
 * The remote connect() has waited to long for an accept() to occur and
 * is removing the connection request.
 *
 * If the connection request is not found then it is currently being
 * processed and a NACK is sent to indicate to the remote connect() to
 * wait for connection to complete.
 *
 * Otherwise the request is removed and an ACK is returned to indicate
 * success.
 */
static __always_inline void
scif_cnctterm_resp(struct micscif_dev *scifdev, struct nodemsg *msg)
{
	unsigned long sflags;
	struct endpt *ep = NULL;
	struct conreq *conreq = NULL;

	ep = micscif_find_listen_ep(msg->dst.port, &sflags);

	if (ep != NULL) {
		conreq = miscscif_get_connection_request(ep, msg->payload[0]);
		spin_unlock_irqrestore(&ep->lock, sflags);
	}

	if (conreq != NULL) {
		kfree(conreq);
		msg->uop = SCIF_TERM_ACK;
		micscif_nodeqp_send(&scif_dev[msg->src.node], msg, NULL);
	}
}

/**
 * scif_termack_resp() - Respond to SCIF_TERM_ACK interrupt message
 * @msg:        Interrupt message
 *
 * Connection termination has been confirmed so set the end point
 * to bound and allow the connection request to error out.
 */
static __always_inline void
scif_termack_resp(struct micscif_dev *scifdev, struct nodemsg *msg)
{
	struct endpt *ep = (struct endpt *)msg->payload[0];
	unsigned long sflags;

	spin_lock_irqsave(&ep->lock, sflags);
	if (ep->state != SCIFEP_BOUND) {
		ep->state = SCIFEP_BOUND;
		wake_up(&ep->diswq);
	}
	spin_unlock_irqrestore(&ep->lock, sflags);
}

/**
 * scif_discnct_resp() - Respond to SCIF_DISCNCT interrupt message
 * @msg:        Interrupt message
 *
 * The remote node has indicated close() has been called on its end
 * point.  Remove the local end point from the connected list, set its
 * state to disconnected and ensure accesses to the remote node are
 * shutdown.
 *
 * When all accesses to the remote end have completed then send a
 * DISCNT_ACK to indicate it can remove its resources and complete
 * the close routine.
 */
static __always_inline void
scif_discnct_resp(struct micscif_dev *scifdev, struct nodemsg *msg)
{
	unsigned long sflags;
	struct endpt *ep = NULL;
	struct endpt *tmpep;
	struct list_head *pos, *tmpq;

	spin_lock_irqsave(&ms_info.mi_connlock, sflags);
	list_for_each_safe(pos, tmpq, &ms_info.mi_connected) {
		tmpep = list_entry(pos, struct endpt, list);
		if (((uint64_t)tmpep == msg->payload[1]) && ((uint64_t)tmpep->remote_ep == msg->payload[0])) {
			list_del(pos);
			put_conn_count(scifdev);
			ep = tmpep;
			spin_lock(&ep->lock);
			break;
		}
	}

	// If the terminated end is not found then this side started closing
	// before the other side sent the disconnect.  If so the ep will no
	// longer be on the connected list.  Reguardless the other side
	// needs to be acked to let it know close is complete.
	if (ep == NULL) {
		// Need to unlock conn lock and restore irq state
		spin_unlock_irqrestore(&ms_info.mi_connlock, sflags);
		goto discnct_resp_ack;
	}

	ep->state = SCIFEP_DISCONNECTED;
	list_add_tail(&ep->list, &ms_info.mi_disconnected);

	// TODO Cause associated resources to be freed.
	// First step: wake up threads blocked in send and recv
	wake_up_interruptible(&ep->sendwq);
	wake_up_interruptible(&ep->recvwq);
	wake_up_interruptible(&ep->conwq);
	spin_unlock(&ep->lock);
	spin_unlock_irqrestore(&ms_info.mi_connlock, sflags);

discnct_resp_ack:
	msg->uop = SCIF_DISCNT_ACK;
	micscif_nodeqp_send(&scif_dev[msg->src.node], msg, NULL);
}

/**
 * scif_discnctack_resp() - Respond to SCIF_DISCNT_ACK interrupt message
 * @msg:        Interrupt message
 *
 * Remote side has indicated it has not more references to local resources
 */
static __always_inline void
scif_discntack_resp(struct micscif_dev *scifdev, struct nodemsg *msg)
{
	struct endpt *ep = (struct endpt *)msg->payload[0];
	unsigned long sflags;

	spin_lock_irqsave(&ep->lock, sflags);
	ep->state = SCIFEP_DISCONNECTED;
	wake_up(&ep->disconwq);
	spin_unlock_irqrestore(&ep->lock, sflags);
}

/**
 * scif_clientsend_resp() - Respond to SCIF_CLIENT_SEND interrupt message
 * @msg:        Interrupt message
 *
 * Remote side is confirming send or recieve interrupt handling is complete.
 */
static __always_inline void
scif_clientsend_resp(struct micscif_dev *scifdev, struct nodemsg *msg)
{
	struct endpt *ep = (struct endpt *)msg->payload[0];

	if (SCIFEP_CONNECTED == ep->state) {
		wake_up_interruptible(&ep->recvwq);
	}
}

/**
 * scif_clientrcvd_resp() - Respond to SCIF_CLIENT_RCVD interrupt message
 * @msg:        Interrupt message
 *
 * Remote side is confirming send or recieve interrupt handling is complete.
 */
static __always_inline void
scif_clientrcvd_resp(struct micscif_dev *scifdev, struct nodemsg *msg)
{
	struct endpt *ep = (struct endpt *)msg->payload[0];

	if (SCIFEP_CONNECTED == ep->state) {
		wake_up_interruptible(&ep->sendwq);
	}
}

/**
 * scif_alloc_req: Respond to SCIF_ALLOC_REQ interrupt message
 * @msg:        Interrupt message
 *
 * Remote side is requesting a memory allocation.
 */
static __always_inline void
scif_alloc_req(struct micscif_dev *scifdev, struct nodemsg *msg)
{
	int err, opcode = (int)msg->payload[3];
	struct reg_range_t *window = 0;
	size_t nr_pages = msg->payload[1];
	struct endpt *ep = (struct endpt *)msg->payload[0];

	might_sleep();

	if (SCIFEP_CONNECTED != ep->state) {
		err = -ENOTCONN;
		goto error;
	}

	switch (opcode) {
	case SCIF_REGISTER:
		if (!(window = micscif_create_remote_window(ep,
			(int)nr_pages))) {
			err = -ENOMEM;
			goto error;
		}
		break;
	default:
		/* Unexpected allocation request */
		printk(KERN_ERR "Unexpected allocation request opcode 0x%x ep = 0x%p "
			" scifdev->sd_state 0x%x scifdev->sd_node 0x%x\n", 
			opcode, ep, scifdev->sd_state, scifdev->sd_node);
		err = -EINVAL;
		goto error;
	};

	/* The peer's allocation request is granted */
	msg->uop = SCIF_ALLOC_GNT;
	msg->payload[0] = (uint64_t)window;
	msg->payload[1] = window->mapped_offset;
	if ((err = micscif_nodeqp_send(ep->remote_dev, msg, ep)))
		micscif_destroy_remote_window(ep, window);
	return;
error:
	/* The peer's allocation request is rejected */
	printk(KERN_ERR "%s %d error %d alloc_ptr %p nr_pages 0x%lx\n", 
		__func__, __LINE__, err, window, nr_pages);
	msg->uop = SCIF_ALLOC_REJ;
	micscif_nodeqp_send(ep->remote_dev, msg, ep);
}

/**
 * scif_alloc_gnt_rej: Respond to SCIF_ALLOC_GNT/REJ interrupt message
 * @msg:        Interrupt message
 *
 * Remote side responded to a memory allocation.
 */
static __always_inline void
scif_alloc_gnt_rej(struct micscif_dev *scifdev, struct nodemsg *msg)
{
	struct allocmsg *handle = (struct allocmsg *)msg->payload[2];
	switch (handle->uop) {
	case SCIF_REGISTER:
	{
		handle->vaddr = (void *)msg->payload[0];
		handle->phys_addr = msg->payload[1];
		if (msg->uop == SCIF_ALLOC_GNT)
			handle->state = OP_COMPLETED;
		else
			handle->state = OP_FAILED;
		wake_up(&handle->allocwq);
		break;
	}
	default:
	{
		printk(KERN_ERR "Bug Unknown alloc uop 0x%x\n", handle->uop);
	}
	}
}

/**
 * scif_free_phys: Respond to SCIF_FREE_PHYS interrupt message
 * @msg:        Interrupt message
 *
 * Remote side is done accessing earlier memory allocation.
 * Remove GTT/PCI mappings created earlier.
 */
static __always_inline void
scif_free_phys(struct micscif_dev *scifdev, struct nodemsg *msg)
{
	return;
}

/**
 * scif_free_phys: Respond to SCIF_FREE_VIRT interrupt message
 * @msg:        Interrupt message
 *
 * Free up memory kmalloc'd earlier.
 */
static __always_inline void
scif_free_virt(struct micscif_dev *scifdev, struct nodemsg *msg)
{
	struct endpt *ep = (struct endpt *)msg->payload[0];
	int opcode = (int)msg->payload[3];
	struct reg_range_t *window =
		(struct reg_range_t *)msg->payload[1];

	switch (opcode) {
	case SCIF_REGISTER:
		micscif_destroy_remote_window(ep, window);
		break;
	default:
		/* Unexpected allocation request */
		BUG_ON(opcode != SCIF_REGISTER);
	};
}

/**
 * scif_recv_register: Respond to SCIF_REGISTER interrupt message
 * @msg:        Interrupt message
 *
 * Update remote window list with a new registered window.
 */
static __always_inline void
scif_recv_register(struct micscif_dev *scifdev, struct nodemsg *msg)
{
	unsigned long sflags;
	struct endpt *ep = (struct endpt *)msg->payload[0];
	struct reg_range_t *window =
		(struct reg_range_t *)msg->payload[1];

	might_sleep();
	RMA_MAGIC(window);
	mutex_lock(&ep->rma_info.rma_lock);
	/* FIXME:
	 * ep_lock lock needed ? rma_lock is already held
	 */
	spin_lock_irqsave(&ep->lock, sflags);
	if (SCIFEP_CONNECTED == ep->state) {
		msg->uop = SCIF_REGISTER_ACK;
		micscif_nodeqp_send(ep->remote_dev, msg, ep);
		micscif_set_nr_pages(ep->remote_dev, window);
		/* No further failures expected. Insert new window */
		micscif_insert_window(window,
			&ep->rma_info.remote_reg_list);
	} else {
		msg->uop = SCIF_REGISTER_NACK;
		micscif_nodeqp_send(ep->remote_dev, msg, ep);
	}
	spin_unlock_irqrestore(&ep->lock, sflags);
	mutex_unlock(&ep->rma_info.rma_lock);
	/*
	 * We could not insert the window but we need to
	 * destroy the window.
	 */
	if (SCIF_REGISTER_NACK == msg->uop)
		micscif_destroy_remote_window(ep, window);
	else {
#ifdef _MIC_SCIF_
		micscif_destroy_remote_lookup(ep, window);
#endif
	}
}

/**
 * scif_recv_unregister: Respond to SCIF_UNREGISTER interrupt message
 * @msg:        Interrupt message
 *
 * Remove window from remote registration list;
 */
static __always_inline void
scif_recv_unregister(struct micscif_dev *scifdev, struct nodemsg *msg)
{
	struct micscif_rma_req req;
	struct reg_range_t *window = NULL;
	struct reg_range_t *recv_window =
		(struct reg_range_t *)msg->payload[0];
	struct endpt *ep;
	int del_window = 0;

	might_sleep();
	RMA_MAGIC(recv_window);
	ep = (struct endpt *)recv_window->ep;
	req.out_window = &window;
	req.offset = recv_window->offset;
	req.prot = 0;
	req.nr_bytes = recv_window->nr_pages << PAGE_SHIFT;
	req.type = WINDOW_FULL;
	req.head = &ep->rma_info.remote_reg_list;
	msg->payload[0] = ep->remote_ep;

	mutex_lock(&ep->rma_info.rma_lock);
	/*
	 * Does a valid window exist?
	 */
	if (micscif_query_window(&req)) {
		printk(KERN_ERR "%s %d -ENXIO\n", __func__, __LINE__);
		msg->uop = SCIF_UNREGISTER_ACK;
		goto error;
	}
	if (window) {
		RMA_MAGIC(window);
		if (window->ref_count)
			put_window_ref_count(window, window->nr_pages);
		window->unreg_state = OP_COMPLETED;
		if (!window->ref_count) {
			msg->uop = SCIF_UNREGISTER_ACK;
			atomic_inc(&ep->rma_info.tw_refcount);
			atomic_add_return((int32_t)window->nr_pages, &ep->rma_info.tw_total_pages);
			ep->rma_info.async_list_del = 1;
			list_del(&window->list_member);
			window->offset = INVALID_VA_GEN_ADDRESS;
			del_window = 1;
		} else
			/* NACK! There are valid references to this window */
			msg->uop = SCIF_UNREGISTER_NACK;
	} else {
		/* The window did not make its way to the list at all. ACK */
		msg->uop = SCIF_UNREGISTER_ACK;
		micscif_destroy_remote_window(ep, recv_window);
	}
error:
	mutex_unlock(&ep->rma_info.rma_lock);
	if (del_window)
		drain_dma_intr(ep->rma_info.dma_chan);
	micscif_nodeqp_send(ep->remote_dev, msg, ep);
	if (del_window)
		micscif_queue_for_cleanup(window, &ms_info.mi_rma);
	return;
}

/**
 * scif_recv_register_ack: Respond to SCIF_REGISTER_ACK interrupt message
 * @msg:        Interrupt message
 *
 * Wake up the window waiting to complete registration.
 */
static __always_inline void
scif_recv_register_ack(struct micscif_dev *scifdev, struct nodemsg *msg)
{
	struct reg_range_t *window =
		(struct reg_range_t *)msg->payload[2];
	RMA_MAGIC(window);
	window->reg_state = OP_COMPLETED;
	wake_up(&window->regwq);
}

/**
 * scif_recv_register_nack: Respond to SCIF_REGISTER_NACK interrupt message
 * @msg:        Interrupt message
 *
 * Wake up the window waiting to inform it that registration
 * cannot be completed.
 */
static __always_inline void
scif_recv_register_nack(struct micscif_dev *scifdev, struct nodemsg *msg)
{
	struct reg_range_t *window =
		(struct reg_range_t *)msg->payload[2];
	RMA_MAGIC(window);
	window->reg_state = OP_FAILED;
	wake_up(&window->regwq);
}
/**
 * scif_recv_unregister_ack: Respond to SCIF_UNREGISTER_ACK interrupt message
 * @msg:        Interrupt message
 *
 * Wake up the window waiting to complete unregistration.
 */
static __always_inline void
scif_recv_unregister_ack(struct micscif_dev *scifdev, struct nodemsg *msg)
{
	struct reg_range_t *window =
		(struct reg_range_t *)msg->payload[1];
	RMA_MAGIC(window);
	window->unreg_state = OP_COMPLETED;
	wake_up(&window->unregwq);
}

/**
 * scif_recv_unregister_nack: Respond to SCIF_UNREGISTER_NACK interrupt message
 * @msg:        Interrupt message
 *
 * Wake up the window waiting to inform it that unregistration
 * cannot be completed immediately.
 */
static __always_inline void
scif_recv_unregister_nack(struct micscif_dev *scifdev, struct nodemsg *msg)
{
	struct reg_range_t *window =
		(struct reg_range_t *)msg->payload[1];
	RMA_MAGIC(window);
	window->unreg_state = OP_FAILED;
	wake_up(&window->unregwq);
}

static __always_inline void
scif_recv_munmap(struct micscif_dev *scifdev, struct nodemsg *msg)
{
	struct micscif_rma_req req;
	struct reg_range_t *window = NULL;
	struct reg_range_t *recv_window =
		(struct reg_range_t *)msg->payload[0];
	struct endpt *ep;
	int del_window = 0;

	might_sleep();
	RMA_MAGIC(recv_window);
	ep = (struct endpt *)recv_window->ep;
	req.out_window = &window;
	req.offset = recv_window->offset;
	req.prot = recv_window->prot;
	req.nr_bytes = recv_window->nr_pages << PAGE_SHIFT;
	req.type = WINDOW_FULL;
	req.head = &ep->rma_info.reg_list;
	msg->payload[0] = ep->remote_ep;

	mutex_lock(&ep->rma_info.rma_lock);
	/*
	 * Does a valid window exist?
	 */
	if (micscif_query_window(&req)) {
		printk(KERN_ERR "%s %d -ENXIO\n", __func__, __LINE__);
		msg->uop = SCIF_UNREGISTER_ACK;
		goto error;
	}

	RMA_MAGIC(window);

	if (window->ref_count)
		put_window_ref_count(window, window->nr_pages);

	if (!window->ref_count) {
		atomic_inc(&ep->rma_info.tw_refcount);
		atomic_add_return((int32_t)window->nr_pages, &ep->rma_info.tw_total_pages);
		ep->rma_info.async_list_del = 1;
		list_del(&window->list_member);
		micscif_free_window_offset(ep, window->offset,
				window->nr_pages << PAGE_SHIFT);
		window->offset_freed = true;
		del_window = 1;
	}
error:
	mutex_unlock(&ep->rma_info.rma_lock);
	if (del_window)
		micscif_queue_for_cleanup(window, &ms_info.mi_rma);
}

/**
 * scif_recv_mark: Handle SCIF_MARK request
 * @msg:	Interrupt message
 *
 * The peer has requested a mark.
 */
static __always_inline void
scif_recv_mark(struct micscif_dev *scifdev, struct nodemsg *msg)
{
	struct endpt *ep = (struct endpt *)msg->payload[0];
	int mark;

	if (SCIFEP_CONNECTED != ep->state) {
		msg->payload[0] = ep->remote_ep;
		msg->uop = SCIF_MARK_NACK;
		micscif_nodeqp_send(ep->remote_dev, msg, ep);
		return;
	}

	if ((mark = micscif_fence_mark(ep)) < 0)
		msg->uop = SCIF_MARK_NACK;
	else
		msg->uop = SCIF_MARK_ACK;
	msg->payload[0] = ep->remote_ep;
	msg->payload[2] = mark;
	micscif_nodeqp_send(ep->remote_dev, msg, ep);
}

/**
 * scif_recv_mark_resp: Handle SCIF_MARK_(N)ACK messages.
 * @msg:	Interrupt message
 *
 * The peer has responded to a SCIF_MARK message.
 */
static __always_inline void
scif_recv_mark_resp(struct micscif_dev *scifdev, struct nodemsg *msg)
{
	struct endpt *ep = (struct endpt *)msg->payload[0];
	struct fence_info *fence_req = (struct fence_info *)msg->payload[1];

	mutex_lock(&ep->rma_info.rma_lock);
	if (SCIF_MARK_ACK == msg->uop) {
		fence_req->state = OP_COMPLETED;
		fence_req->dma_mark = (int)msg->payload[2];
	} else
		fence_req->state = OP_FAILED;
	wake_up(&fence_req->wq);
	mutex_unlock(&ep->rma_info.rma_lock);
}

/**
 * scif_recv_wait: Handle SCIF_WAIT request
 * @msg:	Interrupt message
 *
 * The peer has requested waiting on a fence.
 */
static __always_inline void
scif_recv_wait(struct micscif_dev *scifdev, struct nodemsg *msg)
{
	struct endpt *ep = (struct endpt *)msg->payload[0];
	struct remote_fence_info *fence;

	/*
	 * Allocate structure for remote fence information and
	 * send a NACK if the allocation failed. The peer will
	 * return ENOMEM upon receiving a NACK.
	 */
	if (!(fence = (struct remote_fence_info *)kmalloc(
			sizeof(struct remote_fence_info), GFP_KERNEL))) {
		msg->payload[0] = ep->remote_ep;
		msg->uop = SCIF_WAIT_NACK;
		micscif_nodeqp_send(ep->remote_dev, msg, ep);
		return;
	}

	/* Prepare the fence request */
	memcpy(&fence->msg, msg, sizeof(struct nodemsg));
	INIT_LIST_HEAD(&fence->list_member);

	/* Insert to the global remote fence request list */
	mutex_lock(&ms_info.mi_fencelock);
	ep->rma_info.fence_refcount++;
	list_add_tail(&fence->list_member, &ms_info.mi_fence);
	mutex_unlock(&ms_info.mi_fencelock);

	queue_work(ms_info.mi_misc_wq, &ms_info.mi_misc_work);
}

/**
 * scif_recv_wait_resp: Handle SCIF_WAIT_(N)ACK messages.
 * @msg:	Interrupt message
 *
 * The peer has responded to a SCIF_WAIT message.
 */
static __always_inline void
scif_recv_wait_resp(struct micscif_dev *scifdev, struct nodemsg *msg)
{
	struct endpt *ep = (struct endpt *)msg->payload[0];
	struct fence_info *fence_req = (struct fence_info *)msg->payload[1];

	mutex_lock(&ep->rma_info.rma_lock);
	if (SCIF_WAIT_ACK == msg->uop)
		fence_req->state = OP_COMPLETED;
	else
		fence_req->state = OP_FAILED;
	wake_up(&fence_req->wq);
	mutex_unlock(&ep->rma_info.rma_lock);
}

/**
 * scif_recv_local_signal: Handle SCIF_SIG_LOCAL request
 * @msg:	Interrupt message
 *
 * The peer has requested a signal on a local offset.
 */
static __always_inline void
scif_recv_signal_local(struct micscif_dev *scifdev, struct nodemsg *msg)
{
	int err = 0;
	struct endpt *ep = (struct endpt *)msg->payload[0];

	err = micscif_prog_signal(ep,
			msg->payload[1],
			msg->payload[2],
			RMA_WINDOW_SELF);
	if (err)
		msg->uop = SCIF_SIG_NACK;
	else
		msg->uop = SCIF_SIG_ACK;
	msg->payload[0] = ep->remote_ep;
	if ((err = micscif_nodeqp_send(ep->remote_dev, msg, ep)))
		printk(KERN_ERR "%s %d err %d\n", __func__, __LINE__, err);
}

/**
 * scif_recv_signal_remote: Handle SCIF_SIGNAL_REMOTE request
 * @msg:	Interrupt message
 *
 * The peer has requested a signal on a remote offset.
 */
static __always_inline void
scif_recv_signal_remote(struct micscif_dev *scifdev, struct nodemsg *msg)
{
	int err = 0;
	struct endpt *ep = (struct endpt *)msg->payload[0];

	err = micscif_prog_signal(ep,
			msg->payload[1],
			msg->payload[2],
			RMA_WINDOW_PEER);
	if (err)
		msg->uop = SCIF_SIG_NACK;
	else
		msg->uop = SCIF_SIG_ACK;
	msg->payload[0] = ep->remote_ep;
	if ((err = micscif_nodeqp_send(ep->remote_dev, msg, ep)))
		printk(KERN_ERR "%s %d err %d\n", __func__, __LINE__, err);
}

/**
 * scif_recv_signal_remote: Handle SCIF_SIG_(N)ACK messages.
 * @msg:	Interrupt message
 *
 * The peer has responded to a signal request.
 */
static __always_inline void
scif_recv_signal_resp(struct micscif_dev *scifdev, struct nodemsg *msg)
{
	struct endpt *ep = (struct endpt *)msg->payload[0];
	struct fence_info *fence_req = (struct fence_info *)msg->payload[3];

	mutex_lock(&ep->rma_info.rma_lock);
	if (SCIF_SIG_ACK == msg->uop)
		fence_req->state = OP_COMPLETED;
	else
		fence_req->state = OP_FAILED;
	wake_up(&fence_req->wq);
	mutex_unlock(&ep->rma_info.rma_lock);
}

/*
 * scif_node_wake_up_ack: Handle SCIF_NODE_WAKE_UP_ACK message
 * @msg: Interrupt message
 *
 * Response for a SCIF_NODE_WAKE_UP message.
 */
static __always_inline void
scif_node_wake_up_ack(struct micscif_dev *scifdev, struct nodemsg *msg)
{
	scif_dev[msg->payload[0]].sd_wait_status = OP_COMPLETED;
	wake_up(&scif_dev[msg->payload[0]].sd_wq);
}

/*
 * scif_node_wake_up_nack: Handle SCIF_NODE_WAKE_UP_NACK message
 * @msg: Interrupt message
 *
 * Response for a SCIF_NODE_WAKE_UP message.
 */
static __always_inline void
scif_node_wake_up_nack(struct micscif_dev *scifdev, struct nodemsg *msg)
{
	scif_dev[msg->payload[0]].sd_wait_status = OP_FAILED;
	wake_up(&scif_dev[msg->payload[0]].sd_wq);
}

/*
 * scif_node_remove: Handle SCIF_NODE_REMOVE message
 * @msg: Interrupt message
 *
 * Handle node removal.
 */
static __always_inline void
scif_node_remove(struct micscif_dev *scifdev, struct nodemsg *msg)
{
	msg->payload[0] = micscif_handle_remove_node(msg->payload[0], msg->payload[1]);
	msg->uop = SCIF_NODE_REMOVE_ACK;
	msg->src.node = ms_info.mi_nodeid;
	micscif_nodeqp_send(&scif_dev[SCIF_HOST_NODE], msg, NULL);
}

#ifndef _MIC_SCIF_
/*
 * scif_node_remove_ack: Handle SCIF_NODE_REMOVE_ACK message
 * @msg: Interrupt message
 *
 * The peer has acked a SCIF_NODE_REMOVE message.
 */
static __always_inline void
scif_node_remove_ack(struct micscif_dev *scifdev, struct nodemsg *msg)
{
	bool ack_is_current = true;
	int orig_node = (int)msg->payload[3];

	if ((msg->payload[1] << 32) == DISCONN_TYPE_POWER_MGMT) {
		if (msg->payload[2] != atomic_long_read(&ms_info.mi_unique_msgid))
			ack_is_current = false;
	}

	if (ack_is_current) {
		mic_ctx_t *mic_ctx = get_per_dev_ctx(orig_node - 1);
		if (!mic_ctx) {
			printk(KERN_ERR "%s %d mic_ctx %p orig_node %d\n", 
				__func__, __LINE__, mic_ctx, orig_node);
			return;
		}

		if (msg->payload[0]) {
			pr_debug("%s failed to get remove ack from node id %d", __func__, msg->src.node);
			ms_info.mi_disconnect_status = OP_FAILED;
		}

		atomic_inc(&mic_ctx->disconn_rescnt);
		wake_up(&ms_info.mi_disconn_wq);
	}
}

/*
 * scif_node_create_ack: Handle SCIF_NODE_CREATE_DEP message
 * @msg: Interrupt message
 *
 * Notification about a new SCIF dependency between two nodes.
 */
static __always_inline void
scif_node_create_dep(struct micscif_dev *scifdev, struct nodemsg *msg)
{
	uint32_t src_node = msg->src.node;
	uint32_t dst_node = (uint32_t)msg->payload[0];
	/*
	 * Host driver updates dependency graph.
	 * src_node created dependency on dst_node
	 * src_node -> dst_node
	 */
	micscif_set_nodedep(src_node, dst_node, DEP_STATE_DEPENDENT);
}

/*
 * scif_node_destroy_ack: Handle SCIF_NODE_DESTROY_DEP message
 * @msg: Interrupt message
 *
 * Notification about tearing down an existing SCIF dependency
 * between two nodes.
 */
static __always_inline void
scif_node_destroy_dep(struct micscif_dev *scifdev, struct nodemsg *msg)
{
	uint32_t src_node = msg->src.node;
	uint32_t dst_node = (uint32_t)msg->payload[0];
	/*
	 * Host driver updates dependency graph.
	 * src_node removed dependency on dst_node
	 */
	micscif_set_nodedep(src_node, dst_node, DEP_STATE_NOT_DEPENDENT);
}

/*
 * scif_node_wake_up: Handle SCIF_NODE_WAKE_UP message
 * @msg: Interrupt message
 *
 * The host has received a request to wake up a remote node.
 */
static __always_inline void
scif_node_wake_up(struct micscif_dev *scifdev, struct nodemsg *msg)
{
	/*
	 * Host Driver now needs to wake up the remote node
	 * available in msg->payload[0].
	 */
	uint32_t ret = 0;
	ret = micscif_connect_node((uint32_t)msg->payload[0], false);

	if(!ret) {
		msg->uop = SCIF_NODE_WAKE_UP_ACK;
		micscif_update_p2p_state((uint32_t)msg->payload[0],
				msg->src.node, SCIFDEV_RUNNING);
	} else {
		msg->uop = SCIF_NODE_WAKE_UP_NACK;
	}
	micscif_nodeqp_send(&scif_dev[msg->src.node], msg, NULL);
}
#endif

#ifdef _MIC_SCIF_
static __always_inline void
scif_node_alive_resp(struct micscif_dev *scifdev, struct nodemsg *msg)
{
	msg->uop = SCIF_NODE_ALIVE_ACK;
	msg->src.node = ms_info.mi_nodeid;
	msg->dst.node = SCIF_HOST_NODE;
	micscif_nodeqp_send(&scif_dev[SCIF_HOST_NODE], msg, NULL);
	pr_debug("node alive ack sent from node %d oops_in_progress %d\n", 
			ms_info.mi_nodeid, oops_in_progress);
}
#else
static __always_inline void
scif_node_alive_ack(struct micscif_dev *scifdev, struct nodemsg *msg)
{
	pr_debug("node alive ack received from node %d\n", msg->src.node);
	atomic_set(&scif_dev[msg->src.node].sd_node_alive, 1);
	wake_up(&scifdev->sd_watchdog_wq);
}
#endif


#ifdef _MIC_SCIF_
static __always_inline void
_scif_proxy_dma(struct micscif_dev *scifdev, struct nodemsg *msg, int flags)
{
	struct endpt *ep = (struct endpt *)msg->payload[0];
	off_t loffset = msg->payload[1];
	off_t roffset = msg->payload[2];
	size_t len = msg->payload[3];
	struct dma_channel *chan = ep->rma_info.dma_chan;
	struct endpt_rma_info *rma = &ep->rma_info;
	int err = __scif_writeto(ep, loffset, len, roffset, flags);

	if (!err && rma->proxy_dma_peer_phys &&
		!request_dma_channel(chan)) {
		do_status_update(chan, rma->proxy_dma_peer_phys, OP_COMPLETED);
		free_dma_channel(chan);
	}
	if (!rma->proxy_dma_peer_phys)
		/* The proxy DMA physical address should have been set up? */
		WARN_ON(1);
}

/**
 * scif_proxy_dma: Handle SCIF_PROXY_DMA request.
 * @msg:	Interrupt message
 *
 * The peer has requested a Proxy DMA.
 */
static __always_inline void
scif_proxy_dma(struct micscif_dev *scifdev, struct nodemsg *msg)
{
	_scif_proxy_dma(scifdev, msg, 0x0);
}

/**
 * scif_proxy_ordered_dma: Handle SCIF_PROXY_ORDERED_DMA request.
 * @msg:	Interrupt message
 *
 * The peer has requested an ordered Proxy DMA.
 */
static __always_inline void
scif_proxy_ordered_dma(struct micscif_dev *scifdev, struct nodemsg *msg)
{
	_scif_proxy_dma(scifdev, msg, SCIF_RMA_ORDERED);
}
#endif

#ifndef _MIC_SCIF_
/**
 * scif_node_connect: Respond to SCIF_NODE_CONNECT interrupt message
 * @msg:        Interrupt message
 *
 * Connect the src and dst node by setting up the p2p connection
 * between them. Host here acts like a proxy.
 */
static __always_inline void
scif_node_connect_resp(struct micscif_dev *scifdev, struct nodemsg *msg)
{
	struct micscif_dev *dev_j = scifdev;
	struct micscif_dev *dev_i = NULL;
	struct scif_p2p_info *p2p_ij = NULL;    /* bus addr for j from i */
	struct scif_p2p_info *p2p_ji = NULL;    /* bus addr for i from j */
	struct scif_p2p_info *p2p;
	struct list_head *pos, *tmp;
	uint32_t bid = (uint32_t)msg->payload[0];
	int err;
	uint64_t tmppayload;

	pr_debug("%s:%d SCIF_NODE_CONNECT from %d connecting to %d \n", 
				 __func__, __LINE__, scifdev->sd_node, bid);

	mutex_lock(&ms_info.mi_conflock);
	if (bid < 1 || bid > ms_info.mi_maxid) {
		printk(KERN_ERR "%s %d unknown bid %d\n", __func__, __LINE__, bid);
		goto nack;
	}

	dev_i = &scif_dev[bid];
	mutex_unlock(&ms_info.mi_conflock);
	micscif_inc_node_refcnt(dev_i, 1);
	mutex_lock(&ms_info.mi_conflock);

	if (dev_i->sd_state != SCIFDEV_RUNNING)
		goto ref_nack;

	/*
	 * If the p2p connection is already setup or in the process of setting up
	 * then just ignore this request. The requested node will get informed
	 * by SCIF_NODE_ADD_ACK or SCIF_NODE_ADD_NACK
	 */
	if (!list_empty(&dev_i->sd_p2p)) {
		list_for_each_safe(pos, tmp, &dev_i->sd_p2p) {
			p2p = list_entry(pos, struct scif_p2p_info, 
			ppi_list);
			if (p2p->ppi_peer_id == dev_j->sd_node) {
				mutex_unlock(&ms_info.mi_conflock);
				micscif_dec_node_refcnt(dev_i, 1);
				return;
			}
		}
	}

	p2p_ij = init_p2p_info(dev_i, dev_j);
	p2p_ji = init_p2p_info(dev_j, dev_i);

	list_add_tail(&p2p_ij->ppi_list, &dev_i->sd_p2p);
	list_add_tail(&p2p_ji->ppi_list, &dev_j->sd_p2p);

	/* Send a SCIF_NODE_ADD to dev_i, pass it its bus address
	 * as seen from dev_j
	 */
	msg->uop = SCIF_NODE_ADD;
	msg->src.node = dev_j->sd_node;
	msg->dst.node = dev_i->sd_node;

	p2p_ji->ppi_mic_addr[PPI_APER] = mic_map(msg->src.node - 1,
		p2p_ji->ppi_pa[PPI_APER],
		p2p_ji->ppi_len[PPI_APER] << PAGE_SHIFT);
	msg->payload[0] = p2p_ji->ppi_mic_addr[PPI_APER];

	/* addresses for node j */
	p2p_ij->ppi_mic_addr[PPI_MMIO] =  mic_map(msg->dst.node - 1,
		p2p_ij->ppi_pa[PPI_MMIO],
		p2p_ij->ppi_len[PPI_MMIO] << PAGE_SHIFT);
	msg->payload[1] = p2p_ij->ppi_mic_addr[PPI_MMIO];

	p2p_ij->ppi_mic_addr[PPI_APER] = mic_map(msg->dst.node - 1,
	p2p_ij->ppi_pa[PPI_APER],
	p2p_ij->ppi_len[PPI_APER] << PAGE_SHIFT);
	msg->payload[2] = p2p_ij->ppi_mic_addr[PPI_APER];

	msg->payload[3] = p2p_ij->ppi_len[PPI_APER] << PAGE_SHIFT;

	if ((err = micscif_nodeqp_send(dev_i,  msg, NULL))) {
		printk(KERN_ERR "%s %d error %d\n", __func__, __LINE__, err);
		goto ref_nack;
	}

	/* Same as above but to dev_j */
	msg->uop = SCIF_NODE_ADD;
	msg->src.node = dev_i->sd_node;
	msg->dst.node = dev_j->sd_node;

	tmppayload = msg->payload[0];
	msg->payload[0] = msg->payload[2];
	msg->payload[2] = tmppayload;

	p2p_ji->ppi_mic_addr[PPI_MMIO] = mic_map(msg->dst.node - 1, p2p_ji->ppi_pa[PPI_MMIO],
		p2p_ji->ppi_len[PPI_MMIO] << PAGE_SHIFT);
	msg->payload[1] = p2p_ji->ppi_mic_addr[PPI_MMIO];
	msg->payload[3] = p2p_ji->ppi_len[PPI_APER] << PAGE_SHIFT;

	if ((err = micscif_nodeqp_send(dev_j,  msg, NULL))) {
		printk(KERN_ERR "%s %d error %d\n", __func__, __LINE__, err);
		goto ref_nack;
	}

	mutex_unlock(&ms_info.mi_conflock);
	micscif_dec_node_refcnt(dev_i, 1);
	return;
ref_nack:
	micscif_dec_node_refcnt(dev_i, 1);
nack:
	mutex_unlock(&ms_info.mi_conflock);
	msg->uop = SCIF_NODE_CONNECT_NACK;
	msg->dst.node = dev_j->sd_node;
	msg->payload[0] = bid;
	if ((err = micscif_nodeqp_send(dev_j,  msg, NULL)))
		printk(KERN_ERR "%s %d error %d\n", __func__, __LINE__, err);
}
#endif /* SCIF */

#ifdef _MIC_SCIF_
/**
 * scif_node_connect_nack_resp: Respond to SCIF_NODE_CONNECT_NACK interrupt message
 * @msg:        Interrupt message
 *
 * Tell the node that initiated SCIF_NODE_CONNECT earlier has failed.
 */
static __always_inline void
scif_node_connect_nack_resp(struct micscif_dev *scifdev, struct nodemsg *msg)
{
	struct micscif_dev *peerdev;
	unsigned int bid = msg->payload[0];

	if (bid > MAX_BOARD_SUPPORTED) {
		printk(KERN_ERR "recieved a nack for invalid bid %d\n", bid);
		WARN_ON(1);
		return;
	}

	peerdev = &scif_dev[bid];
	mutex_lock(&peerdev->sd_lock);
	peerdev->sd_state = SCIFDEV_NOTPRESENT;
	mutex_unlock(&peerdev->sd_lock);
	wake_up(&peerdev->sd_p2p_wq);
}
#endif

/**
 * scif_node_add_nack_resp: Respond to SCIF_NODE_ADD_NACK interrupt message
 * @msg:        Interrupt message
 *
 * SCIF_NODE_ADD failed, so inform the waiting wq.
 */
static __always_inline void
scif_node_add_nack_resp(struct micscif_dev *scifdev, struct nodemsg *msg)
{
#ifndef _MIC_SCIF_
	struct micscif_dev *dst_dev = &scif_dev[msg->dst.node];
	pr_debug("SCIF_NODE_ADD_NACK recieved from %d \n", scifdev->sd_node);
	micscif_inc_node_refcnt(dst_dev, 1);
	micscif_nodeqp_send(dst_dev, msg, NULL);
	micscif_dec_node_refcnt(dst_dev, 1);
#else
	struct micscif_dev *peerdev;

	peerdev = &scif_dev[msg->src.node];

	if (peerdev->sd_state == SCIFDEV_NOTPRESENT)
		return;

	mutex_lock(&peerdev->sd_lock);
	peerdev->sd_state = SCIFDEV_NOTPRESENT;
	mutex_unlock(&peerdev->sd_lock);
	wake_up(&peerdev->sd_p2p_wq);
#endif
}

/**
 * scif_get_node_info_resp: Respond to SCIF_GET_NODE_INFO interrupt message
 * @msg:        Interrupt message
 *
 * Retrieve node info i.e maxid, total and node mask from the host.
 */
static __always_inline void
scif_get_node_info_resp(struct micscif_dev *scifdev, struct nodemsg *msg)
{
#ifdef _MIC_SCIF_
	struct get_node_info *node_info = (struct get_node_info *)msg->payload[3];

	mutex_lock(&ms_info.mi_conflock);
	ms_info.mi_mask = msg->payload[0];
	ms_info.mi_maxid = msg->payload[1];
	ms_info.mi_total = msg->payload[2];

	node_info->state = OP_COMPLETED;
	wake_up(&node_info->wq);
	mutex_unlock(&ms_info.mi_conflock);
#else
	swap(msg->dst.node, msg->src.node);
	mutex_lock(&ms_info.mi_conflock);
	msg->payload[0] = ms_info.mi_mask;
	msg->payload[1] = ms_info.mi_maxid;
	msg->payload[2] = ms_info.mi_total;
	mutex_unlock(&ms_info.mi_conflock);

	if (micscif_nodeqp_send(scifdev,  msg, NULL))
		printk(KERN_ERR "%s %d error \n", __func__, __LINE__);
#endif
}

#ifdef ENABLE_TEST
static void
scif_test(struct micscif_dev *scifdev, struct nodemsg *msg)
{
	if (msg->payload[0] != scifdev->count) {
		printk(KERN_ERR "Con fail: payload == %llx\n", msg->payload[0]);
		scifdev->test_done = -1;
	} else if (scifdev->count == TEST_LOOP) {
		pr_debug("Test success state %d!\n", scifdev->sd_state);
		scifdev->test_done = 1;
	}

	if (scifdev->test_done != 0) {
		while (scifdev->test_done != 2) {
			cpu_relax();
			schedule();
		}

		destroy_workqueue(scifdev->producer);
		destroy_workqueue(scifdev->consumer);
		pr_debug("Destroyed workqueue state %d!\n", scifdev->sd_state);
	}
	scifdev->count++;
}
#endif /* ENABLE_TEST */

static void
scif_msg_unknown(struct micscif_dev *scifdev, struct nodemsg *msg)
{
	/* Bogus Node Qp Message? */
	printk(KERN_ERR "Unknown message 0x%xn scifdev->sd_state 0x%x "
		"scifdev->sd_node 0x%x\n", 
		msg->uop, scifdev->sd_state, scifdev->sd_node);
	BUG_ON(1);
}

#ifdef _MIC_SCIF_
static void
smpt_set(struct micscif_dev *scifdev, struct nodemsg *msg)
{
	printk("msd recvd : smpt add\n");
	printk("dma_addr = 0x%llX, entry = 0x%llX\n", msg->payload[0], msg->payload[1]);
	mic_smpt_set(scif_dev->mm_sbox, msg->payload[0], msg->payload[1]);
}
#endif

void (*scif_intr_func[SCIF_MAX_MSG + 1])(struct micscif_dev *, struct nodemsg *msg) = {
	scif_msg_unknown,		// Error
	scif_init_resp,			// SCIF_INIT
	scif_exit_resp,			// SCIF_EXIT
	scif_nodeadd_resp,		// SCIF_NODE_ADD
	scif_nodeaddack_resp,		// SCIF_NODE_ADD_ACK
	scif_cnctreq_resp,		// SCIF_CNCT_REQ
	scif_cnctgnt_resp,		// SCIF_CNCT_GNT
	scif_cnctgntack_resp,		// SCIF_CNCT_GNTACK
	scif_cnctgntnack_resp,		// SCIF_CNCT_GNTNACK
	scif_cnctrej_resp,		// SCIF_CNCT_REJ
	scif_cnctterm_resp,		// SCIF_CNCT_TERM	10
	scif_termack_resp,		// SCIF_TERM_ACK
	scif_discnct_resp,		// SCIF_DISCNCT
	scif_discntack_resp,		// SCIF_DISCNT_ACK
	scif_recv_register,		// SCIF_REGISTER
	scif_recv_register_ack,		// SCIF_REGISTER_ACK
	scif_recv_register_nack,	// SCIF_REGISTER_NACK
	scif_recv_unregister,		// SCIF_UNREGISTER
	scif_recv_unregister_ack,	// SCIF_UNREGISTER_ACK
	scif_recv_unregister_nack,	// SCIF_UNREGISTER_NACK
	scif_alloc_req,			// SCIF_ALLOC_REQ	20
	scif_alloc_gnt_rej,		// SCIF_ALLOC_GNT
	scif_alloc_gnt_rej,		// SCIF_ALLOC_REJ
	scif_free_phys,			// SCIF_FREE_PHYS
	scif_free_virt,			// SCIF_FREE_VIRT
	scif_clientsend_resp,		// SCIF_CLIENT_SENT
	scif_clientrcvd_resp,		// SCIF_CLIENT_RCVD
	scif_recv_munmap,		// SCIF_MUNMAP
	scif_recv_mark,			// SCIF_MARK
	scif_recv_mark_resp,		// SCIF_MARK_ACK
	scif_recv_mark_resp,		// SCIF_MARK_NACK	30
	scif_recv_wait,			// SCIF_WAIT
	scif_recv_wait_resp,		// SCIF_WAIT_ACK
	scif_recv_wait_resp,		// SCIF_WAIT_NACK
	scif_recv_signal_local,		// SCIF_SIG_LOCAL
	scif_recv_signal_remote,	// SCIF_SIG_REMOTE
	scif_recv_signal_resp,		// SCIF_SIG_ACK
	scif_recv_signal_resp,		// SCIF_SIG_NACK
#ifdef _MIC_SCIF_
	scif_msg_unknown,
	scif_msg_unknown,
	scif_msg_unknown,
	scif_msg_unknown,
	scif_msg_unknown,		// SCIF_NODE_CREATE_DEP Not on card
	scif_msg_unknown,		// SCIF_NODE_DESTROY_DEP Not on card
#else
	scif_msg_unknown,
	scif_msg_unknown,
	scif_msg_unknown,
	scif_msg_unknown,
	scif_node_create_dep,		// SCIF_NODE_CREATE_DEP
	scif_node_destroy_dep,		// SCIF_NODE_DESTROY_DEP
#endif
	scif_node_remove,		// SCIF_NODE_REMOVE
#ifdef _MIC_SCIF_
	scif_msg_unknown,		// SCIF_NODE_REMOVE_ACK Not on card
	scif_msg_unknown,		// SCIF_NODE_WAKE_UP Not on card
#else
	scif_node_remove_ack,		// SCIF_NODE_REMOVE_ACK
	scif_node_wake_up,		// SCIF_NODE_WAKE_UP
#endif
	scif_node_wake_up_ack,		// SCIF_NODE_WAKE_UP_ACK
	scif_node_wake_up_nack,		// SCIF_NODE_WAKE_UP_NACK
#ifdef _MIC_SCIF_
	scif_node_alive_resp,		// SCIF_NODE_ALIVE
	scif_msg_unknown,		// SCIF_NODE_ALIVE_ACK not on card
	smpt_set,			// SMPT_SET
#else
	scif_msg_unknown,		// SCIF_NODE_ALIVE not on Host
	scif_node_alive_ack,		// SCIF_NODE_ALIVE_ACK
	scif_msg_unknown,		// SCIF_NODE_ALIVE not on Host
#endif
	scif_msg_unknown,
	scif_msg_unknown,
	scif_msg_unknown,
	scif_msg_unknown,
#ifdef _MIC_SCIF_
	scif_proxy_dma,			// SCIF_PROXY_DMA only for MIC
	scif_proxy_ordered_dma,		// SCIF_PROXY_ORDERED_DMA only for MIC
#else
	scif_msg_unknown,
	scif_msg_unknown,
#endif
#ifdef _MIC_SCIF_
	scif_msg_unknown,
	scif_node_connect_nack_resp,	//SCIF_NODE_CONNECT_NACK
#else
	scif_node_connect_resp,		//SCIF_NODE_CONNECT
	scif_msg_unknown,
#endif
	scif_node_add_nack_resp,	//SCIF_NODE_ADD_NACK
	scif_get_node_info_resp,	//SCIF_GET_NODE_INFO
#ifdef ENABLE_TEST
	scif_test			// SCIF_TEST
#else
	scif_msg_unknown
#endif
};

/**
 * scif_nodeqp_msg_hander() - Common handler for node messages
 * @scifdev: Remote device to respond to
 * @qp: Remote memory pointer
 * @msg: The message to be handled.
 *
 * This routine calls the appriate routine to handle a Node Qp message receipt.
 */
int micscif_max_msg_id = SCIF_MAX_MSG;

static void
micscif_nodeqp_msg_handler(struct micscif_dev *scifdev, struct micscif_qp *qp, struct nodemsg *msg)
{
	micscif_display_message(scifdev, msg, "Rcvd");

	if (msg->uop > (uint32_t)micscif_max_msg_id) {
		/* Bogus Node Qp Message? */
		printk(KERN_ERR "Unknown message 0x%xn scifdev->sd_state 0x%x "
			"scifdev->sd_node 0x%x\n", 
			msg->uop, scifdev->sd_state, scifdev->sd_node);
		BUG_ON(1);
	}

	scif_intr_func[msg->uop](scifdev, msg);
}

/**
 * scif_nodeqp_intrhander() - Interrupt handler for node messages
 * @scifdev:    Remote device to respond to
 * @qp:         Remote memory pointer
 *
 * This routine is triggered by the interrupt mechanism.  It reads
 * messages from the node queue RB and calls the Node QP Message handling
 * routine.
 */
int
micscif_nodeqp_intrhandler(struct micscif_dev *scifdev, struct micscif_qp *qp)
{
	struct nodemsg msg;
	int read_size;

	do {
#ifndef _MIC_SCIF_
		if (qp->blast) {
			scif_wakeup_ep(SCIF_WAKE_UP_RECV);
			qp->blast = 0;
		}
#endif
		if (SCIFDEV_STOPPED == scifdev->sd_state)
			return 0;
		read_size = micscif_rb_get_next(&qp->inbound_q, &msg,
							sizeof(msg));
		/* Stop handling messages if an oops is in progress */
		if (read_size != sizeof(msg) || oops_in_progress)
			break;
#ifndef _MIC_SCIF_
		atomic_set(&scifdev->sd_node_alive, 1);
#endif

		micscif_inc_node_refcnt(scifdev, 1);
		micscif_nodeqp_msg_handler(scifdev, qp, &msg);
		/*
		 * The reference count is reset to SCIF_NODE_IDLE
		 * during scif device cleanup so decrementing the
		 * reference count further is not required.
		 */
		if (SCIFDEV_INIT == scifdev->sd_state)
			return 0;
		if (SCIFDEV_STOPPED == scifdev->sd_state) {
			micscif_dec_node_refcnt(scifdev, 1);
			return 0;
		}
		micscif_rb_update_read_ptr(&qp->inbound_q);
		micscif_dec_node_refcnt(scifdev, 1);
	} while (read_size == sizeof(msg));
#ifdef _MIC_SCIF_
	/*
	 * Keep polling the Node QP RB in case there are active SCIF
	 * P2P connections to provide better Node QP responsiveness
	 * in anticipation of P2P Proxy DMA requests for performance.
	 */
	if (scifdev->sd_proxy_dma_reads &&
		scifdev->num_active_conn &&
		SCIFDEV_STOPPED != scifdev->sd_state) {
		queue_work(scifdev->sd_intr_wq, &scifdev->sd_intr_bh);
		schedule();
	}
#endif
	return read_size;
}

/**
 * micscif_loopb_wq_handler - Loopback Workqueue Handler.
 * @work: loop back work
 *
 * This work queue routine is invoked by the loopback work queue handler.
 * It grabs the recv lock, dequeues any available messages from the head
 * of the loopback message list, calls the node QP message handler,
 * waits for it to return, then frees up this message and dequeues more
 * elements of the list if available.
 */
static void micscif_loopb_wq_handler(struct work_struct *work)
{
	struct micscif_dev *scifdev =
		container_of(work, struct micscif_dev, sd_loopb_work);
	struct micscif_qp *qp = micscif_nodeqp_nextmsg(scifdev);
	struct loopb_msg *msg;

	do {
		msg = NULL;
		spin_lock(&qp->qp_recv_lock);
		if (!list_empty(&scifdev->sd_loopb_recv_q)) {
			msg = list_first_entry(&scifdev->sd_loopb_recv_q, 
					struct loopb_msg, list_member);
			list_del(&msg->list_member);
		}
		spin_unlock(&qp->qp_recv_lock);

		if (msg) {
			micscif_nodeqp_msg_handler(scifdev, qp, &msg->msg);
			kfree(msg);
		}
	} while (msg);
}

/**
 * micscif_loopb_msg_handler() - Workqueue handler for loopback messages.
 * @scifdev: SCIF device
 * @qp: Queue pair.
 *
 * This work queue routine is triggered when a loopback message is received.
 *
 * We need special handling for receiving Node Qp messages on a loopback SCIF
 * device via two workqueues for receiving messages.
 *
 * The reason we need the extra workqueue which is not required with *normal*
 * non-loopback SCIF devices is the potential classic deadlock described below:
 *
 * Thread A tries to send a message on a loopback SCIF devide and blocks since
 * there is no space in the RB while it has the qp_send_lock held or another
 * lock called lock X for example.
 *
 * Thread B: The Loopback Node QP message receive workqueue receives the message
 * and tries to send a message (eg an ACK) to the loopback SCIF device. It tries
 * to grab the send lock again or lock X and deadlocks with Thread A. The RB
 * cannot be drained any further due to this classic deadlock.
 *
 * In order to avoid deadlocks as mentioned above we have an extra level of
 * indirection achieved by having two workqueues.
 * 1) The first workqueue whose handler is micscif_loopb_msg_handler reads
 * messages from the Node QP RB, adds them to a list and queues work for the
 * second workqueue.
 *
 * 2) The second workqueue whose handler is micscif_loopb_wq_handler dequeues
 * messages from the list, handles them, frees up the memory and dequeues
 * more elements from the list if possible.
 */
int
micscif_loopb_msg_handler(struct micscif_dev *scifdev, struct micscif_qp *qp)
{
	int read_size;
	struct loopb_msg *msg;

	do {
		if (!(msg = kmalloc(sizeof(struct loopb_msg), GFP_KERNEL))) {
			printk(KERN_ERR "%s %d ENOMEM\n", __func__, __LINE__);
			return -ENOMEM;
		}

		read_size = micscif_rb_get_next(&qp->inbound_q, &msg->msg,
				sizeof(struct nodemsg));

		if (read_size != sizeof(struct nodemsg)) {
			kfree(msg);
			micscif_rb_update_read_ptr(&qp->inbound_q);
			break;
		}

		spin_lock(&qp->qp_recv_lock);
		list_add_tail(&msg->list_member, &scifdev->sd_loopb_recv_q);
		spin_unlock(&qp->qp_recv_lock);
		queue_work(scifdev->sd_loopb_wq, &scifdev->sd_loopb_work);
		micscif_rb_update_read_ptr(&qp->inbound_q);
	} while (read_size == sizeof(struct nodemsg));
	return read_size;
}

/**
 * micscif_setup_loopback_qp - One time setup work for Loopback Node Qp.
 * @scifdev: SCIF device
 *
 * Sets up the required loopback workqueues, queue pairs, ring buffers
 * and also tests out the Queue Pairs.
 */
int micscif_setup_loopback_qp(struct micscif_dev *scifdev)
{
	int err = 0;
	void *local_q;
	struct micscif_qp *qp;

	/* Set up the work queues */
	if ((err = micscif_setup_interrupts(scifdev)))
		goto error;

	INIT_LIST_HEAD(&scifdev->sd_loopb_recv_q);
	snprintf(scifdev->sd_loopb_wqname, sizeof(scifdev->sd_loopb_wqname),
			"SCIF LOOPB %d", scifdev->sd_node);
	if (!(scifdev->sd_loopb_wq =
		__mic_create_singlethread_workqueue(scifdev->sd_loopb_wqname))){
		err = -ENOMEM;
		goto destroy_intr_wq;
	}
	INIT_WORK(&scifdev->sd_loopb_work, micscif_loopb_wq_handler);
	/* Allocate Self Qpair */
	scifdev->n_qpairs = 1;
	scifdev->qpairs = (struct micscif_qp *)kzalloc(sizeof(struct micscif_qp), GFP_KERNEL);
	if (!scifdev->qpairs) {
		printk(KERN_ERR "Node QP Allocation failed\n");
		err = -ENOMEM;
		goto destroy_loopb_wq;
	}

	qp = scifdev->qpairs;
	qp->magic = SCIFEP_MAGIC;
	spin_lock_init(&qp->qp_send_lock);
	spin_lock_init(&qp->qp_recv_lock);
	init_waitqueue_head(&scifdev->sd_mmap_wq);

	local_q = kzalloc(NODE_QP_SIZE, GFP_KERNEL);
	if (!local_q) {
		printk(KERN_ERR "Ring Buffer Allocation Failed\n");
		err = -ENOMEM;
		goto free_qpairs;
	}

	/*
	 * For loopback the inbound_q and outbound_q are essentially the same
	 * since the Node sends a message on the loopback interface to the
	 * outbound_q which is then received on the inbound_q.
	 */
	micscif_rb_init(&qp->outbound_q,
			&(scifdev->qpairs[0].local_read),
			&(scifdev->qpairs[0].local_write),
			local_q,
			NODE_QP_SIZE);

	micscif_rb_init(&(qp->inbound_q),
			&(scifdev->qpairs[0].local_read),
			&(scifdev->qpairs[0].local_write),
			local_q,
			NODE_QP_SIZE);

	/* Launch the micscif_rb test */
#ifdef ENABLE_TEST
	micscif_qp_testboth(scifdev);
#endif
	return err;
free_qpairs:
	kfree(scifdev->qpairs);
destroy_loopb_wq:
	destroy_workqueue(scifdev->sd_loopb_wq);
destroy_intr_wq:
	destroy_workqueue(scifdev->sd_intr_wq);
error:
	return err;
}

/**
 * micscif_destroy_loopback_qp - One time uninit work for Loopback Node Qp
 * @scifdev: SCIF device
 *
 * Detroys the workqueues and frees up the Ring Buffer and Queue Pair memory.
 */
int micscif_destroy_loopback_qp(struct micscif_dev *scifdev)
{
	micscif_destroy_interrupts(scifdev);
	destroy_workqueue(scifdev->sd_loopb_wq);
	kfree((void *)scifdev->qpairs->outbound_q.rb_base);
	kfree(scifdev->qpairs);
	return 0;
}

#ifndef _MIC_SCIF_
void micscif_destroy_p2p(mic_ctx_t *mic_ctx)
{
	mic_ctx_t * mic_ctx_peer;
	struct micscif_dev *mic_scif_dev;
	struct micscif_dev *peer_dev;
	struct scif_p2p_info *p2p;
	struct list_head *pos, *tmp;
	uint32_t bd;

	if (!mic_p2p_enable)
		return;


	/* FIXME: implement node deletion */
	mic_scif_dev = &scif_dev[mic_get_scifnode_id(mic_ctx)];

	/* Free P2P mappings in the given node for all its peer nodes */
	list_for_each_safe(pos, tmp, &mic_scif_dev->sd_p2p) {
		p2p = list_entry(pos, struct scif_p2p_info, 
				ppi_list);

		mic_unmap(mic_ctx->bi_id, p2p->ppi_mic_addr[PPI_MMIO],
			p2p->ppi_len[PPI_MMIO] << PAGE_SHIFT);
		mic_unmap(mic_ctx->bi_id, p2p->ppi_mic_addr[PPI_APER],
			p2p->ppi_len[PPI_APER] << PAGE_SHIFT);
		pci_unmap_sg(mic_ctx->bi_pdev, 
			p2p->ppi_sg[PPI_MMIO], p2p->sg_nentries[PPI_MMIO], PCI_DMA_BIDIRECTIONAL);
		micscif_p2p_freesg(p2p->ppi_sg[PPI_MMIO]);
		pci_unmap_sg(mic_ctx->bi_pdev, 
			p2p->ppi_sg[PPI_APER], p2p->sg_nentries[PPI_APER], PCI_DMA_BIDIRECTIONAL);
		micscif_p2p_freesg(p2p->ppi_sg[PPI_APER]);
		list_del(pos);
		kfree(p2p);
	}

	/* Free P2P mapping created in the peer nodes for the given node */
	for (bd = SCIF_HOST_NODE + 1; bd <= ms_info.mi_maxid; bd++) {
		peer_dev = &scif_dev[bd];

		list_for_each_safe(pos, tmp, &peer_dev->sd_p2p) {
			p2p = list_entry(pos, struct scif_p2p_info, 
					ppi_list);
			if (p2p->ppi_peer_id == mic_get_scifnode_id(mic_ctx)) {

				mic_ctx_peer = get_per_dev_ctx(peer_dev->sd_node - 1);
				mic_unmap(mic_ctx_peer->bi_id, p2p->ppi_mic_addr[PPI_MMIO],
					p2p->ppi_len[PPI_MMIO] << PAGE_SHIFT);
				mic_unmap(mic_ctx_peer->bi_id, p2p->ppi_mic_addr[PPI_APER],
					p2p->ppi_len[PPI_APER] << PAGE_SHIFT);
				pci_unmap_sg(mic_ctx_peer->bi_pdev, 
					p2p->ppi_sg[PPI_MMIO], p2p->sg_nentries[PPI_MMIO], PCI_DMA_BIDIRECTIONAL);
				micscif_p2p_freesg(p2p->ppi_sg[PPI_MMIO]);
				pci_unmap_sg(mic_ctx_peer->bi_pdev, p2p->ppi_sg[PPI_APER], 
					p2p->sg_nentries[PPI_APER], PCI_DMA_BIDIRECTIONAL);
				micscif_p2p_freesg(p2p->ppi_sg[PPI_APER]);
				list_del(pos);
				kfree(p2p);
			}
		}
	}
}
#endif

/**
 * ONLY TEST CODE BELOW
 */
#ifdef ENABLE_TEST
#include <linux/sched.h>
#include <linux/workqueue.h>
#include "mic/micscif_nodeqp.h"

static void micscif_rb_trigger_consumer(struct work_struct *work)
{
	struct micscif_dev *scifdev = container_of(work, struct micscif_dev, consumer_work);

	while (scifdev->test_done == 0) {
		cpu_relax();
		schedule();
	}
	if (scifdev->test_done != 1)
		printk(KERN_ERR "Consumer failed!\n");
	else
		pr_debug("Test finished: Success\n");
	scifdev->test_done = 2;
}

/**
 * micscif_rb_trigger_producer
 * This is the producer thread to create messages and update the
 * RB write offset accordingly.
 */
static void micscif_rb_trigger_producer(struct work_struct *work)
{
	struct nodemsg msg;
	int count = 0;
	struct micscif_dev *scifdev = container_of(work, struct micscif_dev, producer_work);

	msg.dst.node = scifdev->sd_node;
	msg.uop = SCIF_TEST;

	while (count <= TEST_LOOP) {
		msg.payload[0] = count++;
		micscif_nodeqp_send(scifdev, &msg, NULL);
		/* pr_debug(("Prod payload %llu\n", msg.payload[0]); */
	}
}

/* this is called from the host and the card at the same time on a queue pair.
 * Each sets up a producer and a consumer and spins on the queue pair until done
 */
static void micscif_qp_testboth(struct micscif_dev *scifdev)
{
	scifdev->count = 0;
	scifdev->test_done = 0;
	snprintf(scifdev->producer_name, sizeof(scifdev->producer_name),
		 "PRODUCER %d", scifdev->sd_node);
	snprintf(scifdev->consumer_name, sizeof(scifdev->consumer_name),
		 "CONSUMER %d", scifdev->sd_node);
	scifdev->producer =
		__mic_create_singlethread_workqueue(scifdev->producer_name);
	scifdev->consumer =
		__mic_create_singlethread_workqueue(scifdev->consumer_name);

	INIT_WORK(&scifdev->producer_work, micscif_rb_trigger_producer);
	INIT_WORK(&scifdev->consumer_work, micscif_rb_trigger_consumer);

	queue_work(scifdev->producer, &scifdev->producer_work);
	queue_work(scifdev->consumer, &scifdev->consumer_work);
}
#endif
