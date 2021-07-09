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
#ifndef _MIC_HW_H_
#define _MIC_HW_H_

#define INTEL_PCI_DEVICE_2260 0x2260
#define INTEL_PCI_DEVICE_2262 0x2262
#define INTEL_PCI_DEVICE_2263 0x2263
#define INTEL_PCI_DEVICE_2264 0x2264
#define INTEL_PCI_DEVICE_2265 0x2265

#define INTEL_PCI_SUBSYSTEM_244 0x244

/* Infiniband subclass not yet defined in Linux */
#ifndef PCI_CLASS_NETWORK_INFINIBAND
#define PCI_CLASS_NETWORK_INFINIBAND 0x0207
#endif

/* MLX4_0 MT26428 not defined anywhere */
#ifndef PCI_CLASS_INFINIBAND
#define PCI_CLASS_INFINIBAND 0x0c06
#endif

/* start of MIC X200 register definitions */
#define MIC_MMIO_BAR 0
#define MIC_APER_BAR 2
#define MIC_LINK_EP_OFFSET 0x3F000
#define MIC_VIRT_EP_OFFSET 0x3E000
#define MIC_LINK_DMA_OFFSET 0x21000
#define MIC_VIRT_DMA_OFFSET 0x23000
#define MIC_PORT_ID 0xC8C
#define MIC_SPAD0 0xC6C
#define MIC_SPAD_DPLO 0
#define MIC_SPAD_DPHI 1
#define MIC_SPAD_MCDRAM_SIZE 2
#define MIC_SPAD_DDR_SIZE 3
#define MIC_SPAD_SMBIOS 4
#define MIC_SPAD_COMMAND 5
#define MIC_SPAD_BUFFER_READY 6
#define MIC_SPAD_PROGRESS_CODE 7
#define MIC_SPAD_POST_CODE 8
#define MIC_DBIS 0xC4C
#define MIC_DBIC 0xC50
#define MIC_DBIMS 0xC54
#define MIC_DBIMC 0xC58
#define MIC_GPIO_OUT_REG 0x624
#define MIC_RID_LUT 0xdb4
#define MIC_RID_READ_BACK 0xc90
#define MIC_CAPTURED_RID 0x1dc
#define MIC_CAPTURED_RID_BUS(x) (x & 0xff)
#define MIC_CAPTURED_RID_DEV(x) ((x >> 8) & 0x1f)
/* end of MIC X200  register definitions */

#define MIC_QPI_LINK0_BUS 0x0
#define MIC_QPI_LINK0_DEVICE 0x8
#define MIC_NUM_DOORBELL 16
#define MIC_DOORBELL_IDX_START 0
#define KNL_ROOT_PORT_BUS 0x0
#define KNL_ROOT_PORT_DEV 0x1
#define KNL_ROOT_COMPLEX_BUS 0x0
#define KNL_ROOT_COMPLEX_DEV 0x0
#define KNL_DMA_BUS 0x1
#define KNL_DMA_DEV 0x0
#define KNL_P2P_ROOT_COMPLEX	0x10
#define KNL_P2P_ROOT_PORT	0x11
#define KNL_P2P_DMA		0x12
#define MIC_RID_LUT_ENABLE 0x1
#define MIC_MAX_RID_LUT_ENTRIES 32
#define KNL_DOWNLOAD_READY_BIT_NR 0
#define KNL_BUFFER_READY_BIT_NR 0

#define MIC_MAXDB 16
#define DB_TO_MASK(n) ((u32)(0x1) << (n))

#define MIC_VIRTUAL_TO_LINK_ALUT_CONTROL 0x3fc94
#define ALUT_ARRAY_BASE 0x38000
#define ALUT_ARRAY_OFFSET 0x1000
#define ALUT_LOWER_REMAP_OFFSET 0x0
#define ALUT_UPPER_REMAP_OFFSET 0x400
#define ALUT_PERMISSIONS_OFFSET 0x800
#define ALUT_ENTRY_WRITE_ENABLE (1 << 0)
#define ALUT_ENTRY_READ_ENABLE  (1 << 1)
#define ALUT_ENTRY_COUNT 256
#define ALUT_ENTRY_SIZE (1ULL << 31)

#define MIC_ALUT_SIZE (ALUT_ENTRY_COUNT * ALUT_ENTRY_SIZE)
#define MIC_APER_SIZE (9 * ALUT_ENTRY_SIZE)

#define MIC_GPIO1 0x2
#define MIC_GPIO2 0x4
#define MIC_GPIO_SHORT 75
#define MIC_GPIO_LONG 8000

#define MIC_X200_POST_CODE_RESETTING 0x00
#define MIC_X200_POST_CODE_SHUTDOWN 0xB9
#define MIC_X200_POST_CODE_BIOS_READY 0x7D
#define MIC_X200_POST_CODE_EFI_READY 0xAD
#define MIC_X200_POST_CODE_FW_READY 0xE0
#define MIC_X200_POST_CODE_OS_READY 0xF0
#define MIC_X200_POST_CODE_MIC_DRIVER_READY 0xFF

static const u16 _mic_intr_init[] = {
	MIC_DOORBELL_IDX_START,
	MIC_NUM_DOORBELL,
};

/* MIC EFI boot header maximum size */
#define MIC_EFI_BOOT_HDR_SIZE 0x1000

/* Magic signature to validate boot header "INTC" */
#define MIC_EFI_BOOT_HDR_MAGIC 0x43544e49

/**
 * We need at least 128MB after media image otherwise
 * EFI runtime gets corrupt.
 */
#define MIC_EFI_BOOT_PADDING (128 * 1024 * 1024)

/**
 * struct mic_efi_boot_header - MIC EFI boot header.
 *
 * @magic_sig: Magic signature to validate boot header
 * @efiimage_size: Size of boot image (EFI FAT32 media image)
 * @pad_size: Padding after boot image
 * @firmware_size: Size of firmware (kernel)
 * @ramdisk_size: Size of ramdisk (initrd)
 * @cmdline_size: Size of kernel command line
 *
 *
 * -- Host --
 * The KNL host obtains the download address from SPAD6.
 * The following structure is setup and downloaded to that address.
 * The address of the boot header is placed in SPAD3 for the card.
 *
 * The KNL host then sets SPAD5 to signal the card to boot.
 *
 * -- Card --
 * When the card boots, the BIOS will mount the efiimage and
 * execute /EFI/BOOT/bootx64.efi (renamed from efilinux.efi).
 *
 * efilinux will read its arguments from /EFI/BOOT/efilinux.cfg.
 * This config file contains '-p' which places it in mic_mode.
 *
 * efilinux will then look at the boot header to determine the
 * location of the kernel, ramdisk, and command line arguments.
 * (Both the ramdisk and command line are optional).
 *
 * efilinux will also look at the boot header for a magic signature
 * and if it is valid, will proceed to boot. If, however, the
 * signature is not valid then efilinux will simply exit and the
 * EFI BIOS boot order will continue.
 *
 * efilinux will then setup the boot parameters and boot linux
 * (just like it does when booting from disk).
 *
 *  +-------------------+  <-- offset = SPAD6 (currently 4G)
 *  +  efiimage         +
 *  +-------------------+  <-- offset += efiimage_size
 *  +  Padding          +
 *  +-------------------+  <-- offset += MIC_EFI_BOOT_PADDING
 *  +  Boot Header      +
 *  +-------------------+  <-- offset += MIC_EFI_BOOT_HEADER_SIZE
 *  +  bzImage          +
 *  +-------------------+  <-- offset += kernel_size
 *  +  Ramdisk          +
 *  +-------------------+  <-- offset += ramdisk_size
 *  +  kernel cmd line  +
 *  +-------------------+  <-- ofset += cmdline_size
 *
 */
struct mic_efi_boot_header {
	u32 magic_sig;
	u32 efiimage_size;
	u32 pad_size;
	u32 kernel_size;
	u32 ramdisk_size;
	u32 cmdline_size;
};

int mic_hw_init(struct mic_device *xdev, struct pci_dev *pdev);
void mic_intr_init(struct mic_device *xdev);
void mic_write_spad(struct mic_device *xdev, unsigned int idx, u32 val);
u32 mic_read_spad(struct mic_device *xdev, unsigned int idx);
void mic_set_postcode(struct mic_device *xdev, u8 postcode);
void mic_reset_fw_status(struct mic_device *xdev);
void mic_set_download_ready(struct mic_device *xdev);
bool mic_is_fw_ready(struct mic_device *xdev);
void mic_hw_send_gpio_signal(struct mic_device *xdev, int gpio_type, int signal_length);
void mic_enable_interrupts(struct mic_device *xdev);
void mic_disable_interrupts(struct mic_device *xdev);
void mic_send_intr(struct mic_device *xdev, int doorbell);
void mic_send_p2p_intr(struct mic_device *xdev, int doorbell,
		struct mic_mw *mw);
u32 mic_ack_interrupt(struct mic_device *xdev);
u64 mic_get_fw_addr(struct mic_device *xdev);
int mic_load_fw(struct mic_device *xdev, bool efi_only);
bool mic_dma_filter(struct dma_chan *chan, void *param);
void mic_alut_set(struct mic_device *xdev, dma_addr_t dma_addr, u8 index);
struct mic_alut mic_alut_get(struct mic_device *xdev, u8 index);
bool mic_in_S5(struct mic_device *xdev);
u8 mic_read_post_code(struct mic_device *xdev);
u16 mic_read_rid_lut(struct mic_device *xdev, bool link_side, int index);
void mic_clear_rid_lut(struct mic_device *xdev);
#endif
