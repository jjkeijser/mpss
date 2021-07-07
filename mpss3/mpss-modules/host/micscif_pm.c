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

#include "mic_common.h"
#include "scif.h"
#include "mic/micscif.h"
#include "mic/mic_pm.h"
#include "mic/micveth.h"

extern int set_host_state(mic_ctx_t *mic_ctx, PM_IDLE_STATE state);
extern int pc6_entry_start(mic_ctx_t *mic_ctx);

/* Function that decrements the count of number of PM clients connected
 * to the host.
 */
void
micpm_decrement_clients(void)
{
	if(unlikely(atomic_dec_return(&mic_data.dd_pm.connected_clients) < 0)) {
		PM_DEBUG("connected_clients is negative (%d)\n",
			atomic_read(&mic_data.dd_pm.connected_clients));
	}
	return;
}

static char *pm_message_types[PM_MESSAGE_MAX+1] = {"PM_MESSAGE_PC3READY",
		"PM_MESSAGE_OPEN",
		"PM_MESSAGE_OPEN_ACK",
		"PM_MESSAGE_CLOSE",
		"PM_MESSAGE_CLOSE_ACK",
		"PM_MESSAGE_TEST",
		"PM_MESSAGE_MAX"};
void
micpm_display_message(mic_ctx_t *mic_ctx, void *header, void *msg, const char* label) {
	pm_msg_header *header_ref;
	int msg_len;
	int i=0;
	char *payload;
	scif_epd_t epd = mic_ctx->micpm_ctx.pm_epd;
	header_ref = (pm_msg_header *)header;
	msg_len = header_ref->len;

	if(!epd)
		return;

	if(0 <= header_ref->opcode && header_ref->opcode < PM_MESSAGE_MAX) {
		if(strcmp(label,"SENT")==0) {
			printk("%s: Msg type %s, SrcNode:SrcPort %d:%d, DestNode:DestPort %d:%d", label, 
					pm_message_types[header_ref->opcode], epd->port.node, epd->port.port, 
					epd->peer.node, epd->peer.port);
		}
		else
			printk("%s: Msg type %s, DestNode:DestPort %d:%d, SrcNode:SrcPort %d:%d", label, 
					pm_message_types[header_ref->opcode], epd->port.node, epd->port.port, 
					epd->peer.node, epd->peer.port);
	}


	if(msg != NULL) {
		payload = (char *)msg;
		printk(" Payload");
		for(i=0;i<msg_len;i++){
			printk("0x%02x:", payload[i]);
		}
	}
}

int micpm_update_pc6(mic_ctx_t *mic_ctx, bool set)
{

	int err = 0;
	if (mic_ctx->micpm_ctx.pm_options.pc6_enabled) {
		if (set && !mic_ctx->micpm_ctx.pc6_enabled) {
			mic_ctx->micpm_ctx.pc6_enabled = set;
			queue_delayed_work(mic_ctx->micpm_ctx.pc6_entry_wq, 
				&mic_ctx->micpm_ctx.pc6_entry_work, 
				mic_ctx->micpm_ctx.pc6_timeout*HZ);
		}
		if (set == false) {
			mic_ctx->micpm_ctx.pc6_enabled = set;
			micpm_get_reference(mic_ctx, true);
			micpm_put_reference(mic_ctx);
		}
	} else {
		if (set)
			err = -EINVAL;
		else
			mic_ctx->micpm_ctx.pc6_enabled = set;
	}
	return err;
}

int micpm_update_pc3(mic_ctx_t *mic_ctx, bool set)
{
	int err = 0;
	if (mic_ctx->micpm_ctx.pm_options.pc3_enabled) {
		if (set) {
			mic_ctx->micpm_ctx.pc3_enabled = set;
		} else {
			mic_ctx->micpm_ctx.pc3_enabled = set;
			micpm_get_reference(mic_ctx, true);
			micpm_put_reference(mic_ctx);
		}
	} else {
		if (set)
			err = -EINVAL;
		else
			mic_ctx->micpm_ctx.pc3_enabled = set;
	}
	return err;
}

/*
 * Wraper to scif_send that takes in the buffer to be sent
 * as input.
 */
int
mic_pm_send(mic_ctx_t *mic_ctx, void *msg, uint32_t len)
{
	int err;
	scif_epd_t epd;

	if(mic_ctx == NULL) {
		PM_DEBUG("Mic context not Initialized\n");
		return -EINVAL;
	}

	if((msg == NULL) || (len == 0)) {
		PM_DEBUG("Invalid Parameters\n");
		return -EINVAL;
	}

	epd = mic_ctx->micpm_ctx.pm_epd;
	if(epd == NULL) {
		PM_DEBUG("Scif Endpoint Undefined\n");
		return -EINVAL;
	}

	if ((mic_ctx->micpm_ctx.con_state != PM_CONNECTING) &&
		(mic_ctx->micpm_ctx.con_state != PM_CONNECTED)) {
		PM_DEBUG("Endpoint not in connected state\n");
		return -EINVAL;
	}

	err = scif_send(epd, msg, len, PM_SEND_MODE);
	/*scif_send returns the number of bytes returned on success */
	if(err <= 0) {
		PM_DEBUG("scif_send to node: %d port: %d failed with error %d\n",
				epd->peer.node, epd->peer.port, err);
	} else {
		PM_DEBUG("Bytes sent = %d\n",err);
		err = 0;
	}

	return err;
}

/*
 * Wrapper to scif_recv.
 */
int
mic_pm_recv(mic_ctx_t *mic_ctx, void *msg, uint32_t len)
{
	int err;
	scif_epd_t epd;

	if(mic_ctx == NULL) {
		PM_DEBUG("Mic context not Initialized\n");
		return -EINVAL;
	}

	if((msg == NULL) || (len == 0)) {
		PM_DEBUG("Invalid Parameters\n");
		return -EINVAL;
	}

	epd = mic_ctx->micpm_ctx.pm_epd;
	if(epd == NULL) {
		PM_DEBUG("Scif Endpoint Undefined\n");
		return -EINVAL;
	}

	if ((mic_ctx->micpm_ctx.con_state != PM_CONNECTING) &&
		(mic_ctx->micpm_ctx.con_state != PM_CONNECTED)) {
		PM_DEBUG("Endpoint not in connected state\n");
		return -EINVAL;
	}

	err = scif_recv(epd, msg, len, PM_RECV_MODE);

	if(err <= 0) {
		pr_debug("scif_recv failed with error %d\n", err);
		if(err == 0) {
			/*0 bytes were sent */
			err = -ENXIO;
		}
	} else {
		PM_DEBUG("Bytes received = %d\n",err);
		err = 0;
	}
	return err;
}

/*
 * Function to send a Power Management message over scif. Gets the message type
 * as input and builds a message header. It then creates a single message buffer
 * with this header and body and sends it to the receiving node.
 */
int
mic_pm_send_msg(mic_ctx_t *mic_ctx, PM_MESSAGE type, void *msg, uint32_t len)
{
	pm_msg_header header;
	char *send_msg = NULL;
	int err = 0;

	header.opcode = type;
	header.len = len;

	send_msg = kmalloc(len + sizeof(pm_msg_header), GFP_KERNEL);
	if(send_msg == NULL) {
		PM_DEBUG("error allocating memory");
		err = -ENOMEM;
		return err;
	}
	memcpy(send_msg , &header, sizeof(pm_msg_header));
	if((len != 0) && (msg != NULL)) {
		memcpy((send_msg + sizeof(pm_msg_header)), msg, len);
	}

	if(mic_data.dd_pm.enable_pm_logging) {
		if((len != 0) && (msg != NULL))
			micpm_display_message(mic_ctx,send_msg,send_msg+sizeof(pm_msg_header),"SENT");
		else
			micpm_display_message(mic_ctx,send_msg,NULL,"SENT");
	}
	err = mic_pm_send(mic_ctx, send_msg, len + sizeof(pm_msg_header));
	kfree(send_msg);
	return err;
}

/*
 * Handler invoked when receiving a PC3 ready message.
 */
int
handle_pc3_ready(mic_ctx_t *mic_ctx)
{
	int err = 0;
	PM_ENTRY;
	err = pm_pc3_entry(mic_ctx);
	PM_EXIT;
	return err;
}

/*
 * Handler invoked when receiving the latency response message
 */
int
handle_open_ack(mic_ctx_t *mic_ctx, pm_msg_pm_options *msg)
{
	int err = 0;
	PM_ENTRY;

	if ((mic_ctx == NULL) || (msg == NULL)) {
		err = EINVAL;
		goto inval;
	}

	if ((msg->version.major_version != PM_MAJOR_VERSION) ||
		(msg->version.minor_version != PM_MINOR_VERSION)) {
		printk(KERN_ERR "PM Driver version mismatch. "
			"Expected version: %d.%d Received version %d.%d\n", 
			PM_MAJOR_VERSION, PM_MINOR_VERSION, 
			msg->version.major_version, msg->version.minor_version);
		schedule_work(&mic_ctx->micpm_ctx.pm_close);
		goto inval;
	}

	mic_ctx->micpm_ctx.pm_options.pc3_enabled = msg->pc3_enabled;
	mic_ctx->micpm_ctx.pm_options.pc6_enabled = msg->pc6_enabled;

	mic_ctx->micpm_ctx.pc3_enabled =
			(mic_ctx->micpm_ctx.pm_options.pc3_enabled)? true : false;
	mic_ctx->micpm_ctx.pc6_enabled =
			(mic_ctx->micpm_ctx.pm_options.pc6_enabled)? true : false;

	mic_ctx->micpm_ctx.con_state = PM_CONNECTED;

inval:
	PM_EXIT;
	return err;
}

/*
 * Message handler invoked by the per device receive workqueue when it receives
 * a message from the device.
 */
int
mic_pm_handle_message(mic_ctx_t *mic_ctx, pm_recv_msg_t *recv_msg)
{
	int res = 0;

	if(mic_ctx == NULL) {
		return -EINVAL;
	}

	if(recv_msg == NULL) {
		PM_DEBUG("Undefined message\n");
		return -EINVAL;
	}

	switch(recv_msg->msg_header.opcode) {
	case PM_MESSAGE_PC3READY:
		res = handle_pc3_ready(mic_ctx);
		break;
	case PM_MESSAGE_OPEN_ACK:
		/*Size of the payload needs to be equal to what the
		 * host is trying to cast it to
		 */
		if (sizeof(pm_msg_pm_options) != recv_msg->msg_header.len) {
			printk(KERN_ERR "Incompatible PM message. Opcode = %d\n", 
				recv_msg->msg_header.opcode);
			return -EINVAL;
		}
		res = handle_open_ack(mic_ctx,
			((pm_msg_pm_options *) recv_msg->msg_body));
		break;
	default:
		printk(KERN_ERR "Unknown PM message. Opcode = %d\n", 
			recv_msg->msg_header.opcode);
		break;
	}
	return res;
}

/*
 * retrieve_msg:
 *
 * Retrieve message from the head of list.
 * @mic_ctx: The device context
 * Returns the retrieved message.
 */
pm_recv_msg_t *
pm_retrieve_msg(mic_ctx_t *mic_ctx) {

	pm_recv_msg_t *recv_msg = NULL;
	struct list_head *pos, *tmpq;
	bool msg_found = false;

	mutex_lock(&mic_ctx->micpm_ctx.msg_mutex);
	if (!list_empty_careful(&mic_ctx->micpm_ctx.msg_list))
	{
		list_for_each_safe(pos, tmpq, &mic_ctx->micpm_ctx.msg_list) {
			recv_msg = list_entry(pos, pm_recv_msg_t, msg);
			/*Do not touch the message if its a test message */
			if (recv_msg->msg_header.opcode != PM_MESSAGE_TEST) {
				list_del(&recv_msg->msg);
				msg_found = true;
				break;
			}
		}
	}

	if (msg_found == false)
		recv_msg = NULL;

	mutex_unlock(&mic_ctx->micpm_ctx.msg_mutex);
	return recv_msg;
}

/*
 * pm_process_msg_list:
 *
 * Process the message list of a node and handle each message in the list.
 * @mic_ctx[in]: The deive context whose message list is to be processed
 * Returns: None
 */
void
pm_process_msg_list(mic_ctx_t *mic_ctx) {

	pm_recv_msg_t *process_msg = NULL;
	int ret = 0;

	if(mic_ctx == NULL) {
		PM_DEBUG("Cannot get device handle \n");
		return;
	}

	while(!list_empty(&mic_ctx->micpm_ctx.msg_list)) {
		process_msg = pm_retrieve_msg(mic_ctx);
		if(!process_msg) {
			PM_DEBUG("No Message to process.\n");
			return;
		}

		ret = mic_pm_handle_message(mic_ctx, process_msg);
		if(ret) {
			PM_DEBUG("Power Management message not processed"
					" successfully.\n");
		}

		if(process_msg->msg_body != NULL) {
			kfree(process_msg->msg_body);
		}
		kfree(process_msg);
	}
}

/*
 * Retrieves each message from the message list and calls the handler
 * for the same. After the handler returns, the message is removed
 * from the list and deleted.
 */
static void
mic_pm_msg_handle_work(struct work_struct *msg_handle_work)
{
	pm_wq_t *pm_wq = container_of(msg_handle_work, pm_wq_t, work);
	micpm_ctx_t *pm_ctx = container_of(pm_wq, micpm_ctx_t, handle_msg);
	mic_ctx_t *mic_ctx = container_of(pm_ctx, mic_ctx_t, micpm_ctx);
	pm_process_msg_list(mic_ctx);
	return;
}

static void
pc6_entry_work(struct work_struct *work)
{
	int err;
	micpm_ctx_t *pm_ctx =
		container_of(to_delayed_work(work), 
		micpm_ctx_t, pc6_entry_work);
	mic_ctx_t *mic_ctx = container_of(pm_ctx, mic_ctx_t, micpm_ctx);

	err = pc6_entry_start(mic_ctx);
	if (err == -EAGAIN)
		queue_delayed_work(mic_ctx->micpm_ctx.pc6_entry_wq, 
			&mic_ctx->micpm_ctx.pc6_entry_work, 
			mic_ctx->micpm_ctx.pc6_timeout*HZ);
	return;
}

/*
 * Called when a device creates a PM connection to Host. There can be
 * only one PM connection between Host and a device. The function checks
 * for an existing connection and rejects this new request if present.
 */
static void
mic_pm_accept_work(struct work_struct *work)
{
	scif_epd_t newepd;
	struct scif_portID portID;
	int err;
	uint16_t i;
	mic_ctx_t *mic_ctx;
	mic_data_t *mic_data_p = &mic_data;

	PM_DEBUG("Accept thread waiting for new PM connections\n");
	err =  scif_accept(mic_data.dd_pm.epd, &portID, &newepd, SCIF_ACCEPT_SYNC);
	if (err == -EBUSY || err == -ENODEV) {
		PM_DEBUG("scif_accept error %d\n", err);
		goto continue_accepting;
	}
	else if (err < 0) {
		PM_DEBUG("scif_accept failed with errno %d\n", err);
		goto exit;

	}
	PM_DEBUG("Connection request received. \n");

	mutex_lock(&mic_data.dd_pm.pm_accept_mutex);

	if (newepd->peer.node == SCIF_HOST_NODE) {
		/* Reject connection request from HOST itself */
		PM_DEBUG("PM: Peer node cannot be HOST. Peer Node = %d Peer Port = %d",
				newepd->peer.node, newepd->peer.port);
		scif_close(newepd);
		mutex_unlock(&mic_data.dd_pm.pm_accept_mutex);
		goto continue_accepting;
	}

	/*Only one Power Management connection per node. */
	for (i = 0; i < mic_data_p->dd_numdevs; i++) {
		mic_ctx = get_per_dev_ctx(i);
		if (mic_ctx != NULL) {
			if (mic_ctx->micpm_ctx.pm_epd != NULL) {
				if (mic_ctx->micpm_ctx.pm_epd->peer.node == newepd->peer.node) {
					PM_DEBUG("There is already Power Management connection"
						    " established from this node. Rejecting request.\n");
					PM_DEBUG("Peer Node = %d, Peer Port = %d\n",
						    mic_ctx->micpm_ctx.pm_epd->peer.node,
						    mic_ctx->micpm_ctx.pm_epd->peer.port);
					scif_close(newepd);
					mutex_unlock(&mic_data.dd_pm.pm_accept_mutex);
					goto continue_accepting;
				}
			}
		}

	}
	mutex_unlock(&mic_data.dd_pm.pm_accept_mutex);
	mic_ctx = get_per_dev_ctx(newepd->peer.node -1);
	mic_ctx->micpm_ctx.pm_epd = newepd;
	micpm_start(mic_ctx);


continue_accepting:
	mutex_lock(&mic_data.dd_pm.pm_accept_mutex);
	queue_work(mic_data.dd_pm.accept.wq, 
		   &mic_data.dd_pm.accept.work);
	mutex_unlock(&mic_data.dd_pm.pm_accept_mutex);
exit:
	return;
}

/*
 * Work item function that waits for incoming PM messages from
 * a node. The function adds the message to a per device message
 * list that is later processed by the message handler.
 */
static void
mic_pm_recv_work(struct work_struct *recv_work)
{
	int err = 0;
	int size = 0;

	pm_wq_t *pm_wq = container_of(recv_work, pm_wq_t, work);
	micpm_ctx_t *pm_ctx = container_of(pm_wq, micpm_ctx_t, recv);
	mic_ctx_t *mic_ctx = container_of(pm_ctx, mic_ctx_t, micpm_ctx);
	pm_recv_msg_t *recv_msg = NULL;

	if (mic_ctx == NULL || pm_ctx == NULL) {
		PM_DEBUG("Error retrieving driver context \n");
		goto unqueue;
	}

	size = sizeof(pm_msg_header);
	recv_msg = (void *)kmalloc(sizeof(pm_recv_msg_t), GFP_KERNEL);

	if (recv_msg == NULL) {
		PM_DEBUG("Error allocating memory to save receive message.\n");
		goto unqueue;
	}
	INIT_LIST_HEAD(&recv_msg->msg);
	recv_msg->msg_body = NULL;

	/*Get the header */
	err = mic_pm_recv(mic_ctx, &recv_msg->msg_header, size);
	if (err < 0) {
		PM_DEBUG("Error in scif_recv while waiting for PM header message.\n");
		if (err == -ECONNRESET) {
			/*Remote node is not in a connected state. */
			schedule_work(&mic_ctx->micpm_ctx.pm_close);
		}
		goto unqueue;

	}

	if(recv_msg->msg_header.len != 0) {
		PM_DEBUG("Retrieving %d bytes of message body\n", recv_msg->msg_header.len);
		recv_msg->msg_body = (void *)kmalloc((sizeof(char) * recv_msg->msg_header.len), GFP_KERNEL);
		if (recv_msg->msg_body == NULL) {
			PM_DEBUG("Error allocating memory to receive PM Message\n");
			goto unqueue;
		}
		err = mic_pm_recv(mic_ctx, recv_msg->msg_body, recv_msg->msg_header.len);
		if (err < 0) {
			PM_DEBUG("Error in scif_recv while waiting for PM message body\n");
			if (err == -ECONNRESET) {
				/*Remote node is not in a connected state. */
				schedule_work(&mic_ctx->micpm_ctx.pm_close);
			}
			goto unqueue;
		}
	}

	if(mic_data.dd_pm.enable_pm_logging) {
		micpm_display_message(mic_ctx,&recv_msg->msg_header,
				recv_msg->msg_body,"RECV");
	}

	if ((recv_msg->msg_header.opcode != PM_MESSAGE_CLOSE) &&
			((recv_msg->msg_header.opcode != PM_MESSAGE_CLOSE_ACK))){
		PM_DEBUG("Adding received message from node %d to list.\n",
				mic_ctx->bi_id+1);
		mutex_lock(&mic_ctx->micpm_ctx.msg_mutex);
		list_add_tail(&recv_msg->msg , &mic_ctx->micpm_ctx.msg_list);
		mutex_unlock(&mic_ctx->micpm_ctx.msg_mutex);

		if(likely(recv_msg->msg_header.opcode != PM_MESSAGE_TEST)) {
			PM_DEBUG("Queue message handler work for node: %d\n",mic_ctx->bi_id+1);
			queue_work(mic_ctx->micpm_ctx.handle_msg.wq, 
					&mic_ctx->micpm_ctx.handle_msg.work);
		}

		queue_work(mic_ctx->micpm_ctx.recv.wq, 
			   &mic_ctx->micpm_ctx.recv.work);
	} else {

		if (recv_msg->msg_header.opcode == PM_MESSAGE_CLOSE) {
			mic_pm_send_msg(mic_ctx , PM_MESSAGE_CLOSE_ACK, NULL, 0);
			mic_ctx->micpm_ctx.con_state = PM_DISCONNECTING;
			schedule_work(&mic_ctx->micpm_ctx.pm_close);
		} else {
			mic_ctx->micpm_ctx.con_state = PM_DISCONNECTING;
			wake_up(&mic_ctx->micpm_ctx.disc_wq);
		}
		goto unqueue;
	}
	return;
unqueue:
	if (recv_msg) {
		if (recv_msg->msg_body)
			kfree(recv_msg->msg_body);
		kfree(recv_msg);
	}
	return;
}

/*
 * Work item to handle closing of PM end point to a device and all the
 * related receive workqueues.
 */
static void
mic_pm_close_work(struct work_struct *work)
{
	micpm_ctx_t *pm_ctx = container_of(work, micpm_ctx_t, pm_close);
	mic_ctx_t *mic_ctx = container_of(pm_ctx, mic_ctx_t, micpm_ctx);
	micpm_stop(mic_ctx);
	return;
}

static void
mic_pm_resume_work(struct work_struct *resume_work)
{
	int err;
	pm_wq_t *pm_wq = container_of(resume_work, pm_wq_t, work);
	micpm_ctx_t *pm_ctx = container_of(pm_wq, micpm_ctx_t, resume);
	mic_ctx_t *mic_ctx = container_of(pm_ctx, mic_ctx_t, micpm_ctx);

	if (mic_ctx != NULL) {
		err = pm_start_device(mic_ctx);
		if (err) {
			PM_DEBUG("Failed to start device %d after resume\n",
					mic_ctx->bi_id);
		}
	} else {
		PM_DEBUG("Error retrieving node context.\n");
	}
}

/* Create PM specific workqueues during driver probe.
 *
 * Receive workqueue will store the received message and kick-off
 * a message handler workqueue which will process them.
 *
 * Resume workqueue handles the task of booting uOS rduring
 * OSPM resume/restore phase.
 */
int
setup_pm_workqueues(mic_ctx_t *mic_ctx)
{
	int err = 0;

	if(!mic_ctx) {
		PM_DEBUG("Failed to retrieve device context\n");
		err = -EINVAL;
		goto err;
	}

	/* setup resume wq */
	snprintf(mic_ctx->micpm_ctx.resume.wq_name,
		sizeof(mic_ctx->micpm_ctx.resume.wq_name),
		 "PM_RESUME_WQ %d", mic_get_scifnode_id(mic_ctx));

	if (!(mic_ctx->micpm_ctx.resume.wq
		= __mic_create_singlethread_workqueue(
				mic_ctx->micpm_ctx.resume.wq_name))) {
		err = -ENOMEM;
		goto err;
	}

	/* Setup Receive wq */
	snprintf(mic_ctx->micpm_ctx.recv.wq_name,
			sizeof(mic_ctx->micpm_ctx.recv.wq_name),
			 "RECV_WORK_Q %d", mic_get_scifnode_id(mic_ctx));

	if (!(mic_ctx->micpm_ctx.recv.wq
			= __mic_create_singlethread_workqueue(
					mic_ctx->micpm_ctx.recv.wq_name))) {
		err = -ENOMEM;
		goto err;
	}

	/* Setup Msg handler wq */
	snprintf(mic_ctx->micpm_ctx.handle_msg.wq_name,
		sizeof(mic_ctx->micpm_ctx.handle_msg.wq_name),
		 "MSG_HANDLER_WQ %d", mic_get_scifnode_id(mic_ctx));

	if (!(mic_ctx->micpm_ctx.handle_msg.wq
			= __mic_create_singlethread_workqueue(
					mic_ctx->micpm_ctx.handle_msg.wq_name))) {
		err = -ENOMEM;
		goto err;
	}

	/* Setup pc6 entry wq */
	snprintf(mic_ctx->micpm_ctx.pc6_wq_name,
			sizeof(mic_ctx->micpm_ctx.pc6_wq_name),
			 "PC6_WORK_Q %d", mic_get_scifnode_id(mic_ctx));

	if (!(mic_ctx->micpm_ctx.pc6_entry_wq
		= __mic_create_singlethread_workqueue(
				mic_ctx->micpm_ctx.pc6_wq_name))) {
		err = -ENOMEM;
		goto err;
	}
	INIT_WORK(&mic_ctx->micpm_ctx.recv.work, mic_pm_recv_work);
	INIT_WORK(&mic_ctx->micpm_ctx.handle_msg.work, mic_pm_msg_handle_work);
	INIT_WORK(&mic_ctx->micpm_ctx.pm_close, mic_pm_close_work);
	INIT_WORK(&mic_ctx->micpm_ctx.resume.work, mic_pm_resume_work);
	INIT_DELAYED_WORK(&mic_ctx->micpm_ctx.pc6_entry_work, pc6_entry_work);

err:
	return err;
}
/*Power Management Initialization function. Sets up SCIF
 * end points and accept threads.
 */
int micpm_init()
{
	scif_epd_t epd;
	int con_port;
	int err = 0;

	epd = scif_open();
	if (epd == SCIF_OPEN_FAILED || epd == NULL) {
		PM_DEBUG("scif_open failed\n");
		return -1;
	}

	if ((con_port = scif_bind(epd, SCIF_PM_PORT_0)) < 0) {
		PM_DEBUG("scif_bind to port failed with error %d\n", con_port);
		err = con_port;
		goto exit_close;
	}

	/*No real upper limit on number of connections.
	Once scif_listen accepts 0 as an acceptable parameter for max
	connections(to mean tht there is no upper limit), change this. */
	if ((err = scif_listen(epd, 100)) < 0) {
		PM_DEBUG("Listen ioctl failed with error %d\n", err);
		goto exit_close;
	}
	mic_data.dd_pm.epd = epd;

	snprintf(mic_data.dd_pm.accept.wq_name,
			sizeof(mic_data.dd_pm.accept.wq_name),"PM ACCEPT");

	mic_data.dd_pm.accept.wq =
			__mic_create_singlethread_workqueue(mic_data.dd_pm.accept.wq_name);
	if (!mic_data.dd_pm.accept.wq){
		err = -ENOMEM;
		PM_DEBUG("create workqueue returned null\n");
		goto exit_close;
	}
	INIT_WORK(&mic_data.dd_pm.accept.work, mic_pm_accept_work);
	mutex_init (&mic_data.dd_pm.pm_accept_mutex);
	mutex_init (&mic_data.dd_pm.pm_idle_mutex);
	atomic_set(&mic_data.dd_pm.connected_clients, 0);

	/*Add work to the work queue */
	queue_work(mic_data.dd_pm.accept.wq, 
			&mic_data.dd_pm.accept.work);
	mic_data.dd_pm.enable_pm_logging = 0;
	atomic_set(&mic_data.dd_pm.wakeup_in_progress, 0);

	micpm_dbg_parent_init();

	return err;

exit_close:
	scif_close(epd);
	return err;
}

/*
 * Close the SCIF acceptor endpoint and uninit a lot of driver level
 * data structures including accept threads,
 */
void
micpm_uninit(void)
{
	int err;
	scif_epd_t epd = mic_data.dd_pm.epd;

	if(atomic_read(&mic_data.dd_pm.connected_clients) > 0) {
		PM_DEBUG("connected_clients is nonzero (%d)\n",
			atomic_read(&mic_data.dd_pm.connected_clients));
	}
	err = scif_close(epd);
	if (err != 0) {
		PM_DEBUG("Scif_close failed with error %d\n",err);
	}

	if (mic_data.dd_pm.accept.wq != NULL) {
		PM_DEBUG("Flushing accept workqueue\n");
		flush_workqueue(mic_data.dd_pm.accept.wq);
		destroy_workqueue(mic_data.dd_pm.accept.wq);
		mic_data.dd_pm.accept.wq = NULL;
	}

	mutex_destroy(&mic_data.dd_pm.pm_accept_mutex);
	mutex_destroy(&mic_data.dd_pm.pm_idle_mutex);

	debugfs_remove_recursive(mic_data.dd_pm.pmdbgparent_dir);

}

/*
 * Open the Per device Power Management context.
 */
int
micpm_probe(mic_ctx_t * mic_ctx) {

	int err = 0;

	mic_ctx->micpm_ctx.pm_epd = NULL;
	mic_ctx->micpm_ctx.idle_state = PM_IDLE_STATE_PC0;
	mic_ctx->micpm_ctx.recv.wq = NULL;
	mic_ctx->micpm_ctx.handle_msg.wq = NULL;
	mic_ctx->micpm_ctx.mic_suspend_state = MIC_RESET;
	mic_ctx->micpm_ctx.pc3_enabled = true;
	mic_ctx->micpm_ctx.pc6_enabled = true;
	mic_ctx->micpm_ctx.pm_options.pc3_enabled = 0;
	mic_ctx->micpm_ctx.pm_options.pc6_enabled = 0;

	if ((err = setup_pm_workqueues(mic_ctx)))
		goto err;

	mutex_init (&mic_ctx->micpm_ctx.msg_mutex);
	INIT_LIST_HEAD(&mic_ctx->micpm_ctx.msg_list);
	init_waitqueue_head(&mic_ctx->micpm_ctx.disc_wq);
	atomic_set(&mic_ctx->micpm_ctx.pm_ref_cnt, 0);
	mic_ctx->micpm_ctx.pc6_timeout = PC6_TIMER;

	/* create debugfs entries*/
	micpm_dbg_init(mic_ctx);

err:
	return err;
}

int
micpm_remove(mic_ctx_t * mic_ctx) {

	debugfs_remove_recursive(mic_ctx->micpm_ctx.pmdbg_dir);

	if (mic_ctx->micpm_ctx.resume.wq != NULL) {
		destroy_workqueue(mic_ctx->micpm_ctx.resume.wq);
		mic_ctx->micpm_ctx.resume.wq = NULL;
	}

	if(mic_ctx->micpm_ctx.pc6_entry_wq != NULL) {
		destroy_workqueue(mic_ctx->micpm_ctx.pc6_entry_wq);
		mic_ctx->micpm_ctx.pc6_entry_wq = NULL;
	}

	if(mic_ctx->micpm_ctx.recv.wq != NULL) {
		destroy_workqueue(mic_ctx->micpm_ctx.recv.wq);
		mic_ctx->micpm_ctx.recv.wq = NULL;
	}

	if(mic_ctx->micpm_ctx.handle_msg.wq != NULL) {
		destroy_workqueue(mic_ctx->micpm_ctx.handle_msg.wq);
		mic_ctx->micpm_ctx.handle_msg.wq = NULL;
	}

	micpm_nodemask_uninit(mic_ctx);

	mutex_destroy(&mic_ctx->micpm_ctx.msg_mutex);
	return 0;
}

int
micpm_start(mic_ctx_t *mic_ctx) {

	int ref_cnt;
	mic_ctx->micpm_ctx.con_state = PM_CONNECTING;

	/* queue receiver */
	queue_work(mic_ctx->micpm_ctx.recv.wq, 
			&mic_ctx->micpm_ctx.recv.work);

	atomic_inc(&mic_data.dd_pm.connected_clients);
	if ((ref_cnt = atomic_read(&mic_ctx->micpm_ctx.pm_ref_cnt)))
		printk("Warning: PM ref_cnt is non-zero during start. "
				"ref_cnt = %d PM features may not work as expected\n", 
				ref_cnt);
	mic_ctx->micpm_ctx.idle_state = PM_IDLE_STATE_PC0;
	set_host_state(mic_ctx, PM_IDLE_STATE_PC0);
	return mic_pm_send_msg(mic_ctx , PM_MESSAGE_OPEN, NULL, 0);
}

/*
 * Close the per device Power management context here.
 * It does various things such as: closing scif endpoints,
 * delete pending work items and wait for those that are
 * executing to complete, delete pending messages in the
 * message list, delete pending timers and wait for runnig
 * timers to complete. The function can block.
 */
int
micpm_stop(mic_ctx_t *mic_ctx) {

	int err = 0;
	int node_lost = 0;
	if(mic_ctx == NULL) {
		PM_DEBUG("Mic context not Initialized\n");
		return -EINVAL;
	}

	if ((micpm_get_reference(mic_ctx, true))) {
		PM_DEBUG("get_reference failed. Node may be lost\n");
		node_lost = 1;
	}

	mutex_lock(&mic_data.dd_pm.pm_accept_mutex);
	if ((mic_ctx->micpm_ctx.con_state == PM_CONNECTED) &&
			(mic_ctx->state != MIC_LOST)) {
		if (!mic_pm_send_msg(mic_ctx, PM_MESSAGE_CLOSE, NULL, 0)) {
			err = wait_event_timeout(
				mic_ctx->micpm_ctx.disc_wq, 
				mic_ctx->micpm_ctx.con_state == PM_DISCONNECTING, 
				NODE_ALIVE_TIMEOUT);
			if (!err) {
				PM_DEBUG("Timed out waiting CLOSE ACK"
				" from node.\n");
			}
		}
	}

	if(mic_ctx->micpm_ctx.pm_epd != NULL) {
		PM_DEBUG("Power Management: Closing connection to"
				" node: %d port:%d\n", mic_ctx->micpm_ctx.pm_epd->peer.node,
				mic_ctx->micpm_ctx.pm_epd->peer.port);
		err = scif_close(mic_ctx->micpm_ctx.pm_epd);
		if(err!= 0)
			PM_DEBUG("Scif_close failed with error %d\n",err);
		mic_ctx->micpm_ctx.pm_epd = NULL;
		micpm_decrement_clients();
	}
	mic_ctx->micpm_ctx.con_state = PM_DISCONNECTED;
	mic_ctx->micpm_ctx.idle_state = PM_IDLE_STATE_PC0;
	flush_workqueue(mic_ctx->micpm_ctx.resume.wq);
	flush_workqueue(mic_ctx->micpm_ctx.recv.wq);
	flush_workqueue(mic_ctx->micpm_ctx.handle_msg.wq);
	cancel_delayed_work_sync(&mic_ctx->micpm_ctx.pc6_entry_work);

	/* Process messages in message queue */
	pm_process_msg_list(mic_ctx);

	if (!node_lost)
		micpm_put_reference(mic_ctx);
	mutex_unlock(&mic_data.dd_pm.pm_accept_mutex);
	return err;
}

/*
 * Function to load the uOS and start all the driver components
 * after a resume/restore operation
 */
int
pm_start_device(mic_ctx_t *mic_ctx)
{
	if (!mic_ctx) {
		PM_DEBUG("Error retreving driver context\n");
		return 0;
	}

	PM_DEBUG("Resume MIC device:%d\n", mic_ctx->bi_id);
	/* Make sure the Power reset during Resume/Restore is complete*/
	adapter_wait_reset(mic_ctx);
	wait_for_reset(mic_ctx);

	/*Perform software reset */
	adapter_reset(mic_ctx, RESET_WAIT, !RESET_REATTEMPT);
	wait_for_reset(mic_ctx);

	/* Boot uOS only if it was online before suspend */
	if (MIC_ONLINE == mic_ctx->micpm_ctx.mic_suspend_state) {
		if(adapter_start_device(mic_ctx)) {
			PM_DEBUG("booting uos... failed\n");
		}
	}

	return 0;
}

/*
 * Function to stop all the driver components and unload the uOS
 * during a suspend/hibernate operation
 */
int
pm_stop_device(mic_ctx_t *mic_ctx)
{
	if (!mic_ctx) {
		PM_DEBUG("Error retreving driver context\n");
		return 0;
	}

	mic_ctx->micpm_ctx.mic_suspend_state = mic_ctx->state;

	PM_DEBUG("Suspend MIC device:#%d\n", mic_ctx->bi_id);
	if (MIC_ONLINE == mic_ctx->micpm_ctx.mic_suspend_state) {
		adapter_shutdown_device(mic_ctx);
		if (!wait_for_shutdown_and_reset(mic_ctx)) {
			/* Shutdown failed. Fall back on forced reset */
			adapter_stop_device(mic_ctx, RESET_WAIT, !RESET_REATTEMPT);
			wait_for_reset(mic_ctx);
		}
	}
	else {
		/* If card is in any state but ONLINE, make sure card stops */
		adapter_stop_device(mic_ctx, RESET_WAIT, !RESET_REATTEMPT);
		wait_for_reset(mic_ctx);
	}

	mutex_lock(&mic_ctx->state_lock);
	mic_ctx->state = MIC_RESET;
	mutex_unlock(&mic_ctx->state_lock);
	return 0;
}
