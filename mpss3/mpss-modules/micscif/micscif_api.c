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

#include <linux/poll.h>
#include <linux/time.h>
#include <linux/ktime.h>
#include <linux/sched.h>
#include <linux/kref.h>
#include <linux/module.h>
#include "scif.h"
#include "mic/micscif.h"
#ifndef _MIC_SCIF_
#include "mic_common.h"
#endif
#include "mic/micscif_map.h"

#define SCIF_MAP_ULIMIT 0x40

bool mic_ulimit_check = 0;

char *scif_ep_states[] = {
	"Closed",
	"Unbound",
	"Bound",
	"Listening",
	"Connected",
	"Connecting",
	"Mapping",
	"Closing",
	"Close Listening",
	"Disconnected",
	"Zombie"};

enum conn_async_state {
	ASYNC_CONN_IDLE = 1,	/* ep setup for async connect */
	ASYNC_CONN_INPROGRESS,	/* async connect in progress */
	ASYNC_CONN_FLUSH_WORK	/* async work flush in progress  */
};

/**
 * scif_open() - Create a SCIF end point
 *
 * Create a SCIF end point and set the state to UNBOUND.  This function
 * returns the address of the end point data structure.
 */
scif_epd_t
__scif_open(void)
{
	struct endpt *ep;

	might_sleep();
	if ((ep = (struct endpt *)kzalloc(sizeof(struct endpt), GFP_KERNEL)) == NULL) {
		printk(KERN_ERR "SCIFAPI open: kzalloc fail on scif end point descriptor\n");
		goto err_ep_alloc;
	}

	if ((ep->qp_info.qp = (struct micscif_qp *)
			kzalloc(sizeof(struct micscif_qp), GFP_KERNEL)) == NULL) {
		printk(KERN_ERR "SCIFAPI open: kzalloc fail on scif end point queue pointer\n");
		goto err_qp_alloc;
	}

	spin_lock_init(&ep->lock);
	mutex_init (&ep->sendlock);
	mutex_init (&ep->recvlock);

	if (micscif_rma_ep_init(ep) < 0) {
		printk(KERN_ERR "SCIFAPI _open: RMA EP Init failed\n");
		goto err_rma_init;
	}

	ep->state = SCIFEP_UNBOUND;
	pr_debug("SCIFAPI open: ep %p success\n", ep);
	return (scif_epd_t)ep;

err_rma_init:
	kfree(ep->qp_info.qp);
err_qp_alloc:
	kfree(ep);
err_ep_alloc:
	return NULL;
}

scif_epd_t
scif_open(void)
{
	struct endpt *ep;
	ep = (struct endpt *)__scif_open();
	if (ep)
		kref_init(&(ep->ref_count));
	return (scif_epd_t)ep;
}
EXPORT_SYMBOL(scif_open);

/**
 * scif_close() - Terminate a SCIF end point
 * @epd:        The end point address returned from scif_open()
 *
 * The function terminates a scif connection.  It must ensure all traffic on
 * the connection is finished before removing it.
 *
 * On Connection with memory mapped this become more difficult.  Once normal
 * DMA and message traffic has ended the end point must be placed in a zombie
 * state and wait for the other side to also release it's memory references.
 */
int
__scif_close(scif_epd_t epd)
{
	struct endpt *ep = (struct endpt *)epd;
	struct endpt *tmpep;
	struct list_head *pos, *tmpq;
	unsigned long sflags;
	enum endptstate oldstate;
	int err;
	bool flush_conn;

	pr_debug("SCIFAPI close: ep %p %s\n", ep, scif_ep_states[ep->state]);

	might_sleep();

	spin_lock(&ep->lock);
	flush_conn = (ep->conn_async_state == ASYNC_CONN_INPROGRESS);
	spin_unlock(&ep->lock);

	if (flush_conn)
		flush_workqueue(ms_info.mi_conn_wq);

	micscif_inc_node_refcnt(ep->remote_dev, 1);

	spin_lock_irqsave(&ep->lock, sflags);
	oldstate = ep->state;

	ep->state = SCIFEP_CLOSING;

	switch (oldstate) {
	case SCIFEP_ZOMBIE:
		BUG_ON(SCIFEP_ZOMBIE == oldstate);
	case SCIFEP_CLOSED:
	case SCIFEP_DISCONNECTED:
		spin_unlock_irqrestore(&ep->lock, sflags);
		micscif_unregister_all_windows(epd);
		// Remove from the disconnected list
		spin_lock_irqsave(&ms_info.mi_connlock, sflags);
		list_for_each_safe(pos, tmpq, &ms_info.mi_disconnected) {
			tmpep = list_entry(pos, struct endpt, list);
			if (tmpep == ep) {
				list_del(pos);
				break;
			}
		}
		spin_unlock_irqrestore(&ms_info.mi_connlock, sflags);
		break;
	case SCIFEP_UNBOUND:
	case SCIFEP_BOUND:
	case SCIFEP_CONNECTING:
		spin_unlock_irqrestore(&ep->lock, sflags);
		break;
	case SCIFEP_MAPPING:
	case SCIFEP_CONNECTED:
	case SCIFEP_CLOSING:
	{
		struct nodemsg msg;
		struct endpt *fep = NULL;
		struct endpt *tmpep;
		unsigned long ts = jiffies;
		struct list_head *pos, *tmpq;

		// Very short time before mapping completes and state becomes connected
		// and does a standard teardown.
		ts = jiffies;
		while (ep->state == SCIFEP_MAPPING) {
			cpu_relax();
			if (time_after((unsigned long)jiffies,ts + NODE_ALIVE_TIMEOUT)) {
				printk(KERN_ERR "%s %d ep->state %d\n", __func__, __LINE__, ep->state);
				ep->state = SCIFEP_BOUND;
				break;
			}
		}

		init_waitqueue_head(&ep->disconwq);	// Wait for connection queue
		spin_unlock_irqrestore(&ep->lock, sflags);

		micscif_unregister_all_windows(epd);

		// Remove from the connected list
		spin_lock_irqsave(&ms_info.mi_connlock, sflags);
		list_for_each_safe(pos, tmpq, &ms_info.mi_connected) {
			tmpep = list_entry(pos, struct endpt, list);
			if (tmpep == ep) {
				list_del(pos);
				put_conn_count(ep->remote_dev);
				fep = tmpep;
				spin_lock(&ep->lock);
				break;
			}
		}

		if (fep == NULL) {
			// The other side has completed the disconnect before
			// the end point can be removed from the list.  Therefore
			// the ep lock is not locked, traverse the disconnected list
			// to find the endpoint, release the conn lock and
			// proceed to teardown the end point below.
			list_for_each_safe(pos, tmpq, &ms_info.mi_disconnected) {
				tmpep = list_entry(pos, struct endpt, list);
				if (tmpep == ep) {
					list_del(pos);
					break;
				}
			}
			spin_unlock_irqrestore(&ms_info.mi_connlock, sflags);
			break;
		}

		spin_unlock(&ms_info.mi_connlock);

		// Now we are free to close out the connection
		msg.uop = SCIF_DISCNCT;
		msg.src = ep->port;
		msg.dst = ep->peer;
		msg.payload[0] = (uint64_t)ep;
		msg.payload[1] = ep->remote_ep;

		err = micscif_nodeqp_send(ep->remote_dev, &msg, ep);
		spin_unlock_irqrestore(&ep->lock, sflags);

		if (!err)
			/* Now wait for the remote node to respond */
			wait_event_timeout(ep->disconwq, 
				(ep->state == SCIFEP_DISCONNECTED), NODE_ALIVE_TIMEOUT);
		/*
		 * Grab and release the ep lock to synchronize with the
		 * thread waking us up. If we dont grab this lock, then
		 * the ep might be freed before the wakeup completes
		 * resulting in potential memory corruption.
		 */
		spin_lock_irqsave(&ep->lock, sflags);
		spin_unlock_irqrestore(&ep->lock, sflags);
		break;
	}
	case SCIFEP_LISTENING:
	case SCIFEP_CLLISTEN:
	{
		struct conreq *conreq;
		struct nodemsg msg;
		struct endpt *aep;

		spin_unlock_irqrestore(&ep->lock, sflags);
		spin_lock_irqsave(&ms_info.mi_eplock, sflags);

		// remove from listen list
		list_for_each_safe(pos, tmpq, &ms_info.mi_listen) {
			tmpep = list_entry(pos, struct endpt, list);
			if (tmpep == ep) {
				list_del(pos);
			}
		}
		// Remove any dangling accepts
		while (ep->acceptcnt) {
			aep = list_first_entry(&ep->li_accept, struct endpt, liacceptlist);
			BUG_ON(!aep);
			list_del(&aep->liacceptlist);
			if (aep->port.port && !aep->accepted_ep)
				put_scif_port(aep->port.port);
			list_for_each_safe(pos, tmpq, &ms_info.mi_uaccept) {
				tmpep = list_entry(pos, struct endpt, miacceptlist);
				if (tmpep == aep) {
					list_del(pos);
					break;
				}
			}
			spin_unlock_irqrestore(&ms_info.mi_eplock, sflags);
			spin_lock_irqsave(&ms_info.mi_connlock, sflags);
			list_for_each_safe(pos, tmpq, &ms_info.mi_connected) {
				tmpep = list_entry(pos, struct endpt, list);
				if (tmpep == aep) {
					list_del(pos);
					put_conn_count(aep->remote_dev);
					break;
				}
			}
			list_for_each_safe(pos, tmpq, &ms_info.mi_disconnected) {
				tmpep = list_entry(pos, struct endpt, list);
				if (tmpep == aep) {
					list_del(pos);
					break;
				}
			}
			spin_unlock_irqrestore(&ms_info.mi_connlock, sflags);
			micscif_teardown_ep(aep);
			spin_lock_irqsave(&ms_info.mi_eplock, sflags);
			micscif_add_epd_to_zombie_list(aep, MI_EPLOCK_HELD);
			ep->acceptcnt--;
		}

		spin_lock(&ep->lock);
		spin_unlock(&ms_info.mi_eplock);

		// Remove and reject any pending connection requests.
		while (ep->conreqcnt) {
			conreq = list_first_entry(&ep->conlist, struct conreq, list);
			list_del(&conreq->list);

			msg.uop = SCIF_CNCT_REJ;
			msg.dst.node = conreq->msg.src.node;
			msg.dst.port = conreq->msg.src.port;
			msg.payload[0] = conreq->msg.payload[0];
			msg.payload[1] = conreq->msg.payload[1];
			/*
			 * No Error Handling on purpose for micscif_nodeqp_send().
			 * If the remote node is lost we still want free the connection
			 * requests on the self node.
			 */
			micscif_nodeqp_send(&scif_dev[conreq->msg.src.node], &msg, ep);

			ep->conreqcnt--;
			kfree(conreq);
		}

		// If a kSCIF accept is waiting wake it up
		wake_up_interruptible(&ep->conwq);
		spin_unlock_irqrestore(&ep->lock, sflags);
		break;
	}
	}
	if (ep->port.port && !ep->accepted_ep)
		put_scif_port(ep->port.port);
	micscif_dec_node_refcnt(ep->remote_dev, 1);
	micscif_teardown_ep(ep);
	micscif_add_epd_to_zombie_list(ep, !MI_EPLOCK_HELD);
	return 0;
}

void
scif_ref_rel(struct kref *kref_count)
{
	struct endpt *epd;
	epd = container_of(kref_count, struct endpt, ref_count);
	__scif_close((scif_epd_t)epd);
}

int
scif_close(scif_epd_t epd)
{
	__scif_flush(epd);
	put_kref_count(epd);
	return 0;
}
EXPORT_SYMBOL(scif_close);

/**
 * scif_flush() - Flush the endpoint
 * @epd:        The end point address returned from scif_open()
 *
 */
int
__scif_flush(scif_epd_t epd)
{
	struct endpt *ep = (struct endpt *)epd;
	struct endpt *tmpep;
	struct list_head *pos, *tmpq;
	unsigned long sflags;
	int err;

	might_sleep();

	micscif_inc_node_refcnt(ep->remote_dev, 1);

	spin_lock_irqsave(&ep->lock, sflags);

	switch (ep->state) {
	case SCIFEP_CONNECTED:
	{
		struct nodemsg msg;
		struct endpt *fep = NULL;

		init_waitqueue_head(&ep->disconwq);	// Wait for connection queue
		WARN_ON(ep->files); // files should never be set while connected
		spin_unlock_irqrestore(&ep->lock, sflags);
		spin_lock_irqsave(&ms_info.mi_connlock, sflags);

		list_for_each_safe(pos, tmpq, &ms_info.mi_connected) {
			tmpep = list_entry(pos, struct endpt, list);
			if (tmpep == ep) {
				list_del(pos);
				put_conn_count(ep->remote_dev);
				fep = tmpep;
				spin_lock(&ep->lock);
				break;
			}
		}

		if (fep == NULL) {
			// The other side has completed the disconnect before
			// the end point can be removed from the list.  Therefore
			// the ep lock is not locked, traverse the disconnected list
			// to find the endpoint, release the conn lock.
			list_for_each_safe(pos, tmpq, &ms_info.mi_disconnected) {
				tmpep = list_entry(pos, struct endpt, list);
				if (tmpep == ep) {
					list_del(pos);
					break;
				}
			}
			spin_unlock_irqrestore(&ms_info.mi_connlock, sflags);
			break;
		}

		spin_unlock(&ms_info.mi_connlock);

		msg.uop = SCIF_DISCNCT;
		msg.src = ep->port;
		msg.dst = ep->peer;
		msg.payload[0] = (uint64_t)ep;
		msg.payload[1] = ep->remote_ep;

		err = micscif_nodeqp_send(ep->remote_dev, &msg, ep);

		spin_unlock_irqrestore(&ep->lock, sflags);
		if (!err)
			/* Now wait for the remote node to respond */
			wait_event_timeout(ep->disconwq, 
				(ep->state == SCIFEP_DISCONNECTED), NODE_ALIVE_TIMEOUT);
		spin_lock_irqsave(&ms_info.mi_connlock, sflags);
		spin_lock(&ep->lock);
		list_add_tail(&ep->list, &ms_info.mi_disconnected);
		ep->state = SCIFEP_DISCONNECTED;
		spin_unlock(&ep->lock);
		spin_unlock_irqrestore(&ms_info.mi_connlock, sflags);
		// Wake up threads blocked in send and recv
		wake_up_interruptible(&ep->sendwq);
		wake_up_interruptible(&ep->recvwq);
		break;
	}
	case SCIFEP_LISTENING:
	{
		ep->state = SCIFEP_CLLISTEN;

		// If an accept is waiting wake it up
		wake_up_interruptible(&ep->conwq);
		spin_unlock_irqrestore(&ep->lock, sflags);
		break;
	}
	default:
		spin_unlock_irqrestore(&ep->lock, sflags);
		break;
	}
	micscif_dec_node_refcnt(ep->remote_dev, 1);
	return 0;
}

/**
 * scif_bind() - Bind a SCIF end point to a port ID.
 * @epd:        The end point address returned from scif_open()
 * @pn:         Port ID (number) to bind to
 *
 * Set the port ID associated with the end point and place it in the bound state.
 * If a port ID of zero is requested a non zero port ID is allocated for it.
 *
 * Upon successful compltion the port id (number) will be returned.
 *
 * If the end point is not in the unbound state then return -EISCONN.
 *
 * If port ID zero is specified and allocation of a port ID fails -ENOSPC
 * will be returned.
 */
int
__scif_bind(scif_epd_t epd, uint16_t pn)
{
	struct endpt *ep = (struct endpt *)epd;
	unsigned long sflags;
	int ret = 0;
	int tmp;

	pr_debug("SCIFAPI bind: ep %p %s requested port number %d\n", 
		    ep, scif_ep_states[ep->state], pn);

	might_sleep();

	if (pn) {
		/*
		 * Modeled on http://www.ietf.org/rfc/rfc1700.txt?number=1700
		 * SCIF ports below SCIF_ADMIN_PORT_END can only be bound by
		 * system (or root) processes or by processes executed by
		 * privileged users.
		 */
		if ( pn < SCIF_ADMIN_PORT_END && !capable(CAP_SYS_ADMIN)) {
			ret = -EACCES;
			goto scif_bind_admin_exit;
		}
	}

	spin_lock_irqsave(&ep->lock, sflags);
	if (ep->state == SCIFEP_BOUND) {
		ret = -EINVAL;
		goto scif_bind_exit;
	} else if (ep->state != SCIFEP_UNBOUND) {
		ret = -EISCONN;
		goto scif_bind_exit;
	}

	if (pn) {
		if ((tmp = rsrv_scif_port(pn)) != pn) {
			ret = -EINVAL;
			goto scif_bind_exit;
		}
	} else {
		pn = get_scif_port();
		if (!pn) {
			ret = -ENOSPC;
			goto scif_bind_exit;
		}
	}

	ep->state = SCIFEP_BOUND;
	ep->port.node = ms_info.mi_nodeid;
	ep->port.port = pn;
	ep->conn_async_state = ASYNC_CONN_IDLE;
	ret = pn;
	pr_debug("SCIFAPI bind: bound to port number %d\n", pn);

scif_bind_exit:
	spin_unlock_irqrestore(&ep->lock, sflags);
scif_bind_admin_exit:
	return ret;
}

int
scif_bind(scif_epd_t epd, uint16_t pn)
{
	int ret;
	get_kref_count(epd);
	ret = __scif_bind(epd, pn);
	put_kref_count(epd);
	return ret;
}
EXPORT_SYMBOL(scif_bind);

/**
 * scif_listen() - Place the end point in the listening state
 * @epd:        The end point address returned from scif_open()
 * @backlog:	Maximum number of pending connection requests.
 *
 * The end point is placed in the listening state ready to accept connection
 * requests.  The backlog paramter is saved to indicate the maximun number of
 * connection requests from the remote node to save.  The end point is
 * placed on a list of listening end points to allow a connection request to
 * find it.
 *
 * Upon successful completion a zero is returned.
 *
 * If the end point is not in the bound state -EINVAL or -EISCONN is returned.
 *
 */
int
__scif_listen(scif_epd_t epd, int backlog)
{
	struct endpt *ep = (struct endpt *)epd;
	unsigned long sflags;

	pr_debug("SCIFAPI listen: ep %p %s\n", ep, scif_ep_states[ep->state]);

	might_sleep();
	spin_lock_irqsave(&ep->lock, sflags);
	switch (ep->state) {
	case SCIFEP_ZOMBIE:
		BUG_ON(SCIFEP_ZOMBIE == ep->state);
	case SCIFEP_CLOSED:
	case SCIFEP_CLOSING:
	case SCIFEP_CLLISTEN:
	case SCIFEP_UNBOUND:
	case SCIFEP_DISCONNECTED:
		spin_unlock_irqrestore(&ep->lock, sflags);
		return -EINVAL;
	case SCIFEP_LISTENING:
	case SCIFEP_CONNECTED:
	case SCIFEP_CONNECTING:
	case SCIFEP_MAPPING:
		spin_unlock_irqrestore(&ep->lock, sflags);
		return -EISCONN;
	case SCIFEP_BOUND:
		break;
	}

	ep->state = SCIFEP_LISTENING;
	ep->backlog = backlog;

	ep->conreqcnt = 0;
	ep->acceptcnt = 0;
	INIT_LIST_HEAD(&ep->conlist);	// List of connection requests
	init_waitqueue_head(&ep->conwq);	// Wait for connection queue
	INIT_LIST_HEAD(&ep->li_accept);	// User ep list for ACCEPTREG calls
	spin_unlock_irqrestore(&ep->lock, sflags);

	// Listen status is complete so delete the qp information not needed
	// on a listen before placing on the list of listening ep's
	micscif_teardown_ep((void *)ep);
	ep->qp_info.qp = NULL;

	spin_lock_irqsave(&ms_info.mi_eplock, sflags);
	list_add_tail(&ep->list, &ms_info.mi_listen);
	spin_unlock_irqrestore(&ms_info.mi_eplock, sflags);
	return 0;
}

int
scif_listen(scif_epd_t epd, int backlog)
{
	int ret;
	get_kref_count(epd);
	ret = __scif_listen(epd, backlog);
	put_kref_count(epd);
	return ret;
}
EXPORT_SYMBOL(scif_listen);

#ifdef _MIC_SCIF_
/*
 * scif_p2p_connect:
 * @node:     destination node id
 *
 * Try to setup a p2p connection between the current
 * node and the desitination node. We need host to
 * setup the initial p2p connections. So we send
 * this message to the host which acts like proxy
 * in setting up p2p connection.
 */
static int scif_p2p_connect(int node)
{
	struct micscif_dev *remote_dev = &scif_dev[node];
	struct nodemsg msg;
	int err;

	pr_debug("%s:%d SCIF_NODE_CONNECT to host\n", __func__, __LINE__);
	micscif_inc_node_refcnt(&scif_dev[SCIF_HOST_NODE], 1);

	msg.dst.node = SCIF_HOST_NODE;
	msg.payload[0] = node;
	msg.uop = SCIF_NODE_CONNECT;

	if ((err = micscif_nodeqp_send(&scif_dev[SCIF_HOST_NODE],
		&msg, NULL))) {
		printk(KERN_ERR "%s:%d error while sending SCIF_NODE_CONNECT to"
			" node %d\n", __func__, __LINE__, node);
		micscif_dec_node_refcnt(&scif_dev[SCIF_HOST_NODE], 1);
		goto error;
	}

	wait_event_interruptible_timeout(remote_dev->sd_p2p_wq, 
		(remote_dev->sd_state == SCIFDEV_RUNNING) ||
		(remote_dev->sd_state == SCIFDEV_NOTPRESENT), NODE_ALIVE_TIMEOUT);

	pr_debug("%s:%d SCIF_NODE_CONNECT state:%d\n", __func__, __LINE__, 
						remote_dev->sd_state);
	micscif_dec_node_refcnt(&scif_dev[SCIF_HOST_NODE], 1);
error:
	return err;
}
#endif

static int scif_conn_func(struct endpt *ep)
{
	int err = 0;
	struct nodemsg msg;
	unsigned long sflags;
	int term_sent = 0;

	if ((err = micscif_reserve_dma_chan(ep))) {
		printk(KERN_ERR "%s %d err %d\n", __func__, __LINE__, err);
		ep->state = SCIFEP_BOUND;
		goto connect_error_simple;
	}
	// Initiate the first part of the endpoint QP setup
	err = micscif_setup_qp_connect(ep->qp_info.qp, &ep->qp_info.qp_offset,
			ENDPT_QP_SIZE, ep->remote_dev);
	if (err) {
		printk(KERN_ERR "%s err %d qp_offset 0x%llx\n", 
			__func__, err, ep->qp_info.qp_offset);
		ep->state = SCIFEP_BOUND;
		goto connect_error_simple;
	}

	micscif_inc_node_refcnt(ep->remote_dev, 1);

	// Format connect message and send it
	msg.src = ep->port;
	msg.dst = ep->conn_port;
	msg.uop = SCIF_CNCT_REQ;
	msg.payload[0] = (uint64_t)ep;
	msg.payload[1] = ep->qp_info.qp_offset;
	if ((err = micscif_nodeqp_send(ep->remote_dev, &msg, ep))) {
		micscif_dec_node_refcnt(ep->remote_dev, 1);
		goto connect_error_simple;
	}
	// Wait for request to be processed.
	while ((err = wait_event_interruptible_timeout(ep->conwq, 
		(ep->state != SCIFEP_CONNECTING), NODE_ALIVE_TIMEOUT)) <= 0) {
		if (!err)
			err = -ENODEV;

		pr_debug("SCIFAPI connect: ep %p ^C detected\n", ep);
		// interrupted out of the wait
		if (!term_sent++) {
			int bak_err = err;
			msg.uop = SCIF_CNCT_TERM;
			if (!(err = micscif_nodeqp_send(ep->remote_dev, &msg, ep))) {
retry:
				err = wait_event_timeout(ep->diswq, 
					(ep->state != SCIFEP_CONNECTING), NODE_ALIVE_TIMEOUT);
				if (!err && scifdev_alive(ep))
					goto retry;
				if (!err)
					err = -ENODEV;
				if (err > 0)
					err = 0;
			}
			if (ep->state == SCIFEP_MAPPING) {
				micscif_setup_qp_connect_response(ep->remote_dev,
					ep->qp_info.qp, ep->qp_info.cnct_gnt_payload);
				// Send grant nack
				msg.uop = SCIF_CNCT_GNTNACK;
				msg.payload[0] = ep->remote_ep;
				/* No error handling for Notification messages */
				micscif_nodeqp_send(ep->remote_dev, &msg, ep);
			}
			// Ensure after that even after a timeout the state of the end point is bound
			ep->state = SCIFEP_BOUND;
			if (bak_err)
				err = bak_err;
			break;
		}
	}

	if (err > 0)
		err = 0;

	if (term_sent || err) {
		micscif_dec_node_refcnt(ep->remote_dev, 1);
		goto connect_error_simple;
	}

	if (ep->state == SCIFEP_MAPPING) {
		err = micscif_setup_qp_connect_response(ep->remote_dev,
			ep->qp_info.qp, ep->qp_info.cnct_gnt_payload);

		// If the resource to map the queue are not available then we need
		// to tell the other side to terminate the accept
		if (err) {
			printk(KERN_ERR "%s %d err %d\n", __func__, __LINE__, err);

			// Send grant nack
			msg.uop = SCIF_CNCT_GNTNACK;
			msg.payload[0] = ep->remote_ep;
			/* No error handling for Notification messages */
			micscif_nodeqp_send(ep->remote_dev, &msg, ep);

			ep->state = SCIFEP_BOUND;
			micscif_dec_node_refcnt(ep->remote_dev, 1);
			goto connect_error_simple;
		}

		// Send a grant ack to inform the accept we are done mapping its resources.
		msg.uop = SCIF_CNCT_GNTACK;
		msg.payload[0] = ep->remote_ep;
		if (!(err = micscif_nodeqp_send(ep->remote_dev, &msg, ep))) {
			ep->state = SCIFEP_CONNECTED;
			spin_lock_irqsave(&ms_info.mi_connlock, sflags);
			list_add_tail(&ep->list, &ms_info.mi_connected);
			get_conn_count(ep->remote_dev);
			spin_unlock_irqrestore(&ms_info.mi_connlock, sflags);
			pr_debug("SCIFAPI connect: ep %p connected\n", ep);
		} else
			ep->state = SCIFEP_BOUND;
		micscif_dec_node_refcnt(ep->remote_dev, 1);
		goto connect_error_simple;

	} else if (ep->state == SCIFEP_BOUND) {
		pr_debug("SCIFAPI connect: ep %p connection refused\n", ep);
		err = -ECONNREFUSED;
		micscif_dec_node_refcnt(ep->remote_dev, 1);
		goto connect_error_simple;

	} else {
		pr_debug("SCIFAPI connect: ep %p connection interrupted\n", ep);
		err = -EINTR;
		micscif_dec_node_refcnt(ep->remote_dev, 1);
		goto connect_error_simple;
	}
	micscif_dec_node_refcnt(ep->remote_dev, 1);
connect_error_simple:
	return err;
}

/*
 * micscif_conn_handler:
 *
 * Workqueue handler for servicing non-blocking SCIF connect
 *
 */
void micscif_conn_handler(struct work_struct *work)
{
	struct endpt *ep;

	do {
		ep = NULL;
		spin_lock(&ms_info.mi_nb_connect_lock);
		if (!list_empty(&ms_info.mi_nb_connect_list)) {
			ep = list_first_entry(&ms_info.mi_nb_connect_list, 
					struct endpt, conn_list);
			list_del(&ep->conn_list);
		}
		spin_unlock(&ms_info.mi_nb_connect_lock);
		if (ep) {
			ep->conn_err = scif_conn_func(ep);
			wake_up_interruptible(&ep->conn_pend_wq);
		}
	} while (ep);
}

/**
 * scif_connect() - Request a connection to a remote node
 * @epd:        The end point address returned from scif_open()
 * @dst:	Remote note address informtion
 *
 * The function requests a scif connection to the remote node
 * identified by the dst parameter.  "dst" contains the remote node and
 * port ids.
 *
 * Upon successful complete a zero will be returned.
 *
 * If the end point is not in the bound state -EINVAL will be returned.
 *
 * If during the connection sequence resource allocation fails the -ENOMEM
 * will be returned.
 *
 * If the remote side is not responding to connection requests the caller may
 * terminate this funciton with a signal.  If so a -EINTR will be returned.
 */
int
__scif_connect(scif_epd_t epd, struct scif_portID *dst, bool non_block)
{
	struct endpt *ep = (struct endpt *)epd;
	unsigned long sflags;
	int err = 0;
#ifdef _MIC_SCIF_
	struct micscif_dev *remote_dev;
#endif

	pr_debug("SCIFAPI connect: ep %p %s\n", ep, 
					scif_ep_states[ep->state]);

	if (dst->node > MAX_BOARD_SUPPORTED)
		return -ENODEV;

	might_sleep();

#ifdef _MIC_SCIF_
	remote_dev = &scif_dev[dst->node];
	if ((SCIFDEV_INIT == remote_dev->sd_state ||
		SCIFDEV_STOPPED == remote_dev->sd_state) && mic_p2p_enable)
		if ((err = scif_p2p_connect(dst->node)))
			return err;
#endif

	if (SCIFDEV_RUNNING != scif_dev[dst->node].sd_state &&
		SCIFDEV_SLEEPING != scif_dev[dst->node].sd_state)
		return -ENODEV;

	spin_lock_irqsave(&ep->lock, sflags);
	switch (ep->state) {
	case SCIFEP_ZOMBIE:
		BUG_ON(SCIFEP_ZOMBIE == ep->state);

	case SCIFEP_CLOSED:
	case SCIFEP_CLOSING:
		err = -EINVAL;
		break;

	case SCIFEP_DISCONNECTED:
		if (ep->conn_async_state == ASYNC_CONN_INPROGRESS)
			ep->conn_async_state = ASYNC_CONN_FLUSH_WORK;
		else
			err = -EINVAL;
		break;

	case SCIFEP_LISTENING:
	case SCIFEP_CLLISTEN:
		err = -EOPNOTSUPP;
		break;

	case SCIFEP_CONNECTING:
	case SCIFEP_MAPPING:
		if (ep->conn_async_state == ASYNC_CONN_INPROGRESS)
			err = -EINPROGRESS;
		else
			err = -EISCONN;
		break;

	case SCIFEP_CONNECTED:
		if (ep->conn_async_state == ASYNC_CONN_INPROGRESS)
			ep->conn_async_state = ASYNC_CONN_FLUSH_WORK;
		else
			err = -EISCONN;
		break;

	case SCIFEP_UNBOUND:
		if ((ep->port.port = get_scif_port()) == 0)
			err = -ENOSPC;
		else {
			ep->port.node = ms_info.mi_nodeid;
			ep->conn_async_state = ASYNC_CONN_IDLE;
		}
		/* Fall through */
	case SCIFEP_BOUND:
		/*
		 * If a non-blocking connect has been already initiated (conn_async_state
		 * is either ASYNC_CONN_INPROGRESS or ASYNC_CONN_FLUSH_WORK), the end point
		 * could end up in SCIF_BOUND due an error in the connection
		 * process (e.g., connnection refused)
		 * If conn_async_state is ASYNC_CONN_INPROGRESS - transition to
		 * ASYNC_CONN_FLUSH_WORK so that the error status can be collected.
		 * If the state is already ASYNC_CONN_FLUSH_WORK - then set the error
		 * to EINPROGRESS since some other thread is waiting to collect error status.
		 */
		if (ep->conn_async_state == ASYNC_CONN_INPROGRESS)
			ep->conn_async_state = ASYNC_CONN_FLUSH_WORK;
		else if (ep->conn_async_state == ASYNC_CONN_FLUSH_WORK)
			err = -EINPROGRESS;
		else {
			ep->conn_port = *dst;
			init_waitqueue_head(&ep->sendwq);
			init_waitqueue_head(&ep->recvwq);
			init_waitqueue_head(&ep->conwq);
			init_waitqueue_head(&ep->diswq);
			ep->conn_async_state = 0;

			if (unlikely(non_block))
				ep->conn_async_state = ASYNC_CONN_INPROGRESS;
		}
		break;
	}

	if (err || ep->conn_async_state == ASYNC_CONN_FLUSH_WORK)
			goto connect_simple_unlock1;

	ep->state = SCIFEP_CONNECTING;
	ep->remote_dev = &scif_dev[dst->node];
	ep->sd_state = SCIFDEV_RUNNING;
	ep->qp_info.qp->magic = SCIFEP_MAGIC;
	ep->qp_info.qp->ep = (uint64_t)ep;
	if (ep->conn_async_state == ASYNC_CONN_INPROGRESS) {
		init_waitqueue_head(&ep->conn_pend_wq);
		spin_lock(&ms_info.mi_nb_connect_lock);
		list_add_tail(&ep->conn_list, 
				&ms_info.mi_nb_connect_list);
		spin_unlock(&ms_info.mi_nb_connect_lock);
		err = -EINPROGRESS;
		queue_work(ms_info.mi_conn_wq, &ms_info.mi_conn_work);
	}
connect_simple_unlock1:
	spin_unlock_irqrestore(&ep->lock, sflags);

	if (err)
		return err;
	else if (ep->conn_async_state == ASYNC_CONN_FLUSH_WORK) {
		flush_workqueue(ms_info.mi_conn_wq);
		err = ep->conn_err;
		spin_lock_irqsave(&ep->lock, sflags);
		ep->conn_async_state = ASYNC_CONN_IDLE;
		spin_unlock_irqrestore(&ep->lock, sflags);
	} else {
		err = scif_conn_func(ep);
	}
	return err;
}

int
scif_connect(scif_epd_t epd, struct scif_portID *dst)
{
	int ret;
	get_kref_count(epd);
	ret = __scif_connect(epd, dst, false);
	put_kref_count(epd);
	return ret;
}
EXPORT_SYMBOL(scif_connect);

/**
 * scif_accept() - Accept a connection request from the remote node
 * @epd:        The end point address returned from scif_open()
 * @peer:       Filled in with pear node and port information
 * @newepd:     New end point created for connection
 * @flags:      Indicates sychronous or asynchronous mode
 *
 * The function accepts a connection request from the remote node.  Successful
 * complete is indicate by a new end point being created and passed back
 * to the caller for future reference.
 *
 * Upon successful complete a zero will be returned and the peer information
 * will be filled in.
 *
 * If the end point is not in the listening state -EINVAL will be returned.
 *
 * If during the connection sequence resource allocation fails the -ENOMEM
 * will be returned.
 *
 * If the function is called asynchronously and not connection request are
 * pending it will return -EAGAIN.
 *
 * If the remote side is not sending any connection requests the caller may
 * terminate this funciton with a signal.  If so a -EINTR will be returned.
 */
int
__scif_accept(scif_epd_t epd, struct scif_portID *peer, scif_epd_t *newepd, int flags)
{
	struct endpt *lep = (struct endpt *)epd;
	struct endpt *cep;
	struct conreq *conreq;
	struct nodemsg msg;
	unsigned long sflags;
	int err;

	pr_debug("SCIFAPI accept: ep %p %s\n", lep, scif_ep_states[lep->state]);

	// Error if flags other than SCIF_ACCEPT_SYNC are set
	if (flags & ~SCIF_ACCEPT_SYNC) {
		pr_debug("SCIFAPI accept: ep %p invalid flags %x\n", lep, flags & ~SCIF_ACCEPT_SYNC);
		return -EINVAL;
	}

	if (!peer || !newepd) {
		pr_debug("SCIFAPI accept: ep %p peer %p or newepd %p NULL\n", 
			lep, peer, newepd);
		return -EINVAL;
	}

	might_sleep();
	spin_lock_irqsave(&lep->lock, sflags);
	if (lep->state != SCIFEP_LISTENING) {
		pr_debug("SCIFAPI accept: ep %p not listending\n", lep);
		spin_unlock_irqrestore(&lep->lock, sflags);
		return -EINVAL;
	}

	if (!lep->conreqcnt && !(flags & SCIF_ACCEPT_SYNC)) {
		// No connection request present and we do not want to wait
		pr_debug("SCIFAPI accept: ep %p async request with nothing pending\n", lep);
		spin_unlock_irqrestore(&lep->lock, sflags);
		return -EAGAIN;
	}

retry_connection:
	spin_unlock_irqrestore(&lep->lock, sflags);
	lep->files = current ? current->files : NULL;
	if ((err = wait_event_interruptible(lep->conwq, 
		(lep->conreqcnt || (lep->state != SCIFEP_LISTENING)))) != 0) {
		// wait was interrupted
		pr_debug("SCIFAPI accept: ep %p ^C detected\n", lep);
		return err;	// -ERESTARTSYS
	}

	if (lep->state != SCIFEP_LISTENING) {
		return -EINTR;
	}

	spin_lock_irqsave(&lep->lock, sflags);

	if (!lep->conreqcnt) {
		goto retry_connection;
	}

	// Get the first connect request off the list
	conreq = list_first_entry(&lep->conlist, struct conreq, list);
	list_del(&conreq->list);
	lep->conreqcnt--;
	spin_unlock_irqrestore(&lep->lock, sflags);

	// Fill in the peer information
	peer->node = conreq->msg.src.node;
	peer->port = conreq->msg.src.port;

	// Create the connection endpoint
	cep = (struct endpt *)kzalloc(sizeof(struct endpt), GFP_KERNEL);
	if (!cep) {
		pr_debug("SCIFAPI accept: ep %p new end point allocation failed\n", lep);
		err = -ENOMEM;
		goto scif_accept_error_epalloc;
	}
	spin_lock_init(&cep->lock);
	mutex_init (&cep->sendlock);
	mutex_init (&cep->recvlock);
	cep->state = SCIFEP_CONNECTING;
	cep->remote_dev = &scif_dev[peer->node];
	cep->remote_ep = conreq->msg.payload[0];
	cep->sd_state = SCIFDEV_RUNNING;

	if (!scifdev_alive(cep)) {
		err = -ENODEV;
		printk(KERN_ERR "%s %d err %d\n", __func__, __LINE__, err);
		goto scif_accept_error_qpalloc;
	}

	if (micscif_rma_ep_init(cep) < 0) {
		pr_debug("SCIFAPI accept: ep %p new %p RMA EP init failed\n", lep, cep);
		err = -ENOMEM;
		goto scif_accept_error_qpalloc;
	}

	if ((err = micscif_reserve_dma_chan(cep))) {
		printk(KERN_ERR "%s %d err %d\n", __func__, __LINE__, err);
		goto scif_accept_error_qpalloc;
	}

	cep->qp_info.qp = (struct micscif_qp *)kzalloc(sizeof(struct micscif_qp), GFP_KERNEL);
	if (!cep->qp_info.qp) {
		printk(KERN_ERR "Port Qp Allocation Failed\n");
		err = -ENOMEM;
		goto scif_accept_error_qpalloc;
	}

	cep->qp_info.qp->magic = SCIFEP_MAGIC;
	cep->qp_info.qp->ep = (uint64_t)cep;
	micscif_inc_node_refcnt(cep->remote_dev, 1);
	err = micscif_setup_qp_accept(cep->qp_info.qp, &cep->qp_info.qp_offset,
		conreq->msg.payload[1], ENDPT_QP_SIZE, cep->remote_dev);
	if (err) {
		pr_debug("SCIFAPI accept: ep %p new %p micscif_setup_qp_accept %d qp_offset 0x%llx\n", 
			    lep, cep, err, cep->qp_info.qp_offset);
		micscif_dec_node_refcnt(cep->remote_dev, 1);
		goto scif_accept_error_map;
	}

	cep->port.node = lep->port.node;
	cep->port.port = lep->port.port;
	cep->peer.node = peer->node;
	cep->peer.port = peer->port;
	cep->accepted_ep = true;
	init_waitqueue_head(&cep->sendwq); // Wait for data to be consumed
	init_waitqueue_head(&cep->recvwq); // Wait for data to be produced
	init_waitqueue_head(&cep->conwq);  // Wait for connection request

	// Return the grant message
	msg.uop = SCIF_CNCT_GNT;
	msg.src = cep->port;
	msg.payload[0] = cep->remote_ep;
	msg.payload[1] = cep->qp_info.qp_offset;
	msg.payload[2] = (uint64_t)cep;

	err = micscif_nodeqp_send(cep->remote_dev, &msg, cep);

	micscif_dec_node_refcnt(cep->remote_dev, 1);
	if (err)
		goto scif_accept_error_map;
retry:
	err = wait_event_timeout(cep->conwq, 
		(cep->state != SCIFEP_CONNECTING), NODE_ACCEPT_TIMEOUT);
	if (!err && scifdev_alive(cep))
		goto retry;

	if (!err) {
		err = -ENODEV;
		goto scif_accept_error_map;
	}

	if (err > 0)
		err = 0;

	kfree(conreq);

	spin_lock_irqsave(&cep->lock, sflags);

	if (cep->state == SCIFEP_CONNECTED) {
		// Connect sequence complete return new endpoint information
		*newepd = (scif_epd_t)cep;
		spin_unlock_irqrestore(&cep->lock, sflags);
		pr_debug("SCIFAPI accept: ep %p new %p returning new epnd point\n", lep, cep);
		return 0;
	}

	if (cep->state == SCIFEP_CLOSING) {
		// Remote failed to allocate resources and NAKed the grant.
		// There is at this point nothing referencing the new end point.
		spin_unlock_irqrestore(&cep->lock, sflags);
		micscif_teardown_ep((void *)cep);
		kfree(cep);

		// If call with sync flag then go back and wait.
		if (flags & SCIF_ACCEPT_SYNC) {
			spin_lock_irqsave(&lep->lock, sflags);
			goto retry_connection;
		}

		pr_debug("SCIFAPI accept: ep %p new %p remote failed to allocate resources\n", lep, cep);
		return -EAGAIN;
	}

	// While connect was in progress the other side closed and sent a disconnect
	// so set the end point status to closed but return anyway.  This will allow
	// the caller to drain anything the other side may have put in the message queue.
	*newepd = (scif_epd_t)cep;
	spin_unlock_irqrestore(&cep->lock, sflags);
	return 0;

	// Error allocating or mapping resources
scif_accept_error_map:
	kfree(cep->qp_info.qp);

scif_accept_error_qpalloc:
	kfree(cep);

scif_accept_error_epalloc:
	micscif_inc_node_refcnt(&scif_dev[conreq->msg.src.node], 1);
	// New reject the connection request due to lack of resources
	msg.uop = SCIF_CNCT_REJ;
	msg.dst.node = conreq->msg.src.node;
	msg.dst.port = conreq->msg.src.port;
	msg.payload[0] = conreq->msg.payload[0];
	msg.payload[1] = conreq->msg.payload[1];
	/* No error handling for Notification messages */
	micscif_nodeqp_send(&scif_dev[conreq->msg.src.node], &msg, NULL);
	micscif_dec_node_refcnt(&scif_dev[conreq->msg.src.node], 1);

	kfree(conreq);
	return err;
}

int
scif_accept(scif_epd_t epd, struct scif_portID *peer, scif_epd_t *newepd, int flags)
{
	int ret;
	get_kref_count(epd);
	ret = __scif_accept(epd, peer, newepd, flags);
	if (ret == 0) {
		kref_init(&((*newepd)->ref_count));
	}
	put_kref_count(epd);
	return ret;
}
EXPORT_SYMBOL(scif_accept);

/*
 * scif_msg_param_check:
 * @epd:        The end point address returned from scif_open()
 * @len:	Length to receive
 * @flags:	Syncronous or asynchronous access
 *
 * Validate parameters for messaging APIs scif_send(..)/scif_recv(..).
 */
static inline int
scif_msg_param_check(scif_epd_t epd, int len, int flags)
{
	int ret = -EINVAL;

	if (len < 0)
		goto err_ret;

	if (flags && (!(flags & SCIF_RECV_BLOCK)))
		goto err_ret;

	ret = 0;

err_ret:
	return ret;
}

#define SCIF_BLAST     (1 << 1)	/* Use bit 1 of flags field */

#ifdef SCIF_BLAST
/*
 * Added a temporary implementation of the exception path.
 * The cost to the normal path is 1 local variable (set once and
 * tested once) plus 2 tests for the 'blast' flag.
 * This only apply to the card side kernel API.
 */
#ifndef _MIC_SCIF_
#undef SCIF_BLAST
#endif
#endif

/**
 * _scif_send() - Send data to connection queue
 * @epd:        The end point address returned from scif_open()
 * @msg:	Address to place data
 * @len:	Length to receive
 * @flags:	Syncronous or asynchronous access
 *
 * This function sends a packet of data to the queue * created by the
 * connection establishment sequence.  It returns when the packet has
 * been completely sent.
 *
 * Successful completion returns the number of bytes sent.
 *
 * If the end point is not in the connect state returns -ENOTCONN;
 *
 * This function may be interrupted by a signal and will return -EINTR.
 */
int
_scif_send(scif_epd_t epd, void *msg, int len, int flags)
{
	struct endpt *ep = (struct endpt *)epd;
	struct nodemsg notif_msg;
	unsigned long sflags;
	size_t curr_xfer_len = 0;
	size_t sent_len = 0;
	size_t write_count;
	int ret;
#ifdef SCIF_BLAST
	int tl;
#endif

	if (flags & SCIF_SEND_BLOCK)
		might_sleep();

#ifdef SCIF_BLAST
	if (flags & SCIF_BLAST) {
		/*
		 * Do a decent try to acquire lock (~100 uSec)
		 */
		for (ret = tl = 0; ret < 100 && !tl; ret++) {
			tl = spin_trylock_irqsave(&ep->lock, sflags);
			cpu_relax();
		}
	} else {
		tl = 1;
		spin_lock_irqsave(&ep->lock, sflags);
	}
#else
	spin_lock_irqsave(&ep->lock, sflags);
#endif

	while (sent_len != len) {
		if (ep->state == SCIFEP_DISCONNECTED) {
			ret = (int)(sent_len ? sent_len : -ECONNRESET);
			goto unlock_dec_return;
		}
		if (ep->state != SCIFEP_CONNECTED) {
			ret = (int)(sent_len ? sent_len : -ENOTCONN);
			goto unlock_dec_return;
		}
		if (!scifdev_alive(ep)) {
			ret = (int) (sent_len ? sent_len : -ENODEV);
			goto unlock_dec_return;
		}
		write_count = micscif_rb_space(&ep->qp_info.qp->outbound_q);
		if (write_count) {
			/*
			 * Best effort to send as much data as there
			 * is space in the RB particularly important for the
			 * Non Blocking case.
			 */
			curr_xfer_len = min(len - sent_len, write_count);
			ret = micscif_rb_write(&ep->qp_info.qp->outbound_q, msg,
						(uint32_t)curr_xfer_len);
			if (ret < 0) {
				ret = -EFAULT;
				goto unlock_dec_return;
			}
			if (ret) {
				spin_unlock_irqrestore(&ep->lock, sflags);
				/*
				 * If there is space in the RB and we have the
				 * EP lock held then writing to the RB should
				 * succeed. Releasing spin lock before asserting
				 * to avoid deadlocking the system.
				 */
				BUG_ON(ret);
			}
			/*
			 * Success. Update write pointer.
			 */
			micscif_rb_commit(&ep->qp_info.qp->outbound_q);
#ifdef SCIF_BLAST
			if (flags & SCIF_BLAST) {
				/*
				 * Bypass-path; set flag int the host side node_qp
				 * and ring the doorbell. Host will wake-up all
				 * listeners, such that the message will be seen.
				 * Need micscif_send_host_intr() to be non-static.
				 */
				extern int micscif_send_host_intr(struct micscif_dev *, uint32_t);
				ep->remote_dev->qpairs->remote_qp->blast = 1;
				smp_wmb();    /* Sufficient or need sfence? */
				micscif_send_host_intr(ep->remote_dev, 0);
			} else {
				/*
				 * Normal path: send notification on the
				 * node_qp ring buffer and ring the doorbell.
				 */
				notif_msg.src = ep->port;
				notif_msg.uop = SCIF_CLIENT_SENT;
				notif_msg.payload[0] = ep->remote_ep;
				if ((ret = micscif_nodeqp_send(ep->remote_dev, &notif_msg, ep))) {
					ret = sent_len ? sent_len : ret;
					goto unlock_dec_return;
				}
			}
#else
			/*
			 * Send a notification to the peer about the
			 * produced data message.
			 */
			notif_msg.src = ep->port;
			notif_msg.uop = SCIF_CLIENT_SENT;
			notif_msg.payload[0] = ep->remote_ep;
			if ((ret = micscif_nodeqp_send(ep->remote_dev, &notif_msg, ep))) {
				ret = (int)(sent_len ? sent_len : ret);
				goto unlock_dec_return;
			}
#endif
			sent_len += curr_xfer_len;
			msg = (char *)msg + curr_xfer_len;
			continue;
		}
		curr_xfer_len = min(len - sent_len, (size_t)(ENDPT_QP_SIZE - 1));
		/*
		 * Not enough space in the RB. Return in the Non Blocking case.
		 */
		if (!(flags & SCIF_SEND_BLOCK)) {
			ret = (int)sent_len;
			goto unlock_dec_return;
		}
#ifdef SCIF_BLAST
		/*
		 * Flags SCIF_BLAST and SCIF_SEND_BLOCK are mutually
		 * exclusive, so if we get here we know that SCIF_BLAST
		 * was not set and thus we _do_ have the spinlock.
		 * No need to check variable tl here
		 */
#endif
		spin_unlock_irqrestore(&ep->lock, sflags);
		/*
		 * Wait for a message now in the Blocking case.
		 */
		if ((ret = wait_event_interruptible(ep->sendwq, 
			(SCIFEP_CONNECTED != ep->state) ||
			(micscif_rb_space(&ep->qp_info.qp->outbound_q)
				>= curr_xfer_len) || (!scifdev_alive(ep))))) {
			ret = (int) (sent_len ? sent_len : ret);
			goto dec_return;
		}
		spin_lock_irqsave(&ep->lock, sflags);
	}
	ret = len;
unlock_dec_return:
#ifdef SCIF_BLAST
	if (tl)
#endif
	spin_unlock_irqrestore(&ep->lock, sflags);
dec_return:
	return ret;
}

/**
 * _scif_recv() - Recieve data from connection queue
 * @epd:        The end point address returned from scif_open()
 * @msg:	Address to place data
 * @len:	Length to receive
 * @flags:	Syncronous or asynchronous access
 * @touser: package send to user buffer or kernel
 *
 * This function requests to receive a packet of data from the queue
 * created by the connection establishment sequence.  It reads the amount
 * of data requested before returning.
 *
 * This function differs from the scif_send() by also returning data if the
 * end point is in the disconnected state and data is present.
 *
 * Successful completion returns the number of bytes read.
 *
 * If the end point is not in the connect state or in the disconnected state
 * with data prosent it returns -ENOTCONN;
 *
 * This function may be interrupted by a signal and will return -EINTR.
 */
int
_scif_recv(scif_epd_t epd, void *msg, int len, int flags)
{
	int read_size;
	struct endpt *ep = (struct endpt *)epd;
	unsigned long sflags;
	struct nodemsg notif_msg;
	size_t curr_recv_len = 0;
	size_t remaining_len = len;
	size_t read_count;
	int ret;

	if (flags & SCIF_RECV_BLOCK)
		might_sleep();

	micscif_inc_node_refcnt(ep->remote_dev, 1);
	spin_lock_irqsave(&ep->lock, sflags);
	while (remaining_len) {
		if (ep->state != SCIFEP_CONNECTED &&
			ep->state != SCIFEP_DISCONNECTED) {
			ret = (int) (len - remaining_len) ?
				(int) (len - remaining_len) : -ENOTCONN;
			goto unlock_dec_return;
		}
		read_count = micscif_rb_count(&ep->qp_info.qp->inbound_q,
					(int) remaining_len);
		if (read_count) {
			/*
			 * Best effort to recv as much data as there
			 * are bytes to read in the RB particularly
			 * important for the Non Blocking case.
			 */
			curr_recv_len = min(remaining_len, read_count);
			read_size = micscif_rb_get_next(
					&ep->qp_info.qp->inbound_q,
					msg, (int) curr_recv_len);
			if (read_size < 0){
				/* only could happen when copy to USER buffer
				*/
				ret = -EFAULT;
				goto unlock_dec_return;
			}
			if (read_size != curr_recv_len) {
				spin_unlock_irqrestore(&ep->lock, sflags);
				/*
				 * If there are bytes to be read from the RB and
				 * we have the EP lock held then reading from
				 * RB should succeed. Releasing spin lock before
				 * asserting to avoid deadlocking the system.
				 */
				BUG_ON(read_size != curr_recv_len);
			}
			if (ep->state == SCIFEP_CONNECTED) {
				/*
				 * Update the read pointer only if the endpoint is
				 * still connected else the read pointer might no
				 * longer exist since the peer has freed resources!
				 */
				micscif_rb_update_read_ptr(&ep->qp_info.qp->inbound_q);
				/*
				 * Send a notification to the peer about the
				 * consumed data message only if the EP is in
				 * SCIFEP_CONNECTED state.
				 */
				notif_msg.src = ep->port;
				notif_msg.uop = SCIF_CLIENT_RCVD;
				notif_msg.payload[0] = ep->remote_ep;
				if ((ret = micscif_nodeqp_send(ep->remote_dev, &notif_msg, ep))) {
					ret = (len - (int)remaining_len) ?
						(len - (int)remaining_len) : ret;
					goto unlock_dec_return;
				}
			}
			remaining_len -= curr_recv_len;
			msg = (char *)msg + curr_recv_len;
			continue;
		}
		curr_recv_len = min(remaining_len, (size_t)(ENDPT_QP_SIZE - 1));
		/*
		 * Bail out now if the EP is in SCIFEP_DISCONNECTED state else
		 * we will keep looping forever.
		 */
		if (ep->state == SCIFEP_DISCONNECTED) {
			ret = (len - (int)remaining_len) ?
				(len - (int)remaining_len) : -ECONNRESET;
			goto unlock_dec_return;
		}
		/*
		 * Return in the Non Blocking case if there is no data
		 * to read in this iteration.
		 */
		if (!(flags & SCIF_RECV_BLOCK)) {
			ret = len - (int)remaining_len;
			goto unlock_dec_return;
		}
		spin_unlock_irqrestore(&ep->lock, sflags);
		micscif_dec_node_refcnt(ep->remote_dev, 1);
		/*
		 * Wait for a message now in the Blocking case.
		 * or until other side disconnects.
		 */
		if ((ret = wait_event_interruptible(ep->recvwq, 
				(SCIFEP_CONNECTED != ep->state) ||
				(micscif_rb_count(&ep->qp_info.qp->inbound_q,
				 curr_recv_len) >= curr_recv_len) || (!scifdev_alive(ep))))) {
			ret = (len - remaining_len) ?
				(len - (int)remaining_len) : ret;
			goto dec_return;
		}
		micscif_inc_node_refcnt(ep->remote_dev, 1);
		spin_lock_irqsave(&ep->lock, sflags);
	}
	ret = len;
unlock_dec_return:
	spin_unlock_irqrestore(&ep->lock, sflags);
	micscif_dec_node_refcnt(ep->remote_dev, 1);
dec_return:
	return ret;
}


/**
 * scif_user_send() - Send data to connection queue
 * @epd:        The end point address returned from scif_open()
 * @msg:	Address to place data
 * @len:	Length to receive
 * @flags:	Syncronous or asynchronous access
 *
 * This function is called from the driver IOCTL entry point
 * only and is a wrapper for _scif_send().
 */
int
scif_user_send(scif_epd_t epd, void *msg, int len, int flags)
{
	struct endpt *ep = (struct endpt *)epd;
	int err = 0;
	int sent_len = 0;
	char *tmp;
	int loop_len;
	int chunk_len = min(len, (1 << (MAX_ORDER + PAGE_SHIFT - 1)));;
	pr_debug("SCIFAPI send (U): ep %p %s\n", ep, scif_ep_states[ep->state]);

	if (!len)
		return 0;

	if ((err = scif_msg_param_check(epd, len, flags)))
		goto send_err;

	if (!(tmp = kmalloc(chunk_len, GFP_KERNEL))) {
		err = -ENOMEM;
		goto send_err;
	}
	err = 0;
	micscif_inc_node_refcnt(ep->remote_dev, 1);
	/*
	 * Grabbing the lock before breaking up the transfer in
	 * multiple chunks is required to ensure that messages do
	 * not get fragmented and reordered.
	 */
	mutex_lock(&ep->sendlock);
	
	while (sent_len != len) {
		msg = (void *)((char *)msg + err);
		loop_len = len - sent_len;
		loop_len = min(chunk_len, loop_len);
		if (copy_from_user(tmp, msg, loop_len)) {
			err = -EFAULT;
			goto send_free_err;
		}
		err = _scif_send(epd, (void *)tmp, loop_len, flags);
		if (err < 0) {
			goto send_free_err;
		}
		sent_len += err;
		if (err !=loop_len) {
			goto send_free_err;
		}
	}
send_free_err:
	mutex_unlock(&ep->sendlock);
	micscif_dec_node_refcnt(ep->remote_dev, 1);
	kfree(tmp);
send_err:
	return err < 0 ? err : sent_len;
}

/**
 * scif_user_recv() - Recieve data from connection queue
 * @epd:        The end point address returned from scif_open()
 * @msg:	Address to place data
 * @len:	Length to receive
 * @flags:	Syncronous or asynchronous access
 *
 * This function is called from the driver IOCTL entry point
 * only and is a wrapper for _scif_recv().
 */
int
scif_user_recv(scif_epd_t epd, void *msg, int len, int flags)
{
	struct endpt *ep = (struct endpt *)epd;
	int err = 0;
	int recv_len = 0;
	char *tmp;
	int loop_len;
	int chunk_len = min(len, (1 << (MAX_ORDER + PAGE_SHIFT - 1)));;
	pr_debug("SCIFAPI recv (U): ep %p %s\n", ep, scif_ep_states[ep->state]);

	if (!len)
		return 0;

	if ((err = scif_msg_param_check(epd, len, flags)))
		goto recv_err;

	if (!(tmp = kmalloc(chunk_len, GFP_KERNEL))) {
		err = -ENOMEM;
		goto recv_err;
	}
	err = 0;
	/*
	 * Grabbing the lock before breaking up the transfer in
	 * multiple chunks is required to ensure that messages do
	 * not get fragmented and reordered.
	 */
	mutex_lock(&ep->recvlock);

	while (recv_len != len) {
		msg = (void *)((char *)msg + err);
		loop_len = len - recv_len;
		loop_len = min(chunk_len, loop_len);
		if ((err = _scif_recv(epd, tmp, loop_len, flags)) < 0)
			goto recv_free_err;
		if (copy_to_user(msg, tmp, err)) {
			err = -EFAULT;
			goto recv_free_err;
		}
		recv_len += err;
		if (err !=loop_len) {
			goto recv_free_err;
		}
	}
recv_free_err:
	mutex_unlock(&ep->recvlock);
	kfree(tmp);
recv_err:
	return err < 0 ? err : recv_len;
}

#ifdef SCIF_BLAST
/*
 * Added a temporary implementation of the exception path.
 * The cost to the normal path testing of 2 flag bits instead
 * of just one and a change to condition for node-wakeup.
 */
#endif

/**
 * scif_send() - Send data to connection queue
 * @epd:        The end point address returned from scif_open()
 * @msg:	Address to place data
 * @len:	Length to receive
 * @flags:	Syncronous or asynchronous access
 *
 * This function is called from the kernel mode only and is
 * a wrapper for _scif_send().
 */
int
__scif_send(scif_epd_t epd, void *msg, int len, int flags)
{
	struct endpt *ep = (struct endpt *)epd;
	int ret;

	pr_debug("SCIFAPI send (K): ep %p %s\n", ep, scif_ep_states[ep->state]);
	if (!len)
		return 0;

#ifdef SCIF_BLAST
	/*
	 * KAA: this is same code as scif_msg_param_check(),
	 * but since that routine is shared with scif_recv
	 * I thought is safer to replicate code here.
	 */
	if (len < 0)
		return -EINVAL;

	if (flags && !(flags & (SCIF_SEND_BLOCK | SCIF_BLAST)))
		return -EINVAL;

	if ((flags & (SCIF_SEND_BLOCK | SCIF_BLAST)) ==
			(SCIF_SEND_BLOCK | SCIF_BLAST))
		return -EINVAL;
#else
	if ((ret = scif_msg_param_check(epd, len, flags)))
		return ret;
#endif
	/*
	 * Cannot block while waiting for node to wake up
	 * if non blocking messaging mode is requested. Return
	 * ENODEV if the remote node is idle.
	 */
	if (!(flags & SCIF_SEND_BLOCK) && ep->remote_dev &&
		SCIF_NODE_IDLE == atomic_long_read(
			&ep->remote_dev->scif_ref_cnt))
		return -ENODEV;

	micscif_inc_node_refcnt(ep->remote_dev, 1);

	/*
	 * Grab the mutex lock in the blocking case only
	 * to ensure messages do not get fragmented/reordered.
	 * The non blocking mode is protected using spin locks
	 * in _scif_send().
	 */
	if (flags & SCIF_SEND_BLOCK)
		mutex_lock(&ep->sendlock);

	ret = _scif_send(epd, msg, len, flags);

	if (flags & SCIF_SEND_BLOCK)
		mutex_unlock(&ep->sendlock);

	micscif_dec_node_refcnt(ep->remote_dev, 1);
	return ret;
}

int
scif_send(scif_epd_t epd, void *msg, int len, int flags)
{
	int ret;
	get_kref_count(epd);
	ret = __scif_send(epd, msg, len, flags);
	put_kref_count(epd);
	return ret;
}
EXPORT_SYMBOL(scif_send);

/**
 * scif_recv() - Recieve data from connection queue
 * @epd:        The end point address returned from scif_open()
 * @msg:	Address to place data
 * @len:	Length to receive
 * @flags:	Syncronous or asynchronous access
 *
 * This function is called from the kernel mode only and is
 * a wrapper for _scif_recv().
 */
int
__scif_recv(scif_epd_t epd, void *msg, int len, int flags)
{
	struct endpt *ep = (struct endpt *)epd;
	int ret;

	pr_debug("SCIFAPI recv (K): ep %p %s\n", ep, scif_ep_states[ep->state]);

	if (!len)
		return 0;

	if ((ret = scif_msg_param_check(epd, len, flags)))
		return ret;

	/*
	 * Cannot block while waiting for node to wake up
	 * if non blocking messaging mode is requested. Return
	 * ENODEV if the remote node is idle.
	 */
	if (!flags && ep->remote_dev &&
		SCIF_NODE_IDLE == atomic_long_read(
			&ep->remote_dev->scif_ref_cnt))
		return -ENODEV;

	/*
	 * Grab the mutex lock in the blocking case only
	 * to ensure messages do not get fragmented/reordered.
	 * The non blocking mode is protected using spin locks
	 * in _scif_send().
	 */
	if (flags & SCIF_RECV_BLOCK)
		mutex_lock(&ep->recvlock);

	ret = _scif_recv(epd, msg, len, flags);

	if (flags & SCIF_RECV_BLOCK)
		mutex_unlock(&ep->recvlock);

	return ret;
}

int
scif_recv(scif_epd_t epd, void *msg, int len, int flags)
{
	int ret;
	get_kref_count(epd);
	ret = __scif_recv(epd, msg, len, flags);
	put_kref_count(epd);
	return ret;
}
EXPORT_SYMBOL(scif_recv);

/**
 * __scif_pin_pages - __scif_pin_pages() pins the physical pages which back
 * the range of virtual address pages starting at addr and continuing for
 * len bytes. addr and len are constrained to be multiples of the page size.
 * A successful  scif_register() call returns an opaque pointer value
 * which may be used in subsequent calls to scif_register_pinned_pages().
 *
 * Return Values
 * Upon successful completion, __scif_pin_pages() returns a
 * scif_pinned_pages_t value else an apt error is returned as documented
 * in scif.h. Protections of the set of pinned pages are also returned by
 * reference via out_prot.
 */
int
__scif_pin_pages(void *addr, size_t len, int *out_prot,
		int map_flags, scif_pinned_pages_t *pages)
{
	struct scif_pinned_pages *pinned_pages;
	int nr_pages, err = 0, i;
	bool vmalloc_addr = false;
	bool try_upgrade = false;
	int prot = *out_prot;
	int ulimit = 0;
	struct mm_struct *mm = NULL;

	/* Unsupported flags */
	if (map_flags & ~(SCIF_MAP_KERNEL | SCIF_MAP_ULIMIT))
		return -EINVAL;
	ulimit = !!(map_flags & SCIF_MAP_ULIMIT);

	/* Unsupported protection requested */
	if (prot & ~(SCIF_PROT_READ | SCIF_PROT_WRITE))
		return -EINVAL;

	/* addr/len must be page aligned. len should be non zero */
	if ((!len) ||
		(align_low((uint64_t)addr, PAGE_SIZE) != (uint64_t)addr) ||
		(align_low((uint64_t)len, PAGE_SIZE) != (uint64_t)len))
		return -EINVAL;

	might_sleep();

	nr_pages = (int)(len >> PAGE_SHIFT);

	/* Allocate a set of pinned pages */
	if (!(pinned_pages = micscif_create_pinned_pages(nr_pages, prot)))
		return -ENOMEM;

	if (unlikely(map_flags & SCIF_MAP_KERNEL)) {
		if (is_vmalloc_addr(addr))
			vmalloc_addr = true;

		for (i = 0; i < nr_pages; i++) {
			if (unlikely(vmalloc_addr))
				pinned_pages->pages[i] =
					vmalloc_to_page((char *)addr + (i * PAGE_SIZE) );
			else
				pinned_pages->pages[i] =
					virt_to_page((char *)addr + (i * PAGE_SIZE) );
			pinned_pages->num_pages[i] = 1;
			pinned_pages->nr_contig_chunks++;
		}
		pinned_pages->nr_pages = nr_pages;
		pinned_pages->map_flags = SCIF_MAP_KERNEL;
	} else {
		if (prot == SCIF_PROT_READ)
			try_upgrade = true;
		prot |= SCIF_PROT_WRITE;
retry:
		mm = current->mm;
		down_write(&mm->mmap_sem);
		if (ulimit) {
			err = __scif_check_inc_pinned_vm(mm, nr_pages);
			if (err) {
				up_write(&mm->mmap_sem);
				pinned_pages->nr_pages = 0;
				goto error_unmap;
			}
		}

		pinned_pages->nr_pages = get_user_pages(
				current,
				mm,
				(uint64_t)addr,
				nr_pages,
				!!(prot & SCIF_PROT_WRITE),
				0,
				pinned_pages->pages,
				pinned_pages->vma);
		up_write(&mm->mmap_sem);
		if (nr_pages == pinned_pages->nr_pages) {
#ifdef RMA_DEBUG
			atomic_long_add_return(nr_pages, &ms_info.rma_pin_cnt);
#endif
			micscif_detect_large_page(pinned_pages, addr);
		} else {
			if (try_upgrade) {
				if (ulimit)
					__scif_dec_pinned_vm_lock(mm, nr_pages, 0);
#ifdef RMA_DEBUG
				WARN_ON(atomic_long_sub_return(1,
						&ms_info.rma_mm_cnt) < 0);
#endif
				/* Roll back any pinned pages */
				for (i = 0; i < pinned_pages->nr_pages; i++) {
					if (pinned_pages->pages[i])
						page_cache_release(pinned_pages->pages[i]);
				}
				prot &= ~SCIF_PROT_WRITE;
				try_upgrade = false;
				goto retry;
			}
		}
		pinned_pages->map_flags = 0;
	}

	if (pinned_pages->nr_pages < nr_pages) {
		err = -EFAULT;
		pinned_pages->nr_pages = nr_pages;
		goto dec_pinned;
	}

	*out_prot = prot;
	atomic_set(&pinned_pages->ref_count, nr_pages);
	*pages = pinned_pages;
	return err;
dec_pinned:
	if (ulimit)
		__scif_dec_pinned_vm_lock(mm, nr_pages, 0);
	/* Something went wrong! Rollback */
error_unmap:
	pinned_pages->nr_pages = nr_pages;
	micscif_destroy_pinned_pages(pinned_pages);
	*pages = NULL;
	pr_debug("%s %d err %d len 0x%lx\n", __func__, __LINE__, err, len);
	return err;

}

/**
 * scif_pin_pages - scif_pin_pages() pins the physical pages which back
 * the range of virtual address pages starting at addr and continuing for
 * len bytes. addr and len are constrained to be multiples of the page size.
 * A successful  scif_register() call returns an opaque pointer value
 * which may be used in subsequent calls to scif_register_pinned_pages().
 *
 * Return Values
 * Upon successful completion, scif_register() returns a
 * scif_pinned_pages_t value else an apt error is returned as documented
 * in scif.h
 */
int
scif_pin_pages(void *addr, size_t len, int prot,
		int map_flags, scif_pinned_pages_t *pages)
{
	return __scif_pin_pages(addr, len, &prot, map_flags, pages);
}
EXPORT_SYMBOL(scif_pin_pages);

/**
 * scif_unpin_pages: Unpin a set of pages
 *
 * Return Values:
 * Upon successful completion, scif_unpin_pages() returns 0;
 * else an apt error is returned as documented in scif.h
 */
int
scif_unpin_pages(scif_pinned_pages_t pinned_pages)
{
	int err = 0, ret;

	if (!pinned_pages || SCIFEP_MAGIC != pinned_pages->magic)
		return -EINVAL;

	ret = atomic_sub_return((int32_t)pinned_pages->nr_pages, 
			&pinned_pages->ref_count);
	BUG_ON(ret < 0);

	/*
	 * Destroy the window if the ref count for this set of pinned
	 * pages has dropped to zero. If it is positive then there is
	 * a valid registered window which is backed by these pages and
	 * it will be destroyed once all such windows are unregistered.
	 */
	if (!ret)
		err = micscif_destroy_pinned_pages(pinned_pages);

	return err;
}
EXPORT_SYMBOL(scif_unpin_pages);

/**
 * scif_register_pinned_pages: Mark a memory region for remote access.
 *
 * The scif_register_pinned_pages() function opens a window, a range
 * of whole pages of the registered address space of the endpoint epd,
 * starting at offset po. The value of po, further described below, is
 * a function of the parameters offset and pinned_pages, and the value
 * of map_flags. Each page of the window represents a corresponding
 * physical memory page of pinned_pages; the length of the window is
 * the same as the length of pinned_pages. A successful scif_register()
 * call returns po as the return value.
 *
 * Return Values
 *	Upon successful completion, scif_register_pinned_pages() returns
 *	the offset at which the mapping was placed (po);
 *	else an apt error is returned as documented in scif.h
 */
off_t
__scif_register_pinned_pages(scif_epd_t epd,
	scif_pinned_pages_t pinned_pages, off_t offset, int map_flags)
{
	struct endpt *ep = (struct endpt *)epd;
	uint64_t computed_offset;
	struct reg_range_t *window;
	int err;
	size_t len;

#ifdef DEBUG
	/* Bad EP */
	if (!ep || !pinned_pages || pinned_pages->magic != SCIFEP_MAGIC)
		return -EINVAL;
#endif
	/* Unsupported flags */
	if (map_flags & ~SCIF_MAP_FIXED)
		return -EINVAL;

	len = pinned_pages->nr_pages << PAGE_SHIFT;

	/*
	 * Offset is not page aligned/negative or offset+len
	 * wraps around with SCIF_MAP_FIXED.
	 */
	if ((map_flags & SCIF_MAP_FIXED) &&
		((align_low(offset, PAGE_SIZE) != offset) ||
		(offset < 0) ||
		(offset + (off_t)len < offset)))
		return -EINVAL;

	might_sleep();

	if ((err = verify_epd(ep)))
		return err;

	/* Compute the offset for this registration */
	if ((err = micscif_get_window_offset(ep, map_flags, offset,
			len, &computed_offset)))
		return err;

	/* Allocate and prepare self registration window */
	if (!(window = micscif_create_window(ep, pinned_pages->nr_pages,
			computed_offset, false))) {
		micscif_free_window_offset(ep, computed_offset, len);
		return -ENOMEM;
	}

	window->pinned_pages = pinned_pages;
	window->nr_pages = pinned_pages->nr_pages;
	window->nr_contig_chunks = pinned_pages->nr_contig_chunks;
	window->prot = pinned_pages->prot;

	/*
	 * This set of pinned pages now belongs to this window as well.
	 * Assert if the ref count is zero since it is an error to
	 * pass pinned_pages to scif_register_pinned_pages() after
	 * calling scif_unpin_pages().
	 */
	if (!atomic_add_unless(&pinned_pages->ref_count, 
				(int32_t)pinned_pages->nr_pages, 0))
		BUG_ON(1);

	micscif_inc_node_refcnt(ep->remote_dev, 1);

	if ((err = micscif_send_alloc_request(ep, window))) {
		micscif_dec_node_refcnt(ep->remote_dev, 1);
		printk(KERN_ERR "%s %d err %d\n", __func__, __LINE__, err);
		goto error_unmap;
	}

	/* Prepare the remote registration window */
	if ((err = micscif_prep_remote_window(ep, window))) {
		micscif_dec_node_refcnt(ep->remote_dev, 1);
		micscif_set_nr_pages(ep->remote_dev, window);
		printk(KERN_ERR "%s %d err %d\n", __func__, __LINE__, err);
		goto error_unmap;
	}

	/* Tell the peer about the new window */
	if ((err = micscif_send_scif_register(ep, window))) {
		micscif_dec_node_refcnt(ep->remote_dev, 1);
		printk(KERN_ERR "%s %d err %d\n", __func__, __LINE__, err);
		goto error_unmap;
	}

	micscif_dec_node_refcnt(ep->remote_dev, 1);

	/* No further failures expected. Insert new window */
	mutex_lock(&ep->rma_info.rma_lock);
	set_window_ref_count(window, pinned_pages->nr_pages);
	micscif_insert_window(window, &ep->rma_info.reg_list);
	mutex_unlock(&ep->rma_info.rma_lock);

	return computed_offset;
error_unmap:
	micscif_destroy_window(ep, window);
	printk(KERN_ERR "%s %d err %d\n", __func__, __LINE__, err);
	return err;
}

off_t
scif_register_pinned_pages(scif_epd_t epd,
	scif_pinned_pages_t pinned_pages, off_t offset, int map_flags)
{
	off_t ret;
	get_kref_count(epd);
	ret = __scif_register_pinned_pages(epd, pinned_pages, offset, map_flags);
	put_kref_count(epd);
	return ret;
}
EXPORT_SYMBOL(scif_register_pinned_pages);

/**
 * scif_get_pages - Add references to remote registered pages
 *
 * scif_get_pages() returns the addresses of the physical pages represented
 * by those pages of the registered address space of the peer of epd, starting
 * at offset offset and continuing for len bytes. offset and len are constrained
 * to be multiples of the page size.
 *
 * Return Values
 *	Upon successful completion, scif_get_pages() returns 0;
 *	else an apt error is returned as documented in scif.h.
 */
int
__scif_get_pages(scif_epd_t epd, off_t offset, size_t len, struct scif_range **pages)
{
	struct endpt *ep = (struct endpt *)epd;
	struct micscif_rma_req req;
	struct reg_range_t *window = NULL;
	int nr_pages, err, i;

	pr_debug("SCIFAPI get_pinned_pages: ep %p %s offset 0x%lx len 0x%lx\n", 
		ep, scif_ep_states[ep->state], offset, len);

	if ((err = verify_epd(ep)))
		return err;

	if ((!len) ||
		(offset < 0) ||
		(offset + len < offset) ||
		(align_low((uint64_t)offset, PAGE_SIZE) != (uint64_t)offset) ||
		(align_low((uint64_t)len, PAGE_SIZE) != (uint64_t)len))
		return -EINVAL;

	nr_pages = len >> PAGE_SHIFT;

	req.out_window = &window;
	req.offset = offset;
	req.prot = 0;
	req.nr_bytes = len;
	req.type = WINDOW_SINGLE;
	req.head = &ep->rma_info.remote_reg_list;

	mutex_lock(&ep->rma_info.rma_lock);
	/* Does a valid window exist? */
	if ((err = micscif_query_window(&req))) {
		printk(KERN_ERR "%s %d err %d\n", __func__, __LINE__, err);
		goto error;
	}
	RMA_MAGIC(window);

	/* Allocate scif_range */
	if (!(*pages = kzalloc(sizeof(struct scif_range), GFP_KERNEL))) {
		err = -ENOMEM;
		goto error;
	}

	/* Allocate phys addr array */
	if (!((*pages)->phys_addr = scif_zalloc(nr_pages * sizeof(dma_addr_t)))) {
		err = -ENOMEM;
		goto error;
	}

#ifndef _MIC_SCIF_
	/* Allocate virtual address array */
	if (!((*pages)->va = scif_zalloc(nr_pages * sizeof(void *)))) {
		err = -ENOMEM;
		goto error;
	}
#endif
	/* Populate the values */
	(*pages)->cookie = window;
	(*pages)->nr_pages = nr_pages;
	(*pages)->prot_flags = window->prot;

	for (i = 0; i < nr_pages; i++) {
		(*pages)->phys_addr[i] =
#if !defined(_MIC_SCIF_) && defined(CONFIG_ML1OM)
			is_self_scifdev(ep->remote_dev) ?
				micscif_get_dma_addr(window, offset + (i * PAGE_SIZE),
				NULL, NULL, NULL) : window->phys_addr[i];
#else
			get_phys_addr(micscif_get_dma_addr(window, offset + (i * PAGE_SIZE),
				NULL, NULL, NULL), ep->remote_dev);
#endif
#ifndef _MIC_SCIF_
		if (!is_self_scifdev(ep->remote_dev))
			(*pages)->va[i] =
				get_per_dev_ctx(ep->remote_dev->sd_node - 1)->aper.va +
				(*pages)->phys_addr[i] -
				get_per_dev_ctx(ep->remote_dev->sd_node - 1)->aper.pa;
#endif
	}

	window->get_put_ref_count += nr_pages;
	get_window_ref_count(window, nr_pages);
error:
	mutex_unlock(&ep->rma_info.rma_lock);
	if (err) {
		if (*pages) {
			if ((*pages)->phys_addr)
				scif_free((*pages)->phys_addr, nr_pages * sizeof(dma_addr_t));
#ifndef _MIC_SCIF_
			if ((*pages)->va)
				scif_free((*pages)->va, nr_pages * sizeof(void *));
#endif
			kfree(*pages);
			*pages = NULL;
		}
		printk(KERN_ERR "%s %d err %d\n", __func__, __LINE__, err);
	} else {
		micscif_create_node_dep(ep->remote_dev, nr_pages);
	}
	return err;
}

int
scif_get_pages(scif_epd_t epd, off_t offset, size_t len, struct scif_range **pages)
{
	int ret;
	get_kref_count(epd);
	ret = __scif_get_pages(epd, offset, len, pages);
	put_kref_count(epd);
	return ret;
}
EXPORT_SYMBOL(scif_get_pages);

/**
 * scif_put_pages - Remove references from remote registered pages
 *
 * scif_put_pages() returns a scif_range structure previously obtained by
 * calling scif_get_pages(). When control returns, the physical pages may
 * become available for reuse if and when the window which represented
 * those pages is unregistered. Therefore, those pages must never be accessed.
 *
 * Return Values
 *	Upon success, zero is returned.
 *	else an apt error is returned as documented in scif.h.
 */
int
__scif_put_pages(struct scif_range *pages)
{
	struct endpt *ep;
	struct reg_range_t *window;
	struct nodemsg msg;

	if (!pages || !pages->cookie)
		return -EINVAL;

	window = pages->cookie;

	if (!window || window->magic != SCIFEP_MAGIC ||
		!window->get_put_ref_count)
		return -EINVAL;

	ep = (struct endpt *)window->ep;

        /*
	 * If the state is SCIFEP_CONNECTED or SCIFEP_DISCONNECTED then the
	 * callee should be allowed to release references to the pages,
	 * else the endpoint was not connected in the first place,
	 * hence the ENOTCONN.
	 */
	if (ep->state != SCIFEP_CONNECTED && ep->state != SCIFEP_DISCONNECTED)
		return -ENOTCONN;

	/*
	 * TODO: Re-enable this check once ref counts for kernel mode APIs
	 * have been implemented and node remove call backs are called before
	 * the node is removed. This check results in kernel mode APIs not
	 * being able to release pages correctly since node remove callbacks
	 * are called after the node is removed currently.
	 *	if (!scifdev_alive(ep))
	 *		return -ENODEV;
	 */

	micscif_inc_node_refcnt(ep->remote_dev, 1);
	mutex_lock(&ep->rma_info.rma_lock);

	/* Decrement the ref counts and check for errors */
	window->get_put_ref_count -= pages->nr_pages;
	BUG_ON(window->get_put_ref_count < 0);
	put_window_ref_count(window, pages->nr_pages);

	/* Initiate window destruction if ref count is zero */
	if (!window->ref_count) {
		drain_dma_intr(ep->rma_info.dma_chan);
		/* Inform the peer about this window being destroyed. */
		msg.uop = SCIF_MUNMAP;
		msg.src = ep->port;
		msg.payload[0] = window->peer_window;
		/* No error handling for notification messages */
		micscif_nodeqp_send(ep->remote_dev, &msg, ep);
		list_del(&window->list_member);
		/* Destroy this window from the peer's registered AS */
		micscif_destroy_remote_window(ep, window);
	}
	mutex_unlock(&ep->rma_info.rma_lock);

	micscif_dec_node_refcnt(ep->remote_dev, 1);
	micscif_destroy_node_dep(ep->remote_dev, pages->nr_pages);
	scif_free(pages->phys_addr, pages->nr_pages * sizeof(dma_addr_t));
#ifndef _MIC_SCIF_
	scif_free(pages->va, pages->nr_pages * sizeof(void*));
#endif
	kfree(pages);
	return 0;
}

int
scif_put_pages(struct scif_range *pages)
{
	int ret;
	struct reg_range_t *window = pages->cookie;
	struct endpt *ep = (struct endpt *)window->ep;
	if (atomic_read(&(&(ep->ref_count))->refcount) > 0) {
		kref_get(&(ep->ref_count));
	} else {
		WARN_ON(1);
	}
	ret = __scif_put_pages(pages);
	if (atomic_read(&(&(ep->ref_count))->refcount) > 0) {
		kref_put(&(ep->ref_count), scif_ref_rel);
	} else {
		//WARN_ON(1);
	}
	return ret;
}
EXPORT_SYMBOL(scif_put_pages);

int scif_event_register(scif_callback_t handler)
{
	/* Add to the list of event handlers */
	struct scif_callback *cb = kmalloc(sizeof(*cb), GFP_KERNEL);
	if (!cb)
		return -ENOMEM;
	mutex_lock(&ms_info.mi_event_cblock);
	cb->callback_handler = handler;
	list_add_tail(&cb->list_member, &ms_info.mi_event_cb);
	mutex_unlock(&ms_info.mi_event_cblock);
	return 0;
}
EXPORT_SYMBOL(scif_event_register);

int scif_event_unregister(scif_callback_t handler)
{
	struct list_head *pos, *unused;
	struct scif_callback *temp;
	int err = -EINVAL;

	mutex_lock(&ms_info.mi_event_cblock);
	list_for_each_safe(pos, unused, &ms_info.mi_event_cb) {
		temp = list_entry(pos, struct scif_callback, list_member);
		if (temp->callback_handler == handler) {
			err = 0;
			list_del(pos);
			kfree(temp);
			break;
		}
	}

	mutex_unlock(&ms_info.mi_event_cblock);
	return err;
}
EXPORT_SYMBOL(scif_event_unregister);

/**
 * scif_register - Mark a memory region for remote access.
 *	@epd:		endpoint descriptor
 *	@addr:		starting virtual address
 *	@len:		length of range
 *	@offset:	offset of window
 *	@prot:		read/write protection
 *	@map_flags:	flags
 *
 * Return Values
 *	Upon successful completion, scif_register() returns the offset
 *	at which the mapping was placed else an apt error is returned
 *	as documented in scif.h.
 */
off_t
__scif_register(scif_epd_t epd, void *addr, size_t len, off_t offset,
					int prot, int map_flags)
{
	scif_pinned_pages_t pinned_pages;
	off_t err;
	struct endpt *ep = (struct endpt *)epd;
	uint64_t computed_offset;
	struct reg_range_t *window;
	struct mm_struct *mm = NULL;

	pr_debug("SCIFAPI register: ep %p %s addr %p len 0x%lx"
		" offset 0x%lx prot 0x%x map_flags 0x%x\n", 
		epd, scif_ep_states[epd->state], addr, len, offset, prot, map_flags);

	/* Unsupported flags */
	if (map_flags & ~(SCIF_MAP_FIXED | SCIF_MAP_KERNEL))
		return -EINVAL;

	/* Unsupported protection requested */
	if (prot & ~(SCIF_PROT_READ | SCIF_PROT_WRITE))
		return -EINVAL;

	/* addr/len must be page aligned. len should be non zero */
	if ((!len) ||
		(align_low((uint64_t)addr, PAGE_SIZE) != (uint64_t)addr) ||
		(align_low((uint64_t)len, PAGE_SIZE) != (uint64_t)len))
		return -EINVAL;

	/*
	 * Offset is not page aligned/negative or offset+len
	 * wraps around with SCIF_MAP_FIXED.
	 */
	if ((map_flags & SCIF_MAP_FIXED) &&
		((align_low(offset, PAGE_SIZE) != offset) ||
		(offset < 0) ||
		(offset + (off_t)len < offset)))
		return -EINVAL;


	might_sleep();

#ifdef DEBUG
	/* Bad EP */
	if (!ep)
		return -EINVAL;
#endif

	if ((err = verify_epd(ep)))
		return err;

	/* Compute the offset for this registration */
	if ((err = micscif_get_window_offset(ep, map_flags, offset,
			len, &computed_offset)))
		return err;

	/* Allocate and prepare self registration window */
	if (!(window = micscif_create_window(ep, len >> PAGE_SHIFT,
			computed_offset, false))) {
		micscif_free_window_offset(ep, computed_offset, len);
		return -ENOMEM;
	}

	micscif_inc_node_refcnt(ep->remote_dev, 1);

	window->nr_pages = len >> PAGE_SHIFT;

	if ((err = micscif_send_alloc_request(ep, window))) {
		micscif_destroy_incomplete_window(ep, window);
		micscif_dec_node_refcnt(ep->remote_dev, 1);
		return err;
	}

	if (!(map_flags & SCIF_MAP_KERNEL)) {
		mm = __scif_acquire_mm();
		map_flags |= SCIF_MAP_ULIMIT;
	}
	/* Pin down the pages */
	if ((err = scif_pin_pages(addr, len, prot,
			map_flags & (SCIF_MAP_KERNEL | SCIF_MAP_ULIMIT),
			&pinned_pages))) {
		micscif_destroy_incomplete_window(ep, window);
		micscif_dec_node_refcnt(ep->remote_dev, 1);
		__scif_release_mm(mm);
		goto error;
	}

	window->pinned_pages = pinned_pages;
	window->nr_contig_chunks = pinned_pages->nr_contig_chunks;
	window->prot = pinned_pages->prot;
	window->mm = mm;

	/* Prepare the remote registration window */
	if ((err = micscif_prep_remote_window(ep, window))) {
		micscif_dec_node_refcnt(ep->remote_dev, 1);
		micscif_set_nr_pages(ep->remote_dev, window);
		printk(KERN_ERR "%s %d err %ld\n", __func__, __LINE__, err);
		goto error_unmap;
	}

	/* Tell the peer about the new window */
	if ((err = micscif_send_scif_register(ep, window))) {
		micscif_dec_node_refcnt(ep->remote_dev, 1);
		printk(KERN_ERR "%s %d err %ld\n", __func__, __LINE__, err);
		goto error_unmap;
	}

	micscif_dec_node_refcnt(ep->remote_dev, 1);

	/* No further failures expected. Insert new window */
	mutex_lock(&ep->rma_info.rma_lock);
	set_window_ref_count(window, pinned_pages->nr_pages);
	micscif_insert_window(window, &ep->rma_info.reg_list);
	mutex_unlock(&ep->rma_info.rma_lock);

	pr_debug("SCIFAPI register: ep %p %s addr %p"
		" len 0x%lx computed_offset 0x%llx\n", 
		epd, scif_ep_states[epd->state], addr, len, computed_offset);
	return computed_offset;
error_unmap:
	micscif_destroy_window(ep, window);
error:
	printk(KERN_ERR "%s %d err %ld\n", __func__, __LINE__, err);
	return err;
}

off_t
scif_register(scif_epd_t epd, void *addr, size_t len, off_t offset,
					int prot, int map_flags)
{
	off_t ret;
	get_kref_count(epd);
	ret = __scif_register(epd, addr, len, offset, prot, map_flags);
	put_kref_count(epd);
	return ret;
}
EXPORT_SYMBOL(scif_register);

/**
 * scif_unregister - Release a memory region registered for remote access.
 *	@epd:		endpoint descriptor
 *	@offset:	start of range to unregister
 *	@len:		length of range to unregister
 *
 * Return Values
 *	Upon successful completion, scif_unegister() returns zero
 *	else an apt error is returned as documented in scif.h.
 */
int
__scif_unregister(scif_epd_t epd, off_t offset, size_t len)
{
	struct endpt *ep = (struct endpt *)epd;
	struct reg_range_t *window = NULL;
	struct micscif_rma_req req;
	int nr_pages, err;

	pr_debug("SCIFAPI unregister: ep %p %s offset 0x%lx len 0x%lx\n", 
		ep, scif_ep_states[ep->state], offset, len);

	/* len must be page aligned. len should be non zero */
	if ((!len) ||
		(align_low((uint64_t)len, PAGE_SIZE) != (uint64_t)len))
		return -EINVAL;

	/* Offset is not page aligned or offset+len wraps around */
	if ((align_low(offset, PAGE_SIZE) != offset) ||
		(offset + (off_t)len < offset))
		return -EINVAL;

	if ((err = verify_epd(ep)))
		return err;

	might_sleep();
	nr_pages = (int)(len >> PAGE_SHIFT);

	req.out_window = &window;
	req.offset = offset;
	req.prot = 0;
	req.nr_bytes = len;
	req.type = WINDOW_FULL;
	req.head = &ep->rma_info.reg_list;

	micscif_inc_node_refcnt(ep->remote_dev, 1);
	mutex_lock(&ep->rma_info.rma_lock);
	/* Does a valid window exist? */
	if ((err = micscif_query_window(&req))) {
		printk(KERN_ERR "%s %d err %d\n", __func__, __LINE__, err);
		goto error;
	}
	/* Unregister all the windows in this range */
	if ((err = micscif_rma_list_unregister(window, offset, nr_pages)))
		printk(KERN_ERR "%s %d err %d\n", __func__, __LINE__, err);
error:
	mutex_unlock(&ep->rma_info.rma_lock);
	micscif_dec_node_refcnt(ep->remote_dev, 1);
	return err;
}

int
scif_unregister(scif_epd_t epd, off_t offset, size_t len)
{
	int ret;
	get_kref_count(epd);
	ret = __scif_unregister(epd, offset, len);
	put_kref_count(epd);
	return ret;
}
EXPORT_SYMBOL(scif_unregister);

unsigned int scif_pollfd(struct file *f, poll_table *wait, scif_epd_t epd)
{
	unsigned int ret;
	get_kref_count(epd);
	ret = __scif_pollfd(f, wait, (struct endpt *)epd);
	put_kref_count(epd);
	return ret;
}

unsigned int __scif_pollfd(struct file *f, poll_table *wait, struct endpt *ep)
{
	unsigned int mask = 0;
	unsigned long sflags;

	pr_debug("SCIFAPI pollfd: ep %p %s\n", ep, scif_ep_states[ep->state]);

	micscif_inc_node_refcnt(ep->remote_dev, 1);
	spin_lock_irqsave(&ep->lock, sflags);

	if (ep->conn_async_state == ASYNC_CONN_INPROGRESS) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
		if (!wait || poll_requested_events(wait) & SCIF_POLLOUT) {
#else
		if (!wait || wait->key & SCIF_POLLOUT) {
#endif
		poll_wait(f, &ep->conn_pend_wq, wait);
		if (ep->state == SCIFEP_CONNECTED ||
			ep->state == SCIFEP_DISCONNECTED ||
			ep->conn_err) {
			mask |= SCIF_POLLOUT;
		}
		goto return_scif_poll;
		}
	}

	/* Is it OK to use wait->key?? */
	if (ep->state == SCIFEP_LISTENING) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
		if (!wait || poll_requested_events(wait) & SCIF_POLLIN) {
#else
		if (!wait || wait->key & SCIF_POLLIN) {
#endif
			spin_unlock_irqrestore(&ep->lock, sflags);
			poll_wait(f, &ep->conwq, wait);
			spin_lock_irqsave(&ep->lock, sflags);
			if (ep->conreqcnt)
				mask |= SCIF_POLLIN;
		} else {
			mask |= SCIF_POLLERR;
		}
		goto return_scif_poll;
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
	if (!wait || poll_requested_events(wait) & SCIF_POLLIN) {
#else
	if (!wait || wait->key & SCIF_POLLIN) {
#endif
		if (ep->state != SCIFEP_CONNECTED &&
		    ep->state != SCIFEP_LISTENING &&
		    ep->state != SCIFEP_DISCONNECTED) {
			mask |= SCIF_POLLERR;
			goto return_scif_poll;
		}

		spin_unlock_irqrestore(&ep->lock, sflags);
		poll_wait(f, &ep->recvwq, wait);
		spin_lock_irqsave(&ep->lock, sflags);
		if (micscif_rb_count(&ep->qp_info.qp->inbound_q, 1))
			mask |= SCIF_POLLIN;
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
	if (!wait || poll_requested_events(wait) & SCIF_POLLOUT) {
#else
	if (!wait || wait->key & SCIF_POLLOUT) {
#endif
		if (ep->state != SCIFEP_CONNECTED &&
		    ep->state != SCIFEP_LISTENING) {
			mask |= SCIF_POLLERR;
			goto return_scif_poll;
		}

		spin_unlock_irqrestore(&ep->lock, sflags);
		poll_wait(f, &ep->sendwq, wait);
		spin_lock_irqsave(&ep->lock, sflags);
		if (micscif_rb_space(&ep->qp_info.qp->outbound_q))
			mask |= SCIF_POLLOUT;
	}

return_scif_poll:
	/* If the endpoint is in the diconnected state then return hangup instead of error */
	if (ep->state == SCIFEP_DISCONNECTED) {
		mask &= ~SCIF_POLLERR;
		mask |= SCIF_POLLHUP;
	}

	spin_unlock_irqrestore(&ep->lock, sflags);
	micscif_dec_node_refcnt(ep->remote_dev, 1);
	return mask;
}

/*
 * The private data field of each VMA used to mmap a remote window
 * points to an instance of struct vma_pvt
 */
struct vma_pvt {
	struct endpt *ep;	/* End point for remote window */
	uint64_t offset;	/* offset within remote window */
	bool valid_offset;	/* offset is valid only if the original
				 * mmap request was for a single page
				 * else the offset within the vma is
				 * the correct offset
				 */
	struct kref ref;
};

static void vma_pvt_release(struct kref *ref)
{
	struct vma_pvt *vmapvt = container_of(ref, struct vma_pvt, ref);
	kfree(vmapvt);
}

/**
 * scif_vma_open - VMA open driver callback
 *	@vma: VMM memory area.
 * The open method is called by the kernel to allow the subsystem implementing
 * the VMA to initialize the area. This method is invoked any time a new
 * reference to the VMA is made (when a process forks, for example).
 * The one exception happens when the VMA is first created by mmap;
 * in this case, the driver's mmap method is called instead.
 * This function is also invoked when an existing VMA is split by the kernel
 * due to a call to munmap on a subset of the VMA resulting in two VMAs.
 * The kernel invokes this function only on one of the two VMAs.
 *
 * Return Values: None.
 */
static void scif_vma_open(struct vm_area_struct *vma)
{
	struct vma_pvt *vmapvt = ((vma)->vm_private_data);
	pr_debug("SCIFAPI vma open: vma_start 0x%lx vma_end 0x%lx\n", 
			((vma)->vm_start), ((vma)->vm_end));
	kref_get(&vmapvt->ref);
}

/**
 * scif_munmap - VMA close driver callback.
 *	@vma: VMM memory area.
 * When an area is destroyed, the kernel calls its close operation.
 * Note that there's no usage count associated with VMA's; the area
 * is opened and closed exactly once by each process that uses it.
 *
 * Return Values: None.
 */
void scif_munmap(struct vm_area_struct *vma)
{
	struct endpt *ep;
	struct vma_pvt *vmapvt = ((vma)->vm_private_data);
	int nr_pages = (int)( (((vma)->vm_end) - ((vma)->vm_start)) >> PAGE_SHIFT );
	uint64_t offset;
	struct micscif_rma_req req;
	struct reg_range_t *window = NULL;
	int err;

	might_sleep();
	pr_debug("SCIFAPI munmap: vma_start 0x%lx vma_end 0x%lx\n", 
			((vma)->vm_start), ((vma)->vm_end));
	/* used to be a BUG_ON(), prefer keeping the kernel alive */
	if (!vmapvt) {
		WARN_ON(1);
		printk(KERN_ERR "SCIFAPI munmap: vma_start 0x%lx vma_end 0x%lx\n", 
			((vma)->vm_start), ((vma)->vm_end));
		return;
	}

	ep = vmapvt->ep;
	offset = vmapvt->valid_offset ? vmapvt->offset :
		((vma)->vm_pgoff) << PAGE_SHIFT;
	pr_debug("SCIFAPI munmap: ep %p %s  nr_pages 0x%x offset 0x%llx\n", 
		ep, scif_ep_states[ep->state], nr_pages, offset);

	req.out_window = &window;
	req.offset = offset;
	req.nr_bytes = ((vma)->vm_end) - ((vma)->vm_start);
	req.prot = ((vma)->vm_flags) & (VM_READ | VM_WRITE);
	req.type = WINDOW_PARTIAL;
	req.head = &ep->rma_info.remote_reg_list;

	micscif_inc_node_refcnt(ep->remote_dev, 1);
	mutex_lock(&ep->rma_info.rma_lock);

	if ((err = micscif_query_window(&req)))
		printk(KERN_ERR "%s %d err %d\n", __func__, __LINE__, err);
	else
		micscif_rma_list_munmap(window, offset, nr_pages);

	mutex_unlock(&ep->rma_info.rma_lock);
	micscif_dec_node_refcnt(ep->remote_dev, 1);

	micscif_destroy_node_dep(ep->remote_dev, nr_pages);

	/*
	 * The kernel probably zeroes these out but we still want
	 * to clean up our own mess just in case.
	 */
	vma->vm_ops = NULL;
	((vma)->vm_private_data) = NULL;
	kref_put(&vmapvt->ref, vma_pvt_release);
	micscif_rma_put_task(ep, nr_pages);
}

static const struct vm_operations_struct micscif_vm_ops = {
	.open = scif_vma_open,
	.close = scif_munmap,
};

/**
 * scif_mmap - Map pages in virtual address space to a remote window.
 *	@vma: VMM memory area.
 *	@epd:		endpoint descriptor
 *
 * Return Values
 *	Upon successful completion, scif_mmap() returns zero
 *	else an apt error is returned as documented in scif.h.
 */
int
scif_mmap(struct vm_area_struct *vma, scif_epd_t epd)
{
	struct micscif_rma_req req;
	struct reg_range_t *window = NULL;
	struct endpt *ep = (struct endpt *)epd;
	uint64_t start_offset = ((vma)->vm_pgoff) << PAGE_SHIFT;
	int nr_pages = (int)( (((vma)->vm_end) - ((vma)->vm_start)) >> PAGE_SHIFT);
	int err;
	struct vma_pvt *vmapvt;

	pr_debug("SCIFAPI mmap: ep %p %s start_offset 0x%llx nr_pages 0x%x\n", 
		ep, scif_ep_states[ep->state], start_offset, nr_pages);

	if ((err = verify_epd(ep)))
		return err;

	might_sleep();

	if ((err = micscif_rma_get_task(ep, nr_pages)))
		return err;

	if (!(vmapvt = kzalloc(sizeof(*vmapvt), GFP_KERNEL))) {
		micscif_rma_put_task(ep, nr_pages);
		return -ENOMEM;
	}

	vmapvt->ep = ep;
	kref_init(&vmapvt->ref);

	micscif_create_node_dep(ep->remote_dev, nr_pages);

	req.out_window = &window;
	req.offset = start_offset;
	req.nr_bytes = ((vma)->vm_end) - ((vma)->vm_start);
	req.prot = ((vma)->vm_flags) & (VM_READ | VM_WRITE);
	req.type = WINDOW_PARTIAL;
	req.head = &ep->rma_info.remote_reg_list;

	micscif_inc_node_refcnt(ep->remote_dev, 1);
	mutex_lock(&ep->rma_info.rma_lock);
	/* Does a valid window exist? */
	if ((err = micscif_query_window(&req))) {
		printk(KERN_ERR "%s %d err %d\n", __func__, __LINE__, err);
		goto error;
	}
	RMA_MAGIC(window);

	/* Default prot for loopback */
	if (!is_self_scifdev(ep->remote_dev)) {
#ifdef _MIC_SCIF_
		vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
#else
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
#endif
	}

	/*
	 * VM_DONTCOPY - Do not copy this vma on fork
	 * VM_DONTEXPAND - Cannot expand with mremap()
	 * VM_RESERVED - Count as reserved_vm like IO
	 * VM_PFNMAP - Page-ranges managed without "struct page"
	 * VM_IO - Memory mapped I/O or similar
	 *
	 * We do not want to copy this VMA automatically on a fork(),
	 * expand this VMA due to mremap() or swap out these pages since
	 * the VMA is actually backed by physical pages in the remote
	 * node's physical memory and not via a struct page.
	 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
	vma->vm_flags |= VM_DONTCOPY | VM_DONTEXPAND | VM_DONTDUMP | VM_PFNMAP;
#else
	vma->vm_flags |= VM_DONTCOPY | VM_DONTEXPAND | VM_RESERVED | VM_PFNMAP;
#endif

	if (!is_self_scifdev(ep->remote_dev))
		((vma)->vm_flags) |= VM_IO;

	/* Map this range of windows */
	if ((err = micscif_rma_list_mmap(window,
			start_offset, nr_pages, vma))) {
		printk(KERN_ERR "%s %d err %d\n", __func__, __LINE__, err);
		goto error;
	}
	/* Set up the driver call back */
	vma->vm_ops = &micscif_vm_ops;
	((vma)->vm_private_data) = vmapvt;
	/*
	 * For 1 page sized VMAs the kernel (remap_pfn_range) replaces the
	 * offset in the VMA with the pfn, so in that case save off the
	 * original offset, since the page sized VMA can't be split into
	 * smaller VMAs the offset is not going to change.
	 */
	if (nr_pages == 1) {
		vmapvt->offset = start_offset;
		vmapvt->valid_offset = true;
	}
	err = 0;
error:
	mutex_unlock(&ep->rma_info.rma_lock);
	micscif_dec_node_refcnt(ep->remote_dev, 1);
	if (err) {
		micscif_destroy_node_dep(ep->remote_dev, nr_pages);
		kfree(vmapvt);
		printk(KERN_ERR "%s %d err %d\n", __func__, __LINE__, err);
		micscif_rma_put_task(ep, nr_pages);
	}
	return err;
}

/**
 * scif_readfrom() - Read SCIF offset data from remote connection
 * @epd:	endpoint descriptor
 * @loffset:	offset in local registered address space to which to copy
 * @len:	length of range to copy
 * @roffset:	offset in remote registered address space from which to copy
 * @flags:	flags
 *
 * Return Values
 *	Upon successful completion, scif_readfrom() returns zero
 *	else an apt error is returned as documented in scif.h.
 */
int
scif_readfrom(scif_epd_t epd, off_t loffset, size_t len,
				off_t roffset, int flags)
{
	int ret;
	get_kref_count(epd);
	ret = __scif_readfrom(epd, loffset, len, roffset, flags);
	put_kref_count(epd);
	return ret;
}
EXPORT_SYMBOL(scif_readfrom);

/**
 * scif_writeto() - Send SCIF offset data to remote connection
 * @epd:	endpoint descriptor
 * @loffset:	offset in local registered address space from which to copy
 * @len:	length of range to copy
 * @roffset:	offset in remote registered address space to which to copy
 * @flags:	flags
 *
 * Return Values
 *	Upon successful completion, scif_writeto() returns zero
 *	else an apt error is returned as documented in scif.h.
 *
 */
int scif_writeto(scif_epd_t epd, off_t loffset, size_t len,
				off_t roffset, int flags)
{
	int ret;
	get_kref_count(epd);
	ret = __scif_writeto(epd, loffset, len, roffset, flags);
	put_kref_count(epd);
	return ret;
}
EXPORT_SYMBOL(scif_writeto);

#define HOST_LOOPB_MAGIC_MARK 0xdead

/**
 * scif_fence_mark:
 * @epd:	endpoint descriptor
 * @flags:	control flags
 * @mark:	marked handle returned as output.
 *
 * scif_fence_mark() returns after marking the current set of all uncompleted
 * RMAs initiated through the endpoint epd or marking the current set of all
 * uncompleted RMAs initiated through the peer of endpoint epd. The RMAs are
 * marked with a value returned in mark. The application may subsequently
 * await completion of all RMAs so marked.
 *
 * Return Values
 *	Upon successful completion, scif_fence_mark() returns 0;
 *	else an apt error is returned as documented in scif.h.
 */
int __scif_fence_mark(scif_epd_t epd, int flags, int *mark)
{
	struct endpt *ep = (struct endpt *)epd;
	int err = 0;

	pr_debug("SCIFAPI fence_mark: ep %p %s flags 0x%x mark 0x%x\n", 
		ep, scif_ep_states[ep->state], flags, *mark);

	if ((err = verify_epd(ep)))
		return err;

	/* Invalid flags? */
	if (flags & ~(SCIF_FENCE_INIT_SELF | SCIF_FENCE_INIT_PEER))
		return -EINVAL;

	/* At least one of init self or peer RMA should be set */
	if (!(flags & (SCIF_FENCE_INIT_SELF | SCIF_FENCE_INIT_PEER)))
		return -EINVAL;

	/* Exactly one of init self or peer RMA should be set but not both */
	if ((flags & SCIF_FENCE_INIT_SELF) && (flags & SCIF_FENCE_INIT_PEER))
		return -EINVAL;

#ifndef _MIC_SCIF_
	/*
	 * Host Loopback does not need to use DMA.
	 * Return a valid mark to be symmetric.
	 */
	if (is_self_scifdev(ep->remote_dev)) {
		*mark = HOST_LOOPB_MAGIC_MARK;
		return 0;
	}
#endif

	if (flags & SCIF_FENCE_INIT_SELF) {
		if ((*mark = micscif_fence_mark(epd)) < 0)
			err = *mark;
	} else {
		micscif_inc_node_refcnt(ep->remote_dev, 1);
		err = micscif_send_fence_mark(ep, mark);
		micscif_dec_node_refcnt(ep->remote_dev, 1);
	}
	if (err)
		printk(KERN_ERR "%s %d err %d\n", __func__, __LINE__, err);

	pr_debug("SCIFAPI fence_mark: ep %p %s flags 0x%x mark 0x%x err %d\n", 
		ep, scif_ep_states[ep->state], flags, *mark, err);
	return err;
}

int scif_fence_mark(scif_epd_t epd, int flags, int *mark)
{
	int ret;
	get_kref_count(epd);
	ret = __scif_fence_mark(epd, flags, mark);
	put_kref_count(epd);
	return ret;
}
EXPORT_SYMBOL(scif_fence_mark);

/**
 * scif_fence_wait:
 * @epd:	endpoint descriptor
 * @mark:	mark request.
 *
 * scif_fence_wait() returns after all RMAs marked with mark have completed.
 *
 * Return Values
 *	Upon successful completion, scif_fence_wait() returns 0;
 *	else an apt error is returned as documented in scif.h.
 */
int __scif_fence_wait(scif_epd_t epd, int mark)
{
	struct endpt *ep = (struct endpt *)epd;
	int err = 0;

	pr_debug("SCIFAPI fence_wait: ep %p %s mark 0x%x\n", 
		ep, scif_ep_states[ep->state], mark);

	if ((err = verify_epd(ep)))
		return err;

#ifndef _MIC_SCIF_
	/*
	 * Host Loopback does not need to use DMA.
	 * The only valid mark provided is 0 so simply
	 * return success if the mark is valid.
	 */
	if (is_self_scifdev(ep->remote_dev)) {
		if (HOST_LOOPB_MAGIC_MARK == mark)
			return 0;
		else
			return -EINVAL;
	}
#endif
	if (mark & SCIF_REMOTE_FENCE) {
		micscif_inc_node_refcnt(ep->remote_dev, 1);
		err = micscif_send_fence_wait(epd, mark);
		micscif_dec_node_refcnt(ep->remote_dev, 1);
	} else {
		err = dma_mark_wait(epd->rma_info.dma_chan, mark, true);
		if (!err && atomic_read(&ep->rma_info.tw_refcount))
			queue_work(ms_info.mi_misc_wq, &ms_info.mi_misc_work);
	}

	if (err < 0)
		printk(KERN_ERR "%s %d err %d\n", __func__, __LINE__, err);
	return err;
}

int scif_fence_wait(scif_epd_t epd, int mark)
{
	int ret;
	get_kref_count(epd);
	ret = __scif_fence_wait(epd, mark);
	put_kref_count(epd);
	return ret;
}
EXPORT_SYMBOL(scif_fence_wait);

/*
 * scif_fence_signal:
 * @loff:	local offset
 * @lval:	local value to write to loffset
 * @roff:	remote offset
 * @rval:	remote value to write to roffset
 * @flags:	flags
 *
 * scif_fence_signal() returns after marking the current set of all
 * uncompleted RMAs initiated through the endpoint epd or marking
 * the current set of all uncompleted RMAs initiated through the peer
 * of endpoint epd.
 *
 * Return Values
 * 	Upon successful completion, scif_fence_signal() returns 0;
 *	else an apt error is returned as documented in scif.h.
 */
int __scif_fence_signal(scif_epd_t epd, off_t loff, uint64_t lval,
				off_t roff, uint64_t rval, int flags)
{
	struct endpt *ep = (struct endpt *)epd;
	int err = 0;

	pr_debug("SCIFAPI fence_signal: ep %p %s loff 0x%lx lval 0x%llx "
		"roff 0x%lx rval 0x%llx flags 0x%x\n", 
		ep, scif_ep_states[ep->state], loff, lval, roff, rval, flags);

	if ((err = verify_epd(ep)))
		return err;

	/* Invalid flags? */
	if (flags & ~(SCIF_FENCE_INIT_SELF | SCIF_FENCE_INIT_PEER |
			SCIF_SIGNAL_LOCAL | SCIF_SIGNAL_REMOTE))
		return -EINVAL;

	/* At least one of init self or peer RMA should be set */
	if (!(flags & (SCIF_FENCE_INIT_SELF | SCIF_FENCE_INIT_PEER)))
		return -EINVAL;

	/* Exactly one of init self or peer RMA should be set but not both */
	if ((flags & SCIF_FENCE_INIT_SELF) && (flags & SCIF_FENCE_INIT_PEER))
		return -EINVAL;

	/* At least one of SCIF_SIGNAL_LOCAL or SCIF_SIGNAL_REMOTE required */
	if (!(flags & (SCIF_SIGNAL_LOCAL | SCIF_SIGNAL_REMOTE)))
		return -EINVAL;

	/* Only Dword offsets allowed */
	if ((flags & SCIF_SIGNAL_LOCAL) && (loff & (sizeof(uint32_t) - 1)))
		return -EINVAL;

	/* Only Dword aligned offsets allowed */
	if ((flags & SCIF_SIGNAL_REMOTE) && (roff & (sizeof(uint32_t) - 1)))
		return -EINVAL;

	if (flags & SCIF_FENCE_INIT_PEER) {
		micscif_inc_node_refcnt(ep->remote_dev, 1);
		err = micscif_send_fence_signal(epd, roff,
			rval, loff, lval, flags);
		micscif_dec_node_refcnt(ep->remote_dev, 1);
	} else {
		/* Local Signal in Local RAS */
		if (flags & SCIF_SIGNAL_LOCAL)
			if ((err = micscif_prog_signal(epd, loff,
					lval, RMA_WINDOW_SELF)))
				goto error_ret;

		/* Signal in Remote RAS */
		if (flags & SCIF_SIGNAL_REMOTE) {
			micscif_inc_node_refcnt(ep->remote_dev, 1);
			err = micscif_prog_signal(epd, roff,
					rval, RMA_WINDOW_PEER);
			micscif_dec_node_refcnt(ep->remote_dev, 1);
		}
	}
error_ret:
	if (err)
		printk(KERN_ERR "%s %d err %d\n", __func__, __LINE__, err);
	else if (atomic_read(&ep->rma_info.tw_refcount))
		queue_work(ms_info.mi_misc_wq, &ms_info.mi_misc_work);
	return err;
}

int scif_fence_signal(scif_epd_t epd, off_t loff, uint64_t lval,
				off_t roff, uint64_t rval, int flags)
{
	int ret;
	get_kref_count(epd);
	ret = __scif_fence_signal(epd, loff, lval, roff, rval, flags);
	put_kref_count(epd);
	return ret;
}
EXPORT_SYMBOL(scif_fence_signal);

/**
 * scif_get_nodeIDs - Return information about online nodes
 * @nodes: array space reserved for returning online node IDs
 * @len: number of entries on the nodes array
 * @self: address to place the node ID of this system
 *
 * Return Values
 *	scif_get_nodeIDs() returns the total number of scif nodes
 *	(including host) in the system
 */
int
scif_get_nodeIDs(uint16_t *nodes, int len, uint16_t *self)
{
	int online = 0;
	int offset = 0;
	int node;
#ifdef _MIC_SCIF_
	micscif_get_node_info();
#endif

	*self = ms_info.mi_nodeid;
	mutex_lock(&ms_info.mi_conflock);
	len = SCIF_MIN(len, (int32_t)ms_info.mi_total);
	for (node = 0; node <=(int32_t)ms_info.mi_maxid; node++) {
		if (ms_info.mi_mask & (1UL << node)) {
			online++;
			if (offset < len)
				nodes[offset++] = node;
		}
	}
	pr_debug("SCIFAPI get_nodeIDs total %d online %d filled in %d nodes\n", 
		ms_info.mi_total, online, len);
	mutex_unlock(&ms_info.mi_conflock);

	return online;
}

EXPORT_SYMBOL(scif_get_nodeIDs);

/**
 * micscif_pci_dev:
 * @node: node ID
 *
 *  Return the pci_dev associated with a node.
 */
int micscif_pci_dev(uint16_t node, struct pci_dev **pdev)
{
#ifdef _MIC_SCIF_
       /* This *is* a PCI device, therefore no pdev to return. */
       return -ENODEV;
#else
	 mic_ctx_t *mic_ctx = get_per_dev_ctx(node - 1);
	*pdev = mic_ctx->bi_pdev;
	return 0;
#endif
}

#ifndef _MIC_SCIF_
/**
 * micscif_pci_info:
 * @node: node ID
 *
 * Populate the pci device info pointer associated with a node.
 */
int micscif_pci_info(uint16_t node, struct scif_pci_info *dev)
{
	int i;
	mic_ctx_t *mic_ctx = get_per_dev_ctx(node - 1);
	struct pci_dev *pdev;

	if (!mic_ctx)
		return -ENODEV;

	dev->pdev = pdev = mic_ctx->bi_pdev;
	for (i = 0; i < PCI_NUM_RESOURCES; i++) {
		if (!pci_resource_start(pdev, i)) {
			dev->va[i] = NULL;
			continue;
		}
		if (pci_resource_flags(pdev, i) & IORESOURCE_PREFETCH) {
			/* TODO: Change comparison check for KNL. */
			if (pci_resource_start(pdev, i) == mic_ctx->aper.pa)
				dev->va[i] = mic_ctx->aper.va;
			else
				dev->va[i] = NULL;
		} else {
			dev->va[i] = mic_ctx->mmio.va;
		}
	}
	return 0;
}
#endif

/**
 * scif_pci_info - Populate the pci device info pointer associated with a node
 * @node:              the node to query
 * @scif_pdev:         The scif_pci_info structure to populate.
 *
 * scif_pci_info() populates the provided scif_pci_info structure
 * associated with a node. The requested node ID cannot be the same as
 * the current node.  This routine may only return success when called from
 * the host.
 *
 * Return Values
 *     Upon successful completion, scif_pci_info() returns 0; otherwise the
 *	an appropriate error is returned as documented in scif.h.
 */
int scif_pci_info(uint16_t node, struct scif_pci_info *dev)
{
#ifdef _MIC_SCIF_
	return -EINVAL;
#else
	if (node > ms_info.mi_maxid)
		return -EINVAL;

	if ((scif_dev[node].sd_state == SCIFDEV_NOTPRESENT) ||
	    is_self_scifdev(&scif_dev[node]))
		return -ENODEV;

	return micscif_pci_info(node, dev);
#endif
}
EXPORT_SYMBOL(scif_pci_info);

/*
 * DEBUG helper functions
 */
void
print_ep_state(struct endpt *ep, char *label)
{
	if (ep)
		printk("%s: EP %p state %s\n", 
			label, ep, scif_ep_states[ep->state]);
	else
		printk("%s: EP %p\n state ?\n", label, ep);
}

