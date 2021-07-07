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

/* Contains common definitions for Windows and Linux IO Interface */

#ifndef __IO_INTERFACE_H__
#define __IO_INTERFACE_H__

/*
 * The host driver exports sysfs entries in
 *	/sys/class/mic/micX/
 * The "/sys/class/mic/micX/state" entry reflects the state of the
 * card as it transitions from hardware reset through booting an image
 *
 * All the other entries have valid values when the state entry is either
 * "ready" or "online"
 */

/*
 * -----------------------------------------
 * IOCTL interface information
 * -----------------------------------------
 */

#define IOCTL_FLASHCMD		_IOWR('c', 5, struct ctrlioctl_flashcmd *)
#define IOCTL_CARDMEMCPY	_IOWR('c', 8, struct ctrlioctl_cardmemcpy *)

typedef enum _product_knc_stepping_t
{
	KNC_A_STEP,
	KNC_B0_STEP,
	KNC_C_STEP,
	KNC_B1_STEP
} product_knc_stepping_t;

typedef enum {
	FLASH_CMD_ABORT,
	FLASH_CMD_READ,
	FLASH_CMD_WRITE,
	FLASH_CMD_VERSION,
	RAS_CMD,
	RAS_CMD_INJECT_REPAIR,
	RAS_CMD_CORE_DISABLE,
	RAS_CMD_CORE_ENABLE,
	RAS_CMD_ECC_DISABLE = 0xD,
	RAS_CMD_ECC_ENABLE = 0xE,
	RAS_CMD_EXIT = 0xF,
	/* Driver only commands that are not passed to RASMM */
	FLASH_CMD_READ_DATA,
	FLASH_CMD_STATUS,
} MIC_FLASH_CMD_TYPE;

/**
 * struct ctrlioctl_flashcmd:
 *
 * \param brdnum			board for which IOCLT is requested
 * \param type				arguments needed for the uos escape call
 * \param data				size of escape arguments
 * \param len				uos escape opecode
 *
 * This structure is used for IOCTL_FLASHCMD.
 *
 * This IOCTL can only be issued when /sys/class/mic/mic0/state returns "online"
 * after it has been set to "boot:flash"
 */
struct ctrlioctl_flashcmd {
	uint32_t brdnum;
	MIC_FLASH_CMD_TYPE type;
	void *data;
	uint32_t len;
};


/*
 * IN/OUT structure used by MIC_FLASH_CMD_TYPE FLASH_CMD_VERSION
 * This structure is passed in as data in above command
 */
#define MAX_FLASH_VER_STRLEN 16
struct version_struct {
	uint16_t	hdr_ver;
	uint16_t	odm_ver;//revision for ODM change for flash
	uint64_t	upd_time_bcd;
	uint8_t		upd_ver[MAX_FLASH_VER_STRLEN]; // 16 bytes for flash version
	uint64_t	mfg_time_bcd;
	uint8_t		mfg_ver[MAX_FLASH_VER_STRLEN]; // 16 bytes for flash version
};

/*
 * status values returned in MIC_FLASH_CMD_TYPE FLASH_CMD_STATUS
 */
typedef enum {
	FLASH_IDLE,
	FLASH_CMD_IN_PROGRESS,
	FLASH_CMD_COMPLETED,
	FLASH_CMD_FAILED,
	FLASH_CMD_AUTH_FAILED,
	FLASH_SMC_CMD_IN_PROGRESS,
	FLASH_SMC_CMD_COMPLETE,
	FLASH_SMC_CMD_FAILED,
	FLASH_SMC_CMD_AUTH_FAILED,
	FLASH_CMD_INVALID = 0xF,
} MIC_FLASH_STATUS;

struct flash_stat {
	MIC_FLASH_STATUS status;
	uint32_t percent;
	uint32_t smc_status;
	uint32_t cmd_data;
	uint32_t mm_debug;
};

typedef enum {
	DBOX,
	SBOX,
} MMIO_REGISTER_TYPE;

/**
 * struct ctrlioctl_cardmemcpy:
 *
 * \param brdnum			board for which IOCLT is requested
 * \param start		        card side physical address from which the copy will start
 * \param size		        offset of the register from data is to be read
 * \param dest	            user buffer in which data is to be copied
 *
 * This structure is used for IOCTL_MMIOREAD.
 */
struct ctrlioctl_cardmemcpy {
	uint32_t brdnum;
	uint64_t start;
	uint64_t size;
	void *dest;
};

/*
 * FIXME:: All the typedefines and structures below and their references need
 * to be cleaned up from the driver code
 *---------------------------------------------------------------------------
 */

typedef enum _product_family_t
{
	FAMILY_UNKNOWN = 0,
	FAMILY_ABR,
	FAMILY_KNC
} product_family_t;

typedef enum {
	USAGE_MODE_NORMAL = 0,
	USAGE_MODE_MAINTENANCE,
	USAGE_MODE_ZOMBIE,
	USAGE_MODE_MEMDIAG,
	USAGE_MODE_NORMAL_RESTRICTED,
	USAGE_MODE_NOP,
	USAGE_MODE_MAX,

} CARD_USAGE_MODE;

/*
 * SBOX register definitions
 * TODO: Remove the bit fields and replace them with bitwise operators
 */
typedef union sbox_scratch1_reg {
	uint32_t value;
	struct  {
		uint32_t percent : 7;
		uint32_t status : 4;
		uint32_t command : 4;
		uint32_t smc_status : 4;
		uint32_t reserved : 5;
		uint32_t cmd_data : 7;
		uint32_t mm_debug : 1;
	} bits;
} sbox_scratch1_reg_t;

typedef union sbox_scratch2_reg {
	uint32_t value;
	struct  {
		uint32_t bootstrap_ready : 1;
		uint32_t bsp_apic_id : 9;
		uint32_t reserved : 2;
		uint32_t image_addr : 20;
	} bits;
} sbox_scratch2_reg_t;

#endif //!__IO_INTERFACE_H__
