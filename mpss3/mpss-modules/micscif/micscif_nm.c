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

/* SCIF Node Management */

#include "mic/micscif.h"
#ifndef _MIC_SCIF_
#include "mic_common.h"

#endif
#include "mic/micscif_map.h"
#include "mic/micscif_intr.h"
#ifdef _MIC_SCIF_
extern mic_dma_handle_t mic_dma_handle;
#else
extern bool mic_crash_dump_enabled;
#endif


/**
 * micscif_create_node_dep:
 *
 * @dev: Remote SCIF device.
 * @nr_pages: number of pages*
 *
 * Increment the map SCIF device ref count and notify the host if this is the
 * first dependency being create between the two nodes.
 */
void
micscif_create_node_dep(struct micscif_dev *dev, int nr_pages)
{
#ifdef SCIF_ENABLE_PM
	struct nodemsg notif_msg;

	if (dev) {
		mutex_lock(&dev->sd_lock);
		if (!dev->scif_map_ref_cnt) {
			/* Notify Host if this is the first dependency being created */
			notif_msg.uop = SCIF_NODE_CREATE_DEP;
			notif_msg.src.node = ms_info.mi_nodeid;
			notif_msg.payload[0] = dev->sd_node;
			/* No error handling for Host SCIF device */
			micscif_nodeqp_send(&scif_dev[SCIF_HOST_NODE], &notif_msg, NULL);
		}
		dev->scif_map_ref_cnt += nr_pages;
		mutex_unlock(&dev->sd_lock);
	}
#endif
}

/**
 * micscif_destroy_node_dep:
 *
 * @dev: Remote SCIF device.
 * @nr_pages: number of pages
 *
 * Decrement the map SCIF device ref count and notify the host if a dependency
 * no longer exists between two nodes.
 */
void
micscif_destroy_node_dep(struct micscif_dev *dev, int nr_pages)
{
#ifdef SCIF_ENABLE_PM
	struct nodemsg notif_msg;

	if (dev) {
		mutex_lock(&dev->sd_lock);
		dev->scif_map_ref_cnt -= nr_pages;
		if (!dev->scif_map_ref_cnt) {
			/* Notify Host if all dependencies have been destroyed */
			notif_msg.uop = SCIF_NODE_DESTROY_DEP;
			notif_msg.src.node = ms_info.mi_nodeid;
			notif_msg.payload[0] = dev->sd_node;
			/* No error handling for Host SCIF device */
			micscif_nodeqp_send(&scif_dev[SCIF_HOST_NODE], &notif_msg, NULL);
		}
		mutex_unlock(&dev->sd_lock);
	}
#endif
}

/**
 * micscif_callback:
 *
 * @node: node id of the node added/removed.
 * @event_type: SCIF_NODE_ADDED if a new node is added
 *	SCIF_NODE_REMOVED if a new node is removed
 *
 * Calls the callback function whenever a new node is added/removed
 */
static void micscif_callback(uint16_t node, enum scif_event_type event_type)
{
	struct list_head *pos;
	struct scif_callback *temp;
	union eventd event;

	switch (event_type) {
		case SCIF_NODE_ADDED:
			event.scif_node_added = node;
			break;
		case SCIF_NODE_REMOVED:
			event.scif_node_removed = node;
			break;
		default:
			return;
	}

	mutex_lock(&ms_info.mi_event_cblock);
	list_for_each(pos, &ms_info.mi_event_cb) {
		temp = list_entry(pos, struct scif_callback, list_member);
		temp->callback_handler(event_type, event);
	}
	mutex_unlock(&ms_info.mi_event_cblock);
}

/**
 * micscif_node_remove_callback:
 *
 * @node: node id of the node removed.
 *
 * Calls the callback function whenever a new node is removed
 */
static void micscif_node_remove_callback(int node)
{
	micscif_callback((uint16_t)node, SCIF_NODE_REMOVED);
}

/**
 * micscif_node_add_callback:
 *
 * @node: node id of the node added.
 *
 * Calls the callback function whenever a new node is added
 */
void micscif_node_add_callback(int node)
{
	micscif_callback((uint16_t)node, SCIF_NODE_ADDED);
}

void micscif_cleanup_qp(struct micscif_dev *dev)
{
	struct micscif_qp *qp;

	qp = &dev->qpairs[0];

	if (!qp)
		return;

	scif_iounmap((void*)qp->remote_qp, sizeof(struct micscif_qp), dev);
	scif_iounmap((void*)dev->qpairs[0].outbound_q.rb_base, sizeof(struct micscif_qp), dev);
	qp->remote_qp = NULL;
	dev->qpairs[0].local_write = 0;
	dev->qpairs[0].inbound_q.current_write_offset = 0;
	dev->qpairs[0].inbound_q.current_read_offset = 0;
#ifdef _MIC_SCIF_
	kfree((void*)(qp->inbound_q.rb_base));
	kfree(dev->qpairs);
	qp = NULL;
#endif
}

/*
 * micscif_cleanup_scifdev
 *
 * @dev: Remote SCIF device.
 * Uninitialize SCIF data structures for remote SCIF device.
 */
void micscif_cleanup_scifdev(struct micscif_dev *dev, bool destroy_wq)
{
	int64_t ret;
#ifndef _MIC_SCIF_
	mic_ctx_t *mic_ctx;
#endif
	if (SCIFDEV_NOTPRESENT == dev->sd_state) {
#ifdef _MIC_SCIF_
		/*
		 * If there are any stale qp allocated due to
		 * p2p connection failures then cleanup now
		 */
		micscif_cleanup_qp(dev);
#endif
		return;
	}

	dev->sd_wait_status = OP_FAILED;
	wake_up(&dev->sd_wq);

#ifdef _MIC_SCIF_
	/*
	 * Need to protect destruction of the workqueue since this code
	 * can be called from two contexts:
	 * a) Remove Node Handling.
	 * b) SCIF driver unload
	 */
	mutex_lock(&dev->sd_lock);
	if ((SCIFDEV_RUNNING != dev->sd_state) && (SCIFDEV_SLEEPING != dev->sd_state))
		goto unlock;
	dev->sd_state = SCIFDEV_STOPPED;
	wake_up(&dev->sd_p2p_wq);
	mutex_unlock(&dev->sd_lock);
	deregister_scif_intr_handler(dev);
	if (destroy_wq && dev->sd_intr_wq) {
		destroy_workqueue(dev->sd_intr_wq);
		dev->sd_intr_wq = NULL;
	}
#endif

	mutex_lock(&dev->sd_lock);
#ifndef _MIC_SCIF_
	if ((SCIFDEV_RUNNING != dev->sd_state) && (SCIFDEV_SLEEPING != dev->sd_state))
		goto unlock;
	dev->sd_state = SCIFDEV_STOPPED;
#endif
	/*
	 * Change the state of the remote SCIF device
	 * to idle as soon as the activity counter is
	 * zero. The node state and ref count is
	 * maintained within a single atomic_long_t.
	 * No timeout for this tight loop since we expect
	 * the node to complete the API it is currently
	 * executing following which the scif_ref_count
	 * will drop to zero.
	 */
	do {
		ret = atomic_long_cmpxchg(
			&dev->scif_ref_cnt, 0, SCIF_NODE_IDLE);
		cpu_relax();
	} while (ret && ret != SCIF_NODE_IDLE);

	mutex_unlock(&dev->sd_lock);
	/* Cleanup temporary registered windows */
	flush_workqueue(ms_info.mi_misc_wq);
	mutex_lock(&dev->sd_lock);

#ifdef _MIC_SCIF_
	drain_dma_global(mic_dma_handle);
#else
	mic_ctx = get_per_dev_ctx(dev->sd_node - 1);
	drain_dma_global(mic_ctx->dma_handle);
	micscif_destroy_p2p(mic_ctx);
#endif
	scif_invalidate_ep(dev->sd_node);
	micscif_kill_apps_with_mmaps(dev->sd_node);

	micscif_cleanup_qp(dev);
	mutex_unlock(&dev->sd_lock);
#ifndef _MIC_SCIF_
	mutex_lock(&ms_info.mi_conflock);
	ms_info.mi_mask &= ~(0x1 << dev->sd_node);
	ms_info.mi_total--;
	mutex_unlock(&ms_info.mi_conflock);
#endif

	/* Wait for all applications to unmap remote memory mappings. */
	wait_event(dev->sd_mmap_wq, 
		!micscif_rma_do_apps_have_mmaps(dev->sd_node));
	micscif_cleanup_rma_for_zombies(dev->sd_node);
	micscif_node_remove_callback(dev->sd_node);
	return;
unlock:
	mutex_unlock(&dev->sd_lock);
}

/*
 * micscif_remove_node:
 *
 * @mask: bitmask of nodes in the deactivation set.
 * @flags: Type of deactivation set i.e. Power Management,
 * RAS, Maintenance Mode etc.
 * @block: Can block.
 *
 * Attempt to deactivate a set of remote SCIF devices nodes passed in mask.
 * If the SCIF activity ref count is positive for a remote node then
 * the approporiate bit in the input bitmask is reset and the resultant
 * bitmask is returned.
 */
uint64_t micscif_handle_remove_node(uint64_t mask, uint64_t payload)
{
	int64_t ret;
	int err = 0;
	uint32_t i;
	struct micscif_dev *dev;
	uint64_t flags = 0;
	flags = payload & 0x00000000FFFFFFFF;

	switch(flags) {
	case DISCONN_TYPE_POWER_MGMT:
	{
		uint8_t *nodemask_buf = NULL;
		int size = payload >> 32;

#ifndef _MIC_SCIF_
		nodemask_buf = mic_data.dd_pm.nodemask;
#else
		nodemask_buf = scif_ioremap(mask, size,  &scif_dev[SCIF_HOST_NODE]);
#endif
		if (!nodemask_buf) {
			err = EAGAIN;
			break;
		}

		for (i = 0; i <= ms_info.mi_maxid; i++) {
			dev = &scif_dev[i];
			if (!get_nodemask_bit(nodemask_buf , i))
				continue;
			/*
			 * Try for the SCIF device lock. Bail out if
			 * it is already grabbed since some other
			 * thread is already working on some other
			 * node state transition for this remote SCIF device.
			 */
			if (mutex_trylock(&dev->sd_lock)) {

				if (SCIFDEV_RUNNING != dev->sd_state) {
					mutex_unlock(&dev->sd_lock);
					continue;
				}
				/*
				 * Change the state of the remote SCIF device
				 * to idle only if the activity counter is
				 * already zero. The node state and ref count
				 * is maintained within a single atomic_long_t.
				 */
				ret = atomic_long_cmpxchg(
						&dev->scif_ref_cnt, 0, SCIF_NODE_IDLE);

				if (!ret || ret == SCIF_NODE_IDLE) {
					if (!ret) {
#ifdef _MIC_SCIF_
						drain_dma_global(mic_dma_handle);
#else
						mic_ctx_t *mic_ctx = get_per_dev_ctx(dev->sd_node - 1);
						drain_dma_global(mic_ctx->dma_handle);
#endif
					}
					/*
					 * Turn off the remote SCIF device.
					 * Any communication to this SCIF
					 * after this point will require a
					 * wake up message to the host.
					 */
					dev->sd_state = SCIFDEV_SLEEPING;
					err = 0;
				}
				else {
					/*
					 * Cannot put the remote SCIF device
					 * to sleep.
					 */
					err = EAGAIN;
					mutex_unlock(&dev->sd_lock);
					break;
				}
				mutex_unlock(&dev->sd_lock);
			} else {
				err = EAGAIN;
				break;
			}
		}

#ifndef _MIC_SCIF_
			scif_iounmap(nodemask_buf, size, &scif_dev[SCIF_HOST_NODE]);
#endif

		break;
	}
	case DISCONN_TYPE_LOST_NODE:
	{
		/* In the case of lost node, first paramater
		 * is the node id and not a mask.
		 */
		dev = &scif_dev[mask];
		micscif_cleanup_scifdev(dev, !DESTROY_WQ);
		break;
	}
	default:
	{
		/* Unknown remove node flags */
		BUG_ON(1);
	}
	}

	return err;
}

/**
 * set_nodemask_bit:
 *
 * @node_id[in]: node id to be set in the mask
 *
 * Set bit in the nodemask. each bit represents node. set bit to add node in to
 * activation/de-activation set
 */
//void
//set_nodemask_bit(uint64_t *nodemask, uint32_t node_id)
void
set_nodemask_bit(uint8_t* nodemask, uint32_t node_id, int val)
{
	int index = 0;
	uint8_t *temp_mask;

	index = (int) node_id / 8;
	temp_mask = nodemask + index;
	node_id = node_id - (index * 8);
	if (val)
		*temp_mask |= (1ULL << node_id);
	else
		*temp_mask &= ~(1ULL << node_id);
}

/**
 * check_nodemask_bit:
 *
 * @node_id[in]: node id to be set in the mask
 *
 * Check if a bit in the nodemask corresponding to a
 * node id is set.
 *
 * return 1 if the bit is set. 0 if the bit is cleared.
 */
int
get_nodemask_bit(uint8_t* nodemask, uint32_t node_id) {
	int index = 0;
	uint8_t *temp_mask;

	index = (int) node_id / 8;
	temp_mask = nodemask + index;
	node_id = node_id - (index * 8);
	return *temp_mask & (1ULL << node_id);

}
/**
* nodemask_isvalid - Check if a nodemask is valid after
* calculating the de-activation set.
*
* @nodemask[in]: The nodemask to be checked.
*
* Returns true if valid.
*/
bool nodemask_isvalid(uint8_t* nodemask) {
	uint32_t i;
	for (i = 0; i <= ms_info.mi_maxid; i++) {
		if (get_nodemask_bit(nodemask, i))
			return true;
	}

	return false;
}

#ifndef _MIC_SCIF_
/*
 * micscif_send_rmnode_msg:
 *
 * @mask: Bitmask of nodes in the deactivation set.
 * @node: Destination node for a deactivation set.
 * @flags: Type of deactivation set i.e. Power Management,
 * RAS, Maintenance Mode etc.
 * @orig_node: The node which triggered this remove node message.
 *
 * Sends a deactivation request to the valid nodes not included in the
 * deactivation set from the Host and waits for a response.
 * Returns the response mask received from the node.
 */
uint64_t micscif_send_pm_rmnode_msg(int node, uint64_t nodemask_addr,
		uint64_t nodemask_size, int orig_node) {

	uint64_t ret;
	struct nodemsg notif_msg;
	struct micscif_dev *dev = &scif_dev[node];

	/*
	 * Send remove node msg only to running nodes.
	 * An idle node need not know about another _lost_ node
	 * until it wakes up. When it does, it will request the
	 * host to wake up the _lost_ node to which the host will
	 * respond with a NACK
	 */

	if (SCIFDEV_RUNNING != dev->sd_state)
		return -ENODEV;

	notif_msg.uop = SCIF_NODE_REMOVE;
	notif_msg.src.node = ms_info.mi_nodeid;
	notif_msg.dst.node = node;
	notif_msg.payload[0] = nodemask_addr;
	notif_msg.payload[1] = DISCONN_TYPE_POWER_MGMT;
	notif_msg.payload[1] |= (nodemask_size << 32);
	notif_msg.payload[2] = atomic_long_read(&ms_info.mi_unique_msgid);
	notif_msg.payload[3] = orig_node;
	/* Send the request to remove a set of nodes */
	pr_debug("Send PM rmnode msg for node %d to node %d\n", orig_node, node);
	ret = micscif_nodeqp_send(dev, &notif_msg, NULL);

	return ret;
}

uint64_t micscif_send_lost_node_rmnode_msg(int node, int orig_node) {
	uint64_t ret;
	struct nodemsg notif_msg;
	struct micscif_dev *dev = &scif_dev[node];

	/*
	 * Send remove node msg only to running nodes.
	 * An idle node need not know about another _lost_ node
	 * until it wakes up. When it does, it will request the
	 * host to wake up the _lost_ node to which the host will
	 * respond with a NACK
	 */
	if (SCIFDEV_RUNNING != dev->sd_state)
		return -ENODEV;

	micscif_inc_node_refcnt(dev, 1);
	notif_msg.uop = SCIF_NODE_REMOVE;
	notif_msg.src.node = ms_info.mi_nodeid;
	notif_msg.dst.node = node;
	notif_msg.payload[0] = orig_node;
	notif_msg.payload[1] = DISCONN_TYPE_LOST_NODE;
	notif_msg.payload[3] = orig_node;
	/* Send the request to remove a set of nodes */
	ret = micscif_nodeqp_send(dev, &notif_msg, NULL);
	micscif_dec_node_refcnt(dev, 1);

	return ret;
}

/*
 * micpm_nodemask_uninit:
 * @node - node to uninitalize
 *
 * Deallocate memory for per-card nodemask buffer
*/
void
micpm_nodemask_uninit(mic_ctx_t* mic_ctx)
{
	if (mic_ctx && mic_ctx->micpm_ctx.nodemask.va) {
		mic_ctx_unmap_single(mic_ctx, mic_ctx->micpm_ctx.nodemask.pa,
			mic_ctx->micpm_ctx.nodemask.len);
		kfree(mic_ctx->micpm_ctx.nodemask.va);
	}
}

/*
 * micpm_nodemask_init:
 * @num_devs - no of scif nodes including the host
 * @node - node to initialize
 *
 * Allocate memory for per-card nodemask buffer
*/
int
micpm_nodemask_init(uint32_t num_devs, mic_ctx_t* mic_ctx)
{
	if (!mic_ctx)
		return 0;

	mic_ctx->micpm_ctx.nodemask.len =  ((int) (num_devs / 8) +
				((num_devs % 8) ? 1 : 0));
	mic_ctx->micpm_ctx.nodemask.va = (uint8_t *)
				kzalloc(mic_ctx->micpm_ctx.nodemask.len, GFP_KERNEL);

	if (!mic_ctx->micpm_ctx.nodemask.va) {
		PM_DEBUG("Error allocating nodemask buffer\n");
		return -ENOMEM;
	}

	mic_ctx->micpm_ctx.nodemask.pa  = mic_ctx_map_single(mic_ctx,
			mic_ctx->micpm_ctx.nodemask.va,
			mic_ctx->micpm_ctx.nodemask.len);

	if(mic_map_error(mic_ctx->micpm_ctx.nodemask.pa)) {
		PM_PRINT("Error Mapping nodemask buffer\n");
		kfree(mic_ctx->micpm_ctx.nodemask.va);
	}
	return 0;
}

/**
 * micpm_disconn_uninit:
 * @num_devs - no of scif nodes including host
 * Note - can not use ms_info.mi_total(total no of scif nodes) as it is updated after the driver load is complete
 *
 * Reset/re-initialize data structures needed for PM disconnection. This is necessary everytime the board is reset.
 * Since host(node 0)represents one of the node in network, it is necessary to clear dependency of host with the given node
 */
int
micpm_disconn_uninit(uint32_t num_devs)
{
	uint32_t i;
	uint32_t status = 0;

	/*
	 * ms_info.mi_total is updated after the driver load is complete
	 * switching back to static allocation of max nodes
	 */

	if (ms_info.mi_depmtrx) {

		for (i = 0; i < (int)num_devs; i++) {
			if (ms_info.mi_depmtrx[i]) {
				kfree(ms_info.mi_depmtrx[i]);
			}
		}
		kfree(ms_info.mi_depmtrx);
	}

	if (mic_data.dd_pm.nodemask)
		kfree(mic_data.dd_pm.nodemask);

	return status;
}

/**
 * micpm_disconn_init:
 * @num_devs - no of scif nodes including host
 * Note - can not use ms_info.mi_total(total no of scif nodes) as it is updated after the driver load is complete
 *
 * Allocate memory for dependency graph. Initialize dependencies for the node.
 * The memory allocated is based on the no of devices present during driver load.
 */
int
micpm_disconn_init(uint32_t num_devs)
{
	uint32_t i;
	uint32_t status = 0;
	mic_ctx_t *mic_ctx;

	if (ms_info.mi_depmtrx)
		return status;

	ms_info.mi_depmtrx = (uint32_t**)kzalloc(sizeof(uint32_t*) * num_devs, GFP_KERNEL);
	if (!ms_info.mi_depmtrx) {
		pr_debug("dependency graph initialization failed\n");
		status = -ENOMEM;
		goto exit;
	}

	for (i = 0; i < (int)num_devs; i++) {
		ms_info.mi_depmtrx[i] = (uint32_t*)kzalloc(sizeof(uint32_t) * num_devs, GFP_KERNEL);
		if (!ms_info.mi_depmtrx[i]) {
			micpm_disconn_uninit(num_devs);
			pr_debug("dependency graph initialization failed\n");
			status = -ENOMEM;
			goto exit;
		}
	}
	init_waitqueue_head(&ms_info.mi_disconn_wq);
	atomic_long_set(&ms_info.mi_unique_msgid, 0);

	//In Windows, this code is executed during micpm_probe
	for(i = 0; i < (num_devs - 1); i++) {
		mic_ctx = get_per_dev_ctx(i);
		status = micpm_nodemask_init(num_devs, mic_ctx);
		if (status)
			goto exit;
	}

	/* Set up a nodemask buffer for Host scif node in a common pm_ctx */
	mic_data.dd_pm.nodemask_len =  ((int) (num_devs / 8) +
				((num_devs % 8) ? 1 : 0));
	mic_data.dd_pm.nodemask = (uint8_t *)
				kzalloc(mic_data.dd_pm.nodemask_len, GFP_KERNEL);

	if (!mic_data.dd_pm.nodemask) {
		PM_DEBUG("Error allocating nodemask buffer\n");
		status = -ENOMEM;
		goto exit;
	}

exit:
	return status;
}

/**
 * micscif_set_nodedep:
 *
 * @src_node: node which is creating dependency.
 * @dst_node: node on which dependency is being created
 *
 * sets the given value in dependency graph for src_node -> dst_node
 */
void
micscif_set_nodedep(uint32_t src_node, uint32_t dst_node, enum dependency_state state)
{
	/* We dont need to lock dependency graph while updating
	 * as every node will modify its own row
	 */
	if (ms_info.mi_depmtrx)
		ms_info.mi_depmtrx[src_node][dst_node] = state;
}

/**
 * micscif_get_nodedep:
 *
 * @src_node: node which has/has not created dependency.
 * @dst_node: node on which dependency was/was not created
 *
 * gets the current value in dependency graph for src_node -> dst_node
 */
enum dependency_state
micscif_get_nodedep(uint32_t src_node, uint32_t dst_node)
{
	enum dependency_state state = DEP_STATE_NOT_DEPENDENT;
	if (ms_info.mi_depmtrx)
		state = ms_info.mi_depmtrx[src_node][dst_node];
	return state;
}

/**
 * init_depgraph_stack:
 *
 * @stack_ptr: list head.
 *
 * Initialize linked list to be used as stack
 */
int
init_depgraph_stack(struct list_head *stack_ptr)
{
	int status = 0;

	if (!stack_ptr) {
		pr_debug("%s argument stack_ptr is invalid\n", __func__);
		status = -EINVAL;
		goto exit;
	}
	/* Initialize stack */
	INIT_LIST_HEAD(stack_ptr);

exit:
	return status;
}

/**
 * uninit_depgraph_stack:
 *
 * @stack_ptr: list head for linked list(stack).
 *
 * Empty stack(linked list). Pop all the nodes left in the stack.
 */
int
uninit_depgraph_stack(struct list_head *stack_ptr)
{
	int status = 0;
	uint32_t node_id;
	if (!stack_ptr) {
		pr_debug("%s argument stack_ptr is invalid\n", __func__);
		status = -EINVAL;
		goto exit;
	}

	/* pop all the nodes left in the stack */
	while (!is_stack_empty(stack_ptr)) {
		status = stack_pop_node(stack_ptr, &node_id);
		if (status) {
			pr_debug("%s error while cleaning up depgraph stack\n", __func__);
			status = -EINVAL;
			goto exit;
		}
	}

exit:
	return status;
}

/**
 * is_stack_empty:
 *
 * @stack_ptr: list head for linked list(stack).
 *
 * returns true if the stack is empty.
 */
int
is_stack_empty(struct list_head *stack_ptr)
{
	if(list_empty(stack_ptr)) {
		return 1;
	}
	return 0;
}

/**
 * stack_push_node:
 *
 * @stack_ptr[in]: list head for linked list(stack).
 * @node_id[in]: node id to be pushed
 *
 * Push node in to the stack i.e. create node and add it at the start of linked list
 */
int
stack_push_node(struct list_head *stack_ptr, uint32_t node_id)
{
	int status = 0;
	struct stack_node *datanode = NULL;

	datanode = kmalloc(sizeof(struct stack_node), GFP_KERNEL);
	if (!datanode) {
		pr_debug("%s error allocating memory to stack node.\n", __func__);
		status = -ENOMEM;
		goto exit;
	}

	datanode->node_id = node_id;
	list_add(&datanode->next, stack_ptr);
exit:
	return status;
}

/**
 * stack_pop_node:
 *
 * @stack_ptr[in]: list head for linked list(stack).
 * @node_id[out]: pointer to the node id to be popped
 *
 * Pop node from the stack i.e. delete first entry of linked list and return its data.
 */
int
stack_pop_node(struct list_head *stack_ptr, uint32_t *node_id)
{
	int status = 0;
	struct stack_node *datanode = NULL;

	if(is_stack_empty(stack_ptr)) {
		pr_debug("%s stack found empty when tried to pop\n", __func__);
		status = -EFAULT;
		goto exit;
	}

	datanode = list_first_entry(stack_ptr, struct stack_node, next);
	if (!datanode) {
		pr_debug("%s Unable to pop from stack\n", __func__);
		status = -EFAULT;
		goto exit;
	}
	*node_id = datanode->node_id;

	list_del(&datanode->next);
	if (datanode) {
		kfree(datanode);
	}

exit:
	return status;
}

/**
 * micscif_get_activeset:
 *
 * @node_id[in]: source node id.
 * @nodemask[out]: bitmask of nodes present in activation set
 *
 * Algorithm to find out activation set for the given source node. Activation set is used to re-connect node into
 * the scif network.
 */
int
micscif_get_activeset(uint32_t node_id, uint8_t *nodemask)
{
	int status = 0;
	uint32_t i = 0;
	struct list_head	stack;
	uint8_t visited[128] = {0}; // 128 is max number of nodes.
	uint32_t num_nodes = ms_info.mi_maxid + 1;
	mic_ctx_t *mic_ctx;

	if (!ms_info.mi_depmtrx) {
		status = -EINVAL;
		goto exit;
	}

	status = init_depgraph_stack(&stack);
	if (status) {
		pr_debug("%s failed to initilize depgraph stack\n", __func__);
		goto exit;
	}

	status = stack_push_node(&stack, node_id);
	if (status) {
		pr_debug("%s error while running activation set algorithm\n", __func__);
		goto exit;
	}

	/* mark node visited to avoid repetition of the algorithm for the same node */
	visited[node_id] = 1;

	while (!is_stack_empty(&stack)) {
		status = stack_pop_node(&stack, &node_id);
		if (status) {
			pr_debug("%s error while running activation set algorithm\n", __func__);
			goto exit;
		}

		/* include node_id in the activation set*/
		set_nodemask_bit(nodemask, node_id, 1);

		for (i = 0; i < num_nodes; i++) {
			/* check if node has dependency on any node 'i' which is also disconnected at this time*/
			if ((!visited[i]) && (ms_info.mi_depmtrx[node_id][i] == DEP_STATE_DISCONNECTED)) {
				visited[i] = 1;
				if (i == 0)
					continue;
				mic_ctx = get_per_dev_ctx(i - 1);
				if ((mic_ctx->micpm_ctx.idle_state == PM_IDLE_STATE_PC3) ||
				    (mic_ctx->micpm_ctx.idle_state == PM_IDLE_STATE_PC6)) {
					status = stack_push_node(&stack, i);
					if (status) {
						pr_debug("%s error while running activation set algorithm\n", __func__);
						goto exit;
					}
				}
			}
		}
	} /* end of while (!is_stack_empty(&stack)) */
exit:
	uninit_depgraph_stack(&stack);
	return status;
}

/**
 * micscif_get_minimal_deactiveset:
 *
 * @node_id[in]: source node id.
 * @nodemask[out]: bitmask of nodes present in de-activation set
 * @visited[in/out]: information of which nodes are already visited in de-activation set algorithm
 *
 * Algorithm to find out minimum/must de-activation set for the given source node. This method is part of and used by
 * micscif_get_deactiveset.
 */
int micscif_get_minimal_deactiveset(uint32_t node_id, uint8_t *nodemask, uint8_t *visited)
{
	int status = 0;
	uint32_t i = 0;
	struct list_head stack;
	uint32_t num_nodes = ms_info.mi_maxid + 1;

	if (!ms_info.mi_depmtrx) {
		status = -EINVAL;
		goto exit;
	}

	status = init_depgraph_stack(&stack);
	if (!visited) {
		pr_debug("%s invalid parameter visited", __func__);
		status = -EINVAL;
		goto exit_pop;
	}

	if (status) {
		pr_debug("%s failed to initilize depgraph stack\n", __func__);
		goto exit_pop;
	}

	status = stack_push_node(&stack, node_id);
	if (status) {
		pr_debug("%s error while running de-activation set algorithm\n", __func__);
		goto exit_pop;
	}

	/* mark node visited to avoid repetition of the algorithm for the same node */
	visited[node_id] = 1;

	while (!is_stack_empty(&stack)) {

		status = stack_pop_node(&stack, &node_id);
		if (status) {
			pr_debug("%s error while running de-activation set algorithm\n", __func__);
			goto exit_pop;
		}

		/* include node_id in the activation set*/
		set_nodemask_bit(nodemask, node_id, 1);

		for (i = 0; i < num_nodes; i++) {
			if (!visited[i]) {
				if (ms_info.mi_depmtrx[i][node_id] == DEP_STATE_DEPENDENT) {
					/* The algorithm terminates, if we find any dependent node active */
					status = -EOPNOTSUPP;
					goto exit_pop;
				} else if(ms_info.mi_depmtrx[i][node_id] == DEP_STATE_DISCONNECT_READY) {
					/* node is dependent but ready to get disconnected */
					visited[i] = 1;
					status = stack_push_node(&stack, i);
					if (status) {
						pr_debug("%s error while running de-activation set algorithm\n", __func__);
						goto exit_pop;
					}
				}
			}
		}
	}/*end of while(!is_stack_empty(&stack))*/

exit_pop:
	while (!is_stack_empty(&stack)) {
		status = stack_pop_node(&stack, &node_id);
		if (status) {
			pr_debug("%s error while running activation set algorithm\n", __func__);
			break;
		}
		if (visited)
			visited[node_id] = 0;
	}
exit:
	return status;
}

/**
 * micscif_get_deactiveset:
 *
 * @node_id[in]: source node id.
 * @nodemask[out]: bitmask of nodes present in de-activation set
 * @max_disconn: flag to restrict de-activation set algoritthm to minimum/must set.
 *                True value indicates maximum de-activation set
 *
 * Algorithm to find out de-activation set for the given source node. De-activation set is used to disconnect node into
 * the scif network. The algorithm can find out maximum possible de-activation set(required in situations like
 * power management)if the max_possible flag is set.
 */
int
micscif_get_deactiveset(uint32_t node_id, uint8_t *nodemask, int max_disconn)
{
	int status = 0;
	uint32_t i = 0;
	struct list_head 	stack;
	uint8_t *visited = NULL;
	uint8_t cont_next_step = 0;
	uint32_t num_nodes = ms_info.mi_maxid + 1;
	mic_ctx_t *mic_ctx;

	if (!ms_info.mi_depmtrx) {
		status = -EINVAL;
		goto exit;
	}

	status = init_depgraph_stack(&stack);
	if (status) {
		pr_debug("%s failed to initilize depgraph stack\n", __func__);
		goto exit;
	}

	visited = kzalloc(sizeof(uint8_t) * num_nodes, GFP_KERNEL);
	if (!visited) {
		pr_debug("%s failed to allocated memory for visited array", __func__);
		status = -ENOMEM;
		goto exit;
	}

	status = stack_push_node(&stack, node_id);
	if (status) {
		pr_debug("%s error while running de-activation set algorithm\n", __func__);
		goto exit;
	}

	while (!is_stack_empty(&stack)) {

		status = stack_pop_node(&stack, &node_id);
		if (status) {
			pr_debug("%s error while running de-activation set algorithm\n", __func__);
			goto exit;
		}

		/* check if we want to find out maximum possible de-activation set */
		if (max_disconn) {
			cont_next_step = 1;
		}

		if (!visited[node_id]) {
			status = micscif_get_minimal_deactiveset(node_id, nodemask, visited);
			if (status) {
				if (status == -EOPNOTSUPP) {
					pr_debug("%s No deactivation set found for node %d", __func__, node_id);
					cont_next_step = 0;
				}
				else {
					pr_debug("%s Failed to calculate deactivation set", __func__);
					goto exit;
				}
			}

		} /* end for  if (!visited[node_id]) */

		if (cont_next_step) {
			for (i = 0; i < num_nodes; i++) {
				/* check if we can put more nodes 'i' in de-activation set if this node(dependent node)
				 * is de-activating
				 */
				if ((!visited[i]) &&
				    (ms_info.mi_depmtrx[node_id][i] == DEP_STATE_DISCONNECT_READY)) {
					if (i == 0)
						continue;
					mic_ctx = get_per_dev_ctx(i - 1);
					if (mic_ctx->micpm_ctx.idle_state ==
						PM_IDLE_STATE_PC3_READY) {
						/* This node might be able to get into deactivation set */
						status = stack_push_node(&stack, i);
						if (status) {
							pr_debug("%s error while running de-activation set algorithm\n", __func__);
							goto exit;
						}
					}
				}
			}
		}
	} /* end for while (!is_stack_empty(&stack)) */

	if (!nodemask_isvalid(nodemask)) {
		pr_debug("%s No deactivation set found for node %d", 
		__func__, node_id);
		status = -EOPNOTSUPP;
	}
exit:
	if (visited) {
		kfree(visited);
	}
	uninit_depgraph_stack(&stack);
	return status;
}

/* micscif_update_p2p_state:
 *
 * Update the p2p_disc_state of peer node peer_id in the p2p list of node node_id.
 *
 * @node_id: The node id whose p2p list needs to be updated.
 * @peer_id: The node id in the p2p list of the node_id that will get updated.
 * @scif_state: The state to be updated to.
 *
 */
void micscif_update_p2p_state(uint32_t node_id, uint32_t peer_id, enum scif_state state) {

	struct micscif_dev *dev;
	struct list_head *pos, *tmp;
	struct scif_p2p_info *p2p;

	dev = &scif_dev[node_id];
	if (!list_empty(&dev->sd_p2p)) {
		list_for_each_safe(pos, tmp, &dev->sd_p2p) {
			p2p = list_entry(pos, struct scif_p2p_info, 
			ppi_list);
			if(p2p->ppi_peer_id == peer_id) {
				p2p->ppi_disc_state = state;
				break;
			}
		}
	}
}

/* micscif_p2p_node_exists: Check if a node exists in the
 * list of nodes that have been sent an rmnode message.
 *
 * node_list: The list that contains the nodes that has been
 * sent the rmnode message for this transaction.
 * node_id: the node to be searched for.
 *
 * returns: true of the node exists.False otherwise
 */
bool micscif_rmnode_msg_sent(struct list_head *node_list, uint32_t node_id) {

	struct list_head *pos1, *tmp1;
	struct stack_node *added_node;

	if (!list_empty(node_list)) {
		list_for_each_safe(pos1, tmp1, node_list) {
			added_node = list_entry(pos1, struct stack_node, next);
			if(added_node->node_id == node_id)
				return true;
		}
	}
	return false;
}

/**
 * micscif_execute_disconnecte: Perform PM disconnection of a node
 * with its neighboring nodes.
 *
 * node_id: The node to be disconnected.
 * nodemask: Mask containing the list of nodes (including node_id)
 * to be disconnected.
 * node_list: List of nodes that received the disconnection message.
 */
int micscif_execute_disconnect(uint32_t node_id,
			uint8_t *nodemask,
			struct list_head *node_list)
{
	uint32_t status = 0;
	int ret;
	uint64_t msg_cnt = 0;
	uint32_t i = 0;
	int pending_wakeups = 0;
	mic_ctx_t *send_rmnode_ctx;
	uint32_t node;
	mic_ctx_t *mic_ctx = get_per_dev_ctx(node_id - 1);
	struct scif_p2p_info *p2p;
	struct list_head *pos, *tmp;
	struct micscif_dev *dev;


	/* Always send rmnode msg to SCIF_HOST_NODE */
	memcpy(mic_data.dd_pm.nodemask, nodemask, 
			mic_data.dd_pm.nodemask_len);
	ret = (int) micscif_send_pm_rmnode_msg(SCIF_HOST_NODE, 0, mic_data.dd_pm.nodemask_len,
			node_id);
	/* Add this node to msg list. */
	if(!ret) {
		msg_cnt++;
		stack_push_node(node_list, SCIF_HOST_NODE);
	}

	if((ret == 0)||(ret == -ENODEV)) {
		status = 0;
	}

	/* For each node in the nodemask, traverse its p2p list
	 * and send rmnode_msg to those nodes 1) That are not also
	 * in the node mask and 2) That have not been already sent
	 * rmnode messages in this transaction and 3) That have
	 * their disconnection state as RUNNING.
	 */
	for (i = 0; i <= ms_info.mi_maxid; i++) {
		/* verify if the node is present in deactivation set */
		if (!get_nodemask_bit(nodemask, i))
			continue;

		/* Get to the p2p list of this node */
		dev = &scif_dev[i];
		list_for_each_safe(pos, tmp, &dev->sd_p2p) {
			p2p = list_entry(pos, struct scif_p2p_info, 
			ppi_list);

			if (get_nodemask_bit(nodemask, p2p->ppi_peer_id))
				continue;
			if (p2p->ppi_disc_state == SCIFDEV_SLEEPING)
				continue;

			if(micscif_rmnode_msg_sent(node_list, p2p->ppi_peer_id))
				continue;
			send_rmnode_ctx = get_per_dev_ctx(p2p->ppi_peer_id - 1);
			if (!send_rmnode_ctx->micpm_ctx.nodemask.va) {
				status = -EINVAL;
				goto list_cleanup;
			}

			memcpy(send_rmnode_ctx->micpm_ctx.nodemask.va, nodemask, 
				send_rmnode_ctx->micpm_ctx.nodemask.len);
			ret = (int) micscif_send_pm_rmnode_msg(p2p->ppi_peer_id,
				send_rmnode_ctx->micpm_ctx.nodemask.pa,
				send_rmnode_ctx->micpm_ctx.nodemask.len,node_id);

			/* Add this node to msg list. */
			if(!ret) {
				msg_cnt++;
				stack_push_node(node_list, p2p->ppi_peer_id);
			}

			if((ret == 0)||(ret == -ENODEV)) {
				status = 0;
			}
		}
	}

	ret = wait_event_timeout(ms_info.mi_disconn_wq, 
		(atomic_read(&mic_ctx->disconn_rescnt) == msg_cnt) ||
		(pending_wakeups = atomic_read(&mic_data.dd_pm.wakeup_in_progress)), 
		NODE_ALIVE_TIMEOUT);
	if ((!ret) || (atomic_read(&mic_ctx->disconn_rescnt) != msg_cnt)
		|| (ms_info.mi_disconnect_status == OP_FAILED)) {
		pr_debug("SCIF disconnect failed.  "
			"remove_node messages sent: = %llu "
			"remove_node acks received: %d "
			"Pending wakeups: %d ret = %d\n", msg_cnt, 
			atomic_read(&mic_ctx->disconn_rescnt), 
			pending_wakeups, ret);

		status = -EAGAIN;
		goto list_cleanup;
	}
	return status;

list_cleanup:
	while (!is_stack_empty(node_list))
		stack_pop_node(node_list, &node);
	return status;
}

/**
 * micscif_node_disconnect:
 *
 * @node_id[in]: source node id.
 * @nodemask[out]: bitmask of nodes that have to be disconnected together.
 *                 it represents node_id
 * @disconn_type[in]: flag to identify disconnection type. (for example - power mgmt, lost node, maintenance mode etc)
 *
 * Method responsible for disconnecting node from the scif network. considers dependencies with other node.
 * finds out deactivation set. Sends node queue pair messages to all the scif nodes outside deactivation set
 * returns error if node can not be disconnected from the network.
 */
int micscif_disconnect_node(uint32_t node_id, uint8_t *nodemask, enum disconn_type type)
{
	uint32_t status = 0;
	int ret;
	uint64_t msg_cnt = 0;
	uint32_t i = 0;
	mic_ctx_t *mic_ctx = 0;
	struct list_head node_list;
	uint32_t node;

	if (!node_id)
		return -EINVAL;

	mic_ctx = get_per_dev_ctx(node_id - 1);

	if (!mic_ctx)
		return -EINVAL;

	switch(type) {
	case DISCONN_TYPE_POWER_MGMT:
	{
		if (!nodemask)
			return -EINVAL;

		atomic_long_add(1, &ms_info.mi_unique_msgid);
		atomic_set(&mic_ctx->disconn_rescnt, 0);
		ms_info.mi_disconnect_status = OP_IN_PROGRESS;
		INIT_LIST_HEAD(&node_list);

		status = micscif_execute_disconnect(node_id,
				nodemask, &node_list);
		if (status)
			return status;

		/* Reset unique msg_id */
		atomic_long_set(&ms_info.mi_unique_msgid, 0);

		while (!is_stack_empty(&node_list)) {
			status = stack_pop_node(&node_list, &node);
			if (status)
				break;

			for (i = 0; i <= ms_info.mi_maxid; i++) {
				if (!get_nodemask_bit(nodemask, i))
					continue;
				micscif_update_p2p_state(i, node, SCIFDEV_SLEEPING);
			}
		}
		break;
	}
	case DISCONN_TYPE_LOST_NODE:
	{
		atomic_long_add(1, &ms_info.mi_unique_msgid);
		atomic_set(&mic_ctx->disconn_rescnt, 0);

		for (i = 0; ((i <= ms_info.mi_maxid) && (i != node_id)); i++) {
			ret = (int)micscif_send_lost_node_rmnode_msg(i, node_id);
			if(!ret)
				msg_cnt++;
			if((ret == 0)||(ret == -ENODEV)) {
				status = 0;
			}
		}

		ret = wait_event_timeout(ms_info.mi_disconn_wq, 
			(atomic_read(&mic_ctx->disconn_rescnt) == msg_cnt), 
			NODE_ALIVE_TIMEOUT);
		break;
	}
	default:
		status = -EINVAL;
	}

	return status;
}

/**
 * micscif_node_connect:
 *
 * @node_id[in]: node to wakeup.
 * @bool get_ref[in]: Also get node reference after wakeup by incrementing the PM reference count
 *
 * Method responsible for connecting node into the scif network. considers dependencies with other node.
 * finds out activation set. connects all the depenendent nodes in the activation set
 * returns error if node can not be connected from the network.
 */
int
micscif_connect_node(uint32_t node_id, bool get_ref)
{
	return do_idlestate_exit(get_per_dev_ctx(node_id - 1), get_ref);
}

uint64_t micscif_send_node_alive(int node)
{
	struct nodemsg alive_msg;
	struct micscif_dev *dev = &scif_dev[node];
	int err;

	alive_msg.uop = SCIF_NODE_ALIVE;
	alive_msg.src.node = ms_info.mi_nodeid;
	alive_msg.dst.node = node;
	pr_debug("node alive msg sent to node %d\n", node);
	micscif_inc_node_refcnt(dev, 1);
	err = micscif_nodeqp_send(dev, &alive_msg, NULL);
	micscif_dec_node_refcnt(dev, 1);
	return err;
}

int micscif_handle_lostnode(uint32_t node_id)
{
	mic_ctx_t *mic_ctx;
	uint32_t status = -EOPNOTSUPP;
#ifdef MM_HANDLER_ENABLE
	uint8_t *mmio_va;
	sbox_scratch1_reg_t scratch1reg = {0};
#endif

	printk("%s %d node %d\n", __func__, __LINE__, node_id);
	mic_ctx = get_per_dev_ctx(node_id - 1);

	if (mic_ctx->state != MIC_ONLINE && mic_ctx->state != MIC_SHUTDOWN)
		return 0;

	if (mic_crash_dump_enabled) {
		if (!(status = vmcore_create(mic_ctx)))
			printk("%s %d node %d ready for crash dump!\n", 
				__func__, __LINE__, node_id);
		else
			printk(KERN_ERR "%s %d node %d crash dump failed status %d\n", 
				__func__, __LINE__, node_id, status);
	}

	mic_ctx->crash_count++;
	mutex_lock(&mic_ctx->state_lock);
	if (mic_ctx->state == MIC_ONLINE ||
		mic_ctx->state == MIC_SHUTDOWN)
		mic_setstate(mic_ctx, MIC_LOST);
	mutex_unlock(&mic_ctx->state_lock);

	/* mpssd will handle core dump and reset/auto reboot */
	if (mic_crash_dump_enabled && !status)
		return status;

	printk("%s %d stopping node %d to recover lost node!\n", 
		__func__, __LINE__, node_id);
	status = adapter_stop_device(mic_ctx, 1, !RESET_REATTEMPT);
	wait_for_reset(mic_ctx);

	if (!ms_info.mi_watchdog_auto_reboot) {
		printk("%s %d cannot boot node %d to recover lost node since auto_reboot is off\n", 
		__func__, __LINE__, node_id);
		return status;
	}

/* Disabling MM handler invocation till it is ready to handle errors
 * till then we just reboot the card
 */
#ifdef MM_HANDLER_ENABLE
	mmio_va = mic_ctx->mmio.va;
	scratch1reg.bits.status = FLASH_CMD_INVALID;

	if(mic_ctx->bi_family == FAMILY_ABR) {
		printk("Node %d lost. Cannot recover in KNF\n", node_id);
		status = adapter_start_device(mic_ctx);
		return status;
	}

	printk("Booting maintenance mode handler\n");
	status =  set_card_usage_mode(mic_ctx, USAGE_MODE_MAINTENANCE, NULL, 0);
	if(status) {
		printk("Unable to boot maintenance mode\n");
		return status;
	}

	status = send_flash_cmd(mic_ctx, RAS_CMD, NULL, 0);
	if(status) {
		printk("Unable to recover node\n");
		return status;
	}
	while(scratch1reg.bits.status != FLASH_CMD_COMPLETED) {
		ret = SBOX_READ(mmio_va, SBOX_SCRATCH1);
		scratch1reg.value = ret;
		msleep(1);
		i++;
		printk("Looping for status (time = %d ms)\n", i);
		if(i > NODE_ALIVE_TIMEOUT) {
			status = -ETIME;
			printk("Unable to recover node. Status bit is : %d\n", 
					scratch1reg.bits.status);
			return status;
		}

	}
#endif
	printk("%s %d booting node %d to recover lost node!\n", 
		__func__, __LINE__, node_id);
	status = adapter_start_device(mic_ctx);
	return status;
}

void micscif_watchdog_handler(struct work_struct *work)
{
	struct micscif_dev *dev =
		container_of(to_delayed_work(work), 
				struct micscif_dev, sd_watchdog_work);
	struct _mic_ctx_t *mic_ctx;
	int i = dev->sd_node, err, ret;

	mic_ctx = get_per_dev_ctx(i - 1);

	switch (mic_ctx->sdbic1) {
	case SYSTEM_HALT:
	case SYSTEM_POWER_OFF:
	{
		adapter_stop_device(mic_ctx, 1, !RESET_REATTEMPT);
		wait_for_reset(mic_ctx);
		mic_ctx->sdbic1 = 0;
		break;
	}
	case SYSTEM_RESTART:
	{
		mic_setstate(mic_ctx, MIC_LOST);
		mic_ctx->sdbic1 = 0;
		break;
	}
	case SYSTEM_BOOTING:
	case SYSTEM_RUNNING:
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0))
	case SYSTEM_SUSPEND_DISK:
#endif
		break;
	case 0xdead:
		if (mic_crash_dump_enabled)
			micscif_handle_lostnode(i);
		mic_ctx->sdbic1 = 0;
		break;
	default:
		break;
	}

	switch (mic_ctx->state) {
	case MIC_ONLINE:
		break;
	case MIC_BOOT:
		goto restart_timer;
	case MIC_SHUTDOWN:
	case MIC_LOST:
	case MIC_READY:
	case MIC_NORESPONSE:
	case MIC_BOOTFAIL:
	case MIC_RESET:
	case MIC_RESETFAIL:
	case MIC_INVALID:
		return;
	}

	if (!ms_info.mi_watchdog_enabled)
		return;

	err = micpm_get_reference(mic_ctx, false);
	if (err == -EAGAIN) {
		goto restart_timer;
	} else if (err == -ENODEV) {
		micscif_handle_lostnode(i);
		goto restart_timer;
	}

	if (1 != atomic_cmpxchg(&dev->sd_node_alive, 1, 0)) {

		err = (int)(micscif_send_node_alive(i));

		if (err) {
			micpm_put_reference(mic_ctx);
			goto restart_timer;
		}

		ret = wait_event_timeout(dev->sd_watchdog_wq, 
				(atomic_cmpxchg(&dev->sd_node_alive, 1, 0) == 1), 
				NODE_ALIVE_TIMEOUT);
		if (!ret || err)
			micscif_handle_lostnode(i);
	}
	micpm_put_reference(mic_ctx);

restart_timer:
	if (dev->sd_ln_wq)
		queue_delayed_work(dev->sd_ln_wq, 
			&dev->sd_watchdog_work, NODE_ALIVE_TIMEOUT);
}
#else

long micscif_suspend(uint8_t* nodemask) {
	long ret = 0;
	int i;
	struct micscif_dev *dev;

	for (i = 0; i <= ms_info.mi_maxid; i++) {
		if (get_nodemask_bit(nodemask , i)) {
			dev = &scif_dev[i];
			if (SCIFDEV_RUNNING != dev->sd_state)
				continue;

			ret = atomic_long_cmpxchg(
				&dev->scif_ref_cnt, 0, SCIF_NODE_IDLE);
			if (!ret || ret == SCIF_NODE_IDLE) {
				dev->sd_state = SCIFDEV_SLEEPING;
				ret = 0;
			}
			else {
				set_nodemask_bit(nodemask, i, 0);
				ret = EAGAIN;
			}
		}
	}
	return ret;
}
/*
 * scif_suspend_handler - SCIF tasks before transition to low power state.
 */
int micscif_suspend_handler(struct notifier_block *this,
		unsigned long event, void *ptr)
{
	int ret = 0;
#ifdef SCIF_ENABLE_PM
	int node = 0;
	int size;
	uint8_t *nodemask_buf;

	size = ((int) ((ms_info.mi_maxid + 1) / 8) +
			(((ms_info.mi_maxid + 1) % 8) ? 1 : 0));
	nodemask_buf = (uint8_t*)kzalloc(size, GFP_ATOMIC);
	if(!nodemask_buf)
		return -ENOMEM;

	for (node = 0; node <= ms_info.mi_maxid; node++) {
		if ((node != SCIF_HOST_NODE) && (node != ms_info.mi_nodeid))
			set_nodemask_bit(nodemask_buf, node, 1);
	}

	if (micscif_suspend(nodemask_buf)){
		ret = -EBUSY;
		goto clean_up;
	}

	dma_suspend(mic_dma_handle);
clean_up:
	kfree(nodemask_buf);
#endif
	return ret;
}

/*
 * micscif_resume_handler - SCIF tasks after wake up from low power state.
 */
int micscif_resume_handler(struct notifier_block *this,
		unsigned long event, void *ptr)
{
#ifdef SCIF_ENABLE_PM
#ifdef _MIC_SCIF_
	queue_work(ms_info.mi_misc_wq, &ms_info.mi_misc_work);
#endif
	dma_resume(mic_dma_handle);
#endif
	return 0;
}

/*
 * scif_fail_suspend_handler - SCIF tasks if a previous scif_suspend call has
 * failed since a low power state transition could not be completed.
 */
int micscif_fail_suspend_handler(struct notifier_block *this,
		unsigned long event, void *ptr)
{
/* Stub out function since it is an optimization that isn't working properly */
#if 0
#ifdef SCIF_ENABLE_PM
	int node = 0;
	long ret;
	struct micscif_dev *dev;

	for (node = 0; node <= ms_info.mi_maxid; node++) {
		dev = &scif_dev[node];
		ret = atomic_long_cmpxchg(&dev->scif_ref_cnt, SCIF_NODE_IDLE, 0);
		if (ret != SCIF_NODE_IDLE)
			continue;
		if (SCIFDEV_SLEEPING == dev->sd_state)
			dev->sd_state = SCIFDEV_RUNNING;
	}
#endif
#endif
	return 0;
}

void micscif_get_node_info(void)
{
	struct nodemsg msg;
	struct get_node_info node_info;

	init_waitqueue_head(&node_info.wq);
	node_info.state = OP_IN_PROGRESS;
	micscif_inc_node_refcnt(&scif_dev[SCIF_HOST_NODE], 1);
	msg.uop = SCIF_GET_NODE_INFO;
	msg.src.node = ms_info.mi_nodeid;
	msg.dst.node = SCIF_HOST_NODE;
	msg.payload[3] = (uint64_t)&node_info;

	if ((micscif_nodeqp_send(&scif_dev[SCIF_HOST_NODE], &msg, NULL)))
		goto done;

	wait_event(node_info.wq, node_info.state != OP_IN_PROGRESS);
done:
	micscif_dec_node_refcnt(&scif_dev[SCIF_HOST_NODE], 1);
	/* Synchronize with the thread waking us up */
	mutex_lock(&ms_info.mi_conflock);
	mutex_unlock(&ms_info.mi_conflock);
	;
}
#endif /* _MIC_SCIF_ */
