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
 * Intel MIC driver.
 */
#ifndef __MIC_COMMON_H_
#define __MIC_COMMON_H_

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/virtio_ring.h>


/******************************************************************************
 * Debug
 ******************************************************************************/
#ifdef __KERNEL__

#define mic_host_log(func, fmt, ...) \
	func("[%7u] micH: %s:%u " fmt "\n", task_pid_nr(current), \
	     __func__, __LINE__, ##__VA_ARGS__)

#define mic_log(func, index, fmt, ...) \
	func("[%7u] mic%d: %s:%u " fmt "\n", task_pid_nr(current), \
	     index, __func__, __LINE__, ##__VA_ARGS__)

#define log_mic_err(index, fmt, ...)  mic_log(pr_err, index, fmt, ##__VA_ARGS__)
#define log_mic_warn(index, fmt, ...) mic_log(pr_warn, index, fmt, ##__VA_ARGS__)
#define log_mic_info(index, fmt, ...) mic_log(pr_info, index, fmt, ##__VA_ARGS__)
#define log_mic_dbg(index, fmt, ...)  mic_log(pr_debug, index, fmt, ##__VA_ARGS__)

#define log_mic_host_err(fmt, ...)  mic_host_log(pr_err, fmt, ##__VA_ARGS__)
#define log_mic_host_warn(fmt, ...) mic_host_log(pr_warn, fmt, ##__VA_ARGS__)
#define log_mic_host_info(fmt, ...) mic_host_log(pr_info, fmt, ##__VA_ARGS__)
#define log_mic_host_dbg(fmt, ...) mic_host_log(pr_debug, fmt, ##__VA_ARGS__)

#endif
/******************************************************************************
 * MIC states and commands
 ******************************************************************************/

/**
 * enum mic_state - MIC states.
 */
enum mic_state {
	MIC_STATE_UNKNOWN,
	MIC_STATE_ERROR,
	MIC_STATE_READY,
	MIC_STATE_ONLINE,
	MIC_STATE_ONLINE_FIRMWARE,
	MIC_STATE_SHUTDOWN,
	MIC_STATE_RESETTING,
	MIC_STATE_SHUTTING_DOWN,
	MIC_STATE_BOOTING,
	MIC_STATE_BOOTING_FIRMWARE,
	MIC_STATE_LAST = MIC_STATE_BOOTING_FIRMWARE
};

/**
 * enum mic_command - MIC commands.
 */
enum mic_command {
	MIC_CMD_NONE,
	MIC_CMD_BOOT_FIRMWARE,
	MIC_CMD_BOOT,
	MIC_CMD_RESET,
	MIC_CMD_RESET_FROM_CARD,
	MIC_CMD_RESET_FORCE,
	MIC_CMD_RESET_FORCE_WARM,
	MIC_CMD_SHUTDOWN,
	MIC_CMD_SHUTDOWN_FROM_CARD,
	MIC_CMD_SHUTDOWN_FORCE,
	MIC_CMD_HANDLE_PANIC,
	MIC_CMD_HANDLE_HANG,
	MIC_CMD_LAST = MIC_CMD_HANDLE_HANG
};

enum mic_command_flags {
	MIC_CMD_FLAG_HIDDEN		= 0x1,
	MIC_CMD_FLAG_ABORT_CURRENT	= 0x2
};

/******************************************************************************
 * Virtio
 ******************************************************************************/

#define __mic_align(a, x) (((a) + (x) - 1) & ~((x) - 1))

/**
 * struct mic_device_desc: Virtio device information shared between the
 * virtio driver and userspace backend
 *
 * @type: Device type: console/network/disk etc.  Type 0/-1 terminates.
 * @num_vq: Number of virtqueues.
 * @feature_len: Number of bytes of feature bits.  Multiply by 2: one for
   host features and one for guest acknowledgements.
 * @config_len: Number of bytes of the config array after virtqueues.
 * @status: A status byte, written by the Guest.
 * @config: Start of the following variable length config.
 */
struct mic_device_desc {
	__s8 type;
	__u8 num_vq;
	__u8 feature_len;
	__u8 config_len;
	__u8 status;
	__le64 config[0];
} __attribute__ ((aligned(8)));

/**
 * struct mic_device_ctrl: Per virtio device information in the device page
 * used internally by the host and card side drivers.
 *
 * @vdev: Used for storing MIC vdev information by the guest.
 * @config_change: Set to 1 by host when a config change is requested.
 * @vdev_reset: Set to 1 by guest to indicate virtio device has been reset.
 * @guest_ack: Set to 1 by guest to ack a command.
 * @host_ack: Set to 1 by host to ack a command.
 * @used_address_updated: Set to 1 by guest when the used address should be
 * updated.
 * @c2h_vdev_db: The doorbell number to be used by guest. Set by host.
 * @h2c_vdev_db: The doorbell number to be used by host. Set by guest.
 */
struct mic_device_ctrl {
	__le64 vdev;
	__u8 config_change;
	__u8 vdev_reset;
	__u8 guest_ack;
	__u8 host_ack;
	__u8 used_address_updated;
	__s8 c2h_vdev_db;
	__s8 h2c_vdev_db;
} __attribute__ ((aligned(8)));

/**
 * struct mic_bootparam: Virtio device independent information in device page
 *
 * @magic: A magic value used by the card to ensure it can see the host
 * @h2c_config_db: Host to Card Virtio config doorbell set by card
 * @node_id: Unique id of the node
 * @h2c_scif_db - Host to card SCIF doorbell set by card
 * @c2h_scif_db - Card to host SCIF doorbell set by host
 * @scif_host_dma_addr - SCIF host queue pair DMA address
 * @scif_card_dma_addr - SCIF card queue pair DMA address
 */
struct mic_bootparam {
	__le32 magic;
	__s8 h2c_config_db;
	__u8 node_id;
	__u8 h2c_scif_db;
	__u8 c2h_scif_db;
	__u64 scif_host_dma_addr;
	__u64 scif_card_dma_addr;
} __attribute__ ((aligned(8)));

/**
 * struct mic_device_page: High level representation of the device page
 *
 * @bootparam: The bootparam structure is used for sharing information and
 * status updates between MIC host and card drivers.
 * @desc: Array of MIC virtio device descriptors.
 */
struct mic_device_page {
	struct mic_bootparam bootparam;
	struct mic_device_desc desc[0];
};
/**
 * struct mic_vqconfig: This is how we expect the device configuration field
 * for a virtqueue to be laid out in config space.
 *
 * @address: Guest/MIC physical address of the virtio ring
 * (avail and desc rings)
 * @used_address: Guest/MIC physical address of the used ring
 * @num: The number of entries in the virtio_ring
 */
struct mic_vqconfig {
	__le64 address;
	__le64 used_address;
	__le16 num;
} __attribute__ ((aligned(8)));

/*
 * The alignment to use between consumer and producer parts of vring.
 * This is pagesize for historical reasons.
 */
#define MIC_VIRTIO_RING_ALIGN		4096

#define MIC_MAX_VRINGS			4
#define MIC_VRING_ENTRIES		128

/*
 * Max vring entries (power of 2) to ensure desc and avail rings
 * fit in a single page
 */
#define MIC_MAX_VRING_ENTRIES		128

/**
 * Max size of the desc block in bytes: includes:
 *	- struct mic_device_desc
 *	- struct mic_vqconfig (num_vq of these)
 *	- host and guest features
 *	- virtio device config space
 */
#define MIC_MAX_DESC_BLK_SIZE		256

/**
 * struct _mic_vring_info - Host vring info exposed to userspace backend
 * for the avail index and magic for the card.
 *
 * @avail_idx: host avail idx
 * @magic: A magic debug cookie.
 */
struct _mic_vring_info {
	__u16 avail_idx;
	__le32 magic;
};

/**
 * struct mic_vring - Vring information.
 *
 * @vr: The virtio ring.
 * @info: Host vring information exposed to the userspace backend for the
 * avail index and magic for the card.
 * @va: The va for the buffer allocated for vr and info.
 * @len: The length of the buffer required for allocating vr and info.
 */
struct mic_vring {
	struct vring vr;
	struct _mic_vring_info *info;
	void *va;
	int len;
};

#define mic_aligned_desc_size(d) __mic_align(mic_desc_size(d), 8)

#ifndef INTEL_MIC_CARD
static inline unsigned mic_desc_size(const struct mic_device_desc *desc)
{
	return sizeof(*desc) + desc->num_vq * sizeof(struct mic_vqconfig)
		+ desc->feature_len * 2 + desc->config_len;
}

static inline struct mic_vqconfig *
mic_vq_config(const struct mic_device_desc *desc)
{
	return (struct mic_vqconfig *)(desc + 1);
}

static inline __u8 *mic_vq_features(const struct mic_device_desc *desc)
{
	return (__u8 *)(mic_vq_config(desc) + desc->num_vq);
}

static inline __u8 *mic_vq_configspace(const struct mic_device_desc *desc)
{
	return mic_vq_features(desc) + desc->feature_len * 2;
}
static inline unsigned mic_total_desc_size(struct mic_device_desc *desc)
{
	return mic_aligned_desc_size(desc) + sizeof(struct mic_device_ctrl);
}
#endif

/* Device page size */
#define MIC_DP_SIZE 4096

#define MIC_MAGIC 0xc0ffee00

#define MIC_HOST_LOG_BUFFER (PAGE_SIZE * 512)

#endif
