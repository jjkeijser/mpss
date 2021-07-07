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

#ifndef MICVETH_DMA_H
#define MICVETH_DMA_H

#include <linux/kernel.h>
#include "micint.h"

#include "mic_common.h"
#include "mic_dma_lib.h"
#include <linux/errno.h>
#include <linux/hardirq.h>
#include <linux/types.h>
#include <linux/capability.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/gfp.h>
#include <linux/vmalloc.h>
#include <asm/io.h>
#include <linux/kernel.h>
#include <linux/mm_types.h>
#include <linux/jiffies.h>
#include <linux/timer.h>
#include <linux/irqflags.h>
#include <linux/time.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/semaphore.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <asm/bug.h>
#include <linux/pci.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <asm/atomic.h>
#include <linux/netdevice.h>
#include <linux/debugfs.h>


/*
  Define this if only DMA mode is supported without legacy POLL/INTR modes
  (i.e if only micveth_dma.c is included in the host/card side drivers, i.e
  when linvnet.c is excluded from host side driver and micveth.c from card
  side driver). This will ensure that other global symbols which are at
  present common with legacy modes (in linvnet.c/micveth.c) are all included
  in micveth_dma.c.
*/
#undef STANDALONE_VNET_DMA

/*******************************************************/
#define MICVNET_MSG_RB_SIZE 128
#define DMA_ALIGNMENT L1_CACHE_BYTES
#define VNET_MAX_SKBS 62

/* The maximum total number of outstanding messages possible in the current
   implementation is 2 * VNET_MAX_SKBS + 1. */
#if (MICVNET_MSG_RB_SIZE < 2 * VNET_MAX_SKBS + 2)
#error "MICVNET_MSG_RB_SIZE should be at least (2 * VNET_MAX_SKBS + 2)"
#endif

#if (MICVNET_MSG_RB_SIZE & (MICVNET_MSG_RB_SIZE - 1))
#error "MICVNET_MSG_RB_SIZE should be power of 2"
#endif

enum micvnet_msg_id {
	MICVNET_MSG_ADD_DMA_BUFFER,
	MICVNET_MSG_DMA_COMPLETE,
	MICVNET_MSG_LINK_DOWN,
	MICVNET_MSG_LINK_UP,
};

struct micvnet_msg_add_dma_buffer {
	uint64_t buf_phys;
	uint64_t buf_size;
};

struct micvnet_msg_dma_complete {
	uint64_t dst_phys;
	uint64_t size;
	uint64_t dma_offset;
};

#define VNET_DRIVER_VERSION	1
struct micvnet_msg_link_up {
	uint64_t vnet_driver_version;
};

union micvnet_msg_body {
	struct micvnet_msg_add_dma_buffer	micvnet_msg_add_dma_buffer;
	struct micvnet_msg_dma_complete		micvnet_msg_dma_complete;
	struct micvnet_msg_link_up		micvnet_msg_link_up;
};

struct micvnet_msg {
	uint64_t		msg_id;
	union micvnet_msg_body	body;
};

struct micvnet_msg_rb {
	struct micvnet_msg buf[MICVNET_MSG_RB_SIZE];
	volatile uint32_t head;
	volatile uint32_t tail;
	uint32_t size;
	volatile uint32_t prev_head;
	volatile uint32_t prev_tail;
};

struct micvnet_msg_ring_pair {
	struct micvnet_msg_rb rb_tx;
	struct micvnet_msg_rb rb_rx;
};

struct micvnet_msg_qp {
	struct micvnet_msg_rb *tx;
	struct micvnet_msg_rb *rx;
};

/*******************************************************/

/* Restict micvnet mtu to 63K because ping does not work on RHEL 6.3 with 64K
   MTU - HSD [4118026] */
#define MICVNET_MAX_MTU			(63 * 1024)
#define MICVNET_CARD_UP_MAGIC		0x1A77BBEE

struct rx_node {
	struct list_head	 list;
	struct sk_buff		*skb;
	uint64_t		 phys;
	uint64_t		 size;
};

struct dma_node {
	struct list_head	 list;
	uint64_t		 phys;
	uint64_t		 size;
};

struct tx_node {
	struct list_head	 list;
	struct sk_buff		*skb;
};

struct sched_node {
	struct list_head	 list;
	struct sk_buff		*skb;
	unsigned char		*skb_data_aligned;
	uint64_t		 dma_src_phys;
	uint64_t		 dma_size;
	uint64_t		 dma_offset;
	uint64_t		 dst_phys;
};

struct obj_list {
	char	*buf;
	int	 size;
	size_t	 obj_size;
	volatile uint32_t head;
	volatile uint32_t tail;
};

struct micvnet_info {
	struct pci_dev			*vi_pdev;
	struct net_device		*vi_netdev;
	uint8_t				*vi_sbox;
	uint8_t				*vi_dbox;
	uint32_t			*vi_scratch14;
	mic_ctx_t			*mic_ctx;
	atomic_t			 vi_state;

	struct workqueue_struct		*vi_wq;
	char				 vi_wqname[16];
	struct work_struct			 vi_ws_bh;
	struct work_struct			 vi_ws_tx;
	struct work_struct			 vi_ws_dmacb;
	struct work_struct			 vi_ws_link_down;
	struct work_struct			 vi_ws_stop;
	struct work_struct			 vi_ws_start;

	spinlock_t			 vi_rxlock;
	spinlock_t			 vi_txlock;

#ifdef HOST
	struct micvnet_msg_ring_pair	 vi_rp;
#else
	struct micvnet_msg_ring_pair	*ring_ptr;
#endif
	uint64_t			 vi_rp_phys;
	struct micvnet_msg_qp		 vi_qp;

	struct obj_list			 dnode_list;

	struct list_head		 vi_rx_skb;
	struct list_head		 vi_dma_buf;
	struct list_head		 vi_tx_skb;
	struct list_head		 vi_sched_skb;

	mic_dma_handle_t		 dma_handle;
	struct dma_channel		*dma_chan;
	struct dma_completion_cb	 dma_cb;
	atomic_t			 cnt_dma_complete;

	atomic_t			 cnt_dma_buf_avail;
	bool				 link_down_initiator;
	atomic_t			 cnt_tx_pending;
	wait_queue_head_t		 stop_waitq;
};


struct micvnet {
	atomic_t		lv_active_clients;
	int			created;
};

int micvnet_init(struct device *dev);
void micvnet_exit(void);
int micvnet_probe(mic_ctx_t *mic_ctx);
void micvnet_remove(mic_ctx_t *mic_ctx);
int micvnet_xmit(struct sk_buff *skb, struct net_device *dev);

int micvnet_start(mic_ctx_t *mic_ctx);
void micvnet_stop(mic_ctx_t *mic_ctx);

#ifndef HOST
int __init micvnet_module_init(void);
void __exit micvnet_module_exit(void);
#endif

#ifdef STANDALONE_VNET_DMA
#define micveth_init	micvnet_init
#define micveth_exit	micvnet_exit
#define micveth_probe	micvnet_probe
#define micveth_remove	micvnet_remove
#define micveth_start	micvnet_start
#define micveth_stop	micvnet_stop
#endif

extern int vnet_num_buffers;
#ifndef HOST
extern ulong vnet_addr;
#endif
#endif // MICVETH_DMA_H
