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

#include <mic/micscif.h>

unsigned long scif_get_maxid(void);
static ssize_t show_scif_maxid(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", ms_info.mi_maxid);
}
static DEVICE_ATTR(maxnode, S_IRUGO, show_scif_maxid, NULL);

unsigned long scif_get_total(void);
static ssize_t show_scif_total(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", ms_info.mi_total);
}
static DEVICE_ATTR(total, S_IRUGO, show_scif_total, NULL);

unsigned long scif_get_nodes(void);
static ssize_t show_scif_nodes(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;
	int node;

	len += snprintf(buf + len, PAGE_SIZE, "%d:", ms_info.mi_total);
	len += snprintf(buf + len, PAGE_SIZE, "%d", ms_info.mi_nodeid);

	for (node = 0; node <= ms_info.mi_maxid; node++) {
		if (scif_dev[node].sd_state == SCIFDEV_RUNNING ||
			scif_dev[node].sd_state == SCIFDEV_SLEEPING ||
			is_self_scifdev(&scif_dev[node])) {
			len += snprintf(buf + len, PAGE_SIZE, ",%d", scif_dev[node].sd_node);
		}
	}

	len += snprintf(buf + len, PAGE_SIZE, "\n");
	return len;
}
static DEVICE_ATTR(nodes, S_IRUGO, show_scif_nodes, NULL);

static ssize_t show_watchdog_to(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", ms_info.mi_watchdog_to);
}

static ssize_t store_watchdog_to(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	int i, ret;

	if (sscanf(buf, "%d", &i) != 1)
		goto invalid;

	if (i <= 0)
		goto invalid;

	ms_info.mi_watchdog_to = i;
	ret = strlen(buf);
	printk("Current watchdog timeout %d seconds\n", ms_info.mi_watchdog_to);
	goto bail;

invalid:
	printk(KERN_ERR "Attempt to set invalid watchdog timeout\n");
	ret = -EINVAL;
bail:
	return ret;
}
static DEVICE_ATTR(watchdog_to, S_IRUGO | S_IWUSR, show_watchdog_to, store_watchdog_to);

static ssize_t show_watchdog_enabled(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", ms_info.mi_watchdog_enabled);
}

static ssize_t store_watchdog_enabled(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	int i, ret;
#ifndef _MIC_SCIF_
	struct micscif_dev *scifdev;
	int node;
#endif

	if (sscanf(buf, "%d", &i) != 1)
		goto invalid;

	if (i < 0)
		goto invalid;

	if (i && !ms_info.mi_watchdog_enabled) {
		ms_info.mi_watchdog_enabled = 1;
#ifndef _MIC_SCIF_
		for (node = 1; node <= ms_info.mi_maxid; node++) {
			scifdev = &scif_dev[node];
			if (scifdev->sd_ln_wq)
				queue_delayed_work(scifdev->sd_ln_wq, 
					&scifdev->sd_watchdog_work, NODE_ALIVE_TIMEOUT);
		}
#endif
	}

	if (!i)
		ms_info.mi_watchdog_enabled = 0;

	ret = strlen(buf);
	printk("Watchdog timeout enabled = %d\n", ms_info.mi_watchdog_enabled);
	goto bail;
invalid:
	ret = -EINVAL;
bail:
	return ret;
}
static DEVICE_ATTR(watchdog_enabled, S_IRUGO | S_IWUSR, show_watchdog_enabled, store_watchdog_enabled);

static ssize_t show_watchdog_auto_reboot(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", ms_info.mi_watchdog_auto_reboot);
}

static ssize_t store_watchdog_auto_reboot(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	int i, ret;

	if (sscanf(buf, "%d", &i) != 1)
		goto invalid;

	if (i < 0)
		goto invalid;

	if (i && !ms_info.mi_watchdog_auto_reboot)
		ms_info.mi_watchdog_auto_reboot = 1;

	if (!i)
		ms_info.mi_watchdog_auto_reboot = 0;

	ret = strlen(buf);
	printk("Watchdog auto reboot enabled = %d\n", ms_info.mi_watchdog_auto_reboot);
	goto bail;
invalid:
	ret = -EINVAL;
bail:
	return ret;
}
static DEVICE_ATTR(watchdog_auto_reboot, S_IRUGO | S_IWUSR, show_watchdog_auto_reboot, store_watchdog_auto_reboot);

static ssize_t show_proxy_dma_threshold(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%lld\n", ms_info.mi_proxy_dma_threshold);
}

static ssize_t store_proxy_dma_threshold(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	int ret;
	uint64_t i;

	if (sscanf(buf, "%lld", &i) != 1)
		goto invalid;

	ms_info.mi_proxy_dma_threshold = i;
	ret = strlen(buf);
	printk("P2P proxy DMA Threshold = %lld bytes\n", ms_info.mi_proxy_dma_threshold);
	goto bail;
invalid:
	ret = -EINVAL;
bail:
	return ret;
}
static DEVICE_ATTR(proxy_dma_threshold, S_IRUGO | S_IWUSR, show_proxy_dma_threshold, store_proxy_dma_threshold);

static struct attribute *scif_attributes[] = {
	&dev_attr_maxnode.attr,
	&dev_attr_total.attr,
	&dev_attr_nodes.attr,
	&dev_attr_watchdog_to.attr,
	&dev_attr_watchdog_enabled.attr,
	&dev_attr_watchdog_auto_reboot.attr,
	&dev_attr_proxy_dma_threshold.attr,
	NULL
};

struct attribute_group scif_attr_group = {
	.attrs = scif_attributes
};
