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

#include <linux/errno.h>
#include <linux/hardirq.h>
#include <linux/types.h>
#include <linux/capability.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/gfp.h>
#include <linux/vmalloc.h>
#include <asm/io.h>
#include <linux/kernel.h>
#include <linux/mm_types.h>
#include <linux/jiffies.h>
#include <linux/timer.h>
#include <linux/irqflags.h>
#include <linux/time.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/semaphore.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <asm/bug.h>
#include <linux/pci.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <asm/atomic.h>
#include <linux/netdevice.h>
#include <linux/debugfs.h>

#define MIC_BOOT_PARAM_HEADER_VERSION   8

#define MIC_OS_BOOTSTATUS_SUCCESS		 1
#define MIC_OS_BOOTSTATUS_BOOT_0		 2 // Initial state of uOS boot
#define MIC_OS_BOOTSTATUS_ERROR_VERSION_MISMATCH 3
#define MIC_OS_BOOTSTATUS_ERROR			 4

#define MIC_HOST_DEFAULT		6	// Only value accepted so do not change

#define MIC_ENG_APPLICATION		0
#define MIC_ENG_PAGING			1
#define MIC_ENG_VIDEO			2
#define MIC_ENG_HIGHPRIORITY		3
#define MIC_ENG_MAX_SUPPORTED_ENGINES	4

struct ringbuf_memdesc
{
	uint64_t address;	// Location of the ring buffer
	uint32_t size;		// size of ring buffer
	uint32_t reserved;	// pad
};

struct mic_bootparam
{
	uint64_t bp_version;

	union
	{
		uint32_t bp_bootstatus;
		uint64_t bp_reserved;
	};

	uint64_t bp_vcons_addr;
	uint64_t bp_vcons_size;
	uint64_t bp_shdata_addr;
	uint64_t bp_shdata_size;
	struct ringbuf_memdesc bp_ringbuf[MIC_ENG_MAX_SUPPORTED_ENGINES];

	uint64_t bp_unused0;
	uint64_t bp_unused1;
	uint64_t bp_unused2;
	uint64_t bp_unused3;
	uint64_t bp_unused4;
	uint64_t bp_unused5;
	uint64_t bp_unused6;
	uint64_t bp_unused7;

	uint64_t bp_engstate_addr;

	struct ringbuf_memdesc bp_unused8;

	uint64_t bp_unused9;
	uint64_t bp_unused10;
	uint64_t bp_unused11;

};

struct host_bootparam
{
	uint64_t bp_version; 

	union
	{
		uint64_t bp_host_type;
		uint64_t bp_reserved;
	};

	uint64_t bp_vcons_addr;
	uint64_t bp_vcons_size;

	uint64_t bp_unused0;

	uint64_t bp_engstate_addr;

	struct ringbuf_memdesc bp_ringbuf[MIC_ENG_MAX_SUPPORTED_ENGINES];

	uint64_t bp_dmabuf_size[MIC_ENG_MAX_SUPPORTED_ENGINES];

	uint64_t bp_unused1;
	uint64_t bp_unused2;

	uint64_t bp_aper_size;

	uint8_t  bp_unused3[36];
	uint64_t bp_unused4;

	struct ringbuf_memdesc bp_unused5;

	uint64_t bp_unused6;
	uint64_t bp_unused7;

	uint32_t bp_watchdog_timeout;
};

struct enginestate_mic
{
	uint32_t writeOffset __attribute__((aligned(64)));
	uint32_t lastCompletedFence __attribute__((aligned(64)));
	uint32_t fenceWhenPreempted __attribute__((aligned(64)));
	uint32_t preemptOffset __attribute__((aligned(64)));
};

