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

#include <linux/module.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/version.h>

#include "mic/micveth.h"

#define __VNET_MODE(u, l) #l ,
char *mic_vnet_modes[] = { VNET_MODES };
#undef __VNET_MODE

/*
 *KAA: not sure when this API changed, could have been in 35.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36)
#define	GRRR	const
#else
#define GRRR	/* As nothing */
#endif

static int param_set_vnetmode(const char *val, GRRR struct kernel_param *kp)
{
	int i;
	for (i = 0; i < sizeof(mic_vnet_modes) / sizeof(char *); i++)
		if (!strcmp(val, mic_vnet_modes[i])) {
			mic_vnet_mode = i;
			return 0;
		}
	return -EINVAL;
}

static int param_get_vnetmode(char *buffer, GRRR struct kernel_param *kp)
{
	return sprintf(buffer, "%s", mic_vnet_modes[mic_vnet_mode]);
}

#define param_check_vnetmode(name, p) __param_check(name, p, int)

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36)
struct kernel_param_ops param_ops_vnetmode = {
  .set = param_set_vnetmode,
  .get = param_get_vnetmode,
};
#endif /* Kernel > 2.6.36 */

int mic_vnet_mode = VNET_MODE_DMA;
module_param_named(vnet, mic_vnet_mode, vnetmode, 0400);
#define __VNET_MODE(u, l) " " #l
MODULE_PARM_DESC(vnet, "Vnet operating mode, one of:" VNET_MODES);
#undef __VNET_MODE

int vnet_num_buffers = VNET_MAX_SKBS;
module_param(vnet_num_buffers, int, 0400);
MODULE_PARM_DESC(vnet_num_buffers, "Number of buffers used by the VNET driver");

ulong vnet_addr = 0;
module_param(vnet_addr, ulong, 0400);
MODULE_PARM_DESC(vnet_addr, "Vnet driver host ring address");


