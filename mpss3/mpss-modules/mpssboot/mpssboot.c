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
#include <linux/init.h>
#include <scif.h>
#include <linux/errno.h>
#include <linux/hardirq.h>
#include <linux/types.h>
#include <linux/capability.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/gfp.h>
#include <linux/vmalloc.h>
#include <asm/io.h>
#include <linux/kernel.h>
#include <linux/mm_types.h>
#include <linux/jiffies.h>
#include <linux/timer.h>
#include <linux/irqflags.h>
#include <linux/time.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/semaphore.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <asm/bug.h>
#include <linux/pci.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <asm/atomic.h>
#include <linux/netdevice.h>
#include <linux/debugfs.h>

#define ACPT_BOOTED	  1
#define ACPT_BOOT_ACK	  2
#define ACPT_NACK_VERSION 3
#define ACPT_REQUEST_TIME 4
#define ACPT_TIME_DATA	  5

#define ACPT_VERSION	1

static dev_t dev;
static struct class *class;
static struct device *mbdev;

static int host_notified;
static struct timespec tod;
static int timeset = 0;

static ssize_t
show_timesync(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "Time: %s\n", timeset? "set" : "not set");
}

static ssize_t
set_synctime(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct scif_portID port = {0, MIC_NOTIFY};
	static scif_epd_t epd;
	int proto = ACPT_REQUEST_TIME;
	int version = ACPT_VERSION; 
	int err;

	epd = scif_open();

	if ((err = scif_connect(epd, &port))) {
		printk("MPSSBOOT error, synctime connect failed: %d\n", err);
		goto close_synctime;
	}

	if ((err = scif_send(epd, &version, sizeof(version), 0)) != sizeof(version)) {
		printk("MPSSBOOT send version failed: %d\n", err);
		goto close_synctime;
	}

	if ((err = scif_send(epd, &proto, sizeof(proto), 0)) != sizeof(proto)) {
		printk("MPSSBOOT send boot finished failed: %d\n", err);
		goto close_synctime;
	}

	if ((err = scif_recv(epd, &proto, sizeof(proto), SCIF_RECV_BLOCK)) != sizeof(proto)) {
		printk("MPSSBOOT protocol recv ack failed: %d\n", err);
		goto close_synctime;
	}

	if (proto != ACPT_TIME_DATA) {
		printk("MPSSBOOT failed to receive time data packet %d\n", proto);
		goto close_synctime;
	}

	if ((err = scif_recv(epd, &tod, sizeof(tod), SCIF_RECV_BLOCK)) != sizeof(tod)) {
		printk("MPSSBOOT time data read size failed: %d\n", err);
		goto close_synctime;
	}

	do_settimeofday(&tod);
	printk("MPSSBOOT Time of day sycned with host\n");
	timeset = 1;

close_synctime:
	scif_close(epd);
	return count;
}
static DEVICE_ATTR(synctime, S_IRUGO | S_IWUSR, show_timesync, set_synctime);

static ssize_t
show_host_notified(struct device *dev, struct device_attribute *attr, char *buf)
{
        return snprintf(buf, PAGE_SIZE, "%d\n", host_notified);
}

static ssize_t
set_host_notified(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct scif_portID port = {0, MIC_NOTIFY};
	static scif_epd_t epd;
	int proto = ACPT_BOOTED;
	int version = ACPT_VERSION; 
	int err;

	epd = scif_open();

	if ((err = scif_connect(epd, &port))) {
		printk("MPSSBOOT error, notify connect failed: %d\n", err);
		goto close_notify;
	}

	if ((err = scif_send(epd, &version, sizeof(version), 0)) != sizeof(version)) {
		printk("MPSSBOOT send version failed: %d\n", err);
		goto close_notify;
	}

	if ((err = scif_send(epd, &proto, sizeof(proto), 0)) != sizeof(proto)) {
		printk("MPSSBOOT send boot finished failed: %d\n", err);
		goto close_notify;
	}

	if ((err = scif_recv(epd, &proto, sizeof(proto), SCIF_RECV_BLOCK)) != sizeof(proto)) {
		printk("MPSSBOOT protocol recv ack failed: %d\n", err);
		goto close_notify;
	}

	if (proto != ACPT_BOOT_ACK)
		printk("MPSSBOOT failed to receive boot ACK, got %d\n", proto);
	else
		printk("MPSSBOOT Boot acknowledged\n");

close_notify:
	scif_close(epd);
	return count;
}
static DEVICE_ATTR(host_notified, S_IRUGO | S_IWUSR, show_host_notified, set_host_notified);

static struct attribute *mb_attributes[] = {
	&dev_attr_synctime.attr,
	&dev_attr_host_notified.attr,
	NULL
};

struct attribute_group mb_attr_group = {
	.attrs = mb_attributes
};

/* This function closes the endpoint established on init */
static void
mpssboot_exit(void)
{
	sysfs_remove_group(&mbdev->kobj, &mb_attr_group);
	device_destroy(class, dev);
	class_destroy(class);
}

static char *
mpssboot_devnode(struct device *dev, mode_t *mode)
{
	return kasprintf(GFP_KERNEL, "%s", dev_name(dev));
}

/* This function initializes a SCIF connection to the host */
static int
mpssboot_init(void)
{
	//static struct device dev;
	int result;

	alloc_chrdev_region(&dev, 0, 2, "micnotify");
	class = class_create(THIS_MODULE, "micnotify");
	class->devnode = mpssboot_devnode;
	mbdev = device_create(class, NULL, dev, NULL, "notify");

	result = sysfs_create_group(&mbdev->kobj, &mb_attr_group);
	result = result;
	return 0;
}

module_init(mpssboot_init);
module_exit(mpssboot_exit);
MODULE_LICENSE("GPL");

