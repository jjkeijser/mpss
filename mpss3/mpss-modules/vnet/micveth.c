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

#include "mic/micveth.h"

#define PWR_MGMT_NO_POLL_AFTER_LINKS_UP 1

/* #define HOST */
#define SBOX_MMIO_LENGTH (64 * 1024)

/* Host - Card link initialization rotocol
 * Card comes up and writes MICVETH_LINK_UP_MAGIC to scratch 14 & 15
 * Host detects that the card side interface is up and writes the
 * 1) address of the tx/rx descriptor ring buffer to scratch 14 & 15
 * 2) last 2 octets of the MAC address (allows the host to identify
 * the board number based on its mac address)
 */

/* Host - Card descriptor queue/ring buffer (from the perspective of the host)
 *
 * There is a transmit and a receive queue. Each queue entry has
 * a physical address and a length.
 *
 * Packet transmission
 * The host adds a queue entry with the physical address of the skb and its
 * length and updates the write pointer. The receive side on the card sees the
 * new entry, allocates a new skb, maps the host's skb, copies it to a locally
 * allocated skb and updates the read pointer. The host side later frees up skbs
 * starting from a cached read pointer upto the read pointer
 *
 * Packet reception
 * The host "posts" skbs to the rx queue. The transmit routine on the card
 * copies its local skb to the host skb, updates the write pointer and frees
 * its local skb
 */

/* Vnet interrupts are now functional (with vnet=dma module parameter). In the
   main flow of the driver all polling in the interrupt mode has been
   eliminated. However, polling is still happening in clientpoll() routine which
   tracks if the link is up or down. This can also be replaced by an interrupt
   driven mechanism which will be done in the future. Apart from this, only
   limited testing has been done in the interrupt mode, especially with respect
   to sharing the interrupt with scif. Therefore, for now the default mode of
   operation is still left as poll in micstart.
*/

#define SBOX_SDBIC0_DBREQ_BIT   0x80000000


#ifdef HOST
#else
struct skb_node {
	struct list_head list;
	struct sk_buff *skb;
};

/* List of skbs to be transmitted - global for now assumes KN* has a single interface */
struct list_head skb_list;
LIST_HEAD(skb_list);
#endif

static void _micveth_process_descriptors(micveth_info_t *veth_info);

#ifdef HOST
#else
static int micveth_xmit_enqueue(struct sk_buff *skb, struct net_device *dev, micveth_info_t *veth_info);
static int micveth_xmit_dequeue(struct net_device *dev, micveth_info_t *veth_info);
static struct sk_buff *dequeue_skb(micveth_info_t *veth_info);
static void micvnet_tx_dequeue_handler(struct work_struct *work);

int micveth_start(mic_ctx_t *mic_ctx);
void micveth_stop(mic_ctx_t *mic_ctx);
static int micveth_start_dev(struct net_device *dev);
static int micveth_stop_dev(struct net_device *dev);
#endif

static void micveth_clientpoll(struct work_struct *work);
static void micveth_poll(struct work_struct *work);
static irqreturn_t micvnet_host_intr_handler(int irq, void *cookie);
static void micvnet_intr_bh_handler(struct work_struct *work);
static void micveth_send_intr(micveth_info_t *veth_info);
int get_sbox_irq(int index);

#ifdef HOST
#else
static mic_ctx_t mic_ctx_g;
#endif

micveth_t micveth;

static int
micveth_set_address(struct net_device *dev, void *p)
{
	struct sockaddr *sa = p;

	if (!is_valid_ether_addr(sa->sa_data))
		return -EADDRNOTAVAIL;

	memcpy(dev->dev_addr, sa->sa_data, ETH_ALEN);
	return 0;
}

static void
micveth_multicast_list(struct net_device *dev)
{
}

#ifdef HOST
#else
/* Enqueues an skb for transmission. This is necessary because micveth_xmit is called in
   interrupt context and we cannot call ioremap_nocache from interrupt context. */
static int
micveth_xmit_enqueue(struct sk_buff *skb, struct net_device *dev, micveth_info_t *veth_info)
{
	struct skb_node *new_node = kmalloc(sizeof(*new_node), GFP_ATOMIC);

	if (!new_node)
		return ENOMEM;
	new_node->skb = skb;
	spin_lock(&veth_info->vi_txlock);
	list_add_tail(&new_node->list, &skb_list);
	spin_unlock(&veth_info->vi_txlock);
	return 0;
}

/* Dequeues a skb enqueued by micveth_xmit_enqueue */
static struct sk_buff *
dequeue_skb(micveth_info_t *veth_info)
{
	struct sk_buff *skb = NULL;
	struct skb_node *skb_node = NULL;

	spin_lock_bh(&veth_info->vi_txlock);
	if (!list_empty(&skb_list))
	{
		skb_node = list_entry(skb_list.next, struct skb_node , list);
		list_del(&skb_node->list);
		skb = skb_node->skb;
	}
	spin_unlock_bh(&veth_info->vi_txlock);

	if (skb_node)
		kfree(skb_node);
	return skb;
}

/* Transmits skbs that have been enqueued by the by micveth_xmit_enqueue */
static int
micveth_xmit_dequeue(struct net_device *dev, micveth_info_t *veth_info)
{
	veth_ring_t *ring;
	ring_queue_t *tx_queue;
	ring_desc_t *desc;
	int next_tail;
	void *dst;
	struct sk_buff *skb;

	while ((skb = dequeue_skb(veth_info))) {
		ring = veth_info->ring_ptr;
		tx_queue = &ring->r_rx;

		next_tail = (tx_queue->rq_tail + 1) % tx_queue->rq_length;
		if (next_tail == tx_queue->rq_head) {
			printk(KERN_WARNING "dropping packet\n");
			/* queue_full situation - just drop the packet and let the stack retry */
			return 1;
		}

		desc = &tx_queue->rq_descs[tx_queue->rq_tail];
		dst = ioremap_nocache(desc->rd_phys, skb->len);
		if (!dst) {
			tx_queue->rq_tail = (tx_queue->rq_tail + 1) % tx_queue->rq_length;
			dev_kfree_skb(skb);
			dev->stats.tx_dropped++;
			continue;
		}
		desc->rd_length = skb->len;
		desc->rd_valid = 1;
		memcpy(dst, skb->data, skb->len);
		/*
		 * Need a write memory barrier between copying the skb data to
		 * the buffer and updating the tail pointer.  NOT an smp_wmb(),
		 * because this memory barrier needs to be done even if there is
		 * a single CPU in the system.
		 *
		 * No need for the serializing request (Si bug workaround in
		 * KNF), since the buffer exists in host memory.  If the buffer
		 * lives in card memory, and this code is running on the host,  we
		 * would need extra barriers and a "serializing request" on any write.
		 */
		wmb();
		tx_queue->rq_tail = (tx_queue->rq_tail + 1) % tx_queue->rq_length;
		iounmap(dst);
		dev_kfree_skb(skb);

		if (mic_vnet_mode == VNET_MODE_INTR) {
			micveth_send_intr(veth_info);
		}
	}

	return 0;
}

static void
micvnet_tx_dequeue_handler(struct work_struct *work)
{
	micveth_info_t *veth_info = container_of(work, micveth_info_t, vi_txws);
	struct net_device *dev_veth = veth_info->vi_netdev;

	micveth_xmit_dequeue(dev_veth, veth_info);
}
#endif

#ifdef HOST
#else  // card
/* Transmit callback */
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

	veth_info = &micveth.lv_info[0];
	if (veth_info->vi_state == VETH_STATE_LINKUP) {
		if (micveth_xmit_enqueue(skb, dev, veth_info)) {
			dev_kfree_skb(skb);
			dev->stats.tx_dropped++;
		}
	} else {
		dev_kfree_skb(skb);
	}

	/* Reuse the interrupt workqueue to also queue tx dequeue tasks */
	queue_work(veth_info->vi_wq, &veth_info->vi_txws);

	return NETDEV_TX_OK;
}
#endif

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
	.ndo_set_multicast_list = micveth_multicast_list,
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
	int err = 0;

	veth_info->vi_sbox = ioremap_nocache(SBOX_BASE, SBOX_MMIO_LENGTH);
	veth_info->vi_scratch14 = (uint32_t *)(veth_info->vi_sbox + SBOX_SCRATCH14);
	veth_info->vi_scratch15 = (uint32_t *)(veth_info->vi_sbox + SBOX_SCRATCH14);
	writel(0x55, veth_info->vi_sbox + SBOX_DCR);

	veth_info->mic_ctx = mic_ctx;
	mic_ctx->bi_vethinfo = (void *)veth_info;

	spin_lock_init(&veth_info->vi_txlock);
	spin_lock_init(&veth_info->vi_rxlock);

	if (mic_vnet_mode == VNET_MODE_POLL)
		INIT_DELAYED_WORK(&veth_info->vi_poll, micveth_poll);

	snprintf(veth_info->vi_wqname, sizeof(veth_info->vi_wqname),
		 "VNET INTR %d", 0);
	veth_info->vi_wq = create_singlethread_workqueue(veth_info->vi_wqname);
	INIT_WORK(&veth_info->vi_txws, micvnet_tx_dequeue_handler);

	if (mic_vnet_mode == VNET_MODE_INTR) {
		if ((err = request_irq(get_sbox_irq(VNET_SBOX_INT_IDX),
				micvnet_host_intr_handler, IRQF_DISABLED,
				"micveth intr", veth_info))) {
			printk(KERN_ERR "%s: interrupt registration failed\n", __func__);
			return err;
		}
		INIT_WORK(&veth_info->vi_bh, micvnet_intr_bh_handler);
	}

	// Set the current sk_buff allocation size
	veth_info->vi_skb_mtu = MICVETH_MAX_PACKET_SIZE + 32;

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

	/* Inform host after completing initialization */
	printk("%s: writing magic to SC14 and SC15\n", __FUNCTION__);
	writel(MICVETH_LINK_UP_MAGIC, veth_info->vi_sbox + SBOX_SCRATCH14);
	writel(MICVETH_LINK_UP_MAGIC, veth_info->vi_sbox + SBOX_SCRATCH15);

	return 0;
}

void
micveth_remove_int(mic_ctx_t *mic_ctx)
{
	micveth_stop(mic_ctx);
}

static int __init
micveth_create_int(int num_bds, struct device *dev)
{
	int bd;
	int err = 0;

	printk("micveth_init(%d)\n", num_bds);

	micveth.lv_num_interfaces = num_bds;
	micveth.lv_num_clients = num_bds;
	micveth.lv_active_clients = 0;
	micveth.lv_num_links_remaining = num_bds;

	if ((err = rtnl_link_register(&micveth_link_ops))) {
		printk(KERN_ERR "%s: rtnl_link_register failed!\n", __func__);
		return err;
	}

	// Allocate space for the control of each device in the system.
	micveth.lv_info = kmalloc(sizeof(micveth_info_t) * num_bds, GFP_KERNEL);
	if (!micveth.lv_info) {
		printk(KERN_ERR "%s: micveth_info alloc failed!\n", __func__);
		return -ENOMEM;
	}

	// Initialize state mutex.  Overloaded use for several fields.
	mutex_init(&micveth.lv_state_mutex);

	// Setup of timer for probeing active mic clients.  When the total active board
	// count is zero the poll is not running.
	micveth.lv_pollstate = CLIENT_POLL_STOPPED;
	INIT_DELAYED_WORK(&micveth.lv_poll, micveth_clientpoll);
	init_waitqueue_head(&micveth.lv_wq);

	// Init each of the existing boards.
	for (bd = 0; bd < num_bds; bd++) {
#ifdef HOST
		micveth_probe_int(&micveth.lv_info[bd], &mic_data.dd_bi[bd]->bi_ctx);
#else
		micveth_probe_int(&micveth.lv_info[bd], &mic_ctx_g);
#endif
	}

	return err;
}

static void
micveth_exit_int(void)
{
	micveth_info_t *veth_info = &micveth.lv_info[0];
#ifdef HOST
#endif
	micveth_stop(veth_info->mic_ctx);

	destroy_workqueue(veth_info->vi_wq);
	rtnl_link_unregister(&micveth_link_ops);

#ifdef HOST
#else  // card
	iounmap((void *)veth_info->ring_ptr);
	iounmap(veth_info->vi_sbox);
#endif

	kfree(micveth.lv_info);
}

/* Card side - tell the host that the interface is up */
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

	veth_info->vi_state = VETH_STATE_LINKDOWN;

	return 0;
}

/* Card side - tell the host that the interface is down */
static void
micveth_stop_int(mic_ctx_t *mic_ctx)
{
	micveth_info_t *veth_info = (micveth_info_t *)(mic_ctx->bi_vethinfo);

	if (veth_info->vi_state == VETH_STATE_INITIALIZED)
		return;

	mutex_lock(&micveth.lv_state_mutex);
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

#ifdef HOST
#else  // card
	writel(MICVETH_LINK_DOWN_MAGIC, veth_info->vi_sbox + SBOX_SCRATCH14);
	writel(MICVETH_LINK_DOWN_MAGIC, veth_info->vi_sbox + SBOX_SCRATCH15);
#endif
}

#ifdef HOST
#else  // card
/* Link detection */
static void
micveth_clientpoll(struct work_struct *work)
{
	micveth_info_t *veth_info;
	mic_ctx_t *mic_ctx;
	uint32_t scratch14;
	uint32_t scratch15;
	struct net_device *dev_veth;
	veth_info = &micveth.lv_info[0];
	dev_veth = veth_info->vi_netdev;
	mic_ctx = veth_info->mic_ctx;
	mutex_lock(&micveth.lv_state_mutex);

	if (micveth.lv_pollstate == CLIENT_POLL_STOPPING) {
		micveth.lv_pollstate = CLIENT_POLL_STOPPED;
		mutex_unlock(&micveth.lv_state_mutex);
		wake_up(&micveth.lv_wq);
		return;
	}

	if (veth_info->vi_state == VETH_STATE_LINKUP) {
		scratch14 = readl(veth_info->vi_sbox + SBOX_SCRATCH14);
		scratch15 = readl(veth_info->vi_sbox + SBOX_SCRATCH15);

		if ((MICVETH_LINK_DOWN_MAGIC == scratch14) &&
				(MICVETH_LINK_DOWN_MAGIC == scratch15)) {
			veth_info->vi_state = VETH_STATE_LINKDOWN;
		}
	} else {
		scratch14 = readl(veth_info->vi_sbox + SBOX_SCRATCH14);
		scratch15 = readl(veth_info->vi_sbox + SBOX_SCRATCH15);

		if ((MICVETH_LINK_UP_MAGIC != scratch14) &&
		    (MICVETH_LINK_UP_MAGIC != scratch15)) {
			printk("micveth_clientpoll(): SC14 and SC15 changed from MAGIC, I got the RB addresses!\n");
			writel(MICVETH_LINK_UP_MAGIC, veth_info->vi_sbox + SBOX_SCRATCH14);
			writel(MICVETH_LINK_UP_MAGIC, veth_info->vi_sbox + SBOX_SCRATCH15);
			dev_veth->dev_addr[4] = (scratch15 >> 24) & 0xff;
			dev_veth->dev_addr[5] = (scratch15 >> 16) & 0xff;
			veth_info->vi_ring.phys = ((uint64_t)(scratch15 & 0xffff) << 32) | scratch14;
			veth_info->vi_ring.phys |= (1ULL << 39);
			veth_info->vi_ring.length = sizeof(veth_ring_t);
			veth_info->ring_ptr = ioremap_nocache(veth_info->vi_ring.phys, veth_info->vi_ring.length);
			BUG_ON(veth_info->ring_ptr == NULL);

			printk("micveth_clientpoll(): VETH_STATE_LINKUP\n");
			veth_info->vi_state = VETH_STATE_LINKUP;
			if (mic_vnet_mode == VNET_MODE_POLL) {
				printk("micveth_clientpoll(): poll for work now !!\n");
				schedule_delayed_work(&veth_info->vi_poll, msecs_to_jiffies(MICVETH_POLL_TIMER_DELAY));
			}

			micveth.lv_num_links_remaining--;
		}
	}
	mutex_unlock(&micveth.lv_state_mutex);

#if PWR_MGMT_NO_POLL_AFTER_LINKS_UP
	if (micveth.lv_num_links_remaining)
#endif
		schedule_delayed_work(&micveth.lv_poll, msecs_to_jiffies(MICVETH_CLIENT_TIMER_DELAY));
}
#endif
extern struct sk_buff *jsp_dbg1;

#ifdef HOST
#else  // card
static irqreturn_t
micvnet_host_intr_handler(int irq, void *cookie)
{
	micveth_info_t *veth_info = cookie;
	queue_work(veth_info->vi_wq, &veth_info->vi_bh);
	return IRQ_HANDLED;
}

/* Ring host doorbell 3 interrupt */
static void
micveth_send_intr(micveth_info_t *veth_info)
{
	uint32_t db_reg;

	// Ring host doorbell 3 interrupt
	db_reg = readl(veth_info->vi_sbox + SBOX_SDBIC3) | SBOX_SDBIC0_DBREQ_BIT;
	writel(db_reg, veth_info->vi_sbox + SBOX_SDBIC3);
}

static void
_micveth_process_descriptors(micveth_info_t *veth_info)
{
	veth_ring_t *ring = veth_info->ring_ptr;
	ring_queue_t *rx_queue = &ring->r_tx;
	ring_desc_t desc;
	struct sk_buff *skb;
	void *pkt;
	int receive_skb = 0;
	int err;

	if (veth_info->vi_state != VETH_STATE_LINKUP) {
		return;
	}

	spin_lock(&veth_info->vi_rxlock);

	while (rx_queue->rq_head != rx_queue->rq_tail) {
		desc = rx_queue->rq_descs[rx_queue->rq_head];

		veth_info->vi_netdev->stats.rx_packets++;
		veth_info->vi_netdev->stats.rx_bytes += desc.rd_length;

		pkt = ioremap_nocache(desc.rd_phys, desc.rd_length);
		if (pkt == NULL) {
			veth_info->vi_netdev->stats.rx_dropped++;
			goto update_ring;
		}

		/* handle jumbo frame */
		if (desc.rd_length > ETH_DATA_LEN)
			skb = dev_alloc_skb(veth_info->vi_skb_mtu);
		else
			skb = dev_alloc_skb(ETH_DATA_LEN + 32);
		if (skb == NULL) {
			veth_info->vi_netdev->stats.rx_dropped++;
			iounmap(pkt);
			goto update_ring;
		}

		memcpy(skb_put(skb,desc.rd_length), pkt, desc.rd_length);
		iounmap(pkt);
		skb->dev = veth_info->vi_netdev;
		skb->protocol = eth_type_trans(skb, skb->dev);
		skb->ip_summed = CHECKSUM_NONE;
		local_bh_disable();
		err = netif_receive_skb(skb);
		err = err;
		local_bh_enable();
		/*
		 * Need a general memory barrier between copying the data from
		 * the buffer and updating the head pointer. It's the general
		 * mb() because we're ordering the read of the data with the write.
		 *
		 * No need for the serializing request (Si bug workaround in
		 * KNF), since the buffer exists in host memory.  If the buffer
		 * lives in card memory, and this code is running on the host,  we
		 * would need extra barriers and a "serializing request" on any write.
		 */
		mb();
update_ring:
		rx_queue->rq_head = (rx_queue->rq_head + 1) % rx_queue->rq_length;
		receive_skb++;
	}

	/* Send intr to TX so that pending SKB's can be freed */
	if (receive_skb && mic_vnet_mode == VNET_MODE_INTR) {
		micveth_send_intr(veth_info);
	}

	spin_unlock(&veth_info->vi_rxlock);

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

#endif

#ifdef HOST
#else  // card
static int __init
micveth_module_init_int(void)
{
	mic_ctx_t *mic_ctx = &mic_ctx_g;
	int ret = 0;

	printk("micveth_probe()\n");
	memset(mic_ctx, 0, sizeof(*mic_ctx));
	mic_ctx->bi_id = 0;

	if ((ret = micveth_init(NULL)))
		return ret;
	if ((ret = micveth_init_legacy(1, NULL)))
		return ret;

	return 0;
}

static void __exit
micveth_module_exit_int(void)
{
	micveth_exit();
}
#endif

/*
  VNET driver public API. These are simply wrappers which either invoke the old
  interrupt/poll mode functions or the new DMA mode functions. These are temporary and
  will be phased out with the old interrupt/poll mode so only the DMA mode will be around
  eventually.
 */
int __init
micveth_init(struct device *dev)
{
	if (mic_vnet_mode == VNET_MODE_DMA)
		return micvnet_init(dev);
	/* Intr/poll modes use micveth_init_legacy */
	return 0;
}

int __init
micveth_init_legacy(int num_bds, struct device *dev)
{
	if (mic_vnet_mode != VNET_MODE_DMA)
		return micveth_create_int(num_bds, dev);
	/* DMA mode uses micveth_create */
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
	if (mic_vnet_mode == VNET_MODE_DMA)
		return micvnet_start(mic_ctx);
	else
		return micveth_start_int(mic_ctx);
}

void
micveth_stop(mic_ctx_t *mic_ctx)
{
	if (mic_vnet_mode == VNET_MODE_DMA)
		micvnet_stop(mic_ctx);
	else
		micveth_stop_int(mic_ctx);
}

static int __init
micveth_module_init(void)
{
	printk("vnet: mode: %s, buffers: %d\n", 
		mic_vnet_modes[mic_vnet_mode], vnet_num_buffers);

	if (mic_vnet_mode == VNET_MODE_DMA)
		return micvnet_module_init();
	else
		return micveth_module_init_int();
}

static void __exit
micveth_module_exit(void)
{
	if (mic_vnet_mode == VNET_MODE_DMA)
		micvnet_module_exit();
	else
		micveth_module_exit_int();
}

#ifdef HOST
#else  // card
module_init(micveth_module_init);
module_exit(micveth_module_exit);

MODULE_LICENSE("GPL");
#endif
