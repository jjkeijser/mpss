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

#if !defined(__MIC_COMMON_H)
#define __MIC_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

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
#include <mic/bootparams.h>
#include <mic/micsboxdefine.h>
#include <mic/micdboxdefine.h>
#include <mic/ringbuffer.h>
#include <mic/micscif.h>
#ifdef USE_VCONSOLE
#include <mic/micvcons.h>
#endif
#include <mic/micpsmi.h>
#include <mic/io_interface.h>
#include <mic/mic_pm.h>
#include <mic/mic_dma_api.h>
#include <mic/micveth_common.h>
#include <mic/micscif_nm.h>

#define GET_MAX(a, b)	( ((a) > (b)) ? (a) : (b) )
#define GET_MIN(a, b)	( ((a) < (b)) ? (a) : (b) )

// System Interrupt Cause Read Register 0
#define SBOX_SICR0_DBR(x)		((x) & 0xf)
#define SBOX_SICR0_DMA(x)		(((x) >> 8) & 0xff)

// System Interrupt Cause Enable Register 0
#define SBOX_SICE0_DBR(x)		((x) & 0xf)
#define SBOX_SICE0_DBR_BITS(x)		((x) & 0xf)
#define SBOX_SICE0_DMA(x)		(((x) >> 8) & 0xff)
#define SBOX_SICE0_DMA_BITS(x)		(((x) & 0xff) << 8)

// System Interrupt Cause Read Register 1
#define SBOX_SICR1_SBOXERR(x)		((x) & 0x1)
#define SBOX_SICR1_SPIDONE(x)		(((x) >> 4) & 0x1)

// System Interrupt Cause Set Register 1
#define SBOX_SICC1_SBOXERR(x)		((x) & 0x1)
#define SBOX_SICC1_SPIDONE(x)		(((x) >> 4) & 0x1)

// Offsets in the MMIO Range for register segments
#define HOST_DBOX_BASE_ADDRESS		0x00000000
#define HOST_SBOX_BASE_ADDRESS		0x00010000
#define HOST_GTT_BASE_ADDRESS		0x00040000

#define SCRATCH0_MEM_TEST_DISABLE(x)	((x) & 0x1)
#define SCRATCH0_MEM_USAGE(x)		(((x) >> 1) & 0x3)
#define    SCR0_MEM_ALL			0x0
#define    SCR0_MEM_HALF		0x1
#define    SCR0_MEM_THIRD		0x2
#define    SCR0_MEM_FOURTH		0x3
#define SCRATCH0_MEM_SIZE_KB(x)		((x) >> 0x3)

#define SCRATCH2_DOWNLOAD_STATUS(x)	((x) & 0x1)

#define SCRATCH2_CLEAR_DOWNLOAD_STATUS(x)	((x) & ~0x1)
#define SCRATCH2_APIC_ID(x)		(((x) >> 1) & 0x1ff)
#define SCRATCH2_DOWNLOAD_ADDR(x)	((x) & 0xfffff000)

#define SCRATCH13_SUB_STEP(x)		((x) & 0xf)
#define SCRATCH13_STEP_ID(x)		(((x) >> 4) & 0xf)
#define SCRATCH13_PLATFORM_ID(x)	(((x) >> 18) & 0x3)


#define MEMVOLT_MEMVOLT(x)     (((x) >>SHIFT_MEMVOLT) & MASK_MEMVOLT)
#define MEMFREQ_MEMFREQ(x)     (((x) >>SHIFT_MEMORYFREQ) & MASK_MEMORYFREQ)
#define FAILSAFEOFFSET_FAILSAFE(x) (((x) >>SHIFT_FAIL_SAFE) & MASK_FAIL_SAFE)

#define SCRATCH4_ACTIVE_CORES(x) (((x) >>SHIFT_ACTIVE_CORES) & MASK_ACTIVE_CORES)
#define SCRATCH0_MEMSIZE(x) (((x) >>SHIFT_MEMSIZE) & MASK_MEMSIZE)
#define SCRATCH7_FLASHVERSION(x) (((x) >>SHIFT_FLASHVERSION) & MASK_FLASHVERSION)
#define SCRATCH7_FUSECONFIGREV(x) (((x) >>SHIFT_FUSE_CONFIG_REV) & MASK_FUSE_CONFIG_REV)
#define SCRATCH13_MODEL(x) (((x) >>SHIFT_MODEL) & MASK_MODEL)
#define SCRATCH13_FAMILY_DATA(x) (((x) >>SHIFT_FAMILY_DATA) & MASK_FAMILY_DATA)
#define SCRATCH13_PROCESSOR(x) (((x) >>SHIFT_PROCESSOR) & MASK_PROCESSOR)
#define SCRATCH13_EXTENDED_MODEL(x) (((x) >>SHIFT_EXTENDED_MODEL) & MASK_EXTENDED_MODEL)
#define SCRATCH13_EXTENDED_FAMILY(x) (((x) >>SHIFT_EXTENDED_FAMILY) & MASK_EXTENDED_FAMILY)


#define DBOX_READ(mmio, offset) \
	readl((uint32_t*)((uint8_t*)(mmio) + (HOST_DBOX_BASE_ADDRESS + (offset))))
#define DBOX_WRITE(value, mmio, offset) \
	writel((value), (uint32_t*)((uint8_t*)(mmio) + (HOST_DBOX_BASE_ADDRESS + (offset))))

#define SBOX_READ(mmio, offset) \
	readl((uint32_t*)((uint8_t*)(mmio) + (HOST_SBOX_BASE_ADDRESS + (offset))))
#define SBOX_WRITE(value, mmio, offset) \
	writel((value), (uint32_t*)((uint8_t*)(mmio) + (HOST_SBOX_BASE_ADDRESS + (offset))))

#define SET_BUS_DEV_FUNC(bus, device, function, reg_offset) \
	(( bus << 16 ) | ( device << 11 ) | ( function << 8 ) | reg_offset)

#define GTT_READ(mmio, offset) \
	readl((uint32_t*)((uint8_t*)(mmio) + (HOST_GTT_BASE_ADDRESS + (offset))))
#define GTT_WRITE(value, mmio, offset) \
	writel((value), (uint32_t*)((uint8_t*)(mmio) + (HOST_GTT_BASE_ADDRESS + (offset))))


#define ENABLE_MIC_INTERRUPTS(mmio) { \
	uint32_t sboxSice0reg = SBOX_READ((mmio), SBOX_SICE0); \
	sboxSice0reg |= SBOX_SICE0_DBR_BITS(0xf) | SBOX_SICE0_DMA_BITS(0xff); \
	SBOX_WRITE(sboxSice0reg, (mmio), SBOX_SICE0); }

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))
#endif

#define DLDR_APT_BAR 0
#define DLDR_MMIO_BAR 4

#define PCI_VENDOR_INTEL	0x8086

#define PCI_DEVICE_ABR_2249	0x2249
#define PCI_DEVICE_ABR_224a	0x224a

#define PCI_DEVICE_KNC_2250	0x2250
#define PCI_DEVICE_KNC_2251	0x2251
#define PCI_DEVICE_KNC_2252	0x2252
#define PCI_DEVICE_KNC_2253	0x2253
#define PCI_DEVICE_KNC_2254	0x2254
#define PCI_DEVICE_KNC_2255	0x2255
#define PCI_DEVICE_KNC_2256	0x2256
#define PCI_DEVICE_KNC_2257	0x2257
#define PCI_DEVICE_KNC_2258	0x2258
#define PCI_DEVICE_KNC_2259	0x2259
#define PCI_DEVICE_KNC_225a	0x225a

#define PCI_DEVICE_KNC_225b	0x225b
#define PCI_DEVICE_KNC_225c	0x225c
#define PCI_DEVICE_KNC_225d	0x225d
#define PCI_DEVICE_KNC_225e	0x225e

#define MIC_CMDLINE_BUFSIZE	1024
#define RESET_FAIL_TIME		300

/* Masks for sysfs entries */
#ifdef CONFIG_ML1OM
#define MASK_COREVOLT 0xff
#define MASK_COREFREQ 0xfff
#endif
#define MASK_MEMVOLT 0xff
#define MASK_MEMORYFREQ 0xff
#define MASK_MEMSIZE 0x1fffffff
#define MASK_FLASHVERSION 0xffff
#define MASK_SUBSTEPPING_DATA 0xf
#define MASK_STEPPING_DATA 0xf
#define MASK_MODEL 0xf
#define MASK_FAMILY_DATA 0xf
#define MASK_PROCESSOR 0x3
#define MASK_PLATFORM 0x3
#define MASK_EXTENDED_MODEL 0xf
#define MASK_EXTENDED_FAMILY 0xff
#define MASK_FUSE_CONFIG_REV 0x3ff
#define MASK_ACTIVE_CORES 0x3f
#define MASK_FAIL_SAFE 0xffffffff
#define MASK_FLASH_UPDATE 0xffffffff
/* Shifts for sysfs entries */
#ifdef CONFIG_ML1OM
#define SHIFT_COREVOLT 0
#define SHIFT_COREFREQ 0
#endif
#define SHIFT_MEMVOLT 0
#define SHIFT_MEMORYFREQ 0
#define SHIFT_MEMSIZE 3
#define SHIFT_FLASHVERSION 16
#define SHIFT_SUBSTEPPING_DATA 0
#define SHIFT_STEPPING_DATA 4
#define SHIFT_MODEL 8
#define SHIFT_FAMILY_DATA 12
#define SHIFT_PROCESSOR 16
#define SHIFT_PLATFORM 18
#define SHIFT_EXTENDED_MODEL 20
#define SHIFT_EXTENDED_FAMILY 24
#define SHIFT_FUSE_CONFIG_REV 0
#define SHIFT_ACTIVE_CORES 10
#define SHIFT_FAIL_SAFE 0
#define SHIFT_FLASH_UPDATE 0

#define SKU_NAME_LEN	20

/* Should be updated to reflect the latest interface version in sysfs and wmi property */
#define LINUX_INTERFACE_VERSION "1.0"
#define WINDOWS_INTERFACE_VERSION "1.0"

typedef enum mic_modes
{
	MODE_NONE,
	MODE_LINUX,
	MODE_ELF,
	MODE_FLASH
} MIC_MODES;

typedef enum mic_status
{
	MIC_READY,
	MIC_BOOT,
	MIC_NORESPONSE,
	MIC_BOOTFAIL,
	MIC_ONLINE,
	MIC_SHUTDOWN,
	MIC_LOST,
	MIC_RESET,
	MIC_RESETFAIL,
	MIC_INVALID
} MIC_STATUS;

typedef enum _product_platform_t
{
	PLATFORM_SILICON = 0,
	PLATFORM_EMULATOR = 2,
}product_platform_t;


typedef enum _platform_resource_type
{
	PCI_APERTURE,
	MMIO,
	MAX_RESOURCE_TYPE
}platform_resource_type;

typedef struct _platform_resource_t
{
	uint8_t* va; // mapped by driver
	uint64_t pa; // from PCI config space
	uint64_t len;// from PCI config space
}platform_resource_t;


typedef struct micscifhost_info {
	dma_addr_t	si_pa;
	struct delayed_work	si_bs_check;
	uint32_t		si_bs_wait_count;
} scifhost_info_t;

#define MIC_NUM_DB 4
typedef struct mic_irq {
	spinlock_t		mi_lock;
	struct list_head	mi_dblist[MIC_NUM_DB];  // The 4 doorbell interrupts.
	atomic_t		mi_received;
} mic_irq_t;

typedef struct sysfs_info {
	char	*cmdline;
	char	*kernel_cmdline;
} sysfs_info_t;

typedef struct pm_recv_msg {
	struct list_head msg;
	pm_msg_header msg_header;
	void * msg_body;
} pm_recv_msg_t;

typedef struct pm_wq {
	struct workqueue_struct	*wq;
	struct work_struct		work;
	char			wq_name[20];
} pm_wq_t;

/*
 * Driver wide power management context
 * common power management context for all the devices
 */
typedef struct micscif_pm {
	scif_epd_t 		epd;
	atomic_t		connected_clients;
	pm_wq_t			accept;
	struct mutex		pm_accept_mutex;
	struct mutex		pm_idle_mutex;
	struct dentry		*pmdbgparent_dir;
	uint32_t		enable_pm_logging;
	atomic_t		wakeup_in_progress;
	uint8_t			*nodemask;
	uint32_t		nodemask_len;
} micscif_pm_t;

/* per device power management context */
typedef struct micpm_ctx
{
	scif_epd_t		pm_epd;
	PM_IDLE_STATE		idle_state;
	struct mutex		msg_mutex;
	struct list_head	msg_list;
	uint32_t		pc6_timeout;
	struct work_struct		pm_close;
	MIC_STATUS		mic_suspend_state;
	bool			pc3_enabled;
	bool			pc6_enabled;
	pm_msg_pm_options	pm_options;
	atomic_t		pm_ref_cnt;
	platform_resource_t	nodemask;
	pm_wq_t			recv;
	pm_wq_t			handle_msg;
	pm_wq_t			resume;
	struct workqueue_struct	*pc6_entry_wq;
	struct delayed_work	pc6_entry_work;
	char			pc6_wq_name[20];
	struct dentry		*pmdbg_dir;
	PM_CONNECTION_STATE con_state;
	wait_queue_head_t	disc_wq;
} micpm_ctx_t;

typedef struct _mic_ctx_t {
	platform_resource_t	mmio;
	platform_resource_t	aper;
	uint32_t		apic_id;
	uint32_t		msie;
	ringbuffer		ringbuff[MIC_ENG_MAX_SUPPORTED_ENGINES];
	uint32_t rb_readoff __attribute__((aligned(64)));
	micpm_ctx_t		micpm_ctx;
	CARD_USAGE_MODE		card_usage_mode;
	uint64_t		adptr_base_pa;

	int32_t			bi_id;
	mic_irq_t		bi_irq;
	struct tasklet_struct		bi_dpc;
	scifhost_info_t		bi_scif;
#ifdef USE_VCONSOLE
	micvcons_t		bi_vcons;
#endif
	void			*bi_vethinfo;
	struct mic_psmi_ctx	bi_psmi;
	struct pci_dev		*bi_pdev;

	MIC_STATUS		state;
	struct mutex		state_lock;
	MIC_MODES		mode;
	wait_queue_head_t	resetwq;
	char			*image;
	char			*initramfs;
	struct timer_list	boot_timer;
	unsigned long	boot_start;
	struct work_struct		boot_ws;

	struct workqueue_struct	*resetworkq;
	struct work_struct		resetwork;
	struct workqueue_struct	*ioremapworkq;
	struct work_struct		ioremapwork;
	wait_queue_head_t	ioremapwq;
	uint32_t		reset_count;

	atomic_t		bi_irq_received;
	uint8_t			bi_stepping;
	uint8_t			bi_substepping;
	product_platform_t	bi_platform;
	product_family_t	bi_family;
	struct board_info	*bd_info;
	sysfs_info_t		sysfs_info;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0))
	struct kernfs_node	*sysfs_state;
#else
	struct sysfs_dirent	*sysfs_state;
#endif
	spinlock_t		sysfs_lock;
	mic_dma_handle_t	dma_handle;
	uint32_t		boot_mem;
	mic_smpt_t		*mic_smpt;
	spinlock_t		smpt_lock;
	uint32_t		sdbic1;
	int64_t			etc_comp;
	spinlock_t		ramoops_lock;
	void			*ramoops_va[2];
	int			ramoops_size;
	dma_addr_t	ramoops_pa[2];
	struct proc_dir_entry	*ramoops_dir;
	struct proc_dir_entry	*vmcore_dir;
	/*
	 * List representing chunks of contiguous memory areas and
	 * their offsets in vmcore file.
	 */
	struct list_head	vmcore_list;
	/* Stores the pointer to the buffer containing kernel elf core headers */
	char			*elfcorebuf;
	size_t			elfcorebuf_sz;
	/* Total size of vmcore file. */
	uint64_t		vmcore_size;
	int			crash_count;
	int			boot_count;
	void			*log_buf_addr;
	int			*log_buf_len;
	char			sku_name[SKU_NAME_LEN];
	atomic_t		disconn_rescnt;
	atomic_t		gate_interrupt;
	uint16_t            numa_node;
} mic_ctx_t;


typedef struct mic_irqhander {
	int (*ih_func)(mic_ctx_t *mic_ctx, int doorbell);
	struct list_head ih_list;
	char *ih_idstring;
} mic_irqhandler_t;

/* SKU related definitions and declarations */
#define MAX_DEV_IDS 16
typedef struct sku_info {
	uint32_t fuserev_low;
	uint32_t fuserev_high;
	uint32_t memsize;
	uint32_t memfreq;
	char sku_name[SKU_NAME_LEN];
	struct list_head sku;
} sku_info_t;

int sku_create_node(uint32_t fuserev_low,
		uint32_t fuserev_high, uint32_t mem_size,
		uint32_t mem_freq, char *sku_name,
		sku_info_t ** newnode);

int sku_build_table(void);
void sku_destroy_table(void);
int sku_find(mic_ctx_t *mic_ctx, uint32_t device_id);

/* End SKU related definitions and declarations */

#define MIC_NUM_MSIX_ENTRIES 1
typedef struct mic_data {
	int32_t			dd_numdevs;
	int32_t			dd_inuse;
#ifdef USE_VCONSOLE
	micvcons_port_t		dd_ports[MAX_BOARD_SUPPORTED];
#endif
	struct board_info	*dd_bi[MAX_BOARD_SUPPORTED];
	struct list_head	dd_bdlist;
	micscif_pm_t		dd_pm;
	uint64_t		sysram;
	struct fasync_struct	*dd_fasync;
	struct list_head	sku_table[MAX_DEV_IDS];
} mic_data_t;

#include "mic_interrupts.h"
extern mic_data_t mic_data;
extern struct micscif_dev scif_dev[];

typedef struct acptboot_data {
	scif_epd_t		listen_epd;
	uint16_t		acptboot_pn;
	struct workqueue_struct	*acptbootwq;
	struct work_struct		acptbootwork;
}acptboot_data_t;

void acptboot_exit(void);
int acptboot_init(void);
void adapter_init(void);
int adapter_isr(mic_ctx_t *mic_ctx);
int adapter_imsr(mic_ctx_t *mic_ctx);
int adapter_remove(mic_ctx_t *mic_ctx);
int adapter_do_ioctl(uint32_t cmd, uint64_t arg);
int adapter_stop_device(mic_ctx_t *mic_ctx, int wait_reset, int reattempt);
int adapter_shutdown_device(mic_ctx_t *mic_ctx);
void calculate_etc_compensation(mic_ctx_t *mic_ctx);
int adapter_probe(mic_ctx_t *mic_ctx);
int adapter_post_boot_device(mic_ctx_t *mic_ctx);
int adapter_start_device(mic_ctx_t *mic_ctx);
int adapter_restart_device(mic_ctx_t *mic_ctx);
int adapter_init_device(mic_ctx_t *mic_ctx);
int pm_adapter_do_ioctl(mic_ctx_t *mic_ctx, void *in_buffer);
int adapter_reset_depgraph(mic_ctx_t *mic_ctx);

/*
 * RESET_WAIT : launch the timer thread and wait for reset to complete
 * 		The caller has to add itself to the resetwq by calling wait_for_reset
 * RESET_REATTEMPT : Reattempt reset after detecting failures in reset
 */
#define RESET_WAIT	1
#define RESET_REATTEMPT	1
void adapter_reset(mic_ctx_t *mic_ctx, int wait_reset, int reattempt);

void adapter_wait_reset(mic_ctx_t *mic_ctx);
void get_adapter_memsize(uint8_t *mmio_va, uint32_t *adapter_mem_size);
int wait_for_bootstrap(uint8_t *mmio_va);
void post_boot_startup(struct work_struct *work);
void attempt_reset(struct work_struct *work);

int send_uos_escape(mic_ctx_t *mic_ctx, uint32_t uos_op,
		    uint32_t data_size, void *escape_data);
int boot_linux_uos(mic_ctx_t *mic_ctx, char *imgname, char *initramfsname);

int boot_micdev_app(mic_ctx_t *mic_ctx, char *imgname);
int allocate_tools_buffer(mic_ctx_t *mic_ctx, uint32_t databuf_size,
			  uint32_t stsbuf_size, uint64_t *gddr_data_ptr,
			  uint64_t *gddr_stsbuf_ptr);

int micpm_init(void);
void micpm_uninit(void);
int micpm_stop(mic_ctx_t *mic_ctx);
int micpm_start(mic_ctx_t *mic_ctx);
int micpm_probe(mic_ctx_t *mic_ctx);
int micpm_remove(mic_ctx_t *mic_ctx);
void micpm_nodemask_uninit(mic_ctx_t* mic_ctx);
int micpm_nodemask_init(uint32_t num_devs, mic_ctx_t* mic_ctx);
int micpm_disconn_init(uint32_t num_nodes);
int micpm_disconn_uninit(uint32_t num_nodes);
int micpm_dbg_init(mic_ctx_t *mic_ctx);
void micpm_dbg_parent_init(void);
int pm_reg_read(mic_ctx_t *mic_ctx, uint32_t regoffset);
int micpm_update_pc6(mic_ctx_t *mic_ctx, bool set);
int micpm_update_pc3(mic_ctx_t *mic_ctx, bool set);
int pm_start_device(mic_ctx_t *mic_ctx);
int pm_stop_device(mic_ctx_t *mic_ctx);
int mic_pm_recv(mic_ctx_t *mic_ctx, void *msg, uint32_t len);
int mic_pm_send_msg(mic_ctx_t *mic_ctx, PM_MESSAGE type,
		void *msg, uint32_t len);

int pm_pc3_entry(mic_ctx_t *mic_ctx);
int pm_pc3_exit(mic_ctx_t *mic_ctx);
int do_idlestate_entry(mic_ctx_t *mic_ctx);
int do_idlestate_exit(mic_ctx_t *mic_ctx, bool get_ref);
int is_idlestate_exit_needed(mic_ctx_t *mic_ctx);
uint32_t mic_get_scifnode_id(mic_ctx_t *mic_ctx);

mic_ctx_t* get_per_dev_ctx(uint16_t node);
int get_num_devs(mic_ctx_t *mic_ctx, uint32_t *num_devs);


void adapter_uninit(void);
void adapter_add(mic_ctx_t *mic_ctx);
void adapter_start(mic_ctx_t *mic_ctx);
int send_flash_cmd(mic_ctx_t *mic_ctx, MIC_FLASH_CMD_TYPE type, void *data,
											uint32_t len);
int cmdline_mem(mic_ctx_t *mic_ctx, uint32_t mem);
int get_cardside_mem(mic_ctx_t *mic_ctx, uint64_t start, uint64_t size, void *dest);

int mic_pin_user_pages (void *data, struct page **pages, uint32_t len, int32_t *nf_pages, int32_t nr_pages);
int mic_unpin_user_pages(struct page **pages, uint32_t nf_pages);
product_family_t get_product_family(uint32_t device_id);
void show_stepping_comm(mic_ctx_t *mic_ctx,char *buf);
void micscif_destroy_p2p(mic_ctx_t *mic_ctx);

#ifdef HOST
void mic_smpt_init(mic_ctx_t *mic_ctx);
void mic_smpt_restore(mic_ctx_t *mic_ctx);
#endif
void mic_smpt_uninit(mic_ctx_t *mic_ctx);
int mic_dma_init(void);

#ifndef _MIC_SCIF_
static __always_inline int micpm_get_reference(mic_ctx_t *mic_ctx, bool force_wakeup) {
	int err;
	if (!mic_ctx)
		return -EINVAL;

	if (mic_ctx->micpm_ctx.idle_state == PM_IDLE_STATE_LOST)
		return -ENODEV;

	if (unlikely(!atomic_add_unless(&mic_ctx->micpm_ctx.pm_ref_cnt, 
		1, PM_NODE_IDLE))) {
		if (!force_wakeup) {
			if (is_idlestate_exit_needed(mic_ctx)) {
				return -EAGAIN;
			}
		}

		if ((err = micscif_connect_node(mic_get_scifnode_id(mic_ctx), true)) != 0)
			return -ENODEV;
	}
	return 0;
}
#endif

static __always_inline int micpm_put_reference(mic_ctx_t *mic_ctx) {
	int ret;

	if(!mic_ctx)
		return -EINVAL;

	if (mic_ctx->micpm_ctx.idle_state == PM_IDLE_STATE_LOST)
		return -ENODEV;

	if (unlikely((ret = atomic_sub_return(1, 
			&mic_ctx->micpm_ctx.pm_ref_cnt)) < 0)) {
		printk(KERN_ERR "%s %d Invalid PM ref_cnt %d \n", 
			__func__, __LINE__, atomic_read(&mic_ctx->micpm_ctx.pm_ref_cnt));
	}

	return 0;

}

static __always_inline int
mic_hw_family(int node_id) {
	mic_ctx_t *mic_ctx;

	/* For Host Loopback */
	if (!node_id)
		return -EINVAL;

	mic_ctx = get_per_dev_ctx(node_id - 1);
	return mic_ctx->bi_family;
}

static __always_inline void
wait_for_reset(mic_ctx_t *mic_ctx)
{
	int ret = 0;
	while (!ret) {
	ret = wait_event_timeout(mic_ctx->resetwq, 
			mic_ctx->state != MIC_RESET, RESET_FAIL_TIME * HZ);
	}
}

/* Called only by host PM suspend */
static __always_inline int
wait_for_shutdown_and_reset(mic_ctx_t *mic_ctx)
{
	int ret;
	ret = wait_event_interruptible_timeout(mic_ctx->resetwq, 
		mic_ctx->state != MIC_RESET && mic_ctx->state != MIC_SHUTDOWN, 
		RESET_FAIL_TIME * HZ);
	return ret;
}

static __always_inline void
mic_signal_daemon(void)
{
	if (mic_data.dd_fasync != NULL)
		kill_fasync(&mic_data.dd_fasync, SIGIO, POLL_IN);
}

extern char *micstates[];

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
#define __mic_create_singlethread_workqueue(name)	alloc_ordered_workqueue(name, 0)
#else
#define __mic_create_singlethread_workqueue(name)	create_singlethread_workqueue(name)
#endif

static __always_inline void
mic_setstate(mic_ctx_t *mic_ctx, enum mic_status newstate)
{
	printk("mic%d: Transition from state %s to %s\n", mic_ctx->bi_id, 
		    micstates[mic_ctx->state], micstates[newstate]);
	mic_ctx->state = newstate;
	spin_lock_bh(&mic_ctx->sysfs_lock);
	if (mic_ctx->sysfs_state)
		sysfs_notify_dirent(mic_ctx->sysfs_state);
	spin_unlock_bh(&mic_ctx->sysfs_lock);
}

#define MICREG_POSTCODE 0x242c

static __always_inline uint32_t
mic_getpostcode(mic_ctx_t *mic_ctx)
{
        return DBOX_READ(mic_ctx->mmio.va, MICREG_POSTCODE);
}

static __always_inline int
mic_hw_stepping(int node_id) {
	mic_ctx_t *mic_ctx;

	/* For Host Loopback */
	if (!node_id)
		return -EINVAL;

	mic_ctx = get_per_dev_ctx(node_id - 1);
	return mic_ctx->bi_stepping;
}

#define MIC_IRQ_DB0	0
#define MIC_IRQ_DB1	1
#define MIC_IRQ_DB2	2
#define MIC_IRQ_DB3	3
#define MIC_IRQ_MAX	MIC_IRQ_DB3

int mic_reg_irqhandler(mic_ctx_t *mic_ctx, int doorbell, char *idstring,
		       int (*irqfunc)(mic_ctx_t *mic_ctx, int doorbell));
int mic_unreg_irqhandler(mic_ctx_t *mic_ctx, int doorbell, char *idstring);
void mic_enable_interrupts(mic_ctx_t *mic_ctx);
void mic_disable_interrupts(mic_ctx_t *mic_ctx);
void mic_enable_msi_interrupts(mic_ctx_t *mic_ctx);

int micscif_init(void);
void micscif_destroy(void);
void micscif_probe(mic_ctx_t *mic_ctx);
void micscif_remove(mic_ctx_t *mic_ctx);
void micscif_start(mic_ctx_t *mic_ctx);
void micscif_stop(mic_ctx_t *mic_ctx);

mic_ctx_t *get_device_context(struct pci_dev *dev);
void ramoops_exit(void);
void vmcore_exit(void);
int vmcore_create(mic_ctx_t *mic_ctx);
void vmcore_remove(mic_ctx_t *mic_ctx);

// loads file into memory
int mic_get_file_size(const char *path, uint32_t *file_length);
int mic_load_file(const char *fn, uint8_t *buffer, uint32_t max_size);
#ifndef _MIC_SCIF_
void mic_debug_init(mic_ctx_t *mic_ctx);
#endif
void mic_debug_uninit(void);
void
set_pci_aperture(mic_ctx_t *mic_ctx, uint32_t gtt_index, uint64_t phy_addr, uint32_t num_bytes);
#ifdef __cplusplus
};
#endif

#endif // __MIC_COMMON_H

