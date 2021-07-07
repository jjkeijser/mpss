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

#ifndef MICVETH_H
#define MICVETH_H

#include "micveth_dma.h"

#include "micint.h"
#include "micveth_common.h"

#define MICVETH_MAX_PACKET_SIZE		(63 * 1024)
#define MICVETH_TRANSFER_FIFO_SIZE	128

#define MICVETH_LINK_UP_MAGIC		0x1A77ABEE
#define MICVETH_LINK_DOWN_MAGIC		0x1DEADBEE

#define MICVETH_POLL_TIMER_DELAY	1
#define MICVETH_CLIENT_TIMER_DELAY	10

typedef struct ring_packet {
	struct sk_buff	*pd_skb;
	uint64_t	pd_phys;
	uint64_t	pd_length;
} ring_packet_t;

typedef struct ring_desc {
	uint64_t	rd_phys;
	uint64_t	rd_length;
	uint32_t	rd_valid;
} ring_desc_t;

typedef struct ring_queue {
	uint32_t	rq_head;
	uint32_t	rq_tail;
	uint32_t	rq_length;
	ring_desc_t	rq_descs[MICVETH_TRANSFER_FIFO_SIZE];
} ring_queue_t;

typedef struct ring {
	ring_queue_t	r_tx;
	ring_queue_t	r_rx;
} veth_ring_t;

#define VETH_STATE_INITIALIZED		0
#define VETH_STATE_LINKUP		1
#define VETH_STATE_LINKDOWN		2


typedef struct micveth_info {
	struct pci_dev		*vi_pdev;
	struct net_device 	*vi_netdev;
	uint8_t			*vi_sbox;
	uint8_t			*vi_dbox;
	uint32_t		*vi_scratch14;
	uint32_t		*vi_scratch15;
	mic_ctx_t		*mic_ctx;
	volatile uint32_t	vi_state;
	uint32_t		vi_skb_mtu;

	struct delayed_work	vi_poll;

	struct workqueue_struct	*vi_wq;
	char			vi_wqname[16];
	struct work_struct	vi_bh;
	struct work_struct	vi_txws;

	spinlock_t		vi_rxlock;
	spinlock_t		vi_txlock;

	struct {
		veth_ring_t	ring;
		uint64_t	phys;
		uint64_t	length;
	} vi_ring;

	veth_ring_t		*ring_ptr;

	ring_packet_t		vi_tx_desc[MICVETH_TRANSFER_FIFO_SIZE];
	ring_packet_t		vi_rx_desc[MICVETH_TRANSFER_FIFO_SIZE];
	uint32_t		vi_pend;
} micveth_info_t;

enum {
	CLIENT_POLL_STOPPED,
	CLIENT_POLL_RUNNING,
	CLIENT_POLL_STOPPING,
};

typedef struct micveth {
	int			lv_num_interfaces;
	int			lv_num_clients;
	int			lv_active_clients;
	int			lv_num_links_remaining;
	micveth_info_t		*lv_info;

	struct mutex		lv_state_mutex;

	uint32_t		lv_pollstate;
	struct delayed_work	lv_poll;
	wait_queue_head_t	lv_wq;

} micveth_t;

int micveth_init(struct device *dev);
int micveth_init_legacy(int num_bds, struct device *dev);
void micveth_exit(void);
int micveth_probe(mic_ctx_t *mic_ctx);
void micveth_remove(mic_ctx_t *mic_ctx);
int micveth_start(mic_ctx_t *mic_ctx);
void micveth_stop(mic_ctx_t *mic_ctx);

#endif /* MICVETH_H */
