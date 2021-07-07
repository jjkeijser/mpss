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
#include <linux/mm.h>
#include "hvc_console.h"
#include <mic/micscif_rb.h>
#include <mic/micvcons.h>
#include <asm/io.h>
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
#include <linux/interrupt.h>
#include <asm/mic/mic_common.h>

#define MIC_COOKIE 	0xc0c0
#define MIC_KNC		1

static long vcons_hdr_addr;
static struct micscif_rb mic_out_buf;
static struct micscif_rb mic_in_buf;

struct vcons_info {
	struct vcons_buf *hdr;
	struct vcons_mic_header *mic_hdr;
	char *vcons_op_buf;
	char *vcons_ip_buf;
};

static struct vcons_info vcons_info;
static int dbg = 0;

/* Receive data from the host (mic i/p buffer) */
static int hvc_mic_get_chars(uint32_t vt, char *buf, int count)
{
	int ret, len, get_count;

	len = micscif_rb_count(&mic_in_buf, count);
	get_count = min(len, count);
	ret = micscif_rb_get_next(&mic_in_buf, buf, get_count);
	if (ret == get_count)
		micscif_rb_update_read_ptr(&mic_in_buf);

	return ret;
}

/* Send data to the host (mic o/p buffer) */
static int hvc_mic_put_chars(uint32_t vt, const char *buf, int count)
{
	int ret;
	int put_count;
	volatile int *host_status =
		(volatile int *)&vcons_info.mic_hdr->host_status;
	
	put_count = min(micscif_rb_space(&mic_out_buf), count);
	if (put_count) {
		ret = micscif_rb_write(&mic_out_buf, (void *)buf, put_count);
		BUG_ON(ret);
		micscif_rb_commit(&mic_out_buf);
	} else if (*host_status != MIC_VCONS_HOST_OPEN)
		return count;
	return put_count;
}


static irqreturn_t hvc_mic_handle_interrupt(int irq, void *dev_id)
{
	struct hvc_struct *hp = (struct hvc_struct *)dev_id;
	if (hvc_poll(hp)) {
		hvc_kick();
		return IRQ_HANDLED;
	}
	return IRQ_NONE;
}

static int hvc_mic_notifier_add_irq(struct hvc_struct *hp, int irq)
{
	int ret = request_irq(get_sbox_irq(HVC_SBOX_INT_IDX),
				hvc_mic_handle_interrupt, IRQF_DISABLED,
				"hvc intr", hp);
	if (ret) {
		printk("Unable to register interrupt\n");
		return ret;
	}
	hp->irq_requested = 1;
	return 0;
}

static void hvc_mic_notifier_del_irq(struct hvc_struct *hp, int irq)
{
	if (hp->irq_requested) {
		free_irq(get_sbox_irq(HVC_SBOX_INT_IDX), hp);
		hp->irq_requested = 0;
	}
}

static void hvc_mic_notifier_hangup_irq(struct hvc_struct *hp, int irq)
{
	hvc_mic_notifier_del_irq(hp, irq);
}

static const struct hv_ops hvc_mic_ops = {
	.get_chars = hvc_mic_get_chars,
	.put_chars = hvc_mic_put_chars,
	.notifier_add = hvc_mic_notifier_add_irq,
	.notifier_del = hvc_mic_notifier_del_irq,
	.notifier_hangup = hvc_mic_notifier_hangup_irq,
};

static void dump_vcons_hdr(struct vcons_buf *hdr)
{
	printk(KERN_ERR "host_magic\t%x\n", readl(&hdr->host_magic));
	printk(KERN_ERR "mic_magic\t%x\n", readl(&hdr->mic_magic));
	printk(KERN_ERR "o_buf_dma_addr\t%x\n", readl(&hdr->o_buf_dma_addr));
	printk(KERN_ERR "o_wr\t%x\n", readl(&hdr->o_wr));
	printk(KERN_ERR "o_size\t%x\n", readl(&hdr->o_size));
	printk(KERN_ERR "i_hdr_addr\t%lx\n", readq(&hdr->i_hdr_addr));
	printk(KERN_ERR "i_buf_addr\t%lx\n", readq(&hdr->i_buf_addr));
	printk(KERN_ERR "i_rd\t%x\n", readl(&hdr->i_rd));
}

static int mic_cons_init(void)
{
	int rc;

	if ((rc = hvc_instantiate(MIC_COOKIE, 0, &hvc_mic_ops)))
		printk(KERN_ERR "error instantiating hvc console\n");

	return rc;
}

static struct hvc_struct *hp;
static int __init hvc_mic_init(void)
{
	struct vcons_buf *hdr = NULL;
	struct vcons_buf tmp_hdr;
	int err = 0;
	char *hvc_buf;
	u8 card_type=0;
	uint16_t host_rb_ver, mic_rb_ver;

#if defined(CONFIG_MK1OM)
	card_type = MIC_KNC;
#endif
	hvc_buf = (char *)get_zeroed_page(GFP_KERNEL);
	if (!hvc_buf) {
		printk(KERN_ERR "unable to allocate vcons buffer\n");
		return -ENOMEM;
	}
	if (card_type == MIC_KNC) {
		vcons_info.vcons_ip_buf = hvc_buf;
		vcons_info.mic_hdr = (struct vcons_mic_header *)kzalloc(sizeof(struct vcons_mic_header), GFP_KERNEL);
		if (!vcons_info.mic_hdr) {
			free_page((unsigned long)hvc_buf);
			printk(KERN_ERR "unable to allocate vcons header\n");
			return -ENOMEM;
		}
	} else {
		vcons_info.vcons_ip_buf = hvc_buf + PAGE_SIZE/2;
		vcons_info.mic_hdr = (struct vcons_mic_header *)hvc_buf;
	}

	vcons_info.hdr = hdr = ioremap_nocache(vcons_hdr_addr, 
			sizeof(struct vcons_buf));
	if (!hdr) {
		printk(KERN_ERR "unable to map vcons header\n");
		err = -ENOMEM;
		goto error;
	}

	if (dbg)
		dump_vcons_hdr(hdr);

	if (readl(&hdr->host_magic) != MIC_HOST_VCONS_READY) {
		printk(KERN_ERR "host not ready, giving up\n");
		err = -ENODEV;
		goto error;
	}

	host_rb_ver = readw(&hdr->host_rb_ver);
	mic_rb_ver = micscif_rb_get_version();
	writew(mic_rb_ver, &hdr->mic_rb_ver);
	if (host_rb_ver != mic_rb_ver) {
		printk(KERN_ERR "Card and host ring buffer versions mismatch.");
		printk(KERN_ERR "Card ver: %d, Host ver: %d \n", mic_rb_ver,
								host_rb_ver);
		writel(MIC_VCONS_RB_VER_ERR, &hdr->mic_magic);
		err = -ENXIO;
		goto error;
	}
	memcpy_fromio(&tmp_hdr, hdr, sizeof(struct vcons_buf));

	if (!(vcons_info.vcons_op_buf = ioremap_nocache(tmp_hdr.o_buf_dma_addr, 
							tmp_hdr.o_size))) {
		printk(KERN_ERR "unable to map vcons output buffer\n");
		err = -ENOMEM;
		goto error;
	}

	tmp_hdr.i_hdr_addr = virt_to_phys(vcons_info.mic_hdr);
	tmp_hdr.i_buf_addr = virt_to_phys(vcons_info.vcons_ip_buf);

	if (card_type == MIC_KNC)
		tmp_hdr.i_size = PAGE_SIZE;
	else
		tmp_hdr.i_size = PAGE_SIZE/2;

	micscif_rb_init(&mic_out_buf, (volatile uint32_t *)&vcons_info.mic_hdr->o_rd,
			(volatile uint32_t *)&hdr->o_wr,
			(volatile uint32_t *)vcons_info.vcons_op_buf,
			tmp_hdr.o_size);

	micscif_rb_init(&mic_in_buf,
			(volatile uint32_t *)&hdr->i_rd,
			(volatile uint32_t *)&vcons_info.mic_hdr->i_wr,
			(volatile uint32_t *)vcons_info.vcons_ip_buf,
			tmp_hdr.i_size);

	mic_cons_init();
	hp = hvc_alloc(MIC_COOKIE, 2, &hvc_mic_ops, 128);

	if (IS_ERR(hp)) {
		printk(KERN_ERR "unable to allocate hvc console\n");
		err = PTR_ERR(hp);
	} else {
		writeq(tmp_hdr.i_hdr_addr, &hdr->i_hdr_addr);
		writeq(tmp_hdr.i_buf_addr, &hdr->i_buf_addr);
		writel(tmp_hdr.i_size, &hdr->i_size);
		writel(MIC_VCONS_READY, &hdr->mic_magic);
		if (dbg)
			dump_vcons_hdr(hdr);

		return 0;
	}
error:
	if (hdr)
		iounmap(hdr);
	if (vcons_info.vcons_op_buf)
		iounmap(vcons_info.vcons_op_buf);
#if defined(CONFIG_MK1OM)
	free_page((unsigned long)vcons_info.vcons_ip_buf);
	kfree(vcons_info.mic_hdr);
#else
	free_page((unsigned long)vcons_info.mic_hdr);
#endif
	return err;
}

static void __exit hvc_mic_exit(void)
{
	char buf[8];
	int ret, len;

	writel(0, &vcons_info.hdr->mic_magic);

	do {
		len = micscif_rb_count(&mic_in_buf, sizeof(buf));
		ret = micscif_rb_get_next(&mic_in_buf, buf,
				min(len, (int)sizeof(buf)));
	} while (ret > 0);

	iounmap(vcons_info.hdr);
	iounmap(vcons_info.vcons_op_buf);
#if defined(CONFIG_MK1OM)
	free_page((unsigned long)vcons_info.vcons_ip_buf);
	kfree(vcons_info.mic_hdr);
#else
	free_page((unsigned long)vcons_info.mic_hdr);
#endif
	if (hp)
		hvc_remove(hp);
}

MODULE_PARM_DESC(vcons_hdr_addr, "mic address of vcons hdr");
module_param(vcons_hdr_addr, long, S_IRUGO);
module_param(dbg, int, S_IRUGO);
MODULE_LICENSE("GPL");
module_init(hvc_mic_init);
module_exit(hvc_mic_exit);

