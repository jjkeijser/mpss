/*
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
