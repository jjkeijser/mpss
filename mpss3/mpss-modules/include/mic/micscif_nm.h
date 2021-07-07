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

#ifndef MICSCIF_NM_H
#define MICSCIF_NM_H

#include <scif.h>

#ifdef MIC_IS_EMULATION
#define DEFAULT_WATCHDOG_TO	(INT_MAX)
#define NODE_ALIVE_TIMEOUT	(INT_MAX)
#define NODE_QP_TIMEOUT		(INT_MAX)
#define NODE_ACCEPT_TIMEOUT	(INT_MAX)
#define NODEQP_SEND_TO_MSEC	(INT_MAX)
#else
#define DEFAULT_WATCHDOG_TO	(30)
#define NODE_ALIVE_TIMEOUT	(ms_info.mi_watchdog_to * HZ)
#define NODE_QP_TIMEOUT		(100)
#define NODE_ACCEPT_TIMEOUT	(3 * HZ)
#define NODEQP_SEND_TO_MSEC	(3 * 1000)
#endif

#define SCIF_ENABLE_PM 1

#define	DESTROY_WQ		(true)

enum disconn_type {
	DISCONN_TYPE_POWER_MGMT,
	DISCONN_TYPE_LOST_NODE,
	DISCONN_TYPE_MAINTENANCE_MODE,
};

/*
 * Notify the host about a new dependency with the remote SCIF device.
 * Dependencies are created during scif_mmap()/scif_get_pages().
 */
void micscif_create_node_dep(struct micscif_dev *dev, int nr_pages);

/*
 * Notify the host that an existing dependency with the remote SCIF
 * device no longer exists.
 */
void micscif_destroy_node_dep(struct micscif_dev *dev, int nr_pages);

/**
 * micscif_inc_node_refcnt:
 *
 * @dev: Remote SCIF device.
 * @count: ref count
 *
 * Increment the global activity ref count for the remote SCIF device.
 * If the remote SCIF device is idle, then notify the host to wake up
 * the remote SCIF device and then wait for an ACK.
 */
static __always_inline void
micscif_inc_node_refcnt(struct micscif_dev *dev, long cnt)
{
#ifdef SCIF_ENABLE_PM
	if (unlikely(dev && !atomic_long_add_unless(&dev->scif_ref_cnt, 
		cnt, SCIF_NODE_IDLE))) {
		/*
		 * This code path would not be entered unless the remote
		 * SCIF device has actually been put to sleep by the host.
		 */
		mutex_lock(&dev->sd_lock);
		if (SCIFDEV_STOPPED == dev->sd_state ||
			SCIFDEV_STOPPING == dev->sd_state ||
			SCIFDEV_INIT == dev->sd_state)
			goto bail_out;
		if (test_bit(SCIF_NODE_MAGIC_BIT, 
			&dev->scif_ref_cnt.counter)) {
			/* Notify host that the remote node must be woken */
			struct nodemsg notif_msg;

			dev->sd_wait_status = OP_IN_PROGRESS;
			notif_msg.uop = SCIF_NODE_WAKE_UP;
			notif_msg.src.node = ms_info.mi_nodeid;
			notif_msg.dst.node = SCIF_HOST_NODE;
			notif_msg.payload[0] = dev->sd_node;
			/* No error handling for Host SCIF device */
			micscif_nodeqp_send(&scif_dev[SCIF_HOST_NODE],
						&notif_msg, NULL);
			/*
			 * A timeout is not required since only the cards can
			 * initiate this message. The Host is expected to be alive.
			 * If the host has crashed then so will the cards.
			 */
			wait_event(dev->sd_wq, 
				dev->sd_wait_status != OP_IN_PROGRESS);
			/*
			 * Aieee! The host could not wake up the remote node.
			 * Bail out for now.
			 */
			if (dev->sd_wait_status == OP_COMPLETED) {
				dev->sd_state = SCIFDEV_RUNNING;
				clear_bit(SCIF_NODE_MAGIC_BIT, 
					&dev->scif_ref_cnt.counter);
			}
		}
		/* The ref count was not added if the node was idle. */
		atomic_long_add(cnt, &dev->scif_ref_cnt);
bail_out:
		mutex_unlock(&dev->sd_lock);
	}
#endif
}

/**
 * micscif_dec_node_refcnt:
 *
 * @dev: Remote SCIF device.
 * @nr_pages: number of pages
 *
 * Decrement the global activity ref count for the remote SCIF device.
 * Assert if the ref count drops to negative.
 */
static __always_inline void
micscif_dec_node_refcnt(struct micscif_dev *dev, long cnt)
{
#ifdef SCIF_ENABLE_PM
	if (dev) {
		if (unlikely((atomic_long_sub_return(cnt, 
			&dev->scif_ref_cnt)) < 0)) {
			printk(KERN_ERR "%s %d dec dev %p node %d ref %ld "
				" caller %p Lost Node?? \n", 
				__func__, __LINE__, dev, dev->sd_node, 
				atomic_long_read(&dev->scif_ref_cnt), 
				__builtin_return_address(0));
			atomic_long_add_unless(&dev->scif_ref_cnt, cnt, 
							SCIF_NODE_IDLE);
		}
	}
#endif
}

/* Handle a SCIF_NODE_REMOVE message */
uint64_t micscif_handle_remove_node(uint64_t mask, uint64_t flags);
void micscif_cleanup_scifdev(struct micscif_dev *dev, bool destroy_wq);

void micscif_node_add_callback(int node);

void set_nodemask_bit(uint8_t* nodemask, uint32_t node_id, int val);
int get_nodemask_bit(uint8_t* nodemask, uint32_t node_id);

#ifndef _MIC_SCIF_

/* definition of stack node used in activation/deactivation set algorithms*/
struct stack_node {
	struct list_head next;
	uint32_t node_id;
};

enum dependency_state {
	DEP_STATE_NOT_DEPENDENT,
	DEP_STATE_DEPENDENT,
	DEP_STATE_DISCONNECT_READY,
	DEP_STATE_DISCONNECTED
};


uint64_t micscif_send_pm_rmnode_msg(int node, uint64_t nodemask_addr,
		uint64_t nodemask_size, int orig_node);
uint64_t micscif_send_lost_node_rmnode_msg(int node, int orig_node);

/* definitions of stack methods used in activation/deactivation set algorithms */
int init_depgraph_stack(struct list_head *stack_ptr);
int uninit_depgraph_stack(struct list_head *stack_ptr);
int is_stack_empty(struct list_head *stack_ptr);
int stack_push_node(struct list_head *stack_ptr, uint32_t node_id);
int stack_pop_node(struct list_head *stack_ptr, uint32_t *node_id);
int micscif_get_activeset(uint32_t node_id, uint8_t *nodemask);
int micscif_get_minimal_deactiveset(uint32_t node_id, uint8_t *nodemask, uint8_t *visited);
int micscif_get_deactiveset(uint32_t node_id, uint8_t *nodemask, int max_possible);
void micscif_update_p2p_state(uint32_t node_id, uint32_t peer_id, enum scif_state state);

/* Method responsible for disconnecting node from the scif network */
int micscif_disconnect_node(uint32_t node_id, uint8_t *nodemask, enum disconn_type type);
int micscif_connect_node(uint32_t node_id, bool get_ref);

void micscif_set_nodedep(uint32_t src_node, uint32_t dst_node, enum dependency_state state);
enum dependency_state micscif_get_nodedep(uint32_t src_node, uint32_t dst_node);
uint64_t micscif_send_node_alive(int node);
void micscif_watchdog_handler(struct work_struct *work);
int micscif_handle_lostnode(uint32_t nodeid);
#endif /*_MIC_SCIF_*/

/* SCIF tasks before transition to low power state */
int micscif_suspend_handler(struct notifier_block *notif,
		unsigned long event, void *ptr);

/*
 * SCIF tasks if a previous low power state transition
 * has failed after a suspend call.
 */
int micscif_fail_suspend_handler(struct notifier_block *notif,
		unsigned long event, void *ptr);

/* SCIF tasks after wake up from low powe state */
int micscif_resume_handler(struct notifier_block *notif,
		unsigned long event, void *ptr);

#endif /* MICSCIF_NM_H */
