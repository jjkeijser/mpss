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

/* "Raw" register offsets & bit specifications for MIC */
#ifndef _MIC_MICBASEDEFINE_REGISTERS_H_
#define _MIC_MICBASEDEFINE_REGISTERS_H_

#define COMMON_MMIO_BOX_SIZE		(1<<16)

/* CBOX register base defines */
#define CBOX_BASE		0x0000000000ULL

/* TXS register base defines */
#define TXS0_BASE		0x0800780000ULL
#define TXS1_BASE		0x0800770000ULL
#define TXS2_BASE		0x0800760000ULL
#define TXS3_BASE		0x0800750000ULL
#define TXS4_BASE		0x0800740000ULL
#define TXS5_BASE		0x0800730000ULL
#define TXS6_BASE		0x0800720000ULL
#define TXS7_BASE		0x0800710000ULL
#define TXS8_BASE		0x08006E0000ULL

/* GBOX register base defines */
#define GBOX0_BASE		0x08007A0000ULL
#define GBOX1_BASE		0x0800790000ULL
#define GBOX2_BASE		0x0800700000ULL
#define GBOX3_BASE		0x08006F0000ULL

#define GBOX_CHANNEL0_BASE	0x00000000
#define GBOX_CHANNEL1_BASE	0x00000800
#define GBOX_CHANNEL2_BASE	0x00001000

/* VBOX register base defines */
#define VBOX_BASE		0x08007B0000ULL

/* DBOX register base defines */
#define DBOX_BASE		0x08007C0000ULL

/* SBOX register base defines */
#define SBOX_BASE		0x08007D0000ULL

#define MIC_GTT_BASE		0x0800800000ULL
#define MIC_GTT_TOP		0x080083FFFFULL
#define MIC_GTT_SIZE		(MIC_GTT_TOP - MIC_GTT_BASE + 1)

/*	Aperture defines */
#define MIC_APERTURE_BASE		0x0900000000ULL
#define MIC_APERTURE_TOP		0x090FFFFFFFULL
#define MIC_APERTURE_SIZE		(MIC_APERTURE_TOP - MIC_APERTURE_BASE + 1)

/*	SPI flash defines */
#define MIC_SPI_BOOTLOADER_BASE		0x0FFFFF0000ULL
#define MIC_SPI_BOOTLOADER_TOP		0x0FFFFFFFFFULL
#define MIC_SPI_BOOTLOADER_SIZE		(MIC_SPI_BOOTLOADER_TOP - MIC_SPI_BOOTLOADER_BASE + 1)
#define MIC_SPI_2ND_STAGE_BASE		0x0FFFFE0000ULL
#define MIC_SPI_2ND_STAGE_TOP		0x0FFFFEFFFFULL
#define MIC_SPI_2ND_STAGE_SIZE		(MIC_SPI_2ND_STAGE_TOP - MIC_SPI_2ND_STAGE_BASE + 1)
#define MIC_SPI_PARAMETER_BASE		0x0FFFFDC000ULL
#define MIC_SPI_PARAMETER_TOP		0x0FFFFDFFFFULL
#define MIC_SPI_PARAMETER_SIZE		(MIC_SPI_PARAMETER_TOP - MIC_SPI_PARAMETER_BASE + 1)

/*	remote defines */
#define MIC_REMOTE_BASE		0x1000000000ULL
#define MIC_REMOTE_TOP		0x7FFFFFFFFFULL
#define MIC_REMOTE_SIZE		(MIC_REMOTE_TOP - MIC_REMOTE_BASE + 1)

/*	system defines */
#define MIC_SYSTEM_BASE		0x8000000000ULL
#define MIC_SYSTEM_TOP		0xFFFFFFFFFFULL
#define MIC_SYSTEM_PAGE_SIZE	0x0400000000ULL
#define MIC_SYSTEM_SIZE		(MIC_SYSTEM_TOP - MIC_SYSTEM_BASE + 1)

#define MIC_PHYSICAL_ADDRESS_BITS	 40
#define MIC_PHYSICAL_ADDRESS_SPACE_SIZE	 ( 1ULL << MIC_PHYSICAL_ADDRESS_BITS )

#define MIC_HOST_MMIO_BASE		 DBOX_BASE

#endif
