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

#include "utils.h"
#include "mpssd.h"

#include <dirent.h>
#include <dlfcn.h>
#include <linux/virtio_ids.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <time.h>

extern FILE *logfp;

void
notify_systemd()
{
	typedef int (*sd_notify)(int, const char *);

	void *handler = dlopen("libsystemd.so.0", RTLD_LAZY);
	if (!handler) {
		mpssd_log(PERROR, "cannot open libsystemd library");
	} else {
		sd_notify notify = (sd_notify)dlsym(handler, "sd_notify");
		if (notify) {
			notify(0, "READY=1");
		} else {
			mpssd_log(PERROR, "cannot open sd_notify function");
		}
		dlclose(handler);
	}
}

const char*
get_virtio_device_name(int type)
{
	switch (type) {
	case VIRTIO_ID_CONSOLE:
		return "console";
	case VIRTIO_ID_NET:
		return "net";
	case VIRTIO_ID_BLOCK:
		return "block";
	}
	return "";
}

void
mpsslog(int level, const char *format, ...)
{
	va_list   args;
	char      buffer[4096];
	time_t    time_value;
	struct tm time_tm;
	char      time_buf[20];
	char      thread_name[26];

	if (logfp == NULL) {
		return;
	}

	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);

	time(&time_value);
	localtime_r(&time_value, &time_tm);
	size_t length = strftime(time_buf, sizeof(time_buf), "%F %T", &time_tm);
	time_buf[length] = '\0';

	prctl(PR_GET_NAME, thread_name, 0, 0, 0);

	fprintf(logfp, "%s (%lu) %s: %s", time_buf, syscall(__NR_gettid), thread_name, buffer);
	fflush(logfp);
}

void
mpsslog_raw(int level, const char *format, ...)
{
	va_list args;

	if (!logfp) {
		return;
	}

	va_start(args, format);
	vfprintf(logfp, format, args);
	va_end(args);

	fflush(logfp);
}

void
mpssd_elist_log(mpss_elist& logs, void (*func)(int level, const char* fmt, ...))
{
	logs.print(PINFO, func);
}

void
set_thread_name(const char* dev, const char* name)
{
	char thread_name[26];
	snprintf(thread_name, sizeof(thread_name), "%s %s", dev, name);

	prctl(PR_SET_NAME, thread_name, 0, 0, 0);
}

void
display_modules_version()
{
	std::map<std::string, std::string> modules_version;

	modules_version["cosm_bus"] = get_module_version("cosm_bus");
	modules_version["mic_cosm"] = get_module_version("mic_cosm");
	modules_version["mic_x200"] = get_module_version("mic_x200");
	modules_version["mic_x200_dma"] = get_module_version("mic_x200_dma");
	modules_version["scif_bus"] = get_module_version("scif_bus");
	modules_version["scif"] = get_module_version("scif");
	modules_version["vop_bus"] = get_module_version("vop_bus");
	modules_version["vop"] = get_module_version("vop");

	bool is_equal = true;
	for (const auto& iter : modules_version) {
		if (modules_version["cosm_bus"] != iter.second) {
			is_equal = false;
			break;
		}
	}

	if (is_equal) {
		mpssd_log(PINFO, "Kernel modules version: %s", modules_version["cosm_bus"].c_str());
	} else {
		mpssd_log(PWARN, "Different kernel modules version:");

		std::string ver;
		for (const auto& iter : modules_version) {
			ver += iter.first + ":" + iter.second + " ";
		}
		mpssd_log(PWARN, "%s", ver.c_str());
	}
}
