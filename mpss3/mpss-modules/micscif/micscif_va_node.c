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

/***************************************************************************\
manage available nodes for VaGenAddress
\***************************************************************************/
#include "mic/micscif.h"

/***************************************************************************\
FUNCTION: va_node_init

DESCRIPTION: constructor for allocator for GfxGenAddress
\***************************************************************************/
void va_node_init(struct va_node_allocator *node)
{
	node->pp_slab_directory = 0;
	node->slab_shift = 7; /* 2^7 -> 128 nodes in the slab */
	node->nodes_in_slab = 1<<node->slab_shift;
	node->slab_mask = (node->nodes_in_slab-1);
	node->num_slabs = 0;
	node->num_free_slabs = 0;
	node->free_list = invalid_va_node_index;
}

int va_node_is_valid(uint32_t index)
{
	return invalid_va_node_index != index;
}

/************************************************************************** *\
FUNCTION: va_node_destroy

DESCRIPTION: destructor for allocator for GfxGenAddress
\************************************************************************** */
void va_node_destroy(struct va_node_allocator *node)
{
	uint32_t i;
	if (node->pp_slab_directory) {
		for (i = 0; i < node->num_slabs; i++) {
			kfree(node->pp_slab_directory[i]);
			node->pp_slab_directory[i] = NULL;
		}
		kfree(node->pp_slab_directory);
		node->pp_slab_directory = NULL;
	}
}

/* ************************************************************************* *\
FUNCTION: va_node_realloc

DESCRIPTION: va_node_realloc to add more node arrays
\* ************************************************************************* */
static int va_node_realloc(struct va_node_allocator *node)
{
	uint32_t growSlabs = 2 * (node->num_slabs) + 1;
	struct va_node **ppGrowDirectory =
		kzalloc(sizeof(struct va_node *) * growSlabs, GFP_KERNEL);
	uint32_t i;

	if (!ppGrowDirectory)
		return -ENOMEM;

	if (node->num_slabs) {
		for (i = 0; i < node->num_slabs; i++)
			ppGrowDirectory[i] = node->pp_slab_directory[i];
		kfree(node->pp_slab_directory);
		node->pp_slab_directory = NULL;
	}
	node->pp_slab_directory = ppGrowDirectory;
	node->num_free_slabs = growSlabs - node->num_slabs;
	return 0;
}

/* ************************************************************************* *\
FUNCTION: va_node_grow

DESCRIPTION: add a node array
\* ************************************************************************* */
static int va_node_grow(struct va_node_allocator *node)
{
	struct va_node *pNewSlab;
	uint32_t i, start;
	int ret;

	if (!node->num_free_slabs)
		if ((ret = va_node_realloc(node)) < 0)
			return ret;

	pNewSlab = kzalloc(sizeof(struct va_node) *
			node->nodes_in_slab, GFP_KERNEL);
	if (pNewSlab)
		node->pp_slab_directory[node->num_slabs] = pNewSlab;
	else
		return -ENOMEM;

	/*--------------------------------------------------------
	* add new nodes to free list
	* slightly better than just calling free() for each index
	*/
	start = node->num_slabs * node->nodes_in_slab;
	for (i = 0; i < (node->nodes_in_slab-1); i++)
		/* we could optimize this, but why bother? */
		pNewSlab[i].next = start + i + 1;
	/* add new allocations to start of list */
	pNewSlab[node->nodes_in_slab-1].next = node->free_list;
	node->free_list = start;
	/*-------------------------------------------------------*/

	/* update bookkeeping for array of arrays */
	node->num_slabs++;
	node->num_free_slabs--;
	return 0;
}

/* ************************************************************************* *\
FUNCTION: va_node_get

DESCRIPTION: return a node reference from index
\* ************************************************************************* */
struct va_node *va_node_get(struct va_node_allocator *node, uint32_t index)
{
	uint32_t slabIndex = index >> node->slab_shift;
	uint32_t nodeIndex = index & node->slab_mask;

	return &node->pp_slab_directory[slabIndex][nodeIndex];
}

/* ************************************************************************* *\
FUNCTION: va_node_alloc

DESCRIPTION: return 0 on success with valid index in out_alloc or errno on failure.
\* ************************************************************************* */
int va_node_alloc(struct va_node_allocator *node, uint32_t *out_alloc)
{
	int ret;

	if (!va_node_is_valid(node->free_list))
		if ((ret = va_node_grow(node)) < 0)
			return ret;
	*out_alloc = node->free_list;
	node->free_list = (va_node_get(node, *out_alloc))->next;
	return 0;
}

/* ************************************************************************* *\
FUNCTION: va_node_free

DESCRIPTION: make a node available
\* ************************************************************************* */
void va_node_free(struct va_node_allocator *node, uint32_t index)
{
	struct va_node *tmp = va_node_get(node, index);
	tmp->next = node->free_list;
	node->free_list = index;
}
