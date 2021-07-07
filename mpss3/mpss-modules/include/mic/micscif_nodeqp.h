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

#ifndef MICSCIF_NODEQP
#define MICSCIF_NODEQP

#include "micscif_rb.h"

				   /* Payload Description */
#define SCIF_INIT		1  /* Address of node's node First message sent by a node to
				    * array the host, and host to node
				    */
#define SCIF_EXIT		2  /* Last message telling the host the driver is exiting */
#define SCIF_NODE_ADD		3  /* Tell Online nodes a new node exits */
#define SCIF_NODE_ADD_ACK	4  /* Confirm to host sequence is finished TODO Needed??? */
#define SCIF_CNCT_REQ		5  /* Phys addr of Request connection to a port */
#define SCIF_CNCT_GNT		6  /* Phys addr of new Grant connection request */
#define SCIF_CNCT_GNTACK	7  /* Error type Reject a connection request */
#define SCIF_CNCT_GNTNACK	8  /* Error type Reject a connection request */
#define SCIF_CNCT_REJ		9  /* Error type Reject a connection request */
#define SCIF_CNCT_TERM		10 /* Terminate type Terminate a connection request */
#define SCIF_TERM_ACK		11 /* Terminate type Terminate a connection request */
#define SCIF_DISCNCT		12 /* Notify peer that connection is being terminated */
#define SCIF_DISCNT_ACK		13 /* Notify peer that connection is being terminated */
#define SCIF_REGISTER		14 /* Tell peer about a new registered window */
#define SCIF_REGISTER_ACK	15 /* Notify peer about unregistration success */
#define SCIF_REGISTER_NACK	16 /* Notify peer about registration success */
#define SCIF_UNREGISTER		17 /* Tell peer about unregistering a registered window */
#define SCIF_UNREGISTER_ACK	18 /* Notify peer about registration failure */
#define SCIF_UNREGISTER_NACK	19 /* Notify peer about unregistration failure */
#define SCIF_ALLOC_REQ		20 /* Request a mapped buffer */
#define SCIF_ALLOC_GNT		21 /* Notify peer about allocation success */
#define SCIF_ALLOC_REJ		22 /* Notify peer about allocation failure */
#define SCIF_FREE_PHYS		23 /* Free previously allocated GTT/PCI mappings */
#define SCIF_FREE_VIRT		24 /* Free previously allocated virtual memory */
#define SCIF_CLIENT_SENT	25 /* Notify the peer that a data message has been written to the RB */
#define SCIF_CLIENT_RCVD	26 /* Notify the peer that a data message has been read from the RB */
#define SCIF_MUNMAP		27 /* Acknowledgment for a SCIF_MMAP request */
#define SCIF_MARK		28 /* SCIF Remote Fence Mark Request */
#define SCIF_MARK_ACK		29 /* SCIF Remote Fence Mark Success */
#define SCIF_MARK_NACK		30 /* SCIF Remote Fence Mark Failure */
#define SCIF_WAIT		31 /* SCIF Remote Fence Wait Request */
#define SCIF_WAIT_ACK		32 /* SCIF Remote Fence Wait Success */
#define SCIF_WAIT_NACK		33 /* SCIF Remote Fence Wait Failure */
#define SCIF_SIG_LOCAL		34 /* SCIF Remote Fence Local Signal Request */
#define SCIF_SIG_REMOTE		35 /* SCIF Remote Fence Remote Signal Request */
#define SCIF_SIG_ACK		36 /* SCIF Remote Fence Remote Signal Success */
#define SCIF_SIG_NACK		37 /* SCIF Remote Fence Remote Signal Failure */
#define SCIF_NODE_CREATE_DEP	42 /* Notify the Host that a new dependency is
 				    * being created between two nodes
 				    */
#define SCIF_NODE_DESTROY_DEP	43 /* Notify the Host that an existing dependency is
 				    * being destroyed between two nodes
 				    */
#define SCIF_NODE_REMOVE	44 /* Request to deactivate a set of remote SCIF nodes */
#define SCIF_NODE_REMOVE_ACK	45 /* Response to a SCIF_NODE_REMOVE message */
#define SCIF_NODE_WAKE_UP	46 /* Notification to the Host to wake up a remote node */
#define SCIF_NODE_WAKE_UP_ACK	47 /* Response to SCIF_NODE_WAKE_UP message */
#define SCIF_NODE_WAKE_UP_NACK	48 /* Response to SCIF_NODE_WAKE_UP message. Think Lost Node */
#define SCIF_NODE_ALIVE		49 /* Check if kn* card is alive */
#define SCIF_NODE_ALIVE_ACK	50 /* ACK the for above message */
#define SMPT_SET		51 /* Add a smpt entry */
#define SCIF_PROXY_DMA		56 /* Proxies DMA read requests to peer for performance */
#define SCIF_PROXY_ORDERED_DMA	57 /* Proxies DMA read requests to peer for performance */
#define SCIF_NODE_CONNECT	58 /* Setup a p2p connection b/w two nodes */
#define SCIF_NODE_CONNECT_NACK	59 /* p2p connection is not successful */
#define SCIF_NODE_ADD_NACK	60 /* SCIF_NODE_ADD failed report to the waiting thread(s) */
#define SCIF_GET_NODE_INFO	61 /* Get current node mask from the host*/
#define SCIF_TEST		62 /* Test value Used for test only */
#define SCIF_MAX_MSG		SCIF_TEST


/*
 * The *only* reason we need 2 uint64_t for payload
 * right now is because the SCIF_CNCT_GNT message needs
 * to send across both the QP offset and the QP id.
 *
 * Now we have to increase this to 3 uint64_t because
 * the Alloc message requires the remote EP, allocation size
 * and the allocation handle.
 *
 * Increased to 4 uint64_t because SCIF_FENCE requires
 * ep, offset, len and the waitqueue pointer to wake up.
 */
struct nodemsg {
	struct scif_portID src;
	struct scif_portID dst;
	uint32_t uop;
	uint64_t payload[4];
} __attribute__ ((packed));


/*
 * Generic state used for certain node QP message exchanges
 * like Unregister, Alloc etc.
 */
enum micscif_msg_state {
	OP_IDLE = 1,
	OP_IN_PROGRESS,
	OP_COMPLETED,
	OP_FAILED
};

/*
 * Generic structure used for exchanging ALLOC_REQ/GNT messages.
 */
struct allocmsg {
	dma_addr_t	phys_addr;
	void			*vaddr;
	uint32_t		uop;
	size_t			size;
	enum micscif_msg_state	state;
	wait_queue_head_t	allocwq;
};

/* Interesting structure -- a little difficult because we can only
 * write across the PCIe, so any r/w pointer we need to read is
 * local.  We only need to read the read pointer on the inbound_q
 * and read the write pointer in the outbound_q
 */
struct micscif_qp {
	uint64_t 		ep;
	uint64_t 		magic;
	uint64_t		blast;
#define SCIFEP_MAGIC    0x5c1f000000005c1f
	struct micscif_rb 	outbound_q;
	struct micscif_rb 	inbound_q;
	/* FIXME cache align local_write/read */
	uint32_t 		local_write; /* For local inbound */
	uint32_t 		local_read;  /* For local outbound */
	volatile struct micscif_qp *remote_qp;
	dma_addr_t 	local_buf;  /* Local BS */
	dma_addr_t 	local_qp;
	dma_addr_t 	remote_buf; /* Remote BS */
	volatile uint32_t	qp_state;
#define QP_OFFLINE	0xdead
#define QP_ONLINE	0xc0de
	uint16_t		scif_version;
	spinlock_t 		qp_send_lock;
	spinlock_t 		qp_recv_lock;
};

/*
 * An element in the loopback Node QP message list.
 */
struct loopb_msg {
	struct nodemsg		msg;
	struct list_head	list_member;
};

struct micscif_qp *micscif_nodeqp_nextmsg(struct micscif_dev *scifdev);
int micscif_nodeqp_send(struct micscif_dev *scifdev, struct nodemsg *msg, struct endpt *ep);
int micscif_nodeqp_intrhandler(struct micscif_dev *scifdev, struct micscif_qp *qp);
int micscif_loopb_msg_handler(struct micscif_dev *scifdev, struct micscif_qp *qp);

// Card side only functions
int micscif_setup_card_qp(phys_addr_t host_phys, struct micscif_dev *dev);

int micscif_setuphost_response(struct micscif_dev *scifdev, uint64_t payload);
int micscif_setup_qp_connect(struct micscif_qp *qp, dma_addr_t *qp_offset, int local_size, struct micscif_dev *scifdev);
int micscif_setup_qp_accept(struct micscif_qp *qp, dma_addr_t *qp_offset, dma_addr_t phys, int local_size, struct micscif_dev *scifdev);
int micscif_setup_qp_connect_response(struct micscif_dev *scifdev, struct micscif_qp *qp, uint64_t payload);
int micscif_setup_loopback_qp(struct micscif_dev *scifdev);
int micscif_destroy_loopback_qp(struct micscif_dev *scifdev);
void micscif_teardown_ep(void *endpt);
void micscif_add_epd_to_zombie_list(struct endpt *ep, bool mi_eplock_held);

#endif  /* MICSCIF_NODEQP */
