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

#ifndef MICVETHCOMMON_H
#define MICVETHCOMMON_H

#ifndef ETH_HLEN
#define ETH_HLEN	14
#endif

typedef enum micvnet_state {
	MICVNET_STATE_UNDEFINED,
	MICVNET_STATE_UNINITIALIZED,
	MICVNET_STATE_LINKUP,
	MICVNET_STATE_LINK_DOWN,
	MICVNET_STATE_BEGIN_UNINIT,
	MICVNET_STATE_TRANSITIONING,
}micvnet_state;


/*
 * Fancy way of defining an enumeration and the mapping between them and
 * the module parameter--they're guaranteed to be in sync this way.
 */
#define VNET_MODES				\
	__VNET_MODE(POLL, poll)			\
	__VNET_MODE(INTR, intr)			\
	__VNET_MODE(DMA,  dma)			\
	/* end */
#define __VNET_MODE(u, l) VNET_MODE_##u ,
enum { VNET_MODES };
#undef __VNET_MODE

extern char *mic_vnet_modes[];
extern int mic_vnet_mode;

#endif /* MICVETHCOMMON_H */
