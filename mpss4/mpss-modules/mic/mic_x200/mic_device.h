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
 * Intel MIC X200 PCIe driver.
 */
#ifndef _MIC_DEVICE_H_
#define _MIC_DEVICE_H_

#include <linux/cdev.h>
#include <linux/idr.h>
#include <linux/notifier.h>
#include <linux/version.h>
#include <linux/dmaengine.h>
#ifdef MIC_IN_KERNEL_BUILD
#include <linux/vop_bus.h>
#include <linux/scif_bus.h>
#include <linux/cosm_bus.h>
#else
#include "../bus/vop_bus.h"
#include "../bus/scif_bus.h"
#include "../bus/cosm_bus.h"
#endif
#include "mic_intr.h"
#include "mic_alut.h"

/**
 * DMI (SMBIOS)
 */

/* Type 0 - BIOS Information */
#define DMI_BIOS_VENDOR              0x04
#define DMI_BIOS_VERSION             0x05
#define DMI_BIOS_RELEASE_DATE        0x08

/* Type 1 - System Information */
#define DMI_SYSTEM_MANUFACTURER      0x04
#define DMI_SYSTEM_PRODUCT_NAME      0x05
#define DMI_SYSTEM_VERSION           0x06
#define DMI_SYSTEM_SERIAL_NUMBER     0x07
#define DMI_SYSTEM_UUID              0x08
#define DMI_SYSTEM_SKU_NUMBER        0x19

/* Type 2 - Baseboard Information */
#define DMI_BASEBOARD_TYPE           0x0D

/* Type 4 - Processor Information */
#define DMI_PROCESSOR_MANUFACTURER   0x07
#define DMI_PROCESSOR_ID_DECODED     0x08
#define DMI_PROCESSOR_VERSION        0x10
#define DMI_PROCESSOR_SERIAL_NUMBER  0x20
#define DMI_PROCESSOR_PART_NUMBER    0x22

/* Type 17 - Memory Device */
#define DMI_MCDRAM_SIZE              0x0C

/* Type 148 (0x94) - IFWI Configuration Information */
#define DMI_IFWI_SMC_VERSION         0x06
#define DMI_IFWI_ME_VERSION          0x07
// TODO: implement MCDRAM version
#define DMI_IFWI_MCDRAM_VERSION      0x13
#define DMI_IFWI_FAB_VERSION         0x16
#define DMI_IFWI_NTB_EEPROM_VERSION  0x17

/**
 * enum mic_dmi_syfs_entry - MIC DMI SYSFS entries.
 */
enum mic_dmi_sysfs_entry {
	MIC_DMI_BASEBOARD_TYPE,
	MIC_DMI_BIOS_RELEASE_DATE,
	MIC_DMI_BIOS_VENDOR,
	MIC_DMI_BIOS_VERSION,
	MIC_DMI_BIOS_ME_VERSION,
	MIC_DMI_BIOS_SMC_VERSION,
	MIC_DMI_FAB_VERSION,
	MIC_DMI_MCDRAM_SIZE,
	MIC_DMI_MCDRAM_VERSION,
	MIC_DMI_MIC_DRIVER_VERSION,
	MIC_DMI_MIC_FAMILY,
	MIC_DMI_NTB_EEPROM_VERSION,
	MIC_DMI_OEM_STRINGS,
	MIC_DMI_PROCESSOR_ID_DECODED,
	MIC_DMI_PROCESSOR_MANUFACTURER,
	MIC_DMI_PROCESSOR_PART_NUMBER,
	MIC_DMI_PROCESSOR_SERIAL_NUMBER,
	MIC_DMI_PROCESSOR_STEPPING,
	MIC_DMI_PROCESSOR_VERSION,
	MIC_DMI_SYSTEM_MANUFACTURER,
	MIC_DMI_SYSTEM_PRODUCT_NAME,
	MIC_DMI_SYSTEM_SERIAL_NUMBER,
	MIC_DMI_SYSTEM_SKU_NUMBER,
	MIC_DMI_SYSTEM_UUID,
	MIC_DMI_SYSTEM_VERSION,

	/* last entry */
	MIC_DMI_LAST_ENTRY,

};

/**
 * struct mic_dmi_info - DMI Information.
 *
 * @dmi_kobj: Kobject for DMI sysfs base directory.
 * @dmi_base: DMI base address.
 * @dmi_len: Length of DMI table.
 * @dmi_num: Number of DMI entries.
 * @dmi_ver: DMI version
 * @dmi_avail: DMI is available.
 * @sysfs_entry: Array of SYSFS entries.
 */
struct mic_dmi_info {
	u32 dmi_base;
	u16 dmi_len;
	u16 dmi_num;
	u16 dmi_ver;
	bool dmi_avail;
	size_t mcdram_size;
	char *sysfs_entry[MIC_DMI_LAST_ENTRY];
};

/**
 * enum mic_stepping - MIC stepping ids.
 */
enum mic_stepping {
	MIC_STEP_UNKNOWN = 0x0,
	MIC_STEP_A0 = 0x1,
	MIC_STEP_B0 = 0x2,
};

/**
 * struct mic_device -  MIC device information for each card.
 *
 * @mmio: MMIO bar information.
 * @aper: Aperture bar information.
 * @family: The MIC family to which this device belongs.
 * @id: The unique device id for this MIC device.
 * @stepping: Stepping ID.
 * @pdev: The PCIe device
 * @alut: ALUT information.
 * @intr_info: H/W specific interrupt information.
 * @irq_info: The OS specific irq information
 * @dmi_info: MIC DMI information.
 * @mic_mutex: Mutex for synchronizing access to mic data structures.
 * @dbg_dir: debugfs directory of this MIC device.
 * @bootaddr: MIC boot address.
 * @dp: virtio device page
 * @rdp: remote virtio device page
 * @dp_dma_addr: virtio device page DMA address.
 * @link_side: PEX Link Side
 * @reg_base: Register base offset
 * @intr_reg_base: Interrupt register base offset
 * @peer_intr_reg_base: Remote Interrupt register base offset
 * @dma_ch - DMA channel
 * @vpdev: Virtio over PCIe device on the VOP virtual bus.
 * @scdev: SCIF device on the SCIF virtual bus.
 * @cosm_dev: COSM device on the COSM bus
 * @spad_kobj: Kobject for SPAD sysfs base directory.
 * @peer_count: number of peer x200 cards
 * @net_peer_count: number infiniband/network cards
 */
struct mic_device {
	struct mic_mw mmio;
	struct mic_mw aper;
	int id;
	enum mic_stepping stepping;
	struct pci_dev *pdev;
	struct mic_alut_info *alut;
	struct mic_intr_info *intr_info;
	struct mic_irq_info irq_info;
	struct mic_dmi_info dmi_info;
	struct mutex mic_mutex;
	struct dentry *dbg_dir;
	u64 bootaddr;
	void *dp;
	void __iomem *rdp;
	dma_addr_t dp_dma_addr;
	bool link_side;
	u32 reg_base;
	u32 intr_reg_base;
	u32 peer_intr_reg_base;
	struct dma_chan *dma_ch;
	struct vop_device *vpdev;
	struct scif_hw_dev *scdev;
	struct cosm_device *cosm_dev;
	int peer_count;
	int net_peer_count;
};

/**
 * mic_mmio_read - read from an MMIO register.
 * @mw: MMIO register base virtual address.
 * @offset: register offset.
 *
 * RETURNS: register value.
 */
static inline u32 mic_mmio_read(struct mic_mw *mw, u32 offset)
{
	return ioread32(mw->va + offset);
}

/**
 * mic_mmio_write - write to an MMIO register.
 * @mw: MMIO register base virtual address.
 * @val: the data value to put into the register
 * @offset: register offset.
 *
 * RETURNS: none.
 */
static inline void
mic_mmio_write(struct mic_mw *mw, u32 val, u32 offset)
{
	iowrite32(val, mw->va + offset);
}

void mic_bootparam_init(struct mic_device *xdev);
void mic_stop(struct mic_device *xdev);
void mic_create_debug_dir(struct mic_device *dev);
void mic_delete_debug_dir(struct mic_device *dev);
void __init mic_init_debugfs(void);
void mic_exit_debugfs(void);
int mic_scif_setup(struct mic_device *xdev);
int mic_dmi_scan(struct mic_device *xdev);
void mic_dmi_scan_uninit(struct mic_device *xdev);
extern struct dma_map_ops _mic_dma_ops;
extern struct vop_hw_ops vop_hw_ops;
extern struct cosm_hw_ops cosm_mic_hw_ops;

int mic_sysfs_init(struct cosm_device *cdev);
void mic_sysfs_uninit(struct cosm_device *cdev);

#endif
