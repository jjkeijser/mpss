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

/* ************************************************************************* *\
generate a virtual address for a given size
\* ************************************************************************* */
#include "mic/micscif.h"

/* ************************************************************************* *\
FUNCTION: VaGenAddress::Initialize

DESCRIPTION: Initialize VaGenAddress to point to one node of size = range
\* ************************************************************************* */
static int
va_gen_init_internal(struct va_gen_addr *addr, uint64_t range)
{
	struct va_node *node;
	int err;

	va_node_init(&addr->allocator);
	if ((err = va_node_alloc(&addr->allocator, &addr->hole_list)) < 0)
		goto init_err;
	if (va_node_is_valid(addr->hole_list)) {
		node = va_node_get(&addr->allocator, addr->hole_list);
		node->next = invalid_va_node_index;
		node->base = 0;
		node->range = range;
	}
	addr->claims_list = invalid_va_node_index;
init_err:
	return err;
}

/* ************************************************************************* *\
FUNCTION:   VaGenAddress::Alloc
Allocate virtual memory by searching through free virtual memory
linked list for first range >= desired range.

Note: Free list is sorted by base, we are searching for range.

Return:     Offset to allocated virtual address if successful (in pages).
INVALID_VA_PAGE_INDEX if failed
\* ************************************************************************* */
static uint64_t
va_gen_alloc_internal(struct va_gen_addr *addr, uint64_t range)
{
	//==========================================================================
	// Search for a sufficiently large memory hole (first-fit).
	//--------------------------------------------------------------------------

	// Search for first available hole of sufficient size.
	uint32_t index = addr->hole_list;
	struct va_node *pFind;
	// Used to handle case of an exact range match.
	struct va_node *pPrev = 0;
	uint64_t base;

	if (0 == range || !va_node_is_valid(addr->hole_list))
		return INVALID_VA_PAGE_INDEX;

	pFind = va_node_get(&addr->allocator, index);

	for ( ; ; ) {
		if (pFind->range >= range)
			break;
		else {
			index = pFind->next;
			// No hole sufficiently large.
			if (!va_node_is_valid(index))
				return INVALID_VA_PAGE_INDEX;
			pPrev = pFind;
			pFind = va_node_get(&addr->allocator, index);
		}
	}

	// Found an adequate hole. Get its base.
	base = pFind->base;

	//============================================================================
	// Uncommon case: pFind->range == in_range
	// Remove node from the hole list when exact fit. Note, could leave the
	// hole list empty.
	//----------------------------------------------------------------------------

	if (pFind->range == range) {
		// first node?
		if (addr->hole_list == index)
			addr->hole_list = pFind->next;
		else {
			BUG_ON(!pPrev);
			pPrev->next = pFind->next;
		}
		va_node_free(&addr->allocator, index);
		return base;
	}

	//================================================================================
	// Shrink an existing node that is too large.
	//--------------------------------------------------------------------------------

	else {
		pFind->base  += range;
		pFind->range -= range;
	}

	return base;
}

/* ************************************************************************* *\
FUNCTION: VaGenAddress::FreeClaim

DESCRIPTION:
Removes claimed range from the claims list.
\* ************************************************************************* */
static void
va_gen_free_claim(struct va_gen_addr *addr, uint64_t base, uint64_t range)
{
	struct va_node *pNode = 0;
	struct va_node *pPrev = 0;
	uint32_t index, new_index;
	struct va_node *pNewNode;
	int err;

	if (0 == range)
		return;

	for (index = addr->claims_list; va_node_is_valid(index); index = pNode->next) {
		pNode = va_node_get(&addr->allocator, index);

		if (pNode->base <= base && pNode->base + pNode->range >= base + range) {
			if (pNode->base == base) {
				pNode->base += range;
				pNode->range -= range;
				if (0 == pNode->range) {
					if (pPrev)
						pPrev->next = pNode->next;
					else
						addr->claims_list = pNode->next;
					va_node_free(&addr->allocator, index);
				}
			} else if (pNode->base + pNode->range == base + range) {
				pNode->range -= range;
			} else {
				err = va_node_alloc(&addr->allocator, &new_index);
				BUG_ON(err < 0);
				pNewNode = va_node_get(&addr->allocator, new_index);
				pNewNode->base = base + range;
				pNewNode->range = pNode->range - pNewNode->base;
				pNewNode->next = pNode->next;
				pNode->range = base - pNode->base;
				pNode->next = new_index;
			}
			return;
		}
		if (pNode->base > base + range) {
			pr_debug("Freed claim not found in the list\n");
			return;
		}

		if ((pNode->base < base) ?
				(pNode->base + pNode->range > base) :
				(base + range > pNode->base)) {
			pr_debug("Freed claim partially overlaps the list\n");
			return;
		}
		pPrev = pNode;
	}
}

/* ************************************************************************* *\
FUNCTION: VaGenAddress::InsertAndCoalesce

DESCRIPTION:
O(n) search through free list sorted by base
should average O(n/2), and free list should be much less than the # allocated
coalesce with node before/after if possible
3 possible outcomes:
1.  freed node is inserted into list (0 deallocated)
2.  freed node range coalesced with existing node,
so freed node can be deallocated (1 deallocated)
3.  freed node + another node are coalesced + deallocated
(2 deallocated)
Fails if there is full or partial overlap between inserted
range and ranges in the list

returns false if insert failed
\* ************************************************************************* */
static int
va_gen_insert_and_coalesce(struct va_node_allocator *allocator, uint32_t *list,
			   uint64_t base, uint64_t range)
{
	// search through free list, insert ordered
	// also check for coalesce
	uint32_t findPtr = *list;
	uint32_t prev = *list;
	uint64_t end_range = base + range;
	uint32_t nextPtr, ptr;
	struct va_node *nextNode, *node;
	int err;

	while (va_node_is_valid(findPtr)) {
		struct va_node *find = va_node_get(allocator, findPtr);
		// overlap?
		//    A.start < B.start && A.end > B.start    A-B==A-B A-B==B-A otherwise A-A B-B
		//    B.start < A.start && B.end > A.start    B-A==B-A B-A==A-B otherwise B-B A-A
		//    =>
		//    A.start < B.start ? A.end > B.start : B.end > A.start

		if ((find->base < base) ?
				(find->base + find->range > base) :
				(end_range > find->base)) {
			return -1;
		}
		//----------------------------------------------------------
		// coalesce?  2 possibilities:
		//   1. (pFind->base + pFind->range) == current.base
		//      coalesce, check next node base = endrange,
		//         coalesce with next if possible, deallocate next, exit
		//   2. end_range == pFind->base
		//      coalesce, exit
		if (end_range == find->base) {
			// pr_debug("Coalesce base %lld before %lld\n", base, find->base);
			find->base = base;
			find->range += range;
			return 0;
		} else if ((find->base + find->range) == base) {
			// pr_debug("Coalesce base %lld after %lld\n", base, find->base);
			// leave the base unchanged
			find->range += range;
			// check the next node to see if it coalesces too
			nextPtr = find->next;
			if (va_node_is_valid(nextPtr)) {
				nextNode = va_node_get(allocator, nextPtr);
				// end_range is the same after prior coalesce
				if (nextNode->base == end_range) {
					// pr_debug("Double Coalesce index %d before %d\n", findPtr, nextPtr);
					find->range += nextNode->range;
					find->next = nextNode->next;
					va_node_free(allocator, nextPtr);
				}
			}
			return 0;
		}
		// end coalesce

		//----------------------------------------------------------
		// insert if found a node at a greater address
		else if (find->base > end_range)
			// exit loop, insert node
			break;
		// nothing found yet, next index
		prev = findPtr;
		findPtr = find->next;
	}

	//----------------------------------------------------------
	// insert or append if node
	//   could be at the end or empty free list (find index = INVALID)
	//   or, next node has larger base
	//----------------------------------------------------------
	err = va_node_alloc(allocator, &ptr);
	BUG_ON(err < 0);
	if (!va_node_is_valid(ptr)) {
		printk(KERN_ERR "FAILED to add hole!  base = %lld, range = %lld\n", base, range);
		return 0;
	}
	node = va_node_get(allocator, ptr);
	node->base = base;
	node->range = range;
	node->next = findPtr;
	// First node or empty list (Alloc() can empty the list)
	if (findPtr == *list)
		// pr_debug("List now starts with %d\n", ptr);
		*list = ptr;
	else { // reached the end of the list or insertion
		BUG_ON(!va_node_is_valid(prev));
		// pr_debug("Append index %d after %d\n", ptr, prev);
		(va_node_get(allocator, prev))->next = ptr;
	}
	return 0;
}

/* ************************************************************************* *\
FUNCTION: VaGenAddress::Free

DESCRIPTION:
Frees allocated Virtual Address. Inserts freed range in the list of holes
(available virtual addresses)
\* ************************************************************************* */
static void
va_gen_free_internal(struct va_gen_addr *addr, uint64_t base, uint64_t range)
{
	int result = va_gen_insert_and_coalesce(&addr->allocator, &addr->hole_list, base, range);
	BUG_ON(result < 0);
}

/* ************************************************************************* *\
FUNCTION:   VaGenAddress::Alloc
Allocate virtual memory space.

Note: "Quick and dirty" implementation of aligned Alloc on top of
non-aligned Alloc.

Return:     Offset to allocated virtual address if successful (in pages).
INVALID_VA_PAGE_INDEX if failed
\* ************************************************************************* */
static uint64_t
va_gen_alloc_aligned(struct va_gen_addr *addr, uint64_t range, uint32_t unit_align)
{
	uint64_t base_address = va_gen_alloc_internal(addr, range + unit_align - 1);
	uint64_t aligned_base = base_address;
	if (0 == range || 0 == unit_align)
		return INVALID_VA_PAGE_INDEX;
	//BUG_ON(IsPowerOfTwo(in_unitAlign));

	if (unit_align == 1 || base_address == INVALID_VA_PAGE_INDEX)
		return base_address;

	if (aligned_base > base_address)
		va_gen_free_internal(addr, base_address, aligned_base - base_address);

	if (aligned_base + range < base_address + unit_align - 1)
		va_gen_free_internal(addr, aligned_base + range,
				base_address + unit_align - 1 - aligned_base - range);
	return aligned_base;
}

/* ************************************************************************* *\
FUNCTION: VaGenAddress::Claim

DESCRIPTION:
Claims a SVAS range. Checks if range was claimed before; if not, records
the claim in the claims list

returns false if claim failed
\* ************************************************************************* */
static int
va_gen_claim_internal(struct va_gen_addr *addr, uint64_t base, uint64_t range)
{
	return va_gen_insert_and_coalesce(&addr->allocator, &addr->claims_list, base, range);
}

/* ************************************************************************* *\
FUNCTION:   VaGenAddressMutex::Alloc
Allocate virtual memory space.

Note: Wrapper for unit-testable address generator to add critical
section and convert bytes to pages.
Note: Free() selects between Free[Alloc] and FreeClaim based on
the address range of the freed address.

Return:     Allocated virtual address if successful (in bytes)
INVALID_VA_GEN_ADDRESS if failed
\* ************************************************************************* */
uint64_t
va_gen_alloc(struct va_gen_addr *addr, uint64_t num_bytes, uint32_t align_bytes)
{
	// Convert input bytes to pages which is our unit for the address generator.
	uint64_t num_pages = (uint64_t)(((PAGE_SIZE - 1) + num_bytes) / PAGE_SIZE);
	uint64_t align_pages = align_bytes / PAGE_SIZE;
	uint64_t va_page_index, ret;

	if (align_bytes < PAGE_SIZE) {
		ret = INVALID_VA_GEN_ADDRESS;
		WARN_ON(1);
		goto done;
	}

	if (num_bytes > (0xffffffffULL * PAGE_SIZE)) {
		ret = INVALID_VA_GEN_ADDRESS;
		WARN_ON(1);
		goto done;
	}
	va_page_index = va_gen_alloc_aligned(addr, num_pages, (uint32_t)(align_pages % 0xffffffff) );

	if (va_page_index == INVALID_VA_PAGE_INDEX)
		return INVALID_VA_GEN_ADDRESS;

	// Convert page number to virtual address, adding base.
	ret = va_page_index << PAGE_SHIFT;
	ret += addr->base;
done:
	return ret;
}

// Claims ownership of a memory region
uint64_t
va_gen_claim(struct va_gen_addr *addr, uint64_t address, uint64_t num_bytes)
{
	uint64_t va, num_pages;
	int result;

	if (address + num_bytes > addr->base)
		address = INVALID_VA_GEN_ADDRESS;
	else if (address & (PAGE_SIZE - 1))
		// address not aligned
		address = INVALID_VA_GEN_ADDRESS;
	else {
		va = (uint64_t)(address >> PAGE_SHIFT);
		// pr_debug("%s %d (%#llx,%llx)\n", __func__, __LINE__, va, num_bytes);
		// convert input bytes to pages, our unit for the address generator
		num_pages = (uint64_t)(((PAGE_SIZE-1) + num_bytes) / PAGE_SIZE);
		if ((result = va_gen_claim_internal(addr, va, num_pages)) < 0)
			address = INVALID_VA_GEN_ADDRESS;
	}
	return address;
}

// frees the address range so the pages may be re-assigned
void
va_gen_free(struct va_gen_addr *addr, uint64_t address, uint64_t num_bytes)
{
	uint64_t va, num_pages;

	if (address >= addr->base) {
		// convert virtual address to page number, subtracting base
		address -= addr->base;
		va = (uint64_t)(address >> PAGE_SHIFT);
		// pr_debug("%s %d (%#llx,%llx)\n", __func__, __LINE__, va, num_bytes);
		// convert input bytes to pages, our unit for the address generator
		num_pages = (uint64_t)(((PAGE_SIZE-1) + num_bytes) / PAGE_SIZE);
		va_gen_free_internal(addr, va, num_pages);
	} else {
		va = (uint64_t)(address >> PAGE_SHIFT);
		// pr_debug("%s %d (%#llx,%llx)\n", __func__, __LINE__, va, num_bytes);
		// convert input bytes to pages, our unit for the address generator
		num_pages = (uint64_t)(((PAGE_SIZE-1) + num_bytes) / PAGE_SIZE);
		va_gen_free_claim(addr, va, num_pages);
	}
}

// base and range in bytes, though internal va generator works in pages
int
va_gen_init(struct va_gen_addr *addr, uint64_t base, uint64_t range)
{
	uint64_t rangeInPages = (uint64_t)(range >> PAGE_SHIFT);
	int ret;

	if (!(ret = va_gen_init_internal(addr, rangeInPages)))
		addr->base = base;
	return ret;
}

void
va_gen_destroy(struct va_gen_addr *addr)
{
	va_node_destroy(&addr->allocator);
}
