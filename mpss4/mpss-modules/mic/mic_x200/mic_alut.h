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
 * Intel MIC Host driver.
 */
#ifndef MIC_ALUT_H
#define MIC_ALUT_H
/**
 * struct mic_alut - MIC ALUT entry information.
 * @dma_addr: Base DMA address for this ALUT entry.
 * @ref_count: Number of active mappings for this ALUT entry in bytes.
 */
struct mic_alut {
	dma_addr_t dma_addr;
	s64 ref_count;
	u8 perm;
};

/**
 * struct mic_alut_info - MIC ALUT information.
 * @entry: Array of ALUT entries.
 * @alut_lock: Spin lock protecting access to ALUT data structures.
 * @info: Hardware specific ALUT information.
 * @identity_map_size: identity map size (in bytes).
 * @identity_map_entries: identity map size (in entries).
 * @ref_count: Number of active ALUT mappings (for debug).
 * @map_count: Number of ALUT mappings created (for debug).
 * @unmap_count: Number of ALUT mappings destroyed (for debug).
 */
struct mic_alut_info {
	struct mic_alut *entry;
	spinlock_t alut_lock;
	u64 identity_map_size;
	u64 identity_map_entries;
	s64 ref_count;
	s64 map_count;
	s64 unmap_count;
};

dma_addr_t mic_map_single(struct mic_device *mdev, void *va, size_t size);
void mic_unmap_single(struct mic_device *mdev,
	dma_addr_t mic_addr, size_t size);
dma_addr_t mic_map(struct mic_device *mdev,
	dma_addr_t dma_addr, size_t size);
void mic_unmap(struct mic_device *mdev, dma_addr_t mic_addr, size_t size);
dma_addr_t mic_to_dma_addr(struct mic_device *mdev, dma_addr_t mic_addr);

/**
 * mic_map_error - Check a MIC address for errors.
 *
 * @mdev: pointer to mic_device instance.
 *
 * returns Whether there was an error during mic_map..(..) APIs.
 */
static inline bool mic_map_error(dma_addr_t mic_addr)
{
	return !mic_addr;
}

int  mic_alut_init(struct mic_device *xdev);
void mic_alut_uninit(struct mic_device *xdev);
int  mic_alut_restore(struct mic_device *xdev);
void mic_alut_enable(struct mic_device *xdev);
#endif
