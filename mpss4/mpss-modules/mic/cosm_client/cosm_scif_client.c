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
 * Intel MIC COSM Client Driver.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <linux/seq_file.h>
#include <linux/kthread.h>
#include <linux/version.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/fs.h>
#include <linux/string.h>
#include "../cosm/cosm_main.h"

#define COSM_SCIF_MAX_RETRIES 10
#define COSM_HEARTBEAT_SEND_MSEC (COSM_HEARTBEAT_SEND_SEC * MSEC_PER_SEC/2)

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 19, 0)
#define do_settimeofday64 do_settimeofday
#define timespec64 timespec
#endif

static struct task_struct *client_thread;
static scif_epd_t client_epd;
static struct kobject *cosm_sysfs_obj;
static bool status_triggered;
static volatile bool connected;

/*
 * Reboot notifier: receives shutdown status from the OS and communicates it
 * back to the COSM process on the host
 */
static int cosm_reboot_event(struct notifier_block *this, unsigned long event,
			     void *ptr)
{
	struct cosm_msg msg = { .id = COSM_MSG_STATE_CHANGE };
	int rc;

	switch (event) {
	case SYS_RESTART:
		msg.state = MIC_STATE_RESETTING;
		break;
	case SYS_HALT:
	case SYS_POWER_OFF:
		msg.state = MIC_STATE_SHUTTING_DOWN;
		break;
	default:
		pr_warn("cosm client unsupported event (%lu)\n", event);
		msg.state = MIC_STATE_UNKNOWN;
	}

	log_mic_dbg(0, "state %d", msg.state);

	rc = scif_send(client_epd, &msg, sizeof(msg), SCIF_SEND_BLOCK);
	if (rc < 0)
		pr_err("cosm client reboot send error %d\n", rc);

	return NOTIFY_DONE;
}

static int card_panic_event(struct notifier_block *this, unsigned long event,
				void *ptr)
{
	struct cosm_msg msg = { .id = COSM_MSG_STATE_CHANGE };
	int rc;

	msg.state = MIC_STATE_ERROR;

	rc = scif_send(client_epd, &msg, sizeof(msg), SCIF_SEND_BLOCK);
	if (rc < 0)
		pr_err("cosm client panic send error %d\n", rc);

	return NOTIFY_DONE;
}

static struct notifier_block cosm_reboot = {
	.notifier_call  = cosm_reboot_event,
};

static struct notifier_block card_panic = {
	.notifier_call = card_panic_event,
};

/* Set system time from timespec value received from the host */
static void cosm_set_time(struct cosm_msg *msg)
{
	int rc = do_settimeofday64(&msg->timespec);

	if (rc)
		pr_err("cosm client settimeofday error %d\n", rc);
}

static int cosm_client_send_online(void)
{
	struct cosm_msg msg;
	int rc;

	msg.id = COSM_MSG_STATE_CHANGE;
	msg.state = MIC_STATE_ONLINE;

	rc = scif_send(client_epd, &msg, sizeof(msg), SCIF_SEND_BLOCK);
	if (rc < 0) {
		pr_err("cosm client online send error %d\n", rc);
		return rc;
	}

	return 0;
}

/* COSM client receive message processing */
static void cosm_client_recv(void)
{
	struct cosm_msg msg;
	int rc;

	while (1) {
		rc = scif_recv(client_epd, &msg, sizeof(msg), 0);
		if (!rc) {
			return;
		} else if (rc < 0) {
			pr_err("cosm client scif_recv error %d\n", rc);
			return;
		}

		pr_info("cosm client scif_recv %d id 0x%llx\n", rc, msg.id);

		switch (msg.id) {
		case COSM_MSG_SYNC_TIME:
			cosm_set_time(&msg);
			break;
		case COSM_MSG_SHUTDOWN:
			orderly_poweroff(true);
			break;
		case COSM_MSG_REBOOT:
			orderly_reboot();
			break;
		default:
			pr_err("cosm client unknown msg id %lld\n", msg.id);
			break;
		}
	}
}

/* Initiate connection to the COSM server on the host */
static int cosm_scif_connect(void)
{
	struct scif_port_id port_id;
	int i, err;

	if (client_epd) {
		pr_err("cosm client already connected\n");
		return -EBUSY;
	}

	client_epd = scif_open();
	if (!client_epd) {
		pr_err("cosm client scif_open failed\n");
		return -ENOMEM;
	}

	port_id.node = 0;
	port_id.port = SCIF_COSM_LISTEN_PORT;

	for (i = 0; i < COSM_SCIF_MAX_RETRIES; i++) {
		err = scif_connect(client_epd, &port_id);
		if (err) {
			pr_warn("cosm client scif_connect error %d\n", err);
			msleep(1000);
		} else {
			break;
		}
	}
	if (!err) {
		pr_info("cosm client connected to host\n");
		connected = true;
	} else {
		scif_close(client_epd);
		client_epd = NULL;
	}
	return err;
}

/* Close host SCIF connection */
static void cosm_scif_connect_exit(void)
{
	if (client_epd) {
		pr_info("cosm client closing the endpoint\n");
		scif_close(client_epd);
		client_epd = NULL;
	}
}

/*
 * COSM SCIF client thread function: waits for messages from the host and sends
 * a heartbeat to the host
 */
static int cosm_scif_client(void *unused)
{
	struct cosm_msg msg = { .id = COSM_MSG_HEARTBEAT };
	struct scif_pollepd pollepd;
	int rc;

	pr_info("cosm client thread started\n");
	allow_signal(SIGKILL);

	pollepd.epd = client_epd;
	pollepd.events = POLLIN;

	while (!kthread_should_stop()) {
		rc = scif_send(client_epd, &msg, sizeof(msg), SCIF_SEND_BLOCK);
		if (rc < 0) {
			pr_err("cosm client heartbeat send error %d\n", rc);

			/*
			* When the cosm scif server work exited due to the
			* card supposed hang (MIC_CMD_HANDLE_HANG state) this
			* thread should break.
			*/
			if (rc == -ECONNRESET)
				break;

			msleep(1000);
			continue;
		}

		rc = scif_poll(&pollepd, 1, COSM_HEARTBEAT_SEND_MSEC);
		if (rc < 0) {
			if (rc == -EINTR)
				continue;
			pr_err("cosm client scif_poll error %d, events 0x%x\n",
			       rc, pollepd.revents);
		}

		if (pollepd.revents & POLLIN)
			cosm_client_recv();
	}
	pr_info("cosm client thread stopped\n");
	return 0;
}

static void cosm_scif_probe(struct scif_peer_dev *spdev)
{
	int rc;

	pr_info("cosm client probe dnode %d\n", spdev->dnode);

	/* We are only interested in the host with spdev->dnode == 0 */
	if (spdev->dnode)
		return;

	rc = cosm_scif_connect();
	if (rc)
		return;

	rc = register_reboot_notifier(&cosm_reboot);
	if (rc) {
		pr_err("cosm client register_reboot_notifier error %d\n", rc);
		goto connect_exit;
	}

	rc = atomic_notifier_chain_register(&panic_notifier_list, &card_panic);
	if (rc) {
		pr_err("cosm client panic notifier failed: %d\n", rc);
		goto unreg_reboot;
	}

	client_thread = kthread_run(cosm_scif_client, NULL, "cosm_client");
	if (IS_ERR(client_thread)) {
		rc = PTR_ERR(client_thread);
		pr_err("cosm client kthread_run error %d\n", rc);
		goto unreg_panic;
	}
	return;
unreg_panic:
	atomic_notifier_chain_unregister(&panic_notifier_list, &card_panic);
unreg_reboot:
	unregister_reboot_notifier(&cosm_reboot);
connect_exit:
	cosm_scif_connect_exit();
}

static void cosm_scif_remove(struct scif_peer_dev *spdev)
{
	int rc;

	pr_info("cosm client removing dnode %d\n", spdev->dnode);

	if (spdev->dnode)
		return;

	if (!IS_ERR_OR_NULL(client_thread)) {
		rc = send_sig(SIGKILL, client_thread, 0);
		if (rc) {
			pr_err("cosm client send_sig error %d\n", rc);
			return;
		}
		kthread_stop(client_thread);
	}
	atomic_notifier_chain_unregister(&panic_notifier_list, &card_panic);
	unregister_reboot_notifier(&cosm_reboot);
	cosm_scif_connect_exit();
}

static struct scif_client scif_client_cosm = {
	.name = KBUILD_MODNAME,
	.probe = cosm_scif_probe,
	.remove = cosm_scif_remove,
};


static ssize_t cosm_client_status_store(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	int i;

	if (!sysfs_streq(buf, "online"))
		return -EINVAL;

	if (status_triggered)
		return count;

	status_triggered = true;

	for (i = 0; !connected && (i < COSM_SCIF_MAX_RETRIES); i++)
		msleep(1000);

	if (!connected) {
		pr_err("cosm client status store timeout\n");
		return -ENODEV;
	}

	if (cosm_client_send_online())
		return -EIO;

	return count;
}

static struct kobj_attribute cosm_client_status_attr =
	__ATTR_WO(cosm_client_status);

static int __init cosm_client_init(void)
{
	int rc;

	rc = scif_client_register(&scif_client_cosm);
	if (rc)
		pr_err("cosm client scif_client_register error %d\n", rc);

	cosm_sysfs_obj = kobject_create_and_add("mic", kernel_kobj);

	if (!cosm_sysfs_obj) {
		pr_err("failed to create cosm_client sysfs\n");
		rc = -ENOMEM;
		goto exit;
	}

	rc = sysfs_create_file(cosm_sysfs_obj, &cosm_client_status_attr.attr);
	if (rc)
		pr_err("failed to create cosm_client sysfs status entry\n");

exit:
	return rc;
}

static void __exit cosm_client_exit(void)
{
	scif_client_unregister(&scif_client_cosm);
	kobject_put(cosm_sysfs_obj);
}

module_init(cosm_client_init);
module_exit(cosm_client_exit);

MODULE_AUTHOR("Intel Corporation");
MODULE_DESCRIPTION("Intel(R) MIC Coprocessor State Management (COSM) client driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(MIC_VERSION);
