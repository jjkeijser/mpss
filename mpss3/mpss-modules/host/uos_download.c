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

/* contains code to download uos on MIC card */

#include "mic_common.h"
#include <mic/ringbuffer.h>
#include "micint.h"
#include <linux/virtio_ring.h>
#include <linux/virtio_blk.h>
#include "mic/mic_virtio.h"
#include <linux/proc_fs.h>
#include "mic/micveth.h"


#define APERTURE_SEGMENT_SIZE   ((1) * 1024 * 1024 * 1024ULL)

#define UOS_RESERVE_SIZE_MIN	((128) * 1024 * 1024)
#define OS_RESERVE_SIZE_MIN	((32) * 1024 * 1024)
#define UOS_RESERVE_SIZE_MAX	(((4) * 1024 * 1024 * 1024ULL) - ((4) * 1024))
#define UOS_RESERVE_PERCENT	50

#define UOS_WATCHDOG_TIMEOUT	5000 // default watchdog timeout in milliseconds

#define PCIE_CLASS_CODE(x)			((x) >> 24 )

/* zombie class code as per the HAS is 0xFF
 * but on KNC, we found it as 0x03
 */
#define ZOMBIE_CLASS_CODE			0x03
#define DISABLE_BAR					0x02
#define RESET_FAILED_F2		12870
#define RESET_FAILED_F4		13382

void ramoops_remove(mic_ctx_t *mic_ctx);

static struct proc_dir_entry *ramoops_dir;
struct proc_dir_entry *vmcore_dir;


static void adapter_dpc(unsigned long dpc);
extern int mic_vhost_blk_probe(bd_info_t *bd_info);
extern void mic_vhost_blk_remove(bd_info_t *bd_info);

/* driver wide global common data */
mic_data_t mic_data;
extern int usagemode_param;
extern bool mic_crash_dump_enabled;
extern bool mic_watchdog_auto_reboot;

static int64_t etc_comp = 0;

static uint64_t
etc_read(uint8_t *mmio_va)
{
	uint32_t low;
	uint32_t hi1,hi2;

	do {
		hi1 = SBOX_READ(mmio_va, SBOX_ELAPSED_TIME_HIGH);
		low = SBOX_READ(mmio_va, SBOX_ELAPSED_TIME_LOW);
		hi2 = SBOX_READ(mmio_va, SBOX_ELAPSED_TIME_HIGH);
	} while(hi1 != hi2);

	return((uint64_t)((((uint64_t)hi1 << 32)  | low) >> 5));
}

static int64_t
calc_deltaf(mic_ctx_t *mic_ctx)
{
	const int64_t ETC_CLK_FREQ = 15625000;
	const uint32_t TIME_DELAY_IN_SEC = 10;
	const int64_t etc_cnt1 = ETC_CLK_FREQ * TIME_DELAY_IN_SEC;
	int64_t etc_cnt2;

	uint64_t cnt1, cnt2;
	int64_t deltaf_in_ppm, deltaf;

	/*
	 * (etc_freq2 / etc_freq1) = (etc_count2 / etc_count1)
	 * etc_freq1 = ETC_CLK_FREQ
	 * => etc_count1 = TIME_DELAY_IN_SEC * ETC_CLK_FREQ
	 * (etc_freq2 / etc_freq1) = (etc_count2 / etc_count1)
	 * etc_freq2 = etc_freq1 * (etc_count2 / etc_count1)
	 * etc_freq2 - etc_freq1 = etc_freq1((etc_count2 / etc_count1) - 1)
	 * deltaf = etc_freq1(etc_count2 - etc_count1)/etc_count1
	 * deltaf_in_ppm = deltaf * 10 ^ 6 / etc_freq1
	 * deltaf_in_ppm = ((etc_count2 - etc_count1) * 10 ^ 6) / etc_count1
	 */
	/* Need to implement the monotonic/irqsave logic for windows */
	unsigned long flags;
	struct timespec ts1, ts2;
	int64_t mono_ns;
	int i = 0;
	do {
		local_irq_save(flags);
		cnt1 = etc_read(mic_ctx->mmio.va);
		getrawmonotonic(&ts1);
		local_irq_restore(flags);
		mdelay(TIME_DELAY_IN_SEC * 1000);
		local_irq_save(flags);
		cnt2 = etc_read(mic_ctx->mmio.va);
		getrawmonotonic(&ts2);
		local_irq_restore(flags);
		etc_cnt2 = cnt2 - cnt1;
		ts2 = timespec_sub(ts2, ts1);
		mono_ns = timespec_to_ns(&ts2);
		/* Recalculate etc_cnt2 based on getrawmonotonic */
		etc_cnt2 = (etc_cnt2 * TIME_DELAY_IN_SEC * 1000 * 1000 * 1000) / mono_ns;
		deltaf = ( ETC_CLK_FREQ * (etc_cnt2 - etc_cnt1)) / etc_cnt1;
		deltaf_in_ppm = (1000 * 1000 * (etc_cnt2 - etc_cnt1)) / etc_cnt1;
		i++;
		/*
		 * HSD #4844900
		 * On some of the systems deltaf_in_ppm is turning out
		 * way higher than expected. The only reasons I can think of
		 * are:
		 * i) mmio traffic cauing variable delays for mmio read
		 * ii) NMIs affecting this code
		 */
	} while (i < 10 && (deltaf_in_ppm > 2700 || deltaf_in_ppm < -2700));

	pr_debug("etc deltaf: %lld\n", deltaf);
	/*
	 * For intel chipsets, Spread Spectrum Clocking (SSC) (in the limit)
	 * is downspread with a frequency of 30hz and an amplitude of 0.5%
	 * which translates to 2500ppm. This is also the ppm observed on KNC + CrownPass
	 * Hence, if ppm > 2500, the code would need to retry to eliminate any chance of error
	 * Added an error margin of 1ppm (etc mmio reads can take really long time)
	 */
	if (deltaf_in_ppm > 2700 || deltaf_in_ppm < -2700) {
		printk(KERN_ERR "ETC timer compensation(%lldppm) is much higher"
					"than expected\n", deltaf_in_ppm);
		/*
		 * HSD #4844900
		 * Clamp etc compensation to 2500ppm
		 */
		if (deltaf_in_ppm > 2700)
			deltaf_in_ppm = 2500;
		else
			deltaf_in_ppm = -2500;
		deltaf = (ETC_CLK_FREQ * deltaf_in_ppm) / (1000 * 1000);
	}
	if (deltaf > 0 && deltaf <= 10)
		deltaf = 0;
	return deltaf;
}

void
calculate_etc_compensation(mic_ctx_t *mic_ctx)
{
	if (mic_ctx->bi_family == FAMILY_KNC) {
		if (!etc_comp)
			etc_comp = calc_deltaf(mic_ctx);
		mic_ctx->etc_comp = etc_comp;
	}
}

/*
  DESCRIPTION:: waits for bootstrap loader is finished
  PARAMETERS::
	[in]void *mmio_va - virtual address to access MMIO registers
  RETURN_VALUE:: 0 if successful, non-zero if failure
*/
int
wait_for_bootstrap(uint8_t *mmio_va)
{
	uint32_t scratch2 = 0;
	int count = 0;
#ifdef MIC_IS_EMULATION
	int wait_time = 0;
#endif

	// Wait until the boot loader is finished
	while (!SCRATCH2_DOWNLOAD_STATUS(scratch2)) {
		msleep(100);
		if (count == 600) {
#ifndef MIC_IS_EMULATION
			printk("Firmware is not responding with ready bit\n");
			return -EIO;
#else
			/* We don't want to be polling too often on the emulator, it is SLOW! */
			pr_debug("Wait for bootstrap: %d min(s) \n", wait_time++);
			count = 0;
#endif
		}

		count++;
		scratch2 = SBOX_READ(mmio_va, SBOX_SCRATCH2);
	}

	return 0;
}

/*
  DESCRIPTION::gets adapter memory size. calculates size based on scratch register 0
  PARAMETERS::
	[in]void *mmio_va - virtual address to access MMIO registers
	[out]uint32_t *adapter_mem_size - adapter memory size
  RETURN_VALUE:: none
*/
void
get_adapter_memsize(uint8_t *mmio_va, uint32_t *adapter_mem_size)
{
	uint32_t memsize = 0;
	uint32_t scratch0 = {0};

	scratch0 = SBOX_READ(mmio_va, SBOX_SCRATCH0);
	memsize = SCRATCH0_MEM_SIZE_KB(scratch0) * ((1) * 1024);

	// Adjust the memory size based on the memory usage
	switch (SCRATCH0_MEM_USAGE(scratch0)) {
	case SCR0_MEM_ALL:
		// Do nothing
		break;

	case SCR0_MEM_HALF:
		memsize /= 2;
		break;

	case SCR0_MEM_THIRD:
		memsize /= 3;
		break;

	case SCR0_MEM_FOURTH:
		memsize /= 4;
		break;

	default:
		// DBG_ASSERT_MSG(false, "Invalid memory usage specified by the bootstrap.\n");
		break;
	}

	*adapter_mem_size = memsize;
}

/*
  DESCRIPTION:: gets uos load offset from scratch register 2
  PARAMETERS::
	[in]void *mmio_va - virtual address to access MMIO registers
	[out]uint32_t *uos_load_offset - offset at which uos will be loaded
  RETURN_VALUE:: none
*/
void
get_uos_loadoffset(uint8_t *mmio_va, uint32_t *uos_load_offset)
{
	uint32_t scratch2 = 0;

	scratch2 = SBOX_READ(mmio_va, SBOX_SCRATCH2);
	*uos_load_offset = SCRATCH2_DOWNLOAD_ADDR(scratch2);
}

/*
  DESCRIPTION:: gets reserved size for uos
  PARAMETERS::
	[out]uint32_t *uos_reserve_size - reserved uos size
  RETURN_VALUE:: none
*/
void
get_uos_reserved_size(uint8_t* mmio_va, uint32_t adapter_memsize, uint32_t *uos_reserve_size)
{
	uint32_t reserve_size = 0;

	// Only calculate if not explicitly specified by the user
	reserve_size = (uint32_t)(adapter_memsize * UOS_RESERVE_PERCENT / 100);

	// Make sure there is at least WINDOWS_RESERVE_SIZE_MIN bytes
	reserve_size = GET_MIN(reserve_size, adapter_memsize - OS_RESERVE_SIZE_MIN);

	// Keep in mind maximum uos reserve size is uint32_t, so we never overflow
	reserve_size = GET_MIN(reserve_size, UOS_RESERVE_SIZE_MAX);
	reserve_size = GET_MAX(reserve_size, UOS_RESERVE_SIZE_MIN);

	// Always align uos reserve size to a page
	reserve_size = (uint32_t)AlignLow(reserve_size, ((4) * 1024));

	*uos_reserve_size = reserve_size;
}

/*
  DESCRIPTION:: gets APIC ID from scratch register 2
  PARAMETERS::
	[in]void *mmio_va - virtual address to access MMIO registers
	[out]uint32_t *apic_id - apic id
  RETURN_VALUE:: none
*/
void
get_apic_id(uint8_t *mmio_va, uint32_t *apic_id)
{
	uint32_t scratch2 = 0;

	scratch2 = SBOX_READ(mmio_va, SBOX_SCRATCH2);
	*apic_id = SCRATCH2_APIC_ID(scratch2);
}

/*
  DESCRIPTION::program the PCI aperture as a contiguous window. (only supports upto 4GB memory)
  PARAMETERS::
	[in]mic_ctx_t *mic_ctx - mic ctx
	[in]int gtt_index - beginning gtt entry index
	[in]uint64_t phy_addr - physical address for PCI aperture
	[in]uint32_t num_bytes - size of PCI aperture
  RETURN_VALUE:: None
  */
void
set_pci_aperture(mic_ctx_t *mic_ctx, uint32_t gtt_index, uint64_t phy_addr, uint32_t num_bytes)
{
	uint32_t num_pages;
	uint32_t gtt_entry;
	uint32_t i;

	num_pages = ALIGN(num_bytes, PAGE_SIZE) >> PAGE_SHIFT;

	for (i = 0; i < num_pages; i++) {

		gtt_entry = ((uint32_t)(phy_addr >> PAGE_SHIFT) + i) << 1 | 0x1u;
		GTT_WRITE(gtt_entry, mic_ctx->mmio.va, (gtt_index + i)*sizeof(gtt_entry));
	}

	// XPU_RACE_CONDITION:
	// Writing GttTlbFlushReg DOES NOT flush all write transactions from SBOX to GDDR
	//	because GttTlbFlushReg is an SBOX register and transaction terminates in SBOX
	// MMIO write must use MIC ringbus to be serializing.
	// Writing GTT itself DOES serialize: GTT is in MMIO space, and write goes to the ringbus
	// MemoryBarrier makes sure all writes make it to GDDR before tlbFlush write
	smp_mb(); // FIXME: only needs SFENCE

	// write any value to cause a flush
	SBOX_WRITE(1, mic_ctx->mmio.va, SBOX_TLB_FLUSH);
}

/*
 DESCRIPTION:: Programs a scratch register that the bootstrap reads to determine
			   how large is uOS image.
 PARAMETERS::
   [in]void *mmio_va - virtual address to mmio register,
   [in]uint32_t uos_size - size of uos image
 RETURN_VALUE:: none
*/
void
set_uos_size(uint8_t *mmio_va, uint32_t uos_size)
{
	uint32_t scratch5;

	scratch5 = uos_size;
	// XPU_RACE_CONDITION: write to MMIO space is uncached and flushes WC buffers
	SBOX_WRITE(scratch5, mmio_va, SBOX_SCRATCH5);
}

/*
 DESCRIPTION:: Programs a scratch register that the uOS reads to determine how
			 much memory to reserve.
 PARAMETERS::
   [in]void *mmio_va - virtual address to mmio register,
   [in]uint32_t uos_reserved_size - size of memory to be reserved by uos.
 RETURN_VALUE:: none
*/
void
set_uos_reserved_size(uint8_t *mmio_va, uint32_t uos_reserved_size)
{
	uint32_t scratch3;

	scratch3 = uos_reserved_size;
	// XPU_RACE_CONDITION: write to MMIO space is uncached and flushes WC buffers
	SBOX_WRITE(scratch3, mmio_va, SBOX_SCRATCH3);
}

/*
 DESCRIPTION:: .
 PARAMETERS::
   [in]uint32_t device_id - device ID,
 RETURN_VALUE:: family type
*/
product_family_t
get_product_family(uint32_t device_id)
{
	product_family_t product_family;

	switch (device_id) {
	case PCI_DEVICE_ABR_2249:
	case PCI_DEVICE_ABR_224a:
		product_family = FAMILY_ABR;
		break;

	case PCI_DEVICE_KNC_2250:
	case PCI_DEVICE_KNC_2251:
	case PCI_DEVICE_KNC_2252:
	case PCI_DEVICE_KNC_2253:
	case PCI_DEVICE_KNC_2254:
	case PCI_DEVICE_KNC_2255:
	case PCI_DEVICE_KNC_2256:
	case PCI_DEVICE_KNC_2257:
	case PCI_DEVICE_KNC_2258:
	case PCI_DEVICE_KNC_2259:
	case PCI_DEVICE_KNC_225a:
	case PCI_DEVICE_KNC_225b:
	case PCI_DEVICE_KNC_225c:
	case PCI_DEVICE_KNC_225d:
	case PCI_DEVICE_KNC_225e:
		product_family = FAMILY_KNC;
		break;

	default:
		pr_debug( "Invalid/Unknown device ID %d\r\n", device_id);
		product_family = FAMILY_UNKNOWN;
		break;
	}

	return product_family;
}

/*
  DESCRIPTION:: loads uos image at given path into gddr
  PARAMETERS::
    [in]mic_ctx_t *mic_ctx - mic context
    [in]imgname - file path for uos file to be loaded
    [out]uos_size - size of uos image
 */
int
load_uos_into_gddr(mic_ctx_t *mic_ctx, char *imgname, uint32_t* uos_size, uint64_t *uos_cmd_offset)
{
	void *aperture_va;
	uint8_t *mmio_va;
	uint32_t apic_id = 0;
	uint32_t uos_load_offset = 0;
	uint32_t adapter_memsize = 0;
	int status = 0;

	aperture_va = mic_ctx->aper.va;
	mmio_va = mic_ctx->mmio.va;

	if (mic_ctx->state != MIC_BOOT) {
		printk("Not in booting state\n");
		return -EPERM;
	}

	status = mic_get_file_size(imgname, uos_size);

	if (status) {
		mic_ctx->state = MIC_BOOTFAIL;
		printk("Linux image not found at %s , status returned %d\n", imgname, status);
		return status;
	}

	get_uos_loadoffset(mmio_va, &uos_load_offset);
	// Determine the uOS reserve size after we have the m_pXpu interface
	get_adapter_memsize(mmio_va, &adapter_memsize);

	get_apic_id(mmio_va, &apic_id);
	// store apic_id in adapter context for later use
	mic_ctx->apic_id = apic_id;

	if (mic_ctx->bi_family == FAMILY_ABR){
		// Program the PCI aperture as a contiguous window
		// Need an extra page to provide enough buffer space for command line arguments.
		set_pci_aperture(mic_ctx, 0, uos_load_offset, *uos_size + PAGE_SIZE);
		uos_load_offset = 0;
	}

	// transfer uOs image file to gddr
	status = mic_load_file(imgname, ((uint8_t*)aperture_va) + uos_load_offset, *uos_size);

	// for the emulator we want to skip "downloading" the file
	*uos_cmd_offset = (uint64_t)uos_load_offset + *uos_size;

	// This only applies to KNF bootstrap, it is NOT needed for KNC
	if (mic_ctx->bi_family == FAMILY_ABR) {
		// clear UOS load offset register after uOS was uploaded
		SBOX_WRITE(0, mmio_va, SBOX_SCRATCH2);
		SBOX_READ(mmio_va, SBOX_SCRATCH2);
	}

	return status;
}

/*
  DESCRIPTION:: loads uos initramfs image at given path into gddr for KNC.
  PARAMETERS::
    [in]mic_ctx_t *mic_ctx - mic context
    [in]initramfsname - file path for uos initramfs file to be loaded
 */
int
load_initramfs(mic_ctx_t *mic_ctx, char *initramfsname, uint32_t *initramfs_image, uint32_t *initramfs_size)
{
	uint8_t *aperture_va;
	uint8_t *mmio_va;
	uint32_t apic_id = 0;
	uint32_t uos_load_offset = 0;
	uint32_t file_load_offset = 0;
	uint32_t adapter_memsize = 0;
	uint32_t file_size = 0;
	int status = 0;
	uint32_t *ramfs_addr_ptr;

	aperture_va = mic_ctx->aper.va;
	mmio_va = mic_ctx->mmio.va;

	if (mic_ctx->state != MIC_BOOT) {
		printk("Not in booting state\n");
		return -EPERM;
	}

	status = mic_get_file_size(initramfsname, &file_size);

	if (status) {
		mic_ctx->state = MIC_BOOTFAIL;
		printk("Init ram disk image not found at %s , status returned %d\n", initramfsname, status);
		return status;
	}

	get_uos_loadoffset(mmio_va, &uos_load_offset);
	file_load_offset = uos_load_offset << 1; /* Place initramfs higher than kernel; 128MB is ok */

	*initramfs_size = file_size;
	*initramfs_image = file_load_offset;

	// Determine the uOS reserve size after we have the m_pXpu interface
	get_adapter_memsize(mmio_va, &adapter_memsize);
	get_apic_id(mmio_va, &apic_id);

	// store apic_id in adapter context for later use
	mic_ctx->apic_id = apic_id;

	// transfer uOs image file to gddr
	status = mic_load_file(initramfsname, aperture_va + file_load_offset, file_size);

	// write the initramfs load address and size to the fields in the kernel header
	ramfs_addr_ptr = (uint32_t *)(aperture_va + uos_load_offset + 0x218);
	*ramfs_addr_ptr = file_load_offset;
	ramfs_addr_ptr = (uint32_t *)(aperture_va + uos_load_offset + 0x21c);
	*ramfs_addr_ptr = *initramfs_size;

	return status;
}

struct tmpqp {
	uint64_t ep;
	uint64_t magic;
};

int
load_command_line(mic_ctx_t *mic_ctx, uint64_t uos_cmd_offset)
{
	void *cmd_line_va = mic_ctx->aper.va + uos_cmd_offset;
	uint32_t cmdlen = 0;
	char *buf = NULL;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,34)) || defined(RHEL_RELEASE_CODE)
	struct board_info *bi = mic_ctx->bd_info;
#endif

#ifdef USE_VCONSOLE
	micvcons_t *vcons = &mic_ctx->bi_vcons;
	dma_addr_t vc_hdr_dma_addr = 0;
#endif

	/*
	 * mic_ctx->boot_mem will also be set in IOCTL to boot the card in restricted memory
	 * FIXME::This code is added to keep the backward compatibility with IOCTLs
	 */
	if (mic_ctx->bi_family == FAMILY_KNC)
		if (mic_ctx->boot_mem == 0 || mic_ctx->boot_mem > mic_ctx->aper.len >> 20)
			mic_ctx->boot_mem = (uint32_t)(mic_ctx->aper.len >> 20);
	if (!(buf = kzalloc(MIC_CMDLINE_BUFSIZE, GFP_KERNEL))) {
		printk(KERN_ERR "failed to allocate %d bytes for uOS command line\n", 
			    MIC_CMDLINE_BUFSIZE);
		return -ENOMEM;
	}

	cmdlen = snprintf(buf, MIC_CMDLINE_BUFSIZE, "card=%d vnet=%s scif_id=%d scif_addr=0x%llx",
				mic_ctx->bi_id, mic_vnet_modes[mic_vnet_mode],
				mic_ctx->bi_id + 1, mic_ctx->bi_scif.si_pa);

	if (mic_vnet_mode == VNET_MODE_DMA) {
		struct micvnet_info *vnet_info = mic_ctx->bi_vethinfo;
		cmdlen += snprintf(buf + cmdlen, MIC_CMDLINE_BUFSIZE - cmdlen,
			" vnet_addr=0x%llx", vnet_info->vi_rp_phys);
	}

#ifdef USE_VCONSOLE
	if (vcons->dc_enabled)
		vc_hdr_dma_addr = vcons->dc_hdr_dma_addr;

	cmdlen += snprintf(buf + cmdlen, MIC_CMDLINE_BUFSIZE - cmdlen,
		" vcons_hdr_addr=0x%llx", vc_hdr_dma_addr);
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,34)) || defined(RHEL_RELEASE_CODE)
	cmdlen += snprintf(buf + cmdlen, MIC_CMDLINE_BUFSIZE - cmdlen, " virtio_addr=0x%llx",
				mic_ctx_map_single(mic_ctx, bi->bi_virtio, sizeof(struct vb_shared)));
#endif

	if (mic_ctx->boot_mem)
		cmdlen += snprintf(buf + cmdlen, MIC_CMDLINE_BUFSIZE - cmdlen,
					 " mem=%dM", mic_ctx->boot_mem);
	mic_ctx->boot_mem = 0;

	if (mic_ctx->ramoops_size)
		cmdlen += snprintf(buf + cmdlen, MIC_CMDLINE_BUFSIZE - cmdlen,
			" ramoops_size=%d ramoops_addr=0x%llx",
			mic_ctx->ramoops_size, mic_ctx->ramoops_pa[0]);

	cmdlen += snprintf(buf + cmdlen, MIC_CMDLINE_BUFSIZE - cmdlen,
		" p2p=%d p2p_proxy=%d", mic_p2p_enable, mic_p2p_proxy_enable);

	if (mic_ctx->bi_family == FAMILY_KNC)
		cmdlen += snprintf(buf + cmdlen, MIC_CMDLINE_BUFSIZE - cmdlen,
			" etc_comp=%lld", mic_ctx->etc_comp);

	cmdlen += snprintf(buf + cmdlen, MIC_CMDLINE_BUFSIZE - cmdlen,
		" reg_cache=%d", mic_reg_cache_enable);
	cmdlen += snprintf(buf + cmdlen, MIC_CMDLINE_BUFSIZE - cmdlen,
		" ulimit=%d", mic_ulimit_check);
	cmdlen += snprintf(buf + cmdlen, MIC_CMDLINE_BUFSIZE - cmdlen,
		" huge_page=%d", mic_huge_page_enable);
	if (mic_crash_dump_enabled)
		cmdlen += snprintf(buf + cmdlen, MIC_CMDLINE_BUFSIZE - cmdlen,
			" crashkernel=1M@80M");
	/*
	 * Limitations in the Intel Jaketown and Ivytown platforms require SCIF
	 * to proxy P2P DMA read transfers in order to convert them into a P2P DMA
	 * write for better performance. The SCIF module on MIC needs the
	 * numa node the MIC is connected to on the host to make decisions
	 * about whether to proxy P2P DMA reads or not based on whether the two MIC
	 * devices are connected to the same QPI/socket/numa node or not.
	 * The assumption here is that a socket/QPI will have a unique
	 * numa node number.
	 */
	pr_debug("CPU family = %d, CPU model = %d\n", boot_cpu_data.x86, boot_cpu_data.x86_model);

	if (mic_p2p_proxy_enable && (boot_cpu_data.x86==6) &&
		(boot_cpu_data.x86_model == 45 || boot_cpu_data.x86_model == 62)) {
		int numa_node = dev_to_node(&mic_ctx->bi_pdev->dev);
		if (-1 != numa_node) {
			if (boot_cpu_data.x86_model == 45)
				ms_info.mi_proxy_dma_threshold = SCIF_PROXY_DMA_THRESHOLD_JKT;
			if (boot_cpu_data.x86_model == 62)
				ms_info.mi_proxy_dma_threshold = SCIF_PROXY_DMA_THRESHOLD_IVT;
			cmdlen += snprintf(buf + cmdlen, MIC_CMDLINE_BUFSIZE - cmdlen,
				" numa_node=%d", numa_node);
			cmdlen += snprintf(buf + cmdlen, MIC_CMDLINE_BUFSIZE - cmdlen,
				" p2p_proxy_thresh=%lld", ms_info.mi_proxy_dma_threshold);
		}
	}

	if (mic_ctx->sysfs_info.cmdline != NULL)
		snprintf(buf + cmdlen, MIC_CMDLINE_BUFSIZE - cmdlen,
			       " %s", mic_ctx->sysfs_info.cmdline);
	else
		snprintf(buf + cmdlen, MIC_CMDLINE_BUFSIZE - cmdlen,
			       " hostname=mic%d ipaddr=171.31.%d.2 quiet console=ttyS0,115200n8",
			       mic_ctx->bi_id, mic_ctx->bi_id + 1);

	memcpy_toio(cmd_line_va, buf, strlen(buf) + 1);

	if (mic_ctx->sysfs_info.kernel_cmdline != NULL)
		kfree(mic_ctx->sysfs_info.kernel_cmdline);

	if ((mic_ctx->sysfs_info.kernel_cmdline = kmalloc(strlen(buf) + 1, GFP_KERNEL)) != NULL)
		strcpy(mic_ctx->sysfs_info.kernel_cmdline, buf);

	kfree(buf);
	return 0;
}

/*
  DESCRIPTION:: method responsible for programming scratch register with uos image size
		and notifying bootstrap to start booting uos
  PARAMETERS::
    [in]mic_ctx_t *mic_ctx - mic context
    [in]uint32_t uos_size - size of uos image
 */
int
notify_uosboot(mic_ctx_t *mic_ctx, uint32_t uos_size)
{
	int status = 0;
	uint32_t adapter_memsize = 0;
	uint32_t uos_reserved_size = 0;
	uint8_t* mmio_va = mic_ctx->mmio.va;

	// Program the register with uOS image size for bootstrap
	set_uos_size(mmio_va, uos_size);

	get_adapter_memsize(mmio_va, &adapter_memsize);

	// Program the register to inform the uOS of how much space to reserve
	get_uos_reserved_size(mmio_va, adapter_memsize, &uos_reserved_size);
	set_uos_reserved_size(mmio_va, uos_reserved_size);

	mic_send_bootstrap_intr(mic_ctx);

	return status;
}

/*
 DESCRIPTION :: boots Linux OS on the card
 PARAMETERS ::
  [in]mic_ctx_t *mic_ctx - mic context
  [in]char *imgname - file path for uos image to be loaded on the card
  RETURN_VALUE:: 0 if successful, non-zero if failure
*/
int
boot_linux_uos(mic_ctx_t *mic_ctx, char *imgname, char *initramfsname)
{
	int status = 0;
	uint32_t uos_size = 0;
	uint64_t uos_cmd_offset = 0;
	uint32_t initramfs_image = 0;
	uint32_t initramfs_size = 0;

	printk("MIC %d Booting\n", mic_ctx->bi_id);

	if (mic_ctx->state != MIC_BOOT) {
		printk(KERN_ERR "MIC %d is not in offline mode\n", mic_ctx->bi_id);
		return -EPERM;
	}

	//loads uos image at given path into gddr
	if ((status = load_uos_into_gddr(mic_ctx, imgname, &uos_size, &uos_cmd_offset)) != 0) {
		printk("Cannot load uos in gddr\n");
		return status;
	}

	if (initramfsname && (status = load_initramfs(mic_ctx, initramfsname, &initramfs_image, &initramfs_size)) != 0) {
		printk("Cannot load initramfs in gddr\n");
		return status;
	}

	status = load_command_line(mic_ctx, uos_cmd_offset);

	//program scratch register with uos image size and notify bootstrap
	status = notify_uosboot(mic_ctx, uos_size);

	return status;
}

/*
 DESCRIPTION :: boots Maintenance mode handler on the card
 PARAMETERS ::
  [in]mic_ctx_t *mic_ctx - mic context
  [in]char *imgname - file path for uos image to be loaded on the card
  RETURN_VALUE:: 0 if successful, non-zero if failure
*/
int boot_micdev_app(mic_ctx_t *mic_ctx, char *imgname)
{
	int status = 0;
	uint32_t uos_size = 0;
	uint8_t *mmio_va = 0;
	uint64_t uos_cmd_offset = 0;
	int32_t temp_scratch2 = 0;

	printk("MIC %d Booting\n", mic_ctx->bi_id);
	mmio_va = mic_ctx->mmio.va;
	status = load_uos_into_gddr(mic_ctx, imgname, &uos_size, &uos_cmd_offset);
	if(status) {
		printk("Cannot load uos in gddr\n");
		goto exit;
	}

	temp_scratch2 = SBOX_READ(mmio_va, SBOX_SCRATCH2);
	/* clear download bit */
	temp_scratch2 = SCRATCH2_CLEAR_DOWNLOAD_STATUS(temp_scratch2);
	SBOX_WRITE(temp_scratch2, mmio_va, SBOX_SCRATCH2);

	//program scratch register with uos image size and notify bootstrap
	status = notify_uosboot(mic_ctx, uos_size);
	if(status)
		goto exit;
	status = wait_for_bootstrap(mmio_va);
exit:
	if(status) {
		mic_setstate(mic_ctx, MIC_BOOTFAIL);
	} else {
		mic_setstate(mic_ctx, MIC_ONLINE);
		mic_ctx->boot_count++;
		printk("ELF booted succesfully\n");
		;
	}
	return status;
}

/* Perform hardware reset of the device */
void
reset_timer(unsigned long arg)
{
	mic_ctx_t *mic_ctx = (mic_ctx_t *)arg;
	uint32_t scratch2 = 0;
	uint32_t postcode = mic_getpostcode(mic_ctx);

	printk("mic%d: Resetting (Post Code %c%c)\n", mic_ctx->bi_id, 
		    postcode & 0xff, (postcode >> 8) & 0xff);
	mic_ctx->reset_count++;

	/* Assuming that the bootstrap takes around 90 seconds to reset,
	 * we fail after 300 seconds, thus allowing 3 attempts to reset
	 */
	if (mic_ctx->reset_count == RESET_FAIL_TIME ||
		!postcode || 0xffffffff == postcode || mic_ctx->state == MIC_RESETFAIL) {
		mic_ctx->reset_count = 0;
		mic_setstate(mic_ctx, MIC_RESETFAIL);
		wake_up(&mic_ctx->resetwq);
		printk("MIC %d RESETFAIL postcode %c%c %d\n", mic_ctx->bi_id, 
			postcode & 0xff, (postcode >> 8) & 0xff, postcode);
		return;
	}

	/* check for F2 or F4 error codes from bootstrap */
	if ((postcode == RESET_FAILED_F2) || (postcode == RESET_FAILED_F4)) {
		if (mic_ctx->resetworkq) {
			queue_work(mic_ctx->resetworkq, &mic_ctx->resetwork);
		} else {
			mic_ctx->reset_count = 0;
			mic_setstate(mic_ctx, MIC_RESETFAIL);
			wake_up(&mic_ctx->resetwq);
			return;
		}
	}

	/* checking if bootstrap is ready or still resetting */
	scratch2 = SBOX_READ(mic_ctx->mmio.va, SBOX_SCRATCH2);
	if (SCRATCH2_DOWNLOAD_STATUS(scratch2)) {
		mic_ctx->boot_start = 0;
		mic_setstate(mic_ctx, MIC_READY);

		if (mic_ctx->msie)
			mic_enable_msi_interrupts(mic_ctx);
		mic_enable_interrupts(mic_ctx);
		mic_smpt_restore(mic_ctx);
		micscif_start(mic_ctx);

		wake_up(&mic_ctx->resetwq);
		mic_ctx->reset_count = 0;

		return;
	}

	mic_ctx->boot_timer.function = reset_timer;
	mic_ctx->boot_timer.data = (unsigned long)mic_ctx;
	mic_ctx->boot_timer.expires = jiffies + HZ;

	add_timer(&mic_ctx->boot_timer);
}

void
adapter_wait_reset(mic_ctx_t *mic_ctx)
{
	mic_ctx->boot_timer.function = reset_timer;
	mic_ctx->boot_timer.data = (unsigned long)mic_ctx;
	mic_ctx->boot_timer.expires = jiffies + HZ;
	mic_ctx->boot_start = jiffies;

	add_timer(&mic_ctx->boot_timer);
}

void
adapter_reset(mic_ctx_t *mic_ctx, int wait_reset, int reattempt)
{
	uint32_t resetReg;
	mutex_lock(&mic_ctx->state_lock);
	/* TODO: check state for lost node as well once design is done */
	if ((mic_ctx->state == MIC_RESET || mic_ctx->state == MIC_READY) && (reattempt == 0)) {
		if (wait_reset == 0) {
			mic_setstate(mic_ctx, MIC_INVALID);
			del_timer_sync(&mic_ctx->boot_timer);
			mutex_unlock(&mic_ctx->state_lock);
			return;
		}
		mutex_unlock(&mic_ctx->state_lock);
		return;
	}

	mic_setstate(mic_ctx, MIC_RESET);

	mutex_unlock(&mic_ctx->state_lock);

	del_timer_sync(&mic_ctx->boot_timer);

	//Write 0 to uos download status otherwise we might continue booting
	//before reset has completed...
	SBOX_WRITE(0, mic_ctx->mmio.va, SBOX_SCRATCH2);

	// Virtual network link value should be 0 before reset
	SBOX_WRITE(0, mic_ctx->mmio.va, SBOX_SCRATCH14);

	// Data from Doorbell1 about restart/shutdown should be 0 before reset
	SBOX_WRITE(0, mic_ctx->mmio.va, SBOX_SDBIC1);

	//This will trigger reset
	resetReg = SBOX_READ(mic_ctx->mmio.va, SBOX_RGCR);
	resetReg |= 0x1;
	SBOX_WRITE(resetReg, mic_ctx->mmio.va, SBOX_RGCR);

	/* At least of KNF it seems we really want to delay at least 1 second */
	/* after touching reset to prevent a lot of problems. */
	msleep(1000);

	if (!wait_reset) {
		return;
	}

	adapter_wait_reset(mic_ctx);

}

void ramoops_flip(mic_ctx_t *mic_ctx);

int
adapter_shutdown_device(mic_ctx_t *mic_ctx)
{
	;

	if (micpm_get_reference(mic_ctx, true))
		return 0;

	mutex_lock(&mic_ctx->state_lock);
	if (mic_ctx->state == MIC_ONLINE) {
		mic_setstate(mic_ctx, MIC_SHUTDOWN);

		/*
		 * Writing to SBOX RDMASR0 will generate an interrupt
		 * on the uOS which will initiate orderly shutdown.
		 */
		mic_send_sht_intr(mic_ctx);
	}
	mutex_unlock(&mic_ctx->state_lock);

	micpm_put_reference(mic_ctx);
	return 0;
}

int
adapter_stop_device(mic_ctx_t *mic_ctx, int wait_reset, int reattempt)
{
	;

	micvcons_stop(mic_ctx);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,34) || \
	defined(RHEL_RELEASE_CODE)
	mic_vhost_blk_stop(mic_ctx->bd_info);
#endif
	micveth_stop(mic_ctx);

	micpm_stop(mic_ctx);
	micscif_stop(mic_ctx);
	vmcore_remove(mic_ctx);
	close_dma_device(mic_ctx->bi_id + 1, &mic_ctx->dma_handle);
	ramoops_flip(mic_ctx);

	/* Calling adapter_reset after issuing Host shutdown/reboot
 	* leads to randon NMIs. These are not rleated to any Card in
 	* specific but occurs on the PCI bridge.  */
	if ((system_state == SYSTEM_POWER_OFF) ||
		(system_state == SYSTEM_RESTART) ||
		(system_state == SYSTEM_HALT))
		return 0;
	adapter_reset(mic_ctx, wait_reset, reattempt);

    return 0;
}

static void
destroy_reset_workqueue(mic_ctx_t *mic_ctx)
{
	struct workqueue_struct *tempworkq;
	tempworkq = mic_ctx->resetworkq;
	mic_ctx->resetworkq = NULL;
	destroy_workqueue(tempworkq);
	del_timer_sync(&mic_ctx->boot_timer);
}

int
adapter_remove(mic_ctx_t *mic_ctx)
{

#ifdef USE_VCONSOLE
	if (mic_ctx->bi_vcons.dc_hdr_virt) {
		mic_ctx_unmap_single(mic_ctx, mic_ctx->bi_vcons.dc_hdr_dma_addr,
			sizeof(struct vcons_buf));
		kfree(mic_ctx->bi_vcons.dc_hdr_virt);
		mic_ctx->bi_vcons.dc_hdr_virt = NULL;
	}

	if (mic_ctx->bi_vcons.dc_buf_virt) {
		mic_ctx_unmap_single(mic_ctx, mic_ctx->bi_vcons.dc_dma_addr,
			MICVCONS_BUF_SIZE);
		free_pages((uint64_t)mic_ctx->bi_vcons.dc_buf_virt, 0);
		mic_ctx->bi_vcons.dc_buf_virt = NULL;
	}
#endif

	mic_psmi_uninit(mic_ctx);
	micpm_remove(mic_ctx);
	micscif_remove(mic_ctx);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,34)) || defined(RHEL_RELEASE_CODE)
	mic_vhost_blk_remove(mic_ctx->bd_info);
#endif
	micveth_remove(mic_ctx);
	mic_unreg_irqhandler(mic_ctx, 0x1, "MIC SHUTDOWN DoorBell 1");

	ramoops_remove(mic_ctx);
	vmcore_remove(mic_ctx);
	mic_smpt_uninit(mic_ctx);
	/* Make sure that no reset timer is running after the workqueue is destroyed */
	destroy_reset_workqueue(mic_ctx);

	if (mic_ctx->mmio.va) {
		iounmap((void *)mic_ctx->mmio.va);
		mic_ctx->mmio.va = 0;
	}

	if (mic_ctx->aper.va) {
		iounmap((void *)mic_ctx->aper.va);
		mic_ctx->aper.va = 0;
	}


	return 0;
}

#define MIC_MAX_BOOT_TIME 180	// Maximum number of seconds to wait for boot to complete

static void
online_timer(unsigned long arg)
{
	mic_ctx_t *mic_ctx = (mic_ctx_t *)arg;
	uint64_t delay = (jiffies - mic_ctx->boot_start) / HZ;

	if (mic_ctx->state == MIC_ONLINE)
		return;

	if (delay > MIC_MAX_BOOT_TIME) {
		printk("Fail booting MIC %d. Wait time execeed %d seconds\n", mic_ctx->bi_id, MIC_MAX_BOOT_TIME);
		mic_ctx->state = MIC_BOOTFAIL;
		return;
	}

	mic_ctx->boot_timer.function = online_timer;
	mic_ctx->boot_timer.data = (unsigned long)mic_ctx;
	mic_ctx->boot_timer.expires = jiffies + HZ;
	add_timer(&mic_ctx->boot_timer);

	if (!(delay % 5))
		printk("Waiting for MIC %d boot %lld\n", mic_ctx->bi_id, delay);
}

static void
boot_timer(unsigned long arg)
{
	mic_ctx_t *mic_ctx = (mic_ctx_t *)arg;
	struct micvnet_info *vnet_info = (struct micvnet_info *) mic_ctx->bi_vethinfo;
	uint64_t delay = (jiffies - mic_ctx->boot_start) / HZ;
	bool timer_restart = false;

	if ((mic_ctx->state != MIC_BOOT) && (mic_ctx->state != MIC_ONLINE)) {
		return;
	}

	if (delay > MIC_MAX_BOOT_TIME) {
		printk("Fail booting MIC %d. Wait time execeed %d seconds\n", mic_ctx->bi_id, MIC_MAX_BOOT_TIME);
		mic_ctx->state = MIC_BOOTFAIL;
		return;
	}

	if (!(delay % 5))
		printk("Waiting for MIC %d boot %lld\n", mic_ctx->bi_id, delay);

	if (mic_vnet_mode != VNET_MODE_DMA)
		timer_restart = (SBOX_READ(mic_ctx->mmio.va, SBOX_SCRATCH14) == 0)?
				true : false;
	else if (atomic_read(&vnet_info->vi_state) != MICVNET_STATE_LINKUP)
		timer_restart = (mic_ctx->state != MIC_ONLINE)? true: false;

	if (timer_restart) {
		mic_ctx->boot_timer.function = boot_timer;
		mic_ctx->boot_timer.data = (unsigned long)mic_ctx;
		mic_ctx->boot_timer.expires = jiffies + HZ;

		add_timer(&mic_ctx->boot_timer);
		return;
	}

	mic_ctx->boot_timer.function = online_timer;
	mic_ctx->boot_timer.data = (unsigned long)mic_ctx;
	mic_ctx->boot_timer.expires = jiffies + HZ;
	add_timer(&mic_ctx->boot_timer);

	printk("MIC %d Network link is up\n", mic_ctx->bi_id);
	schedule_work(&mic_ctx->boot_ws);
}

void
post_boot_startup(struct work_struct *work)
{

	mic_ctx_t *mic_ctx
		= container_of(work, mic_ctx_t, boot_ws);

	if (micpm_get_reference(mic_ctx, true) != 0)
		return;

	// We should only enable DMA after uos is booted
	BUG_ON(open_dma_device(mic_ctx->bi_id+1,
				     mic_ctx->mmio.va + HOST_SBOX_BASE_ADDRESS,
				     &mic_ctx->dma_handle));
	if (micveth_start(mic_ctx))
		printk(KERN_ERR "%s: micveth_start failed\n", __FUNCTION__);
	micpm_put_reference(mic_ctx);

}

void
attempt_reset(struct work_struct *work)
{
	mic_ctx_t *mic_ctx
		= container_of(work, mic_ctx_t, resetwork);
	printk("Reattempting reset after F2/F4 failure\n");
	adapter_reset(mic_ctx, RESET_WAIT, RESET_REATTEMPT);
}

static void
ioremap_work(struct work_struct *work)
{
	mic_ctx_t *mic_ctx
		= container_of(work, mic_ctx_t, ioremapwork);
	mic_ctx->aper.va = ioremap_wc(mic_ctx->aper.pa, mic_ctx->aper.len);
	if (mic_ctx->aper.va == NULL) {
		printk(KERN_ERR "mic %d: failed to map aperture space\n", mic_ctx->bi_id);
		mutex_lock(&mic_ctx->state_lock);
		mic_setstate(mic_ctx, MIC_RESETFAIL);
		mutex_unlock(&mic_ctx->state_lock);
	}
	wake_up(&mic_ctx->ioremapwq);
}

int
adapter_post_boot_device(mic_ctx_t *mic_ctx)
{
	mic_ctx->boot_timer.function = boot_timer;
	mic_ctx->boot_timer.data = (unsigned long)mic_ctx;
	mic_ctx->boot_timer.expires = jiffies + HZ;
	mic_ctx->boot_start = jiffies;

	add_timer(&mic_ctx->boot_timer);
	return 0;
}

int
mic_shutdown_host_doorbell_intr_handler(mic_ctx_t *mic_ctx, int doorbell)
{
	struct micscif_dev *dev = &scif_dev[mic_get_scifnode_id(mic_ctx)];
	mic_ctx->sdbic1 = SBOX_READ(mic_ctx->mmio.va, SBOX_SDBIC1);
	SBOX_WRITE(0x0, mic_ctx->mmio.va, SBOX_SDBIC1);
	if (mic_ctx->sdbic1)
		queue_delayed_work(dev->sd_ln_wq, 
				&dev->sd_watchdog_work, 0);
	return 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
static int
ramoops_proc_show(struct seq_file *m, void *data)
{
	uint64_t id = ((uint64_t)data) & 0xffffffff;
	uint64_t entry = ((uint64_t)data) >> 32;
	struct list_head *pos, *tmpq;
	bd_info_t *bd = NULL;
	mic_ctx_t *mic_ctx = NULL;
	char *record;
	char *end;
	int size = 0;
	int l = 0;
	char *output;
	unsigned long flags;

	list_for_each_safe(pos, tmpq, &mic_data.dd_bdlist) {
		bd = list_entry(pos, bd_info_t, bi_list);
		mic_ctx = &bd->bi_ctx;
		if (mic_ctx->bi_id == id)
			break;
	}

	if (mic_ctx == NULL)
		return 0;

	spin_lock_irqsave(&mic_ctx->ramoops_lock, flags);

	record = mic_ctx->ramoops_va[entry];
	if (record == NULL) {
		spin_unlock_irqrestore(&mic_ctx->ramoops_lock, flags);
		return -EEXIST;
	}

	size = mic_ctx->ramoops_size;
	end = record + size;

	if ((output = kzalloc(size, GFP_ATOMIC)) == NULL) {
		spin_unlock_irqrestore(&mic_ctx->ramoops_lock, flags);
		return -ENOMEM;
	}

	l += scnprintf(output, size, "%s", record);

	spin_unlock_irqrestore(&mic_ctx->ramoops_lock, flags);

	seq_printf(m, "%s", output);
	return 0;
}

static int
ramoops_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, ramoops_proc_show, NULL);
}

struct file_operations ramoops_proc_fops = {
	.open		= ramoops_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
        .release 	= single_release,
};

#else // LINUX VERSION
static int
ramoops_read(char *buf, char **start, off_t offset, int len, int *eof, void *data)
{
	uint64_t id = ((uint64_t)data) & 0xffffffff;
	uint64_t entry = ((uint64_t)data) >> 32;
	struct list_head *pos, *tmpq;
	bd_info_t *bd = NULL;
	mic_ctx_t *mic_ctx = NULL;
	char *record;
	char *end;
	int size = 0;
	int l = 0;
	int left_to_read;
	char *output;
	unsigned long flags;

	list_for_each_safe(pos, tmpq, &mic_data.dd_bdlist) {
		bd = list_entry(pos, bd_info_t, bi_list);
		mic_ctx = &bd->bi_ctx;
		if (mic_ctx->bi_id == id)
			break;
	}

	if (mic_ctx == NULL)
		return 0;

	spin_lock_irqsave(&mic_ctx->ramoops_lock, flags);

	record = mic_ctx->ramoops_va[entry];
	if (record == NULL) {
		spin_unlock_irqrestore(&mic_ctx->ramoops_lock, flags);
		*eof = 1;
		return 0;
	}

	size = mic_ctx->ramoops_size;
	end = record + size;

	if ((output = kzalloc(size, GFP_ATOMIC)) == NULL) {
		spin_unlock_irqrestore(&mic_ctx->ramoops_lock, flags);
		return -ENOMEM;
	}

	l += scnprintf(output, size, "%s", record);

	spin_unlock_irqrestore(&mic_ctx->ramoops_lock, flags);

	left_to_read = l - offset;
	if (left_to_read < 0)
		left_to_read = 0;
	if (left_to_read == 0)
		*eof = 1;

	left_to_read = min(len, left_to_read);
	memcpy(buf, output + offset, left_to_read);
	kfree(output);
	*start = buf;
	return left_to_read;
}
#endif // LINUX VERSION

int
set_ramoops_pa(mic_ctx_t *mic_ctx)
{
	if (mic_ctx->ramoops_pa[0] == 0L) {
		kfree(mic_ctx->ramoops_va[0]);
		mic_ctx->ramoops_size = 0;
		mic_ctx->ramoops_va[0] = NULL;
		return 1;
	}
	return 0;
}

int ramoops_count = 4;

void
ramoops_probe(mic_ctx_t *mic_ctx)
{
	char name[64];

	mic_ctx->ramoops_size = ramoops_count * PAGE_SIZE;
	if ((mic_ctx->ramoops_va[0] = kzalloc(mic_ctx->ramoops_size, GFP_KERNEL)) != NULL) {
		spin_lock_init(&mic_ctx->ramoops_lock);
		mic_ctx->ramoops_va[1] = NULL;

		mic_ctx->ramoops_pa[0] = mic_ctx_map_single(mic_ctx, mic_ctx->ramoops_va[0],
							    mic_ctx->ramoops_size);
		if (set_ramoops_pa(mic_ctx))
			return;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
		snprintf(name, 64, "mic%d", mic_ctx->bi_id);
		proc_create_data(name, 0444, ramoops_dir, &ramoops_proc_fops,
				 (void *)(long)mic_ctx->bi_id);

		snprintf(name, 64, "mic%d_prev", mic_ctx->bi_id);
		proc_create_data(name, 0444, ramoops_dir, &ramoops_proc_fops,
				 (void *)((long)mic_ctx->bi_id | (1L << 32)));
#else // LINUX VERSION
		snprintf(name, 64, "mic%d", mic_ctx->bi_id);
		if (create_proc_read_entry(name, 0444, ramoops_dir, ramoops_read,
					   (void *)(long)mic_ctx->bi_id) == NULL)
			printk("Failed to intialize /proc/mic_ramoops/%s\n", name);

		snprintf(name, 64, "mic%d_prev", mic_ctx->bi_id);
		if (create_proc_read_entry(name, 0444, ramoops_dir, ramoops_read,
					   (void *)((long)mic_ctx->bi_id | (1L << 32))) == NULL)
			printk("Failed to intialize /proc/mic_ramoops/%s\n", name);
#endif //LINUX VERSION
	} else {
		mic_ctx->ramoops_size = 0;
	}
}

void
ramoops_flip(mic_ctx_t *mic_ctx)
{
	unsigned long flags;

	if (mic_ctx->ramoops_size == 0)
		return;

	spin_lock_irqsave(&mic_ctx->ramoops_lock, flags);
	if (mic_ctx->ramoops_va[1] != NULL) {
		mic_ctx_unmap_single(mic_ctx, mic_ctx->ramoops_pa[1], mic_ctx->ramoops_size);
		kfree(mic_ctx->ramoops_va[1]);
	}

	mic_ctx->ramoops_pa[1] = mic_ctx->ramoops_pa[0];
	mic_ctx->ramoops_va[1] = mic_ctx->ramoops_va[0];
	if ((mic_ctx->ramoops_va[0] = kzalloc(mic_ctx->ramoops_size, GFP_ATOMIC)) != NULL) {
		mic_ctx->ramoops_pa[0] = mic_ctx_map_single(mic_ctx, mic_ctx->ramoops_va[0],
							    mic_ctx->ramoops_size);
		set_ramoops_pa(mic_ctx);
	}
	spin_unlock_irqrestore(&mic_ctx->ramoops_lock, flags);
}

int
adapter_probe(mic_ctx_t *mic_ctx)
{
	int db;
	uint32_t scratch13;
	int32_t status = 0;

	// Init the irq information
	atomic_set(&mic_ctx->bi_irq.mi_received, 0);
	spin_lock_init(&mic_ctx->bi_irq.mi_lock);
	tasklet_init(&mic_ctx->bi_dpc, adapter_dpc, (unsigned long)&mic_ctx->bi_dpc);

	for (db = 0; db < MIC_NUM_DB; db++) {
		INIT_LIST_HEAD(&mic_ctx->bi_irq.mi_dblist[db]);
	}

	if (mic_ctx->msie)
		mic_enable_msi_interrupts(mic_ctx);

	scratch13 = SBOX_READ(mic_ctx->mmio.va, SBOX_SCRATCH13);
	mic_ctx->bi_stepping = SCRATCH13_STEP_ID(scratch13);
	mic_ctx->bi_substepping = SCRATCH13_SUB_STEP(scratch13);
#ifdef MIC_IS_EMULATION
	mic_ctx->bi_platform = PLATFORM_EMULATOR;
#else
	mic_ctx->bi_platform = SCRATCH13_PLATFORM_ID(scratch13);
#endif

	mic_enable_interrupts(mic_ctx);
	if (micveth_probe(mic_ctx))
		printk(KERN_ERR "%s: micveth_probe failed\n", __FUNCTION__);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,34)) || defined(RHEL_RELEASE_CODE)
	if (mic_vhost_blk_probe(mic_ctx->bd_info))
		printk(KERN_ERR "%s: mic_vhost_blk_probe failed\n", __FUNCTION__);
#endif
	micscif_probe(mic_ctx);
	if(micpm_probe(mic_ctx))
		printk(KERN_ERR "%s: micpm_probe failed\n", __FUNCTION__);

	mic_reg_irqhandler(mic_ctx, 1, "MIC SHUTDOWN DoorBell 1",
			mic_shutdown_host_doorbell_intr_handler);

	ramoops_probe(mic_ctx);
	if (status) {
		printk("boot_linux_uos failed \n");
		return status;
	}

	// We should only enable DMA after uos is booted
	//mic_dma_lib_init(mic_ctx->mmio.va+HOST_SBOX_BASE_ADDRESS);

	return status;
}

int
adapter_start_device(mic_ctx_t *mic_ctx)
{
	int ret;

	mutex_lock(&mic_ctx->state_lock);
	if (mic_ctx->state == MIC_READY) {
		mic_setstate(mic_ctx, MIC_BOOT);
	} else {
		mutex_unlock(&mic_ctx->state_lock);
		/* TODO: Unknown state handling? */
		printk(KERN_ERR "%s %d state %d??\n", 
			__func__, __LINE__, mic_ctx->state);
		ret = -EINVAL;
		goto exit;
	}
	mutex_unlock(&mic_ctx->state_lock);
	mic_ctx->mode = MODE_LINUX;
	ret = boot_linux_uos(mic_ctx, mic_ctx->image, mic_ctx->initramfs);
	if (ret) {
		printk(KERN_ERR "boot_linux_uos failed %d\n", ret);
		goto exit;
	}

	ret = adapter_post_boot_device(mic_ctx);
	if (ret) {
		printk(KERN_ERR "adapter post boot failed %d\n", ret);
		goto exit;
	}

	pr_debug("adapter started successfully\n");
exit:
	return ret;
}

int
adapter_init_device(mic_ctx_t *mic_ctx)
{
#ifdef USE_VCONSOLE
	struct vcons_buf *vcons_buf;
#endif
	uint32_t mmio_data_cc; /* mmio data from class code register */
	uint32_t mmio_data_bar; /* mmio data from bar enable register */
	uint32_t device_id;
	int err = 0;

	spin_lock_init(&mic_ctx->sysfs_lock);
	mic_setstate(mic_ctx, MIC_RESET);
	mic_ctx->mode = MODE_NONE;
	mic_ctx->reset_count = 0;
	mutex_init (&mic_ctx->state_lock);
	init_waitqueue_head(&mic_ctx->resetwq);
	init_waitqueue_head(&mic_ctx->ioremapwq);
	init_timer(&mic_ctx->boot_timer);
	if (!(mic_ctx->resetworkq = __mic_create_singlethread_workqueue("RESET WORK")))
		return -ENOMEM;
	if (!(mic_ctx->ioremapworkq = __mic_create_singlethread_workqueue("IOREMAP_WORK"))) {
		err = -EINVAL;
		goto destroy_reset_wq;
	}
	INIT_WORK(&mic_ctx->ioremapwork, ioremap_work);
	INIT_WORK(&mic_ctx->boot_ws, post_boot_startup);
	INIT_WORK(&mic_ctx->resetwork, attempt_reset);
	atomic_set(&mic_ctx->gate_interrupt, 0);

	device_id = mic_ctx->bi_pdev->device;
	mic_ctx->bi_family = get_product_family(device_id);

	if ((mic_ctx->mmio.va = ioremap_nocache(mic_ctx->mmio.pa, 
						mic_ctx->mmio.len)) == NULL) {
		printk("mic %d: failed to map mmio space\n", mic_ctx->bi_id);
		err = -ENOMEM;
		goto destroy_remap_wq;
	}

	if (mic_ctx->aper.pa == 0) {
		/*
		 * Read class code from SBOX_PCIE_PCI_REVISION_ID_AND_C_0X8 register
		 * If the mode is zombie, then
		 * 1> Aperture is not available
		 * 2> Register 0x5CD4 is written to 0x00000002 to disable all BARs except MMIO
		 * 3> Register 0x5808 is written to 0xFF0000XX to set the class ID to a generic PCI device.
		 */
		mmio_data_cc = SBOX_READ(mic_ctx->mmio.va, SBOX_PCIE_PCI_REVISION_ID_AND_C_0X8);
		mmio_data_cc = PCIE_CLASS_CODE(mmio_data_cc);
		mmio_data_bar = SBOX_READ(mic_ctx->mmio.va, SBOX_PCIE_BAR_ENABLE);

		if((mmio_data_cc == ZOMBIE_CLASS_CODE) && (mmio_data_bar == DISABLE_BAR)) {
			mic_ctx->card_usage_mode = USAGE_MODE_ZOMBIE;
			usagemode_param = USAGE_MODE_ZOMBIE;
		} else {
			printk("Error: Not in zombie mode and aperture is 0\n");
			err = -EINVAL;
			goto adap_init_unmapmmio;
		}
	} else {
		if (mic_ctx->ioremapworkq) {
			queue_work(mic_ctx->ioremapworkq, &mic_ctx->ioremapwork);
		} else {
			if ((mic_ctx->aper.va = ioremap_wc(mic_ctx->aper.pa, mic_ctx->aper.len)) == NULL) {
				printk("mic %d: failed to map aperture space\n", mic_ctx->bi_id);
				err = -EINVAL;
				goto adap_init_unmapmmio;
			}
		}
	}

	mic_debug_init(mic_ctx);
	mic_smpt_init(mic_ctx);
#ifdef USE_VCONSOLE
	// Allocate memory for PCI serial console
	mic_ctx->bi_vcons.dc_buf_virt = (void *)get_zeroed_page(GFP_KERNEL);
	mic_ctx->bi_vcons.dc_hdr_virt = kzalloc(sizeof(struct vcons_buf), GFP_KERNEL);

	if ((!mic_ctx->bi_vcons.dc_buf_virt) || (!mic_ctx->bi_vcons.dc_hdr_virt)) {
		printk(KERN_ERR "mic %d: failed to allocate memory for vcons buffer\n", 
			    mic_ctx->bi_id);
		mic_ctx->bi_vcons.dc_enabled = 0;
		if (mic_ctx->bi_vcons.dc_buf_virt)
			free_pages((uint64_t)mic_ctx->bi_vcons.dc_buf_virt, 0);
		if (mic_ctx->bi_vcons.dc_hdr_virt)
			kfree(mic_ctx->bi_vcons.dc_hdr_virt);
	} else {
		mic_ctx->bi_vcons.dc_hdr_dma_addr = mic_ctx_map_single(mic_ctx,
						mic_ctx->bi_vcons.dc_hdr_virt,
						sizeof(struct vcons_buf));
		mic_ctx->bi_vcons.dc_dma_addr = mic_ctx_map_single(mic_ctx,
						mic_ctx->bi_vcons.dc_buf_virt,
						MICVCONS_BUF_SIZE);
		if ((!mic_ctx->bi_vcons.dc_dma_addr) ||
			(!mic_ctx->bi_vcons.dc_hdr_dma_addr))
			mic_ctx->bi_vcons.dc_enabled = 0;
		else
			mic_ctx->bi_vcons.dc_enabled = 1;
		mic_ctx->bi_vcons.dc_size = MICVCONS_BUF_SIZE;
		vcons_buf = (struct vcons_buf *)(mic_ctx->bi_vcons.dc_hdr_virt);
		vcons_buf->o_buf_dma_addr = mic_ctx->bi_vcons.dc_dma_addr;
		vcons_buf->o_size = MICVCONS_BUF_SIZE;
		smp_wmb();
		vcons_buf->host_magic = MIC_HOST_VCONS_READY;
		vcons_buf->host_rb_ver = micscif_rb_get_version();
	}
#endif // USE_VCONSOLE
	mic_ctx->boot_mem = 0;
	mic_psmi_init(mic_ctx);
	mic_ctx->dma_handle = NULL;
	mic_ctx->sdbic1 = 0;
    // To avoid hazard on Windows, sku_build_table is done on DriverEntry
	sku_build_table();
	device_id = mic_ctx->bi_pdev->device;
	sku_find(mic_ctx, device_id);
    // To avoid hazard on Windows, sku_destroy_table is done on MicUnload
	sku_destroy_table();

	/* Determine the amount of compensation that needs to be applied to MIC's ETC timer */
	calculate_etc_compensation(mic_ctx);

	return 0;

adap_init_unmapmmio:
	iounmap(mic_ctx->mmio.va);
destroy_remap_wq:
	destroy_workqueue(mic_ctx->ioremapworkq);
destroy_reset_wq:
	destroy_workqueue(mic_ctx->resetworkq);
	return err;
}

void
mic_enable_interrupts(mic_ctx_t *mic_ctx)
{
	ENABLE_MIC_INTERRUPTS(mic_ctx->mmio.va);
}

void
mic_disable_interrupts(mic_ctx_t *mic_ctx)
{
	uint32_t sboxSice0reg;

	sboxSice0reg = SBOX_READ(mic_ctx->mmio.va, SBOX_SICE0);
	SBOX_WRITE(sboxSice0reg, mic_ctx->mmio.va, SBOX_SICC0);
}

void
mic_enable_msi_interrupts(mic_ctx_t *mic_ctx)
{
	uint32_t sboxMXARreg;

	// Only support single MSI interrupt for now
	sboxMXARreg = SBOX_SICE0_DBR_BITS(0xf) | SBOX_SICE0_DMA_BITS(0xff);
	if (mic_ctx->bi_family == FAMILY_KNC)
		SBOX_WRITE(sboxMXARreg, mic_ctx->mmio.va, SBOX_MXAR0_K1OM);
	else
		SBOX_WRITE(sboxMXARreg, mic_ctx->mmio.va, SBOX_MXAR0);
}

int
mic_reg_irqhandler(mic_ctx_t *mic_ctx, int doorbell, char *idstring,
		   int (*irqfunc)(mic_ctx_t *mic_ctx, int doorbell))
{
	mic_irqhandler_t *irqhandle;
	unsigned long flags;

	if (doorbell > MIC_IRQ_MAX) {
		return EINVAL;
	}

	if (!(irqhandle = kmalloc(sizeof(mic_irqhandler_t), GFP_ATOMIC)))
		goto memerror1;

	if (!(irqhandle->ih_idstring = kmalloc(strlen(idstring) + 1, GFP_ATOMIC)))
		goto memerror2;

	irqhandle->ih_func = irqfunc;
	strcpy(irqhandle->ih_idstring, idstring);

	spin_lock_irqsave(&mic_ctx->bi_irq.mi_lock, flags);
	list_add_tail(&irqhandle->ih_list, &mic_ctx->bi_irq.mi_dblist[doorbell]);
	spin_unlock_irqrestore(&mic_ctx->bi_irq.mi_lock, flags);
	return 0;

memerror2:
	kfree(irqhandle);
memerror1:
	return -ENOMEM;
}

int
mic_unreg_irqhandler(mic_ctx_t *mic_ctx, int doorbell, char *idstring)
{
	mic_irqhandler_t *irqhandle;
	struct list_head *pos, *tmpq;
	unsigned long flags;

	spin_lock_irqsave(&mic_ctx->bi_irq.mi_lock, flags);
	list_for_each_safe(pos, tmpq, &mic_ctx->bi_irq.mi_dblist[doorbell]) {
		irqhandle = list_entry(pos, mic_irqhandler_t, ih_list);
		if (strcmp(idstring, irqhandle->ih_idstring) == 0) {
			list_del(pos);
			kfree(irqhandle->ih_idstring);
			kfree(irqhandle);
		}
	}
	spin_unlock_irqrestore(&mic_ctx->bi_irq.mi_lock, flags);

	return 0;
}

static __always_inline
void adapter_process_one_interrupt(mic_ctx_t *mic_ctx, uint32_t events)
{
	mic_irqhandler_t *irqhandle;
	struct list_head *pos;
	int doorbell;

	atomic_inc(&mic_ctx->bi_irq.mi_received);

	if (SBOX_SICR0_DBR(events)) {
		for (doorbell = 0; doorbell < 4; doorbell++) {
			if (SBOX_SICR0_DBR(events) & (0x1 << doorbell)) {
				spin_lock(&mic_ctx->bi_irq.mi_lock);
				list_for_each(pos, &mic_ctx->bi_irq.mi_dblist[doorbell]) {
					irqhandle = list_entry(pos, mic_irqhandler_t, ih_list);
					irqhandle->ih_func(mic_ctx, doorbell);
				}
				spin_unlock(&mic_ctx->bi_irq.mi_lock);
			}
		}

	}

	if (SBOX_SICR0_DMA(events))
		host_dma_interrupt_handler(mic_ctx->dma_handle, events);
}

int
adapter_isr(mic_ctx_t *mic_ctx)
{
	volatile uint32_t sboxSicr0reg;
	if (atomic_cmpxchg(&mic_ctx->gate_interrupt, 0, 1) == 1)
		return -1;

	sboxSicr0reg = SBOX_READ(mic_ctx->mmio.va, SBOX_SICR0);

	if (unlikely(!sboxSicr0reg)) {
		// Spurious interrupt
		atomic_set(&mic_ctx->gate_interrupt, 0);
		return -1;
	}

	// tell mic that we recived interrupt otherwise it will keep sending them
	SBOX_WRITE(sboxSicr0reg, mic_ctx->mmio.va, SBOX_SICR0);

	// This only applies to KNC B0
	if (FAMILY_KNC == mic_ctx->bi_family &&
		mic_ctx->bi_stepping >= KNC_B0_STEP)
		mic_enable_interrupts(mic_ctx);

	atomic_set(&mic_ctx->gate_interrupt, 0);
	adapter_process_one_interrupt(mic_ctx, sboxSicr0reg);
	return 0;
}

int
adapter_imsr(mic_ctx_t *mic_ctx)
{
#if 0 /* TODO: disable interrupt when KNC auto-enable isn't used */
	mic_disable_interrupts(mic_ctx);
#endif
	tasklet_schedule(&mic_ctx->bi_dpc);
	return 0;
}

static void adapter_dpc(unsigned long dpc)
{
	mic_ctx_t *mic_ctx =
		container_of((struct tasklet_struct *)dpc, mic_ctx_t, bi_dpc);

	volatile uint32_t sboxSicr0reg;

	if (atomic_cmpxchg(&mic_ctx->gate_interrupt, 0, 1) == 1)
		return;

	/* Clear pending bit array */
	if (FAMILY_KNC == mic_ctx->bi_family) {
		if (KNC_A_STEP == mic_ctx->bi_stepping)
			SBOX_WRITE(1, mic_ctx->mmio.va, SBOX_MSIXPBACR_K1OM);
	} else
		SBOX_WRITE(1, mic_ctx->mmio.va, SBOX_MSIXPBACR);

	sboxSicr0reg = SBOX_READ(mic_ctx->mmio.va, SBOX_SICR0);
	if (unlikely(!sboxSicr0reg)) {
		atomic_set(&mic_ctx->gate_interrupt, 0);
		return;
	}

	SBOX_WRITE(sboxSicr0reg, mic_ctx->mmio.va, SBOX_SICR0);

	// This only applies to KNC B0
	if (FAMILY_KNC == mic_ctx->bi_family &&
		mic_ctx->bi_stepping >= KNC_B0_STEP)
		mic_enable_interrupts(mic_ctx);

	atomic_set(&mic_ctx->gate_interrupt, 0);
	adapter_process_one_interrupt(mic_ctx, sboxSicr0reg);
}

void ramoops_init(void)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
	ramoops_dir = proc_mkdir("mic_ramoops", NULL);
#else
	ramoops_dir = create_proc_entry("mic_ramoops", S_IFDIR | S_IRUGO, NULL);
#endif
}

void ramoops_exit(void)
{
	remove_proc_entry("mic_ramoops", NULL);
}

void ramoops_remove(mic_ctx_t *mic_ctx)
{
	char name[64];
	int i;

	snprintf(name, 64, "mic%d", mic_ctx->bi_id);
	remove_proc_entry(name, ramoops_dir);

	snprintf(name, 64, "mic%d_prev", mic_ctx->bi_id);
	remove_proc_entry(name, ramoops_dir);
	if (mic_ctx->ramoops_size == 0)
		return;

	for (i = 0; i < 2; i++) {
		if (mic_ctx->ramoops_va[i] != NULL) {
			mic_ctx_unmap_single(mic_ctx, mic_ctx->ramoops_pa[i],
					     mic_ctx->ramoops_size);
			kfree(mic_ctx->ramoops_va[i]);
		}
	}
}

void vmcore_init(void)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
	vmcore_dir = proc_mkdir("mic_vmcore", NULL);
#else
	vmcore_dir = create_proc_entry("mic_vmcore", S_IFDIR | S_IRUGO, NULL);
#endif
}

void vmcore_exit(void)
{
	if (vmcore_dir) {
		remove_proc_entry("mic_vmcore", NULL);
		vmcore_dir = NULL;
	}
}

void vmcore_remove(mic_ctx_t *mic_ctx)
{
	char name[64];

	snprintf(name, 64, "mic%d", mic_ctx->bi_id);
	if (mic_ctx->vmcore_dir) {
		remove_proc_entry(name, vmcore_dir);
		mic_ctx->vmcore_dir = NULL;
	}
	if (mic_ctx->elfcorebuf) {
		kfree(mic_ctx->elfcorebuf);
		mic_ctx->elfcorebuf = NULL;
		mic_ctx->elfcorebuf_sz = 0;
		mic_ctx->vmcore_size = 0;
	}
}


void
adapter_init(void)
{
	// Per driver init ONLY.
	mic_dma_init();
	micscif_init();
	micpm_init();
	ramoops_init();
	vmcore_init();
	INIT_LIST_HEAD(&mic_data.dd_bdlist);
}


void show_stepping_comm(mic_ctx_t *mic_ctx,char *buf)
{
#define STEPINGSTRSIZE 3
	char string[STEPINGSTRSIZE];
	switch (mic_ctx->bi_family) {
	case FAMILY_ABR:
		switch (mic_ctx->bi_stepping) {
		case 0:
			string[0] = 'A';
			string[1] =  mic_ctx->bi_substepping + '0';
			break;
		case 2:
			string[0] = 'B';
			string[1] = '0';
			break;
		case 3:
			string[0] = 'B';
			string[1] = '1';
			break;
		case 4:
			string[0] = 'C';
			string[1] = '0';
			break;
		case 5:
			string[0] = 'C';
			string[1] = '1';
			break;
		case 6:
			string[0] = 'D';
			string[1] = '0';
			break;
		default:
			string[0] = '?';
			string[1] = '?';
			break;
		}
		break;
	case FAMILY_KNC:
		switch (mic_ctx->bi_stepping) {
		case KNC_A_STEP:
			string[0] = 'A';
			string[1] = '0';
			break;
		case KNC_B0_STEP:
			string[0] = 'B';
			string[1] = '0';
			break;
		case KNC_B1_STEP:
			string[0] = 'B';
			string[1] = '1';
			break;
		case KNC_C_STEP:
			string[0] = 'C';
			string[1] = '0';
			break;
		default:
			string[0] = '?';
			string[1] = '?';
			break;
		}
		break;
	default:
		string[0] = '?';
		string[1] = '?';
		break;
	}

	string[2] = '\0';

	strncpy(buf,string,STEPINGSTRSIZE);
}


