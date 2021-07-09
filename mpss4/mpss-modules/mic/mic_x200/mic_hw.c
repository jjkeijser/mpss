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

#include <linux/fs.h>
#include <linux/pci.h>
#include <linux/sched.h>
#include <linux/firmware.h>
#include <linux/completion.h>
#include <linux/delay.h>

#include "../common/mic_dev.h"
#include "mic_device.h"
#include "mic_hw.h"

static int mic_check_pci_aperture_len(struct mic_device *xdev,
				      u64 length, u64 offset)
{
	if (xdev->aper.len < xdev->bootaddr + offset + length) {
		dev_err(&xdev->pdev->dev,
			"PCI aperture is too small aper.len=%llu bootaddr=%llu"
			"length=%llu offset=%llu",
			xdev->aper.len, xdev->bootaddr, length, offset);

		return -EINVAL;
	}

	return 0;
}

/*
 * mic_rid_lut_offset - calculate mmio offset of given RID-LUT entry
 */
u32 mic_rid_lut_offset(bool link_side, int index)
{
	u32 base_reg;

	if (link_side)
		base_reg = MIC_LINK_EP_OFFSET;
	else
		base_reg = MIC_VIRT_EP_OFFSET;

	return base_reg + MIC_RID_LUT + (2 * index);
}

u16 mic_read_rid_lut(struct mic_device *xdev, bool link_side, int index)
{
	return ioread16(xdev->mmio.va + mic_rid_lut_offset(link_side, index));
}

/*
 * mic_write_rid_lut_entry - construct and write RID LUT value
 *
 * @xdev: pointer to mic_device instance
 * @bus: bus number of device
 * @dev: device number of device
 * @idx: index of rid_lut entry
 */
static void
mic_write_rid_lut(struct mic_device *xdev, bool link_side, int index, u8 bus,
		  u8 dev)
{
	u16 val = (bus << 8) | ((dev & 0x1f) << 3) | MIC_RID_LUT_ENABLE;

	if (index >= MIC_MAX_RID_LUT_ENTRIES) {
		dev_err(&xdev->pdev->dev,
			"Can't add RID-LUT entry (%02x:%02x)\n", bus, dev);
		return;
	}
	iowrite16(val, xdev->mmio.va + mic_rid_lut_offset(link_side, index));
}

void mic_clear_rid_lut(struct mic_device *xdev)
{
	int i;

	for (i = 0; i < MIC_MAX_RID_LUT_ENTRIES; ++i) {
		iowrite16(0, xdev->mmio.va + mic_rid_lut_offset(false, i));
		iowrite16(0, xdev->mmio.va + mic_rid_lut_offset(true, i));
	}
}

static void write_link_lut(struct mic_device *xdev, int index, u8 bus, u8 dev)
{
	mic_write_rid_lut(xdev, true, index, bus, dev);
}

static void write_virt_lut(struct mic_device *xdev, int index, u8 bus, u8 dev)
{
	mic_write_rid_lut(xdev, false, index, bus, dev);
}

static inline bool is_mic_x200(struct pci_dev *pdev)
{
	if (pdev->vendor != PCI_VENDOR_ID_INTEL)
		return false;

	switch (pdev->device) {
	case INTEL_PCI_DEVICE_2260:
	case INTEL_PCI_DEVICE_2262:
	case INTEL_PCI_DEVICE_2263:
		return true;
	default:
		return false;
	}
}

/*
 * mic_program_rid_lut - program RID LUT
 *
 * @xdev: pointer to mic_device instance
 * @pdev: The PCIe device
 *
 * RETURNS: 0 on success and errno on failure
 */
static int
mic_program_rid_lut(struct mic_device *xdev, struct pci_dev *pdev)
{
	struct pci_dev *mic_pdev = NULL;
	u16 rid;
	u16 real_rid;
	int n;

	/* RID-LUT is programmed only from the link/host side */
	if (!xdev->link_side)
		return 0;

	mic_clear_rid_lut(xdev);

	/*
	 * Program link/host side RID-LUT
	 */
	n = 0;

	/*
	 * Root complex RID required for host CPU writes.
	 */
	write_link_lut(xdev, n++, KNL_ROOT_COMPLEX_BUS, KNL_ROOT_COMPLEX_DEV);

	/*
	 * MIC_RID_READ_BACK contains the RID of the device that issued
	 * a read to this register (usually it's the root port),
	 * so add it to RID-LUT.
	 */
	real_rid = mic_mmio_read(&xdev->mmio, MIC_RID_READ_BACK);
	write_link_lut(xdev, n++, PCI_BUS_NUM(real_rid), PCI_SLOT(real_rid));

	/*
	 * KNL DMA RID required for DMA reads & writes. Use captured devids
	 * instead of kernel provided ones to get real HW ids under VM.
	 * RID-LUT table doesn't see any VM devices.
	 */
	rid = mic_mmio_read(&xdev->mmio,
			    MIC_LINK_DMA_OFFSET + MIC_CAPTURED_RID);
	write_link_lut(xdev, n++, MIC_CAPTURED_RID_BUS(rid),
		       MIC_CAPTURED_RID_DEV(rid));

	/*
	 * QPI RID required for cards connected to different CPUs (pre-SKL).
	 */
	write_link_lut(xdev, n++, MIC_QPI_LINK0_BUS, MIC_QPI_LINK0_DEVICE);

	/*
	 * In order to enable P2P we have to add other coprocessors connected
	 * to the system.
	 */
	while (true) {
		mic_pdev = pci_get_device(PCI_ANY_ID, PCI_ANY_ID, mic_pdev);
		if (!mic_pdev)
			break;

		if (pdev == mic_pdev)
			continue;

		if (is_mic_x200(mic_pdev)) {
			write_link_lut(xdev, n++, mic_pdev->bus->number,
				       KNL_P2P_ROOT_COMPLEX);
			write_link_lut(xdev, n++, mic_pdev->bus->number,
				       KNL_P2P_ROOT_PORT);
			write_link_lut(xdev, n++, mic_pdev->bus->number,
				       KNL_P2P_DMA);
			xdev->peer_count++;
			continue;
		}

		/*
		 * Also in order to enable CCL-proxy we have to add Infiniband
		 * controllers connected to the same CPU. mlx4_0 devices show
		 * as PCI_CLASS_NETWORK_OTHER or PCI_CLASS_INFINIBAND and
		 * mlx5_0 devices have PCI_CLASS_NETWORK_INFINIBAND.
		 */
		if (mic_pdev->class == PCI_CLASS_NETWORK_INFINIBAND << 8 ||
		    mic_pdev->class == PCI_CLASS_NETWORK_OTHER << 8 ||
		    mic_pdev->class == PCI_CLASS_INFINIBAND << 8) {
			write_link_lut(xdev, n++, mic_pdev->bus->number, 0);
			xdev->net_peer_count++;
		}
	}

	/*
	 * Program virtual/card side RID-LUT
	 */
	n = 0;

	/*
	 * KNL root complex bridge RID required for KNL CPU writes.
	 */
	write_virt_lut(xdev, n++, KNL_ROOT_COMPLEX_BUS, KNL_ROOT_COMPLEX_DEV);

	/*
	 * KNL root port required for KNL CPU reads.
	 */
	write_virt_lut(xdev, n++, KNL_ROOT_PORT_BUS, KNL_ROOT_PORT_DEV);

	/*
	 * KNL DMA RID required for KNL DMA reads & writes.
	 */
	write_virt_lut(xdev, n++, KNL_DMA_BUS, KNL_DMA_DEV);

	return 0;
}

/*
 * mic_hw_init - Initialize any hardware specific information
 *
 * @xdev: pointer to mic_device instance
 * @pdev: The PCIe device
 *
 * RETURNS: 0 on success and errno on failure
 */
int mic_hw_init(struct mic_device *xdev, struct pci_dev *pdev)
{
	u32 val;
	int rc;

	/*
	 * Failing to read MIC_PORT_ID means we are on PCI bus (not PCIe),
	 * which is only possible on VM, so assume we are on the link side.
	 */
	if (pci_read_config_dword(pdev, MIC_PORT_ID, &val))
		xdev->link_side = 1;
	else
		xdev->link_side = !!(val & (1 << 31));

	if (xdev->link_side) {
		xdev->reg_base = MIC_LINK_EP_OFFSET;
		xdev->intr_reg_base = 0x10;
	} else {
		xdev->reg_base = MIC_VIRT_EP_OFFSET;
		xdev->peer_intr_reg_base = 0x10;
	}

	rc = mic_program_rid_lut(xdev, pdev);
	if (rc) {
		log_mic_err(xdev->id, "can't program RID LUT: %d", rc);
		return rc;
	}

	log_mic_dbg(xdev->id, "link_side %d reg_base 0x%x", xdev->link_side,
		    xdev->reg_base);
	return rc;
}

/**
 * mic_write_spad() - write to the scratchpad register
 * @xdev: pointer to mic_device instance
 * @idx: index to the scratchpad register, 0 based
 * @val: the data value to put into the register
 *
 * This function allows writing of a 32bit value to the indexed scratchpad
 * register.
 *
 * RETURNS: none.
 */
void mic_write_spad(struct mic_device *xdev, unsigned int idx, u32 val)
{
	mic_mmio_write(&xdev->mmio, val,
		       xdev->reg_base + MIC_SPAD0 + idx * 4);
}

/**
 * mic_read_spad() - read from the scratchpad register
 * @xdev: pointer to mic_device instance
 * @idx: index to scratchpad register, 0 based
 *
 * This function allows reading of the 32bit scratchpad register.
 *
 * RETURNS: An appropriate -ERRNO error value on error, or zero for success.
 */
u32 mic_read_spad(struct mic_device *xdev, unsigned int idx)
{
	u32 val = mic_mmio_read(&xdev->mmio,
				xdev->reg_base + MIC_SPAD0 + idx * 4);

	return val;
}

/**
 * mic_set_postcode() - write postcode to the scratchpad register
 * @xdev: pointer to mic_device instance
 * @postcode: new postcode to put into the register
 *
 * This function allows writing a new 8bit postcode to the scratchpad
 * postcode register.
 *
 * RETURNS: none.
 */
void mic_set_postcode(struct mic_device *xdev, u8 postcode)
{
	u32 spad_val = (postcode << 24);

	dev_info(&xdev->pdev->dev, "Changing post code to %#04x\n", postcode);
	mic_write_spad(xdev, MIC_SPAD_PROGRESS_CODE, spad_val);
}

/*
 * mic_reset_fw_status - Reset firmware status fields.
 * @xdev: pointer to mic_device instance
 */
void mic_reset_fw_status(struct mic_device *xdev)
{
	unsigned long val;

	/* Clear download ready bit */
	val = mic_read_spad(xdev, MIC_SPAD_COMMAND);
	clear_bit(KNL_DOWNLOAD_READY_BIT_NR, &val);
	mic_write_spad(xdev, MIC_SPAD_COMMAND, val);

	/* Clear buffer ready bit */
	val = mic_read_spad(xdev, MIC_SPAD_BUFFER_READY);
	clear_bit(KNL_BUFFER_READY_BIT_NR, &val);
	mic_write_spad(xdev, MIC_SPAD_BUFFER_READY, val);
}

/*
 * mic_is_fw_ready - Check if firmware is ready.
 * @xdev: pointer to mic_device instance
 */
bool mic_is_fw_ready(struct mic_device *xdev)
{
	unsigned long val = mic_read_spad(xdev, MIC_SPAD_BUFFER_READY);
	int is_ready = test_bit(KNL_BUFFER_READY_BIT_NR, &val);

	dev_dbg(&xdev->pdev->dev,
		"Reading buf_rdy=0x%lx bios_rdy=%s\n",
		val, is_ready ? "yes" : "no");
	return !!is_ready;
}

/**
 * mic_hw_sent_gpio_signal - Sent signal to GPIO<1> or GPIO<2>
 * @xdev: pointer to mic_device instance
 * @gpio_type: type of GPIO pin
 * @signal_length: pulse length, which is sending to GPIO pin
 *
 * To reset the card (using GPIO<1> or GPIO<2>) generated pulse
 * must be greater than 50ms and less than 4s.
 */
void mic_hw_send_gpio_signal(struct mic_device *xdev, int gpio_type, int signal_length)
{
	u32 out_reg;
	out_reg = mic_mmio_read(&xdev->mmio, MIC_GPIO_OUT_REG);

	mic_mmio_write(&xdev->mmio, out_reg & ~gpio_type, MIC_GPIO_OUT_REG);

	msleep(signal_length);
	mic_mmio_write(&xdev->mmio, out_reg | gpio_type, MIC_GPIO_OUT_REG);
	dev_dbg(&xdev->pdev->dev, "send signal to GPIO<%x> %d\n", gpio_type, signal_length);
}

/**
 * mic_enable_interrupts - Enable interrupts.
 * @xdev: pointer to mic_device instance
 */
void mic_enable_interrupts(struct mic_device *xdev)
{
	u32 offset = xdev->reg_base + xdev->intr_reg_base
			+ MIC_DBIMC;
	mic_mmio_write(&xdev->mmio, 0xFFFF, offset);
}

/**
 * mic_disable_interrupts - Disable interrupts.
 * @xdev: pointer to mic_device instance
 */
void mic_disable_interrupts(struct mic_device *xdev)
{
	u32 offset = xdev->reg_base + xdev->intr_reg_base
			+ MIC_DBIMS;
	mic_mmio_write(&xdev->mmio, 0xFFFF, offset);
}

/**
 * mic_set_download_ready - Set download ready status field.
 * @xdev: pointer to mic_device instance
 */
void mic_set_download_ready(struct mic_device *xdev)
{
	unsigned long val = mic_read_spad(xdev, MIC_SPAD_COMMAND);

	set_bit(KNL_DOWNLOAD_READY_BIT_NR, &val);
	mic_write_spad(xdev, MIC_SPAD_COMMAND, val);
}

/**
 * __mic_send_intr - Send interrupt to MIC.
 * @xdev: pointer to mic_device instance
 * @doorbell: doorbell number.
 */
void mic_send_intr(struct mic_device *xdev, int doorbell)
{
	u32 offset = xdev->reg_base + xdev->peer_intr_reg_base
			+ MIC_DBIS;

	mic_mmio_write(&xdev->mmio, DB_TO_MASK(doorbell), offset);
}

/**
 * __mic_send_p2p_intr - Send interrupt from one MIC peer to other peer.
 * @xdev: pointer to mic_device instance
 * @doorbell: doorbell number.
 * @mw: MMIO register base virtual address.
 */
void mic_send_p2p_intr(struct mic_device *xdev, int doorbell,
		struct mic_mw *mw)
{
	u32 offset = xdev->reg_base + MIC_DBIS;

	mic_mmio_write(mw, DB_TO_MASK(doorbell), offset);
}
/**
 * mic_ack_interrupt - Device specific interrupt handling.
 * @xdev: pointer to mic_device instance
 *
 * Returns: bitmask of doorbell events triggered.
 */
u32 mic_ack_interrupt(struct mic_device *xdev)
{
	u32 offset = xdev->reg_base + xdev->intr_reg_base
			+ MIC_DBIC;
	u32 reg;

	mic_disable_interrupts(xdev);
	reg = mic_mmio_read(&xdev->mmio, offset);
	mic_mmio_write(&xdev->mmio, reg, offset);
	mic_enable_interrupts(xdev);
	return reg;
}

/**
 * mic_hw_intr_init() - Initialize h/w specific interrupt
 * information.
 * @xdev: pointer to mic_device instance
 */
void mic_intr_init(struct mic_device *xdev)
{
	xdev->intr_info = (struct mic_intr_info *)_mic_intr_init;
}

/**
 * mic_get_fw_addr() - Get firmware address.
 * @xdev: pointer to mic_device instance
 *
 * Returns: firmware address
 */
u64 mic_get_fw_addr(struct mic_device *xdev)
{
	u32 val;
	u64 fw_addr;

	/* get spad buffer ready */
	val = mic_read_spad(xdev, MIC_SPAD_BUFFER_READY);

	/* bit 31:12 -- BAR offset in MB units */
	fw_addr = ((u64)(val & 0xfffff000)) << 8;

	log_mic_info(xdev->id, "buf_rdy=0x%x dl_addr=0x%llx", val, fw_addr);

	return fw_addr;
}

/**
 * mic_load_cmdline() - Load cmdline to MIC.
 * @xdev: pointer to mic_device instance
 * @hdr: pointer to mic_efi_boot_header instance
 * @offset: offset to load image
 *
 * RETURNS: An appropriate -ERRNO error value on error, or zero for success.
 */
static int mic_load_cmdline(struct mic_device *xdev,
				struct mic_efi_boot_header *hdr, u64 offset)
{
	u32 boot_mem;
	u32 len = 0;
	char *buf;
	const char *kernel_cmdline;
	int rc;
#define CMDLINE_SIZE 2048

	boot_mem = xdev->aper.len >> 20;
	buf = kzalloc(CMDLINE_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	len = snprintf(buf, CMDLINE_SIZE - len, " mem=%dM mic_console=%llx,%X",
		boot_mem, xdev->cosm_dev->log_buf_dma_addr,
		xdev->cosm_dev->log_buf_size);

	mutex_lock(&xdev->cosm_dev->config_mutex);
	kernel_cmdline = xdev->cosm_dev->config.kernel_cmdline;
	if (kernel_cmdline)
		snprintf(buf + len, CMDLINE_SIZE - len, " %s", kernel_cmdline);
	mutex_unlock(&xdev->cosm_dev->config_mutex);

	hdr->cmdline_size = strlen(buf) + 1;

	rc = mic_check_pci_aperture_len(xdev, offset, hdr->cmdline_size);
	if (!rc)
		memcpy_toio(xdev->aper.va + xdev->bootaddr + offset, buf,
			    hdr->cmdline_size);
	kfree(buf);

	return rc;
}

/**
 * _mic_request_firmware - Wrapper for request_firmware.
 * @firmware_p: pointer to firmware image
 * @name: name of firmware file
 * @device: device for which firmware is being loaded
 *
 * Kernel versions < 3.18 do not check if firmware name is null.
 * Make sure name is not null before calling request_firmware().
 */
static int _mic_request_firmware(const struct firmware **firmware_p,
				 const char *name, struct device *device)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 18, 0)
	if (!name || name[0] == '\0')
		return -EINVAL;
#endif
	return request_firmware(firmware_p, name, device);
}

/**
 * mic_load_firmware() - Load firmware from file.
 * @xdev: pointer to mic_device instance
 * @fw: pointer to firmware image
 * @filename: image filename
 * @offset: offset to load image
 * @fw_size: size of loaded image
 */
static int mic_load_firmware(struct mic_device *xdev,
			     const struct firmware **fw, const char *filename,
			     u64 offset, u32 *fw_size)
{
	int rc = _mic_request_firmware(fw, filename, &xdev->pdev->dev);
	if (rc < 0) {
		dev_err(&xdev->pdev->dev, "request_firmware failed: %d %s\n",
			rc, filename);
		return rc;
	}

	rc = mic_check_pci_aperture_len(xdev, offset, (*fw)->size);
	if (!rc) {
		memcpy_toio(xdev->aper.va + xdev->bootaddr + offset,
			    (*fw)->data, (*fw)->size);
		*fw_size = PAGE_ALIGN((*fw)->size);
	}

	dev_dbg(&xdev->pdev->dev, "Loaded firmware: %s offset %llu size %u\n",
		filename, offset, *fw_size);

	release_firmware(*fw);
	return rc;
}

/**
 * mic_load_fw() - Load firmware to MIC.
 * @xdev: pointer to mic_device instance
 *
 * Before the firmware is downloaded to the card it is examined
 * to see if it is a bzImage or an efiimage (EFI,FAT32,ISO).
 *
 * If it is an efiimage then only the efiimage is downloaded.
 *
 * If, however, it is a bzImage then the remaining elements
 * are downloaded to the card (see mic_hw.h for diagram).
 *
 * RETURNS: An appropriate -ERRNO error value on error, or zero for success.
 */
int mic_load_fw(struct mic_device *xdev, bool efi_only)
{
	int rc;
	u64 offset;
	struct mic_efi_boot_header hdr;
	const struct firmware *fw;

	memset(&hdr, 0, sizeof(hdr));
	hdr.magic_sig = MIC_EFI_BOOT_HDR_MAGIC;
	hdr.pad_size = MIC_EFI_BOOT_PADDING;
	xdev->bootaddr = mic_get_fw_addr(xdev);

	/* Load the EFI image to the mic memory. */
	mutex_lock(&xdev->cosm_dev->config_mutex);
	rc = mic_load_firmware(xdev, &fw, xdev->cosm_dev->config.efi_image,
			       0, &hdr.efiimage_size);
	mutex_unlock(&xdev->cosm_dev->config_mutex);
	if (rc)
		return rc;

	/* Skip loading firmware required only to the CoprocessorOS boot. */
	if (efi_only) {
		log_mic_info(xdev->id, "skip loading OS specific firmware");
		goto exit;
	}

	/* Load the kernel image to the mic memory. */
	offset = hdr.efiimage_size + hdr.pad_size +
		 MIC_EFI_BOOT_HDR_SIZE;
	mutex_lock(&xdev->cosm_dev->config_mutex);
	rc = mic_load_firmware(xdev, &fw, xdev->cosm_dev->config.kernel_image,
			       offset, &hdr.kernel_size);
	mutex_unlock(&xdev->cosm_dev->config_mutex);
	if (rc)
		return rc;

	/* Load the InitRamFs image to the mic memory. */
	offset += hdr.kernel_size;
	mutex_lock(&xdev->cosm_dev->config_mutex);
	rc = mic_load_firmware(xdev, &fw,
			       xdev->cosm_dev->config.initramfs_image,
			       offset, &hdr.ramdisk_size);
	mutex_unlock(&xdev->cosm_dev->config_mutex);
	if (rc)
		return rc;

	/* Load the kernel command line to the mic memory. */
	mic_load_cmdline(xdev, &hdr, offset + hdr.ramdisk_size);

	/* Load the boot header to the mic memory. */
	offset = hdr.efiimage_size + hdr.pad_size;

	rc = mic_check_pci_aperture_len(xdev, sizeof(hdr), offset);
	if (!rc)
		memcpy_toio(xdev->aper.va + xdev->bootaddr + offset,
			    &hdr, sizeof(hdr));
	else
		return rc;

	/*
	 * FIXME: Until we have BIOS support use SPAD3(DDR_SIZE)
	 *        for passing header offset to card.
	 *        Mike H. confirmed that DDR_SIZE is not used anywhere
	 *        in the linux/windows driver so we don't need to worry
	 *        about restoring it.
	 */
	mic_write_spad(xdev, MIC_SPAD_DDR_SIZE, offset);

exit:
	/*
	 * This barrier ensures that all copy operations to the mic memory
	 * have been completed.
	 */
	wmb();

	return 0;
}

/**
 * mic_dma_filter - DMA filter function
 * @chan: The DMA channel to compare with
 * @param: Data passed from DMA engine
 *
 * Returns: true if DMA device matches the PCIe device and false otherwise.
 */
bool mic_dma_filter(struct dma_chan *chan, void *param)
{
	struct pci_dev *pdev = param;
	struct pci_dev *dma_pdev =
			container_of(chan->device->dev, struct pci_dev, dev);

	/*
	 * DMA and NTB are connected directly to the same bus (host and VM)
	 */
	if ((dma_pdev->bus == pdev->bus) &&
	    (dma_pdev->slot == pdev->slot) &&
	    PCI_FUNC(dma_pdev->devfn) == PCI_FUNC(pdev->devfn) + 1)
		return true;

	/*
	 * NTB is located on the same bus hierarchy as DMA device (card)
	 */
	while ((pdev = pdev->bus->self)) {
		if (dma_pdev->bus->self == pdev)
			return true;
	}
	return false;
}

/**
 * mic_alut_set - Update an ALUT entry with a DMA address.
 * @xdev: pointer to mic_device instance
 *
 * RETURNS: none.
 */
void mic_alut_set(struct mic_device *xdev, dma_addr_t dma_addr, u8 index)
{
	uint32_t lower_addr = dma_addr;
	uint32_t upper_addr = dma_addr >> 32;
	uint32_t offset = ((index % 128) * 4) +
		((index / 128) * ALUT_ARRAY_OFFSET);

	/**
	 * UPPER_REMAP register holds bits 63:32
	 * LOWER_REMAP register holds bits 31:0
	 * PERMISSIONS register should be set to 0b11 (read/write)
	 */
	mic_mmio_write(&xdev->mmio, lower_addr,
		ALUT_ARRAY_BASE + ALUT_LOWER_REMAP_OFFSET +
		offset);
	mic_mmio_write(&xdev->mmio, upper_addr,
		ALUT_ARRAY_BASE + ALUT_UPPER_REMAP_OFFSET +
		offset);
	mic_mmio_write(&xdev->mmio, ALUT_ENTRY_READ_ENABLE |
		ALUT_ENTRY_WRITE_ENABLE, ALUT_ARRAY_BASE +
		ALUT_PERMISSIONS_OFFSET + offset);
}

/**
 * mic_alut_get - read an ALUT entry with a given index.
 * @xdev: pointer to mic_device instance
 * @index: index of entry to read
 * RETURNS: mic_alut struct containing read values
 */
struct mic_alut mic_alut_get(struct mic_device *xdev, u8 index)
{
	struct mic_alut entry;
	uint32_t lower_addr, upper_addr;
	uint32_t offset = ((index % 128) * 4) +
		((index / 128) * ALUT_ARRAY_OFFSET);

	/**
	 * UPPER_REMAP register holds bits 63:32
	 * LOWER_REMAP register holds bits 31:0
	 * PERMISSIONS register should be set to 0b11 (read/write)
	 */
	lower_addr = mic_mmio_read(&xdev->mmio,
			     ALUT_ARRAY_BASE + ALUT_LOWER_REMAP_OFFSET
			     + offset);
	upper_addr = mic_mmio_read(&xdev->mmio,
			     ALUT_ARRAY_BASE + ALUT_UPPER_REMAP_OFFSET
			     + offset);
	entry.dma_addr = ((uint64_t)upper_addr << 32) + lower_addr;

	entry.perm = mic_mmio_read(&xdev->mmio,
			 ALUT_ARRAY_BASE + ALUT_PERMISSIONS_OFFSET
			 + offset);

	entry.ref_count = 0;

	return entry;
}

u8 mic_read_post_code(struct mic_device *xdev)
{
	u32 val = mic_read_spad(xdev, MIC_SPAD_PROGRESS_CODE);

	return (u8)(val >> 24);
}

/**
 * mic_in_S5 - Check if MIC is in S5.
 * @xdev: pointer to mic_device instance
 *
 * This function checks post code to verify if MIC is in S5.
 * RETURNS: true if MIC is in S5 and false if not.
 */
bool mic_in_S5(struct mic_device *xdev)
{
	return (mic_read_post_code(xdev) == MIC_X200_POST_CODE_SHUTDOWN);
}
