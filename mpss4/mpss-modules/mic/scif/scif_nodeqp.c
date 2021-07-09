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
 * Intel SCIF driver.
 */
#include "../bus/scif_bus.h"
#include "scif_peer_bus.h"
#include "scif_main.h"
#include "scif_nodeqp.h"
#include "scif_map.h"

#define NODEQP_USECS_DELAY 10
#define NODEQP_TOTAL_SECS_DELAY 3
#define NODEQP_DELAY_LOOPS (NODEQP_TOTAL_SECS_DELAY * 1000000 / NODEQP_USECS_DELAY)

/*
 ************************************************************************
 * SCIF node Queue Pair (QP) setup flow:
 *
 * 1) SCIF driver gets probed with a scif_hw_dev via the scif_hw_bus
 * 2) scif_setup_qp(..) allocates the local qp and calls
 *	scif_setup_qp_connect(..) which allocates and maps the local
 *	buffer for the inbound QP
 * 3) The local node updates the device page with the DMA address of the QP
 * 4) A delayed work is scheduled (qp_dwork) which periodically reads if
 *	the peer node has updated its QP DMA address
 * 5) Once a valid non zero address is found in the QP DMA address field
 *	in the device page, the local node maps the remote node's QP,
 *	updates its outbound QP and sends a SCIF_INIT message to the peer
 * 6) The SCIF_INIT message is received by the peer node QP interrupt bottom
 *	half handler by calling scif_init(..)
 * 7) scif_init(..) registers a new SCIF peer node by calling
 *	scif_peer_register_device(..) which signifies the addition of a new
 *	SCIF node
 * 8) On the mgmt node, P2P network setup/teardown is initiated if all the
 *	remote nodes are online via scif_p2p_setup(..)
 * 9) For P2P setup, the host maps the remote nodes' aperture and memory
 *	bars and sends a SCIF_NODE_ADD message to both nodes
 * 10) As part of scif_nodeadd, both nodes set up their local inbound
 *	QPs and send a SCIF_NODE_ADD_ACK to the mgmt node
 * 11) As part of scif_node_add_ack(..) the mgmt node forwards the
 *	SCIF_NODE_ADD_ACK to the remote nodes
 * 12) As part of scif_node_add_ack(..) the remote nodes update their
 *	outbound QPs, make sure they can access memory on the remote node
 *	and then add a new SCIF peer node by calling
 *	scif_peer_register_device(..) which signifies the addition of a new
 *	SCIF node.
 * 13) The SCIF network is now established across all nodes.
 *
 ************************************************************************
 * SCIF node QP teardown flow (initiated by non mgmt node):
 *
 * 1) SCIF driver gets a remove callback with a scif_hw_dev via the scif_hw_bus
 * 2) The device page QP DMA address field is updated with 0x0
 * 3) A non mgmt node now cleans up all local data structures and sends a
 *	SCIF_EXIT message to the peer and waits for a SCIF_EXIT_ACK
 * 4) As part of scif_exit(..) handling scif_disconnect_node(..) is called
 * 5) scif_disconnect_node(..) sends a SCIF_NODE_REMOVE message to all the
 *	peers and waits for a SCIF_NODE_REMOVE_ACK
 * 6) As part of scif_node_remove(..) a remote node unregisters the peer
 *	node from the SCIF network and sends a SCIF_NODE_REMOVE_ACK
 * 7) When the mgmt node has received all the SCIF_NODE_REMOVE_ACKs
 *	it sends itself a node remove message whose handling cleans up local
 *	data structures and unregisters the peer node from the SCIF network
 * 8) The mgmt node sends a SCIF_EXIT_ACK
 * 9) Upon receipt of the SCIF_EXIT_ACK the node initiating the teardown
 *	completes the SCIF remove routine
 * 10) The SCIF network is now torn down for the node initiating the
 *	teardown sequence
 *
 ************************************************************************
 * SCIF node QP teardown flow (initiated by mgmt node):
 *
 * 1) SCIF driver gets a remove callback with a scif_hw_dev via the scif_hw_bus
 * 2) The device page QP DMA address field is updated with 0x0
 * 3) The mgmt node calls scif_disconnect_node(..)
 * 4) scif_disconnect_node(..) sends a SCIF_NODE_REMOVE message to all the peers
 *	and waits for a SCIF_NODE_REMOVE_ACK
 * 5) As part of scif_node_remove(..) a remote node unregisters the peer
 *	node from the SCIF network and sends a SCIF_NODE_REMOVE_ACK
 * 6) When the mgmt node has received all the SCIF_NODE_REMOVE_ACKs
 *	it unregisters the peer node from the SCIF network
 * 7) The mgmt node sends a SCIF_EXIT message and waits for a SCIF_EXIT_ACK.
 * 8) A non mgmt node upon receipt of a SCIF_EXIT message calls scif_stop(..)
 *	which would clean up local data structures for all SCIF nodes and
 *	then send a SCIF_EXIT_ACK back to the mgmt node
 * 9) Upon receipt of the SCIF_EXIT_ACK the the mgmt node sends itself a node
 *	remove message whose handling cleans up local data structures and
 *	destroys any P2P mappings.
 * 10) The SCIF hardware device for which a remove callback was received is now
 *	disconnected from the SCIF network.
 */
/*
 * Initializes "local" data structures for the QP. Allocates the QP
 * ring buffer (rb) and initializes the "in bound" queue.
 */
int scif_setup_qp_connect(struct scif_qp *qp, dma_addr_t *qp_offset,
			  int local_size, struct scif_dev *scifdev)
{
	void *local_q = qp->inbound_q.rb_base;
	int err = 0;
	u32 tmp_rd = 0;

	spin_lock_init(&qp->send_lock);
	spin_lock_init(&qp->recv_lock);

	/* Allocate rb only if not already allocated */
	if (!local_q) {
		local_q = kzalloc(local_size, GFP_KERNEL);
		if (!local_q) {
			err = -ENOMEM;
			return err;
		}
	}

	err = scif_map_single(&qp->local_buf, local_q, scifdev, local_size);
	if (err)
		goto kfree;
	/*
	 * To setup the inbound_q, the buffer lives locally, the read pointer
	 * is remote and the write pointer is local.
	 */
	scif_rb_init(&qp->inbound_q,
		     &tmp_rd,
		     &qp->local_write,
		     local_q, get_count_order(local_size));
	/*
	 * The read pointer is NULL initially and it is unsafe to use the ring
	 * buffer til this changes!
	 */
	qp->inbound_q.read_ptr = NULL;
	err = scif_map_single(qp_offset, qp,
			      scifdev, sizeof(struct scif_qp));
	if (err)
		goto unmap;
	qp->local_qp = *qp_offset;
	return err;
unmap:
	scif_unmap_single(qp->local_buf, scifdev, local_size);
	qp->local_buf = 0;
kfree:
	kfree(local_q);
	return err;
}

/* When the other side has already done it's allocation, this is called */
int scif_setup_qp_accept(struct scif_qp *qp, dma_addr_t *qp_offset,
			 dma_addr_t phys, int local_size,
			 struct scif_dev *scifdev)
{
	void *local_q;
	void *remote_q;
	struct scif_qp *remote_qp;
	int remote_size;
	int err = 0;

	spin_lock_init(&qp->send_lock);
	spin_lock_init(&qp->recv_lock);
	/* Start by figuring out where we need to point */
	remote_qp = scif_ioremap(phys, sizeof(struct scif_qp), scifdev);
	if (!remote_qp)
		return -EIO;
	qp->remote_qp = remote_qp;
	if (qp->remote_qp->magic != SCIFEP_MAGIC) {
		err = -EIO;
		goto iounmap;
	}
	qp->remote_buf = remote_qp->local_buf;
	remote_size = qp->remote_qp->inbound_q.size;
	remote_q = scif_ioremap(qp->remote_buf, remote_size, scifdev);
	if (!remote_q) {
		err = -EIO;
		goto iounmap;
	}
	qp->remote_qp->local_write = 0;
	/*
	 * To setup the outbound_q, the buffer lives in remote memory,
	 * the read pointer is local, the write pointer is remote
	 */
	scif_rb_init(&qp->outbound_q,
		     &qp->local_read,
		     &qp->remote_qp->local_write,
		     remote_q,
		     get_count_order(remote_size));
	local_q = kzalloc(local_size, GFP_KERNEL);
	if (!local_q) {
		err = -ENOMEM;
		goto iounmap_1;
	}
	err = scif_map_single(&qp->local_buf, local_q, scifdev, local_size);
	if (err)
		goto kfree;
	qp->remote_qp->local_read = 0;
	/*
	 * To setup the inbound_q, the buffer lives locally, the read pointer
	 * is remote and the write pointer is local
	 */
	scif_rb_init(&qp->inbound_q,
		     &qp->remote_qp->local_read,
		     &qp->local_write,
		     local_q, get_count_order(local_size));
	err = scif_map_single(qp_offset, qp, scifdev,
			      sizeof(struct scif_qp));
	if (err)
		goto unmap;
	qp->local_qp = *qp_offset;
	return err;
unmap:
	scif_unmap_single(qp->local_buf, scifdev, local_size);
	qp->local_buf = 0;
kfree:
	kfree(local_q);
iounmap_1:
	scif_iounmap(remote_q, remote_size, scifdev);
	qp->outbound_q.rb_base = NULL;
iounmap:
	scif_iounmap(qp->remote_qp, sizeof(struct scif_qp), scifdev);
	qp->remote_qp = NULL;
	return err;
}

int scif_setup_qp_connect_response(struct scif_dev *scifdev,
				   struct scif_qp *qp, u64 payload)
{
	int err = 0;
	void *r_buf;
	int remote_size;
	phys_addr_t tmp_phys;

	qp->remote_qp = scif_ioremap(payload, sizeof(struct scif_qp), scifdev);

	if (!qp->remote_qp) {
		err = -ENOMEM;
		goto error;
	}

	if (qp->remote_qp->magic != SCIFEP_MAGIC) {
		dev_err(&scifdev->sdev->dev,
			"SCIFEP_MAGIC mismatch between self %d remote %d\n",
			scif_dev[scif_info.nodeid].node, scifdev->node);
		err = -ENODEV;
		goto error;
	}

	tmp_phys = qp->remote_qp->local_buf;
	remote_size = qp->remote_qp->inbound_q.size;
	r_buf = scif_ioremap(tmp_phys, remote_size, scifdev);

	if (!r_buf)
		return -EIO;

	qp->local_read = 0;
	scif_rb_init(&qp->outbound_q,
		     &qp->local_read,
		     &qp->remote_qp->local_write,
		     r_buf,
		     get_count_order(remote_size));
	/*
	 * Because the node QP may already be processing an INIT message, set
	 * the read pointer so the cached read offset isn't lost
	 */
	qp->remote_qp->local_read = qp->inbound_q.current_read_offset;
	/*
	 * resetup the inbound_q now that we know where the
	 * inbound_read really is.
	 */
	scif_rb_init(&qp->inbound_q,
		     &qp->remote_qp->local_read,
		     &qp->local_write,
		     qp->inbound_q.rb_base,
		     get_count_order(qp->inbound_q.size));
error:
	return err;
}

static __always_inline void
scif_send_msg_intr(struct scif_dev *scifdev)
{
	struct scif_hw_dev *sdev = scifdev->sdev;

	if (scifdev_is_p2p(scifdev))
		sdev->hw_ops->send_p2p_intr(sdev, scifdev->rdb, &scifdev->mmio);
	else
		sdev->hw_ops->send_intr(sdev, scifdev->rdb);
}

int scif_qp_response(phys_addr_t phys, struct scif_dev *scifdev)
{
	int err = 0;
	struct scifmsg msg;

	err = scif_setup_qp_connect_response(scifdev, scifdev->qpairs, phys);
	if (!err) {
		/*
		 * Now that everything is setup and mapped, we're ready
		 * to tell the peer about our queue's location
		 */
		msg.uop = SCIF_INIT;
		msg.dst.node = scifdev->node;
		err = scif_nodeqp_send(scifdev, &msg);
	}
	return err;
}

void scif_send_exit(struct scif_dev *scifdev)
{
	struct scifmsg msg;
	int ret;
	long timeout;

	scifdev->exit = OP_IN_PROGRESS;
	msg.uop = SCIF_EXIT;
	msg.src.node = scif_info.nodeid;
	msg.dst.node = scifdev->node;
	ret = scif_nodeqp_send(scifdev, &msg);
	if (ret) {
		log_scif_warn(scifdev->node, "scif_nodeqp_send failed");
		goto done;
	}

	/* Wait for a SCIF_EXIT_ACK message */
	timeout = wait_event_timeout(scif_info.exitwq,
				     scifdev->exit == OP_COMPLETED,
				     SCIF_NODE_ALIVE_TIMEOUT);

	log_scif_info(scifdev->node, "timeout %ld", timeout);
	if (!timeout)
		log_scif_warn(scifdev->node, "Did not receive SCIF_EXIT_ACK");

done:
	scifdev->exit = OP_IDLE;
}

int scif_setup_qp(struct scif_dev *scifdev)
{
	int err = 0;
	int local_size;
	struct scif_qp *qp;

	local_size = SCIF_NODE_QP_SIZE;

	qp = kzalloc(sizeof(*qp), GFP_KERNEL);
	if (!qp) {
		err = -ENOMEM;
		return err;
	}
	qp->magic = SCIFEP_MAGIC;
	scifdev->qpairs = qp;
	err = scif_setup_qp_connect(qp, &scifdev->qp_dma_addr,
				    local_size, scifdev);
	if (err)
		goto free_qp;
	/*
	 * We're as setup as we can be. The inbound_q is setup, w/o a usable
	 * outbound q.  When we get a message, the read_ptr will be updated,
	 * and we will pull the message.
	 */
	return err;
free_qp:
	kfree(scifdev->qpairs);
	scifdev->qpairs = NULL;
	return err;
}

/* Init p2p mappings required to access peerdev from scifdev */
static struct scif_p2p_info *
scif_init_p2p_info(struct scif_dev *scifdev, struct scif_dev *peerdev)
{
	struct scif_hw_dev *sdev = scifdev->sdev;
	struct scif_hw_dev *psdev = peerdev->sdev;
	struct scif_p2p_info *p2p;
	dma_addr_t mmio_dma, aper_dma;

	p2p = kzalloc(sizeof(*p2p), GFP_KERNEL);
	if (!p2p)
		return NULL;

	mmio_dma = sdev->hw_ops->map_peer_resource(sdev, psdev->mmio->pa,
						   psdev->mmio->len);
	if (!mmio_dma)
		goto free_p2p;

	aper_dma = sdev->hw_ops->map_peer_resource(sdev, psdev->aper->pa,
						   psdev->aper->len);
	if (!aper_dma)
		goto mmio_unmap;

	p2p->ppi_da[SCIF_PPI_MMIO] = mmio_dma;
	p2p->ppi_len[SCIF_PPI_MMIO] = psdev->mmio->len;
	p2p->ppi_da[SCIF_PPI_APER] = aper_dma;
	p2p->ppi_len[SCIF_PPI_APER] = psdev->aper->len;

	p2p->ppi_peer_id = peerdev->node;
	return p2p;

mmio_unmap:
	sdev->hw_ops->unmap_peer_resource(sdev, psdev->mmio->pa,
					  psdev->mmio->len);
free_p2p:
	kfree(p2p);
	return NULL;
}

/* Uninitialize and release resources from a p2p mapping */
static void scif_deinit_p2p_info(struct scif_dev *scifdev,
				 struct scif_p2p_info *p2p)
{
	struct scif_hw_dev *sdev = scifdev->sdev;

	sdev->hw_ops->unmap_peer_resource(sdev, p2p->ppi_da[SCIF_PPI_MMIO],
					  p2p->ppi_len[SCIF_PPI_MMIO]);
	sdev->hw_ops->unmap_peer_resource(sdev, p2p->ppi_da[SCIF_PPI_APER],
					  p2p->ppi_len[SCIF_PPI_APER]);
	kfree(p2p);
}

/**
 * scif_node_connect: Respond to SCIF_NODE_CONNECT interrupt message
 * @dst: Destination node
 *
 * Connect the src and dst node by setting up the p2p connection
 * between them. Management node here acts like a proxy.
 */
static void scif_node_connect(struct scif_dev *scifdev, int dst)
{
	struct scif_dev *dev_j = scifdev;
	struct scif_dev *dev_i = NULL;
	struct scif_p2p_info *p2p_ij = NULL;    /* bus addr for j from i */
	struct scif_p2p_info *p2p_ji = NULL;    /* bus addr for i from j */
	struct scif_p2p_info *p2p;
	struct list_head *pos, *tmp;
	struct scifmsg msg;
	int err;
	u64 tmppayload;

	if (dst < 1 || dst > scif_info.maxid)
		return;

	dev_i = &scif_dev[dst];

	if (!_scifdev_alive(dev_i))
		return;
	/*
	 * If the p2p connection is already setup or in the process of setting
	 * up then just ignore this request. The requested node will get
	 * informed by SCIF_NODE_ADD_ACK or SCIF_NODE_ADD_NACK
	 */
	if (!list_empty(&dev_i->p2p)) {
		list_for_each_safe(pos, tmp, &dev_i->p2p) {
			p2p = list_entry(pos, struct scif_p2p_info, ppi_list);
			if (p2p->ppi_peer_id == dev_j->node)
				return;
		}
	}
	p2p_ij = scif_init_p2p_info(dev_i, dev_j);
	if (!p2p_ij)
		return;
	p2p_ji = scif_init_p2p_info(dev_j, dev_i);
	if (!p2p_ji) {
		scif_deinit_p2p_info(dev_i, p2p_ij);
		return;
	}
	list_add_tail(&p2p_ij->ppi_list, &dev_i->p2p);
	list_add_tail(&p2p_ji->ppi_list, &dev_j->p2p);

	/*
	 * Send a SCIF_NODE_ADD to dev_i, pass it its bus address
	 * as seen from dev_j
	 */
	msg.uop = SCIF_NODE_ADD;
	msg.src.node = dev_j->node;
	msg.dst.node = dev_i->node;

	msg.payload[0] = p2p_ji->ppi_da[SCIF_PPI_APER];
	msg.payload[1] = p2p_ij->ppi_da[SCIF_PPI_MMIO];
	msg.payload[2] = p2p_ij->ppi_da[SCIF_PPI_APER];
	msg.payload[3] = p2p_ij->ppi_len[SCIF_PPI_APER];

	err = scif_nodeqp_send(dev_i,  &msg);
	if (err) {
		dev_err(&scifdev->sdev->dev,
			"%s %d error %d\n", __func__, __LINE__, err);
		return;
	}

	/* Same as above but to dev_j */
	msg.uop = SCIF_NODE_ADD;
	msg.src.node = dev_i->node;
	msg.dst.node = dev_j->node;

	tmppayload = msg.payload[0];
	msg.payload[0] = msg.payload[2];
	msg.payload[2] = tmppayload;
	msg.payload[1] = p2p_ji->ppi_da[SCIF_PPI_MMIO];
	msg.payload[3] = p2p_ji->ppi_len[SCIF_PPI_APER];

	scif_nodeqp_send(dev_j, &msg);
}

static void scif_p2p_setup(void)
{
	int i, j;

	if (!scif_info.p2p_enable)
		return;

	for (i = 1; i <= scif_info.maxid; i++)
		if (!_scifdev_alive(&scif_dev[i]))
			return;

	for (i = 1; i <= scif_info.maxid; i++) {
		for (j = 1; j <= scif_info.maxid; j++) {
			struct scif_dev *scifdev = &scif_dev[i];

			if (i == j)
				continue;
			scif_node_connect(scifdev, j);
		}
	}
}

static char *message_types[SCIF_MAX_MSG + 1] = {
#define SCIF_MSG_TYPE_STR(msg) [msg] = #msg
	SCIF_MSG_TYPE_STR(SCIF_INIT),
	SCIF_MSG_TYPE_STR(SCIF_EXIT),
	SCIF_MSG_TYPE_STR(SCIF_EXIT_ACK),
	SCIF_MSG_TYPE_STR(SCIF_NODE_ADD),
	SCIF_MSG_TYPE_STR(SCIF_NODE_ADD_ACK),
	SCIF_MSG_TYPE_STR(SCIF_NODE_ADD_NACK),
	SCIF_MSG_TYPE_STR(SCIF_NODE_REMOVE),
	SCIF_MSG_TYPE_STR(SCIF_NODE_REMOVE_ACK),
	SCIF_MSG_TYPE_STR(SCIF_CNCT_REQ),
	SCIF_MSG_TYPE_STR(SCIF_CNCT_GNT),
	SCIF_MSG_TYPE_STR(SCIF_CNCT_GNTACK),
	SCIF_MSG_TYPE_STR(SCIF_CNCT_GNTNACK),
	SCIF_MSG_TYPE_STR(SCIF_CNCT_REJ),
	SCIF_MSG_TYPE_STR(SCIF_DISCNCT),
	SCIF_MSG_TYPE_STR(SCIF_DISCNT_ACK),
	SCIF_MSG_TYPE_STR(SCIF_CLIENT_SENT),
	SCIF_MSG_TYPE_STR(SCIF_CLIENT_RCVD),
	SCIF_MSG_TYPE_STR(SCIF_GET_NODE_INFO),
	SCIF_MSG_TYPE_STR(SCIF_REGISTER),
	SCIF_MSG_TYPE_STR(SCIF_REGISTER_ACK),
	SCIF_MSG_TYPE_STR(SCIF_REGISTER_NACK),
	SCIF_MSG_TYPE_STR(SCIF_UNREGISTER),
	SCIF_MSG_TYPE_STR(SCIF_DELETE_WINDOW),
	SCIF_MSG_TYPE_STR(SCIF_USED_WINDOW),
	SCIF_MSG_TYPE_STR(SCIF_ALLOC_REQ),
	SCIF_MSG_TYPE_STR(SCIF_ALLOC_GNT),
	SCIF_MSG_TYPE_STR(SCIF_ALLOC_REJ),
	SCIF_MSG_TYPE_STR(SCIF_FREE_VIRT),
	SCIF_MSG_TYPE_STR(SCIF_MARK),
	SCIF_MSG_TYPE_STR(SCIF_MARK_ACK),
	SCIF_MSG_TYPE_STR(SCIF_MARK_NACK),
	SCIF_MSG_TYPE_STR(SCIF_WAIT),
	SCIF_MSG_TYPE_STR(SCIF_WAIT_ACK),
	SCIF_MSG_TYPE_STR(SCIF_WAIT_NACK),
	SCIF_MSG_TYPE_STR(SCIF_SIG_LOCAL),
	SCIF_MSG_TYPE_STR(SCIF_SIG_REMOTE),
	SCIF_MSG_TYPE_STR(SCIF_SIG_ACK),
	SCIF_MSG_TYPE_STR(SCIF_SIG_NACK),
#undef SCIF_MSG_TYPE_STR
};

static void
scif_display_message(struct scif_dev *scifdev, struct scifmsg *msg,
		     const char *label)
{
	const char *msg_str;
	if (!scif_info.en_msg_log)
		return;
	if (msg->uop > SCIF_MAX_MSG) {
		dev_err(&scifdev->sdev->dev,
			"%s: unknown msg type %d\n", label, msg->uop);
		return;
	}

	msg_str = message_types[msg->uop];
	if (!msg_str)
		msg_str = "UNKNOWN";

	pr_info("[%7u] scif%d: %s %s, src %d:%d, dest %d:%d payload 0x%llx:0x%llx:0x%llx:0x%llx\n",
		task_pid_nr(current), scifdev->node, label,
		msg_str, msg->src.node, msg->src.port, msg->dst.node,
		msg->dst.port, msg->payload[0], msg->payload[1],
		msg->payload[2], msg->payload[3]);
}

static int scif_loopback_nodeqp_send(struct scifmsg *msg);

int _scif_nodeqp_send(struct scif_dev *scifdev, struct scifmsg *msg)
{
	struct scif_qp *qp = scifdev->qpairs;
	int err = -ENOMEM, loop_cnt = 0;

	scif_display_message(scifdev, msg, "Sent");

	if (unlikely(scifdev_self(scifdev)))
		return scif_loopback_nodeqp_send(msg);

	if (!qp) {
		err = -EINVAL;
		goto error;
	}

	spin_lock(&qp->send_lock);
	while ((err = scif_rb_write(&qp->outbound_q,
				    msg, sizeof(struct scifmsg)))) {
		if (loop_cnt++ > NODEQP_DELAY_LOOPS) {
			err = -ENODEV;
			break;
		}
		udelay(NODEQP_USECS_DELAY);
	}
	if (!err)
		scif_rb_commit(&qp->outbound_q);
	spin_unlock(&qp->send_lock);
	if (!err)
		scif_send_msg_intr(scifdev);
error:
	if (err)
		dev_dbg(&scifdev->sdev->dev,
			"%s %d error %d uop %d\n",
			 __func__, __LINE__, err, msg->uop);
	return err;
}

/**
 * scif_nodeqp_send - Send a message on the node queue pair
 * @scifdev: Scif Device.
 * @msg: The message to be sent.
 */
int scif_nodeqp_send(struct scif_dev *scifdev, struct scifmsg *msg)
{
	int err;
	struct device *spdev = NULL;

	if (msg->uop > SCIF_EXIT_ACK) {
		/* Dont send messages once the exit flow has begun */
		if (OP_IDLE != scifdev->exit)
			return -ENODEV;

		spdev = scif_get_peer_dev(scifdev);
		if (!spdev)
			return -ENODEV;
	}
	err = _scif_nodeqp_send(scifdev, msg);
	if (msg->uop > SCIF_EXIT_ACK)
		scif_put_peer_dev(spdev);
	return err;
}

/*
 * scif_misc_handler:
 *
 * Work queue handler for servicing miscellaneous SCIF tasks.
 * Examples include:
 * 1) Remote fence requests.
 * 2) Destruction of temporary registered windows
 *    created during scif_vreadfrom()/scif_vwriteto().
 * 3) Cleanup of zombie endpoints.
 */
void scif_misc_handler(struct work_struct *work)
{
	scif_rma_handle_remote_fences();
	scif_rma_destroy_windows();
	scif_rma_destroy_tcw_invalid();
	scif_cleanup_zombie_epd();
}

/**
 * scif_init() - Respond to SCIF_INIT interrupt message
 * @scifdev:    Remote SCIF device node
 * @msg:        Interrupt message
 */
static __always_inline void
scif_init(struct scif_dev *scifdev, struct scifmsg *msg)
{
	/*
	 * Allow the thread waiting for device page updates for the peer QP DMA
	 * address to complete initializing the inbound_q.
	 */
	flush_delayed_work(&scifdev->qp_dwork);

	scif_peer_register_device(scifdev);

	if (scif_is_mgmt_node()) {
		mutex_lock(&scif_info.conflock);
		scif_p2p_setup();
		mutex_unlock(&scif_info.conflock);
	}
}

/**
 * scif_exit() - Respond to SCIF_EXIT interrupt message
 * @scifdev:    Remote SCIF device node
 * @msg:        Interrupt message
 *
 * This function stops the SCIF interface for the node which sent
 * the SCIF_EXIT message and starts waiting for that node to
 * resetup the queue pair again.
 */
static __always_inline void
scif_exit(struct scif_dev *scifdev, struct scifmsg *unused)
{
	scifdev->exit_ack_pending = true;
	if (scif_is_mgmt_node())
		scif_disconnect_node(scifdev->node, false);
	else
		scif_stop(scifdev);
	schedule_delayed_work(&scifdev->qp_dwork,
			      msecs_to_jiffies(1000));
}

/**
 * scif_exitack() - Respond to SCIF_EXIT_ACK interrupt message
 * @scifdev:    Remote SCIF device node
 * @msg:        Interrupt message
 *
 */
static __always_inline void
scif_exit_ack(struct scif_dev *scifdev, struct scifmsg *unused)
{
	scifdev->exit = OP_COMPLETED;
	wake_up(&scif_info.exitwq);
}

/**
 * scif_node_add() - Respond to SCIF_NODE_ADD interrupt message
 * @scifdev:    Remote SCIF device node
 * @msg:        Interrupt message
 *
 * When the mgmt node driver has finished initializing a MIC node queue pair it
 * marks the node as online. It then looks for all currently online MIC cards
 * and send a SCIF_NODE_ADD message to identify the ID of the new card for
 * peer to peer initialization
 *
 * The local node allocates its incoming queue and sends its address in the
 * SCIF_NODE_ADD_ACK message back to the mgmt node, the mgmt node "reflects"
 * this message to the new node
 */
static __always_inline void
scif_node_add(struct scif_dev *scifdev, struct scifmsg *msg)
{
	struct scif_dev *newdev;
	dma_addr_t qp_offset;
	int qp_connect;
	struct scif_hw_dev *sdev;

	dev_dbg(&scifdev->sdev->dev,
		"Scifdev %d:%d received NODE_ADD msg for node %d\n",
		scifdev->node, msg->dst.node, msg->src.node);
	dev_dbg(&scifdev->sdev->dev,
		"Remote address for this node's aperture %llx\n",
		msg->payload[0]);
	newdev = &scif_dev[msg->src.node];
	newdev->node = msg->src.node;
	newdev->sdev = scif_dev[SCIF_MGMT_NODE].sdev;
	sdev = newdev->sdev;

	if (scif_setup_intr_wq(newdev)) {
		dev_err(&scifdev->sdev->dev,
			"failed to setup interrupts for %d\n", msg->src.node);
		goto interrupt_setup_error;
	}
	newdev->mmio.va = scif_ioremap(msg->payload[1],
					sdev->mmio->len,
					newdev);
	if (!newdev->mmio.va) {
		dev_err(&scifdev->sdev->dev,
			"failed to map mmio for %d\n", msg->src.node);
		goto mmio_map_error;
	}
	newdev->qpairs = kzalloc(sizeof(*newdev->qpairs), GFP_KERNEL);
	if (!newdev->qpairs)
		goto qp_alloc_error;
	/*
	 * Set the base address of the remote node's memory since it gets
	 * added to qp_offset
	 */
	newdev->base_addr = msg->payload[0];

	qp_connect = scif_setup_qp_connect(newdev->qpairs, &qp_offset,
					   SCIF_NODE_QP_SIZE, newdev);
	if (qp_connect) {
		dev_err(&scifdev->sdev->dev,
			"failed to setup qp_connect %d\n", qp_connect);
		goto qp_connect_error;
	}

	newdev->db = sdev->hw_ops->next_db(sdev);
	newdev->cookie = sdev->hw_ops->request_irq(sdev, scif_intr_handler,
						   "SCIF_INTR", newdev,
						   newdev->db);
	if (IS_ERR(newdev->cookie)) {
		newdev->cookie = NULL;
		goto qp_connect_error;
	}
	newdev->qpairs->magic = SCIFEP_MAGIC;
	newdev->qpairs->qp_state = SCIF_QP_OFFLINE;

	msg->uop = SCIF_NODE_ADD_ACK;
	msg->dst.node = msg->src.node;
	msg->src.node = scif_info.nodeid;
	msg->payload[0] = qp_offset;
	msg->payload[2] = newdev->db;
	scif_nodeqp_send(&scif_dev[SCIF_MGMT_NODE], msg);
	return;
qp_connect_error:
	kfree(newdev->qpairs);
	newdev->qpairs = NULL;
qp_alloc_error:
	iounmap(newdev->mmio.va);
	newdev->mmio.va = NULL;
mmio_map_error:
interrupt_setup_error:
	dev_err(&scifdev->sdev->dev,
		"node add failed for node %d\n", msg->src.node);
	msg->uop = SCIF_NODE_ADD_NACK;
	msg->dst.node = msg->src.node;
	msg->src.node = scif_info.nodeid;
	scif_nodeqp_send(&scif_dev[SCIF_MGMT_NODE], msg);
}

void scif_poll_qp_state(struct work_struct *work)
{
#define SCIF_NODE_QP_RETRY 100
#define SCIF_NODE_QP_TIMEOUT 100
	struct scif_dev *peerdev = container_of(work, struct scif_dev,
							p2p_dwork.work);
	struct scif_qp *qp = peerdev->qpairs;

	if (qp->qp_state != SCIF_QP_ONLINE ||
	    qp->remote_qp->qp_state != SCIF_QP_ONLINE) {
		if (peerdev->p2p_retry++ == SCIF_NODE_QP_RETRY) {
			dev_err(&peerdev->sdev->dev,
				"Warning: QP check timeout with state %d\n",
				qp->qp_state);
			goto timeout;
		}
		schedule_delayed_work(&peerdev->p2p_dwork,
				      msecs_to_jiffies(SCIF_NODE_QP_TIMEOUT));
		return;
	}
	return;
timeout:
	dev_err(&peerdev->sdev->dev,
		"%s %d remote node %d offline,  state = 0x%x\n",
		__func__, __LINE__, peerdev->node, qp->qp_state);
	qp->remote_qp->qp_state = SCIF_QP_OFFLINE;
	scif_peer_unregister_device(peerdev);
	scif_cleanup_scifdev(peerdev);
}

/**
 * scif_node_add_ack() - Respond to SCIF_NODE_ADD_ACK interrupt message
 * @scifdev:    Remote SCIF device node
 * @msg:        Interrupt message
 *
 * After a MIC node receives the SCIF_NODE_ADD_ACK message it send this
 * message to the mgmt node to confirm the sequence is finished.
 *
 */
static __always_inline void
scif_node_add_ack(struct scif_dev *scifdev, struct scifmsg *msg)
{
	struct scif_dev *peerdev;
	struct scif_qp *qp;
	struct scif_dev *dst_dev = &scif_dev[msg->dst.node];

	dev_dbg(&scifdev->sdev->dev,
		"Scifdev %d received SCIF_NODE_ADD_ACK msg src %d dst %d\n",
		scifdev->node, msg->src.node, msg->dst.node);
	dev_dbg(&scifdev->sdev->dev,
		"payload %llx %llx %llx %llx\n", msg->payload[0],
		msg->payload[1], msg->payload[2], msg->payload[3]);
	if (scif_is_mgmt_node()) {
		/*
		 * the lock serializes with scif_qp_response_ack. The mgmt node
		 * is forwarding the NODE_ADD_ACK message from src to dst we
		 * need to make sure that the dst has already received a
		 * NODE_ADD for src and setup its end of the qp to dst
		 */
		mutex_lock(&scif_info.conflock);
		msg->payload[1] = scif_info.maxid;
		scif_nodeqp_send(dst_dev, msg);
		mutex_unlock(&scif_info.conflock);
		return;
	}
	peerdev = &scif_dev[msg->src.node];
	peerdev->sdev = scif_dev[SCIF_MGMT_NODE].sdev;
	peerdev->node = msg->src.node;

	qp = peerdev->qpairs;

	if ((scif_setup_qp_connect_response(peerdev, peerdev->qpairs,
					    msg->payload[0])))
		goto local_error;
	peerdev->rdb = msg->payload[2];
	qp->remote_qp->qp_state = SCIF_QP_ONLINE;

	scif_peer_register_device(peerdev);

	schedule_delayed_work(&peerdev->p2p_dwork, 0);
	return;
local_error:
	scif_cleanup_scifdev(peerdev);
}

/**
 * scif_node_add_nack: Respond to SCIF_NODE_ADD_NACK interrupt message
 * @msg:        Interrupt message
 *
 * SCIF_NODE_ADD failed, so inform the waiting wq.
 */
static __always_inline void
scif_node_add_nack(struct scif_dev *scifdev, struct scifmsg *msg)
{
	if (scif_is_mgmt_node()) {
		struct scif_dev *dst_dev = &scif_dev[msg->dst.node];

		dev_dbg(&scifdev->sdev->dev,
			"SCIF_NODE_ADD_NACK received from %d\n", scifdev->node);
		scif_nodeqp_send(dst_dev, msg);
	}
}

/*
 * scif_node_remove: Handle SCIF_NODE_REMOVE message
 * @msg: Interrupt message
 *
 * Handle node removal.
 */
static __always_inline void
scif_node_remove(struct scif_dev *scifdev, struct scifmsg *msg)
{
	int node = msg->payload[0];
	struct scif_dev *scdev = &scif_dev[node];

	scdev->node_remove_ack_pending = true;
	scif_handle_remove_node(node);
}

/*
 * scif_node_remove_ack: Handle SCIF_NODE_REMOVE_ACK message
 * @msg: Interrupt message
 *
 * The peer has acked a SCIF_NODE_REMOVE message.
 */
static __always_inline void
scif_node_remove_ack(struct scif_dev *scifdev, struct scifmsg *msg)
{
	struct scif_dev *sdev = &scif_dev[msg->payload[0]];

	atomic_inc(&sdev->disconn_rescnt);
	wake_up(&sdev->disconn_wq);
}

/**
 * scif_get_node_info: Respond to SCIF_GET_NODE_INFO interrupt message
 * @msg:        Interrupt message
 *
 * Retrieve node info i.e maxid and total from the mgmt node.
 */
static __always_inline void
scif_get_node_info_resp(struct scif_dev *scifdev, struct scifmsg *msg)
{
	if (scif_is_mgmt_node()) {
		swap(msg->dst.node, msg->src.node);
		mutex_lock(&scif_info.conflock);
		msg->payload[1] = scif_info.maxid;
		msg->payload[2] = scif_info.total;
		mutex_unlock(&scif_info.conflock);
		scif_nodeqp_send(scifdev, msg);
	} else {
		struct completion *node_info =
			(struct completion *)msg->payload[3];

		mutex_lock(&scif_info.conflock);
		scif_info.maxid = msg->payload[1];
		scif_info.total = msg->payload[2];
		complete_all(node_info);
		mutex_unlock(&scif_info.conflock);
	}
}

static void
scif_msg_unknown(struct scif_dev *scifdev, struct scifmsg *msg)
{
	/* Bogus Node Qp Message? */
	dev_err(&scifdev->sdev->dev,
		"Unknown message 0x%xn scifdev->node 0x%x\n",
		msg->uop, scifdev->node);
}

static void (*scif_intr_func[SCIF_MAX_MSG + 1])
	    (struct scif_dev *, struct scifmsg *msg) = {
	scif_msg_unknown,	/* Error */
	scif_init,		/* SCIF_INIT */
	scif_exit,		/* SCIF_EXIT */
	scif_exit_ack,		/* SCIF_EXIT_ACK */
	scif_node_add,		/* SCIF_NODE_ADD */
	scif_node_add_ack,	/* SCIF_NODE_ADD_ACK */
	scif_node_add_nack,	/* SCIF_NODE_ADD_NACK */
	scif_node_remove,	/* SCIF_NODE_REMOVE */
	scif_node_remove_ack,	/* SCIF_NODE_REMOVE_ACK */
	scif_cnctreq,		/* SCIF_CNCT_REQ */
	scif_cnctgnt,		/* SCIF_CNCT_GNT */
	scif_cnctgnt_ack,	/* SCIF_CNCT_GNTACK */
	scif_cnctgnt_nack,	/* SCIF_CNCT_GNTNACK */
	scif_cnctrej,		/* SCIF_CNCT_REJ */
	scif_discnct,		/* SCIF_DISCNCT */
	scif_discnt_ack,	/* SCIF_DISCNT_ACK */
	scif_clientsend,	/* SCIF_CLIENT_SENT */
	scif_clientrcvd,	/* SCIF_CLIENT_RCVD */
	scif_get_node_info_resp,/* SCIF_GET_NODE_INFO */
	scif_recv_reg,		/* SCIF_REGISTER */
	scif_recv_reg_ack,	/* SCIF_REGISTER_ACK */
	scif_recv_reg_nack,	/* SCIF_REGISTER_NACK */
	scif_recv_unreg,	/* SCIF_UNREGISTER */
	scif_recv_del_window,	/* SCIF_DELETE_WINDOW */
	scif_recv_used_window,  /* SCIF_USED_WINDOW */
	scif_alloc_req,		/* SCIF_ALLOC_REQ */
	scif_alloc_gnt_rej,	/* SCIF_ALLOC_GNT */
	scif_alloc_gnt_rej,	/* SCIF_ALLOC_REJ */
	scif_free_virt,		/* SCIF_FREE_VIRT */
	scif_recv_mark,		/* SCIF_MARK */
	scif_recv_mark_resp,	/* SCIF_MARK_ACK */
	scif_recv_mark_resp,	/* SCIF_MARK_NACK */
	scif_recv_wait,		/* SCIF_WAIT */
	scif_recv_wait_resp,	/* SCIF_WAIT_ACK */
	scif_recv_wait_resp,	/* SCIF_WAIT_NACK */
	scif_recv_sig_local,	/* SCIF_SIG_LOCAL */
	scif_recv_sig_remote,	/* SCIF_SIG_REMOTE */
	scif_recv_sig_resp,	/* SCIF_SIG_ACK */
	scif_recv_sig_resp,	/* SCIF_SIG_NACK */
};

/**
 * scif_nodeqp_msg_handler() - Common handler for node messages
 * @scifdev: Remote device to respond to
 * @msg: The message to be handled.
 *
 * This routine calls the appropriate routine to handle a Node Qp
 * message receipt
 */
static int scif_max_msg_id = SCIF_MAX_MSG;

static void
scif_nodeqp_msg_handler(struct scif_dev *scifdev, struct scifmsg *msg)
{
	scif_display_message(scifdev, msg, "Rcvd");

	if (msg->uop > (u32)scif_max_msg_id) {
		/* Bogus Node Qp Message? */
		dev_err(&scifdev->sdev->dev,
			"Unknown message 0x%xn scifdev->node 0x%x\n",
			msg->uop, scifdev->node);
		return;
	}

	scif_intr_func[msg->uop](scifdev, msg);
}

/**
 * scif_nodeqp_intrhandler() - Interrupt handler for node messages
 * @scifdev:    Remote device to respond to
 * @qp:         Remote memory pointer
 *
 * This routine is triggered by the interrupt mechanism.  It reads
 * messages from the node queue RB and calls the Node QP Message handling
 * routine.
 */
void scif_nodeqp_intrhandler(struct scif_dev *scifdev, struct scif_qp *qp)
{
	struct scifmsg msg;
	int read_size;

	do {
		read_size = scif_rb_get_next(&qp->inbound_q, &msg, sizeof(msg));
		if (!read_size)
			break;
		scif_nodeqp_msg_handler(scifdev, &msg);
		/*
		 * The node queue pair is unmapped so skip the read pointer
		 * update after receipt of a SCIF_EXIT_ACK
		 */
		if (SCIF_EXIT_ACK == msg.uop)
			break;
		scif_rb_update_read_ptr(&qp->inbound_q);
	} while (1);
}

static struct kmem_cache *loopb_cache;
/**
 * scif_loopb_wq_handler - Loopback Workqueue Handler.
 * @work: loopback work
 *
 * This work queue routine is invoked by the loopback work queue handler.
 * It calls the node QP message handler and frees the message up.
 */
static void scif_loopb_wq_handler(struct work_struct *work)
{
	struct scif_dev *scifdev = scif_info.loopb_dev;
	struct scif_loopb_msg *msg =
		container_of(work, struct scif_loopb_msg, work);

	scif_nodeqp_msg_handler(scifdev, &msg->msg);
	kmem_cache_free(loopb_cache, msg);
}

/*
 * scif_loopback_nodeqp_send - Sends message to the current node.
 * @msg: SCIF message to send.
 *
 * It copies message to the work which put in the scif_loopb_wq_handler.
 */
static int scif_loopback_nodeqp_send(struct scifmsg *msg)
{
	struct scif_loopb_msg *lmsg;

	lmsg = kmem_cache_alloc(loopb_cache, GFP_ATOMIC);
	if (!lmsg)
		return -ENOMEM;
	memcpy(&lmsg->msg, msg, sizeof(*msg));
	INIT_WORK(&lmsg->work, scif_loopb_wq_handler);
	queue_work(scif_info.loopb_wq, &lmsg->work);
	return 0;
}

/**
 * scif_setup_loopback_qp - One time setup work for Loopback Node Qp.
 * @scifdev: SCIF device
 *
 * Sets up the required loopback workqueues, queue pairs and ring buffers
 */
int scif_setup_loopback_qp(struct scif_dev *scifdev)
{
	int err = 0;

	log_scif_info(scifdev->node, "scifdev %p", scifdev);
	err = scif_setup_intr_wq(scifdev);
	if (err)
		goto exit;
	loopb_cache = kmem_cache_create("loopb_cache",
				sizeof(struct scif_loopb_msg),
				0,
				SLAB_HWCACHE_ALIGN,
				NULL);
	if (!loopb_cache) {
		err = -ENOMEM;
		goto destroy_intr;
	}
	scif_info.loopb_wq =
		alloc_ordered_workqueue("SCIF LOOPB %d", 0, scifdev->node);
	if (!scif_info.loopb_wq) {
		err = -ENOMEM;
		goto destroy_loopb_cache;
	}
	scif_info.nodeid = scifdev->node;
	scif_peer_register_device(scifdev);
	scif_info.loopb_dev = scifdev;
	return 0;
destroy_loopb_cache:
	kmem_cache_destroy(loopb_cache);
destroy_intr:
	scif_destroy_intr_wq(scifdev);
exit:
	return err;
}

/**
 * scif_destroy_loopback_qp - One time uninit work for Loopback Node Qp
 * @scifdev: SCIF device
 *
 * Destroys the workqueues and frees up the Ring Buffer and Queue Pair memory.
 */
int scif_destroy_loopback_qp(struct scif_dev *scifdev)
{
	log_scif_info(scifdev->node, "scifdev %p", scifdev);

	scif_peer_unregister_device(scifdev);
	destroy_workqueue(scif_info.loopb_wq);
	kmem_cache_destroy(loopb_cache);
	scif_destroy_intr_wq(scifdev);
	scifdev->sdev = NULL;
	scif_info.loopb_dev = NULL;
	return 0;
}

void scif_destroy_p2p(struct scif_dev *scifdev)
{
	struct scif_dev *peer_dev;
	struct scif_p2p_info *p2p;
	struct list_head *pos, *tmp;
	int bd;

	mutex_lock(&scif_info.conflock);
	/* Free P2P mappings in the given node for all its peer nodes */
	list_for_each_safe(pos, tmp, &scifdev->p2p) {
		p2p = list_entry(pos, struct scif_p2p_info, ppi_list);
		scif_deinit_p2p_info(scifdev, p2p);
		list_del(pos);
	}

	/* Free P2P mapping created in the peer nodes for the given node */
	for (bd = SCIF_MGMT_NODE + 1; bd <= scif_info.maxid; bd++) {
		peer_dev = &scif_dev[bd];
		list_for_each_safe(pos, tmp, &peer_dev->p2p) {
			p2p = list_entry(pos, struct scif_p2p_info, ppi_list);
			if (p2p->ppi_peer_id == scifdev->node) {
				scif_deinit_p2p_info(peer_dev, p2p);
				list_del(pos);
			}
		}
	}
	mutex_unlock(&scif_info.conflock);
}
