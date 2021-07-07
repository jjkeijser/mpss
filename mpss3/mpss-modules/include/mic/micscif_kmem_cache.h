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

#ifndef MIC_KMEM_CACHE_H
#define MIC_KMEM_CACHE_H
#define MAX_UNALIGNED_BUF_SIZE (1024 * 1024ULL)
#define KMEM_UNALIGNED_BUF_SIZE (MAX_UNALIGNED_BUF_SIZE + (L1_CACHE_BYTES << 1))
#include<linux/slab.h>
extern struct kmem_cache *unaligned_cache;

static inline void micscif_kmem_cache_free(void *buffer)
{
	kmem_cache_free(unaligned_cache, buffer);
}

static inline void *micscif_kmem_cache_alloc(void)
{
	return kmem_cache_alloc(unaligned_cache, GFP_KERNEL|GFP_ATOMIC);
}

static inline struct kmem_cache *micscif_kmem_cache_create(void)
{
	return kmem_cache_create("Unaligned_DMA", KMEM_UNALIGNED_BUF_SIZE, 0, SLAB_HWCACHE_ALIGN, NULL);
}

static inline void micscif_kmem_cache_destroy(void)
{
	kmem_cache_destroy(unaligned_cache);
}
#endif
