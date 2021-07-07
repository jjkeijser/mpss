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

#ifndef MICVCONS_H
#define MICVCONS_H

#include <linux/tty.h>
#include <linux/tty_flip.h>

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
#include <mic/micscif.h>
#include <mic/micscif_nm.h>

#define MICVCONS_DEVICE_NAME "ttyMIC"

#define MICVCONS_BUF_SIZE		PAGE_SIZE
#define MICDCONS_MAX_OUTPUT_BYTES	64
#define MICVCONS_SHORT_TIMEOUT	100
#define MICVCONS_MAX_TIMEOUT	500

#define MIC_VCONS_READY		0xc0de
#define MIC_VCONS_SLEEPING	0xd00d
#define MIC_VCONS_WAKINGUP	0xd12d
#define MIC_HOST_VCONS_READY	0xbad0
#define MIC_VCONS_HOST_OPEN	0xbad1
#define MIC_VCONS_RB_VER_ERR	0xbad2

#define MICVCONS_TIMER_RESTART	1
#define MICVCONS_TIMER_SHUTDOWN	0

typedef struct micvcons {
	int			dc_enabled;
	void			*dc_hdr_virt;
	void			*dc_buf_virt;
	dma_addr_t	dc_hdr_dma_addr;
	dma_addr_t	dc_dma_addr;
	uint32_t		dc_size;
} micvcons_t;

typedef struct micvcons_port {
	struct board_info	*dp_bdinfo;
	struct micvcons		*dp_vcons;
	struct micscif_rb	*dp_in;
	struct micscif_rb	*dp_out;
	struct tty_struct	*dp_tty;
	struct list_head	list_member;
	/*
	 * work queue to schedule work that wakes up a sleeping card
	 * and read the data from the buffer.
	 */
	struct workqueue_struct	*dp_wq;
	struct work_struct		dp_wakeup_read_buf;

	spinlock_t		dp_lock;
	struct mutex		dp_mutex;

	volatile int		dp_bytes;
	volatile uint32_t	dp_canread;

	volatile struct file	*dp_reader;
	volatile struct file	*dp_writer;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
	struct tty_port		port;
#endif
} micvcons_port_t;

/* vcons IPC layout */
struct vcons_buf
{
	uint32_t	host_magic;
	uint32_t	mic_magic;

	uint16_t	host_rb_ver;
	uint16_t	mic_rb_ver;

	/* mic o/p buffer */
	dma_addr_t o_buf_dma_addr;	/* host buf dma addr*/
	uint32_t	o_wr;
	uint32_t	o_size;

	/* mic i/p buffer */
	uint64_t	i_hdr_addr;		/* mic hdr addr */
	uint64_t	i_buf_addr;		/* mic buf addr */
	uint32_t	i_rd;
	uint32_t	i_size;
};

struct vcons_mic_header
{
	uint32_t o_rd;
	uint32_t i_wr;
	uint32_t host_status;
};

int micvcons_start(struct _mic_ctx_t *mic_ctx);
int micvcons_port_write(struct micvcons_port *port, const unsigned char *buf,
				int count);
struct _mic_ctx_t;
void micvcons_stop(struct _mic_ctx_t *mic_ctx);
int micvcons_pm_disconnect_node(uint8_t *node_bitmask, enum disconn_type type);
#endif /* MICVCONS_H */
