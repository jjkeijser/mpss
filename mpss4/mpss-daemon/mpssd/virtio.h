/*
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
 */

#pragma once

#include "mpssconfig.h"
#include "sync_utils.h"

/*
 * This is done for compilation of inline function in included files which
 * are not compatible with C++, using C++ keywords and pointer conversions
 */
#pragma GCC diagnostic warning "-fpermissive"
#define class class_
#include <mic_common.h>
#include <linux/virtio_blk.h>
#include <linux/virtio_net.h>
#include <linux/virtio_console.h>
#undef class
#pragma GCC diagnostic error "-fpermissive"

#include <thread>
#include <vector>

#define PAGE_SIZE 4096

struct virtblk_dev_page_t {
	struct mic_device_desc dd;
	struct mic_vqconfig vqconfig[1];
	__u32 host_features, guest_acknowledgements;
	struct virtio_blk_config blk_config;
};

struct virtnet_dev_page_t {
	struct mic_device_desc dd;
	struct mic_vqconfig vqconfig[2];
	__u32 host_features, guest_acknowledgements;
	struct virtio_net_config net_config;
};

struct virtcons_dev_page_t {
	struct mic_device_desc dd;
	struct mic_vqconfig vqconfig[2];
	__u32 host_features, guest_acknowledgements;
	struct virtio_console_config cons_config;
};

class mic_device_context {
	shutdown_status m_ss;

	void init();

public:
	virtblk_dev_page_t virtblk_dev_page[BLOCK_MAX_COUNT];
	virtnet_dev_page_t virtnet_dev_page;
	virtcons_dev_page_t virtcons_dev_page;

	std::vector<std::thread> m_vblk_vector;
	std::thread m_vnet_thread;
	std::thread m_vcon_thread;

	mic_device_context() {
		init();
	}

	bool need_shutdown() {
		return m_ss.is_shutdown_requested();
	}

	void shutdown();
};

void virtio_net(mic_device_context *mdc, mic_info* mic);
void* virtio_console(mic_device_context *mdc, mic_info* mic);
void virtio_block(mic_device_context *mdc, mic_info* mic, int idx);
bool setup_virtio_block(struct mic_info* mic, mic_device_context* mdc, int idx);
