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

#ifndef __MPSSTRANSFER_H_
#define __MPSSTRANSFER_H_
#include <sys/time.h>

#define BUFFERSIZE 2048

#define READY_FOR_START 1
#define SET_TIME	2
#define SET_TIME_ACK	3
#define FILE_START	4
#define FILE_ATTRS	5
#define FILE_BLOCK	6
#define FILE_BLOCK_ACK	7
#define FILE_END	8
#define FILE_END_ACK	9
#define TERM_REQ	10
#define TERM_ACK	11

struct time_pack {
	struct timeval	tod;
	struct timezone	tzone;
};

struct file_attrs {
	int size;
};

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

#endif /* __MPSSTRANSFER_H_ */
