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
 * Based on 'drivers/firmware/dmi_scan.c'
 */

#include <linux/types.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/ctype.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/dmi.h>

#ifdef MIC_IN_KERNEL_BUILD
#include <linux/mic_common.h>
#else
#include "../common/mic_common.h"
#endif
#include "../common/mic_dev.h"
#include "mic_device.h"
#include "mic_hw.h"

static const char *dmi_string_nosave(const struct dmi_header *dm, u8 s)
{
	const u8 *bp = ((u8 *)dm) + dm->length;

	if (s) {
		s--;
		while (s > 0 && *bp) {
			bp += strlen(bp) + 1;
			s--;
		}

		if (*bp)
			return bp;
	}

	return "";
}

static char *dmi_string(const struct dmi_header *dm, u8 s)
{
	const char *bp = dmi_string_nosave(dm, s);
	char *str;
	size_t len;

	len = strlen(bp) + 1;
	str = kmalloc(len, GFP_KERNEL);
	if (str != NULL)
		strncpy(str, bp, len);

	return str;
}

/*
 *	We have to be cautious here. We have seen BIOSes with DMI pointers
 *	pointing to completely the wrong place for example
 */
static void dmi_table(struct mic_device *xdev,
			  void (*decode)(struct mic_device *xdev,
				     const struct dmi_header *, void *),
		      void *private_data)
{
	u8 *data, *buf;
	int i = 0;
	struct mic_dmi_info *dinfo = &xdev->dmi_info;
	u16 len = dinfo->dmi_len;
	u16 num = dinfo->dmi_num;

	buf = xdev->aper.va + dinfo->dmi_base;
	data = buf;

	/*
	 *	Stop when we see all the items the table claimed to have
	 *	OR we run off the end of the table (also happens)
	 */
	while ((i < num) && (data - buf + sizeof(struct dmi_header)) <= len) {
		const struct dmi_header *dm = (const struct dmi_header *)data;

		/*
		 *  We want to know the total length (formatted area and
		 *  strings) before decoding to make sure we won't run off the
		 *  table in dmi_decode or dmi_string
		 */
		data += dm->length;
		while ((data - buf < len - 1) && (data[0] || data[1]))
				data++;
		if (data - buf < len - 1)
			decode(xdev, dm, private_data);
		data += 2;
		i++;
	}
}

static int dmi_checksum(const u8 *buf, u8 len)
{
	u8 sum = 0;
	int a;

	for (a = 0; a < len; a++)
		sum += buf[a];

	return sum == 0;
}

static void dmi_save_processor(struct mic_device *xdev,
			       const struct dmi_header *dm)
{
	const char *d = (const char *)dm;
	const u8 *p = &d[DMI_PROCESSOR_ID_DECODED];
	u32 eax = (u32)(*(const u32 *)(p));
	struct mic_dmi_info *dinfo = &xdev->dmi_info;
	char *s;

	dev_dbg(&xdev->pdev->dev,
		"CPUID: %02X %02X %02X %02X %02X %02X %02X %02X\n",
		p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);

	/* processor signature */
	s = kmalloc(64, GFP_KERNEL);
	if (!s)
		return;

	scnprintf(s, 64, "Type %u, Family %u, Model %u, Stepping %u",
		  (eax >> 12) & 0x3, ((eax >> 20) & 0xFF) + ((eax >> 8) & 0x0F),
		  ((eax >> 12) & 0xF0) + ((eax >> 4) & 0x0F), eax & 0xF);

	dinfo->sysfs_entry[MIC_DMI_PROCESSOR_ID_DECODED] = s;

	/* processor stepping */
	s = kmalloc(3, GFP_KERNEL);
	if (!s)
		return;

	switch (eax & 0xF) {
	case 0:
		scnprintf(s, 3, "%s", "A0");
		xdev->stepping = MIC_STEP_A0;
		break;
	case 1:
		scnprintf(s, 3, "%s", "B0");
		xdev->stepping = MIC_STEP_B0;
		break;
	default:
		scnprintf(s, 3, "%s", "??");
		log_mic_dbg(xdev->id, "not supported stepping %u", eax & 0xF);
		xdev->stepping = MIC_STEP_UNKNOWN;
	}

	dinfo->sysfs_entry[MIC_DMI_PROCESSOR_STEPPING] = s;

	/* mic family */
	s = kmalloc(5, GFP_KERNEL);
	if (!s)
		return;

	scnprintf(s, 5, "%s", "x200");

	dinfo->sysfs_entry[MIC_DMI_MIC_FAMILY] = s;
}

static void dmi_save_brd_type(struct mic_device *xdev,
			      const struct dmi_header *dm)
{
	const char *d = (const char *)dm;
	const u8 *p = &d[DMI_BASEBOARD_TYPE];
	struct mic_dmi_info *dinfo = &xdev->dmi_info;
	char *s;

	s = kmalloc(5, GFP_KERNEL);
	if (!s)
		return;

	scnprintf(s, 5, "0x%02x", *p);

	dinfo->sysfs_entry[MIC_DMI_BASEBOARD_TYPE] = s;
}

static void dmi_save_entry(struct mic_device *xdev, const struct dmi_header *dm,
			   int entry, int offset)
{
	const char *d = (const char *)dm;
	struct mic_dmi_info *dinfo = &xdev->dmi_info;
	char *p;

	p = dmi_string(dm, d[offset]);

	if (p && entry < MIC_DMI_LAST_ENTRY)
		dinfo->sysfs_entry[entry] = p;
}

static void dmi_save_uuid(struct mic_device *xdev, const struct dmi_header *dm,
			  int entry, int offset)
{
	const u8 *d = (u8 *)dm + offset;
	struct mic_dmi_info *dinfo = &xdev->dmi_info;
	char *s;
	int is_ff = 1, is_00 = 1, i;

	for (i = 0; i < 16 && (is_ff || is_00); i++) {
		if (d[i] != 0x00)
			is_00 = 0;
		if (d[i] != 0xFF)
			is_ff = 0;
	}

	if (is_ff || is_00)
		return;

	s = kmalloc(37, GFP_KERNEL);
	if (!s)
		return;

	scnprintf(s, 37, "%pUL", d);
	dinfo->sysfs_entry[MIC_DMI_SYSTEM_UUID] = s;
}

static void dmi_save_oem_strings(struct mic_device *xdev,
				 const struct dmi_header *dm)
{
	int i, count = *(u8 *)(dm + 1);
	struct mic_dmi_info *dinfo = &xdev->dmi_info;
	ssize_t len = 0;
	char *s;

	s = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!s)
		return;

	s[0] = '\0';

	for (i = 1; i <= count; i++) {
		const char *oemname = dmi_string_nosave(dm, i);
		ssize_t oem_len = strlen(oemname);

		if (oem_len + len + 1 > PAGE_SIZE)
			break;

		strncat(s, oemname, oem_len);
		len += oem_len + 1;
		strncat(s, " ", 1);
	}
	dinfo->sysfs_entry[MIC_DMI_OEM_STRINGS] = s;
}

static void dmi_parse_memsize(struct mic_device *xdev,
			      const struct dmi_header *dm)
{
	struct mic_dmi_info *dinfo = &xdev->dmi_info;

	const char *d = (const char *)dm;
	const u8 *p = &d[DMI_MCDRAM_SIZE];
	u16 size = (u16)(*(const u16 *)(p));
	u16 unit = 0;

	/* If the value is 0, no memory device is installed in the socket. */
	if (!size)
		return;

	/* If the size is unknown, the field value is FFFFh. */
	if (size == 0xFFFF) {
		dev_warn(&xdev->pdev->dev, "Unknown memory size\n");
		return;
	}

	/* Don't support the Extended Size field. */
	if (size == 0x7FFF) {
		dev_warn(&xdev->pdev->dev,
				"Not supported the Extended Size field\n");
		return;
	}

	/* If the value is specified in kilobyte units. */
	if (size & 0x8000) {
		dev_warn(&xdev->pdev->dev,
				"Memory size specified in kilobyte units\n");
		size &= 0x7FFF;
		unit = 10;
	}

	dinfo->mcdram_size += (size >> unit);
}

static void dmi_decode(struct mic_device *xdev, const struct dmi_header *dm,
		       void *dummy)
{
	switch (dm->type) {
	case 0:		/* BIOS Information */
		dmi_save_entry(xdev, dm, MIC_DMI_BIOS_RELEASE_DATE,
				DMI_BIOS_RELEASE_DATE);
		dmi_save_entry(xdev, dm, MIC_DMI_BIOS_VENDOR,
				DMI_BIOS_VENDOR);
		dmi_save_entry(xdev, dm, MIC_DMI_BIOS_VERSION,
				DMI_BIOS_VERSION);
		break;
	case 1:		/* System Information */
		dmi_save_uuid(xdev, dm, MIC_DMI_SYSTEM_UUID,
				DMI_SYSTEM_UUID);
		dmi_save_entry(xdev, dm, MIC_DMI_SYSTEM_PRODUCT_NAME,
				DMI_SYSTEM_PRODUCT_NAME);
		dmi_save_entry(xdev, dm, MIC_DMI_SYSTEM_SKU_NUMBER,
				DMI_SYSTEM_SKU_NUMBER);
		dmi_save_entry(xdev, dm, MIC_DMI_SYSTEM_SERIAL_NUMBER,
				DMI_SYSTEM_SERIAL_NUMBER);
		dmi_save_entry(xdev, dm, MIC_DMI_SYSTEM_MANUFACTURER,
				DMI_SYSTEM_MANUFACTURER);
		dmi_save_entry(xdev, dm, MIC_DMI_SYSTEM_VERSION,
				DMI_SYSTEM_VERSION);
		break;
	case 2:		/* Baseboard Information */
		dmi_save_brd_type(xdev, dm);
		break;
	case 4:		/* Processor Information */
		dmi_save_processor(xdev, dm);
		dmi_save_entry(xdev, dm, MIC_DMI_PROCESSOR_PART_NUMBER,
				DMI_PROCESSOR_PART_NUMBER);
		dmi_save_entry(xdev, dm, MIC_DMI_PROCESSOR_SERIAL_NUMBER,
				DMI_PROCESSOR_SERIAL_NUMBER);
		dmi_save_entry(xdev, dm, MIC_DMI_PROCESSOR_MANUFACTURER,
				DMI_PROCESSOR_MANUFACTURER);
		dmi_save_entry(xdev, dm, MIC_DMI_PROCESSOR_VERSION,
				DMI_PROCESSOR_VERSION);
		break;
	case 11:	/* OEM Strings */
		dmi_save_oem_strings(xdev, dm);
		break;
	case 17:	/* Memory Device */
		dmi_parse_memsize(xdev, dm);
		break;
	case 148:	/* IFWI Configuration Information */
		dmi_save_entry(xdev, dm, MIC_DMI_BIOS_SMC_VERSION,
			       DMI_IFWI_SMC_VERSION);
		dmi_save_entry(xdev, dm, MIC_DMI_BIOS_ME_VERSION,
			       DMI_IFWI_ME_VERSION);
		dmi_save_entry(xdev, dm, MIC_DMI_MCDRAM_VERSION,
			       DMI_IFWI_MCDRAM_VERSION);
		dmi_save_entry(xdev, dm, MIC_DMI_FAB_VERSION,
			       DMI_IFWI_FAB_VERSION);
		dmi_save_entry(xdev, dm, MIC_DMI_NTB_EEPROM_VERSION,
			       DMI_IFWI_NTB_EEPROM_VERSION);
		break;
	}
}

static void dmi_decode_start(struct mic_device *xdev)
{
	struct mic_dmi_info *dinfo = &xdev->dmi_info;
	dinfo->mcdram_size = 0;
}

static void dmi_decode_stop(struct mic_device *xdev)
{
	struct mic_dmi_info *dinfo = &xdev->dmi_info;
	char *s;

	/* mcdram size */
	s = kmalloc(11, GFP_KERNEL);
	if (!s)
		return;

	scnprintf(s, 11, "%lu", dinfo->mcdram_size);

	dinfo->sysfs_entry[MIC_DMI_MCDRAM_SIZE] = s;

	/* mic driver version */
	s = kmalloc(16, GFP_KERNEL);
	if (!s)
		return;

	scnprintf(s, 16, "%s", MIC_VERSION);

	dinfo->sysfs_entry[MIC_DMI_MIC_DRIVER_VERSION] = s;
}

static int print_filtered(char *buf, size_t len, const char *info)
{
	int c = 0;
	const char *p;

	if (!info)
		return c;

	for (p = info; *p; p++)
		if (isprint(*p))
			c += scnprintf(buf + c, len - c, "%c", *p);
		else
			c += scnprintf(buf + c, len - c, "\\x%02x", *p & 0xff);
	return c;
}

static void dmi_format_ids(struct mic_device *xdev, char *buf, size_t len)
{
	int c = 0;
	struct mic_dmi_info *dinfo = &xdev->dmi_info;

	c += print_filtered(buf + c, len - c,
			    dinfo->sysfs_entry[MIC_DMI_SYSTEM_MANUFACTURER]);
	c += scnprintf(buf + c, len - c, " ");
	c += print_filtered(buf + c, len - c,
			    dinfo->sysfs_entry[MIC_DMI_SYSTEM_PRODUCT_NAME]);
	c += scnprintf(buf + c, len - c, ", BIOS ");
	c += print_filtered(buf + c, len - c,
			    dinfo->sysfs_entry[MIC_DMI_BIOS_VERSION]);
	c += scnprintf(buf + c, len - c, " ");
	c += print_filtered(buf + c, len - c,
			    dinfo->sysfs_entry[MIC_DMI_BIOS_RELEASE_DATE]);
}

/*
 * Check for DMI/SMBIOS headers in the system firmware image.  Any
 * SMBIOS header must start 16 bytes before the DMI header, so take a
 * 32 byte buffer and check for DMI at offset 16 and SMBIOS at offset
 * 0.  If the DMI header is present, set dmi_ver accordingly (SMBIOS
 * takes precedence) and return 0.  Otherwise return 1.
 */
static int dmi_present(struct mic_device *xdev, const u8 *buf)
{
	u16 smbios_ver;
	char dmi_ids_string[128];
	struct mic_dmi_info *dinfo = &xdev->dmi_info;

	if (memcmp(buf, "_SM_", 4) == 0 &&
	    buf[5] < 32 && dmi_checksum(buf, buf[5])) {
		smbios_ver = (buf[6] << 8) + buf[7];

		/* Some BIOS report weird SMBIOS version, fix that up */
		switch (smbios_ver) {
		case 0x021F:
		case 0x0221:
			log_mic_info(xdev->id, "SMBIOS version fixup(2.%d->2.%d)",
				smbios_ver & 0xFF, 3);
			smbios_ver = 0x0203;
			break;
		case 0x0233:
			log_mic_info(xdev->id, "SMBIOS version fixup(2.%d->2.%d)", 51, 6);
			smbios_ver = 0x0206;
			break;
		}
	} else {
		smbios_ver = 0;
	}

	buf += 16;

	if (memcmp(buf, "_DMI_", 5) == 0 && dmi_checksum(buf, 15)) {
		dinfo->dmi_num = (buf[13] << 8) | buf[12];
		dinfo->dmi_len = (buf[7] << 8) | buf[6];
		dinfo->dmi_base = (buf[11] << 24) | (buf[10] << 16) |
				   (buf[9] << 8) | buf[8];

		if (smbios_ver) {
			dinfo->dmi_ver = smbios_ver;
			dev_dbg(&xdev->pdev->dev,
				"SMBIOS %d.%d present.\n", dinfo->dmi_ver >> 8,
				dinfo->dmi_ver & 0xFF);
		} else {
			dinfo->dmi_ver = (buf[14] & 0xF0) << 4 |
					  (buf[14] & 0x0F);
			dev_dbg(&xdev->pdev->dev,
				"Legacy DMI %d.%d present.\n",
				dinfo->dmi_ver >> 8, dinfo->dmi_ver & 0xFF);
		}

		dev_dbg(&xdev->pdev->dev,
			"dmi_ver=0x%x dmi_base=0x%x dmi_len=0x%x dmi_num=0x%x\n",
			dinfo->dmi_ver, dinfo->dmi_base,
			dinfo->dmi_len, dinfo->dmi_num);

		dmi_decode_start(xdev);
		dmi_table(xdev, dmi_decode, NULL);
		dmi_decode_stop(xdev);
		dmi_format_ids(xdev, dmi_ids_string, sizeof(dmi_ids_string));
		dev_dbg(&xdev->pdev->dev, "%s\n", dmi_ids_string);
		return 0;
	}

	return 1;
}

static u32 mic_get_smbios_addr(struct mic_device *xdev)
{
	u32 smbios_addr;

	/* get SMBIOS address */
	smbios_addr = mic_read_spad(xdev, MIC_SPAD_SMBIOS);
	return smbios_addr;
}

int mic_dmi_scan(struct mic_device *xdev)
{
	struct mic_dmi_info *dinfo = &xdev->dmi_info;
	char __iomem *p, *q;
	char buf[32];

	mic_dmi_scan_uninit(xdev);

	mutex_lock(&xdev->mic_mutex);
	p = xdev->aper.va;
	xdev->bootaddr = mic_get_smbios_addr(xdev);
	if (xdev->bootaddr) {
		dev_dbg(&xdev->pdev->dev,
			"Searching 0x%llx for SMBIOS region\n", xdev->bootaddr);
		p += xdev->bootaddr;
	} else {
		goto scan_err;
	}

	/*
	 * Iterate over all possible DMI header addresses q.
	 * Maintain the 32 bytes around q in buf.  On the
	 * first iteration, substitute zero for the
	 * out-of-range bytes so there is no chance of falsely
	 * detecting an SMBIOS header.
	 */
	memset(buf, 0, 16);
	for (q = p; q < p + 0x10000; q += 16) {
		memcpy_fromio(buf + 16, q, 16);
		if (!dmi_present(xdev, buf)) {
			dinfo->dmi_avail = 1;
			mutex_unlock(&xdev->mic_mutex);
			return 0;
		}
		memcpy(buf, buf + 16, 16);
	}

scan_err:
	mutex_unlock(&xdev->mic_mutex);
	dinfo->dmi_avail = 0;
	dev_err(&xdev->pdev->dev, "Can't determine SMBIOS address\n");
	return -ENODEV;
}


/* Un-initialization */
void mic_dmi_scan_uninit(struct mic_device *xdev)
{
	int i;
	struct mic_dmi_info *dinfo = &xdev->dmi_info;

	mutex_lock(&xdev->mic_mutex);
	if (dinfo->dmi_avail) {
		/* free each sysfs_entry[] allocated by ADD_DMI_ATTR macro */
		for (i = 0; i < MIC_DMI_LAST_ENTRY; i++) {
			kfree(dinfo->sysfs_entry[i]);
			dinfo->sysfs_entry[i] = NULL;
		}
	}
	dinfo->dmi_avail = 0;
	mutex_unlock(&xdev->mic_mutex);
}
