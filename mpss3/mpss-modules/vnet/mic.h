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

#ifndef MICDLDR_H
#define MICDLDR_H

#define MIC_DECONS_DISABLE	0
#define MIC_DECONS_ENABLE	1

typedef struct mic_upload {
	int	up_brdnum;
	int	up_uossize;
	char	*up_uosbuf;
	int	up_dcons;
	int	up_uoslog;
	int	up_uosreserve;
} mic_upload_t;

typedef struct mic_sys_config {
	int	sc_numCards;
} mic_sys_config_t;

#define UOS_NOT_BOOTED		0
#define	UOS_BOOTING		1
#define	UOS_BOOT_FAILED		2
#define	UOS_BOOT_SUCCEED	3
#define	UOS_RUNNING		4
#define	UOS_WEDGED		5
#define UOS_UNKNOWN		6

#define PCI_VENDOR_INTEL	0x8086

#define PCI_SPEED_GEN1		1
#define PCI_SPEED_GEN2		2

#define GDDR_VENDOR_SAMSUNG	1
#define GDDR_VENDOR_QIMONDA	2
#define GDDR_VENDOR_HYNIX	6

#define GDDR_DENSITY_512MB	0
#define GDDR_DENSITY_1GB	1

typedef struct mic_brd_config {
	int		bc_brdnum;
	struct {
		char		step[4];
		int		freqMhz;
		int		vid;
		int		uvolts;
	} bc_core;
	struct {
		unsigned short	vendor;
		unsigned short	device;
		unsigned int	class;
		char		capableSpeed;
		char		capableWidth;
		char		currentSpeed;
		char		currentWidth;
	} bc_pcie;
	struct {
		char	vendor;
		char	density;
		char	fifoDepth;
		short	freq;		// MT/sec
		int	size;		// Mbytes
	} bc_gddr;
	int		bc_uOSstate;
} mic_brd_config_t;

#define MIC_UPLOAD_UOS	_IOWR('l', 1, struct mic_upload) 
#define MIC_RESET_UOS	_IOWR('l', 2, int) 
#define MIC_SYS_CONFIG	_IOWR('l', 3, struct mic_sys_config) 
#define MIC_BRD_CONFIG	_IOWR('l', 4, struct mic_brd_config) 

#endif // MICDLDR_H

