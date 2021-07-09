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

#include <arpa/inet.h>
#include <linux/types.h>
#include <sys/types.h>
#include <string>

#pragma GCC diagnostic warning "-fpermissive"
#include <mic_common.h>
#pragma GCC diagnostic error "-fpermissive"

#ifndef MPSS_EXPORT
#define MPSS_EXPORT __attribute__((visibility("default")))
#endif

#define TRUE	1
#define FALSE	0
#define NOTSET	-1

typedef void (*log_func_t)(int level, const char* fmt, ...);

enum MIC_VERBOSE_MODE {
	MIC_VERBOSE_NONE = 0x0,
	MIC_VERBOSE_NORMAL = 0x1 << 1,
	MIC_VERBOSE_MIC_NAME = 0x1 << 2,
	MIC_VERBOSE_DEBUG = 0x1 << 3,
};

const std::string MIC_CMD_NONE_STR = "none";
const std::string MIC_CMD_BOOT_FIRMWARE_STR = "boot_firmware";
const std::string MIC_CMD_BOOT_STR = "boot";
const std::string MIC_CMD_RESET_STR = "reset";
const std::string MIC_CMD_RESET_FORCE_STR = "reset_force";
const std::string MIC_CMD_SHUTDOWN_STR = "shutdown";
const std::string MIC_CMD_SHUTDOWN_FORCE_STR = "shutdown_force";

MPSS_EXPORT void mic_set_verbose_mode(int mode);

MPSS_EXPORT int mic_state_to_id(const char* state);
MPSS_EXPORT std::string mic_read_sysfs_file(int fd);

MPSS_EXPORT int mic_get_state_id(struct mic_info* mic, log_func_t print);
MPSS_EXPORT int mic_boot(struct mic_info* mic, log_func_t print);
MPSS_EXPORT int mic_reset(struct mic_info* mic, int force, log_func_t print);
MPSS_EXPORT int mic_shutdown(struct mic_info* mic, int force, log_func_t print);

// From utilfuncs.c - Builds file name path based on mpss environment structure
MPSS_EXPORT int mpssut_filename(char *buffer, int maxlen, const char *format, ...) __attribute__((format(printf, 3, 4)));
MPSS_EXPORT std::string mpssut_configname(const std::string& name);

// From utilfuncs.c - Prints MIC kernel log
class mpss_elist;
MPSS_EXPORT int mpssut_print_card_log(struct mic_info *mic, mpss_elist& perrs);

// From utilsfuncs.c - Create and remove directory trees based on mpss environment structure
MPSS_EXPORT int mpssut_mksubtree(const char *dir, uid_t uid, gid_t gid, int mode);
MPSS_EXPORT int mpssut_deltree(const char *dir);
MPSS_EXPORT int mpssut_rename(const char *ifilename, const char *ofilename);

// From micenv.c - Get the lockfile used between mpssd and micctrl
// This will probably be moved to mpssconfig.h at some time.
MPSS_EXPORT int mpssenv_aquire_lockfile();

// From micenv.c - check the lockfile used between mpssd and micctrl
MPSS_EXPORT int mpssenv_check_lockfile();

/* align addr on a size boundary - adjust address up/down if needed */
#define _ALIGN_DOWN(addr, size)  ((addr)&(~((size)-1)))
#define _ALIGN_UP(addr, size)    _ALIGN_DOWN(addr + size - 1, size)

/* align addr on a size boundary - adjust address up if needed */
#define _ALIGN(addr, size)     _ALIGN_UP(addr, size)

/* to align the pointer to the (next) page boundary */
#define PAGE_ALIGN(addr)        _ALIGN(addr, PAGE_SIZE)

MPSS_EXPORT int load_mic_modules(void);
MPSS_EXPORT int unload_mic_modules(void);

MPSS_EXPORT int exec_command(const std::string& command);
MPSS_EXPORT int exec_command(const std::string& command, mpss_elist& output);

MPSS_EXPORT bool path_exists(const std::string& path);

struct in6_ifreq {
	struct in6_addr ifr6_addr;
	__u32           ifr6_prefixlen;
	int             ifr6_ifindex;
};
