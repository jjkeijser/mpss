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
 * Intel MIC Coprocessor State Management (COSM) Driver.
 */
#include <linux/version.h>
#include <linux/slab.h>
#include "cosm_main.h"

static ssize_t
mic_sysfs_show_string(char *buf, const char *string)
{
	if (string)
		return scnprintf(buf, PAGE_SIZE, "%s\n", string);
	else
		return 0;
}

static ssize_t
mic_sysfs_store_string(const char **string, const char *buf, size_t count)
{
	char *tmp;
	const char *old;

	if (!string)
		return -EINVAL;

	tmp = kmalloc(count + 1, GFP_KERNEL);
	if (!tmp)
		return -ENOMEM;

	strncpy(tmp, buf, count);
	if (tmp[count - 1] == '\n')
		tmp[count - 1] = '\0';
	else
		tmp[count] = '\0';

	old = xchg(string, tmp);
	kfree(old);
	return count;
}

static ssize_t
efi_image_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cosm_device *cdev = dev_get_drvdata(dev);
	ssize_t ret;

	mutex_lock(&cdev->config_mutex);
	ret = mic_sysfs_show_string(buf, cdev->config.efi_image);
	mutex_unlock(&cdev->config_mutex);

	return ret;
}

static ssize_t
efi_image_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct cosm_device *cdev = dev_get_drvdata(dev);
	ssize_t ret;

	mutex_lock(&cdev->config_mutex);
	ret = mic_sysfs_store_string(&cdev->config.efi_image, buf, count);
	mutex_unlock(&cdev->config_mutex);

	return ret;
}
static DEVICE_ATTR_RW(efi_image);

static ssize_t
kernel_image_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cosm_device *cdev = dev_get_drvdata(dev);
	ssize_t ret;

	mutex_lock(&cdev->config_mutex);
	ret = mic_sysfs_show_string(buf, cdev->config.kernel_image);
	mutex_unlock(&cdev->config_mutex);

	return ret;
}

static ssize_t
kernel_image_store(struct device *dev,
		   struct device_attribute *attr, const char *buf,
		   size_t count)
{
	struct cosm_device *cdev = dev_get_drvdata(dev);
	ssize_t ret;

	mutex_lock(&cdev->config_mutex);
	ret = mic_sysfs_store_string(&cdev->config.kernel_image, buf, count);
	mutex_unlock(&cdev->config_mutex);

	return ret;
}
static DEVICE_ATTR_RW(kernel_image);

static ssize_t
kernel_cmdline_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct cosm_device *cdev = dev_get_drvdata(dev);
	ssize_t ret;

	mutex_lock(&cdev->config_mutex);
	ret = mic_sysfs_show_string(buf, cdev->config.kernel_cmdline);
	mutex_unlock(&cdev->config_mutex);

	return ret;
}

static ssize_t
kernel_cmdline_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct cosm_device *cdev = dev_get_drvdata(dev);
	ssize_t ret;

	mutex_lock(&cdev->config_mutex);
	ret = mic_sysfs_store_string(&cdev->config.kernel_cmdline, buf, count);
	mutex_unlock(&cdev->config_mutex);

	return ret;
}
static DEVICE_ATTR_RW(kernel_cmdline);

static ssize_t
initramfs_image_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct cosm_device *cdev = dev_get_drvdata(dev);
	ssize_t ret;

	mutex_lock(&cdev->config_mutex);
	ret = mic_sysfs_show_string(buf, cdev->config.initramfs_image);
	mutex_unlock(&cdev->config_mutex);

	return ret;
}

static ssize_t
initramfs_image_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct cosm_device *cdev = dev_get_drvdata(dev);
	ssize_t ret;

	mutex_lock(&cdev->config_mutex);
	ret = mic_sysfs_store_string(&cdev->config.initramfs_image, buf, count);
	mutex_unlock(&cdev->config_mutex);

	return ret;
}
static DEVICE_ATTR_RW(initramfs_image);

static ssize_t
execute_on_ready_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct cosm_device *cdev = dev_get_drvdata(dev);

	u8 command = cdev->config.execute_on_ready;

	return scnprintf(buf, PAGE_SIZE, "%s\n", cosm_commands[command].string);
}

static ssize_t
execute_on_ready_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	int rc;
	u8 command;
	struct cosm_device *cdev = dev_get_drvdata(dev);

	rc = cosm_get_command_id(cdev, buf, &command);
	if (rc < 0)
		return rc;

	cdev->config.execute_on_ready = command;

	return count;
}
static DEVICE_ATTR_RW(execute_on_ready);

static ssize_t
boot_timeout_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cosm_device *cdev = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", cdev->config.boot_timeout);
}

static ssize_t
boot_timeout_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct cosm_device *cdev = dev_get_drvdata(dev);
	int ret;
	unsigned int timeout;

	ret = kstrtouint(buf, 10, &timeout);
	if (ret)
		goto exit;

	if (timeout < COSM_MIN_BOOT_TIMEOUT) {
		ret = -EINVAL;
		log_mic_err(cdev->index, "boot timeout smaller than minimal (%u)",
			    COSM_MIN_BOOT_TIMEOUT);
		goto exit;
	}

	cdev->config.boot_timeout = timeout;
	ret = count;
exit:
	return ret;
}
static DEVICE_ATTR_RW(boot_timeout);

static ssize_t
shutdown_timeout_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cosm_device *cdev = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", cdev->config.shutdown_timeout);
}

static ssize_t
shutdown_timeout_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct cosm_device *cdev = dev_get_drvdata(dev);
	int ret;
	unsigned int timeout;

	ret = kstrtouint(buf, 10, &timeout);
	if (ret)
		goto exit;

	if (timeout < COSM_MIN_SHUTDOWN_TIMEOUT) {
		ret = -EINVAL;
		log_mic_err(cdev->index,
			"shutdown timeout smaller than minimal (%u)",
			COSM_MIN_SHUTDOWN_TIMEOUT);
		goto exit;
	}

	cdev->config.shutdown_timeout = timeout;
	ret = count;
exit:
	return ret;
}
static DEVICE_ATTR_RW(shutdown_timeout);

static struct attribute *cosm_config_attrs[] = {
	&dev_attr_efi_image.attr,
	&dev_attr_kernel_image.attr,
	&dev_attr_kernel_cmdline.attr,
	&dev_attr_initramfs_image.attr,
	&dev_attr_execute_on_ready.attr,
	&dev_attr_boot_timeout.attr,
	&dev_attr_shutdown_timeout.attr,
	NULL
};

static const struct attribute_group cosm_config_group = {
	.name = "config",
	.attrs = cosm_config_attrs,
	.bin_attrs = NULL
};


/******************************************************************************
 * Initialization of default COSM group.
 ******************************************************************************/

static ssize_t
state_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cosm_device *cdev = dev_get_drvdata(dev);
	u8 state = cdev->state;

	if (state > MIC_STATE_LAST)
		return -EINVAL;

	return scnprintf(buf, PAGE_SIZE, "%s\n", cosm_states[state].string);
}

static ssize_t
state_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	int rc;
	u8 command;
	struct cosm_device *cdev = dev_get_drvdata(dev);

	rc = cosm_get_command_id(cdev, buf, &command);
	if (rc < 0)
		return rc;

	rc = cosm_set_command(cdev, command);
	if (rc < 0)
		return rc;

	return count;
}
static DEVICE_ATTR_RW(state);

static ssize_t
state_msg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cosm_device *cdev = dev_get_drvdata(dev);
	const char *state_msg = cdev->state_msg;

	if (state_msg)
		return scnprintf(buf, PAGE_SIZE, "%s\n", state_msg);

	return 0;
}
static DEVICE_ATTR_RO(state_msg);

static ssize_t
log_buf_read(struct file *file, struct kobject *kobj,
		struct bin_attribute *bin_attr, char *to,
		loff_t pos, size_t count)
{
	int ret = 0;
	struct device *dev = container_of(kobj, struct device, kobj);
	struct cosm_device *cdev = dev_get_drvdata(dev);

	ret = memory_read_from_buffer(to, count, &pos, cdev->log_buf_cpu_addr, cdev->log_buf_size);

	return ret;
}
static BIN_ATTR(log_buf, S_IRUSR | S_IRGRP, log_buf_read, NULL, 0);

static struct attribute *cosm_default_attrs[] = {
	&dev_attr_state.attr,
	&dev_attr_state_msg.attr,
	NULL
};

static struct bin_attribute *cosm_default_bin_attrs[] = {
	&bin_attr_log_buf,
	NULL
};

static const struct attribute_group cosm_default_group = {
	.attrs = cosm_default_attrs,
	.bin_attrs = cosm_default_bin_attrs
};


/******************************************************************************
 * Initialization of all COSM groups.
 ******************************************************************************/

static const struct attribute_group *cosm_groups[] = {
	&cosm_config_group,
	&cosm_default_group,
	NULL,
};

void cosm_sysfs_init(struct cosm_device *cdev)
{
	cdev->sysfs_attr_group = cosm_groups;
}
