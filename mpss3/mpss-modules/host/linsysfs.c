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
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/kernel.h>
#include "mic/micveth.h"

#define SBOX_SCR9_VENDORID(x)	((x) & 0xf)
#define SBOX_SCR9_REVISION(x)	(((x) >> 4) & 0xf)
#define SBOX_SCR9_DENSITY(x)	(((x) >> 8) & 0x3)
#define SBOX_SCR9_ECC(x)	(((x) >> 29) & 0x1)

bd_info_t *
dev_to_bdi(struct device *dev)
{
	struct list_head *pos, *tmpq;
	bd_info_t *bdi = NULL;
	list_for_each_safe(pos, tmpq, &mic_data.dd_bdlist) {
		bdi = list_entry(pos, bd_info_t, bi_list);
		if (bdi->bi_sysfsdev == dev)
			break;
	}
	return bdi;
}

/*
 * sysfs entries in lieu of MMIO ioctl
 */

struct device_attribute_sbox {
	struct device_attribute devattr;
	uint32_t offset, mask, shift;
};

uint32_t
bd_sbox_read(bd_info_t *bdi, uint32_t offset)
{
	uint32_t reg_value, ret;
	ret = micpm_get_reference(&bdi->bi_ctx, true);
	if (ret)
		return -EAGAIN;
	reg_value = SBOX_READ(bdi->bi_ctx.mmio.va, offset);
	ret = micpm_put_reference(&bdi->bi_ctx);
	if (ret)
		return -EAGAIN;

	return reg_value;
}

#define DEVICE_ATTR_SBOX(_name, _mode, _offset, _mask, _shift) \
struct device_attribute_sbox sbox_attr_##_name = \
{ __ATTR(_name, _mode, show_sbox_register, NULL), _offset, _mask, _shift }

ssize_t
show_sbox_register(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct device_attribute_sbox *attr_sbox = container_of(attr,
				struct device_attribute_sbox, devattr);
	bd_info_t *bdi = dev_to_bdi(dev);
	return snprintf(buf, PAGE_SIZE, "%x\n",
	(bd_sbox_read(bdi, attr_sbox->offset) >> attr_sbox->shift) & attr_sbox->mask);
}

#ifdef CONFIG_ML1OM
static DEVICE_ATTR_SBOX(corevoltage, S_IRUGO, SBOX_COREVOLT, MASK_COREVOLT, SHIFT_COREVOLT);
static DEVICE_ATTR_SBOX(corefrequency, S_IRUGO, SBOX_COREFREQ, MASK_COREFREQ, SHIFT_COREFREQ);
#endif
static DEVICE_ATTR_SBOX(memoryvoltage, S_IRUGO, SBOX_MEMVOLT, MASK_MEMVOLT, SHIFT_MEMVOLT);
static DEVICE_ATTR_SBOX(memoryfrequency, S_IRUGO, SBOX_MEMORYFREQ, MASK_MEMORYFREQ, SHIFT_MEMORYFREQ);
static DEVICE_ATTR_SBOX(memsize, S_IRUGO, SBOX_SCRATCH0, MASK_MEMSIZE, SHIFT_MEMSIZE);
static DEVICE_ATTR_SBOX(flashversion, S_IRUGO, SBOX_SCRATCH7, MASK_FLASHVERSION, SHIFT_FLASHVERSION);

/* HW Info */
static DEVICE_ATTR_SBOX(substepping_data, S_IRUGO, SBOX_SCRATCH13, MASK_SUBSTEPPING_DATA, SHIFT_SUBSTEPPING_DATA);
static DEVICE_ATTR_SBOX(stepping_data, S_IRUGO, SBOX_SCRATCH13, MASK_STEPPING_DATA, SHIFT_STEPPING_DATA);
static DEVICE_ATTR_SBOX(model, S_IRUGO, SBOX_SCRATCH13, MASK_MODEL, SHIFT_MODEL);
static DEVICE_ATTR_SBOX(family_data, S_IRUGO, SBOX_SCRATCH13, MASK_FAMILY_DATA, SHIFT_FAMILY_DATA);
static DEVICE_ATTR_SBOX(processor, S_IRUGO, SBOX_SCRATCH13, MASK_PROCESSOR, SHIFT_PROCESSOR);
static DEVICE_ATTR_SBOX(platform, S_IRUGO, SBOX_SCRATCH13, MASK_PLATFORM, SHIFT_PLATFORM);
static DEVICE_ATTR_SBOX(extended_model, S_IRUGO, SBOX_SCRATCH13, MASK_EXTENDED_MODEL, SHIFT_EXTENDED_MODEL);
static DEVICE_ATTR_SBOX(extended_family, S_IRUGO, SBOX_SCRATCH13, MASK_EXTENDED_FAMILY, SHIFT_EXTENDED_FAMILY);
/* copy of fuse_configuration_revision [129:120] */
static DEVICE_ATTR_SBOX(fuse_config_rev, S_IRUGO, SBOX_SCRATCH7, MASK_FUSE_CONFIG_REV, SHIFT_FUSE_CONFIG_REV);

static DEVICE_ATTR_SBOX(active_cores, S_IRUGO, SBOX_SCRATCH4, MASK_ACTIVE_CORES, SHIFT_ACTIVE_CORES);
static DEVICE_ATTR_SBOX(fail_safe_offset, S_IRUSR, SBOX_FAIL_SAFE_OFFSET, MASK_FAIL_SAFE, SHIFT_FAIL_SAFE);

ssize_t show_flash_update(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint32_t value, ret;
	bd_info_t *bdi = dev_to_bdi(dev);
	ret = micpm_get_reference(&bdi->bi_ctx, true);
	if (ret)
		return -EAGAIN;
	value =  DBOX_READ(bdi->bi_ctx.mmio.va, DBOX_SWF0X0);
	ret = micpm_put_reference(&bdi->bi_ctx);
	if (ret)
		return -EAGAIN;

	return snprintf(buf, PAGE_SIZE, "%x\n", value);
}

static ssize_t
set_flash_update(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long value;
	int ret;
	bd_info_t *bdi = dev_to_bdi(dev);
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,39)
	ret = kstrtoul(buf, 0, &value);
	if (ret)
		return count;
#else
	value = simple_strtoul(buf, NULL, 10);
#endif
	ret = micpm_get_reference(&bdi->bi_ctx, true);
	if (ret)
		return -EAGAIN;
	DBOX_WRITE((unsigned int)value, bdi->bi_ctx.mmio.va, DBOX_SWF0X0);
	ret = micpm_put_reference(&bdi->bi_ctx);
	if (ret)
		return -EAGAIN;

	return count;

}
static DEVICE_ATTR(flash_update, S_IRUSR | S_IWUSR, show_flash_update, set_flash_update);

ssize_t
show_meminfo(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint32_t value;
	bd_info_t *bdi = dev_to_bdi(dev);
		value =  bd_sbox_read(bdi, SBOX_SCRATCH9);
		return snprintf(buf, PAGE_SIZE, "vendor:%x,revision:%x"
					",density:%x,ecc_enable:%x",
				SBOX_SCR9_VENDORID(value), SBOX_SCR9_REVISION(value),
				SBOX_SCR9_DENSITY(value), SBOX_SCR9_ECC(value));
}
static DEVICE_ATTR(meminfo, S_IRUGO, show_meminfo, NULL);

ssize_t
show_sku(struct device *dev, struct device_attribute *attr, char *buf)
{
	bd_info_t *bdi = dev_to_bdi(dev);
	return snprintf(buf, PAGE_SIZE, "%s\n", bdi->bi_ctx.sku_name);
}
static DEVICE_ATTR(sku, S_IRUGO, show_sku, NULL);
/******************************************************************************/

static ssize_t
show_version(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", BUILD_VERSION);
}
static DEVICE_ATTR(version, S_IRUGO, show_version, NULL);

static ssize_t
show_p2p(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", mic_p2p_enable? "enable" : "disable");
}
static DEVICE_ATTR(peer2peer, S_IRUGO, show_p2p, NULL);

static struct attribute *host_attributes[] = {
	&dev_attr_version.attr,
	&dev_attr_peer2peer.attr,
	NULL
};

struct attribute_group host_attr_group = {
	.attrs = host_attributes
};

static ssize_t
show_family(struct device *dev, struct device_attribute *attr, char *buf)
{
	static const char KNF[] = "Knights Ferry";
	static const char KNC[] = "x100";
	bd_info_t *bdi = dev_to_bdi(dev);
	const char *card = NULL;
	mic_ctx_t *mic_ctx = &bdi->bi_ctx;

	if (mic_ctx->bi_family == FAMILY_ABR)
		card = KNF;
	else
		card = KNC;

	if (card)
		return snprintf(buf, PAGE_SIZE, "%s\n", card);
	else
		return snprintf(buf, PAGE_SIZE, "Unknown\n");
}
static DEVICE_ATTR(family, S_IRUGO, show_family, NULL);

static ssize_t
show_stepping(struct device *dev, struct device_attribute *attr, char *buf)
{
	bd_info_t *bdi = dev_to_bdi(dev);
	char string[3];
	show_stepping_comm(&bdi->bi_ctx,string);
	return snprintf(buf, PAGE_SIZE, "%s\n", string);
}
static DEVICE_ATTR(stepping, S_IRUGO, show_stepping, NULL);

char *micstates[] = {"ready", "booting", "no response", "boot failed",
		     "online", "shutdown", "lost", "resetting", "reset failed", "invalid"};
static ssize_t
show_micstate(struct device *dev, struct device_attribute *attr, char *buf)
{
	bd_info_t *bdi = dev_to_bdi(dev);

	if (bdi->bi_ctx.state >= MIC_INVALID)
		mic_setstate(&bdi->bi_ctx, MIC_INVALID);
	return snprintf(buf, PAGE_SIZE, "%s", micstates[bdi->bi_ctx.state]);
}

static int
match_micstate(const char **buf, const char *string)
{
	size_t len = strlen(string);
	if (!strncmp(*buf, string, len)) {
		*buf += len;
		return true;
	}
	return false;
}

static ssize_t
set_micstate(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	const char *default_mm_image = "/usr/share/mpss/boot/rasmm-kernel.from-eeprom.elf";

	bd_info_t *bdi = dev_to_bdi(dev);
	mic_ctx_t *mic_ctx = &bdi->bi_ctx;
	int mode;
	size_t len;
	char *arg, *arg2 = NULL;
	int err = 0;

	/* parse the new state */
	if (match_micstate(&buf, "boot:linux:")) {
		mode = MODE_LINUX;
	} else if (match_micstate(&buf, "boot:elf:")) {
		mode = MODE_ELF;
	} else if (match_micstate(&buf, "boot:flash:")) {
		mode = MODE_FLASH;
	} else if (sysfs_streq(buf, "reset")) {

		mutex_lock(&mic_ctx->state_lock);
		if (mic_ctx->state == MIC_READY) {
			mutex_unlock(&mic_ctx->state_lock);
			return -EINVAL;
		}

		mutex_unlock(&mic_ctx->state_lock);
		adapter_stop_device(mic_ctx, 1, 0);
		return count;
	} else if (sysfs_streq(buf, "reset:force")) {
		int reattempt = !RESET_REATTEMPT;

		mutex_lock(&mic_ctx->state_lock);
		if (mic_ctx->state == MIC_READY)
			reattempt = RESET_REATTEMPT;

		mutex_unlock(&mic_ctx->state_lock);
		adapter_stop_device(mic_ctx, 1, reattempt);
		return count;
	} else if (sysfs_streq(buf, "shutdown")) {
		adapter_shutdown_device(mic_ctx);
		return count;
	} else {
		return -EINVAL;
	}

	/* we're booting something; a filename follows the colon */
	len = strlen(buf);
	if (buf && buf[0] == '\n') {
		len = 0;
	}
	if (!len && mode == MODE_FLASH) {
		buf = default_mm_image;
		len = strlen(buf);
	}
	if (!(arg = kmalloc(len + 1, GFP_KERNEL)))
		return -ENOMEM;
	memcpy(arg, buf, len + 1);
	if (arg[len - 1] == '\n')
		arg[len - 1] = '\0';

	/* if booting linux, there may be yet another filename */
	if (mode == MODE_LINUX && (arg2 = strchr(arg, ':')))
		*arg2++ = '\0';

	/* atomically change the state */
	mutex_lock(&mic_ctx->state_lock);
	if (mic_ctx->state == MIC_READY) {
		kfree(mic_ctx->image);
		mic_ctx->mode = mode;
		mic_ctx->image = arg;
		mic_ctx->initramfs = arg2;
		mic_setstate(mic_ctx, MIC_BOOT);
		mutex_unlock(&mic_ctx->state_lock);
		printk("mic image: %s\n", mic_ctx->image);
	} else {
		kfree(arg);
		printk(KERN_ERR "Error! Card not in offline/ready state. Cannot change mode\n");
		mutex_unlock(&mic_ctx->state_lock);
		return -EIO;
	}

	/* actually perform the boot */
	if (mode == MODE_LINUX) {
		mic_ctx->card_usage_mode = USAGE_MODE_NORMAL;
		err = boot_linux_uos(mic_ctx, mic_ctx->image, mic_ctx->initramfs);
		if (!err)
			adapter_post_boot_device(mic_ctx);
	} else {
		err = boot_micdev_app(mic_ctx, mic_ctx->image);
	}

	if (!err)
		return count;
	printk("booting failed %d\n", err);
	return err;
}
static DEVICE_ATTR(state, S_IRUGO|S_IWUSR, show_micstate, set_micstate);

char *micmodes[] = {"N/A", "linux", "elf", "flash"};

static ssize_t
show_mode(struct device *dev, struct device_attribute *attr, char *buf)
{
	bd_info_t *bdi = dev_to_bdi(dev);

	if (bdi->bi_ctx.mode > MODE_FLASH)
		bdi->bi_ctx.mode = MODE_NONE;
	return snprintf(buf, PAGE_SIZE, "%s", micmodes[bdi->bi_ctx.mode]);
}
static DEVICE_ATTR(mode, S_IRUGO, show_mode, NULL);

int scif_get_node_status(int node_id);
static char *scif_status_stings[] = {"not present", "initializing", "online",
				     "sleeping", "stopping", "stopped"};
static ssize_t
show_scif_status(struct device *dev, struct device_attribute *attr, char *buf)
{
	bd_info_t *bdi = dev_to_bdi(dev);
	int scif_status;

	scif_status = scif_get_node_status(bdi->bi_ctx.bi_id + 1);
	return snprintf(buf, PAGE_SIZE, "%s\n", scif_status_stings[scif_status]);
}
static DEVICE_ATTR(scif_status, S_IRUGO, show_scif_status, NULL);

static ssize_t
show_image(struct device *dev, struct device_attribute *attr, char *buf)
{
	bd_info_t *bdi = dev_to_bdi(dev);
	return snprintf(buf, PAGE_SIZE, "%s", bdi->bi_ctx.image);
}
static DEVICE_ATTR(image, S_IRUGO, show_image, NULL);

static ssize_t
show_initramfs(struct device *dev, struct device_attribute *attr, char *buf)
{
	bd_info_t *bdi = dev_to_bdi(dev);
	return snprintf(buf, PAGE_SIZE, "%s", bdi->bi_ctx.initramfs);
}
static DEVICE_ATTR(initramfs, S_IRUGO, show_initramfs, NULL);

static ssize_t
show_postcode(struct device *dev, struct device_attribute *attr, char *buf)
{
	bd_info_t *bdi = dev_to_bdi(dev);
	mic_ctx_t *mic_ctx = &bdi->bi_ctx;
	uint32_t postcode;

	if ((micpm_get_reference(mic_ctx, true))) {
		PM_DEBUG("get_reference failed. Node may be lost\n");
		return -EBUSY;
	}
	postcode = mic_getpostcode(mic_ctx);
	if (postcode == 0xffffffff) {
		printk("Invalid Postcode : %c%c\n", postcode & 0xff, (postcode >> 8) & 0xff);
		micpm_put_reference(mic_ctx);
		return -ENXIO;
	}

	if (postcode == 0x0) {
		printk("Postcode : %c%c\n", postcode & 0xff, (postcode >> 8) & 0xff);
		micpm_put_reference(mic_ctx);
		return -EAGAIN;
	}
	micpm_put_reference(mic_ctx);
	return snprintf(buf, PAGE_SIZE, "%c%c", postcode & 0xff, (postcode >> 8) & 0xff);
}
static DEVICE_ATTR(post_code, S_IRUGO, show_postcode, NULL);

static ssize_t
show_boot_count(struct device *dev, struct device_attribute *attr, char *buf)
{
	bd_info_t *bdi = dev_to_bdi(dev);
	mic_ctx_t *mic_ctx = &bdi->bi_ctx;

	return snprintf(buf, PAGE_SIZE, "%d", mic_ctx->boot_count);
}
static DEVICE_ATTR(boot_count, S_IRUGO, show_boot_count, NULL);

static ssize_t
show_crash_count(struct device *dev, struct device_attribute *attr, char *buf)
{
	bd_info_t *bdi = dev_to_bdi(dev);
	mic_ctx_t *mic_ctx = &bdi->bi_ctx;

	return snprintf(buf, PAGE_SIZE, "%d", mic_ctx->crash_count);
}
static DEVICE_ATTR(crash_count, S_IRUGO, show_crash_count, NULL);

static ssize_t
show_cmdline(struct device *dev, struct device_attribute *attr, char *buf)
{
	bd_info_t *bdi = dev_to_bdi(dev);
	mic_ctx_t *mic_ctx = &bdi->bi_ctx;
	char *cmdline = mic_ctx->sysfs_info.cmdline;

	if (cmdline == NULL) {
		return snprintf(buf, PAGE_SIZE, "not set\n");
	} else {
		return snprintf(buf, PAGE_SIZE, "%s\n", cmdline);
	}
	return 0;
}

static ssize_t
set_cmdline(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	bd_info_t *bdi = dev_to_bdi(dev);
	mic_ctx_t *mic_ctx = &bdi->bi_ctx;

	if (mic_ctx->sysfs_info.cmdline != NULL)
		kfree(mic_ctx->sysfs_info.cmdline);

	if ((mic_ctx->sysfs_info.cmdline = kmalloc(count + 100, GFP_ATOMIC)) == NULL)
		return -ENOMEM;
	strcpy(mic_ctx->sysfs_info.cmdline, buf);

	if (mic_ctx->sysfs_info.cmdline[count - 1] == '\n')
		mic_ctx->sysfs_info.cmdline[count - 1] = '\0';

	return count;
}
static DEVICE_ATTR(cmdline, S_IRUGO|S_IWUSR, show_cmdline, set_cmdline);

static ssize_t
show_kernel_cmdline(struct device *dev, struct device_attribute *attr, char *buf)
{
	bd_info_t *bdi = dev_to_bdi(dev);
	mic_ctx_t *mic_ctx = &bdi->bi_ctx;
	char *cmdline = mic_ctx->sysfs_info.kernel_cmdline;

	if ((mic_ctx->state == MIC_READY) || (cmdline == NULL)) {
		return snprintf(buf, PAGE_SIZE, "ready\n");
	} else {
		return snprintf(buf, PAGE_SIZE, "%s\n", cmdline);
	}
}
static DEVICE_ATTR(kernel_cmdline, S_IRUGO, show_kernel_cmdline, NULL);

static ssize_t show_pc3_enabled(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	bd_info_t *bdi = dev_to_bdi(dev);
	mic_ctx_t *mic_ctx = &bdi->bi_ctx;
	return snprintf(buf, PAGE_SIZE, "%d\n", mic_ctx->micpm_ctx.pc3_enabled);
}
static ssize_t
store_pc3_enabled(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int i, ret;
	bd_info_t *bdi = dev_to_bdi(dev);
	mic_ctx_t *mic_ctx = &bdi->bi_ctx;

	if(sscanf(buf, "%d", &i) != 1) {
		ret = -EINVAL;
		goto exit;
	}

	if (i < 0) {
		ret = -EINVAL;
		goto exit;
	}

	ret = micpm_update_pc3(mic_ctx, (i) ? true : false);
	if (ret)
		goto exit;

	pr_debug("pc3_enabled = %d\n", mic_ctx->micpm_ctx.pc3_enabled);
	ret = count;
exit:
	return ret;
}
static DEVICE_ATTR(pc3_enabled, S_IRUGO | S_IWUSR, show_pc3_enabled, store_pc3_enabled);

static ssize_t show_pc6_enabled(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	bd_info_t *bdi = dev_to_bdi(dev);
	mic_ctx_t *mic_ctx = &bdi->bi_ctx;
	return snprintf(buf, PAGE_SIZE, "%d\n", mic_ctx->micpm_ctx.pc6_enabled);
}

static ssize_t
store_pc6_enabled(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int i, ret;
	bd_info_t *bdi = dev_to_bdi(dev);
	mic_ctx_t *mic_ctx = &bdi->bi_ctx;

	if(sscanf(buf, "%d", &i) != 1) {
		ret = -EINVAL;
		goto exit;
	}

	if (i < 0) {
		ret = -EINVAL;
		goto exit;
	}

	ret = micpm_update_pc6(mic_ctx, (i) ? true : false);
	if (ret)
		goto exit;

	pr_debug("pc6_enabled = %d\n", mic_ctx->micpm_ctx.pc6_enabled);
	ret = count;
exit:
	return ret;
}

static DEVICE_ATTR(pc6_enabled, S_IRUGO | S_IWUSR, show_pc6_enabled, store_pc6_enabled);

static ssize_t show_pc6_timeout(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	bd_info_t *bdi = dev_to_bdi(dev);
	mic_ctx_t *mic_ctx = &bdi->bi_ctx;
	return snprintf(buf, PAGE_SIZE, "%u\n", mic_ctx->micpm_ctx.pc6_timeout);
}
static ssize_t
store_pc6_timeout(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int i, ret;
	bd_info_t *bdi = dev_to_bdi(dev);
	mic_ctx_t *mic_ctx = &bdi->bi_ctx;

	if(sscanf(buf, "%d", &i) != 1) {
		ret = -EINVAL;
		goto exit;
	}

	if (i < 0) {
		ret = -EINVAL;
		goto exit;
	}

	if (mic_ctx->micpm_ctx.pc6_timeout != i) {
		mic_ctx->micpm_ctx.pc6_timeout = i;
	}
	pr_debug("pc6 timeout set to %us\n", mic_ctx->micpm_ctx.pc6_timeout);
	ret = count;
exit:
	return ret;
}
static DEVICE_ATTR(pc6_timeout, S_IRUGO | S_IWUSR, show_pc6_timeout, store_pc6_timeout);

static ssize_t show_log_buf_addr(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	bd_info_t *bdi = dev_to_bdi(dev);
	mic_ctx_t *mic_ctx = &bdi->bi_ctx;

	return snprintf(buf, PAGE_SIZE, "%p\n", mic_ctx->log_buf_addr);
}

static ssize_t
store_log_buf_addr(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	uint64_t addr;
	bd_info_t *bdi = dev_to_bdi(dev);
	mic_ctx_t *mic_ctx = &bdi->bi_ctx;

	if (sscanf(buf, "%llx", &addr) != 1) {
		ret = -EINVAL;
		goto exit;
	}

	mic_ctx->log_buf_addr = (void*)addr;
	ret = count;
exit:
	return ret;
}
static DEVICE_ATTR(log_buf_addr, S_IRUGO | S_IWUSR, show_log_buf_addr, store_log_buf_addr);

static ssize_t show_log_buf_len(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	bd_info_t *bdi = dev_to_bdi(dev);
	mic_ctx_t *mic_ctx = &bdi->bi_ctx;

	return snprintf(buf, PAGE_SIZE, "%p\n", mic_ctx->log_buf_len);
}

static ssize_t
store_log_buf_len(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	uint64_t addr;
	bd_info_t *bdi = dev_to_bdi(dev);
	mic_ctx_t *mic_ctx = &bdi->bi_ctx;

	if (sscanf(buf, "%llx", &addr) != 1) {
		ret = -EINVAL;
		goto exit;
	}

	mic_ctx->log_buf_len = (int*)addr;
	ret = count;
exit:
	return ret;
}
static DEVICE_ATTR(log_buf_len, S_IRUGO | S_IWUSR, show_log_buf_len, store_log_buf_len);

union serialnum {
	uint32_t values[3];
	char serial[13];
};

static ssize_t
show_serialnumber(struct device *dev, struct device_attribute *attr, char *buf)
{
	bd_info_t *bdi = dev_to_bdi(dev);
	union serialnum serial;
	uint32_t ret;

	memset(serial.serial, 0, sizeof(serial.serial));
	ret = micpm_get_reference(&bdi->bi_ctx, true);
	if (ret)
		return -EAGAIN;
	serial.values[0] = DBOX_READ(bdi->bi_ctx.mmio.va, DBOX_SWF1X0);
	serial.values[1] = DBOX_READ(bdi->bi_ctx.mmio.va, DBOX_SWF1X1);
	serial.values[2] = DBOX_READ(bdi->bi_ctx.mmio.va, DBOX_SWF1X2);
	ret = micpm_put_reference(&bdi->bi_ctx);
	if (ret)
		return -EAGAIN;
	return snprintf(buf, PAGE_SIZE, "%s", serial.serial);
}
static DEVICE_ATTR(serialnumber, S_IRUGO, show_serialnumber, NULL);

static ssize_t
show_interface_version(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s", LINUX_INTERFACE_VERSION);
}
static DEVICE_ATTR(interface_version, S_IRUGO, show_interface_version, NULL);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,34) || \
	defined(RHEL_RELEASE_CODE)
extern ssize_t show_virtblk_file(struct device *dev, struct device_attribute *attr, char *buf);
extern ssize_t store_virtblk_file(struct device *dev, struct device_attribute *attr,
								  const char *buf, size_t count);
static DEVICE_ATTR(virtblk_file, S_IRUGO | S_IWUSR, show_virtblk_file, store_virtblk_file);
#endif

static struct attribute *bd_attributes[] = {
	&dev_attr_family.attr,
	&dev_attr_stepping.attr,
	&dev_attr_state.attr,
	&dev_attr_mode.attr,
	&dev_attr_image.attr,
	&dev_attr_initramfs.attr,
	&dev_attr_post_code.attr,
	&dev_attr_boot_count.attr,
	&dev_attr_crash_count.attr,
	&dev_attr_cmdline.attr,
	&dev_attr_kernel_cmdline.attr,
	&dev_attr_serialnumber.attr,
	&dev_attr_scif_status.attr,
	&dev_attr_meminfo.attr,
	&dev_attr_pc3_enabled.attr,
	&dev_attr_pc6_enabled.attr,
	&dev_attr_pc6_timeout.attr,
	&dev_attr_flash_update.attr,
	&dev_attr_log_buf_addr.attr,
	&dev_attr_log_buf_len.attr,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,34) || \
	defined(RHEL_RELEASE_CODE)
	&dev_attr_virtblk_file.attr,
#endif
	&dev_attr_sku.attr,
	&dev_attr_interface_version.attr,

#ifdef CONFIG_ML1OM
	&sbox_attr_corevoltage.devattr.attr,
	&sbox_attr_corefrequency.devattr.attr,
#endif
	&sbox_attr_memoryvoltage.devattr.attr,
	&sbox_attr_memoryfrequency.devattr.attr,
	&sbox_attr_memsize.devattr.attr,
	&sbox_attr_flashversion.devattr.attr,
	&sbox_attr_substepping_data.devattr.attr,
	&sbox_attr_stepping_data.devattr.attr,
	&sbox_attr_model.devattr.attr,
	&sbox_attr_family_data.devattr.attr,
	&sbox_attr_processor.devattr.attr,
	&sbox_attr_platform.devattr.attr,
	&sbox_attr_extended_model.devattr.attr,
	&sbox_attr_extended_family.devattr.attr,
	&sbox_attr_fuse_config_rev.devattr.attr,
	&sbox_attr_active_cores.devattr.attr,
	&sbox_attr_fail_safe_offset.devattr.attr,
	NULL
};

struct attribute_group bd_attr_group = {
	.attrs = bd_attributes
};
