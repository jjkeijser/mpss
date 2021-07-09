/*
 * Intel MIC Platform Software Stack (MPSS)
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2016 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * BSD LICENSE
 *
 * Copyright(c) 2016 Intel Corporation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Intel SCIF driver.
 */
#ifndef SCIF_MAP_H
#define SCIF_MAP_H

#include "../bus/scif_bus.h"

static __always_inline void *
scif_alloc_coherent(dma_addr_t *dma_handle,
		    struct scif_dev *scifdev, size_t size,
		    gfp_t gfp)
{
	void *va;

	if (scifdev_self(scifdev)) {
		va = kmalloc(size, gfp);
		if (va)
			*dma_handle = virt_to_phys(va);
	} else {
		va = dma_alloc_coherent(&scifdev->sdev->dev,
					size, dma_handle, gfp);
		if (va && scifdev_is_p2p(scifdev))
			*dma_handle = *dma_handle + scifdev->base_addr;
	}
	return va;
}

static __always_inline void
scif_free_coherent(void *va, dma_addr_t local,
		   struct scif_dev *scifdev, size_t size)
{
	if (scifdev_self(scifdev)) {
		kfree(va);
	} else {
		if (scifdev_is_p2p(scifdev) && local > scifdev->base_addr)
			local = local - scifdev->base_addr;
		dma_free_coherent(&scifdev->sdev->dev,
				  size, va, local);
	}
}

static __always_inline int
scif_map_single(dma_addr_t *dma_handle,
		void *local, struct scif_dev *scifdev, size_t size)
{
	int err = 0;

	if (scifdev_self(scifdev)) {
		*dma_handle = virt_to_phys((local));
	} else {
		*dma_handle = dma_map_single(&scifdev->sdev->dev,
					     local, size, DMA_BIDIRECTIONAL);
		if (dma_mapping_error(&scifdev->sdev->dev, *dma_handle))
			err = -ENOMEM;
		else if (scifdev_is_p2p(scifdev))
			*dma_handle = *dma_handle + scifdev->base_addr;
	}
	if (err)
		*dma_handle = 0;
	return err;
}

static __always_inline void
scif_unmap_single(dma_addr_t local, struct scif_dev *scifdev,
		  size_t size)
{
	if (!scifdev_self(scifdev)) {
		if (scifdev_is_p2p(scifdev))
			local = local - scifdev->base_addr;
		dma_unmap_single(&scifdev->sdev->dev, local,
				 size, DMA_BIDIRECTIONAL);
	}
}

static __always_inline void *
scif_ioremap(dma_addr_t phys, size_t size, struct scif_dev *scifdev)
{
	void *out_virt;
	struct scif_hw_dev *sdev = scifdev->sdev;

	if (scifdev_self(scifdev))
		out_virt = phys_to_virt(phys);
	else
		out_virt = (void __force *)
			   sdev->hw_ops->ioremap(sdev, phys, size);
	return out_virt;
}

static __always_inline void
scif_iounmap(void *virt, size_t len, struct scif_dev *scifdev)
{
	if (!scifdev_self(scifdev)) {
		struct scif_hw_dev *sdev = scifdev->sdev;

		sdev->hw_ops->iounmap(sdev, (void __force __iomem *)virt);
	}
}

static __always_inline int
scif_map_page(dma_addr_t *dma_handle, struct page *page,
	      struct scif_dev *scifdev)
{
	int err = 0;

	if (scifdev_self(scifdev)) {
		*dma_handle = page_to_phys(page);
	} else {
		struct scif_hw_dev *sdev = scifdev->sdev;
		*dma_handle = dma_map_page(&sdev->dev,
					   page, 0x0, PAGE_SIZE,
					   DMA_BIDIRECTIONAL);
		if (dma_mapping_error(&sdev->dev, *dma_handle))
			err = -ENOMEM;
		else if (scifdev_is_p2p(scifdev))
			*dma_handle = *dma_handle + scifdev->base_addr;
	}
	if (err)
		*dma_handle = 0;
	return err;
}

static __always_inline void
scif_unmap_page(dma_addr_t local, struct scif_dev *scifdev,
		  size_t size)
{
	if (!scifdev_self(scifdev)) {
		if (scifdev_is_p2p(scifdev))
			local = local - scifdev->base_addr;
		dma_unmap_page(&scifdev->sdev->dev, local,
				 size, DMA_BIDIRECTIONAL);
	}
}

#endif  /* SCIF_MAP_H */
