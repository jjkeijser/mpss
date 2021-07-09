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

#ifndef _COSM_COSM_H_
#define _COSM_COSM_H_

#ifdef MIC_IN_KERNEL_BUILD
#include <linux/scif.h>
#else
#include "../scif/scif.h"
#endif
#include "../bus/cosm_bus.h"

#define COSM_MIN_BOOT_TIMEOUT		90
#define COSM_MIN_SHUTDOWN_TIMEOUT	60

struct cosm_state {
	unsigned int timeout;
	const char*  string;
	int allowed_commands;
};

extern struct cosm_state cosm_states[];


struct cosm_command {
	int (*invoke)(struct cosm_device *cdev, const char **state_msg);
	int (*complete)(struct cosm_device *cdev, unsigned int time,
			const char **state_msg);
	unsigned int (*get_timeout)(struct cosm_device *cdev);
	const char* string;
	u8 temporal_state;
	u8 target_state;
	u8 flags;
};

extern struct cosm_command cosm_commands[];

const char* cosm_msg_to_string(u8 msg);

int cosm_set_command(struct cosm_device *cdev, u8 command);
int cosm_get_command_id(struct cosm_device *cdev, const char* string, u8 *command);

#define COSM_HEARTBEAT_SEND_SEC 30
#define SCIF_COSM_LISTEN_PORT  201

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 19, 0)
#define getnstimeofday64 getnstimeofday
#define timespec64 timespec
#endif


/**
 * enum COSM msg id's
 * @COSM_MSG_SHUTDOWN: host->card trigger shutdown
 * @COSM_MSG_REBOOT: host->card trigger reboot
 * @COSM_MSG_SYNC_TIME: host->card send host time to card to sync time
 * @COSM_MSG_HEARTBEAT: card->host heartbeat
 * @COSM_MSG_STATE_CHANGE: card->host with current status (shutdown/online) as payload
 */
enum cosm_msg_id {
	COSM_MSG_SHUTDOWN,
	COSM_MSG_REBOOT,
	COSM_MSG_SYNC_TIME,
	COSM_MSG_HEARTBEAT,
	COSM_MSG_STATE_CHANGE
};

struct cosm_msg {
	u64 id;
	union {
		u8 state;
		struct timespec64 timespec;
	};
};

void cosm_sysfs_init(struct cosm_device *cdev);
void cosm_init_debugfs(void);
void cosm_exit_debugfs(void);
void cosm_create_debug_dir(struct cosm_device *cdev);
void cosm_delete_debug_dir(struct cosm_device *cdev);
int cosm_scif_init(void);
void cosm_scif_exit(void);
void cosm_scif_work(struct work_struct *work);

#endif
