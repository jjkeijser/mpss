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
#include <linux/pci.h>
#include <linux/moduleparam.h>

#include "../common/mic_dev.h"
#include "mic_device.h"
#include "mic_hw.h"
#include "mic_alut.h"

static inline u8 mic_sys_addr_to_alut(struct mic_device *xdev, dma_addr_t pa)
{
	return (pa / ALUT_ENTRY_SIZE);
}

static inline u64 mic_alut_to_pa(struct mic_device *xdev, u8 index)
{
	return index * ALUT_ENTRY_SIZE;
}

static inline u64 mic_alut_offset(struct mic_device *xdev, dma_addr_t pa)
{
	return pa % ALUT_ENTRY_SIZE;
}

static inline u64 mic_alut_align_low(struct mic_device *xdev, dma_addr_t pa)
{
	return rounddown(pa, ALUT_ENTRY_SIZE);
}

static inline u64 mic_alut_align_high(struct mic_device *xdev, dma_addr_t pa)
{
	return roundup(pa, ALUT_ENTRY_SIZE);
}

/* Populate an ALUT entry and update the reference counts. */
static void mic_add_alut_entry(int spt, u64 addr, int entries,
			       struct mic_device *xdev)
{
	struct mic_alut_info *alut = xdev->alut;
	int i;

	for (i = spt; i < spt + entries; i++,
		addr += ALUT_ENTRY_SIZE) {
		if (!alut->entry[i].ref_count &&
		    (alut->entry[i].dma_addr != addr)) {
			mic_alut_set(xdev, addr, i);
			alut->entry[i].dma_addr = addr;
		}
		alut->entry[i].ref_count++;
	}
}

/*
 * Find an available MIC address in MIC ALUT address space
 * for a given DMA address and size.
 */
static dma_addr_t mic_alut_op(struct mic_device *xdev, u64 dma_addr,
			      int entries, size_t size)
{
	int spt;
	int ae = 0;
	int i;
	unsigned long flags;
	dma_addr_t mic_addr = 0;
	dma_addr_t addr = dma_addr;
	struct mic_alut_info *alut = xdev->alut;

	spin_lock_irqsave(&alut->alut_lock, flags);

	/* find existing entries */
	for (i = 0; i < ALUT_ENTRY_COUNT; i++) {
		if (alut->entry[i].dma_addr == addr) {
			ae++;
			addr += ALUT_ENTRY_SIZE;
		} else if (ae) /* cannot find contiguous entries */
			goto not_found;

		if (ae == entries)
			goto found;
	}

	/* find free entry */
	for (ae = 0, i = 0; i < ALUT_ENTRY_COUNT; i++) {
		ae = (alut->entry[i].ref_count == 0) ? ae + 1 : 0;
		if (ae == entries)
			goto found;
	}

not_found:
	spin_unlock_irqrestore(&alut->alut_lock, flags);
	return mic_addr;

found:
	spt = i - entries + 1;
	mic_addr = mic_alut_to_pa(xdev, spt);
	mic_add_alut_entry(spt, dma_addr, entries, xdev);
	alut->map_count++;
	alut->ref_count++;
	spin_unlock_irqrestore(&alut->alut_lock, flags);
	return mic_addr;
}

/*
 * Returns number of alut entries needed for dma_addr to dma_addr + size
 * also returns the reference count array for each of those entries
 * and the starting alut address
 */
static int mic_get_alut_ref_count(struct mic_device *xdev, dma_addr_t dma_addr,
				  size_t size, u64 *alut_start)
{
	u64 start =  dma_addr;
	u64 end = dma_addr + size;
	int i = 0;

	while (start < end) {
		i++;
		start = mic_alut_align_high(xdev, start + 1);
	}

	if (alut_start)
		*alut_start = mic_alut_align_low(xdev, dma_addr);

	return i;
}

/*
 * mic_to_dma_addr - Converts a MIC address to a DMA address.
 *
 * @xdev: pointer to mic_device instance.
 * @mic_addr: MIC address.
 *
 * returns a DMA address.
 */
dma_addr_t mic_to_dma_addr(struct mic_device *xdev, dma_addr_t mic_addr)
{
	struct mic_alut_info *alut = xdev->alut;
	int spt;
	dma_addr_t dma_addr;

	if (!xdev->alut)
		return mic_addr;

	spt = mic_sys_addr_to_alut(xdev, mic_addr);
	dma_addr = alut->entry[spt].dma_addr +
		mic_alut_offset(xdev, mic_addr);
	return dma_addr;
}

/**
 * mic_map - Maps a DMA address to a MIC physical address.
 *
 * @xdev: pointer to mic_device instance.
 * @dma_addr: DMA address.
 * @size: Size of the region to be mapped.
 *
 * This API converts the DMA address provided to a DMA address understood
 * by MIC. Caller should check for errors by calling mic_map_error(..).
 *
 * returns DMA address as required by MIC.
 */
dma_addr_t mic_map(struct mic_device *xdev, dma_addr_t dma_addr, size_t size)
{
	dma_addr_t mic_addr = 0;
	int num_entries;
	u64 alut_start;

	if (!xdev->alut)
		return dma_addr;

	if (!size)
		return 0;

	num_entries = mic_get_alut_ref_count(xdev, dma_addr, size, &alut_start);

	/* Set the alut table appropriately and get an aligned address */
	mic_addr = mic_alut_op(xdev, alut_start, num_entries, size);

        return mic_addr + mic_alut_offset(xdev, dma_addr);
}

/**
 * mic_unmap - Unmaps a MIC physical address.
 *
 * @xdev: pointer to mic_device instance.
 * @mic_addr: MIC physical address.
 * @size: Size of the region to be unmapped.
 *
 * This API unmaps the mappings created by mic_map(..).
 *
 * returns None.
 */
void mic_unmap(struct mic_device *xdev, dma_addr_t mic_addr, size_t size)
{
	struct mic_alut_info *alut = xdev->alut;
	int num_alut;
	int spt;
	int i;
	unsigned long flags;

	if (!size)
		return;

	if (!xdev->alut)
		return;

	spt = mic_sys_addr_to_alut(xdev, mic_addr);

	/* Get number of alut entries to be mapped, ref count array */
	num_alut = mic_get_alut_ref_count(xdev, mic_addr, size, NULL);

	spin_lock_irqsave(&alut->alut_lock, flags);
	alut->unmap_count++;
	alut->ref_count--;

	for (i = spt; i < spt + num_alut; i++) {
		alut->entry[i].ref_count--;
		if (alut->entry[i].ref_count < 0)
			dev_warn(&xdev->pdev->dev,
				 "ref count for entry %d is negative\n", i);
	}
	spin_unlock_irqrestore(&alut->alut_lock, flags);
}

/**
 * mic_map_single - Maps a virtual address to a MIC physical address.
 *
 * @xdev: pointer to mic_device instance.
 * @va: Kernel direct mapped virtual address.
 * @size: Size of the region to be mapped.
 *
 * This API calls pci_map_single(..) for the direct mapped virtual address
 * and then converts the DMA address provided to a DMA address understood
 * by MIC. Caller should check for errors by calling mic_map_error(..).
 *
 * returns DMA address as required by MIC.
 */
dma_addr_t mic_map_single(struct mic_device *xdev, void *va, size_t size)
{
	dma_addr_t mic_addr = 0;
	struct pci_dev *pdev = xdev->pdev;
	dma_addr_t dma_addr =
			pci_map_single(pdev, va, size, PCI_DMA_BIDIRECTIONAL);

	if (!pci_dma_mapping_error(pdev, dma_addr)) {
		mic_addr = mic_map(xdev, dma_addr, size);
		if (!mic_addr) {
			dev_err(&xdev->pdev->dev,
				"mic_map failed dma_addr 0x%llx size 0x%lx\n",
				dma_addr, size);
			pci_unmap_single(pdev, dma_addr,
					 size, PCI_DMA_BIDIRECTIONAL);
		}
	}
	return mic_addr;
}


/**
 * mic_unmap_single - Unmaps a MIC physical address.
 *
 * @xdev: pointer to mic_device instance.
 * @mic_addr: MIC physical address.
 * @size: Size of the region to be unmapped.
 *
 * This API unmaps the mappings created by mic_map_single(..).
 *
 * returns None.
 */
void
mic_unmap_single(struct mic_device *xdev, dma_addr_t mic_addr, size_t size)
{
	struct pci_dev *pdev = xdev->pdev;
	dma_addr_t dma_addr = mic_to_dma_addr(xdev, mic_addr);
	mic_unmap(xdev, mic_addr, size);
	pci_unmap_single(pdev, dma_addr, size, PCI_DMA_BIDIRECTIONAL);
}

/**
 * mic_alut_enable - Enable ALUT
 *
 * @xdev: pointer to mic_device instance.
 *
 * ALUT needs to be specifically enabled before using.
 *
 * returns None.
 */
void mic_alut_enable(struct mic_device *xdev)
{
	if (!xdev->alut)
		return;

	/* Enable bit 28 (Link-to-Virtual A-LUT enable bit) */
	mic_mmio_write(&xdev->mmio, 1 << 28,
			MIC_VIRTUAL_TO_LINK_ALUT_CONTROL);
}

/**
 * mic_alut_restore - Restore MIC System Memory Page Tables.
 *
 * @xdev: pointer to mic_device instance.
 *
 * Restore the ALUT HW registers to values previously stored in the
 * SW data structures.
 *
 * returns -EFAULT when writing to any ALUT entry was unsuccessful
 *
 */
int mic_alut_restore(struct mic_device *xdev)
{
	int i;
	dma_addr_t dma_addr;
	struct mic_alut entry_check;
	unsigned long timeout = jiffies + msecs_to_jiffies(1000);

	if (!xdev->alut)
		return 0;

	for (i = 0; i < ALUT_ENTRY_COUNT; i++) {
		dma_addr = xdev->alut->entry[i].dma_addr;
retry:
		mic_alut_set(xdev, dma_addr, i);
		entry_check = mic_alut_get(xdev, i);

		if (entry_check.dma_addr != dma_addr) {
			/*
			 * A-LUT requires time before it's ready to be written,
			 * that is why we need to try for some time
			 */
			if (!time_is_before_jiffies(timeout))
				goto retry;
			dev_err(&xdev->pdev->dev,
				"ALUT entry %d: read %#llx, should be %#llx\n",
				i, entry_check.dma_addr, dma_addr);
			return -EFAULT;
		}
	}

	return 0;
}

/**
 * mic_alut_init - Initialize MIC X200 address lookup tables.
 *
 * @xdev: pointer to mic_device instance.
 *
 * returns 0 for success and -errno for error.
 */
int mic_alut_init(struct mic_device *xdev)
{
	int i, err = 0;
	dma_addr_t dma_addr;
	struct mic_alut_info *alut;
	/* don't enable alut if IOMMU is enabled */
	if (intel_iommu_enabled)
		return 0;

	xdev->alut = kzalloc(sizeof(*xdev->alut), GFP_KERNEL);
	if (!xdev->alut)
		return -ENOMEM;

	alut = xdev->alut;

	/* init SW data structures */
	alut->identity_map_size = MIC_ALUT_SIZE;
	alut->identity_map_size -= (xdev->peer_count * MIC_APER_SIZE);
	alut->identity_map_size -= (xdev->net_peer_count * ALUT_ENTRY_SIZE);
	alut->identity_map_entries = (alut->identity_map_size / ALUT_ENTRY_SIZE);

	log_mic_dbg(xdev->id, "identity_map_size: %lld",
		    alut->identity_map_size);

	alut->entry = kmalloc_array(ALUT_ENTRY_COUNT,
					 sizeof(*alut->entry), GFP_KERNEL);
	if (!alut->entry) {
		err = -ENOMEM;
		goto free_alut;
	}
	spin_lock_init(&alut->alut_lock);
	alut->ref_count = 0;

	for (i = 0; i < ALUT_ENTRY_COUNT; i++) {
		dma_addr = i * ALUT_ENTRY_SIZE;

		if (i < alut->identity_map_entries) {
			alut->entry[i].dma_addr = dma_addr;
			alut->entry[i].ref_count = 1;
			alut->ref_count++;
		} else {
			alut->entry[i].dma_addr = 0;
			alut->entry[i].ref_count = 0;
		}

	}
	alut->map_count = 0;
	alut->unmap_count = 0;

	/* init HW registers */
	mic_alut_enable(xdev);
	return mic_alut_restore(xdev);

free_alut:
	kfree(alut);
	return err;
}

/**
 * mic_alut_uninit - UnInitialize MIC X200 address lookup tables.
 *
 * @xdev: pointer to mic_device instance.
 *
 * returns None.
 */
void mic_alut_uninit(struct mic_device *xdev)
{
	struct mic_alut_info *alut = xdev->alut;
	int i;

	if (!xdev->alut)
		return;

	dev_dbg(&xdev->pdev->dev,
		"nodeid %d ALUT ref count %lld map %lld unmap %lld\n",
		xdev->id, alut->ref_count,
		alut->map_count, alut->unmap_count);

	for (i = 0; i < ALUT_ENTRY_COUNT; i++) {
		dev_dbg(&xdev->pdev->dev,
			"ALUT entry[%d] dma_addr = 0x%llx ref_count = %lld\n",
			i, alut->entry[i].dma_addr,
			alut->entry[i].ref_count);
		if (i < xdev->alut->identity_map_entries) {
			if (alut->entry[i].ref_count != 1)
				dev_warn(&xdev->pdev->dev,
					 "ref count for entry %d is not one\n", i);
		} else {
			if (alut->entry[i].ref_count)
				dev_warn(&xdev->pdev->dev,
					 "ref count for entry %d is not zero\n", i);
		}
	}
	kfree(alut->entry);
	kfree(alut);
}
