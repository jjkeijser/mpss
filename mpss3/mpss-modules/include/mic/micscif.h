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

#ifndef MICSCIF_H
#define MICSCIF_H

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

#ifdef _MODULE_SCIF_
#include <linux/mman.h>
#include <linux/pagemap.h>
#include <asm/uaccess.h>
#include <linux/poll.h>
#include <linux/mmzone.h>
#include <linux/version.h>
#endif /* MODULE_SCIF */

#include <linux/notifier.h>
#include "scif.h"
#include "mic/micbaseaddressdefine.h"
#include "mic/micsboxdefine.h"

/* The test runs in a separate thread context from the bottom
 * half that processes messages from the card and setup p2p
 * when these run concurrently, p2p messages get lost since they
 * may be consumed by the test thread
 */
//#define ENABLE_TEST	// Used to enable testing at board connect
#ifdef MIC_IS_EMULATION
#define TEST_LOOP 2
#else
#define TEST_LOOP 2000
#endif

//#define P2P_HACK 0
#include "scif.h"
#include "scif_ioctl.h"

#define SCIF_READY_MAGIC_NUM 0x1eedfee0

#ifndef SCIF_MAJOR
#define SCIF_MAJOR 0 /* Use dynamic major number by default */
#endif

#define SCIF_HOST_NODE	0  // By default the host is always node zero

#define SCIF_RMA_TEMP_CACHE_LIMIT 0x20000
/*
 * The overhead for proxying a P2P DMA read to convert it to
 * a DMA write by sending a SCIF Node QP message has been
 * seen to be higher than programming a P2P DMA Read on self
 * for transfer sizes less than the PROXY_DMA_THRESHOLD.
 * The minimum threshold is different for Jaketown versus
 * Ivytown and tuned for best DMA performance.
 */
#define SCIF_PROXY_DMA_THRESHOLD_JKT (32 * 1024ULL)
#define SCIF_PROXY_DMA_THRESHOLD_IVT (1024 * 1024ULL)

//#define RMA_DEBUG 0

/* Pre-defined L1_CACHE_SHIFT is 6 on RH and 7 on Suse */
#undef L1_CACHE_SHIFT
#define L1_CACHE_SHIFT	6
#undef L1_CACHE_BYTES
#define L1_CACHE_BYTES	(1 << L1_CACHE_SHIFT)

#define MI_EPLOCK_HELD  (true)
#define MAX_RDMASR	8

// Device wide SCIF information
struct micscif_info {
	uint32_t	 mi_nodeid;	// Node ID this node is to others.

	struct mutex	 mi_conflock;	// Configuration lock (used in p2p setup)
	uint32_t	 mi_maxid;	// Max known board ID
	uint32_t	 mi_total;	// Total number of running interfaces
	uint32_t	 mi_nr_zombies; // Keep track of the number of zombie EP.
	unsigned long	 mi_mask;	// bit mask of online scif interfaces
	uint64_t	 mi_nr_ioremap; // Keep track of number of ioremap() calls on the host
					// to decide when to purge aliases for performance.
	spinlock_t	 mi_eplock;
	spinlock_t	 mi_connlock;
	spinlock_t	 mi_rmalock;    // Synchronize access to list of temporary registered
					// windows to be destroyed.
	struct mutex	 mi_fencelock;  // Synchronize access to list of remote fences requested.
	struct mutex	 mi_event_cblock;
	spinlock_t	 mi_nb_connect_lock;

	struct list_head mi_uaccept;	// List of user acceptreq waiting for acceptreg
	struct list_head mi_listen;	// List of listening end points
	struct list_head mi_zombie;	// List of zombie end points with pending RMA's.
	struct list_head mi_connected;	// List of end points in connected state
	struct list_head mi_disconnected;	// List of end points in disconnected state
	struct list_head mi_rma;	// List of temporary registered windows to be destroyed.
	struct list_head mi_rma_tc;	// List of temporary
					// registered & cached windows
					// to be destroyed.
	struct list_head mi_fence;	// List of remote fence requests.
	struct list_head mi_event_cb; /* List of event handlers registered */
	struct list_head mi_nb_connect_list;
#ifdef CONFIG_MMU_NOTIFIER
	struct list_head mi_mmu_notif_cleanup;
#endif
	struct notifier_block mi_panic_nb;
#ifndef _MIC_SCIF_
	/* The host needs to keep track of node dependencies in form of graph.
	 * This will need to be dynamically grown to support hotplug.
	 */
	uint32_t         **mi_depmtrx;
	/*
	 * Wait queue used for blocking while waiting for nodes
	 * to respond for disconnect message sent from host.
	 */
	wait_queue_head_t	mi_disconn_wq;
	/* stus of node remove operation*/
	uint64_t mi_disconnect_status;
	atomic_long_t mi_unique_msgid;
#endif
	/*
	 * Watchdog timeout on the host. Timer expiry will result in the host
	 * treating the remote node as a lost node. Default value is
	 * DEFAULT_WATCHDOG_TO and can be modified to a value greater than 1
	 * second via SCIF sysfs watchdog_to entry.
	 */
	int		mi_watchdog_to;	// Watchdog timeout
	int		mi_watchdog_enabled;	// Watchdog timeout enabled
	int		mi_watchdog_auto_reboot;	// Watchdog auto reboot enabled
	struct workqueue_struct *mi_misc_wq;  // Workqueue for miscellaneous SCIF tasks.
	struct work_struct	mi_misc_work;
#ifdef CONFIG_MMU_NOTIFIER
	struct workqueue_struct *mi_mmu_notif_wq;  // Workqueue for MMU notifier cleanup tasks.
	struct work_struct	mi_mmu_notif_work;
#endif
	int		nr_gtt_entries;	// GTT Debug Counter to detect leaks
	uint64_t	nr_2mb_pages;  // Debug Counter for number of 2mb pages.
	uint64_t	nr_4k_pages; // Debug Counter for number of 4K pages
	uint8_t		en_msg_log;
	wait_queue_head_t mi_exitwq;
	unsigned long	mi_rma_tc_limit;
	uint64_t	mi_proxy_dma_threshold;
#ifdef RMA_DEBUG
	atomic_long_t	rma_mm_cnt;
	atomic_long_t	rma_unaligned_cpu_cnt;
	atomic_long_t	rma_alloc_cnt;
	atomic_long_t	rma_pin_cnt;
#ifdef CONFIG_MMU_NOTIFIER
	atomic_long_t	mmu_notif_cnt;
#endif
#endif
#ifdef _MIC_SCIF_
	int mi_intr_rcnt[MAX_RDMASR]; // Ref count to track SCIF Interrupt Handlers
#endif
	struct workqueue_struct 	*mi_conn_wq;
	struct work_struct		mi_conn_work;
};

extern struct micscif_info ms_info;

#define SCIF_NODE_MAGIC_BIT 63
/* Magic value used to indicate a remote idle node without grabbing any locks */
#define SCIF_NODE_IDLE (1ULL << SCIF_NODE_MAGIC_BIT)

enum scif_state {
	SCIFDEV_NOTPRESENT,
	SCIFDEV_INIT,
	SCIFDEV_RUNNING,
	SCIFDEV_SLEEPING,
	SCIFDEV_STOPPING,
	SCIFDEV_STOPPED
};

extern bool mic_p2p_enable;
extern bool mic_p2p_proxy_enable;
extern bool mic_reg_cache_enable;
extern bool mic_ulimit_check;
/* p2p mapping from node id to peer id */
struct scif_p2p_info {
	int		    ppi_peer_id;
	struct scatterlist  *ppi_sg[2];
	uint64_t            sg_nentries[2]; // no of entries in scatterlists
	dma_addr_t    ppi_pa[2];  // one for mmio; one for aper
	dma_addr_t    ppi_mic_addr[2];  // one for mmio; one for aper
	uint64_t	    ppi_len[2];
#define PPI_MMIO        0
#define PPI_APER        1
	enum scif_state		ppi_disc_state; //Disconnection state of this peer node.
	struct list_head   ppi_list;
};

/* one per remote node */
struct micscif_dev {
	uint16_t		sd_node;
	enum scif_state		sd_state;
	volatile void		*mm_sbox;
	uint64_t		sd_base_addr;		/* Remote node's base bus addr
							 * for the local node's aperture
							 */
#ifndef _MIC_SCIF_
	struct list_head	sd_p2p;			/* List of bus addresses for
							 * other nodes, these are allocated
							 * by the host driver and are
							 * valid only on the host node
							 */
	struct delayed_work	sd_watchdog_work;
	wait_queue_head_t	sd_watchdog_wq;
	struct workqueue_struct	*sd_ln_wq;
	char			sd_ln_wqname[16];
#endif

	int			n_qpairs;		/* FIXME:
							 * This is always set to 1,
							 */

	struct micscif_qp	*qpairs;		/* Same FIXME as above
							 * There is single qp established
							 * with this remote node
							 */

	struct workqueue_struct       *sd_intr_wq;		/* sd_intr_wq & sd_intr_bh
							 * together constitute the workqueue
							 * infrastructure needed to
							 * run the bottom half handler
							 * for messages received from
							 * this node
							 */
	char			sd_intr_wqname[16];
	struct work_struct		sd_intr_bh;
	unsigned int		sd_intr_handle;
	uint32_t		sd_rdmasr;
	struct workqueue_struct	*sd_loopb_wq;
	char			sd_loopb_wqname[16];
	struct work_struct		sd_loopb_work;
	struct list_head	sd_loopb_recv_q;
	/* Lock to synchronize remote node state transitions */
	struct mutex		sd_lock;
	/*
	 * Global Ref count per SCIF device tracking all SCIF API's which
	 * might communicate across PCIe.
	 */
	atomic_long_t	scif_ref_cnt;
	/*
	 * Global Ref count per SCIF device tracking scif_mmap()/
	 * scif_get_pages(). sd_lock protects scif_map_ref_cnt
	 * hence it does not need to be an atomic operation. Note that
	 * scif_mmap()/scif_get_pages() is not in the critical
	 * perf path.
	 */
	int			scif_map_ref_cnt;
	/*
	 * Wait queue used for blocking while waiting for nodes
	 * to wake up or to be removed.
	 */
	wait_queue_head_t	sd_wq;
	uint64_t		sd_wait_status;
#ifdef _MIC_SCIF_
	wait_queue_head_t	sd_p2p_wq;
	bool			sd_proxy_dma_reads;
	struct delayed_work	sd_p2p_dwork;
	int			sd_p2p_retry;
#endif
	/*
	 * The NUMA node the peer is attached to on the host.
	 */
	int			sd_numa_node;
	/*
	 * Waitqueue for blocking while waiting for remote memory
	 * mappings to drop to zero.
	 */
	wait_queue_head_t	sd_mmap_wq;

	/* When a nodeqp message is received, this is set.
	 * And it is reset by the watchdog time */
	atomic_t		sd_node_alive;
	int			num_active_conn;
#ifdef ENABLE_TEST
	struct workqueue_struct	*producer;
	struct workqueue_struct	*consumer;
	char			producer_name[16];
	char			consumer_name[16];
	struct work_struct		producer_work;
	struct work_struct		consumer_work;
	int			count;
	int			test_done;
#endif // ENABLE_TEST
};

extern struct micscif_dev scif_dev[];

#include "mic/micscif_nodeqp.h"
#include "mic/micscif_nm.h"
#include "mic/micscif_smpt.h"
#include "mic/micscif_va_gen.h"
#include "mic/mic_dma_api.h"
#include "mic/mic_dma_lib.h"
#include "mic/micscif_rma.h"
#include "mic/micscif_rma_list.h"

/*
 * data structure used to sync SCIF_GET_NODE_INFO messaging
 */
struct get_node_info {
	enum micscif_msg_state  state;
	wait_queue_head_t       wq;
};

static inline uint64_t align_low(uint64_t data, uint32_t granularity)
{
	return ALIGN(data - (granularity - 1), granularity);
}

#define SCIF_MIN(a, b) (((a) < (b)) ? (a) : (b))
#define SCIF_MAX(a, b) (((a) > (b)) ? (a) : (b))

enum endptstate {
	SCIFEP_CLOSED,		// Internal state
	SCIFEP_UNBOUND,		// External state
	SCIFEP_BOUND,		// External state
	SCIFEP_LISTENING,	// External state
	SCIFEP_CONNECTED,	// External state
	SCIFEP_CONNECTING,	// Internal state
	SCIFEP_MAPPING,		// Internal state
	SCIFEP_CLOSING,		// Internal state
	SCIFEP_CLLISTEN,	// Internal state
	SCIFEP_DISCONNECTED,	// Internal state
	SCIFEP_ZOMBIE		// Internal state
};

extern char *scif_ep_states[];

// Used for coordinating connection accept sequence. This is the data structure
// for the conlist in the endpoint.
struct conreq {
	struct nodemsg	  msg;
	struct list_head list;
};

/* Size of the RB for the Node QP */
#define NODE_QP_SIZE    0x10000
/* Size of the RB for the Endpoint QP */
#define ENDPT_QP_SIZE   0x1000

struct endpt_qp_info {
	/* Qpair for this endpoint */
	struct micscif_qp *qp;
	/*
	 * Physical addr of the QP for Host or
	 * GTT offset of the QP for MIC.
	 * Required for unmapping the QP during close.
	 */
	dma_addr_t qp_offset;
	/*
	 * Payload in a SCIF_CNCT_GNT message containing the
	 * physical address of the remote_qp.
	 */
	dma_addr_t cnct_gnt_payload;
};

#define SCIFEP_MAGIC	0x5c1f000000005c1f

struct endpt {
	volatile enum endptstate state;
	spinlock_t		lock;

	struct scif_portID	port;
	struct scif_portID	peer;

	int			backlog;

	struct endpt_qp_info 	qp_info;
	struct endpt_rma_info	rma_info;
	/*
	 * scifdev used by this endpt to communicate with remote node.
	 */
	struct micscif_dev	*remote_dev;
	uint64_t		remote_ep;
	/*
	 * Keep track of number of connection requests.
	 */
	int			conreqcnt;
	/*
	 * Cache remote SCIF device state.
	 */
	enum scif_state		sd_state;
	/*
	 * True if the endpoint was created
	 * via scif_accept(..).
	 */
	bool			accepted_ep;
	/*
	 * Open file information used to match the id passed
	 * in with the flush routine.
	 */
	struct files_struct	*files;
	/*
	 * Reference count for functions using this endpoint.
	 */
	struct kref        ref_count;
	struct list_head	conlist;
	wait_queue_head_t	conwq;
	wait_queue_head_t	disconwq;
	wait_queue_head_t	diswq;
	wait_queue_head_t	sendwq;
	wait_queue_head_t	recvwq;
	struct mutex		sendlock;
	struct mutex		recvlock;
	struct list_head	list;

#ifdef CONFIG_MMU_NOTIFIER
	struct list_head	mmu_list;
#endif

	struct list_head	li_accept;	/* pending ACCEPTREG */
	int			acceptcnt;	/* pending ACCEPTREG cnt */
	struct list_head	liacceptlist;	/* link to listen accept */
	struct list_head	miacceptlist;	/* link to mi_uaccept */
	struct endpt		*listenep;	/* associated listen ep */

	/* Non-blocking connect */
	struct work_struct		conn_work;
	struct scif_portID	conn_port;
	int					conn_err;
	int					conn_async_state;
	wait_queue_head_t	conn_pend_wq;
	struct list_head	conn_list;
};

static __always_inline void
micscif_queue_for_cleanup(struct reg_range_t *window, struct list_head *list)
{
	struct endpt *ep = (struct endpt *)window->ep;
	INIT_LIST_HEAD(&window->list_member);
	window->dma_mark = get_dma_mark(ep->rma_info.dma_chan);
	spin_lock(&ms_info.mi_rmalock);
	list_add_tail(&window->list_member, list);
	spin_unlock(&ms_info.mi_rmalock);
	queue_work(ms_info.mi_misc_wq, &ms_info.mi_misc_work);
}

static __always_inline void
__micscif_rma_destroy_tcw_helper(struct reg_range_t *window)
{
	list_del(&window->list_member);
	micscif_queue_for_cleanup(window, &ms_info.mi_rma_tc);
}

void print_ep_state(struct endpt *ep, char *label);

// Function prototypes needed by Unix/Linux drivers linking to scif
int scif_fdopen(struct file *f);
int scif_fdclose(struct file *f);
int scif_process_ioctl(struct file *f, unsigned int cmd, uint64_t arg);
int micscif_mmap(struct file *file, struct vm_area_struct *vma);
int scif_mmap(struct vm_area_struct *vma, scif_epd_t epd);
void scif_munmap(struct vm_area_struct *vma);
void scif_proc_init(void);
void scif_proc_cleanup(void);
int scif_user_send(scif_epd_t epd, void *msg, int len, int flags);
int scif_user_recv(scif_epd_t epd, void *msg, int len, int flags);
int __scif_pin_pages(void *addr, size_t len, int *out_prot,
	int map_flags, scif_pinned_pages_t *pages);
scif_epd_t __scif_open(void);
int __scif_bind(scif_epd_t epd, uint16_t pn);
int __scif_listen(scif_epd_t epd, int backlog);
int __scif_connect(scif_epd_t epd, struct scif_portID *dst, bool non_block);
int __scif_accept(scif_epd_t epd, struct scif_portID *peer, scif_epd_t
*newepd, int flags);
int __scif_close(scif_epd_t epd);
int __scif_send(scif_epd_t epd, void *msg, int len, int flags);
int __scif_recv(scif_epd_t epd, void *msg, int len, int flags);
off_t __scif_register(scif_epd_t epd, void *addr, size_t len, off_t offset,
int prot_flags, int map_flags);
int __scif_unregister(scif_epd_t epd, off_t offset, size_t len);
int __scif_readfrom(scif_epd_t epd, off_t loffset, size_t len, off_t
roffset, int rma_flags);
int __scif_writeto(scif_epd_t epd, off_t loffset, size_t len, off_t
roffset, int rma_flags);
int __scif_fence_mark(scif_epd_t epd, int flags, int *mark);
int __scif_fence_wait(scif_epd_t epd, int mark);
int __scif_fence_signal(scif_epd_t epd, off_t loff, uint64_t lval, off_t roff,
uint64_t rval, int flags);
off_t __scif_register_pinned_pages(scif_epd_t epd,
scif_pinned_pages_t pinned_pages, off_t offset, int map_flags);
int __scif_get_pages(scif_epd_t epd, off_t offset, size_t len,
struct scif_range **pages);
int __scif_put_pages(struct scif_range *pages);
int __scif_flush(scif_epd_t epd);

void micscif_misc_handler(struct work_struct *work);
void micscif_conn_handler(struct work_struct *work);

uint16_t rsrv_scif_port(uint16_t port);
uint16_t get_scif_port(void);
void put_scif_port(uint16_t port);

void micscif_send_exit(void);

void scif_ref_rel(struct kref *kref_count);

#ifdef _MODULE_SCIF_
unsigned int micscif_poll(struct file *f, poll_table *wait);
unsigned int scif_pollfd(struct file *f, poll_table *wait, scif_epd_t epd);
unsigned int __scif_pollfd(struct file *f, poll_table *wait, struct endpt *ep);
int micscif_flush(struct file *f, fl_owner_t id);
#endif

#ifdef _MIC_SCIF_
void mic_debug_init(void);
void micscif_get_node_info(void);
void scif_poll_qp_state(struct work_struct *work);
#endif
void mic_debug_uninit(void);

#define serializing_request(x) ((void)*(volatile uint8_t*)(x))

// State list helper functions.
// Each of these functions must be called with the end point lock unlocked.  If
// the end point is found on the list the end point returned will have its lock
// set and sflags will return the value to be used to do an unlock_irqrestore
// at the end of the calling function.
static inline struct endpt *
micscif_find_listen_ep(uint16_t port, unsigned long *sflags)
{
	struct endpt *ep = NULL;
	struct list_head *pos, *tmpq;
	unsigned long flags;

	spin_lock_irqsave(&ms_info.mi_eplock, flags);
	list_for_each_safe(pos, tmpq, &ms_info.mi_listen) {
		ep = list_entry(pos, struct endpt, list);
		if (ep->port.port == port) {
			*sflags = flags;
			spin_lock(&ep->lock);
			spin_unlock(&ms_info.mi_eplock);
			return ep;
		}
	}
	spin_unlock_irqrestore(&ms_info.mi_eplock, flags);
	return (struct endpt *)NULL;
}

// Must be called with end point locked
static inline struct conreq *
miscscif_get_connection_request(struct endpt *ep, uint64_t payload)
{
	struct conreq *conreq;
	struct list_head *pos, *tmpq;

	list_for_each_safe(pos, tmpq, &ep->conlist) {
		conreq = list_entry(pos, struct conreq, list);
		if (conreq->msg.payload[0] == payload) {
			list_del(pos);
			ep->conreqcnt--;
			return conreq;
		}
	}
	return (struct conreq *)NULL;
}

// There is no requirement for the callee to have the end point
// locked like other API's above.
static inline void
micscif_remove_zombie_ep(struct endpt *ep)
{
	struct list_head *pos, *tmpq;
	unsigned long sflags;
	struct endpt *tmpep;

	spin_lock_irqsave(&ms_info.mi_eplock, sflags);
	list_for_each_safe(pos, tmpq, &ms_info.mi_zombie) {
		tmpep = list_entry(pos, struct endpt, list);
		if (tmpep == ep) {
			list_del(pos);
			ms_info.mi_nr_zombies--;
		}
	}
	spin_unlock_irqrestore(&ms_info.mi_eplock, sflags);
}

static inline void
micscif_cleanup_zombie_epd(void)
{
	struct list_head *pos, *tmpq;
	unsigned long sflags;
	struct endpt *ep;

	spin_lock_irqsave(&ms_info.mi_eplock, sflags);
	list_for_each_safe(pos, tmpq, &ms_info.mi_zombie) {
		ep = list_entry(pos, struct endpt, list);
		if (micscif_rma_ep_can_uninit(ep)) {
			list_del(pos);
			ms_info.mi_nr_zombies--;
			va_gen_destroy(&ep->rma_info.va_gen);
			kfree(ep);
		}
	}
	spin_unlock_irqrestore(&ms_info.mi_eplock, sflags);
}

#define	SCIF_WAKE_UP_SEND	(1 << 1)
#define	SCIF_WAKE_UP_RECV	(1 << 2)

/**
 * scif_wakeup_ep() - Wake up all clients based on the type
 * requested i.e. threads blocked in scif_send(..) and/or scif_recv(..).
 */
static inline void
scif_wakeup_ep(int type)
{
	struct endpt *ep;
	unsigned long sflags;
	struct list_head *pos, *tmpq;

	spin_lock_irqsave(&ms_info.mi_connlock, sflags);
	list_for_each_safe(pos, tmpq, &ms_info.mi_connected) {
		ep = list_entry(pos, struct endpt, list);
		if (type & SCIF_WAKE_UP_SEND)
			wake_up_interruptible(&ep->sendwq);
		if (type & SCIF_WAKE_UP_RECV)
			wake_up_interruptible(&ep->recvwq);
	}
	spin_unlock_irqrestore(&ms_info.mi_connlock, sflags);
}

/*
 * is_self_scifdev:
 * @dev: The remote SCIF Device
 *
 * Returns true if the SCIF Device passed is the self aka Loopback SCIF device.
 */
static inline int is_self_scifdev(struct micscif_dev *dev)
{
	return dev->sd_node == ms_info.mi_nodeid;
}

/*
 * is_p2p_scifdev:
 * @dev: The remote SCIF Device
 *
 * Returns true if the SCIF Device is a MIC Peer to Peer SCIF device.
 */
static inline bool is_p2p_scifdev(struct micscif_dev *dev)
{
#ifdef _MIC_SCIF_
	return dev != &scif_dev[SCIF_HOST_NODE] && !is_self_scifdev(dev);
#else
	return false;
#endif
}

/*
 * get_conn_count:
 * @dev: The remote SCIF Device
 *
 * Increments the number of active SCIF connections. Callee is expected
 * to synchronize calling this API with put_conn_count.
 */
static __always_inline void
get_conn_count(struct micscif_dev *dev)
{
	dev->num_active_conn++;
}

/*
 * put_conn_count:
 * @dev: The remote SCIF Device
 *
 * Decrements the number of active connections. Callee is expected
 * to synchronize calling this API with get_conn_count.
 */
static __always_inline void
put_conn_count(struct micscif_dev *dev)
{
	dev->num_active_conn--;
	BUG_ON(dev->num_active_conn < 0);
}

/*
 * get_kref_count:
 * epd: SCIF endpoint
 *
 * Increments kmod endpoint reference count. Callee is expected
 * to synchronize calling this API with put_kref_count.
 */
static __always_inline void
get_kref_count(scif_epd_t epd)
{
	kref_get(&(epd->ref_count));
}

/*
 * put_kref_count:
 * epd: SCIF endpoint
 *
 * Decrements kmod endpoint reference count. Callee is expected
 * to synchronize calling this API with get_kref_count.
 */
static __always_inline void
put_kref_count(scif_epd_t epd)
{
	kref_put(&(epd->ref_count), scif_ref_rel);
}

/*
 * is_scifdev_alive:
 * @dev: The remote SCIF Device
 *
 * Returns true if the remote SCIF Device is running or sleeping for
 * this endpoint.
 */
static inline int scifdev_alive(struct endpt *ep)
{
	return (((SCIFDEV_RUNNING == ep->remote_dev->sd_state) ||
		(SCIFDEV_SLEEPING == ep->remote_dev->sd_state)) &&
		SCIFDEV_RUNNING == ep->sd_state);
}

/*
 * verify_epd:
 * ep: SCIF endpoint
 *
 * Checks several generic error conditions and returns the
 * appropiate error.
 */
static inline int verify_epd(struct endpt *ep)
{
	if (ep->state == SCIFEP_DISCONNECTED)
		return -ECONNRESET;

	if (ep->state != SCIFEP_CONNECTED)
		return -ENOTCONN;

	if (!scifdev_alive(ep))
		return -ENODEV;

	return 0;
}

/**
 * scif_invalidate_ep() - Set remote SCIF device state for all connected
 * and disconnected endpoints for a particular node to SCIFDEV_STOPPED,
 * change endpoint state to disconnected and wake up all send/recv/con
 * waitqueues.
 */
static inline void
scif_invalidate_ep(int node)
{
	struct endpt *ep;
	unsigned long sflags;
	struct list_head *pos, *tmpq;

	spin_lock_irqsave(&ms_info.mi_connlock, sflags);
	list_for_each_safe(pos, tmpq, &ms_info.mi_disconnected) {
		ep = list_entry(pos, struct endpt, list);
		if (ep->remote_dev->sd_node == node) {
			spin_lock(&ep->lock);
			ep->sd_state = SCIFDEV_STOPPED;
			spin_unlock(&ep->lock);
		}
	}
	list_for_each_safe(pos, tmpq, &ms_info.mi_connected) {
		ep = list_entry(pos, struct endpt, list);
		if (ep->remote_dev->sd_node == node) {
			list_del(pos);
			put_conn_count(ep->remote_dev);
			spin_lock(&ep->lock);
			ep->state = SCIFEP_DISCONNECTED;
			list_add_tail(&ep->list, &ms_info.mi_disconnected);
			ep->sd_state = SCIFDEV_STOPPED;
			wake_up_interruptible(&ep->sendwq);
			wake_up_interruptible(&ep->recvwq);
			wake_up_interruptible(&ep->conwq);
			spin_unlock(&ep->lock);
		}
	}
	spin_unlock_irqrestore(&ms_info.mi_connlock, sflags);
	flush_workqueue(ms_info.mi_conn_wq);
}

/*
 * Only Debug Functions Below
 */
#define SCIF_CRUMB pr_debug("%s %d\n", __func__, __LINE__)

static inline void
micscif_display_all_zombie_ep(void)
{
	struct list_head *pos, *tmpq;
	unsigned long sflags;
	struct endpt *ep;

	pr_debug("Zombie Info Start\n");
	spin_lock_irqsave(&ms_info.mi_eplock, sflags);
	list_for_each_safe(pos, tmpq, &ms_info.mi_zombie) {
		ep = list_entry(pos, struct endpt, list);
		if (!list_empty(&ep->rma_info.reg_list))
			micscif_display_all_windows(&ep->rma_info.reg_list);
		if (!list_empty(&ep->rma_info.remote_reg_list))
			micscif_display_all_windows(
				&ep->rma_info.remote_reg_list);
	}
	spin_unlock_irqrestore(&ms_info.mi_eplock, sflags);
	pr_debug("Zombie Info End\n");
}

static inline void dump_ep(scif_epd_t epd, const char *func, int line)
{
	struct endpt *ep = (struct endpt *)epd;
	pr_debug("%s %d state %d lock %p port.node 0x%x"
		"port.port 0x%x peer.node 0x%x peer.port 0x%x backlog %d qp %p"
		"qp_offset 0x%llx cnct_gnt_payload 0x%llx remote_dev %p\n", 
		func, line, ep->state, &ep->lock, ep->port.node, 
		ep->port.port, ep->peer.node, ep->peer.port, ep->backlog, 
		ep->qp_info.qp, ep->qp_info.qp_offset, 
		ep->qp_info.cnct_gnt_payload, ep->remote_dev);
}

static inline void dump_qp(volatile struct micscif_qp *qp, const char *func, int line)
{
	pr_debug("%s %d qp %p local_buf 0x%llx"
		" local_qp 0x%llx remote_buf 0x%llx remote_qp %p ep 0x%llx\n", 
		func, line, qp, qp->local_buf, 
		qp->local_qp, qp->remote_buf, qp->remote_qp, qp->ep);
}

static inline void dump_rb(struct micscif_rb *rb, const char *func, int line)
{
	pr_debug("%s %d rb %p rb_base %p *read_ptr 0x%x"
			" *write_ptr 0x%x size 0x%x"
			" cro 0x%x cwo 0x%x ocro 0x%x ocwo 0x%x\n", 
			func, line, rb, rb->rb_base, *rb->read_ptr, 
			*rb->write_ptr, rb->size, rb->current_read_offset, 
			rb->current_write_offset, 
			rb->old_current_read_offset, 
			rb->old_current_write_offset);
}

#endif /* MICSCIF_H */
