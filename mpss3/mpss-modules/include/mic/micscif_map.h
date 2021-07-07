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

#ifndef MICSCIF_MAP_H
#define MICSCIF_MAP_H

static __always_inline
void *get_local_va(off_t off, struct reg_range_t *window, size_t len)
{
	struct page **pages = window->pinned_pages->pages;
	
	uint64_t page_nr = ((off - window->offset) >> PAGE_SHIFT);

	off_t page_off = off & ~PAGE_MASK;

	return (void *)((uint64_t)
		(page_address(pages[page_nr])) | page_off);
}

static __always_inline void
scif_iounmap(void *virt, size_t len, struct micscif_dev *dev)
{
#ifdef _MIC_SCIF_
	if (!is_self_scifdev(dev))
		iounmap(virt);
#endif
}

#ifdef _MIC_SCIF_
/* FIXME: fix the documentation and functions names since these are also 
 * used in p2p
 */
/*
 * Maps the VA passed in local to the aperture and returns the
 * corresponding GTT index in offset by reference.
 * In the loopback case simply return the physical address.
 */
static __always_inline int
map_virt_into_aperture(phys_addr_t *out_offset,
		void *local,
		struct micscif_dev *dev,
		size_t size)
{
	if (is_self_scifdev(dev))
		*out_offset = virt_to_phys(local);
	else {
		/* Error unwinding code relies on return value being zero */
		*out_offset = virt_to_phys(local);
		if (dev != &scif_dev[0])
			*out_offset = *out_offset + dev->sd_base_addr;
	}

	return 0;
}

/*
 * Maps the struct page passed in page to the aperture and returns the
 * corresponding GTT index in offset by reference.
 * In the loopback case simply return the physical address.
 */
static __always_inline int
map_page_into_aperture(phys_addr_t *out_offset,
			struct page *page,
			struct micscif_dev *dev)
{
	if (is_self_scifdev(dev))
		*out_offset = page_to_phys(page);
	else {
		/* Error unwinding code relies on return value being zero */
		*out_offset = page_to_phys(page);
		if (dev != &scif_dev[0])
			*out_offset = *out_offset + dev->sd_base_addr;
	}
	return 0;
}

/*
 * Nothing to do on card side
 */
static __always_inline void
unmap_from_aperture(phys_addr_t local,
		struct micscif_dev *dev,
		size_t size)
{
}

/*
 * Maps Host physical address passed in phys to MIC.
 * In the loopback case simply return the VA from the PA.
 */
static __always_inline void *
scif_ioremap(phys_addr_t phys, size_t size, struct micscif_dev *dev)
{
	void *out_virt;

	if (is_self_scifdev(dev))
		out_virt = phys_to_virt(phys);
	else
		out_virt = ioremap_nocache(phys, size);

	return out_virt;
}

/*
 * Get the system physical address from the physical address passed
 * by the host. In the case of loopback simply return the physical
 * address.
 */
static __always_inline phys_addr_t
get_phys_addr(phys_addr_t phys, struct micscif_dev *dev)
{
	return phys;
}

#else /* !_MIC_SCIF_ */
/*
 * Maps the VA passed in local to the aperture and returns the
 * corresponding physical address in offset.
 * In the loopback case simply return the physical address.
 */
static __always_inline int
map_virt_into_aperture(phys_addr_t *out_offset,
		void *local,
		struct micscif_dev *dev,
		size_t size)
{
	int err = 0;
        int bid;
	struct pci_dev *hwdev;

	if (is_self_scifdev(dev))
		*(out_offset) = virt_to_phys((local));
	else {

		bid = dev->sd_node - 1;
		hwdev = get_per_dev_ctx(bid)->bi_pdev;
                *out_offset = mic_map_single(bid, hwdev, local, size);
                if (mic_map_error(*out_offset))
                        err = -ENOMEM;
	}

	if (err)
		*out_offset = 0;

	return err;
}
/*
 * Maps the struct page passed in page to the aperture and returns the
 * corresponding physical address in offset.
 * In the loopback case simply return the physical address.
 */
static __always_inline int
map_page_into_aperture(phys_addr_t *out_offset,
			struct page *page,
			struct micscif_dev *dev)
{
	int err = 0;
	int bid;
	dma_addr_t mic_addr;
	struct pci_dev *hwdev;

	if (is_self_scifdev(dev))
		*out_offset = page_to_phys(page);
	else {

		bid = dev->sd_node - 1;
		hwdev = get_per_dev_ctx(bid)->bi_pdev;

		*out_offset = pci_map_page(hwdev, page, 0x0, PAGE_SIZE, 
				PCI_DMA_BIDIRECTIONAL);
		if (pci_dma_mapping_error(hwdev, *out_offset)) {
			err = -EINVAL;
		} else {
			if (!(mic_addr = mic_map(bid, *out_offset, PAGE_SIZE))) {
				printk(KERN_ERR "mic_map failed board id %d\
					     addr %#016llx size %#016zx\n", 
					     bid, *out_offset, PAGE_SIZE);
				pci_unmap_single(hwdev, *out_offset, 
						       PAGE_SIZE, PCI_DMA_BIDIRECTIONAL);
				err = -EINVAL;
			} else
				*out_offset = mic_addr;
		}
	}

	if (err)
		*out_offset = 0;

	return err;
}

/*
 * Unmaps the physical address passed in lo/al from the PCIe aperture.
 * Nothing to do in the loopback case.
 */
static __always_inline void
unmap_from_aperture(phys_addr_t local,
	struct micscif_dev *dev,
	size_t size)
{

	if (!is_self_scifdev(dev))
		mic_ctx_unmap_single(get_per_dev_ctx(dev->sd_node - 1),
			local, size);
}

/*
 * TODO: I'm thinking maybe we should take the apt_phys offset off of this macro
 * and have it be outside ...
 * Maps the page corresponding to the GTT offset passed in phys.
 * In the loopback case simply return the VA from the PA.
 */
static __always_inline void *
scif_ioremap(phys_addr_t phys, size_t size, struct micscif_dev *dev)
{
	void *out_virt;

	if (is_self_scifdev(dev))
		out_virt = phys_to_virt(phys);
	else {
		out_virt = get_per_dev_ctx(dev->sd_node - 1)->aper.va + phys;
	}
	return out_virt;
}

static __always_inline phys_addr_t
get_phys_addr(phys_addr_t phys, struct micscif_dev *dev)
{
	phys_addr_t out_phys;

	if (is_self_scifdev(dev))
		out_phys = phys;
	else {
		phys_addr_t __apt_base =
		(phys_addr_t)get_per_dev_ctx(dev->sd_node - 1)->aper.pa;
		out_phys = phys + __apt_base;
	}

	return out_phys;
}

#endif /* !_MIC_SCIF_ */

#endif  /* MICSCIF_MAP_H */
