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
 *
 * Based on 'drivers/firmware/dmi-id.c'
 *
 * Export SMBIOS/DMI info via sysfs to userspace
 *
 * Copyright 2007, Lennart Poettering
 *
 * Licensed under GPLv2
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kobject.h>
#include <linux/dmi.h>
#include <linux/device.h>
#include <linux/capability.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/stat.h>

#ifdef MIC_IN_KERNEL_BUILD
#include <linux/mic_common.h>
#else
#include "../common/mic_common.h"
#endif
#include "../common/mic_dev.h"
#include "mic_device.h"
#include "mic_hw.h"

struct mic_sysfs_dev_attr {
	struct device_attribute dev_attr;
	int index;
};

#define MIC_SYSFS_ATTR(_name, _mode, _show, _index)		\
struct mic_sysfs_dev_attr mic_sysfs_dev_attr_##_name =		\
	{ .dev_attr = __ATTR(_name, _mode, _show, NULL),	\
	  .index = _index }

#define to_mic_sysfs_dev_attr(_dev_attr) \
	container_of(_dev_attr, struct mic_sysfs_dev_attr, dev_attr)

static inline struct mic_device *sysfsdev_to_xdev(struct device *dev)
{
	struct cosm_device *cdev = dev_get_drvdata(dev);
	return dev_get_drvdata(cdev->dev.parent);
}

/*
 * SPAD attributes
 */
static ssize_t mic_sysfs_spad_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct mic_device *xdev = sysfsdev_to_xdev(dev);
	int index = to_mic_sysfs_dev_attr(attr)->index;

	if (index == MIC_SPAD_POST_CODE) {
		u8 val = mic_read_post_code(xdev);
		return scnprintf(buf, PAGE_SIZE, "0x%02x\n", val);
	} else {
		return scnprintf(buf, PAGE_SIZE, "0x%08x\n",
				 mic_read_spad(xdev, index));
	}
}

MIC_SYSFS_ATTR(spad0, S_IRUGO, mic_sysfs_spad_show, 0);
MIC_SYSFS_ATTR(spad1, S_IRUGO, mic_sysfs_spad_show, 1);
MIC_SYSFS_ATTR(spad2, S_IRUGO, mic_sysfs_spad_show, 2);
MIC_SYSFS_ATTR(spad3, S_IRUGO, mic_sysfs_spad_show, 3);
MIC_SYSFS_ATTR(spad4, S_IRUGO, mic_sysfs_spad_show, 4);
MIC_SYSFS_ATTR(spad5, S_IRUGO, mic_sysfs_spad_show, 5);
MIC_SYSFS_ATTR(spad6, S_IRUGO, mic_sysfs_spad_show, 6);
MIC_SYSFS_ATTR(spad7, S_IRUGO, mic_sysfs_spad_show, 7);
MIC_SYSFS_ATTR(post_code, S_IRUGO, mic_sysfs_spad_show, MIC_SPAD_POST_CODE);

static struct attribute *mic_sysfs_spad_attributes[] = {
	&mic_sysfs_dev_attr_spad0.dev_attr.attr,
	&mic_sysfs_dev_attr_spad1.dev_attr.attr,
	&mic_sysfs_dev_attr_spad2.dev_attr.attr,
	&mic_sysfs_dev_attr_spad3.dev_attr.attr,
	&mic_sysfs_dev_attr_spad4.dev_attr.attr,
	&mic_sysfs_dev_attr_spad5.dev_attr.attr,
	&mic_sysfs_dev_attr_spad6.dev_attr.attr,
	&mic_sysfs_dev_attr_spad7.dev_attr.attr,
	&mic_sysfs_dev_attr_post_code.dev_attr.attr,
	NULL
};

static struct attribute_group mic_sysfs_spad_group = {
	.name = "spad",
	.attrs = mic_sysfs_spad_attributes,
};


/*
 * DMI attributes
 */
static ssize_t mic_sysfs_info_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct mic_device *xdev = sysfsdev_to_xdev(dev);
	int index = to_mic_sysfs_dev_attr(attr)->index;
	struct mic_dmi_info *dinfo;
	char *entry;

	if (!xdev)
		goto field_empty;

	dinfo = &xdev->dmi_info;

	if (!dinfo->dmi_avail) {
		goto field_empty;
	}

	entry = dinfo->sysfs_entry[index];

	if (entry)
		return scnprintf(buf, PAGE_SIZE, "%s\n", entry);

field_empty:
	return scnprintf(buf, PAGE_SIZE, "<empty>\n");
}


#define MIC_SYSFS_DMI_ATTR(_name, _mode, _index) \
	MIC_SYSFS_ATTR(_name, _mode, mic_sysfs_info_show, _index)

MIC_SYSFS_DMI_ATTR(baseboard_type,
		S_IRUGO, MIC_DMI_BASEBOARD_TYPE);
MIC_SYSFS_DMI_ATTR(bios_release_date,
		S_IRUGO, MIC_DMI_BIOS_RELEASE_DATE);
MIC_SYSFS_DMI_ATTR(bios_vendor,
		S_IRUGO, MIC_DMI_BIOS_VENDOR);
MIC_SYSFS_DMI_ATTR(bios_version,
		S_IRUGO, MIC_DMI_BIOS_VERSION);
MIC_SYSFS_DMI_ATTR(bios_me_version,
		S_IRUGO, MIC_DMI_BIOS_ME_VERSION);
MIC_SYSFS_DMI_ATTR(bios_smc_version,
		S_IRUGO, MIC_DMI_BIOS_SMC_VERSION);
MIC_SYSFS_DMI_ATTR(fab_version,
		S_IRUGO, MIC_DMI_FAB_VERSION);
MIC_SYSFS_DMI_ATTR(mcdram_size,
		S_IRUGO, MIC_DMI_MCDRAM_SIZE);
MIC_SYSFS_DMI_ATTR(mcdram_version,
		S_IRUGO, MIC_DMI_MCDRAM_VERSION);
MIC_SYSFS_DMI_ATTR(mic_driver_version,
		S_IRUGO, MIC_DMI_MIC_DRIVER_VERSION);
MIC_SYSFS_DMI_ATTR(mic_family,
		S_IRUGO, MIC_DMI_MIC_FAMILY);
MIC_SYSFS_DMI_ATTR(ntb_eeprom_version,
		S_IRUGO, MIC_DMI_NTB_EEPROM_VERSION);
MIC_SYSFS_DMI_ATTR(oem_strings,
		S_IRUGO, MIC_DMI_OEM_STRINGS);
MIC_SYSFS_DMI_ATTR(processor_id_decoded,
		S_IRUGO, MIC_DMI_PROCESSOR_ID_DECODED);
MIC_SYSFS_DMI_ATTR(processor_manufacturer,
		S_IRUGO, MIC_DMI_PROCESSOR_MANUFACTURER);
MIC_SYSFS_DMI_ATTR(processor_part_number,
		S_IRUGO, MIC_DMI_PROCESSOR_PART_NUMBER);
MIC_SYSFS_DMI_ATTR(processor_serial_number,
		S_IRUGO, MIC_DMI_PROCESSOR_SERIAL_NUMBER);
MIC_SYSFS_DMI_ATTR(processor_stepping,
		S_IRUGO, MIC_DMI_PROCESSOR_STEPPING);
MIC_SYSFS_DMI_ATTR(processor_version,
		S_IRUGO, MIC_DMI_PROCESSOR_VERSION);
MIC_SYSFS_DMI_ATTR(system_manufacturer,
		S_IRUGO, MIC_DMI_SYSTEM_MANUFACTURER);
MIC_SYSFS_DMI_ATTR(system_product_name,
		S_IRUGO, MIC_DMI_SYSTEM_PRODUCT_NAME);
MIC_SYSFS_DMI_ATTR(system_serial_number,
		S_IRUGO, MIC_DMI_SYSTEM_SERIAL_NUMBER);
MIC_SYSFS_DMI_ATTR(system_sku_number,
		S_IRUGO, MIC_DMI_SYSTEM_SKU_NUMBER);
MIC_SYSFS_DMI_ATTR(system_uuid,
		S_IRUGO, MIC_DMI_SYSTEM_UUID);
MIC_SYSFS_DMI_ATTR(system_version,
		S_IRUGO, MIC_DMI_SYSTEM_VERSION);


static struct attribute *mic_sysfs_info_attributes[] = {
	&mic_sysfs_dev_attr_baseboard_type.dev_attr.attr,
	&mic_sysfs_dev_attr_bios_release_date.dev_attr.attr,
	&mic_sysfs_dev_attr_bios_vendor.dev_attr.attr,
	&mic_sysfs_dev_attr_bios_version.dev_attr.attr,
	&mic_sysfs_dev_attr_bios_me_version.dev_attr.attr,
	&mic_sysfs_dev_attr_bios_smc_version.dev_attr.attr,
	&mic_sysfs_dev_attr_fab_version.dev_attr.attr,
	&mic_sysfs_dev_attr_mcdram_size.dev_attr.attr,
	&mic_sysfs_dev_attr_mcdram_version.dev_attr.attr,
	&mic_sysfs_dev_attr_mic_driver_version.dev_attr.attr,
	&mic_sysfs_dev_attr_mic_family.dev_attr.attr,
	&mic_sysfs_dev_attr_ntb_eeprom_version.dev_attr.attr,
	&mic_sysfs_dev_attr_oem_strings.dev_attr.attr,
	&mic_sysfs_dev_attr_processor_id_decoded.dev_attr.attr,
	&mic_sysfs_dev_attr_processor_manufacturer.dev_attr.attr,
	&mic_sysfs_dev_attr_processor_part_number.dev_attr.attr,
	&mic_sysfs_dev_attr_processor_serial_number.dev_attr.attr,
	&mic_sysfs_dev_attr_processor_stepping.dev_attr.attr,
	&mic_sysfs_dev_attr_processor_version.dev_attr.attr,
	&mic_sysfs_dev_attr_system_manufacturer.dev_attr.attr,
	&mic_sysfs_dev_attr_system_product_name.dev_attr.attr,
	&mic_sysfs_dev_attr_system_serial_number.dev_attr.attr,
	&mic_sysfs_dev_attr_system_sku_number.dev_attr.attr,
	&mic_sysfs_dev_attr_system_uuid.dev_attr.attr,
	&mic_sysfs_dev_attr_system_version.dev_attr.attr,
	NULL
};

static struct attribute_group mic_sysfs_info_group = {
	.name = "info",
	.attrs = mic_sysfs_info_attributes,
};

static const struct attribute_group *mic_sysfs_attribute_groups[] = {
	&mic_sysfs_spad_group,
	&mic_sysfs_info_group,
	NULL
};

int mic_sysfs_init(struct cosm_device *cdev)
{
	return sysfs_create_groups(&cdev->sysfs_dev->kobj,
			mic_sysfs_attribute_groups);
}

void mic_sysfs_uninit(struct cosm_device *cdev)
{
	sysfs_remove_groups(&cdev->sysfs_dev->kobj, mic_sysfs_attribute_groups);
}
