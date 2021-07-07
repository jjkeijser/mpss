
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

#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/list.h>
#include <linux/circ_buf.h>
#include <linux/reboot.h>
#include "mic_common.h"
#include "mic/micveth_dma.h"
#include "mic/mic_macaddr.h"

/* TODO: Clean up shutdown, let DMA's drain */

#ifndef HOST
#define SBOX_SDBIC0_DBREQ_BIT   0x80000000
#define SBOX_MMIO_LENGTH	(64 * 1024)
#endif
#define STOP_WAIT_TIMEOUT	(4 * HZ)

#ifndef HOST
static mic_ctx_t mic_ctx_g;
#endif

struct micvnet micvnet;


static void micvnet_send_intr(struct micvnet_info *vnet_info);
static int micvnet_init_msg_rings(struct micvnet_info *vnet_info);
static int micvnet_init_rx_skb_send_msg(struct micvnet_info *vnet_info);
static void micvnet_send_add_dma_buffer_messages(struct micvnet_info *vnet_info);
static void micvnet_stop_ws(struct work_struct *work);
static void micvnet_start_ws(struct work_struct *work);
int get_sbox_irq(int index);

static __always_inline mic_ctx_t *
vnet_to_ctx(struct micvnet_info *vnet_info)
{
	return vnet_info->mic_ctx;
}

static __always_inline void
micvnet_wake_queue(struct micvnet_info *vnet_info)
{
	if (atomic_read(&vnet_info->vi_state) == MICVNET_STATE_LINKUP)
		netif_wake_queue(vnet_info->vi_netdev);
}

static __always_inline void
micvnet_dec_cnt_tx_pending(struct micvnet_info *vnet_info)
{
	if (atomic_dec_and_test(&vnet_info->cnt_tx_pending) &&
	   (atomic_read(&vnet_info->vi_state) == MICVNET_STATE_LINK_DOWN))
		wake_up_interruptible(&vnet_info->stop_waitq);
}


/***********************************************************
  Pre-allocated "list" of objects which are allocated and deallocated in FIFO
  sequence. Allows reservation of memory at init time to prevent mem allocation
  failures at run time. */
static int
list_obj_list_init(int num_obj, size_t obj_size, struct obj_list *list)
{
	list->size = num_obj + 1;
	list->obj_size = obj_size;
	list->head = list->tail	= 0;

	if (!(list->buf = kmalloc(list->size * list->obj_size, GFP_KERNEL))) {
		printk(KERN_ERR "%s: list alloc failed\n", __func__);
		return -ENOMEM;
	}
	return 0;
}

static void
list_obj_list_deinit(struct obj_list *list)
{
	if (list->buf) {
		kfree(list->buf);
		list->buf = NULL;
	}
}

static void *
list_obj_alloc(struct obj_list *list)
{
	char *obj;

	/* Remove bug_on() here to handle VNET OOO messages. In OOO conditions
	 * requests to allocate more objects than list->size are possible.  */
        if (((list->head + 1) % list->size) == list->tail) {
		printk(KERN_ERR "%s: BUG: no free objects in obj list\n", __func__);
		return NULL;
	}

	obj = list->buf + list->head * list->obj_size;
	wmb();
	list->head = (list->head + 1) % list->size;

	return obj;
}

void
list_obj_free(struct obj_list *list)
{
	/* Remove bug_on() here to handle VNET OOO messages */
        if (list->tail == list->head) {
		printk(KERN_ERR "%s: BUG: free too many list objects\n", __func__);
		return;
	}

	list->tail = (list->tail + 1) % list->size;
}

/***********************************************************
 *  Vnet message functions
 */
#ifdef HOST
static void
micvnet_msg_rb_init(struct micvnet_msg_rb *rb)
{
	rb->head = rb->tail = 0;
	rb->size = MICVNET_MSG_RB_SIZE;
	rb->prev_head = rb->prev_tail = rb->size - 1;
}

static void
micvnet_reset_msg_rings(struct micvnet_info *vnet_info)
{
	micvnet_msg_rb_init(vnet_info->vi_qp.tx);
	micvnet_msg_rb_init(vnet_info->vi_qp.rx);
}
#endif

static void
micvnet_msg_rb_write_msg(struct micvnet_info *vnet_info, struct micvnet_msg *msg)
{
	struct micvnet_msg_rb *rb = vnet_info->vi_qp.tx;

	/* The condition below should never occur under normal conditions
	   because the VNET message ring buffer size is at least 1 greater than
	   the maximum total number of outstanding messages possible in the
	   system. However, all bets are off if VNET OOO messages are
	   seen. Therefore remove the previous bug_on() here and busy wait. */
	while (((rb->head + 1) % rb->size) == rb->tail)
		cpu_relax();

	if (!(rb->head == (rb->prev_head + 1) % rb->size))
		printk(KERN_ERR "BUG: head not equal to prev_head + 1:\n \
			head %d prev_head %d\n", rb->head, rb->prev_head);

	smp_mb();
#ifdef HOST
	rb->buf[rb->head] = *msg;
#else
	memcpy_toio(&rb->buf[rb->head], msg, sizeof(*msg));
#endif
	smp_mb();
	serializing_request(&rb->buf[rb->head]);

	rb->prev_head = rb->head;
	rb->head = (rb->head + 1) % rb->size;
#ifndef HOST
	rb->head = rb->head;
#endif
	smp_mb();
	serializing_request(&rb->head);
}

static int
micvnet_msg_rb_read_msg(struct micvnet_info *vnet_info, struct micvnet_msg *msg)
{
	struct micvnet_msg_rb *rb = vnet_info->vi_qp.rx;

	if (rb->tail == rb->head)
		return 1;

	if (!(rb->tail == (rb->prev_tail + 1) % rb->size))
		printk(KERN_ERR "BUG: tail not equal to prev_tail + 1:\n \
			tail %d prev_tail %d\n", rb->tail, rb->prev_tail);

	smp_mb();
#ifdef HOST
	*msg = rb->buf[rb->tail];
#else
	memcpy_fromio(msg, &rb->buf[rb->tail], sizeof(*msg));
#endif
	smp_mb();
	serializing_request(&rb->buf[rb->tail]);

	rb->prev_tail = rb->tail;
	rb->tail = (rb->tail + 1) % rb->size;
#ifndef HOST
	rb->tail = rb->tail;
#endif
	smp_mb();
	serializing_request(&rb->tail);

	return 0;
}

void
micvnet_msg_send_msg(struct micvnet_info *vnet_info, struct micvnet_msg *msg)
{
	micvnet_msg_rb_write_msg(vnet_info, msg);
#ifdef HOST
	if (micpm_get_reference(vnet_to_ctx(vnet_info), true))
		return;
#endif
	micvnet_send_intr(vnet_info);
#ifdef HOST
	micpm_put_reference(vnet_to_ctx(vnet_info));
#endif
}

static void
micvnet_msg_send_add_dma_buffer_msg(struct micvnet_info *vnet_info,
				    struct rx_node *rnode)
{
	struct micvnet_msg msg;
	struct micvnet_msg_add_dma_buffer
		*body = &msg.body.micvnet_msg_add_dma_buffer;

	msg.msg_id     = MICVNET_MSG_ADD_DMA_BUFFER;
	body->buf_phys = rnode->phys;
	body->buf_size = rnode->size;
	micvnet_msg_send_msg(vnet_info, &msg);
}

static void
micvnet_msg_recv_add_dma_buffer(struct micvnet_info *vnet_info,
				struct micvnet_msg_add_dma_buffer *msg)
{
	struct dma_node *dnode;

	/* Remove bug_on() here to handle VNET OOO messages */
	if (!(dnode = list_obj_alloc(&vnet_info->dnode_list)))
		return;

	dnode->phys = msg->buf_phys;
	dnode->size = msg->buf_size;

	spin_lock(&vnet_info->vi_rxlock);
	list_add_tail(&dnode->list, &vnet_info->vi_dma_buf);
	spin_unlock(&vnet_info->vi_rxlock);

	atomic_inc(&vnet_info->cnt_dma_buf_avail);
	micvnet_wake_queue(vnet_info);
}

static void
micvnet_msg_send_dma_complete_msg(struct micvnet_info *vnet_info,
				  struct sched_node *snode)
{
	struct micvnet_msg msg;
	struct micvnet_msg_dma_complete
		*body = &msg.body.micvnet_msg_dma_complete;

	msg.msg_id	 = MICVNET_MSG_DMA_COMPLETE;
	body->dst_phys	 = snode->dst_phys;
	body->size	 = snode->skb->len;
	body->dma_offset = snode->dma_offset;
	micvnet_msg_send_msg(vnet_info, &msg);
}

/* Handle an unexpected out-of-order message */
static int
micvnet_msg_handle_ooo_msg(struct micvnet_info *vnet_info,
			struct micvnet_msg_dma_complete *msg)
{
	struct micvnet_msg_rb *rb = vnet_info->vi_qp.rx;
	struct rx_node *rnode;
	struct list_head *pos, *tmpl;
	bool found = false;

	rnode = list_entry((&vnet_info->vi_rx_skb)->next, struct rx_node, list);

	/* Normal operation */
	if (rnode->phys == msg->dst_phys
		&& msg->size <= (rnode->size - 3 * DMA_ALIGNMENT)
		&& msg->dma_offset < 2 * DMA_ALIGNMENT)
		return 0;

	/* Flag that weird stuff's going on */
	printk(KERN_ERR "BUG: Unexpected vnet dma_complete message parameters:\n \
			rnode->phys %p, msg->dst_phys %p\n		\
			rnode->size %lld, msg->size %lld, msg->dma_offset %lld\n \
			rx rb head %d tail %d size %d\n", 
		(char *) rnode->phys, (char *) msg->dst_phys, 
		rnode->size, msg->size, msg->dma_offset, 
		rb->head, rb->tail, rb->size);

	/* if message is received in order but with incorrect parameters
	   (size/dma_offset), drop it, but re-add the rnode at the back of the
	   rx_skb list, as well as at tx, similar to what is done below for ooo
	   case. */
	if (rnode->phys == msg->dst_phys) {
		list_del(&rnode->list);
		list_add_tail(&rnode->list, &vnet_info->vi_rx_skb);
		micvnet_msg_send_add_dma_buffer_msg(vnet_info, rnode);
		vnet_info->vi_netdev->stats.rx_dropped++;
		return 1;
	}

	/* Start of OOO message processing. First check if the message has
	 * really been received OOO. If it is completely unknown to us we just
	 * drop it and go on. */
	list_for_each(pos, &vnet_info->vi_rx_skb) {
		rnode = list_entry(pos, struct rx_node, list);
		if (rnode->phys == msg->dst_phys) {
			found = true;
			break;
		}
	}

	if (!found) {
		vnet_info->vi_netdev->stats.rx_dropped++;
		return 1;
	}

	vnet_info->vi_netdev->stats.rx_errors++;

	/* Skip all the rnode's till we find the one we are looking for. Rather
	 * than free rnode skb's and reallocate them, and therby risk allocation
	 * failures, we simply delete the rnode's from their current position on
	 * the rnode list and re-add them at back of the list, as well as add
	 * them back at tx.  */
	list_for_each_safe(pos, tmpl, &vnet_info->vi_rx_skb) {
		rnode = list_entry(pos, struct rx_node, list);
		if (rnode->phys == msg->dst_phys)
			break;

		list_del(&rnode->list);
		list_add_tail(&rnode->list, &vnet_info->vi_rx_skb);
		micvnet_msg_send_add_dma_buffer_msg(vnet_info, rnode);
	}

	return 0;
}

static void
micvnet_msg_recv_dma_complete(struct micvnet_info *vnet_info,
			      struct micvnet_msg_dma_complete *msg)
{
	struct rx_node *rnode;
	struct sk_buff *skb;

	vnet_info->vi_netdev->stats.rx_packets++;

	if (micvnet_msg_handle_ooo_msg(vnet_info, msg))
		return;

	rnode = list_entry((&vnet_info->vi_rx_skb)->next, struct rx_node, list);
	/* Our OOO message handling guarantees that rnode->phys == msg->dst_phys */

	vnet_info->vi_netdev->stats.rx_bytes += msg->size;
	list_del(&rnode->list);

	spin_lock_bh(&vnet_info->vi_txlock);
	if (atomic_read(&vnet_info->vi_state) != MICVNET_STATE_LINKUP) {
		spin_unlock_bh(&vnet_info->vi_txlock);
		goto skip_adding_new_buffers;
	}
	atomic_inc(&vnet_info->cnt_tx_pending);
	spin_unlock_bh(&vnet_info->vi_txlock);

	/* OOM handling: check if a new SKB can be allocated. If not, we will re-add the
	   old SKB to TX and not give it to the network stack, i.e. drop it */
	if (micvnet_init_rx_skb_send_msg(vnet_info)) {
		list_add_tail(&rnode->list, &vnet_info->vi_rx_skb);
		micvnet_msg_send_add_dma_buffer_msg(vnet_info, rnode);
		micvnet_dec_cnt_tx_pending(vnet_info);
		vnet_info->vi_netdev->stats.rx_dropped++;
		return;
	}
	micvnet_dec_cnt_tx_pending(vnet_info);

skip_adding_new_buffers:
	skb = rnode->skb;
	skb_reserve(skb, msg->dma_offset);
	skb_put(skb, msg->size);
	skb->dev = vnet_info->vi_netdev;
	skb->protocol = eth_type_trans(skb, skb->dev);
	skb->ip_summed = CHECKSUM_NONE;

	local_bh_disable();
	netif_receive_skb(skb);
	local_bh_enable();

#ifdef HOST
	mic_ctx_unmap_single(vnet_to_ctx(vnet_info), rnode->phys, rnode->size);
#endif
	kfree(rnode);
}

static void
micvnet_msg_send_link_down_msg(struct work_struct *work)
{
	struct micvnet_info *vnet_info
		= container_of(work, struct micvnet_info, vi_ws_link_down);
	struct micvnet_msg msg;
	msg.msg_id = MICVNET_MSG_LINK_DOWN;
	micvnet_msg_send_msg(vnet_info, &msg);
}

static void
micvnet_msg_recv_msg_link_down(struct micvnet_info *vnet_info)
{
	atomic_set(&vnet_info->vi_state, MICVNET_STATE_BEGIN_UNINIT);

	if (vnet_info->link_down_initiator)
		wake_up_interruptible(&vnet_info->stop_waitq);
	else
		schedule_work(&vnet_info->vi_ws_stop);
}

static void
micvnet_msg_send_link_up_msg(struct micvnet_info *vnet_info)
{
	struct micvnet_msg msg;
	struct micvnet_msg_link_up
		*body = &msg.body.micvnet_msg_link_up;

	msg.msg_id = MICVNET_MSG_LINK_UP;
	body->vnet_driver_version = VNET_DRIVER_VERSION;
	micvnet_msg_send_msg(vnet_info, &msg);
}

static void
micvnet_msg_recv_msg_link_up(struct micvnet_info *vnet_info,
			     struct micvnet_msg_link_up *msg)
{
	if (msg->vnet_driver_version != VNET_DRIVER_VERSION) {
		printk(KERN_ERR "%s: Error: vnet driver version mismatch: "
			"expected %d actual %lld\n"
			"Ensure that host and card modules are "
			"from the same build.\n", 
			__func__, VNET_DRIVER_VERSION, 
			msg->vnet_driver_version);
		return;
	}
#ifdef HOST
	schedule_work(&vnet_info->vi_ws_start);
#else
	micvnet_send_add_dma_buffer_messages(vnet_info);
#endif
}

static void
micvnet_msg_process_messages(struct micvnet_info *vnet_info)
{
	struct micvnet_msg msg;

#ifdef HOST
	micpm_get_reference(vnet_to_ctx(vnet_info), true);
#endif
	while (!micvnet_msg_rb_read_msg(vnet_info, &msg)) {
		switch(msg.msg_id) {
		case MICVNET_MSG_ADD_DMA_BUFFER:
			micvnet_msg_recv_add_dma_buffer
				(vnet_info,
				 &msg.body.micvnet_msg_add_dma_buffer);
			break;

		case MICVNET_MSG_DMA_COMPLETE:
			micvnet_msg_recv_dma_complete
				(vnet_info,
				 &msg.body.micvnet_msg_dma_complete);
			break;

		case MICVNET_MSG_LINK_DOWN:
			micvnet_msg_recv_msg_link_down(vnet_info);
			break;

		case MICVNET_MSG_LINK_UP:
			micvnet_msg_recv_msg_link_up(vnet_info,
						&msg.body.micvnet_msg_link_up);
			break;

		default:
			printk(KERN_ERR "BUG: unknown vnet msg id: %lld\n", msg.msg_id);
			break;
		}
	}
#ifdef HOST
	micpm_put_reference(vnet_to_ctx(vnet_info));
#endif
}

/***********************************************************
 *  Interrupts
 */
#ifdef HOST
static int
micvnet_host_doorbell_intr_handler(mic_ctx_t *mic_ctx, int doorbell)
{
	struct micvnet_info *vnet_info;
	vnet_info = mic_ctx->bi_vethinfo;

	queue_work(vnet_info->vi_wq, &vnet_info->vi_ws_bh);
	return 0;
}
#else
static irqreturn_t 
micvnet_host_intr_handler(int irq, void *data)
{
	struct micvnet_info *vnet_info = data;
	queue_work(vnet_info->vi_wq, &vnet_info->vi_ws_bh);
	return IRQ_HANDLED;
}
#endif

static void
micvnet_intr_bh_handler(struct work_struct *work)
{
	struct micvnet_info *vnet_info
		= container_of(work, struct micvnet_info, vi_ws_bh);

	micvnet_msg_process_messages(vnet_info);
}

#ifdef HOST
static void
micvnet_send_intr(struct micvnet_info *vnet_info)
{
	mic_ctx_t *mic_ctx = vnet_info->mic_ctx;
	mic_send_vnet_intr(mic_ctx);
}
#else
/* Ring host doorbell 3 interrupt */
static void
micvnet_send_intr(struct micvnet_info *vnet_info)
{
	uint32_t db_reg;

	/* Ring host doorbell 3 interrupt */
	db_reg = readl(vnet_info->vi_sbox + SBOX_SDBIC3)
			| SBOX_SDBIC0_DBREQ_BIT;
	writel(db_reg, vnet_info->vi_sbox + SBOX_SDBIC3);
}
#endif

/***********************************************************
 *  Net device ops and rtnl link ops
 */
/*
  Do nothing in ndo_open and ndo_stop. There are two reasons for this:
  1. Since host and card side drivers are driver pairs, if ifconfig up or
     ifconfig down occurs on one side this needs to be communicated to the other
     side other side otherwise in the current implementation this can bring down
     the system. Ignoring ifconfig up or down avoids this issue.
  2. For now, micvnet_init is called before the dma can be initialized. However,
     as soon as micvnet_init has been called and netdev has been created, the OS
     can invoke .ndo_open, which however requires the DMA to have been
     initialized. But DMA can not be initialized until later (at present after
     the card has booted).
  Therefore we ourselves call micvnet_start and micvnet_stop at appropriate
  times when we are ready for them. The only consequence is all packets till
  micvnet_start has been invoked will be dropped in ndo_start_xmit.
 */

/* Start callback  */
static int
micvnet_start_dev(struct net_device *dev)
{
	struct micvnet_info *vnet_info = dev->ml_priv;

	/* Stop the queue till the state becomes LINKUP. The queue will be started when
	   dma buffers are added in micvnet_msg_recv_add_dma_buffer(). Not doing this
	   results in packets getting dropped till state is LINKUP. */
	if (atomic_read(&vnet_info->vi_state) != MICVNET_STATE_LINKUP)
		netif_stop_queue(vnet_info->vi_netdev);

	return 0;
}

/* Stop callback */
static int
micvnet_stop_dev(struct net_device *dev)
{
	return 0;
}

static void
micvnet_dma_cb_bh(struct work_struct *work)
{
	struct micvnet_info
		*vnet_info = container_of(work, struct micvnet_info, vi_ws_dmacb);
	struct sched_node *snode;

	if (!atomic_read(&vnet_info->cnt_dma_complete))
		return;

	do {
		spin_lock_bh(&vnet_info->vi_txlock);
		snode = list_entry((&vnet_info->vi_sched_skb)->next, 
					 struct sched_node, list);
		list_del(&snode->list);
		spin_unlock_bh(&vnet_info->vi_txlock);

		micvnet_msg_send_dma_complete_msg(vnet_info, snode);

		micvnet_dec_cnt_tx_pending(vnet_info);
#ifdef HOST
		mic_ctx_unmap_single(vnet_to_ctx(vnet_info),
				snode->dma_src_phys, snode->dma_size);
		micpm_put_reference(vnet_to_ctx(vnet_info));
#endif
		kfree_skb(snode->skb);
		kfree(snode);

	} while (!atomic_dec_and_test(&vnet_info->cnt_dma_complete));
}

static void
micvnet_dma_completion_callback(uint64_t data)
{
	struct micvnet_info *vnet_info = (struct micvnet_info *) data;

	atomic_inc(&vnet_info->cnt_dma_complete);

	queue_work(vnet_info->vi_wq, &vnet_info->vi_ws_dmacb);
}

static int
micvnet_do_dma(struct micvnet_info *vnet_info, struct sched_node *snode)
{
	uint64_t dma_src, dma_dst;
	int ret = 0;

	dma_src = snode->dma_src_phys;
	dma_dst = ALIGN(snode->dst_phys, DMA_ALIGNMENT);
	snode->dma_offset = (snode->skb->data - snode->skb_data_aligned)
				+ (dma_dst - snode->dst_phys);
	if ((ret = request_dma_channel(vnet_info->dma_chan)))
		goto err_exit;

	ret = do_dma(vnet_info->dma_chan,
		     DO_DMA_INTR,
		     dma_src,
		     dma_dst,
		     snode->dma_size,
		     &vnet_info->dma_cb);

	free_dma_channel(vnet_info->dma_chan);

err_exit:
	return ret;
}

static int
micvnet_schedule_dma(struct micvnet_info *vnet_info)
{
	struct tx_node *tnode;
	struct sched_node *snode;
	struct dma_node *dnode;
	struct sk_buff *skb;
	int ret = 0;
	/* tnode */
	spin_lock_bh(&vnet_info->vi_txlock);
	BUG_ON(list_empty(&vnet_info->vi_tx_skb));
	tnode = list_entry((&vnet_info->vi_tx_skb)->next, 
				 struct tx_node, list);
	list_del(&tnode->list);
	spin_unlock_bh(&vnet_info->vi_txlock);
	skb = tnode->skb;
	kfree(tnode);

#ifdef HOST
	if ((ret = micpm_get_reference(vnet_to_ctx(vnet_info), true)))
		goto err_exit_no_dec_node_refcnt;
#endif

	/* dnode */
	spin_lock(&vnet_info->vi_rxlock);
	BUG_ON(list_empty(&vnet_info->vi_dma_buf));
	dnode = list_entry((&vnet_info->vi_dma_buf)->next, 
				 struct dma_node, list);
	spin_unlock(&vnet_info->vi_rxlock);
	if (dnode->size < skb->len + 3 * DMA_ALIGNMENT) {
		ret = -ENOMEM;
		goto err_exit;
	}

	/* snode */
	if (!(snode = kmalloc(sizeof(*snode), GFP_KERNEL))) {
		ret = -ENOMEM;
		goto err_exit;
	}
	snode->skb = skb;
	snode->dst_phys = dnode->phys;
	snode->skb_data_aligned
		= (unsigned char *) ((uint64_t) skb->data & ~(DMA_ALIGNMENT - 1));
	snode->dma_size
		= ALIGN((skb->len + (skb->data - snode->skb_data_aligned)), 
			      DMA_ALIGNMENT);
#ifdef HOST
	snode->dma_src_phys = mic_ctx_map_single(vnet_to_ctx(vnet_info),
			snode->skb_data_aligned,
			snode->dma_size);
	if (mic_map_error(snode->dma_src_phys)) {
		kfree(snode);
		ret = -ENOMEM;
		goto err_exit;
	}
#else
	snode->dma_src_phys = virt_to_phys(snode->skb_data_aligned);
#endif

	if ((ret = micvnet_do_dma(vnet_info, snode))) {
#ifdef HOST
		mic_ctx_unmap_single(vnet_to_ctx(vnet_info),
				       snode->dma_src_phys, snode->dma_size);
#endif
		kfree(snode);
		goto err_exit;
	}

	/* Update snode/dnode lists only after all operations have successfully
	   completed and no further errors are possible */
	spin_lock_bh(&vnet_info->vi_txlock);
	list_add_tail(&snode->list, &vnet_info->vi_sched_skb);
	spin_unlock_bh(&vnet_info->vi_txlock);

	spin_lock(&vnet_info->vi_rxlock);
	list_del(&dnode->list);
	spin_unlock(&vnet_info->vi_rxlock);
	list_obj_free(&vnet_info->dnode_list);

	vnet_info->vi_netdev->stats.tx_packets++;
	vnet_info->vi_netdev->stats.tx_bytes += skb->len;

	return ret;

err_exit:
#ifdef HOST
	micpm_put_reference(vnet_to_ctx(vnet_info));
err_exit_no_dec_node_refcnt:
#endif
	micvnet_dec_cnt_tx_pending(vnet_info);
	atomic_inc(&vnet_info->cnt_dma_buf_avail);
	micvnet_wake_queue(vnet_info);
	skb->dev->stats.tx_dropped++;
	kfree_skb(skb);
	return ret;
}

static void
micvnet_schedule_dmas(struct work_struct *work)
{
	struct micvnet_info *vnet_info
		= container_of(work, struct micvnet_info, vi_ws_tx);
	volatile bool tx_skb_list_empty;
	while (1) {
		spin_lock_bh(&vnet_info->vi_txlock);
		tx_skb_list_empty = list_empty(&vnet_info->vi_tx_skb);
		spin_unlock_bh(&vnet_info->vi_txlock);
		if (tx_skb_list_empty)
			break;

		micvnet_schedule_dma(vnet_info);
	}
}

int
micvnet_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct micvnet_info *vnet_info = (struct micvnet_info*)dev->ml_priv;
	struct tx_node  *tnode;
	if (!vnet_info || !atomic_read(&vnet_info->cnt_dma_buf_avail)){
		goto err_exit;
	}

	if (!(tnode = kmalloc(sizeof(*tnode), GFP_ATOMIC)))
		goto err_exit;
	tnode->skb = skb;

	spin_lock(&vnet_info->vi_txlock);
	if (atomic_read(&vnet_info->vi_state) != MICVNET_STATE_LINKUP)
		goto err_exit_unlock;
	list_add_tail(&tnode->list, &vnet_info->vi_tx_skb);
	atomic_inc(&vnet_info->cnt_tx_pending);
	spin_unlock(&vnet_info->vi_txlock);

	queue_work(vnet_info->vi_wq, &vnet_info->vi_ws_tx);

	if (atomic_dec_and_test(&vnet_info->cnt_dma_buf_avail))
		netif_stop_queue(vnet_info->vi_netdev);

	return NETDEV_TX_OK;

err_exit_unlock:
	kfree(tnode);
	spin_unlock(&vnet_info->vi_txlock);
err_exit:
	kfree_skb(skb);
	dev->stats.tx_dropped++;
	return NETDEV_TX_OK;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,2,0)
static void
micvnet_multicast_list(struct net_device *dev)
{
}
#endif

static int
micvnet_set_address(struct net_device *dev, void *p)
{
	struct sockaddr *sa = p;

	if (!is_valid_ether_addr(sa->sa_data))
		return -EADDRNOTAVAIL;

	memcpy(dev->dev_addr, sa->sa_data, ETH_ALEN);
	return 0;
}

#define MIN_MTU 68
#define MAX_MTU MICVNET_MAX_MTU

static int
micvnet_change_mtu(struct net_device *dev, int new_mtu)
{
	if (new_mtu < MIN_MTU || new_mtu > MAX_MTU)
		return -EINVAL;
	dev->mtu = new_mtu;
	return 0;
}

union serial {
	uint32_t regs[3];
	char	 string[13];
};

void
mic_get_serial_from_dbox(struct micvnet_info *vni, char *serialnum)
{
	union serial serial;
#ifdef HOST
	serial.regs[0] = DBOX_READ(vni->mic_ctx->mmio.va, DBOX_SWF1X0);
	serial.regs[1] = DBOX_READ(vni->mic_ctx->mmio.va, DBOX_SWF1X1);
	serial.regs[2] = DBOX_READ(vni->mic_ctx->mmio.va, DBOX_SWF1X2);
#else
	serial.regs[0] = readl(vni->vi_dbox + DBOX_SWF1X0);
	serial.regs[1] = readl(vni->vi_dbox + DBOX_SWF1X1);
	serial.regs[2] = readl(vni->vi_dbox + DBOX_SWF1X2);
#endif
	serial.string[12] = '\0';
	strcpy(serialnum, serial.string);
}

int
micvnet_setmac_from_serial(struct net_device *dev)
{
	struct micvnet_info *vni = (struct micvnet_info *)dev->ml_priv;
	char serialnum[17];
	int err;
	
	mic_get_serial_from_dbox(vni, serialnum);
#ifdef HOST
	err = mic_get_mac_from_serial(serialnum, dev->dev_addr, 1);
#else
	err = mic_get_mac_from_serial(serialnum, dev->dev_addr, 0);
#endif
	return err;
}

static const struct net_device_ops micvnet_netdev_ops = {
	.ndo_open		= micvnet_start_dev,
	.ndo_stop		= micvnet_stop_dev,
	.ndo_start_xmit		= micvnet_xmit,
	.ndo_validate_addr	= eth_validate_addr,
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,2,0)
	.ndo_set_multicast_list = micvnet_multicast_list,
#endif
	.ndo_set_mac_address	= micvnet_set_address,
	.ndo_change_mtu		= micvnet_change_mtu,
};

static void
micvnet_setup(struct net_device *dev)
{
	ether_setup(dev);

	/* Initialize the device structure. */
	dev->netdev_ops = &micvnet_netdev_ops;
	dev->destructor = free_netdev;

	/* Fill in device structure with ethernet-generic values. */
	dev->mtu = MICVNET_MAX_MTU;
	dev->flags &= ~IFF_MULTICAST;
}

static struct rtnl_link_ops micvnet_link_ops __read_mostly = {
	.kind		= "micvnet",
	.setup		= micvnet_setup,
};

/***********************************************************
 *  Vnet init/deinit
 */
static int
micvnet_init_hw_regs(struct micvnet_info *vnet_info)
{
#ifdef HOST
	mic_ctx_t *mic_ctx = vnet_info->mic_ctx;

	vnet_info->vi_pdev = mic_ctx->bi_pdev;
	vnet_info->vi_sbox = (uint8_t *)((unsigned long) mic_ctx->mmio.va +
					 HOST_SBOX_BASE_ADDRESS);
	vnet_info->vi_scratch14
		= (uint32_t *)((unsigned long)mic_ctx->mmio.va +
			       HOST_SBOX_BASE_ADDRESS + SBOX_SCRATCH14);
#else
	vnet_info->vi_sbox = ioremap_nocache(SBOX_BASE, SBOX_MMIO_LENGTH);
	vnet_info->vi_dbox = ioremap_nocache(DBOX_BASE, SBOX_MMIO_LENGTH);
	if (!vnet_info->vi_sbox) {
		printk(KERN_ERR "%s: NULL SBOX ptr\n", __func__);
		return -ENOMEM;
	}
	vnet_info->vi_scratch14
		= (uint32_t *)(vnet_info->vi_sbox + SBOX_SCRATCH14);
#endif
	return 0;
}

static void
micvnet_deinit_hw_regs(struct micvnet_info *vnet_info)
{
#ifndef HOST
	iounmap(vnet_info->vi_sbox);
	iounmap(vnet_info->vi_dbox);
#endif
}

static int
micvnet_init_interrupts(struct micvnet_info *vnet_info)
{
	mic_ctx_t *mic_ctx = vnet_info->mic_ctx;
	int ret = 0;

	spin_lock_init(&vnet_info->vi_txlock);
	spin_lock_init(&vnet_info->vi_rxlock);

	snprintf(vnet_info->vi_wqname, sizeof(vnet_info->vi_wqname),
		 "VNET WQ %d", mic_ctx->bi_id);

	if (!(vnet_info->vi_wq =
	      __mic_create_singlethread_workqueue(vnet_info->vi_wqname))) {
		printk(KERN_ERR "%s: create_singlethread_workqueue\n", __func__);
		return -ENOMEM;
	}
	init_waitqueue_head(&vnet_info->stop_waitq);

	INIT_WORK(&vnet_info->vi_ws_bh, micvnet_intr_bh_handler);
	INIT_WORK(&vnet_info->vi_ws_tx, micvnet_schedule_dmas);
	INIT_WORK(&vnet_info->vi_ws_dmacb, micvnet_dma_cb_bh);
	INIT_WORK(&vnet_info->vi_ws_link_down, micvnet_msg_send_link_down_msg);
	INIT_WORK(&vnet_info->vi_ws_stop, micvnet_stop_ws);
	INIT_WORK(&vnet_info->vi_ws_start, micvnet_start_ws);
#ifdef HOST
	if ((ret = mic_reg_irqhandler(mic_ctx, 3, "Host DoorBell 3",
				      micvnet_host_doorbell_intr_handler))) {
#else
	if ((ret = request_irq(get_sbox_irq(VNET_SBOX_INT_IDX),
				micvnet_host_intr_handler, IRQF_DISABLED,
				"vnet intr", vnet_info))) {
#endif
		printk(KERN_ERR "%s: interrupt registration failed\n", __func__);
		goto err_exit_destroy_workqueue;
	}
	return 0;

err_exit_destroy_workqueue:
	destroy_workqueue(vnet_info->vi_wq);
	return ret;
}

static void
micvnet_deinit_interrupts(struct micvnet_info *vnet_info)
{
#ifdef HOST
	mic_unreg_irqhandler(vnet_info->mic_ctx, 3, "Host DoorBell 3");
#else
	free_irq(get_sbox_irq(VNET_SBOX_INT_IDX), vnet_info);
#endif
	destroy_workqueue(vnet_info->vi_wq);
}


static int
micvnet_init_netdev(struct micvnet_info *vnet_info)
{
	struct net_device *dev_vnet;
	int ret = 0;


#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,17,0))
	if ((dev_vnet = (struct net_device *)alloc_netdev(sizeof(struct micvnet_info), "mic%d",
					   NET_NAME_UNKNOWN, micvnet_setup)) == NULL) {
#else
	if ((dev_vnet = (struct net_device *)alloc_netdev(sizeof(struct micvnet_info), "mic%d", 
					   micvnet_setup)) == NULL) {
#endif
		printk(KERN_ERR "%s: alloc_netdev failed\n", __func__);
		return -ENOMEM;
	}

	vnet_info->vi_netdev = dev_vnet;
	dev_vnet->ml_priv = vnet_info;

	if (micvnet_setmac_from_serial(dev_vnet))
		random_ether_addr(dev_vnet->dev_addr);

	dev_vnet->rtnl_link_ops = &micvnet_link_ops;

	if ((ret = register_netdev(dev_vnet)) < 0) {
		printk(KERN_ERR "%s: register_netdev failed %d\n", __func__, ret);
		free_netdev(dev_vnet);
		return ret;
	}

	return 0;
}

static int
micvnet_init_msg_rings(struct micvnet_info *vnet_info)
{
#ifdef HOST
 	vnet_info->vi_qp.tx = &vnet_info->vi_rp.rb_tx;
	vnet_info->vi_qp.rx = &vnet_info->vi_rp.rb_rx;
	micvnet_reset_msg_rings(vnet_info);

	vnet_info->vi_rp_phys = mic_ctx_map_single(vnet_to_ctx(vnet_info),
		       		       		&vnet_info->vi_rp,
					       sizeof(vnet_info->vi_rp));
	if (mic_map_error(vnet_info->vi_rp_phys)) {
		printk(KERN_ERR "%s: mic_map_error failed\n", __func__);
		return -ENOMEM;
	}
#else
	if (!(vnet_info->vi_rp_phys = vnet_addr)) {
		printk(KERN_ERR "%s: null vnet_addr\n", __func__);
		return -ENOMEM;
	}
	vnet_info->ring_ptr
		= ioremap_nocache(vnet_info->vi_rp_phys, 
				       sizeof(struct micvnet_msg_ring_pair));
	if (!vnet_info->ring_ptr) {
		printk(KERN_ERR "%s: NULL ring ptr\n", __func__);
		return -ENOMEM;
	}
	vnet_info->vi_qp.tx = &vnet_info->ring_ptr->rb_rx;
	vnet_info->vi_qp.rx = &vnet_info->ring_ptr->rb_tx;
#endif
	return 0;
}

static void
micvnet_deinit_msg_rings(struct micvnet_info *vnet_info)
{
#ifdef HOST
	mic_ctx_unmap_single(vnet_to_ctx(vnet_info),
			     vnet_info->vi_rp_phys, sizeof(vnet_info->vi_rp));
#else
	iounmap(vnet_info->ring_ptr);
#endif
}

static int
micvnet_init_lists(struct micvnet_info *vnet_info)
{
	int ret;
	if ((ret = list_obj_list_init(VNET_MAX_SKBS, sizeof(struct dma_node),
				      &vnet_info->dnode_list)))
		return ret;

	INIT_LIST_HEAD(&vnet_info->vi_rx_skb);
	INIT_LIST_HEAD(&vnet_info->vi_dma_buf);
	INIT_LIST_HEAD(&vnet_info->vi_tx_skb);
	INIT_LIST_HEAD(&vnet_info->vi_sched_skb);
	return 0;
}

static void
micvnet_deinit_lists(struct micvnet_info *vnet_info)
{
	struct list_head *pos, *tmpq;
	struct rx_node *rnode;
	struct tx_node *tnode;
	struct dma_node *dnode;
	struct sched_node *snode;

	list_for_each_safe(pos, tmpq, &vnet_info->vi_rx_skb) {
		rnode = list_entry(pos, struct rx_node, list);
		list_del(&rnode->list);
#ifdef HOST
		mic_ctx_unmap_single(vnet_to_ctx(vnet_info),
			         rnode->phys, rnode->size);
#endif
		kfree_skb(rnode->skb);
		kfree(rnode);
	}

	list_for_each_safe(pos, tmpq, &vnet_info->vi_dma_buf) {
		dnode = list_entry(pos, struct dma_node, list);
		list_del(&dnode->list);
		list_obj_free(&vnet_info->dnode_list);
	}

	list_for_each_safe(pos, tmpq, &vnet_info->vi_tx_skb) {
		tnode = list_entry(pos, struct tx_node, list);
		list_del(&tnode->list);
		kfree_skb(tnode->skb);
		kfree(tnode);
	}

	list_for_each_safe(pos, tmpq, &vnet_info->vi_sched_skb) {
		snode = list_entry(pos, struct sched_node, list);
		list_del(&snode->list);
#ifdef HOST
		mic_ctx_unmap_single(vnet_to_ctx(vnet_info), snode->dma_src_phys,
				snode->dma_size);
		micpm_put_reference(vnet_to_ctx(vnet_info));
#endif
		kfree_skb(snode->skb);
		kfree(snode);
	}

	list_obj_list_deinit(&vnet_info->dnode_list);
}
static int
micvnet_init_dma(struct micvnet_info *vnet_info)
{
	mic_ctx_t *mic_ctx = vnet_info->mic_ctx;
	int ret;

	 /* Note: open_dma_device must use mic_ctx->dma_handle since that is
	    used in the isr */
#ifdef HOST
	if (micpm_get_reference(mic_ctx, true) != 0) {
		printk(KERN_ERR "%s: micpm_get_reference failed\n", __func__);
		return -ENODEV;
	}

	if ((ret = open_dma_device(mic_ctx->bi_id + 1,
				   mic_ctx->mmio.va + HOST_SBOX_BASE_ADDRESS,
				   &mic_ctx->dma_handle))) {
		printk(KERN_ERR "%s: open_dma_device failed\n", __func__);
		micpm_put_reference(mic_ctx);
		return ret;
	}
	micpm_put_reference(mic_ctx);
#else
	if ((ret = open_dma_device(0, 0, &mic_ctx->dma_handle))) {
		printk(KERN_ERR "%s: open_dma_device failed\n", __func__);
		return ret;
	}
#endif

	vnet_info->dma_handle = mic_ctx->dma_handle;

	if ((ret = allocate_dma_channel(vnet_info->dma_handle,
					&vnet_info->dma_chan))) {
		printk(KERN_ERR "%s: allocate_dma_channel failed\n", __func__);
		goto err_exit_close_dma;
	}
	free_dma_channel(vnet_info->dma_chan);
	vnet_info->dma_cb.dma_completion_func = micvnet_dma_completion_callback;
	vnet_info->dma_cb.cb_cookie = (uint64_t) vnet_info;
	atomic_set(&vnet_info->cnt_dma_complete, 0);
	atomic_set(&vnet_info->cnt_dma_buf_avail, 0);
	vnet_info->link_down_initiator = false;
	atomic_set(&vnet_info->cnt_tx_pending, 0);
	return 0;

err_exit_close_dma:
	close_dma_device(mic_ctx->bi_id + 1, &vnet_info->dma_handle);
	return ret;
}

static void
micvnet_deinit_dma(struct micvnet_info *vnet_info)
{
	mic_ctx_t *mic_ctx = vnet_info->mic_ctx;

	close_dma_device(mic_ctx->bi_id + 1, &vnet_info->dma_handle);
}
static int
micvnet_alloc_rx_node(struct micvnet_info *vnet_info, struct rx_node **node)
{
	struct rx_node *rnode;

	if (!(rnode = kmalloc(sizeof(*rnode), GFP_KERNEL)))
		return -ENOMEM;

	rnode->size = vnet_info->vi_netdev->mtu + 3 * DMA_ALIGNMENT + ETH_HLEN;

	if (!(rnode->skb = dev_alloc_skb(rnode->size))) {
		kfree(rnode);
		return -ENOMEM;
	}

#ifdef HOST
	rnode->phys = mic_ctx_map_single(vnet_to_ctx(vnet_info),
					rnode->skb->data, rnode->size);
	if (mic_map_error(rnode->phys)) {
		kfree_skb(rnode->skb);
		kfree(rnode);
		return -ENOMEM;
	}
#else
	rnode->phys = virt_to_phys(rnode->skb->data);
#endif

	*node = rnode;

	return 0;
}

static int
micvnet_init_rx_skb_send_msg(struct micvnet_info *vnet_info)
{
	struct rx_node *rnode;
	int ret = 0;

	if (unlikely(ret = micvnet_alloc_rx_node(vnet_info, &rnode)))
		return ret;

	list_add_tail(&rnode->list, &vnet_info->vi_rx_skb);

	micvnet_msg_send_add_dma_buffer_msg(vnet_info, rnode);

	return 0;
}

static int
micvnet_init_rx_skbs(struct micvnet_info *vnet_info)
{
	struct rx_node *rnode;
	int i, ret = 0;


	if ( (vnet_num_buffers <= 0) || (vnet_num_buffers > VNET_MAX_SKBS) )
		vnet_num_buffers = VNET_MAX_SKBS;

	for (i = 0; i < vnet_num_buffers; i++) {
		if (unlikely(ret = micvnet_alloc_rx_node(vnet_info, &rnode)))
			return ret;

		list_add_tail(&rnode->list, &vnet_info->vi_rx_skb);
	}

	return ret;
}

static void
micvnet_send_add_dma_buffer_messages(struct micvnet_info *vnet_info)
{
	struct rx_node *rnode;
	struct list_head *pos;

	list_for_each(pos, &vnet_info->vi_rx_skb) {
		rnode = list_entry(pos, struct rx_node, list);
		micvnet_msg_send_add_dma_buffer_msg(vnet_info, rnode);
	}
}

static void
micvnet_initiate_link_down(struct micvnet_info *vnet_info)
{
	int ret;
	netif_tx_disable(vnet_info->vi_netdev);
	spin_lock_bh(&vnet_info->vi_txlock);
	atomic_set(&vnet_info->vi_state, MICVNET_STATE_LINK_DOWN);
	spin_unlock_bh(&vnet_info->vi_txlock);

	/* This wait precludes this function to be called from the context of
	 * the vnet wq thread */
	ret = wait_event_interruptible_timeout(
		vnet_info->stop_waitq, 
		(atomic_read(&vnet_info->cnt_tx_pending) == 0), 
		STOP_WAIT_TIMEOUT);
	if (!ret)
		printk(KERN_ERR "%s timeout waiting for Tx dma buffers to drain\n", __func__);
	/* To avoid introducing a lock in micvnet_msg_send_msg() send the
	 * LINK_DOWN message from vnet wq thread context. LINK_DOWN will be the
	 * LAST message sent. */
	queue_work(vnet_info->vi_wq, &vnet_info->vi_ws_link_down);
}

static void
micvnet_stop_deinit(struct micvnet_info *vnet_info)
{
	flush_workqueue(vnet_info->vi_wq);
	atomic_set(&vnet_info->vi_state, MICVNET_STATE_UNINITIALIZED);

	micvnet_deinit_dma(vnet_info);
	micvnet_deinit_lists(vnet_info);
#ifdef HOST
	micvnet_reset_msg_rings(vnet_info);
#endif
	atomic_dec(&micvnet.lv_active_clients);
}

int
micvnet_probe(mic_ctx_t *mic_ctx)
{
	struct micvnet_info *vnet_info;
	int ret = 0;

	mic_ctx->bi_vethinfo = NULL;

	if (!micvnet.created)
		return 1;

	if (!(vnet_info = kzalloc(sizeof(struct micvnet_info), GFP_KERNEL))) {
		printk(KERN_ERR "%s: vnet_info alloc failed\n", __func__);
		return -ENOMEM;
	}

	mic_ctx->bi_vethinfo = vnet_info;
	vnet_info->mic_ctx = mic_ctx;
	if ((ret = micvnet_init_hw_regs(vnet_info)))
		goto err_exit_free_vnet_info;
	if ((ret = micvnet_init_msg_rings(vnet_info)))
		goto err_exit_deinit_hw_regs;
	if ((ret = micvnet_init_interrupts(vnet_info)))
		goto err_exit_deinit_msg_rings;
	if ((ret = micvnet_init_netdev(vnet_info)))
		goto err_exit_deinit_interrupts;

	atomic_set(&vnet_info->vi_state, MICVNET_STATE_UNINITIALIZED);
	return 0;

err_exit_deinit_interrupts:
	micvnet_deinit_interrupts(vnet_info);
err_exit_deinit_msg_rings:
	micvnet_deinit_msg_rings(vnet_info);
err_exit_deinit_hw_regs:
	micvnet_deinit_hw_regs(vnet_info);
err_exit_free_vnet_info:
	kfree(vnet_info);

	return ret;
}

void
micvnet_remove(mic_ctx_t *mic_ctx)
{
	struct micvnet_info
		*vnet_info = (struct micvnet_info *) mic_ctx->bi_vethinfo;

	if (!vnet_info)
		return;

	micvnet_stop(mic_ctx);

	vnet_info->vi_netdev->ml_priv = NULL;

	micvnet_deinit_interrupts(vnet_info);
	micvnet_deinit_msg_rings(vnet_info);
	micvnet_deinit_hw_regs(vnet_info);

	mic_ctx->bi_vethinfo = NULL;

	kfree(vnet_info);
}

int
micvnet_execute_start(struct micvnet_info *vnet_info)
{
	int ret = 0;

	if (!vnet_info) {
		printk(KERN_ERR "%s: vnet_info is NULL\n", __func__);
		return 1;
	}

	if (atomic_cmpxchg(&vnet_info->vi_state, MICVNET_STATE_UNINITIALIZED, 
				 MICVNET_STATE_TRANSITIONING) != MICVNET_STATE_UNINITIALIZED) {
		printk(KERN_ERR "%s: wrong vnet state %d\n", __func__, 
			    atomic_read(&vnet_info->vi_state));
		return 1;
	}

	if ((ret = micvnet_init_lists(vnet_info)))
		goto err_exit;
	if ((ret = micvnet_init_dma(vnet_info)))
		goto err_exit_deinit_lists;
	if ((ret = micvnet_init_rx_skbs(vnet_info))) {
		printk(KERN_ERR "%s: micvnet_init_rx_skbs failed\n", __func__);
		goto err_exit_deinit_dma;
	}

	memset(&vnet_info->vi_netdev->stats, 0, sizeof(vnet_info->vi_netdev->stats));
	atomic_inc(&micvnet.lv_active_clients);
	atomic_set(&vnet_info->vi_state, MICVNET_STATE_LINKUP);

	micvnet_msg_send_link_up_msg(vnet_info);
#ifdef HOST
	micvnet_send_add_dma_buffer_messages(vnet_info);
#else
	writel(MICVNET_CARD_UP_MAGIC, vnet_info->vi_scratch14);
	/* Card adds DMA buffers to host after receiving MICVNET_MSG_LINK_UP */
#endif
	return 0;

err_exit_deinit_dma:
	micvnet_deinit_dma(vnet_info);
err_exit_deinit_lists:
	/* RX SKB's are deallocated in micvnet_deinit_lists() */
	micvnet_deinit_lists(vnet_info);
err_exit:
	atomic_set(&vnet_info->vi_state, MICVNET_STATE_UNINITIALIZED);
	return ret;
}

static void
micvnet_start_ws(struct work_struct *work)
{
	struct micvnet_info *vnet_info
		= container_of(work, struct micvnet_info, vi_ws_start);

	micvnet_execute_start(vnet_info);
}

int micvnet_start(mic_ctx_t *mic_ctx)
{
#ifndef HOST
	struct micvnet_info *vnet_info = (struct micvnet_info *) mic_ctx->bi_vethinfo;
	micvnet_execute_start(vnet_info);
#endif
	return 0;
}

void
micvnet_execute_stop(struct micvnet_info *vnet_info)
{
	int ret;
	if (!vnet_info)
		return;

	switch(atomic_read(&vnet_info->vi_state)) {
	case MICVNET_STATE_LINKUP:
	case MICVNET_STATE_BEGIN_UNINIT:
		break;
	default:
		return;
	}

#ifdef HOST
	if ((micpm_get_reference(vnet_to_ctx(vnet_info), true)) != 0)
		goto exit;
#endif
	micvnet_initiate_link_down(vnet_info);
	if (vnet_info->link_down_initiator && !(vnet_info->mic_ctx->state == MIC_SHUTDOWN && vnet_info->mic_ctx->sdbic1)){
		ret = wait_event_interruptible_timeout(
			vnet_info->stop_waitq, 
			(atomic_read(&vnet_info->vi_state) == MICVNET_STATE_BEGIN_UNINIT), 
			STOP_WAIT_TIMEOUT);
		if (!ret)
			printk(KERN_ERR "%s: timeout waiting for link down message response\n", __func__);
	}

#ifdef HOST
	micpm_put_reference(vnet_to_ctx(vnet_info));
exit:
#endif
	micvnet_stop_deinit(vnet_info);
}

void
micvnet_stop(mic_ctx_t *mic_ctx)
{
	struct micvnet_info *vnet_info = (struct micvnet_info *) mic_ctx->bi_vethinfo;

	vnet_info->link_down_initiator = true;
	micvnet_execute_stop(vnet_info);
}

static void
micvnet_stop_ws(struct work_struct *work)
{
	struct micvnet_info *vnet_info
		= container_of(work, struct micvnet_info, vi_ws_stop);

	vnet_info->link_down_initiator = false;
	micvnet_execute_stop(vnet_info);
}

#if !defined(WINDOWS) && defined(HOST)
static ssize_t
show_vnet(struct device *dev, struct device_attribute *attr, char *buf);
DEVICE_ATTR(vnet, S_IRUGO, show_vnet, NULL);

static ssize_t
show_vnet(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "Number of active vnet clients: %d\n",
			atomic_read(&micvnet.lv_active_clients));
}
#endif

int
micvnet_init(struct device *dev)
{
	int ret = 0;

	micvnet.created = 0;
	atomic_set(&micvnet.lv_active_clients, 0);

	if ((ret = rtnl_link_register(&micvnet_link_ops))) {
		printk(KERN_ERR "%s: rtnl_link_register failed\n", __func__);
		return ret;
	}

#ifdef HOST
	if ((ret = device_create_file(dev, &dev_attr_vnet))) {
		printk(KERN_ERR "%s: device_create_file failed\n", __func__);
		rtnl_link_unregister(&micvnet_link_ops);
		return ret;
	}
#endif
	micvnet.created = 1;
	return 0;
}

void
micvnet_exit(void)
{
	rtnl_link_unregister(&micvnet_link_ops);
}

#ifndef HOST
static void __exit
_micvnet_module_exit(void)
{
	mic_ctx_t *mic_ctx = &mic_ctx_g;

	micvnet_stop(mic_ctx);
	micvnet_remove(mic_ctx);
	micvnet_exit();
}

static int
micvnet_reboot(struct notifier_block *notifier, unsigned long unused1, void *unused2)
{
	/* Calling _micvnet_module_exit() here will hang the uOS during shutdown in NFS
	 * root case */
	return NOTIFY_OK;
}

static struct notifier_block micvnet_reboot_notifier = {
	.notifier_call = micvnet_reboot,
	.priority = 0,
};

void __exit
micvnet_module_exit(void)
{
	unregister_reboot_notifier(&micvnet_reboot_notifier);
	_micvnet_module_exit();
}

int __init
micvnet_module_init(void)
{
	mic_ctx_t *mic_ctx = &mic_ctx_g;
	int ret = 0;

	if ((ret = register_reboot_notifier(&micvnet_reboot_notifier))) {
		printk(KERN_ERR "register_reboot_notifier failed: error %d\n", ret);
		goto err_exit;
	}

	memset(mic_ctx, 0, sizeof(*mic_ctx));
	mic_ctx->bi_id = 0;

	if ((ret = micvnet_init(NULL)))
		goto err_exit_unregister_reboot_notifier;
	if ((ret = micvnet_probe(mic_ctx)))
		goto err_exit_micvnet_exit;
	if ((ret = micvnet_start(mic_ctx)))
		goto err_exit_micvnet_remove;

	return 0;

err_exit_micvnet_remove:
	micvnet_remove(mic_ctx);
err_exit_micvnet_exit:
	micvnet_exit();
err_exit_unregister_reboot_notifier:
	unregister_reboot_notifier(&micvnet_reboot_notifier);
err_exit:
	printk(KERN_ERR "%s failed: error %d\n", __func__, ret);
	return ret;
}

#ifdef STANDALONE_VNET_DMA
module_init(micvnet_module_init);
module_exit(micvnet_module_exit);
#endif

MODULE_LICENSE("GPL");
#endif
