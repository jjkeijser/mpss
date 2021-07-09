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
#ifndef __MIC_DEV_H__
#define __MIC_DEV_H__

#include <linux/types.h>
#include <linux/dma_remapping.h>
#include <linux/dma-mapping.h>
#include <linux/version.h>

/* The maximum number of MIC devices supported in a single host system. */
#define MIC_MAX_NUM_DEVS 128

/**
 * mic_intr_source - The type of source that will generate
 * the interrupt.The number of types needs to be in sync with
 * MIC_NUM_INTR_TYPES
 *
 * MIC_INTR_DB: The source is a doorbell
 * MIC_INTR_DMA: The source is a DMA channel
 * MIC_INTR_ERR: The source is an error interrupt e.g. SBOX ERR
 * MIC_NUM_INTR_TYPES: Total number of interrupt sources.
 */
enum mic_intr_type {
	MIC_INTR_DB = 0,
	MIC_INTR_DMA,
	MIC_INTR_ERR,
	MIC_NUM_INTR_TYPES
};

/**
 * enum mic_hw_family - The hardware family to which a device belongs.
 */
enum mic_hw_family {
	MIC_FAMILY_X100 = 0,
	MIC_FAMILY_X200,
	MIC_FAMILY_UNKNOWN,
	MIC_FAMILY_LAST
};

/**
 * struct mic_mw - MIC memory window
 *
 * @pa: Base physical address.
 * @va: Base ioremap'd virtual address.
 * @len: Size of the memory window.
 */
struct mic_mw {
	phys_addr_t pa;
	void __iomem *va;
	resource_size_t len;
};

/*
 * Scratch pad register offsets used by the host to communicate
 * device page DMA address to the card.
 */
#define MIC_DPLO_SPAD 14
#define MIC_DPHI_SPAD 15

/*
 * These values are supposed to be in the config_change field of the
 * device page when the host sends a config change interrupt to the card.
 */
#define MIC_VIRTIO_PARAM_DEV_REMOVE 0x1
#define MIC_VIRTIO_PARAM_CONFIG_CHANGED 0x2

/* Maximum number of DMA channels */
#define MIC_MAX_DMA_CHAN 4

#if defined(RHEL_RELEASE_VERSION)
	#if RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7, 3)
		#define IOMMU_SUPPORTS_KNL
	#endif
#elif defined(CONFIG_SUSE_KERNEL)
	#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0)
		#define IOMMU_SUPPORTS_KNL
	#endif
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 7, 0)
	#define IOMMU_SUPPORTS_KNL
#endif

#if defined(RHEL_RELEASE_VERSION)
	#define DMA_ALIAS_MEMBER pci_dev_rh->dma_alias_mask
#else
	#define DMA_ALIAS_MEMBER dma_alias_mask
#endif

#endif
