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
 * Intel MIC COSM Bus Driver.
 */
#ifndef _COSM_BUS_H_
#define _COSM_BUS_H_

#include <linux/version.h>
#include <linux/completion.h>
#include "../scif/scif.h"
#include "../common/mic_common.h"
#include "../common/mic_dev.h"

/**
 * struct cosm_device_config - Set of configuration options set by ring 3.
 *
 * @efi_image: Path to file containing the EFI image.
 * @kernel_image: Path to file containing the coprocessor OS kernel image.
 * @kernel_cmdline: Extra command line passed to the coprocessor kernel.
 * @initramfs_image: Base filesystem for the embedded Linux file system.
 * @execute_on_ready: The command identifier to be executed when the MIC device
 *                    reaches the READY state.
 * @boot_timeout: The timeout of the boot operation.
 * @shutdown_timeout: The timeout of the shutdown operation.
 * @log_buf_addr: Address of the dmesg buffer of the MIC device.
 * @log_buf_len: Length of the dmesg buffer of the MIC device.
 */
struct cosm_device_config {
	const char *efi_image;
	const char *kernel_image;
	const char *kernel_cmdline;
	const char *initramfs_image;
	u8 execute_on_ready;
	unsigned int boot_timeout;
	unsigned int shutdown_timeout;
};

/**
 * struct cosm_device - Representation of a COSM device.
 *
 * @index: Unique position on the COSM bus.
 * @dev: Underlying device.
 * @sysfs_dev: Device for sysfs entries.
 * @hw_ops: The hardware bus ops for this device.
 * @config_mutex: Mutex for synchronizing access to the config fields.
 * @config: Set of configuration options set by ring 3.
 * @is_module_locked: Lock for preventing unload COSM module in unsafe state.
 * @sysfs_attr_group: Pointer to list of sysfs attribute groups.
 * @sysfs_node: The sysfs node for notifying ring 3 about MIC state changes.
 * @command_mutex: Mutex for blocking several command requests in the same time.
 * @current_command.id: Identifier of the current processing command.
 * @current_command.received_online: Set to true when the online message from
 *                                   the MIC device has been received.
 * @current_command.abort: Inform whether abort the current processing command.
 * @state_changed: Completion used to notify about the MIC state change.
 * @state: The current state of the MIC device.
 * @previous_state: The previous state of the MIC device.
 * @state_msg: Message associated with the current MIC state.
 * @state_flow_work: Work for processing requested command.
 * @scif_work: Work for handling per device SCIF connections.
 * @epd_mutex: Mutex for synchronizing access to the below endpoint.
 * @epd: The SCIF endpoint for this COSM device.
 */
struct cosm_device {
	/**
	 * Public fields used also by external modules.
	 */
	int index;
	struct device dev;
	struct device *sysfs_dev;

	struct cosm_hw_ops *hw_ops;

	struct mutex config_mutex;
	struct cosm_device_config config;

	/**
	 * Private fields used only by the COSM module.
	 */
	bool is_module_locked;

	const struct attribute_group **sysfs_attr_group;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
	struct kernfs_node *sysfs_node;
#else
	struct sysfs_dirent *sysfs_node;
#endif

	struct mutex command_mutex;

	struct {
		u8 id;
		bool received_online;
		bool abort;
	} current_command;

	struct completion state_changed;

	u8 state;
	u8 previous_state;
	const char *state_msg;

	struct work_struct state_flow_work;
	struct work_struct scif_work;

	struct mutex epd_mutex;
	scif_epd_t epd;

	dma_addr_t log_buf_dma_addr;
	void *log_buf_cpu_addr;
	int log_buf_size;
};

/**
 * struct cosm_driver - Operations for a COSM driver.
 *
 * @driver: Underlying device driver (populate name and owner).
 * @probe: The function to call when a device is found.  Returns 0 or -errno.
 * @remove: The function to call when a device is removed.
 */
struct cosm_driver {
	struct device_driver driver;
	int (*probe)(struct cosm_device *dev);
	void (*remove)(struct cosm_device *dev);
};

/**
 * struct cosm_hw_ops - The hardware bus ops for the COSM device.
 *
 * @boot_firmware: Start booting  the EFI image (used for the firmware update
 *                 or the memory test).
 * @boot: Start booting the coprocessor OS.
 * @reset: Start the reset operation of the MIC device.
 * @shutdown: Start the shutdown operation of the MIC device.
 * @reset_timeout: The timeout of the reset operation.
 * @boot_firmware_timeout: The timeout of the boot_firmware operation.
 * @detect_state: Try to detect and return the current state of the MIC device.
 *                Return MIC_STATE_UNKNOWN if it is not possible.
 * @cleanup: Cleanup all resources created by above operations.
 * @dev_init: Invoked when the COSM device is created to initiate all resources
 *            needed by the MIC device.
 * @dev_update: Invoked when the MIC device reaches the READY state. Used to
 *              update MIC device information, like SMBIOS entries.
 * @dev_uninit: Invoked when the COSM device is removed to release all resources
 *              created by the MIC device.
 * @aper: Return the MIC device PCIe aperture.
 */
struct cosm_hw_ops {
	int (*boot_firmware)(struct cosm_device *cdev);
	int (*boot)(struct cosm_device *cdev);
	int (*reset)(struct cosm_device *cdev);
	int (*reset_warm)(struct cosm_device *cdev);
	int (*shutdown)(struct cosm_device *cdev);

	int reset_timeout;
	int boot_firmware_timeout;

	int (*detect_state)(struct cosm_device *cdev);

	void (*cleanup)(struct cosm_device *cdev);

	int (*dev_init)(struct cosm_device *cdev);
	int (*dev_update)(struct cosm_device *cdev);
	void (*dev_uninit)(struct cosm_device *cdev);

	struct mic_mw *(*aper)(struct cosm_device *cdev);
	u64 (*max_supported_address)(struct cosm_device *cdev);
};

struct cosm_device *
cosm_register_device(struct device *pdev, struct cosm_hw_ops *hw_ops);
void cosm_unregister_device(struct cosm_device *dev);
int cosm_register_driver(struct cosm_driver *drv);
void cosm_unregister_driver(struct cosm_driver *drv);
struct cosm_device *cosm_find_cdev_by_id(int id);

static inline struct cosm_device *dev_to_cosm(struct device *dev)
{
	return container_of(dev, struct cosm_device, dev);
}

static inline struct cosm_driver *drv_to_cosm(struct device_driver *drv)
{
	return container_of(drv, struct cosm_driver, driver);
}
#endif /* _COSM_BUS_H */
