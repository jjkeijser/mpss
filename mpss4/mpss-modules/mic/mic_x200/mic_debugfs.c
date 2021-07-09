/*
 * Intel MIC Platform Software Stack (MPSS)
 * Copyright (c) 2016, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * Intel MIC X200 PCIe driver.
 */
#include <linux/debugfs.h>
#include <linux/pci.h>
#include <linux/seq_file.h>

#ifdef MIC_IN_KERNEL_BUILD
#include <linux/mic_common.h>
#else
#include "../common/mic_common.h"
#endif
#include "../common/mic_dev.h"
#include "mic_device.h"
#include "mic_hw.h"

/* Debugfs parent dir */
static struct dentry *mic_dbg;

static int mic_alut_show(struct seq_file *s, void *pos)
{
	int i;
	struct mic_device *xdev = s->private;
	unsigned long flags;

	const char *head_str =
	        " id ||    dma_addr    | perm(r/w) | ref_count\n";

	int block_size = MIC_APER_SIZE / ALUT_ENTRY_SIZE;

	seq_printf(s, head_str);

	for (i = 0; i < strlen(head_str); ++i)
		seq_puts(s, "=");
	seq_puts(s, "\n");

	if (xdev->alut) {
		struct mic_alut_info *alut_info = xdev->alut;
		spin_lock_irqsave(&alut_info->alut_lock, flags);
		for (i = 0; i < ALUT_ENTRY_COUNT; i++) {
			struct mic_alut entry = mic_alut_get(xdev, i);
			char sep = i % block_size ? '|' : 'T';
			seq_printf(s,
				   "%3d %c%c %#14llx %c       %03o %c %10lld\n",
				   i, sep, sep, entry.dma_addr, sep, entry.perm,
				   sep, alut_info->entry[i].ref_count);
		}
		spin_unlock_irqrestore(&alut_info->alut_lock, flags);
	} else {
		seq_printf(s, "ALUT is not enabled\n");
	}

	for (i = 0; i < strlen(head_str); ++i)
		seq_puts(s, "=");
	seq_puts(s, "\n");

	return 0;
}

static int mic_alut_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, mic_alut_show, inode->i_private);
}

static int mic_alut_debug_release(struct inode *inode, struct file *file)
{
	return single_release(inode, file);
}

static const struct file_operations mic_alut_file_ops = {
	.owner   = THIS_MODULE,
	.open    = mic_alut_debug_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = mic_alut_debug_release
};

static int mic_gpio_show(struct seq_file *s, void *pos)
{
	return 0;
}


static int mic_gpio_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, mic_gpio_show, inode->i_private);
}

static ssize_t mic_gpio_debug_write(struct file *file, const char __user *user_buf,
			size_t size, loff_t *ppos, int gpio_pin)
{
	struct mic_device *xdev = file->f_inode->i_private;
	char *buf;
	int signal_len;

	if ((buf = kmalloc(size + 1, GFP_KERNEL)) == NULL)
		return -EINVAL;
	if (copy_from_user(buf, user_buf, size))
		goto error;
	buf[size] = '\0';

	if (sysfs_streq(buf, "short"))
		signal_len = MIC_GPIO_SHORT;
	else if (sysfs_streq(buf, "long"))
		signal_len = MIC_GPIO_LONG;
	else
		goto error;

	if (gpio_pin == MIC_GPIO1 && signal_len == MIC_GPIO_LONG)
		goto error;

	mic_hw_send_gpio_signal(xdev, gpio_pin, signal_len);
	kfree(buf);
	return size;
error:
	kfree(buf);
	return -EINVAL;
}

static ssize_t mic_gpio1_debug_write(struct file *file, const char __user *user_buf,
			size_t size, loff_t *ppos)
{
	return mic_gpio_debug_write(file, user_buf, size, ppos, MIC_GPIO1);
}

static ssize_t mic_gpio2_debug_write(struct file *file, const char __user *user_buf,
			size_t size, loff_t *ppos)
{
	return mic_gpio_debug_write(file, user_buf, size, ppos, MIC_GPIO2);
}

static const struct file_operations mic_gpio1_file_ops = {
	.owner	 = THIS_MODULE,
	.open	 = mic_gpio_debug_open,
	.read	 = seq_read,
	.write	 = mic_gpio1_debug_write,
	.release = single_release
};

static int mic_rid_lut_show(struct seq_file *s, void *pos, bool link)
{
	u16 entry;
	int i;
	struct mic_device *xdev = s->private;
	const char *head_str =
		"idx | bus:dev | enabled\n";

	if (!&xdev->mmio)
		return -ENODEV;

	seq_puts(s, head_str);
	for (i = 0; i < strlen(head_str); ++i)
		seq_putc(s, '=');
	seq_putc(s, '\n');

	for (i = 0; i < MIC_MAX_RID_LUT_ENTRIES; ++i) {
		entry = mic_read_rid_lut(xdev, link, i);
		seq_printf(s, "%3d |  %02x:%02x  | %x\n",
			i,
			PCI_BUS_NUM(entry),
			PCI_SLOT(entry),
			entry & 0x1);
	}
	return 0;
}

static int mic_rid_lut_link_show(struct seq_file *s, void *pos)
{
	return mic_rid_lut_show(s, pos, true);
}

static int mic_rid_lut_virtual_show(struct seq_file *s, void *pos)
{
	return mic_rid_lut_show(s, pos, false);
}

static int mic_rid_lut_link_open(struct inode *inode, struct file *file)
{
	return single_open(file, mic_rid_lut_link_show, inode->i_private);
}

static int mic_rid_lut_virtual_open(struct inode *inode, struct file *file)
{
	return single_open(file, mic_rid_lut_virtual_show, inode->i_private);
}

static const struct file_operations mic_gpio2_file_ops = {
	.owner	 = THIS_MODULE,
	.open	 = mic_gpio_debug_open,
	.read	 = seq_read,
	.write	 = mic_gpio2_debug_write,
	.release = single_release
};

static const struct file_operations mic_rid_lut_link_ops = {
	.owner   = THIS_MODULE,
	.open    = mic_rid_lut_link_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release
};

static const struct file_operations mic_rid_lut_virtual_ops = {
	.owner   = THIS_MODULE,
	.open    = mic_rid_lut_virtual_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release
};

/**
 * mic_create_debug_dir - Initialize MIC debugfs entries.
 */
void mic_create_debug_dir(struct mic_device *xdev)
{
	char name[16];

	if (!mic_dbg)
		return;

	scnprintf(name, sizeof(name), "mic%d", xdev->id);
	xdev->dbg_dir = debugfs_create_dir(name, mic_dbg);
	if (!xdev->dbg_dir)
		return;

	debugfs_create_file("alut", 0444, xdev->dbg_dir, xdev,
				&mic_alut_file_ops);

	debugfs_create_file("gpio1", 0644, xdev->dbg_dir, xdev,
				&mic_gpio1_file_ops);

	debugfs_create_file("gpio2", 0644, xdev->dbg_dir, xdev,
				&mic_gpio2_file_ops);

	debugfs_create_file("rid_lut_link", 0444, xdev->dbg_dir, xdev,
			&mic_rid_lut_link_ops);

	debugfs_create_file("rid_lut_virtual", 0444, xdev->dbg_dir, xdev,
			&mic_rid_lut_virtual_ops);
}

/**
 * mic_delete_debug_dir - Uninitialize MIC debugfs entries.
 */
void mic_delete_debug_dir(struct mic_device *xdev)
{
	if (!xdev->dbg_dir)
		return;

	debugfs_remove_recursive(xdev->dbg_dir);
}

/**
 * mic_init_debugfs - Initialize global debugfs entry.
 */
void __init mic_init_debugfs(void)
{
	mic_dbg = debugfs_create_dir(KBUILD_MODNAME, NULL);
	if (!mic_dbg)
		pr_err("can't create debugfs dir\n");
}

/**
 * mic_exit_debugfs - Uninitialize global debugfs entry
 */
void mic_exit_debugfs(void)
{
	debugfs_remove(mic_dbg);
}
