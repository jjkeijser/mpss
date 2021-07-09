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

#include <linux/cred.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/idr.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/version.h>
#include "cosm_main.h"

static const char cosm_driver_name[] = "mic";

/* Class of MIC devices for sysfs accessibility. */
static struct class *g_cosm_class;
/* Number of MIC devices */
static atomic_t g_num_dev;

/* Timeout interval in ms */
const unsigned int cosm_timeout_interval = 1000;

/* Maximal timeout in seconds */
const unsigned int cosm_max_state_timeout = 3 * 60;

/* Maximal timeout for command start completion in seconds */
const unsigned int cosm_command_start_timeout = 30;


struct cosm_state cosm_states[] = {
	[MIC_STATE_READY] = {
		.string = "ready",
		.allowed_commands = BIT(MIC_CMD_BOOT_FIRMWARE) |
				    BIT(MIC_CMD_BOOT) |
				    BIT(MIC_CMD_RESET) |
				    BIT(MIC_CMD_SHUTDOWN)
	},
	[MIC_STATE_ONLINE] = {
		.string = "online",
		.allowed_commands = BIT(MIC_CMD_RESET) |
				    BIT(MIC_CMD_RESET_FROM_CARD) |
				    BIT(MIC_CMD_SHUTDOWN) |
				    BIT(MIC_CMD_SHUTDOWN_FROM_CARD)
	},
	[MIC_STATE_ONLINE_FIRMWARE] = {
		.string = "online_firmware",
		.allowed_commands = BIT(MIC_CMD_RESET)
	},
	[MIC_STATE_SHUTDOWN] = {
		.string = "shutdown",
		.allowed_commands = BIT(MIC_CMD_RESET)
	},
	[MIC_STATE_ERROR] = {
		.string = "error",
		.allowed_commands = BIT(MIC_CMD_RESET)
	},
	[MIC_STATE_UNKNOWN]		= {
		.string = "unknown",
		.allowed_commands = BIT(MIC_CMD_RESET)
	},
	[MIC_STATE_RESETTING]		= { .string = "resetting" },
	[MIC_STATE_SHUTTING_DOWN]	= { .string = "shutting_down" },
	[MIC_STATE_BOOTING]		= { .string = "booting" },
	[MIC_STATE_BOOTING_FIRMWARE]	= { .string = "booting_firmware" }
};



/******************************************************************************
 * Helper functions
 ******************************************************************************/

const char*
cosm_msg_to_string(u8 msg)
{
	switch (msg) {
	case COSM_MSG_SHUTDOWN:
		return "shutdown";
	case COSM_MSG_SYNC_TIME:
		return "sync time";
	case COSM_MSG_HEARTBEAT:
		return "heartbeat";
	case COSM_MSG_STATE_CHANGE:
		return "state change";
	}

	return NULL;
}

static inline bool
is_command_allowed(u8 state, u8 command)
{
	return cosm_states[state].allowed_commands & BIT(command);
}


/******************************************************************************
 * The STOP command
 ******************************************************************************/

static void
cosm_stop(struct cosm_device *cdev)
{
	log_mic_info(cdev->index, "cleanup devices (scif and vop)");
	cdev->hw_ops->cleanup(cdev);
}


/******************************************************************************
 * The BOOT_FIRMWARE command
 ******************************************************************************/

static int
cosm_boot_firmware(struct cosm_device *cdev, const char **state_msg)
{
	int rc;

	WARN_ON(cdev->previous_state != MIC_STATE_READY);

	rc = cdev->hw_ops->boot_firmware(cdev);
	log_mic_info(cdev->index, "rc %d", rc);

	return rc;
}

static int
cosm_boot_firmware_complete(struct cosm_device *cdev, unsigned int time,
			    const char **state_msg)
{
	if (cdev->hw_ops->detect_state(cdev) != MIC_STATE_ONLINE_FIRMWARE)
		return -EAGAIN;

	return 0;
}

static unsigned int
cosm_boot_firmware_timeout(struct cosm_device *cdev)
{
	return cdev->hw_ops->boot_firmware_timeout;
}


/******************************************************************************
 * The BOOT command
 ******************************************************************************/

static bool
cosm_is_boot_safe(struct cosm_device *cdev)
{
	u64 max_supported_address = cdev->hw_ops->max_supported_address(cdev);
	u64 max_system_address = __pa(high_memory) - 1;

	if (intel_iommu_enabled)
		return true;

	/*
	 * Check whether whole system memory can be mapped by the card.
	 */
	if (max_system_address <= max_supported_address)
		return true;

	log_mic_err(cdev->index, "System memory above supported value");
	log_mic_err(cdev->index, "Please add kernel command line parameter mem=%lluG",
		    max_supported_address >> 30);

#if defined(IOMMU_SUPPORTS_KNL)
	log_mic_err(cdev->index, "or enable IOMMU using intel_iommu=on");
#endif

	return false;
}

static int
cosm_boot(struct cosm_device *cdev, const char **state_msg)
{
	int rc;

	WARN_ON(cdev->previous_state != MIC_STATE_READY);

	if (!cosm_is_boot_safe(cdev)) {
		*state_msg = "memory above supported value, enable IOMMU";
		return -E2BIG;
	}

	rc = cdev->hw_ops->boot(cdev);
	if (rc)
		log_mic_err(cdev->index, "hw_ops->boot failed (rc %d)", rc);

	return rc;
}

static int
cosm_boot_complete(struct cosm_device *cdev, unsigned int time,
		   const char **state_msg)
{
	if (cdev->current_command.received_online)
		return 0;
	else
		return -EAGAIN;
}

static unsigned int
cosm_boot_timeout(struct cosm_device *cdev)
{
	return cdev->config.boot_timeout;
}


/******************************************************************************
 * The RESET command
 ******************************************************************************/

static int
cosm_reset_force(struct cosm_device *cdev, const char **state_msg)
{
	cosm_stop(cdev);
	log_mic_info(cdev->index, "force resetting the card");
	return cdev->hw_ops->reset(cdev);
}

static int
cosm_reset_force_warm(struct cosm_device *cdev, const char **state_msg)
{
	cosm_stop(cdev);
	log_mic_info(cdev->index, "resetting the card");
	return cdev->hw_ops->reset_warm(cdev);
}

static int
cosm_reset(struct cosm_device *cdev, const char **state_msg)
{
	int rc;
	struct cosm_msg msg = { .id = COSM_MSG_REBOOT };

	if (cdev->previous_state == MIC_STATE_UNKNOWN)
		if (cdev->hw_ops->detect_state(cdev) == MIC_STATE_RESETTING)
			return 0;

	mutex_lock(&cdev->epd_mutex);
	if (cdev->epd && cdev->previous_state == MIC_STATE_ONLINE) {
		rc = scif_send(cdev->epd, &msg, sizeof(msg), SCIF_SEND_BLOCK);
		if (rc > 0) {
			mutex_unlock(&cdev->epd_mutex);
			log_mic_info(cdev->index, "reset request sent");
			return 0;
		}
	}
	mutex_unlock(&cdev->epd_mutex);

	cosm_stop(cdev);
	log_mic_info(cdev->index, "resetting the card");
	return cdev->hw_ops->reset(cdev);
}

static int
cosm_reset_from_card_complete(struct cosm_device *cdev, unsigned int time,
			      const char **state_msg)
{
	if (cdev->hw_ops->detect_state(cdev) != MIC_STATE_READY)
		return -EAGAIN;

	cosm_stop(cdev);

	cdev->config.execute_on_ready = MIC_CMD_BOOT;
	return 0;
}

static int
cosm_reset_complete(struct cosm_device *cdev, unsigned int time,
		    const char **state_msg)
{
	int rc;

	if (cdev->hw_ops->detect_state(cdev) != MIC_STATE_READY)
		return -EAGAIN;

	cosm_stop(cdev);

	rc = cdev->hw_ops->dev_update(cdev);
	if (rc)
		log_mic_err(cdev->index, "hw_ops->dev_update failed (rc %d)", rc);

	return 0;
}

static unsigned int
cosm_reset_timeout(struct cosm_device *cdev)
{
	/* longest possible transition: online -> ready */
	return cdev->config.shutdown_timeout + cdev->hw_ops->reset_timeout;
}

static int
cosm_handle_hang_complete(struct cosm_device *cdev, unsigned int time,
			  const char **state_msg)
{
	*state_msg = "coprocessor not responding";
	return 0;
}

static int
cosm_handle_panic_complete(struct cosm_device *cdev,unsigned int time,
			   const char **state_msg)
{
	*state_msg = "kernel panic";
	return 0;
}

/******************************************************************************
 * The SHUTDOWN command
 ******************************************************************************/

static int
cosm_shutdown_force(struct cosm_device *cdev, const char **state_msg)
{
	cosm_stop(cdev);
	log_mic_info(cdev->index, "force shutting down the card");
	return cdev->hw_ops->shutdown(cdev);
}

static int
cosm_shutdown(struct cosm_device *cdev, const char **state_msg)
{
	int rc;
	struct cosm_msg msg = { .id = COSM_MSG_SHUTDOWN };

	WARN_ON((cdev->previous_state != MIC_STATE_READY) &&
		(cdev->previous_state != MIC_STATE_ONLINE));

	if (cdev->hw_ops->detect_state(cdev) == MIC_STATE_READY) {
		cosm_stop(cdev);
		log_mic_info(cdev->index, "shutdown from the ready state");
		return cdev->hw_ops->shutdown(cdev);
	}

	mutex_lock(&cdev->epd_mutex);
	if (cdev->epd && cdev->previous_state == MIC_STATE_ONLINE) {
		rc = scif_send(cdev->epd, &msg, sizeof(msg), SCIF_SEND_BLOCK);
		if (rc > 0) {
			mutex_unlock(&cdev->epd_mutex);
			log_mic_info(cdev->index, "shutdown request sent");
			return 0;
		}
	}
	mutex_unlock(&cdev->epd_mutex);

	log_mic_err(cdev->index, "shutdown failed");
	return -EINVAL;
}

static int
cosm_shutdown_complete(struct cosm_device *cdev, unsigned int time,
		       const char **state_msg)
{
	if (cdev->hw_ops->detect_state(cdev) != MIC_STATE_SHUTDOWN)
		return -EAGAIN;

	cosm_stop(cdev);

	return 0;
}

static unsigned int
cosm_shutdown_timeout(struct cosm_device *cdev)
{
	return cdev->config.shutdown_timeout;
}


/******************************************************************************
 * COSM commands declaration
 ******************************************************************************/

struct cosm_command cosm_commands[] = {
	[MIC_CMD_NONE] = {
		.string = "none",
		.flags = MIC_CMD_FLAG_HIDDEN,
	},
	[MIC_CMD_HANDLE_PANIC] = {
		.string = "handle_panic",
		.complete = cosm_handle_panic_complete,
		.flags = MIC_CMD_FLAG_HIDDEN | MIC_CMD_FLAG_ABORT_CURRENT,
		.temporal_state = MIC_STATE_UNKNOWN,
		.target_state = MIC_STATE_ERROR
	},
	[MIC_CMD_HANDLE_HANG] = {
		.string = "handle_hang",
		.complete = cosm_handle_hang_complete,
		.flags = MIC_CMD_FLAG_HIDDEN | MIC_CMD_FLAG_ABORT_CURRENT,
		.temporal_state = MIC_STATE_UNKNOWN,
		.target_state = MIC_STATE_ERROR
	},
	[MIC_CMD_BOOT_FIRMWARE] = {
		.string = "boot_firmware",
		.invoke = cosm_boot_firmware,
		.complete = cosm_boot_firmware_complete,
		.get_timeout = cosm_boot_firmware_timeout,
		.temporal_state = MIC_STATE_BOOTING_FIRMWARE,
		.target_state = MIC_STATE_ONLINE_FIRMWARE
	},
	[MIC_CMD_BOOT] = {
		.string = "boot",
		.invoke = cosm_boot,
		.complete = cosm_boot_complete,
		.get_timeout = cosm_boot_timeout,
		.temporal_state = MIC_STATE_BOOTING,
		.target_state = MIC_STATE_ONLINE
	},
	[MIC_CMD_RESET] = {
		.string = "reset",
		.invoke = cosm_reset,
		.complete = cosm_reset_complete,
		.get_timeout = cosm_reset_timeout,
		.temporal_state = MIC_STATE_RESETTING,
		.target_state = MIC_STATE_READY
	},
	[MIC_CMD_RESET_FROM_CARD] = {
		.string = "reset_from_card",
		.complete = cosm_reset_from_card_complete,
		.get_timeout = cosm_reset_timeout,
		.temporal_state = MIC_STATE_RESETTING,
		.target_state = MIC_STATE_READY,
		.flags = MIC_CMD_FLAG_HIDDEN
	},
	[MIC_CMD_RESET_FORCE] = {
		.string = "reset_force",
		.invoke = cosm_reset_force,
		.complete = cosm_reset_complete,
		.get_timeout = cosm_reset_timeout,
		.temporal_state = MIC_STATE_RESETTING,
		.target_state = MIC_STATE_READY,
		.flags = MIC_CMD_FLAG_ABORT_CURRENT
	},
	[MIC_CMD_RESET_FORCE_WARM] = {
		.string = "reset_force_warm",
		.invoke = cosm_reset_force_warm,
		.complete = cosm_reset_complete,
		.get_timeout = cosm_reset_timeout,
		.temporal_state = MIC_STATE_RESETTING,
		.target_state = MIC_STATE_READY,
		.flags = MIC_CMD_FLAG_ABORT_CURRENT
	},
	[MIC_CMD_SHUTDOWN] = {
		.string = "shutdown",
		.invoke = cosm_shutdown,
		.complete = cosm_shutdown_complete,
		.get_timeout = cosm_shutdown_timeout,
		.temporal_state = MIC_STATE_SHUTTING_DOWN,
		.target_state = MIC_STATE_SHUTDOWN
	},
	[MIC_CMD_SHUTDOWN_FROM_CARD] = {
		.string = "shutdown_from_card",
		.complete = cosm_shutdown_complete,
		.get_timeout = cosm_shutdown_timeout,
		.temporal_state = MIC_STATE_SHUTTING_DOWN,
		.target_state = MIC_STATE_SHUTDOWN,
		.flags = MIC_CMD_FLAG_HIDDEN
	},
	[MIC_CMD_SHUTDOWN_FORCE] = {
		.string = "shutdown_force",
		.invoke = cosm_shutdown_force,
		.complete = cosm_shutdown_complete,
		.get_timeout = cosm_shutdown_timeout,
		.temporal_state = MIC_STATE_SHUTTING_DOWN,
		.target_state = MIC_STATE_SHUTDOWN,
		.flags = MIC_CMD_FLAG_ABORT_CURRENT
	}
};

/******************************************************************************
 * State flow functions
 ******************************************************************************/

static void cosm_lock_module(struct cosm_device *cdev)
{
	if (!cdev->is_module_locked) {
		if (!try_module_get(THIS_MODULE)) {
			log_mic_err(cdev->index, "can't get reference for module");
			return;
		}
		cdev->is_module_locked = true;
		log_mic_info(cdev->index, "lock module");
	}
}

static void cosm_unlock_module(struct cosm_device *cdev)
{
	if (cdev->is_module_locked) {
		module_put(THIS_MODULE);
		cdev->is_module_locked = false;
		log_mic_info(cdev->index, "unlock module");
	}
}

static void
cosm_update_module_lock(struct cosm_device *cdev, u8 state)
{
	if (state == MIC_STATE_READY || state == MIC_STATE_SHUTDOWN ||
		state == MIC_STATE_ERROR) {
		cosm_unlock_module(cdev);
	} else {
		cosm_lock_module(cdev);
	}
}

static void
cosm_switch_to_state(struct cosm_device *cdev, u8 state, const char *state_msg)
{
	u8 current_state = cdev->state;

	if (state > MIC_STATE_LAST) {
		log_mic_err(cdev->index, "invalid state %u", state);
		return;
	}

	if (current_state == MIC_STATE_READY || state == MIC_STATE_ERROR)
		cdev->config.execute_on_ready = MIC_CMD_NONE;

	log_mic_info(cdev->index, "switch state '%s' to '%s'",
		 cosm_states[current_state].string,
		 cosm_states[state].string);

	cosm_update_module_lock(cdev, state);

	cdev->previous_state = current_state;
	cdev->state = state;
	cdev->state_msg = state_msg;

	complete(&cdev->state_changed);
	sysfs_notify_dirent(cdev->sysfs_node);
}

static bool is_command_already_executed(struct cosm_device *cdev, struct cosm_command *cmd)
{
	if (cdev->current_command.id == MIC_CMD_NONE && cmd->target_state == cdev->state) {
		log_mic_info(cdev->index, "skip processing '%s' command - already in a target state", cmd->string);
		return true;
	}
	if (cdev->current_command.id != MIC_CMD_NONE && cmd->target_state == cosm_commands[cdev->current_command.id].target_state) {
		log_mic_info(cdev->index, "skip processing '%s' command - already processing to a target state", cmd->string);
		return true;
	}
	return false;
}

int cosm_set_command(struct cosm_device *cdev, u8 command)
{
	struct cosm_command *cmd = &cosm_commands[command];
	int rc = 0;

	mutex_lock(&cdev->command_mutex);

	if (cmd->flags & MIC_CMD_FLAG_ABORT_CURRENT) {
		cdev->current_command.abort = true;
		goto exec_command;
	}
	if (is_command_already_executed(cdev, cmd)) {
		rc = -EALREADY;
		goto exit;
	}
	if (cdev->current_command.id != MIC_CMD_NONE) {
		rc = -EBUSY;
		goto exit;
	}
	if (!is_command_allowed(cdev->state, command)) {
		rc = -EPERM;
		goto exit;
	}

exec_command:
	log_mic_info(cdev->index, "cancel state_flow_work");
	cancel_work_sync(&cdev->state_flow_work);

	log_mic_info(cdev->index, "request '%s' on the '%s' state", cmd->string,
		     cosm_states[cdev->state].string);

	cdev->current_command.id = command;
	cdev->current_command.abort = false;
	cdev->current_command.received_online = false;

	init_completion(&cdev->state_changed);
	schedule_work(&cdev->state_flow_work);

	/*
	 * Wait for execution of state_flow_work where the command is started.
	 * Once the command is started the card state is switched to temporal
	 * one, what ensure the state file (in sysfs) is updated before this
	 * function exits.
	 */
	rc = wait_for_completion_interruptible_timeout(
			&cdev->state_changed,
			msecs_to_jiffies(cosm_command_start_timeout * 1000));

	if (rc == 0 || rc == -ERESTARTSYS) {
		log_mic_warn(cdev->index, "completion interrupted (rc %d)", rc);
		rc = -EINTR;
		goto exit;
	}

	log_mic_info(cdev->index, "completion finished");
	rc = 0;

exit:
	mutex_unlock(&cdev->command_mutex);
	return rc;
}

int cosm_get_command_id(struct cosm_device *cdev, const char *string, u8 *command)
{
	struct cosm_command *cmd;
	u8 i;

	if (!command)
		return -EINVAL;

	for (i = 0; i <= MIC_CMD_LAST; i++) {
		cmd = &cosm_commands[i];
		if (cmd->flags & MIC_CMD_FLAG_HIDDEN)
			continue;
		if (sysfs_streq(string, cmd->string)) {
			*command = i;
			return 0;
		}
	}
	return -EINVAL;
}

static void cosm_state_flow(struct work_struct *work)
{
	struct cosm_command *command;
	int command_rc;
	unsigned int command_timeout = 0;
	unsigned long start_time;
	unsigned long timeout;
	const char *state_msg = NULL;
	u8 command_id;
	u8 state = MIC_STATE_UNKNOWN;

	struct cosm_device *cdev = container_of(work, struct cosm_device,
			state_flow_work);

	command_id = cdev->current_command.id;
	WARN_ON(command_id == MIC_CMD_NONE);

	command = &cosm_commands[command_id];

	if (command->get_timeout)
		command_timeout = command->get_timeout(cdev);

	if (!command_timeout || command_timeout > cosm_max_state_timeout)
		command_timeout = cosm_max_state_timeout;

	log_mic_info(cdev->index, "processing '%s' command with timeout %u sec",
		 command->string, command_timeout);

	start_time = jiffies;
	timeout = start_time + msecs_to_jiffies(command_timeout * 1000);

	if (command->temporal_state != MIC_STATE_UNKNOWN)
		cosm_switch_to_state(cdev, command->temporal_state, NULL);

	if (command->invoke) {
		command_rc = command->invoke(cdev, &state_msg);
		if (command_rc) {
			state = MIC_STATE_ERROR;
			goto command_exit;
		}
	}

	while (true) {
		command_rc = command->complete(cdev,
				jiffies_to_msecs(jiffies - start_time) / 1000,
				&state_msg);

		if (command_rc == 0) {
			state = command->target_state;
			break;
		}

		if (command_rc == -EINTR) {
			state = MIC_STATE_ERROR;
			state_msg = "interrupted by signal";
			break;
		}

		if (time_is_before_jiffies(timeout)) {
			state = MIC_STATE_ERROR;
			state_msg = "interrupted by timeout";
			break;
		}

		if (cdev->current_command.abort) {
			/* exit without setting error state */
			log_mic_info(cdev->index, "aborting current command");
			return;
		}

		if (command_rc == -EAGAIN) {
			msleep(cosm_timeout_interval);
			// TODO: use wait_event_timeout to make command processing faster
			continue;
		}

		state = MIC_STATE_ERROR;
		state_msg = "interrupted by unknown complete code";
		break;
	}

command_exit:
	cosm_switch_to_state(cdev, state, state_msg);
	cdev->current_command.id = MIC_CMD_NONE;

	log_mic_info(cdev->index, "processing time %u sec",
		 jiffies_to_msecs(jiffies - start_time) / 1000);
}


static int cosm_driver_probe(struct cosm_device *cdev)
{
	int rc;

	/* Initialize SCIF server at first probe */
	if (atomic_add_return(1, &g_num_dev) == 1) {
		rc = cosm_scif_init();
		if (rc)
			goto scif_exit;
	}
	mutex_init(&cdev->config_mutex);
	mutex_init(&cdev->command_mutex);
	mutex_init(&cdev->epd_mutex);

	INIT_WORK(&cdev->scif_work, cosm_scif_work);
	INIT_WORK(&cdev->state_flow_work, cosm_state_flow);
	cosm_sysfs_init(cdev);
	cdev->sysfs_dev = device_create_with_groups(g_cosm_class,
				cdev->dev.parent,
				MKDEV(0, cdev->index), cdev,
				cdev->sysfs_attr_group,
				"mic%d", cdev->index);
	if (IS_ERR(cdev->sysfs_dev)) {
		rc = PTR_ERR(cdev->sysfs_dev);
		log_mic_err(cdev->index,
			"device_create_with_groups failed rc %d", rc);
		goto scif_exit;
	}

	rc = cdev->hw_ops->dev_init(cdev);
	if (rc) {
		log_mic_err(cdev->index, "hw_ops->dev_init failed (rc %d)", rc);
		goto device_exit;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
	cdev->sysfs_node = sysfs_get_dirent(cdev->sysfs_dev->kobj.sd, "state");
#else
	cdev->sysfs_node = sysfs_get_dirent(cdev->sysfs_dev->kobj.sd,
		NULL, "state");
#endif
	if (!cdev->sysfs_node) {
		rc = -ENODEV;
		log_mic_err(cdev->index, "sysfs_get_dirent failed rc %d", rc);
		goto hw_ops_exit;
	}

	cdev->is_module_locked = false;
	cosm_lock_module(cdev);

	cdev->config.boot_timeout = COSM_MIN_BOOT_TIMEOUT;
	cdev->config.shutdown_timeout = COSM_MIN_SHUTDOWN_TIMEOUT;

	cdev->state = MIC_STATE_UNKNOWN;
	cdev->previous_state = MIC_STATE_UNKNOWN;
	cosm_set_command(cdev, MIC_CMD_RESET);

	return 0;
hw_ops_exit:
	cdev->hw_ops->dev_uninit(cdev);
device_exit:
	device_destroy(g_cosm_class, MKDEV(0, cdev->index));
scif_exit:
	if (atomic_dec_and_test(&g_num_dev))
		cosm_scif_exit();
	return rc;
}

static void cosm_driver_remove(struct cosm_device *cdev)
{
	cdev->hw_ops->dev_uninit(cdev);
	sysfs_put(cdev->sysfs_node);
	device_destroy(g_cosm_class, MKDEV(0, cdev->index));

	cosm_stop(cdev);

	log_mic_info(cdev->index, "cancel scif_work");
	cancel_work_sync(&cdev->scif_work);

	log_mic_info(cdev->index, "cancel state_flow_work");
	cancel_work_sync(&cdev->state_flow_work);

	if (atomic_dec_and_test(&g_num_dev))
		cosm_scif_exit();

	/* These sysfs entries might have allocated */
	kfree(cdev->config.efi_image);
	kfree(cdev->config.kernel_image);
	kfree(cdev->config.kernel_cmdline);
	kfree(cdev->config.initramfs_image);
}

static int cosm_suspend(struct device *dev)
{
	dev_err(dev, "suspending and freezing host system  not supported by MIC drivers\n");
	return -EBUSY;
}

static const struct dev_pm_ops cosm_pm_ops = {
	.suspend = cosm_suspend,
	.freeze = cosm_suspend
};

static struct cosm_driver cosm_driver = {
	.driver = {
		.name =  KBUILD_MODNAME,
		.owner = THIS_MODULE,
		.pm = &cosm_pm_ops,
	},
	.probe = cosm_driver_probe,
	.remove = cosm_driver_remove
};

static int __init cosm_init(void)
{
	int ret;

	g_cosm_class = class_create(THIS_MODULE, cosm_driver_name);
	if (IS_ERR(g_cosm_class)) {
		ret = PTR_ERR(g_cosm_class);
		pr_err("cosm server class_create error %d", ret);
		goto exit;
	}

	ret = cosm_register_driver(&cosm_driver);
	if (ret) {
		pr_err("cosm server cosm_register_driver error %d", ret);
		goto ida_destroy;
	}
	return 0;

ida_destroy:
	class_destroy(g_cosm_class);
exit:
	return ret;
}

static void __exit cosm_exit(void)
{
	cosm_unregister_driver(&cosm_driver);
	class_destroy(g_cosm_class);
}

module_init(cosm_init);
module_exit(cosm_exit);

MODULE_AUTHOR("Intel Corporation");
MODULE_DESCRIPTION("Intel(R) MIC Coprocessor State Management (COSM) driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(MIC_VERSION);
