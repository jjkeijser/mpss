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

#pragma once

#include "mpssconfig.h"

#include <sys/types.h>

void notify_systemd();

ssize_t get_dir_size(const char *micname, char *dirpath);
const char* get_virtio_device_name(int type);


void mpsslog(int level, const char *format, ...) __attribute__((format(printf, 2, 3)));
void mpsslog_raw(int level, const char *format, ...) __attribute__((format(printf, 2, 3)));

#define mpssd_log(level, fmt, ...) \
	mpsslog(level, "<%s:%u> " fmt "\n", __func__, __LINE__, ##__VA_ARGS__)

void mpssd_elist_log(mpss_elist& logs, void (*func)(int level, const char* fmt, ...));

void set_thread_name(const char* dev, const char* name);

void display_modules_version();
