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

#include <linux/string.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <micint.h>

#include <scif.h>
#include <mic_common.h>

#define ACPT_BACKLOG 120
#define ACPT_POLL_MS 2000

#define ACPT_BOOTED	  1
#define ACPT_BOOT_ACK	  2
#define ACPT_NACK_VERSION 3
#define ACPT_REQUEST_TIME 4
#define ACPT_TIME_DATA	  5

#define ACPT_VERSION	1

static acptboot_data_t *acptboot_data;


void acptboot_getconn(struct work_struct *work)
{
	mic_ctx_t *node_ctx;
	struct scif_portID data;
	scif_epd_t conn_epd;
	struct timespec tod;
	int proto;
	int version;
	int err;

	if ((err = scif_accept(acptboot_data->listen_epd, &data, &conn_epd,
						SCIF_ACCEPT_SYNC))) {
		pr_debug("ACPTBOOT: scif_accept_failed %d\n", err);
		return;

		//goto requeue_accept;
	}

	if (!data.node) {
		printk(KERN_ERR "ACPTBOOT: connect received from invalid dev %d\n", 
								-EINVAL);
		goto close_epd;
	}

	if ((err = scif_recv(conn_epd, &version, sizeof(version), SCIF_RECV_BLOCK)) != sizeof(version)) {
		printk(KERN_ERR "ACPTBOOT: failed to recieve version number err %d\n", err);
		goto close_epd;
	}

	if ((err = scif_recv(conn_epd, &proto, sizeof(proto), SCIF_RECV_BLOCK)) != sizeof(proto)) {
		printk(KERN_ERR "ACPTBOOT: failed to recieve proto id %d\n", err);
		goto close_epd;
	}

	switch (proto) {
	case ACPT_BOOTED:
		node_ctx = get_per_dev_ctx(data.node - 1);
		mic_setstate(node_ctx, MIC_ONLINE);
		node_ctx->boot_count++;

		proto = ACPT_BOOT_ACK;
		scif_send(conn_epd, &proto, sizeof(proto), SCIF_SEND_BLOCK);
		break;

	case ACPT_REQUEST_TIME:
		getnstimeofday(&tod);
		proto = ACPT_TIME_DATA;
		scif_send(conn_epd, &proto, sizeof(proto), SCIF_SEND_BLOCK);
		scif_send(conn_epd, &tod, sizeof(tod), SCIF_SEND_BLOCK);
		break;
	}

close_epd:
	if ((err = scif_close(conn_epd)))
		printk(KERN_ERR "ACPTBOOT: scif_close failed %d\n", err);

//requeue_accept:
	queue_work(acptboot_data->acptbootwq, &acptboot_data->acptbootwork);
}

void acptboot_exit(void)
{
	int err = 0;
	if (acptboot_data) {
		if (acptboot_data->listen_epd)
			if ((err = scif_close(acptboot_data->listen_epd)) < 0)
				pr_debug("scif_close failed %d\n", err);
		destroy_workqueue(acptboot_data->acptbootwq);

		kfree(acptboot_data);
	}
}

int
acptboot_init(void)
{
	int err, ret;

	acptboot_data = (acptboot_data_t *)kzalloc(sizeof(*acptboot_data), GFP_KERNEL);

	if (!acptboot_data) {
		printk(KERN_ERR "ACPTBOOT: memory allocation failure\n");
		return -ENOMEM;
	}

	acptboot_data->listen_epd = scif_open();

	if (!acptboot_data->listen_epd) {
		printk(KERN_ERR "ACPTBOOT: scif_open() failed!\n");
		err = -ENOMEM;
		goto error;
	}

	err = scif_bind(acptboot_data->listen_epd, MIC_NOTIFY);
	if (err < 0) {
		pr_debug("ACPTBOOT: scif_bind() failed! %d\n", err);
		goto error;
	}

	acptboot_data->acptboot_pn = err;

	err = scif_listen(acptboot_data->listen_epd, ACPT_BACKLOG);
	if (err < 0) {
		pr_debug("scif_listen() failed! %d\n", err);
		goto error;

	}

	pr_debug("ACPT endpoint listening port %d\n", 
						acptboot_data->acptboot_pn);

	// Create workqueue
	acptboot_data->acptbootwq = __mic_create_singlethread_workqueue(
							"ACPTBOOT_WQ");

	if (!acptboot_data->acptbootwq) {
		printk(KERN_ERR "%s %d wq creation failed!\n", __func__, __LINE__);
		goto error;
	}

	INIT_WORK(&acptboot_data->acptbootwork, acptboot_getconn);
	queue_work(acptboot_data->acptbootwq, 
					&acptboot_data->acptbootwork);
	return 0;

error:

	if (acptboot_data->listen_epd)
		if ((ret = scif_close(acptboot_data->listen_epd)) < 0)
			pr_debug("ACPTBOOT: scif_close() failed! %d\n", ret);

	kfree(acptboot_data);

	return err;
}

