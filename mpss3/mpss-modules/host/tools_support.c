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

/* code to send escape calls to uOS; meant to test the ring buffer */

#include "mic_common.h"
#include "mic/mic_dma_lib.h"
#include "mic/mic_dma_api.h"
#include <mic/micscif.h>
#include <mic/micscif_smpt.h>

// constants defined for flash commands for setting PCI aperture
#define RASMM_DEFAULT_OFFSET 0x4000000
#define RASMM_FLASH_SIZE 0x200000
#define MAX_CORE_INDEX 61
#define SKU_MEM_DIVIDE 4
#define SKU_LOW_MEM 0
#define SKU_HIGH_MEM 1
#define FREQ_2P4 0x630
#define FREQ_4P5 0x65A
#define FREQ_5P0 0x664
#define FREQ_5P5 0x66E
#define MASK_MEMFREQ 0xfff
#define SHIFT_MEMFREQ 16

int
mic_unpin_user_pages(struct page **pages, uint32_t nf_pages)
{
	uint32_t j = 0;
	uint32_t status = 0;
	if (pages) {
		for (j = 0; j < nf_pages; j++) {
			if (pages[j]) {
				SetPageDirty(pages[j]);
				page_cache_release(pages[j]);
			}
		}
		kfree(pages);
	}

	return status;
}

int
mic_pin_user_pages (void *data, struct page **pages, uint32_t len, int32_t *nf_pages, int32_t nr_pages)
{

	int32_t status = 0;


	if (!(pages)) {
		printk("%s Failed to allocate memory for pages\n", __func__);
		status = -ENOMEM;
		return status;

	}

	// pin the user pages; use semaphores on linux for doing the same
	down_read(&current->mm->mmap_sem);
	*nf_pages = (int32_t)get_user_pages(current, current->mm, (uint64_t)data,
			  nr_pages, PROT_WRITE, 1, pages, NULL);
	up_read(&current->mm->mmap_sem);

	// compare if the no of final pages is equal to no of requested pages
	if ((*nf_pages) < nr_pages) {
		printk("%s failed to do _get_user_pages\n", __func__);
		status = -EFAULT;
		mic_unpin_user_pages(pages, *nf_pages);
		return status;
	}


	return status;

}

int
send_flash_cmd(mic_ctx_t *mic_ctx, MIC_FLASH_CMD_TYPE type, void *data, uint32_t len)
{
	int32_t status = 0;
	uint8_t *mmio_va = mic_ctx->mmio.va;
	sbox_scratch1_reg_t scratch1reg = {0};
	sbox_scratch2_reg_t scratch2reg = {0};
	uint32_t ret = 0;
	void *src;
	struct timeval t;
	struct flash_stat *statbuf = NULL;
	uint64_t temp;
	uint32_t i = 0;
	struct version_struct *verbuf = NULL;
	int32_t offset = 0;
	uint8_t cmddata = 0;

	scratch1reg.bits.status = FLASH_CMD_INVALID;
	switch (type) {
	case FLASH_CMD_READ:

		/*
		 * image address = the upper 20 bits of the 32-bit of scracth2 register
		 * is card side physical address where the flash image resides
		 * program scratch2 register to notify the image address
		 */
		scratch2reg.bits.image_addr = RASMM_DEFAULT_OFFSET >> 12;
		SBOX_WRITE(scratch2reg.value, mmio_va, SBOX_SCRATCH2);

		/* set command */
		scratch1reg.bits.command = FLASH_CMD_READ;
		SBOX_WRITE(scratch1reg.value, mmio_va, SBOX_SCRATCH1);

		mic_send_bootstrap_intr(mic_ctx);
		break;

	case FLASH_CMD_READ_DATA:

		/*
		 * flash read_data command : set pci aperture to 128MB
		 * read the value of scratch2 in a variable
		 */
		ret = SBOX_READ(mmio_va, SBOX_SCRATCH2);
		scratch2reg.value = ret;

		/*
		 * convert physical to virtual address
		 * image address = the upper 20 bits of the 32-bit KNC side physical
		 * address where the flash image resides
		 */
		offset = scratch2reg.bits.image_addr << 12 ;
		if (len == 0) {
			status = -EINVAL;
			goto exit;
		}

		if (len > (mic_ctx->aper.len - offset)) {
			status = -EINVAL;
			goto exit;
		}
		src = mic_ctx->aper.va + offset;

		temp = copy_to_user(data, src, len);
		if (temp > 0) {
			printk("error while copy to user \n");
			status = -EFAULT;
			goto exit;
		}
		break;

	case FLASH_CMD_ABORT:

		scratch1reg.bits.command = FLASH_CMD_ABORT;
		SBOX_WRITE(scratch1reg.value, mmio_va, SBOX_SCRATCH1);

		mic_send_bootstrap_intr(mic_ctx);
		break;

	case FLASH_CMD_VERSION:

		/*
		 * image address = the upper 20 bits of the 32-bit of scracth2 register
		 * is card side physical address where the flash image resides
		 */
		scratch2reg.bits.image_addr = RASMM_DEFAULT_OFFSET >> 12;
		SBOX_WRITE(scratch2reg.value, mmio_va, SBOX_SCRATCH2);

		/*
		 * flash version command : similar to read_data command.
		 * Instead of get_user_pages(), use kmalloc() as we are allocating
		 * buffer of lesser size
		 */
		scratch1reg.bits.command = FLASH_CMD_VERSION;
		SBOX_WRITE(scratch1reg.value, mmio_va, SBOX_SCRATCH1);

		mic_send_bootstrap_intr(mic_ctx);

		/* poll for completion */
		while(scratch1reg.bits.status != FLASH_CMD_COMPLETED) {
			ret = SBOX_READ(mmio_va, SBOX_SCRATCH1);
			scratch1reg.value = ret;
			msleep(1);
			i++;
			printk("Looping for status (time = %d ms)\n", i);
			if(i > 3000) {
				status = -ETIME;
				goto exit;
			}

		}

		src = mic_ctx->aper.va + RASMM_DEFAULT_OFFSET;

		if (len == 0) {
			status = -EINVAL;
			goto exit;
		}
		verbuf = kmalloc(len, GFP_KERNEL);
		if (!verbuf) {
			status = -ENOMEM;
			goto exit;
		}

		memcpy(verbuf, src, len);

		printk("header verbuf is : %x\n", verbuf->hdr_ver);
		printk("odm verbuf is : %x\n", verbuf->odm_ver);
		printk("uptd time bcd is : %llu\n", verbuf->upd_time_bcd);
		printk("updated verbuf is : %d\n", *((int*)(&verbuf->upd_ver)));
		printk("mfg time bcd is : %llu\n", verbuf->mfg_time_bcd);
		printk("mfg verbuf is : %d\n", *((int*)(&verbuf->mfg_ver)));

		temp = copy_to_user(data, verbuf, len);
		if(temp > 0) {
			printk("error while copy to user \n");
			status = -EFAULT;
			if(verbuf) {
				kfree(verbuf);
			}
			goto exit;
		}

		if(verbuf) {
			kfree(verbuf);
		}

		break;

	case FLASH_CMD_WRITE:

		/* flash write command : pin user pages for the data buffer which contains
		 * the image.
		 * For the write command, we provide the offset for writing.
		 * GTT is set to 64MB and offset = 0.
		 */
		if (len > (mic_ctx->aper.len - RASMM_DEFAULT_OFFSET)) {
			status = -EINVAL;
			goto exit;
		}
		src = mic_ctx->aper.va + RASMM_DEFAULT_OFFSET;
		if (len == 0) {
			status = -EINVAL;
			goto exit;
		}
		temp = copy_from_user(src, data, len);
		if (temp > 0) {
			printk("error while copying from user \n");
			status = -EFAULT;
			goto exit;
		}

		/* image address = the upper 20 bits of the 32-bit KNC side physical
		 * address where the flash image resides
		 */
		scratch2reg.bits.image_addr = RASMM_DEFAULT_OFFSET >> 12;
		SBOX_WRITE(scratch2reg.value, mmio_va, SBOX_SCRATCH2);

		scratch1reg.bits.command = FLASH_CMD_WRITE;
		SBOX_WRITE(scratch1reg.value, mmio_va, SBOX_SCRATCH1);

		mic_send_bootstrap_intr(mic_ctx);
		;

	break;

	case RAS_CMD_CORE_DISABLE:
	case RAS_CMD_CORE_ENABLE:
		if (copy_from_user(&cmddata, data, sizeof(cmddata))) {
			status = -EFAULT;
			goto exit;
		}
		scratch1reg.bits.cmd_data = cmddata;
		if (cmddata > MAX_CORE_INDEX) {
			printk("Parameter given is greater than physical core index\n");
			status = -EINVAL;
			goto exit;
		}

	case RAS_CMD:
	case RAS_CMD_INJECT_REPAIR:
	case RAS_CMD_ECC_DISABLE:
	case RAS_CMD_ECC_ENABLE:
	case RAS_CMD_EXIT:
		do_gettimeofday(&t);
		SBOX_WRITE(t.tv_sec, mmio_va, SBOX_SCRATCH3);
		scratch1reg.bits.command = type;
		SBOX_WRITE(scratch1reg.value, mmio_va, SBOX_SCRATCH1);

		mic_send_bootstrap_intr(mic_ctx);

	break;

	case FLASH_CMD_STATUS:

		/* status command : mmio read of SCRATCH1 register
		 * The percentage completion is only updated on the
		 * Flash Write function as currently implemented.
		 * The other functions are expected to complete almost instantly
		 */
		if(len != sizeof(struct flash_stat)) {
			status = -EINVAL;
			goto exit;
		}
		if (len == 0) {
			status = -EINVAL;
			goto exit;
		}
		statbuf = kmalloc(len, GFP_KERNEL);
		if(!statbuf) {
			status = -ENOMEM;
			goto exit;
		}

		temp = SBOX_READ(mmio_va, SBOX_SCRATCH1);
		scratch1reg.value = (uint32_t)temp;

		statbuf->status = scratch1reg.bits.status;
		statbuf->percent = scratch1reg.bits.percent;
		statbuf->smc_status = scratch1reg.bits.smc_status;
		statbuf->cmd_data = scratch1reg.bits.cmd_data;
		statbuf->mm_debug = scratch1reg.bits.mm_debug;

		temp = copy_to_user(data, statbuf, len);
		if(temp > 0) {
			printk("Error copying data to user buffer\n");
			status = -EFAULT;
			if(statbuf) {
				kfree(statbuf);
			}
			goto exit;
		}

		if(statbuf) {
			kfree(statbuf);
		}

	break;

	default:
		printk(KERN_ERR "Unknown command\n");
		status = -EOPNOTSUPP;
	break;

	}

	exit :
	return status;
}

int get_cardside_mem(mic_ctx_t *mic_ctx, uint64_t start, uint64_t size, void *dest)
{
	int32_t status = 0;
	uint64_t len;
	uint64_t dest_pa;
	struct dma_channel *ch = NULL;
	int flags = 0;
	int poll_cookie;
	int i, next_page;
	int j;
	uint64_t num_pages;
	uint64_t card_pa;
	int32_t nf_pages = 0;
	uint64_t nr_pages = 0;
	struct page **pages = NULL;
	void *pg_virt_add;
	unsigned long t = jiffies;
	int dma_ret = 0;
	card_pa = start;
	len = size;

	if (len % PAGE_SIZE)
		nr_pages = (len >> PAGE_SHIFT) + 1;
	else
		nr_pages = len >> PAGE_SHIFT;

	flags |= DO_DMA_POLLING;
	num_pages = len / PAGE_SIZE;
	next_page = 0;

	pages = kmalloc(nr_pages * sizeof(struct page*), GFP_KERNEL);
	if (!pages)
		return -ENOMEM;
	status = mic_pin_user_pages(dest, pages, (uint32_t)len, &nf_pages, (int32_t)nr_pages);

	if (status)
		goto exit;

	/* allocate_dma_channel should fail in 2 cases : 1. if it doesnt get dma channel
	 * then it times out 2. there is no device present
	 */
	status = micpm_get_reference(mic_ctx, true);
	if (status)
		goto exit;

	while ((dma_ret = allocate_dma_channel(mic_ctx->dma_handle, &ch)) != 0) {
			if (dma_ret == -ENODEV) {
				printk("No device present\n");
				status = -ENODEV;
				goto put_ref;
			}
			msleep(1);
			if (time_after(jiffies,t + NODE_ALIVE_TIMEOUT)) {
				printk("dma channel allocation error\n");
				status = -EBUSY;
				goto put_ref;
			}
	}

	for(j = 0; j < num_pages; j++) {
		i = 0;
		pg_virt_add = lowmem_page_address(pages[j]);
		/* get card side address */
		dest_pa = mic_ctx_map_single(mic_ctx, pg_virt_add, PAGE_SIZE);

		/* do dma and keep polling for completion */
		poll_cookie = do_dma(ch, flags, card_pa + next_page, dest_pa, PAGE_SIZE, NULL);
		pr_debug("Poll cookie %d\n", poll_cookie);
		if (0 > poll_cookie) {
			printk("Error programming the dma descriptor\n");
			status = poll_cookie;
			goto put_ref;
		} else if (-2 == poll_cookie) {
			printk( "Copy was done successfully, check for validity\n");
		} else if(-1 != poll_cookie) {
			while (i < 10000 && 1 != poll_dma_completion(poll_cookie, ch)) {
				i++;
			}
			if (i == 10000) {
				printk("DMA timed out \n");
			} else {
				pr_debug("DMA SUCCESS at %d\n", i);
				/* increment by PAGE_SIZE on DMA SUCCESS to transfer next page */
				next_page = next_page + PAGE_SIZE;
			}
		}
		mic_ctx_unmap_single(mic_ctx, (dma_addr_t)dest_pa, PAGE_SIZE);
	}

put_ref:
	micpm_put_reference(mic_ctx);
exit:
	mic_unpin_user_pages(pages, nf_pages);
	if (ch)
		free_dma_channel(ch);
	return status;
}

/* SKU functions */
void
sku_swap_list(struct list_head *in, struct list_head *out)
{
	struct list_head *pos, *tmp;
	sku_info_t *node;
	list_for_each_safe(pos, tmp, in) {
		node = list_entry(pos, sku_info_t, sku);
		list_del(pos);
		list_add_tail(&node->sku, out);
	}
}

int
sku_create_node(uint32_t fuserev_low,
		uint32_t fuserev_high, uint32_t mem_size,
		uint32_t mem_freq, char *sku_name,
		sku_info_t ** newnode)
{
	sku_info_t *temp;

	temp = kmalloc(sizeof(sku_info_t), GFP_KERNEL);
	if (temp == NULL)
		return -ENOMEM;
	temp->fuserev_low = fuserev_low;
	temp->fuserev_high = fuserev_high;
	temp->memsize = mem_size;
	temp->memfreq = mem_freq;
	strncpy(temp->sku_name, sku_name, SKU_NAME_LEN - 1);
	temp->sku_name[SKU_NAME_LEN - 1] = '\0';
	*newnode = temp;
	return 0;
}

void
sku_destroy_table()
{
	int i;
	sku_info_t *node;
	struct list_head *pos, *tmp;
	for (i = 0; i < MAX_DEV_IDS; i++)
		list_for_each_safe(pos, tmp, &mic_data.sku_table[i]) {
			node = list_entry(pos, sku_info_t, sku);
			list_del(pos);
			kfree(node);
		}
}

int
sku_find(mic_ctx_t *mic_ctx, uint32_t device_id)
{
	int ret = 0;
	uint32_t cnt = 0;
	sku_info_t *match, *newnode = NULL, *skunode;
	struct list_head skulist_memsize_in;
	struct list_head skulist_memfreq_in;
	struct list_head skulist_out;
	uint32_t fuse_rev, memsize, memfreq;
	struct list_head *pos, *tmp;
	const char *invalid = "INVALID SKU";

	/* Use the LSB as index to the array of pointers to the SKU table*/
	device_id = device_id & 0xf;

	if (device_id > MAX_DEV_IDS) {
		strncpy(mic_ctx->sku_name, invalid, SKU_NAME_LEN - 1);
		mic_ctx->sku_name[SKU_NAME_LEN - 1] = '\0';
		return -EINVAL;
	}

	INIT_LIST_HEAD(&skulist_memsize_in);
	INIT_LIST_HEAD(&skulist_memfreq_in);
	INIT_LIST_HEAD(&skulist_out);

	/* Search by fuse_config_rev */
	fuse_rev = SBOX_READ(mic_ctx->mmio.va, SBOX_SCRATCH7);
	fuse_rev = (fuse_rev >> SHIFT_FUSE_CONFIG_REV) & MASK_FUSE_CONFIG_REV;

	list_for_each_safe(pos, tmp, &mic_data.sku_table[device_id]) {
		match = list_entry(pos, sku_info_t, sku);
		if ((match->fuserev_low <= fuse_rev) && (match->fuserev_high >= fuse_rev)) {
			cnt++;
			ret = sku_create_node(match->fuserev_low, match->fuserev_high,
					match->memsize, match->memfreq, match->sku_name, &newnode);
			if (ret) {
				strncpy(mic_ctx->sku_name, invalid, SKU_NAME_LEN - 1);
				mic_ctx->sku_name[SKU_NAME_LEN - 1] = '\0';
				goto cleanup;
			}
			list_add_tail(&newnode->sku, &skulist_out);
		}
	}
	/* If only one node is present, the match has been found */
	if (cnt == 1) {
		strncpy(mic_ctx->sku_name, newnode->sku_name, SKU_NAME_LEN - 1);
		mic_ctx->sku_name[SKU_NAME_LEN - 1] = '\0';
		goto cleanup;
	}

	sku_swap_list(&skulist_out, &skulist_memsize_in);
	/* Search by memsize */
	memsize = SBOX_READ(mic_ctx->mmio.va, SBOX_SCRATCH0);
	memsize = (memsize >> SHIFT_MEMSIZE) & MASK_MEMSIZE;
	memsize = memsize >> 20;
		if (memsize > SKU_MEM_DIVIDE)
			memsize = SKU_HIGH_MEM;
		else
			memsize = SKU_LOW_MEM;

	cnt = 0;
	list_for_each_safe(pos, tmp, &skulist_memsize_in) {
		match = list_entry(pos, sku_info_t, sku);
		/* Use the MSB for comparison */
		/* Assumption - From the latest documentation, a particular
		 * combination of device id and fuse_rev can either have memory
		 * <=4GB (SKU_LOW_MEM) or > 4GB (SKU_HIGH_MEM)
		 */
		if (memsize == match->memsize) {
			cnt++;
			ret = sku_create_node(match->fuserev_low, match->fuserev_high,
					match->memsize, match->memfreq, match->sku_name, &newnode);
			if (ret) {
				strncpy(mic_ctx->sku_name, invalid, SKU_NAME_LEN - 1);
				mic_ctx->sku_name[SKU_NAME_LEN - 1] = '\0';
				goto cleanup;
			}
			list_add_tail(&newnode->sku, &skulist_out);
		}

	}
	list_for_each_safe(pos, tmp, &skulist_memsize_in) {
		skunode = list_entry(pos, sku_info_t, sku);
		list_del(pos);
		kfree(skunode);
	}
	if (cnt == 1) {
		strncpy(mic_ctx->sku_name, newnode->sku_name, SKU_NAME_LEN - 1);
		mic_ctx->sku_name[SKU_NAME_LEN - 1] = '\0';
		goto cleanup;
	}

	sku_swap_list(&skulist_out, &skulist_memfreq_in);
	/* Search by memfreq */
	memfreq = SBOX_READ(mic_ctx->mmio.va, SBOX_SCRATCH9);
	memfreq = (memfreq >> SHIFT_MEMFREQ) & MASK_MEMFREQ;

	cnt = 0;
	list_for_each_safe(pos, tmp, &skulist_memfreq_in) {
		match = list_entry(pos, sku_info_t, sku);
		if (memfreq == match->memfreq) {
			cnt++;
			ret = sku_create_node(match->fuserev_low, match->fuserev_high,
					match->memsize, match->memfreq, match->sku_name, &newnode);
			if (ret) {
				strncpy(mic_ctx->sku_name, invalid, SKU_NAME_LEN - 1);
				mic_ctx->sku_name[SKU_NAME_LEN - 1] = '\0';
				goto cleanup;
			}
			list_add_tail(&newnode->sku, &skulist_out);
		}

	}
	list_for_each_safe(pos, tmp, &skulist_memfreq_in) {
		skunode = list_entry(pos, sku_info_t, sku);
		list_del(pos);
		kfree(skunode);
	}
	if (cnt == 1) {
		strncpy(mic_ctx->sku_name, newnode->sku_name, SKU_NAME_LEN - 1);
		mic_ctx->sku_name[SKU_NAME_LEN - 1] = '\0';
	} else {
		strncpy(mic_ctx->sku_name, invalid, SKU_NAME_LEN - 1);
		mic_ctx->sku_name[SKU_NAME_LEN - 1] = '\0';
	}


cleanup:
	list_for_each_safe(pos, tmp, &skulist_out) {
		skunode = list_entry(pos, sku_info_t, sku);
		list_del(pos);
		kfree(skunode);
	}

	return ret;
}


int
sku_build_table(void)
{
	int i = 0;
	sku_info_t *newnode = NULL;

	for ( i = 0; i < MAX_DEV_IDS; i++)
		INIT_LIST_HEAD(&mic_data.sku_table[i]);

	/*2250*/
	if (sku_create_node(0, 1, SKU_LOW_MEM, FREQ_2P4, "A0PO-SKU1", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[0]);

	if (sku_create_node(0, 1, SKU_HIGH_MEM, FREQ_2P4, "A0PO-SKU1", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[0]);

	if (sku_create_node(2, 3, SKU_LOW_MEM, FREQ_4P5,"ES1-SKU2", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[0]);

	if (sku_create_node(2, 3, SKU_HIGH_MEM, FREQ_4P5, "ES1-SKU2", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[0]);

	if (sku_create_node(4, 49, SKU_HIGH_MEM, FREQ_4P5, "ES1B-SKU2", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[0]);

	if (sku_create_node(50, 100, SKU_HIGH_MEM, FREQ_4P5, "B0PO-SKU2", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[0]);

	if (sku_create_node(101, 150, SKU_HIGH_MEM, FREQ_5P0, "ES2-P1640", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[0]);

	if (sku_create_node(153, 154, SKU_HIGH_MEM, FREQ_5P0, "B1PO-5110P", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[0]);

	if (sku_create_node(151, 152, SKU_HIGH_MEM, FREQ_5P0, "B1PO-P1640/D1650", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[0]);

	if (sku_create_node(156, 156, SKU_HIGH_MEM, FREQ_5P0, "B1PRQ-5110P", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[0]);

	if (sku_create_node(155, 155, SKU_HIGH_MEM, FREQ_5P0, "B1QS-5110P", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[0]);

	if (sku_create_node(157, 157, SKU_HIGH_MEM, FREQ_5P0, "B1PRQ-5110P", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[0]);

	if (sku_create_node(158, 250, SKU_HIGH_MEM, FREQ_5P0, "B1PRQ-5110P/5120D", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[0]);

	if (sku_create_node(251, 253, SKU_HIGH_MEM, FREQ_5P0, "C0-5110P", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[0]);

	if (sku_create_node(254, 255, SKU_HIGH_MEM, FREQ_5P0, "C0QS-5110P", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[0]);

	if (sku_create_node(251, 253, SKU_HIGH_MEM, FREQ_5P5, "C0-5120D", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[0]);

	if (sku_create_node(254, 255, SKU_HIGH_MEM, FREQ_5P5, "C0QS-5120D", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[0]);

	if (sku_create_node(256, 350, SKU_HIGH_MEM, FREQ_5P0, "C0PRQ-5110P/5140P", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[0]);

	if (sku_create_node(256, 350, SKU_HIGH_MEM, FREQ_5P5, "C0PRQ-5120D/5140D", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[0]);

	/*2251*/
	if (sku_create_node(0, 1, SKU_LOW_MEM, FREQ_2P4, "A0PO-SKU2", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[1]);

	if (sku_create_node(0, 1, SKU_HIGH_MEM, FREQ_2P4, "A0PO-SKU2", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[1]);

	/*2252*/
	if (sku_create_node(0, 1, SKU_LOW_MEM, FREQ_2P4, "A0PO-SKU3", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[2]);

	/*2253*/
	if (sku_create_node(0, 1, SKU_LOW_MEM, FREQ_2P4, "A0PO-SKU4", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[3]);

	if (sku_create_node(2, 3, SKU_LOW_MEM, FREQ_2P4, "ES1-SKU5", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[3]);

	if (sku_create_node(4, 49, SKU_LOW_MEM, FREQ_2P4, "ES1B-SKU5", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[3]);

	if (sku_create_node(50, 100, SKU_LOW_MEM, FREQ_4P5, "B0PO-SKU5", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[3]);

	/*2254*/

	/*2255*/
	if (sku_create_node(0, 1, SKU_LOW_MEM, FREQ_2P4, "A0PO-SKUX", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[5]);

	/*2256*/
	if (sku_create_node(0, 1, SKU_LOW_MEM, FREQ_2P4, "A0PO-SKU5", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[6]);

	/*2257*/
	if (sku_create_node(0, 1, SKU_LOW_MEM, FREQ_2P4, "A0PO-SKUZ", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[7]);

	/*2258*/
	if (sku_create_node(2, 3, SKU_LOW_MEM, FREQ_4P5, "ES1-SKU1", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[8]);
	if (sku_create_node(2, 3, SKU_HIGH_MEM, FREQ_4P5, "ES1-SKU1", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[8]);
	if (sku_create_node(4, 49, SKU_HIGH_MEM, FREQ_5P5, "ES1B-SKU1", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[8]);
	if (sku_create_node(50, 100, SKU_HIGH_MEM, FREQ_5P5, "B0PO-SKU1", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[8]);

	/*2259*/
	if (sku_create_node(2, 3, SKU_LOW_MEM, FREQ_4P5, "ES1-SKU3", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[9]);

	if (sku_create_node(2, 3, SKU_HIGH_MEM, FREQ_4P5, "ES1-SKU3", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[9]);

	/*225A*/
	if (sku_create_node(2, 3, SKU_LOW_MEM, FREQ_4P5, "ES1-SKU4", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[10]);

	if (sku_create_node(4, 49, SKU_LOW_MEM, FREQ_5P0, "ES1B-SKU4", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[10]);

	if (sku_create_node(50, 100, SKU_LOW_MEM, FREQ_5P0, "B0PO-SKU4", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[10]);

	if (sku_create_node(101, 150, SKU_LOW_MEM, FREQ_5P0, "ES2-SKU4", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[10]);

	/*225B*/
	if (sku_create_node(4, 49, SKU_HIGH_MEM, FREQ_5P5, "ES1B-SKU3cs", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[11]);

	if (sku_create_node(4, 49, SKU_LOW_MEM, FREQ_5P5, "ES1B-SKU3ncs", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[11]);

	if (sku_create_node(50, 100, SKU_HIGH_MEM, FREQ_5P5, "B0PO-SKU3cs", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[11]);

	if (sku_create_node(50, 100, SKU_LOW_MEM, FREQ_5P5, "B0PO-SKU3ncs", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[11]);

	/*225C*/
	if (sku_create_node(101, 150, SKU_HIGH_MEM, FREQ_5P5, "ES2-P/A/X 1750", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[12]);

	if (sku_create_node(153, 154, SKU_HIGH_MEM, FREQ_5P5, "B1PO-7110 P/A/X", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[12]);

	if (sku_create_node(155, 155, SKU_HIGH_MEM, FREQ_5P5, "B1QS-7110 P/A/X", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[12]);

	if (sku_create_node(151, 152, SKU_HIGH_MEM, FREQ_5P0, "B1PO-P/A 1750", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[12]);

	if (sku_create_node(156, 156, SKU_HIGH_MEM, FREQ_5P5, "B1PRQ-7110 P/A/X", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[12]);

	if (sku_create_node(157, 157, SKU_HIGH_MEM, FREQ_5P5, "B1PRQ-7110 P/A/X", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[12]);

	if (sku_create_node(158, 202, SKU_HIGH_MEM, FREQ_5P5, "B1PRQ-7110 P/X", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[12]);

	if (sku_create_node(203, 250, SKU_HIGH_MEM, FREQ_5P5, "B1PRQ-SE10 P/X", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[12]);

	if (sku_create_node(251, 253, SKU_HIGH_MEM, FREQ_5P5, "C0-7120 P/A/X/D", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[12]);

	if (sku_create_node(254, 255, SKU_HIGH_MEM, FREQ_5P5, "C0QS-7120 P/A/X/D", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[12]);

	if (sku_create_node(256, 350, SKU_HIGH_MEM, FREQ_5P5, "C0PRQ-7120 P/A/X/D", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[12]);

	/*225D*/
	if (sku_create_node(101, 150, SKU_LOW_MEM, FREQ_5P0, "ES2-P1310", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[13]);

	if (sku_create_node(101, 150, SKU_HIGH_MEM, FREQ_5P0, "ES2-A1330", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[13]);

	if (sku_create_node(153, 154, SKU_LOW_MEM, FREQ_5P0, "B1PO-3110P", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[13]);

	if (sku_create_node(153, 154, SKU_HIGH_MEM, FREQ_5P0, "B1PO-3115A", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[13]);

	if (sku_create_node(157, 157, SKU_LOW_MEM, FREQ_5P0, "B1PRQ-3110P", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[13]);

	if (sku_create_node(157, 157, SKU_HIGH_MEM, FREQ_5P0, "B1PRQ-3115A", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[13]);

	if (sku_create_node(156, 156, SKU_LOW_MEM, FREQ_5P0, "B1PRQ-3110P", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[13]);

	if (sku_create_node(156, 156, SKU_HIGH_MEM, FREQ_5P0, "B1PRQ-3115A", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[13]);

	if (sku_create_node(155, 155, SKU_HIGH_MEM, FREQ_5P0, "B1QS-3115A", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[13]);

	if (sku_create_node(155, 155, SKU_LOW_MEM, FREQ_5P0, "B1QS-3110P", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[13]);

	if (sku_create_node(158, 250, SKU_HIGH_MEM, FREQ_5P0, "B1PRQ-3120P", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[13]);

	if (sku_create_node(251, 253, SKU_HIGH_MEM, FREQ_5P0, "C0-3120 P/A", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[13]);

	if (sku_create_node(254, 255, SKU_HIGH_MEM, FREQ_5P0, "C0QS-3120 P/A", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[13]);

	if (sku_create_node(256, 350, SKU_HIGH_MEM, FREQ_5P0, "C0PRQ-3120/3140 P/A", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[13]);

	/*225E*/
	if (sku_create_node(157, 157, SKU_HIGH_MEM, FREQ_5P0, "B1PRQ-31S1P", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[14]);

	if (sku_create_node(158, 250, SKU_HIGH_MEM, FREQ_5P0, "B1PRQ-31S1P", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[14]);

	if (sku_create_node(251, 253, SKU_HIGH_MEM, FREQ_5P0, "C0-31S1P", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[14]);

	if (sku_create_node(254, 255, SKU_HIGH_MEM, FREQ_5P0, "C0QS-31S1P", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[14]);

	if (sku_create_node(256, 350, SKU_HIGH_MEM, FREQ_5P0, "C0PRQ-31S1P", &newnode))
        return -ENOMEM;
	list_add_tail(&newnode->sku, &mic_data.sku_table[14]);

    return 0; // Successed
}
