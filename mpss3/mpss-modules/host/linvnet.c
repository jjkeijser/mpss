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

#include "micint.h"
#include "mic_common.h"
#include <mic/micsboxdefine.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/kernel.h>
#include "mic/micveth.h"

#define PWR_MGMT_NO_POLL_AFTER_LINKS_UP 1

/*
  In intr/poll modes, mic_smpt_uninit has already been called before
  micveth_destroy is called during rmmod. This results in host driver crash. The
  current workaround is, given the 'legacy' nature of VNET intr/poll modes, to
  not call mic_ctx_unmap_single() at rmmod. This workaround will result in some
  unmapped memory and a warn_on from micscif_smpt.c.
 */
#define WA_UNMAP_AT_RMMOD 0

static void micveth_clientpoll(struct work_struct *work);
static void micveth_poll(struct work_struct *work);
static int micvnet_host_doorbell_intr_handler(mic_ctx_t *mic_ctx, int doorbell);
static void micvnet_intr_bh_handler(struct work_struct *work);
void micveth_send_intr(micveth_info_t *veth_info);

micveth_t micveth;

void dump_skb(struct sk_buff *skb, int xmit);

static inline
mic_ctx_t *veth_to_ctx(micveth_info_t *veth_info)
{
	return veth_info->mic_ctx;
}

static int
micveth_set_address(struct net_device *dev, void *p)
{
	struct sockaddr *sa = p;

	if (!is_valid_ether_addr(sa->sa_data))
		return -EADDRNOTAVAIL;

	memcpy(dev->dev_addr, sa->sa_data, ETH_ALEN);
	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,2,0)
static void
micveth_multicast_list(struct net_device *dev)
{
}
#endif

static int
micveth_deliver(struct sk_buff *skb, struct net_device *dev, micveth_info_t *veth_info)
{
	veth_ring_t *ring;
	ring_queue_t *tx_queue;
	ring_desc_t *desc;
	ring_packet_t *packet;
	int next_tail;

	//dump_skb(skb, 1);

	spin_lock(&veth_info->vi_txlock);
	ring = &veth_info->vi_ring.ring;
	tx_queue = &ring->r_tx;

	next_tail = (tx_queue->rq_tail + 1) % tx_queue->rq_length;
	if (next_tail == tx_queue->rq_head) {
		// queue_full situation - just drop the packet and let the stack retry
		spin_unlock(&veth_info->vi_txlock);
		return 1;
	}

	desc = &tx_queue->rq_descs[tx_queue->rq_tail];
	packet = &veth_info->vi_tx_desc[tx_queue->rq_tail];
	packet->pd_skb = skb;
	packet->pd_phys = mic_ctx_map_single(veth_to_ctx(veth_info),
					     skb->data, skb->len);
	packet->pd_length = skb->len;
	desc->rd_phys = packet->pd_phys;
	desc->rd_length = skb->len;
	desc->rd_valid = 1;

	/*
	 * Need a write memory barrier between copying the skb data to
	 * the buffer and updating the tail pointer.  NOT an smp_wmb(),
	 * because this memory barrier needs to be done even if there is
	 * a single CPU in the system.
	 */
	wmb();
	tx_queue->rq_tail = (tx_queue->rq_tail + 1) % tx_queue->rq_length;
	spin_unlock(&veth_info->vi_txlock);

	if (mic_vnet_mode == VNET_MODE_INTR) {
		micveth_send_intr(veth_info);
	}

	return 0;
}

static int
micveth_xmit(struct sk_buff *skb, struct net_device *dev)
{
	micveth_info_t *veth_info;

	if (be16_to_cpu(skb->protocol) == ETH_P_IPV6) {
		kfree_skb(skb);
		dev->stats.tx_dropped++;
		return NETDEV_TX_OK;
	}

	dev->stats.tx_packets++;
	dev->stats.tx_bytes += skb->len;

	veth_info = dev->ml_priv;

	if (veth_info->vi_state != VETH_STATE_LINKUP) {
		kfree_skb(skb);
		dev->stats.tx_dropped++;
		return NETDEV_TX_OK;
	}

	if (micveth_deliver(skb, dev, veth_info)) {
		kfree_skb(skb);
		dev->stats.tx_dropped++;
	}

	return NETDEV_TX_OK;
}

static int
micveth_change_mtu(struct net_device *dev, int new_mtu)
{
	dev->mtu = new_mtu;
	return 0;
}

/* Start callback  */
static int
micveth_start_dev(struct net_device *dev)
{
	micveth_info_t *veth_info = dev->ml_priv;

	micveth_start(veth_info->mic_ctx);
	return 0;
}

/* Stop callback */
static int
micveth_stop_dev(struct net_device *dev)
{
	micveth_info_t *veth_info = dev->ml_priv;

	micveth_stop(veth_info->mic_ctx);
	return 0;
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,28)
static const struct net_device_ops veth_netdev_ops = {
	.ndo_open		= micveth_start_dev,
	.ndo_stop		= micveth_stop_dev,
	.ndo_start_xmit		= micveth_xmit,
	.ndo_validate_addr	= eth_validate_addr,
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,2,0)
	.ndo_set_multicast_list = micveth_multicast_list,
#endif
	.ndo_set_mac_address	= micveth_set_address,
	.ndo_change_mtu		= micveth_change_mtu,
};
#endif

static void
micveth_setup(struct net_device *dev)
{
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,28)
	dev->hard_start_xmit = micveth_xmit;
	dev->set_multicast_list = micveth_multicast_list;
	dev->set_mac_address = micveth_set_address;
#endif
	ether_setup(dev);

	/* Initialize the device structure. */
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,28)
	dev->netdev_ops = &veth_netdev_ops;
#endif
	dev->destructor = free_netdev;

	/* Fill in device structure with ethernet-generic values. */
	dev->mtu = (MICVETH_MAX_PACKET_SIZE);
	dev->tx_queue_len = 0;
	dev->flags &= ~IFF_MULTICAST;
	random_ether_addr(dev->dev_addr);
}

static int
micveth_validate(struct nlattr *tb[], struct nlattr *data[])
{
	if (tb[IFLA_ADDRESS]) {
		if (nla_len(tb[IFLA_ADDRESS]) != ETH_ALEN)
			return -EINVAL;
		if (!is_valid_ether_addr(nla_data(tb[IFLA_ADDRESS])))
			return -EADDRNOTAVAIL;
	}
	return 0;
}

static struct rtnl_link_ops micveth_link_ops __read_mostly = {
	.kind		= "micveth",
	.setup		= micveth_setup,
	.validate	= micveth_validate,
};

static int
micveth_probe_int(micveth_info_t *veth_info, mic_ctx_t *mic_ctx)
{
	struct net_device *dev_veth;
	ring_queue_t *queue;
	ring_desc_t *desc;
	ring_packet_t *packet;
	int idx;
	int err = 0;

	veth_info->vi_pdev = mic_ctx->bi_pdev;
	veth_info->vi_sbox = (uint8_t *)((unsigned long)mic_ctx->mmio.va +
					 HOST_SBOX_BASE_ADDRESS);
	veth_info->vi_scratch14 = (uint32_t *)((unsigned long)mic_ctx->mmio.va +
					       HOST_SBOX_BASE_ADDRESS + SBOX_SCRATCH14);
	veth_info->vi_scratch15 = (uint32_t *)((unsigned long)mic_ctx->mmio.va +
					       HOST_SBOX_BASE_ADDRESS + SBOX_SCRATCH15);
	veth_info->mic_ctx = mic_ctx;
	mic_ctx->bi_vethinfo = (void *)veth_info;

	spin_lock_init(&veth_info->vi_txlock);
	spin_lock_init(&veth_info->vi_rxlock);

	if (mic_vnet_mode == VNET_MODE_POLL)
		INIT_DELAYED_WORK(&veth_info->vi_poll, micveth_poll);

	// Set the current sk_buff allocation size
	veth_info->vi_skb_mtu = MICVETH_MAX_PACKET_SIZE + 32;

	// Get the physical memory address for the ring descriptors
	veth_info->vi_ring.phys = mic_ctx_map_single(veth_to_ctx(veth_info), &veth_info->vi_ring.ring,
						     sizeof(veth_ring_t));
	veth_info->vi_ring.length = sizeof(veth_ring_t);

	queue = &veth_info->vi_ring.ring.r_tx;
	queue->rq_head = 0;
	queue->rq_tail = 0;
	queue->rq_length = MICVETH_TRANSFER_FIFO_SIZE;

	veth_info->vi_pend = 0;

	packet = &veth_info->vi_tx_desc[0];
	for (idx = 0; idx < queue->rq_length; idx++) {
		desc = &queue->rq_descs[idx];
		packet[idx].pd_skb = NULL;
		packet[idx].pd_phys = 0;
		packet[idx].pd_length = 0;

		desc->rd_phys = 0;
		desc->rd_length = 0;
		desc->rd_valid = 0;
	}

	// This is the recieve end.
	queue = &veth_info->vi_ring.ring.r_rx;
	queue->rq_head = 0;
	queue->rq_tail = 0;
	queue->rq_length = MICVETH_TRANSFER_FIFO_SIZE;

	packet = &veth_info->vi_rx_desc[0];
	for (idx = 0; idx < queue->rq_length; idx++) {
		desc = &queue->rq_descs[idx];
		if (!(packet[idx].pd_skb = dev_alloc_skb(veth_info->vi_skb_mtu)))
			return -ENOMEM;
		packet[idx].pd_phys = mic_ctx_map_single(veth_to_ctx(veth_info), packet[idx].pd_skb->data,
							 veth_info->vi_skb_mtu);
		packet[idx].pd_length = veth_info->vi_skb_mtu;

		desc->rd_phys = packet[idx].pd_phys;
		desc->rd_length = packet[idx].pd_length;
		desc->rd_valid = 1;
	}
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 17, 0)
	if ((dev_veth = alloc_netdev(sizeof(micveth_info_t), "mic%d", micveth_setup)) == NULL) {
#else
	if ((dev_veth = alloc_netdev(sizeof(micveth_info_t), "mic%d", NET_NAME_UNKNOWN, micveth_setup)) == NULL) {
#endif
		return -ENOMEM;
	}

	veth_info->vi_netdev = dev_veth;
	dev_veth->ml_priv = veth_info;
	dev_veth->rtnl_link_ops = &micveth_link_ops;

	if ((err = register_netdev(dev_veth)) < 0) {
		printk("register netdev failed %d\n", err);
		free_netdev(dev_veth);
		return err;
	}

	veth_info->vi_state = VETH_STATE_INITIALIZED;
	return 0;
}

static ssize_t show_veth(struct device *dev,
			 struct device_attribute *attr, char *buf);
DEVICE_ATTR(veth, S_IRUGO, show_veth, NULL);

static int
micveth_init_int(int num_bds, struct device *dev)
{
	int bd;
	int err = 0;

	micveth.lv_num_interfaces = num_bds;
	micveth.lv_num_clients = num_bds;
	micveth.lv_active_clients = 0;
	micveth.lv_num_links_remaining = num_bds;

	BUG_ON(rtnl_link_register(&micveth_link_ops));

	// Allocate space for the control of each device in the system.
	micveth.lv_info = kmalloc(sizeof(micveth_info_t) * num_bds, GFP_KERNEL);

	// Initialize state mutex.  Overloaded use for several fields.
	mutex_init(&micveth.lv_state_mutex);

	// Setup of timer for probeing active mic clients.  When the total active board
	// count is zero the poll is not running.
	micveth.lv_pollstate = CLIENT_POLL_STOPPED;
	INIT_DELAYED_WORK(&micveth.lv_poll, micveth_clientpoll);
	init_waitqueue_head(&micveth.lv_wq);

	// Init each of the existing boards.
	for (bd = 0; bd < num_bds; bd++) {
		micveth_probe_int(&micveth.lv_info[bd], &mic_data.dd_bi[bd]->bi_ctx);
	}

	err = device_create_file(dev, &dev_attr_veth);
	return err;
}

static void
micveth_exit_int(void)
{
	mic_ctx_t *mic_ctx = kmalloc(sizeof(mic_ctx_t), GFP_KERNEL);
	micveth_info_t *veth_info;
	ring_packet_t *packet;
	int bd;
	int idx;

	rtnl_link_unregister(&micveth_link_ops);

	for (bd = 0; bd < micveth.lv_num_clients; bd++) {
		veth_info = &micveth.lv_info[bd];

		/* veth_info->mic_ctx == mic_data.dd_bi[bd] is freed in
		   remove so cannot be used in exit */
		mic_ctx->bi_vethinfo = veth_info;
		micveth_stop(mic_ctx);

#if WA_UNMAP_AT_RMMOD
		mic_ctx_unmap_single(veth_to_ctx(veth_info), veth_info->vi_ring.phys,
				     sizeof(veth_ring_t));
#endif

		for (idx = 0; idx < veth_info->vi_ring.ring.r_tx.rq_length; idx++) {
			packet = &veth_info->vi_tx_desc[idx];
			if (packet->pd_skb != NULL) {
#if WA_UNMAP_AT_RMMOD
				mic_ctx_unmap_single(veth_to_ctx(veth_info), packet->pd_phys,
						     packet->pd_skb->len);
#endif
				kfree_skb(packet->pd_skb);
			}
		}

		for (idx = 0; idx < veth_info->vi_ring.ring.r_rx.rq_length; idx++) {
			packet = &veth_info->vi_rx_desc[idx];
#if WA_UNMAP_AT_RMMOD
			mic_ctx_unmap_single(veth_to_ctx(veth_info), packet->pd_phys, packet->pd_skb->len);
#endif
			kfree_skb(packet->pd_skb);
		}
	}

	kfree(mic_ctx);
	kfree(micveth.lv_info);
}

static int
micveth_start_int(mic_ctx_t *mic_ctx)
{
	micveth_info_t *veth_info = &micveth.lv_info[mic_ctx->bi_id];

	// Eventuall (very soon) most of the descriptor allocation for a board will be done here
	if (veth_info->vi_state != VETH_STATE_INITIALIZED)
		return 0;

	mutex_lock(&micveth.lv_state_mutex);

	if (micveth.lv_pollstate == CLIENT_POLL_STOPPED) {
		schedule_delayed_work(&micveth.lv_poll, msecs_to_jiffies(MICVETH_CLIENT_TIMER_DELAY));
		micveth.lv_pollstate = CLIENT_POLL_RUNNING;
	}

	micveth.lv_active_clients++;
	mutex_unlock(&micveth.lv_state_mutex);

	veth_info->vi_pend = 0;

	veth_info->vi_ring.ring.r_tx.rq_head = 0;
	veth_info->vi_ring.ring.r_tx.rq_tail = 0;

	veth_info->vi_ring.ring.r_rx.rq_head = 0;
	veth_info->vi_ring.ring.r_rx.rq_tail = 0;
	veth_info->vi_state = VETH_STATE_LINKDOWN;

	if (mic_vnet_mode == VNET_MODE_INTR) {
		snprintf(veth_info->vi_wqname, sizeof(veth_info->vi_wqname),
			 "VNET INTR %d\n", mic_ctx->bi_id);
		veth_info->vi_wq = create_singlethread_workqueue(veth_info->vi_wqname);
		INIT_WORK(&veth_info->vi_bh, micvnet_intr_bh_handler);

		// Install interrupt handler on doorbell 3
		mic_reg_irqhandler(mic_ctx, 3, "Host DoorBell 3",
				   micvnet_host_doorbell_intr_handler);
	}

	return 0;
}

static void
micveth_stop_int(mic_ctx_t *mic_ctx)
{
	micveth_info_t *veth_info = (micveth_info_t *)(mic_ctx->bi_vethinfo);

	if (veth_info->vi_state == VETH_STATE_INITIALIZED)
		return;

	mutex_lock(&micveth.lv_state_mutex);

	if (mic_vnet_mode == VNET_MODE_INTR) {
		// Remove interrupt handler on doorbell 3
		mic_unreg_irqhandler(mic_ctx, 3, "Host DoorBell 3");

		destroy_workqueue(veth_info->vi_wq);
	}

	micveth.lv_active_clients--;
	veth_info->vi_state = VETH_STATE_INITIALIZED;

	if (micveth.lv_active_clients) {
		mutex_unlock(&micveth.lv_state_mutex);
		return;
	}

	micveth.lv_num_links_remaining = micveth.lv_num_clients;

#if PWR_MGMT_NO_POLL_AFTER_LINKS_UP
	micveth.lv_pollstate = CLIENT_POLL_STOPPED;
	mutex_unlock(&micveth.lv_state_mutex);
#else
	micveth.lv_pollstate = CLIENT_POLL_STOPPING;
	mutex_unlock(&micveth.lv_state_mutex);
	wait_event(micveth.lv_wq, micveth.lv_pollstate == CLIENT_POLL_STOPPED);
#endif
}

#define NO_SRATCHREGREAD_AFTER_CONNECT  1
static void
micveth_clientpoll(struct work_struct *work)
{
	micveth_info_t *veth_info;
	uint32_t transRingHi;
	uint32_t transRingLo;
	uint32_t scratch14 = 0;
	uint32_t scratch15 = 0;
	int bd;
	static int enter = 0;

	if (enter == 0)
	{
		printk("micveth is polling\n");
		enter = 1;
	}

	mutex_lock(&micveth.lv_state_mutex);
	if (micveth.lv_pollstate == CLIENT_POLL_STOPPING) {
		micveth.lv_pollstate = CLIENT_POLL_STOPPED;
		mutex_unlock(&micveth.lv_state_mutex);
		wake_up(&micveth.lv_wq);
		return;
	}

	// Check for state changes for each board in the system
	for (bd = 0; bd < micveth.lv_num_clients; bd++) {
		veth_info = &micveth.lv_info[bd];

		// Do not poll boards that have not had the interface started.
		if (veth_info->vi_state == VETH_STATE_INITIALIZED) {
			break;
		}

#ifdef NO_SRATCHREGREAD_AFTER_CONNECT
		if(veth_info->vi_state != VETH_STATE_LINKUP) {
#endif
		scratch14 = readl(veth_info->vi_scratch14);
		scratch15 = readl(veth_info->vi_scratch15);
#ifdef NO_SRATCHREGREAD_AFTER_CONNECT
		}
#endif

		if (veth_info->vi_state == VETH_STATE_LINKUP) {
			if (scratch14 == MICVETH_LINK_DOWN_MAGIC) {
				veth_info->vi_state = VETH_STATE_LINKDOWN;
			}
		} else if (veth_info->vi_state == VETH_STATE_LINKDOWN) {
			if (scratch14 == MICVETH_LINK_UP_MAGIC) {
				// Write the transfer ring address.
				transRingHi = (uint32_t)(veth_info->vi_ring.phys >> 32);
				transRingLo = (uint32_t)(veth_info->vi_ring.phys & 0xffffffff);

				writel(transRingLo, veth_info->vi_scratch14);
				writel(transRingHi, veth_info->vi_scratch15);

				veth_info->vi_state = VETH_STATE_LINKUP;
				printk("MIC virtual ethernet up for board %d\n", bd);
#ifdef MIC_IS_EMULATION
				printk("Card wrote Magic: It must be UP!\n");
#endif

				if (mic_vnet_mode == VNET_MODE_POLL) {
					schedule_delayed_work(&veth_info->vi_poll,
						      msecs_to_jiffies(MICVETH_POLL_TIMER_DELAY));
				}

				micveth.lv_num_links_remaining--;
			}
#ifdef MIC_IS_EMULATION
			else if (scratch14) {
				printk("---> 0x%x \n", scratch14);
				writel(0x0, veth_info->vi_scratch14);
			}
#endif
		}
	}

	mutex_unlock(&micveth.lv_state_mutex);

#if PWR_MGMT_NO_POLL_AFTER_LINKS_UP
	if (micveth.lv_num_links_remaining)
#endif
		schedule_delayed_work(&micveth.lv_poll, msecs_to_jiffies(MICVETH_CLIENT_TIMER_DELAY));
}

static int
micvnet_host_doorbell_intr_handler(mic_ctx_t *mic_ctx, int doorbell)
{
	micveth_info_t *veth_info;
	veth_info = &micveth.lv_info[mic_ctx->bi_id];
	queue_work(veth_info->vi_wq, &veth_info->vi_bh);
	return 0;
}

void
micveth_send_intr(micveth_info_t *veth_info)
{
	mic_ctx_t *mic_ctx = veth_info->mic_ctx;
	mic_send_vnet_intr(mic_ctx);
}

void
_micveth_process_descriptors(micveth_info_t *veth_info)
{
	veth_ring_t *ring = &veth_info->vi_ring.ring;
	ring_queue_t *rx_queue = &ring->r_rx;
	ring_queue_t *tx_queue = &ring->r_tx;
	ring_desc_t *desc;
	ring_packet_t *packet;
	struct sk_buff *skb;
	int receive_skb = 0;
	int err;

	if (veth_info->vi_state != VETH_STATE_LINKUP) {
		return;
	}

	spin_lock_bh(&veth_info->vi_rxlock);

	while (rx_queue->rq_head != rx_queue->rq_tail) {
		desc = &rx_queue->rq_descs[rx_queue->rq_head];

		veth_info->vi_netdev->stats.rx_packets++;
		veth_info->vi_netdev->stats.rx_bytes += desc->rd_length;

		packet = &veth_info->vi_rx_desc[rx_queue->rq_head];

		skb = packet->pd_skb;
		skb_put(skb, desc->rd_length);

		//dump_skb(skb, 0);
		mic_ctx_unmap_single(veth_to_ctx(veth_info), packet->pd_phys, veth_info->vi_skb_mtu);
		packet->pd_skb = dev_alloc_skb(veth_info->vi_skb_mtu);
		packet->pd_phys = mic_ctx_map_single(veth_to_ctx(veth_info), packet->pd_skb->data,
						     veth_info->vi_skb_mtu);
		desc->rd_phys = packet->pd_phys;
		desc->rd_length = packet->pd_length;

		skb->dev = veth_info->vi_netdev;
		skb->protocol = eth_type_trans(skb, skb->dev);
		skb->ip_summed = CHECKSUM_NONE;

		err = netif_receive_skb(skb);
		/*
		 * Need a general memory barrier between copying the data from
		 * the buffer and updating the head pointer. It's the general
		 * mb() because we're ordering the read of the data with the write.
		 */
		mb();
		rx_queue->rq_head = (rx_queue->rq_head + 1) % rx_queue->rq_length;
		receive_skb++;
	}

	/* Send intr to TX so that pending SKB's can be freed */
	if (receive_skb && mic_vnet_mode == VNET_MODE_INTR) {
		micveth_send_intr(veth_info);
	}

	spin_unlock_bh(&veth_info->vi_rxlock);

	spin_lock_bh(&veth_info->vi_txlock);

	// Also handle completed tx requests
	while (veth_info->vi_pend != tx_queue->rq_head) {
		desc = &tx_queue->rq_descs[veth_info->vi_pend];
		packet = &veth_info->vi_tx_desc[veth_info->vi_pend];

		skb = packet->pd_skb;
		packet->pd_skb = NULL;

		mic_ctx_unmap_single(veth_to_ctx(veth_info), packet->pd_phys, skb->len);
		packet->pd_phys = 0;

		kfree_skb(skb);

		veth_info->vi_pend = (veth_info->vi_pend + 1) % tx_queue->rq_length;
	}

	spin_unlock_bh(&veth_info->vi_txlock);

	if (mic_vnet_mode == VNET_MODE_POLL) {
		schedule_delayed_work(&veth_info->vi_poll, msecs_to_jiffies(MICVETH_POLL_TIMER_DELAY));
	}
}

static void
micvnet_intr_bh_handler(struct work_struct *work)
{
	micveth_info_t *veth_info = container_of(work, micveth_info_t, vi_bh);
	_micveth_process_descriptors(veth_info);
}

static void
micveth_poll(struct work_struct *work)
{
	micveth_info_t *veth_info = container_of(work, micveth_info_t, vi_poll.work);

	_micveth_process_descriptors(veth_info);
}

static ssize_t
show_veth(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n",
			micveth.lv_pollstate == CLIENT_POLL_RUNNING ?
			"running" : "stopped");
}

/*
  VNET driver public API. These are simply wrappers which either invoke the old
  interrupt/poll mode functions or the new DMA mode functions. These are temporary and
  will be phased out with the old interrupt/poll mode so only the DMA mode will be around
  eventually.
 */
int __init
micveth_init(struct device *dev)
{
	printk("vnet: mode: %s, buffers: %d\n", 
		mic_vnet_modes[mic_vnet_mode], vnet_num_buffers);

	if (mic_vnet_mode == VNET_MODE_DMA)
		return micvnet_init(dev);
	/* Intr/poll modes use micveth_init_legacy */
	return 0;
}

int __init
micveth_init_legacy(int num_bds, struct device *dev)
{
	if (mic_vnet_mode != VNET_MODE_DMA)
		return micveth_init_int(num_bds, dev);
	/* DMA mode uses micveth_init */
	return 0;
}

void
micveth_exit(void)
{
	if (mic_vnet_mode == VNET_MODE_DMA)
		micvnet_exit();
	else
		micveth_exit_int();
}

int
micveth_probe(mic_ctx_t *mic_ctx)
{
	if (mic_vnet_mode == VNET_MODE_DMA)
		return micvnet_probe(mic_ctx);
	/* No support for micveth_probe in legacy intr/poll modes */
	return 0;
}

void
micveth_remove(mic_ctx_t *mic_ctx)
{
	if (mic_vnet_mode == VNET_MODE_DMA)
		micvnet_remove(mic_ctx);
	/* No support for micveth_remove in legacy intr/poll modes */
}

int
micveth_start(mic_ctx_t *mic_ctx)
{
	micveth_info_t *veth_info = mic_ctx->bi_vethinfo;
	int err;

	if (mic_vnet_mode == VNET_MODE_DMA)
		err = micvnet_start(mic_ctx);
	else
		err = micveth_start_int(mic_ctx);

	if (!err)
		netif_carrier_on(veth_info->vi_netdev);

	return err;
}

void
micveth_stop(mic_ctx_t *mic_ctx)
{
	micveth_info_t *veth_info = mic_ctx->bi_vethinfo;

	if (mic_vnet_mode == VNET_MODE_DMA)
		micvnet_stop(mic_ctx);
	else
		micveth_stop_int(mic_ctx);

	if (veth_info)
		netif_carrier_off(veth_info->vi_netdev);
}
