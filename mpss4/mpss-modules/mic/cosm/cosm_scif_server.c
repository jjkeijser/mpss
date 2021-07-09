/*
 * Intel MIC Platform Software Stack (MPSS)
 * Copyright (c) 2016, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * Intel MIC Coprocessor State Management (COSM) Driver.
 */

#include <linux/kthread.h>
#include <linux/sched/signal.h>
#include "cosm_main.h"

/*
 * The COSM driver uses SCIF to communicate between the management node and the
 * MIC cards. SCIF is used to (a) Send a shutdown command to the card (b)
 * receive a shutdown status back from the card upon completion of shutdown and
 * (c) receive periodic heartbeat messages from the card used to deduce if the
 * card has crashed.
 *
 * A COSM server consisting of a SCIF listening endpoint waits for incoming
 * connections from the card. Upon acceptance of the connection, a separate
 * work-item is scheduled to handle SCIF message processing for that card. The
 * life-time of this work-item is therefore the time from which the connection
 * from a card is accepted to the time at which the connection is closed. A new
 * work-item starts each time the card boots and is alive till the card (a)
 * shuts down (b) is reset (c) crashes (d) cosm_client driver on the card is
 * unloaded.
 *
 * From the point of view of COSM interactions with SCIF during card
 * shutdown, reset and crash are as follows:
 *
 * Card shutdown
 * -------------
 * 1. COSM client on the card invokes orderly_poweroff() in response to SHUTDOWN
 *    message from the host.
 * 2. Card driver shutdown callback invokes scif_unregister_device(..) resulting
 *    in scif_remove(..) getting called on the card
 * 3. scif_remove -> scif_stop -> scif_handle_remove_node ->
 *    scif_peer_unregister_device -> device_unregister for the host peer device
 * 4. During device_unregister remove(..) method of cosm_client is invoked which
 *    closes the COSM SCIF endpoint on the card. This results in a SCIF_DISCNCT
 *    message being sent to host SCIF. SCIF_DISCNCT message processing on the
 *    host SCIF sets the host COSM SCIF endpoint state to DISCONNECTED and wakes
 *    up the host COSM thread blocked in scif_poll(..) resulting in
 *    scif_poll(..)  returning POLLHUP.
 * 5. On the card, scif_peer_release_dev is next called which results in an
 *    SCIF_EXIT message being sent to the host and after receiving the
 *    SCIF_EXIT_ACK from the host the peer device teardown on the card is
 *    complete.
 * 6. As part of the SCIF_EXIT message processing on the host, host sends a
 *    SCIF_REMOVE_NODE to itself corresponding to the card being removed. This
 *    starts a similar SCIF peer device teardown sequence on the host
 *    corresponding to the card being shut down.
 *
 * Card reset
 * ----------
 * The case of interest here is when the card has not been previously shut down
 * since most of the steps below are skipped in that case:

 * 1. cosm_stop(..) invokes hw_ops->stop(..) method of the base PCIe driver
 *    which unregisters the SCIF HW device resulting in scif_remove(..) being
 *    called on the host.
 * 2. scif_remove(..) calls scif_disconnect_node(..) which results in a
 *    SCIF_EXIT message being sent to the card.
 * 3. The card executes scif_stop() as part of SCIF_EXIT message
 *    processing. This results in the COSM endpoint on the card being closed and
 *    the SCIF host peer device on the card getting unregistered similar to
 *    steps 3, 4 and 5 for the card shutdown case above. scif_poll(..) on the
 *    host returns POLLHUP as a result.
 * 4. On the host, card peer device unregister and SCIF HW remove(..) also
 *    subsequently complete.
 *
 * Card crash
 * ----------
 * If a reset is issued after the card has crashed, there is no SCIF_DISCNT
 * message from the card which would result in scif_poll(..) returning
 * POLLHUP. In this case when the host SCIF driver sends a SCIF_REMOVE_NODE
 * message to itself resulting in the card SCIF peer device being unregistered,
 * this results in a scif_peer_release_dev -> scif_cleanup_scifdev->
 * scif_invalidate_ep call sequence which sets the endpoint state to
 * DISCONNECTED and results in scif_poll(..) returning POLLHUP.
 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 17, 0)
#define getnstimeofday64 getnstimeofday
#define timespec64 timespec
#endif

#define COSM_SCIF_BACKLOG 16
#define COSM_HEARTBEAT_CHECK_DELTA_SEC 10
#define COSM_HEARTBEAT_TIMEOUT_SEC \
		(COSM_HEARTBEAT_SEND_SEC + COSM_HEARTBEAT_CHECK_DELTA_SEC)

static struct task_struct *server_thread;
static scif_epd_t listen_epd;

/* Store MIC card's shutdown status internally when it is received */
void cosm_status_handler(struct cosm_device *cdev, u8 state)
{
	const char *state_str = cosm_states[state].string;

	log_mic_info(cdev->index, "card state change '%s'", state_str);

	switch (state) {
	case MIC_STATE_ONLINE:
		cdev->current_command.received_online = true;
		break;

	case MIC_STATE_RESETTING:
		cosm_set_command(cdev, MIC_CMD_RESET_FROM_CARD);
		break;

	case MIC_STATE_SHUTTING_DOWN:
		cosm_set_command(cdev, MIC_CMD_SHUTDOWN_FROM_CARD);
		break;

	case MIC_STATE_ERROR:
		cosm_set_command(cdev, MIC_CMD_HANDLE_PANIC);
		break;

	default:
		log_mic_warn(cdev->index, "unsupported state: %s", state_str);
		break;
	}
}

/*
 * Set card state to ONLINE when a new SCIF connection from a MIC card is
 * received. Normally the state is BOOTING when the connection comes in, but can
 * be ONLINE if cosm_client driver on the card was unloaded and then reloaded.
 */

/* Non-blocking recv. Read and process all available messages */
static int cosm_scif_recv(struct cosm_device *cdev)
{
	struct cosm_msg msg;
	int rc;

	while (1) {
		rc = scif_recv(cdev->epd, &msg, sizeof(msg), 0);
		if (!rc)
			break;

		if (rc < 0) {
			log_mic_dbg(cdev->index, "rc %d", rc);
			break;
		}

		log_mic_dbg(cdev->index, "rc %d id 0x%llx", rc, msg.id);

		switch (msg.id) {
		case COSM_MSG_STATE_CHANGE:
			cosm_status_handler(cdev, msg.state);
			break;
		case COSM_MSG_HEARTBEAT:
			/* Nothing to do, heartbeat only unblocks scif_poll */
			break;
		default:
			log_mic_err(cdev->index, "unknown msg.id %lld", msg.id);
			break;
		}

		if (msg.state == MIC_STATE_ERROR)
			return 1;
	}
	return 0;
}

/* Send host time to the MIC card to sync system time between host and MIC */
static int cosm_send_time(struct cosm_device *cdev)
{
	struct cosm_msg msg = { .id = COSM_MSG_SYNC_TIME };
	int rc;

	getnstimeofday64(&msg.timespec);
	rc = scif_send(cdev->epd, &msg, sizeof(msg), SCIF_SEND_BLOCK);
	if (rc < 0) {
		log_mic_err(cdev->index, "scif_send failed rc %d", rc);
		return rc;
	}
	return 0;
}

/*
 * Work function for handling work for a SCIF connection from a particular MIC
 * card. It first sets the card state to ONLINE and then calls scif_poll to
 * block on activity such as incoming messages on the SCIF endpoint. When the
 * endpoint is closed, the work function exits, completing its life cycle, from
 * MIC card boot to card shutdown/reset/crash.
 */
void cosm_scif_work(struct work_struct *work)
{
	struct cosm_device *cdev = container_of(work, struct cosm_device,
						scif_work);
	struct scif_pollepd pollepd;
	int rc;

	log_mic_info(cdev->index, "starting");

	if (cosm_send_time(cdev))
		goto exit;

	while (true) {
		pollepd.epd = cdev->epd;
		pollepd.events = POLLIN;

		rc = scif_poll(&pollepd, 1, COSM_HEARTBEAT_TIMEOUT_SEC * MSEC_PER_SEC);
		if (rc < 0) {
			log_mic_err(cdev->index, "scif_poll rc %d", rc);
			continue;
		}

		/* There is a message from the card */
		if (pollepd.revents & POLLIN) {
			if (cosm_scif_recv(cdev))
				break;
		}

		/* The peer endpoint is closed or this endpoint disconnected */
		if (pollepd.revents & (POLLHUP | POLLERR)) {
			log_mic_info(cdev->index, "POLLHUP or POLLERR");
			break;
		}

		if (rc == 0) {
			cosm_set_command(cdev, MIC_CMD_HANDLE_HANG);
			break;
		}
	}
exit:
	mutex_lock(&cdev->epd_mutex);
	scif_close(cdev->epd);
	cdev->epd = NULL;
	mutex_unlock(&cdev->epd_mutex);

	log_mic_info(cdev->index, "exiting");
}

/*
 * COSM SCIF server thread function. Accepts incoming SCIF connections from MIC
 * cards, finds the correct cosm_device to associate that connection with and
 * schedules individual work items for each MIC card.
 */
static int cosm_scif_server(void *unused)
{
	struct cosm_device *cdev;
	scif_epd_t newepd = NULL;
	struct scif_port_id port_id;
	int rc;

	allow_signal(SIGKILL);

	while (!kthread_should_stop()) {
		rc = scif_accept(listen_epd, &port_id, &newepd,
				 SCIF_ACCEPT_SYNC);
		if (rc != 0) {
			if (rc != -EINTR)
				pr_err("cosm server accept error %d", rc);
			continue;
		}

		/*
		 * Associate the incoming connection with a particular
		 * cosm_device, COSM device ID == SCIF node ID - 1
		 */
		cdev = cosm_find_cdev_by_id(port_id.node - 1);

		if (cdev) {
			log_mic_info(cdev->index, "accepted new epd: 0x%p",
				     newepd);
			flush_work(&cdev->scif_work);
			mutex_lock(&cdev->epd_mutex);
			if (!cdev->epd) {
				cdev->epd = newepd;
				newepd = NULL;
				schedule_work(&cdev->scif_work);
			} else {
				log_mic_err(cdev->index,
					    "cdev epd still exists: 0x%p",
					    cdev->epd);
			}
			mutex_unlock(&cdev->epd_mutex);

			/* Drop reference acquired by cosm_find_cdev_by_id */
			put_device(&cdev->dev);
		}

		if (newepd)
			scif_close(newepd);
	}

	pr_info("cosm server thread stopped");
	return 0;
}

static int cosm_scif_listen(void)
{
	int rc;

	listen_epd = scif_open();
	if (!listen_epd) {
		pr_err("cosm server: scif_open failed");
		return -ENOMEM;
	}

	rc = scif_bind(listen_epd, SCIF_COSM_LISTEN_PORT);
	if (rc < 0) {
		pr_err("cosm server bind error %d", rc);
		goto err;
	}

	rc = scif_listen(listen_epd, COSM_SCIF_BACKLOG);
	if (rc < 0) {
		pr_err("cosm server listen error %d", rc);
		goto err;
	}
	return 0;
err:
	scif_close(listen_epd);
	listen_epd = NULL;
	return rc;
}

static void cosm_scif_listen_exit(void)
{
        pr_info("cosm server closing listen_epd");
	if (listen_epd) {
		scif_close(listen_epd);
		listen_epd = NULL;
	}
}

/*
 * Create a listening SCIF endpoint and a server kthread which accepts incoming
 * SCIF connections from MIC cards
 */
int cosm_scif_init(void)
{
	int rc = cosm_scif_listen();
	if (rc) {
		pr_err("cosm server cosm_scif_listen error %d", rc);
		goto err;
	}

	server_thread = kthread_run(cosm_scif_server, NULL, "cosm_server");
	if (IS_ERR(server_thread)) {
		rc = PTR_ERR(server_thread);
		pr_err("cosm server kthread_run error %d", rc);
		goto listen_exit;
	}
	return 0;
listen_exit:
	cosm_scif_listen_exit();
err:
	return rc;
}

/* Stop the running server thread and close the listening SCIF endpoint */
void cosm_scif_exit(void)
{
	int rc;

	if (!IS_ERR_OR_NULL(server_thread)) {
		rc = send_sig(SIGKILL, server_thread, 0);
		if (rc) {
			pr_err("cosm server send_sig error %d", rc);
			return;
		}
		kthread_stop(server_thread);
	}

	cosm_scif_listen_exit();
}
