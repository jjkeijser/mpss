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

/*
 *	fs/proc/vmcore.c Interface for accessing the crash
 * 				 dump from the system's previous life.
 * 	Heavily borrowed from fs/proc/kcore.c
 *	Created by: Hariprasad Nellitheertha (hari@in.ibm.com)
 *	Copyright (C) IBM Corporation, 2004. All rights reserved
 *
 */

#include <linux/version.h>
#include <linux/mm.h>
#include <linux/proc_fs.h>
#include <linux/user.h>
#include <linux/elf.h>
#include <linux/elfcore.h>
#include <linux/slab.h>
#include <linux/highmem.h>
#include <linux/bootmem.h>
#include <linux/init.h>
#include <linux/crash_dump.h>
#include <linux/list.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
#include <linux/kcore.h>
#endif
#include "mic_common.h"

extern struct proc_dir_entry *vmcore_dir;

/* Stores the physical address of elf header of crash image. */
unsigned long long elfcorehdr_addr = 0x50e9000;

/**
 * mic_copy_oldmem_page - copy one page from "oldmem"
 * @pfn: page frame number to be copied
 * @buf: target memory address for the copy; this can be in kernel address
 *	space or user address space (see @userbuf)
 * @csize: number of bytes to copy
 * @offset: offset in bytes into the page (based on pfn) to begin the copy
 * @userbuf: if set, @buf is in user address space, use copy_to_user(),
 *	otherwise @buf is in kernel address space, use memcpy().
 *
 * Copy a page from "oldmem". For this page, there is no pte mapped
 * in the current kernel. We stitch up a pte, similar to kmap_atomic.
 */
ssize_t mic_copy_oldmem_page(mic_ctx_t *mic_ctx,
		unsigned long pfn, char *buf,
		size_t csize, unsigned long offset, int userbuf)
{
	void  *vaddr, *tmp;
	int err;
	struct dma_channel *dma_chan;
	dma_addr_t mic_dst_phys_addr;

	vaddr = mic_ctx->aper.va + (pfn << PAGE_SHIFT);

	if (!csize)
		return 0;
	if (csize == PAGE_SIZE && !offset) {
		if (!(tmp = (void*)__get_free_pages(GFP_KERNEL, get_order(PAGE_SIZE)))) {
			printk(KERN_ERR "%s: tmp buffer allocation failed\n", __func__);
			return -ENOMEM;
		}
		mic_dst_phys_addr = mic_ctx_map_single(mic_ctx, tmp, csize);
		if (mic_map_error(mic_dst_phys_addr)) {
			printk(KERN_ERR "%s: mic_ctx_map_single failed\n", __func__);
			free_pages((unsigned long)tmp, get_order(PAGE_SIZE));
			return -ENOMEM;
		}

		if ((allocate_dma_channel(mic_ctx->dma_handle, &dma_chan))) {
			printk(KERN_ERR "%s: allocate_dma_channel failed\n", __func__);
			mic_ctx_unmap_single(mic_ctx, mic_dst_phys_addr, csize);
			free_pages((unsigned long)tmp, get_order(PAGE_SIZE));
			return -EBUSY;
		}

		err = do_dma(dma_chan,
				0,
				pfn << PAGE_SHIFT,
				mic_dst_phys_addr,
				csize,
				NULL);
		if (err) {
			printk(KERN_ERR "DMA do_dma err %s %d err %d src 0x%lx "
				"dst 0x%llx csize 0x%lx\n", 
				__func__, __LINE__, err, pfn << PAGE_SHIFT, 
				mic_dst_phys_addr, csize);
			free_dma_channel(dma_chan);
			mic_ctx_unmap_single(mic_ctx, mic_dst_phys_addr, csize);
			free_pages((unsigned long)tmp, get_order(PAGE_SIZE));
			return err;
		}
		free_dma_channel(dma_chan);
		err = drain_dma_poll(dma_chan);
		if (err) {
			printk(KERN_ERR "DMA poll err %s %d err %d src 0x%lx i"
				"dst 0x%llx csize 0x%lx\n", 
				__func__, __LINE__, err, pfn << PAGE_SHIFT, 
				mic_dst_phys_addr, csize);
			mic_ctx_unmap_single(mic_ctx, mic_dst_phys_addr, csize);
			free_pages((unsigned long)tmp, get_order(PAGE_SIZE));
			return err;
		}
		if (userbuf) {
			if (copy_to_user(buf, tmp, csize)) {
				mic_ctx_unmap_single(mic_ctx, mic_dst_phys_addr, csize);
				free_pages((unsigned long)tmp, get_order(PAGE_SIZE));
				return -EFAULT;
			}
		} else {
			memcpy(buf, tmp, csize);
		}
		smp_mb();
		mic_ctx_unmap_single(mic_ctx, mic_dst_phys_addr, csize);
		free_pages((unsigned long)tmp, get_order(PAGE_SIZE));
	} else {
		if (userbuf) {
			if (copy_to_user(buf, vaddr + offset, csize))
				return -EFAULT;
		} else
			memcpy_fromio(buf, vaddr + offset, csize);
	}
	return csize;
}

/* Reads a page from the oldmem device from given offset. */
static ssize_t read_from_oldmem(mic_ctx_t *mic_ctx,
				char *buf, size_t count,
				u64 *ppos, int userbuf)
{
	unsigned long pfn, offset;
	size_t nr_bytes;
	ssize_t read = 0, tmp;

	if (!count)
		return 0;

	offset = (unsigned long)(*ppos % PAGE_SIZE);
	pfn = (unsigned long)(*ppos / PAGE_SIZE);

	do {
		if (count > (PAGE_SIZE - offset))
			nr_bytes = PAGE_SIZE - offset;
		else
			nr_bytes = count;

		tmp = mic_copy_oldmem_page(mic_ctx, pfn, buf, nr_bytes, offset, userbuf);
		if (tmp < 0)
			return tmp;
		*ppos += nr_bytes;
		count -= nr_bytes;
		buf += nr_bytes;
		read += nr_bytes;
		++pfn;
		offset = 0;
	} while (count);

	return read;
}

/* Maps vmcore file offset to respective physical address in memroy. */
static u64 map_offset_to_paddr(loff_t offset, struct list_head *vc_list,
					struct vmcore **m_ptr)
{
	struct vmcore *m;
	u64 paddr;

	list_for_each_entry(m, vc_list, list) {
		u64 start, end;
		start = m->offset;
		end = m->offset + m->size - 1;
		if (offset >= start && offset <= end) {
			paddr = m->paddr + offset - start;
			*m_ptr = m;
			return paddr;
		}
	}
	*m_ptr = NULL;
	return 0;
}

/* Read from the ELF header and then the crash dump. On error, negative value is
 * returned otherwise number of bytes read are returned.
 */
static ssize_t read_vmcore(struct file *file, char __user *buffer,
				size_t buflen, loff_t *fpos)
{
	ssize_t acc = 0, tmp;
	size_t tsz;
	u64 start, nr_bytes;
	struct vmcore *curr_m = NULL;
	struct inode *inode = file->f_path.dentry->d_inode;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
	mic_ctx_t *mic_ctx = PDE_DATA(inode);
#else
	struct proc_dir_entry *entry = PDE(inode);
	mic_ctx_t *mic_ctx = entry->data;
#endif

	if (buflen == 0 || *fpos >= mic_ctx->vmcore_size)
		return 0;

	/* trim buflen to not go beyond EOF */
	if (buflen > mic_ctx->vmcore_size - *fpos)
		buflen = mic_ctx->vmcore_size - *fpos;

	/* Read ELF core header */
	if (*fpos < mic_ctx->elfcorebuf_sz) {
		tsz = mic_ctx->elfcorebuf_sz - *fpos;
		if (buflen < tsz)
			tsz = buflen;
		if (copy_to_user(buffer, mic_ctx->elfcorebuf + *fpos, tsz))
			return -EFAULT;
		buflen -= tsz;
		*fpos += tsz;
		buffer += tsz;
		acc += tsz;

		/* leave now if filled buffer already */
		if (buflen == 0)
			return acc;
	}

	start = map_offset_to_paddr(*fpos, &mic_ctx->vmcore_list, &curr_m);
	if (!curr_m)
		return -EINVAL;
	if ((tsz = (PAGE_SIZE - (start & ~PAGE_MASK))) > buflen)
		tsz = buflen;

	/* Calculate left bytes in current memory segment. */
	nr_bytes = (curr_m->size - (start - curr_m->paddr));
	if (tsz > nr_bytes)
		tsz = nr_bytes;

	while (buflen) {
		tmp = read_from_oldmem(mic_ctx,buffer, tsz, &start, 1);
		if (tmp < 0)
			return tmp;
		buflen -= tsz;
		*fpos += tsz;
		buffer += tsz;
		acc += tsz;
		if (start >= (curr_m->paddr + curr_m->size)) {
			if (curr_m->list.next == &mic_ctx->vmcore_list)
				return acc;	/*EOF*/
			curr_m = list_entry(curr_m->list.next,
						struct vmcore, list);
			start = curr_m->paddr;
		}
		if ((tsz = (PAGE_SIZE - (start & ~PAGE_MASK))) > buflen)
			tsz = buflen;
		/* Calculate left bytes in current memory segment. */
		nr_bytes = (curr_m->size - (start - curr_m->paddr));
		if (tsz > nr_bytes)
			tsz = nr_bytes;
	}
	return acc;
}

static const struct file_operations proc_vmcore_operations = {
	.read		= read_vmcore,
};

static struct vmcore* get_new_element(void)
{
	return kzalloc(sizeof(struct vmcore), GFP_KERNEL);
}

static u64 get_vmcore_size_elf64(char *elfptr)
{
	int i;
	u64 size;
	Elf64_Ehdr *ehdr_ptr;
	Elf64_Phdr *phdr_ptr;

	ehdr_ptr = (Elf64_Ehdr *)elfptr;
	phdr_ptr = (Elf64_Phdr*)(elfptr + sizeof(Elf64_Ehdr));
	size = sizeof(Elf64_Ehdr) + ((ehdr_ptr->e_phnum) * sizeof(Elf64_Phdr));
	for (i = 0; i < ehdr_ptr->e_phnum; i++) {
		size += phdr_ptr->p_memsz;
		phdr_ptr++;
	}
	return size;
}

static u64 get_vmcore_size_elf32(char *elfptr)
{
	int i;
	u64 size;
	Elf32_Ehdr *ehdr_ptr;
	Elf32_Phdr *phdr_ptr;

	ehdr_ptr = (Elf32_Ehdr *)elfptr;
	phdr_ptr = (Elf32_Phdr*)(elfptr + sizeof(Elf32_Ehdr));
	size = sizeof(Elf32_Ehdr) + ((ehdr_ptr->e_phnum) * sizeof(Elf32_Phdr));
	for (i = 0; i < ehdr_ptr->e_phnum; i++) {
		size += phdr_ptr->p_memsz;
		phdr_ptr++;
	}
	return size;
}

/* Merges all the PT_NOTE headers into one. */
static int merge_note_headers_elf64(mic_ctx_t *mic_ctx,
				char *elfptr, size_t *elfsz,
				struct list_head *vc_list)
{
	int i, nr_ptnote=0, rc=0;
	char *tmp;
	Elf64_Ehdr *ehdr_ptr;
	Elf64_Phdr phdr, *phdr_ptr;
	Elf64_Nhdr *nhdr_ptr;
	u64 phdr_sz = 0, note_off;

	ehdr_ptr = (Elf64_Ehdr *)elfptr;
	phdr_ptr = (Elf64_Phdr*)(elfptr + sizeof(Elf64_Ehdr));
	for (i = 0; i < ehdr_ptr->e_phnum; i++, phdr_ptr++) {
		int j;
		void *notes_section;
		struct vmcore *new;
		u64 offset, max_sz, sz, real_sz = 0;
		if (phdr_ptr->p_type != PT_NOTE)
			continue;
		nr_ptnote++;
		max_sz = phdr_ptr->p_memsz;
		offset = phdr_ptr->p_offset;
		notes_section = kmalloc(max_sz, GFP_KERNEL);
		if (!notes_section)
			return -ENOMEM;
		rc = read_from_oldmem(mic_ctx, notes_section, max_sz, &offset, 0);
		if (rc < 0) {
			kfree(notes_section);
			return rc;
		}
		nhdr_ptr = notes_section;
		for (j = 0; j < max_sz; j += sz) {
			if (nhdr_ptr->n_namesz == 0)
				break;
			sz = sizeof(Elf64_Nhdr) +
				((nhdr_ptr->n_namesz + 3) & ~3) +
				((nhdr_ptr->n_descsz + 3) & ~3);
			real_sz += sz;
			nhdr_ptr = (Elf64_Nhdr*)((char*)nhdr_ptr + sz);
		}

		/* Add this contiguous chunk of notes section to vmcore list.*/
		new = get_new_element();
		if (!new) {
			kfree(notes_section);
			return -ENOMEM;
		}
		new->paddr = phdr_ptr->p_offset;
		new->size = real_sz;
		list_add_tail(&new->list, vc_list);
		phdr_sz += real_sz;
		kfree(notes_section);
	}

	/* Prepare merged PT_NOTE program header. */
	phdr.p_type    = PT_NOTE;
	phdr.p_flags   = 0;
	note_off = sizeof(Elf64_Ehdr) +
			(ehdr_ptr->e_phnum - nr_ptnote +1) * sizeof(Elf64_Phdr);
	phdr.p_offset  = note_off;
	phdr.p_vaddr   = phdr.p_paddr = 0;
	phdr.p_filesz  = phdr.p_memsz = phdr_sz;
	phdr.p_align   = 0;

	/* Add merged PT_NOTE program header*/
	tmp = elfptr + sizeof(Elf64_Ehdr);
	memcpy(tmp, &phdr, sizeof(phdr));
	tmp += sizeof(phdr);

	/* Remove unwanted PT_NOTE program headers. */
	i = (nr_ptnote - 1) * sizeof(Elf64_Phdr);
	*elfsz = *elfsz - i;
	memmove(tmp, tmp+i, ((*elfsz)-sizeof(Elf64_Ehdr)-sizeof(Elf64_Phdr)));

	/* Modify e_phnum to reflect merged headers. */
	ehdr_ptr->e_phnum = ehdr_ptr->e_phnum - nr_ptnote + 1;

	return 0;
}

/* Merges all the PT_NOTE headers into one. */
static int merge_note_headers_elf32(mic_ctx_t *mic_ctx,
			char *elfptr, size_t *elfsz,
			struct list_head *vc_list)
{
	int i, nr_ptnote=0, rc=0;
	char *tmp;
	Elf32_Ehdr *ehdr_ptr;
	Elf32_Phdr phdr, *phdr_ptr;
	Elf32_Nhdr *nhdr_ptr;
	u64 phdr_sz = 0, note_off;

	ehdr_ptr = (Elf32_Ehdr *)elfptr;
	phdr_ptr = (Elf32_Phdr*)(elfptr + sizeof(Elf32_Ehdr));
	for (i = 0; i < ehdr_ptr->e_phnum; i++, phdr_ptr++) {
		int j;
		void *notes_section;
		struct vmcore *new;
		u64 offset, max_sz, sz, real_sz = 0;
		if (phdr_ptr->p_type != PT_NOTE)
			continue;
		nr_ptnote++;
		max_sz = phdr_ptr->p_memsz;
		offset = phdr_ptr->p_offset;
		notes_section = kmalloc(max_sz, GFP_KERNEL);
		if (!notes_section)
			return -ENOMEM;
		rc = read_from_oldmem(mic_ctx, notes_section, max_sz, &offset, 0);
		if (rc < 0) {
			kfree(notes_section);
			return rc;
		}
		nhdr_ptr = notes_section;
		for (j = 0; j < max_sz; j += sz) {
			if (nhdr_ptr->n_namesz == 0)
				break;
			sz = sizeof(Elf32_Nhdr) +
				((nhdr_ptr->n_namesz + 3) & ~3) +
				((nhdr_ptr->n_descsz + 3) & ~3);
			real_sz += sz;
			nhdr_ptr = (Elf32_Nhdr*)((char*)nhdr_ptr + sz);
		}

		/* Add this contiguous chunk of notes section to vmcore list.*/
		new = get_new_element();
		if (!new) {
			kfree(notes_section);
			return -ENOMEM;
		}
		new->paddr = phdr_ptr->p_offset;
		new->size = real_sz;
		list_add_tail(&new->list, vc_list);
		phdr_sz += real_sz;
		kfree(notes_section);
	}

	/* Prepare merged PT_NOTE program header. */
	phdr.p_type    = PT_NOTE;
	phdr.p_flags   = 0;
	note_off = sizeof(Elf32_Ehdr) +
			(ehdr_ptr->e_phnum - nr_ptnote +1) * sizeof(Elf32_Phdr);
	phdr.p_offset  = note_off;
	phdr.p_vaddr   = phdr.p_paddr = 0;
	phdr.p_filesz  = phdr.p_memsz = phdr_sz;
	phdr.p_align   = 0;

	/* Add merged PT_NOTE program header*/
	tmp = elfptr + sizeof(Elf32_Ehdr);
	memcpy(tmp, &phdr, sizeof(phdr));
	tmp += sizeof(phdr);

	/* Remove unwanted PT_NOTE program headers. */
	i = (nr_ptnote - 1) * sizeof(Elf32_Phdr);
	*elfsz = *elfsz - i;
	memmove(tmp, tmp+i, ((*elfsz)-sizeof(Elf32_Ehdr)-sizeof(Elf32_Phdr)));

	/* Modify e_phnum to reflect merged headers. */
	ehdr_ptr->e_phnum = ehdr_ptr->e_phnum - nr_ptnote + 1;

	return 0;
}

/* Add memory chunks represented by program headers to vmcore list. Also update
 * the new offset fields of exported program headers. */
static int process_ptload_program_headers_elf64(char *elfptr,
						size_t elfsz,
						struct list_head *vc_list)
{
	int i;
	Elf64_Ehdr *ehdr_ptr;
	Elf64_Phdr *phdr_ptr;
	loff_t vmcore_off;
	struct vmcore *new;

	ehdr_ptr = (Elf64_Ehdr *)elfptr;
	phdr_ptr = (Elf64_Phdr*)(elfptr + sizeof(Elf64_Ehdr)); /* PT_NOTE hdr */

	/* First program header is PT_NOTE header. */
	vmcore_off = sizeof(Elf64_Ehdr) +
			(ehdr_ptr->e_phnum) * sizeof(Elf64_Phdr) +
			phdr_ptr->p_memsz; /* Note sections */

	for (i = 0; i < ehdr_ptr->e_phnum; i++, phdr_ptr++) {
		if (phdr_ptr->p_type != PT_LOAD)
			continue;

		/* Add this contiguous chunk of memory to vmcore list.*/
		new = get_new_element();
		if (!new)
			return -ENOMEM;
		new->paddr = phdr_ptr->p_offset;
		new->size = phdr_ptr->p_memsz;
		list_add_tail(&new->list, vc_list);

		/* Update the program header offset. */
		phdr_ptr->p_offset = vmcore_off;
		vmcore_off = vmcore_off + phdr_ptr->p_memsz;
	}
	return 0;
}

static int process_ptload_program_headers_elf32(char *elfptr,
						size_t elfsz,
						struct list_head *vc_list)
{
	int i;
	Elf32_Ehdr *ehdr_ptr;
	Elf32_Phdr *phdr_ptr;
	loff_t vmcore_off;
	struct vmcore *new;

	ehdr_ptr = (Elf32_Ehdr *)elfptr;
	phdr_ptr = (Elf32_Phdr*)(elfptr + sizeof(Elf32_Ehdr)); /* PT_NOTE hdr */

	/* First program header is PT_NOTE header. */
	vmcore_off = sizeof(Elf32_Ehdr) +
			(ehdr_ptr->e_phnum) * sizeof(Elf32_Phdr) +
			phdr_ptr->p_memsz; /* Note sections */

	for (i = 0; i < ehdr_ptr->e_phnum; i++, phdr_ptr++) {
		if (phdr_ptr->p_type != PT_LOAD)
			continue;

		/* Add this contiguous chunk of memory to vmcore list.*/
		new = get_new_element();
		if (!new)
			return -ENOMEM;
		new->paddr = phdr_ptr->p_offset;
		new->size = phdr_ptr->p_memsz;
		list_add_tail(&new->list, vc_list);

		/* Update the program header offset */
		phdr_ptr->p_offset = vmcore_off;
		vmcore_off = vmcore_off + phdr_ptr->p_memsz;
	}
	return 0;
}

/* Sets offset fields of vmcore elements. */
static void set_vmcore_list_offsets_elf64(char *elfptr,
						struct list_head *vc_list)
{
	loff_t vmcore_off;
	Elf64_Ehdr *ehdr_ptr;
	struct vmcore *m;

	ehdr_ptr = (Elf64_Ehdr *)elfptr;

	/* Skip Elf header and program headers. */
	vmcore_off = sizeof(Elf64_Ehdr) +
			(ehdr_ptr->e_phnum) * sizeof(Elf64_Phdr);

	list_for_each_entry(m, vc_list, list) {
		m->offset = vmcore_off;
		vmcore_off += m->size;
	}
}

/* Sets offset fields of vmcore elements. */
static void set_vmcore_list_offsets_elf32(char *elfptr,
						struct list_head *vc_list)
{
	loff_t vmcore_off;
	Elf32_Ehdr *ehdr_ptr;
	struct vmcore *m;

	ehdr_ptr = (Elf32_Ehdr *)elfptr;

	/* Skip Elf header and program headers. */
	vmcore_off = sizeof(Elf32_Ehdr) +
			(ehdr_ptr->e_phnum) * sizeof(Elf32_Phdr);

	list_for_each_entry(m, vc_list, list) {
		m->offset = vmcore_off;
		vmcore_off += m->size;
	}
}

static int parse_crash_elf64_headers(mic_ctx_t *mic_ctx)
{
	int rc=0;
	Elf64_Ehdr ehdr;
	u64 addr;

	addr = elfcorehdr_addr;

	/* Read Elf header */
	rc = read_from_oldmem(mic_ctx, (char*)&ehdr, sizeof(Elf64_Ehdr), &addr, 0);
	if (rc < 0)
		return rc;

	/* Do some basic Verification. */
	if (memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0 ||
		(ehdr.e_type != ET_CORE) ||
#ifdef CONFIG_CRASH_DUMP
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,36))
		!vmcore_elf64_check_arch(&ehdr) ||
#else
		!vmcore_elf_check_arch(&ehdr) ||
#endif
#else
		!elf_check_arch(&ehdr) ||
#endif
		ehdr.e_ident[EI_CLASS] != ELFCLASS64 ||
		ehdr.e_ident[EI_VERSION] != EV_CURRENT ||
		ehdr.e_version != EV_CURRENT ||
		ehdr.e_ehsize != sizeof(Elf64_Ehdr) ||
		ehdr.e_phentsize != sizeof(Elf64_Phdr) ||
		ehdr.e_phnum == 0) {
		printk(KERN_WARNING "Warning: Core image elf header is not"
					"sane\n");
		return -EINVAL;
	}

	WARN_ON(mic_ctx->elfcorebuf);
	/* Read in all elf headers. */
	mic_ctx->elfcorebuf_sz = sizeof(Elf64_Ehdr) + ehdr.e_phnum * sizeof(Elf64_Phdr);
	mic_ctx->elfcorebuf = kmalloc(mic_ctx->elfcorebuf_sz, GFP_KERNEL);
	if (!mic_ctx->elfcorebuf)
		return -ENOMEM;
	addr = elfcorehdr_addr;
	rc = read_from_oldmem(mic_ctx, mic_ctx->elfcorebuf, mic_ctx->elfcorebuf_sz, &addr, 0);
	if (rc < 0) {
		kfree(mic_ctx->elfcorebuf);
		mic_ctx->elfcorebuf = NULL;
		return rc;
	}

	/* Merge all PT_NOTE headers into one. */
	rc = merge_note_headers_elf64(mic_ctx, mic_ctx->elfcorebuf, &mic_ctx->elfcorebuf_sz, &mic_ctx->vmcore_list);
	if (rc) {
		kfree(mic_ctx->elfcorebuf);
		mic_ctx->elfcorebuf = NULL;
		return rc;
	}
	rc = process_ptload_program_headers_elf64(mic_ctx->elfcorebuf, mic_ctx->elfcorebuf_sz,
							&mic_ctx->vmcore_list);
	if (rc) {
		kfree(mic_ctx->elfcorebuf);
		mic_ctx->elfcorebuf = NULL;
		return rc;
	}
	set_vmcore_list_offsets_elf64(mic_ctx->elfcorebuf, &mic_ctx->vmcore_list);
	return 0;
}

static int parse_crash_elf32_headers(mic_ctx_t *mic_ctx)
{
	int rc=0;
	Elf32_Ehdr ehdr;
	u64 addr;

	addr = elfcorehdr_addr;

	/* Read Elf header */
	rc = read_from_oldmem(mic_ctx, (char*)&ehdr, sizeof(Elf32_Ehdr), &addr, 0);
	if (rc < 0)
		return rc;

	/* Do some basic Verification. */
	if (memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0 ||
		(ehdr.e_type != ET_CORE) ||
		!elf_check_arch(&ehdr) ||
		ehdr.e_ident[EI_CLASS] != ELFCLASS32||
		ehdr.e_ident[EI_VERSION] != EV_CURRENT ||
		ehdr.e_version != EV_CURRENT ||
		ehdr.e_ehsize != sizeof(Elf32_Ehdr) ||
		ehdr.e_phentsize != sizeof(Elf32_Phdr) ||
		ehdr.e_phnum == 0) {
		printk(KERN_WARNING "Warning: Core image elf header is not"
					"sane\n");
		return -EINVAL;
	}

	WARN_ON(mic_ctx->elfcorebuf);
	/* Read in all elf headers. */
	mic_ctx->elfcorebuf_sz = sizeof(Elf32_Ehdr) + ehdr.e_phnum * sizeof(Elf32_Phdr);
	mic_ctx->elfcorebuf = kmalloc(mic_ctx->elfcorebuf_sz, GFP_KERNEL);
	if (!mic_ctx->elfcorebuf)
		return -ENOMEM;
	addr = elfcorehdr_addr;
	rc = read_from_oldmem(mic_ctx, mic_ctx->elfcorebuf, mic_ctx->elfcorebuf_sz, &addr, 0);
	if (rc < 0) {
		kfree(mic_ctx->elfcorebuf);
		mic_ctx->elfcorebuf = NULL;
		return rc;
	}

	/* Merge all PT_NOTE headers into one. */
	rc = merge_note_headers_elf32(mic_ctx, mic_ctx->elfcorebuf, &mic_ctx->elfcorebuf_sz, &mic_ctx->vmcore_list);
	if (rc) {
		kfree(mic_ctx->elfcorebuf);
		mic_ctx->elfcorebuf = NULL;
		return rc;
	}
	rc = process_ptload_program_headers_elf32(mic_ctx->elfcorebuf, mic_ctx->elfcorebuf_sz,
								&mic_ctx->vmcore_list);
	if (rc) {
		kfree(mic_ctx->elfcorebuf);
		mic_ctx->elfcorebuf = NULL;
		return rc;
	}
	set_vmcore_list_offsets_elf32(mic_ctx->elfcorebuf, &mic_ctx->vmcore_list);
	return 0;
}

static int parse_crash_elf_headers(mic_ctx_t *mic_ctx)
{
	unsigned char e_ident[EI_NIDENT];
	u64 addr;
	int rc=0;

	addr = elfcorehdr_addr;
	rc = read_from_oldmem(mic_ctx, e_ident, EI_NIDENT, &addr, 0);
	if (rc < 0)
		return rc;
	if (memcmp(e_ident, ELFMAG, SELFMAG) != 0) {
		printk(KERN_WARNING "Warning: Core image elf header"
					" not found\n");
		return -EINVAL;
	}

	if (e_ident[EI_CLASS] == ELFCLASS64) {
		rc = parse_crash_elf64_headers(mic_ctx);
		if (rc)
			return rc;

		/* Determine vmcore size. */
		mic_ctx->vmcore_size = get_vmcore_size_elf64(mic_ctx->elfcorebuf);
	} else if (e_ident[EI_CLASS] == ELFCLASS32) {
		rc = parse_crash_elf32_headers(mic_ctx);
		if (rc)
			return rc;

		/* Determine vmcore size. */
		mic_ctx->vmcore_size = get_vmcore_size_elf32(mic_ctx->elfcorebuf);
	} else {
		printk(KERN_WARNING "Warning: Core image elf header is not"
					" sane\n");
		return -EINVAL;
	}
	return 0;
}

/* Init function for vmcore module. */
int vmcore_create(mic_ctx_t *mic_ctx)
{
	int rc = 0;
        char name[64];
	if (!vmcore_dir) {
		rc = -ENOMEM;
		return rc;
	}
	INIT_LIST_HEAD(&mic_ctx->vmcore_list);
	rc = parse_crash_elf_headers(mic_ctx);
	if (rc) {
		printk(KERN_WARNING "Kdump: vmcore not initialized\n");
		if (mic_ctx->vmcore_dir) {
			remove_proc_entry(name, vmcore_dir);
			mic_ctx->vmcore_dir = NULL;
		}
		return rc;
	}
	snprintf(name, 64, "mic%d", mic_ctx->bi_id);
	if (!mic_ctx->vmcore_dir) {
		mic_ctx->vmcore_dir = proc_create_data(name, S_IRUSR,
			vmcore_dir, &proc_vmcore_operations, mic_ctx);
		if (!mic_ctx->vmcore_dir) {
			printk(KERN_WARNING "Kdump: proc creation for %s failed\n", name);
			rc = -ENOMEM;
			return rc;
		}
	}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
#else
	if (mic_ctx->vmcore_dir)
		mic_ctx->vmcore_dir->size = mic_ctx->vmcore_size;
#endif
	return 0;
}
