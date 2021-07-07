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

#include "micint.h"

int mic_psmi_open(struct file *filp)
{
	bd_info_t *bd_info = mic_data.dd_bi[0];
	if (!bd_info->bi_ctx.bi_psmi.enabled)
		return -EINVAL;
	((filp)->private_data) = &bd_info->bi_ctx;
	return 0;
}

extern int usagemode_param;

ssize_t mic_psmi_read(struct file * filp, char __user *buf,
			size_t count, loff_t *pos)
{
	ssize_t total_bytes = 0;
	unsigned int pg_no, pg_off, bytes;
	mic_ctx_t *mic_ctx = ((filp)->private_data);
	struct mic_psmi_ctx *psmi_ctx = &mic_ctx->bi_psmi;
	loff_t mem_size;

	if (!psmi_ctx->enabled)
		return -EINVAL;
	if (FAMILY_ABR == mic_ctx->bi_family &&
			USAGE_MODE_NORMAL != usagemode_param)
		mem_size = MIC_APERTURE_SIZE;
	else
		mem_size = psmi_ctx->dma_mem_size;
	if (*pos >= mem_size || count <= 0)
		return 0;
	if (*pos + count > mem_size)
		count = mem_size - *pos;
	/* read aperture memory */
	if (USAGE_MODE_NORMAL != usagemode_param) {
		if (copy_to_user(buf,
			mic_ctx->aper.va + *pos, count))
			return -EFAULT;
		goto read_exit;
	}
	/* read host memory allocated for psmi handler */
	pg_no = *pos / MIC_PSMI_PAGE_SIZE;
	pg_off = *pos % MIC_PSMI_PAGE_SIZE;
	while (total_bytes < count) {
		pci_dma_sync_single_for_cpu(mic_ctx->bi_pdev,
			psmi_ctx->dma_tbl[pg_no + 1].pa,
				MIC_PSMI_PAGE_SIZE, PCI_DMA_BIDIRECTIONAL);
		bytes = MIC_PSMI_PAGE_SIZE - pg_off;
		if (total_bytes + bytes > count)
			bytes = count - total_bytes;
		if (copy_to_user(buf,
			(void *)psmi_ctx->va_tbl[pg_no].pa + pg_off, bytes))
			return -EFAULT;
		total_bytes += bytes;
		buf += bytes;
		pg_no++;
		/* Only the first page needs an offset */
		pg_off = 0;
	}
read_exit:
	*pos += count;
	return count;
}

static ssize_t show_mem_size(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	mic_ctx_t *mic_ctx = dev_get_drvdata(dev);
	struct mic_psmi_ctx *psmi_ctx = &mic_ctx->bi_psmi;

	return snprintf(buf, PAGE_SIZE, "%ld\n",
			(unsigned long)psmi_ctx->dma_mem_size);
}
static DEVICE_ATTR(mem_size, S_IRUGO, show_mem_size, NULL);

static struct attribute *psmi_attributes[] = {
	&dev_attr_mem_size.attr,
	NULL
};

struct attribute_group psmi_attr_group = {
	.attrs = psmi_attributes
};

#if (defined(RHEL_RELEASE_CODE) && \
	(LINUX_VERSION_CODE == KERNEL_VERSION(2,6,32))) || \
		LINUX_VERSION_CODE > KERNEL_VERSION(2,6,34)
static ssize_t mic_psmi_read_ptes(struct file *filp, struct kobject *kobj,
	struct bin_attribute *attr, char *buf, loff_t pos, size_t size)
#else
static ssize_t mic_psmi_read_ptes(struct kobject *kobj,
	struct bin_attribute *attr, char *buf, loff_t pos, size_t size)
#endif
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct mic_psmi_ctx *psmi_ctx =
		&((mic_ctx_t *)dev_get_drvdata(dev))->bi_psmi;

	if (pos >= psmi_ctx->dma_tbl_size || size <= 0)
		return 0;
	if (pos + size > psmi_ctx->dma_tbl_size)
		size = psmi_ctx->dma_tbl_size - pos;
	memcpy(buf, psmi_ctx->dma_tbl, size);
	return size;
}

struct bin_attribute mic_psmi_ptes_attr = {
	.attr = {
		.name = "psmi_ptes",
		.mode = S_IRUSR
	},
	.read = mic_psmi_read_ptes
};

extern bool mic_psmi_enable;
module_param_named(psmi, mic_psmi_enable, bool, S_IRUSR);
MODULE_PARM_DESC(psmi, "Enable/disable mic psmi");
