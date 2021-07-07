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

/* generate a virtual address for a given size */

#ifndef MICSCIF_VA_GEN_H
#define MICSCIF_VA_GEN_H

#include "micscif_va_node.h"

/*
 * To avoid collisions with user applications trying to use
 * MAP_FIXED with scif_register(), the following window address space
 * allocation scheme is used.
 *
 * 1) (0) - (2 ^ 62 - 1))
 *	Window Address Space that can be claimed using MAP_FIXED.
 * 2) (2 ^ 62) - (2 ^ 63) - 1)
 *	Window address space used for allocations by the SCIF driver
 *	when MAP_FIXED is not passed.
 */
#define VA_GEN_MIN      0x4000000000000000
#define VA_GEN_RANGE    0x3f00000000000000

#define INVALID_VA_GEN_ADDRESS 0xff00000000000000
#define INVALID_VA_PAGE_INDEX  0xff00000000000

struct va_gen_addr {
    struct va_node_allocator allocator;
    uint32_t hole_list;
    uint32_t claims_list;
    uint64_t base;
};

/*
 * return a base for the range
 * caller trusted to keep track of both base and range
 */
uint64_t va_gen_alloc(struct va_gen_addr *addr,
		      uint64_t num_bytes, uint32_t align_bytes);

/* Claim ownership of memory region. Fails if already occupied */
uint64_t va_gen_claim(struct va_gen_addr *addr,
		      uint64_t address, uint64_t num_bytes);

/* release ownership of a base/range */
void va_gen_free(struct va_gen_addr *addr,
		 uint64_t address, uint64_t num_bytes);

int va_gen_init(struct va_gen_addr *addr, uint64_t base, uint64_t range);

void va_gen_destroy(struct va_gen_addr *addr);

#endif
