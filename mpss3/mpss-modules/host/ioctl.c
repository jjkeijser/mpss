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

/* contains code to handle MIC IO control codes */

#include "mic_common.h"

static int do_send_flash_cmd(mic_ctx_t *mic_ctx, struct ctrlioctl_flashcmd *args);
static int get_card_mem(mic_ctx_t *mic_ctx, struct ctrlioctl_cardmemcpy *args);

/*
 DESCRIPTION:: Gets the opcode from the input buffer and call appropriate method
 PARAMETERS::
   [in]mic_ctx_t *mic_ctx - pointer to the mic private context
   [in]void *in_buffer - input buffer containing opcode + ioctl arguments,
 RETURN_VALUE:: 0 if successful, non-zero if failure
*/
int
adapter_do_ioctl(uint32_t cmd, uint64_t arg)
{
	int status = 0;
	mic_ctx_t *mic_ctx = NULL;

	void __user *argp = (void __user *)arg;
	switch (cmd) {

	case IOCTL_FLASHCMD:
	{
		struct ctrlioctl_flashcmd args = {0};

		if (copy_from_user(&args, argp, sizeof(struct ctrlioctl_flashcmd))) {
			return -EFAULT;
		}

		if (args.brdnum >= (uint32_t)mic_data.dd_numdevs) {
			printk(KERN_ERR "IOCTL error: given board num is invalid\n");
			return -EINVAL;
		}

		mic_ctx = get_per_dev_ctx(args.brdnum);
		if (!mic_ctx) {
			printk(KERN_ERR "IOCTL error: null mic context\n");
			return -ENODEV;
		}

		/* Make sure we are running in flash mode */
		if (mic_ctx->mode != MODE_FLASH || mic_ctx->state != MIC_ONLINE) {
			printk(KERN_ERR "%s Card is not online in flash mode or online state\n", __func__);
			return -EPERM;
		}

		if (mic_ctx->bi_family != FAMILY_KNC) {
			printk(KERN_ERR "%s IOCTL_FLASHCMD not supported for non KNC family cards\n", __func__);
			return -EPERM;
		}

		status = do_send_flash_cmd(mic_ctx, &args);
		if (status) {
			printk(KERN_ERR "IOCTL error: failed to complete IOCTL for bdnum %d\n", args.brdnum);
			return status;
		}

		if (copy_to_user(argp, &args, sizeof(struct ctrlioctl_flashcmd))) {
			return -EFAULT;
		}

		break;
	}

	case IOCTL_CARDMEMCPY:
	{
		struct ctrlioctl_cardmemcpy args = {0};

		if (copy_from_user(&args, argp, sizeof(struct ctrlioctl_cardmemcpy))) {
			return -EFAULT;
		}

		if (args.brdnum >= (uint32_t)mic_data.dd_numdevs) {
			printk(KERN_ERR "IOCTL error: given board num is invalid\n");
			return -EINVAL;
		}
		mic_ctx = get_per_dev_ctx(args.brdnum);
		if (!mic_ctx) {
			printk(KERN_ERR "IOCTL error: null mic context\n");
			return -ENODEV;
		}

		if(mic_ctx->state != MIC_ONLINE || mic_ctx->mode != MODE_LINUX) {
			status = -EPERM;
			printk("Error ! Card not in linux mode or online state!\n");
			return status;
		}

		status = get_card_mem(mic_ctx, &args);
		if (status) {
			printk(KERN_ERR "IOCTL error: failed to complete IOCTL for bdnum %d\n", args.brdnum);
			return status;
		}

		;
		break;
	}

	default:
		printk("Invalid IOCTL\n");
		status = -EINVAL;
		break;
	}

	return status;
}

int
do_send_flash_cmd(mic_ctx_t *mic_ctx, struct ctrlioctl_flashcmd *args)
{
	int status = 0;

	if(!capable(CAP_SYS_ADMIN)) {
		printk(KERN_ERR "Cannot execute unless sysadmin\n");
		return -EACCES;
	}

	pr_debug("%s\n IN:: brdnum = %d, type = %x, data = %p, len = %x\n", 
			__func__, args->brdnum, args->type, args->data, args->len);

	status = send_flash_cmd(mic_ctx, args->type, args->data, args->len);

	return status;
}


int
get_card_mem(mic_ctx_t *mic_ctx, struct ctrlioctl_cardmemcpy *args)
{
	int32_t status = 0;

	if(!capable(CAP_SYS_ADMIN)) {
		printk(KERN_ERR "Cannot execute unless sysadmin\n");
		return -EACCES;
	}

	if (args->dest == NULL) {
		status = EINVAL;
		goto exit;
	}
	pr_debug("%s\n IN:: brdnum = %d, start = %qx, size = %qx, dest = %p\n", 
			__func__, args->brdnum, args->start, args->size, args->dest);

	status = get_cardside_mem(mic_ctx, args->start, args->size, args->dest);

exit:
	return status;

}
