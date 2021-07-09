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

#include <mic.h>
#include <mpssconfig.h>
#include <libmpsscommon.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <unistd.h>

static int g_verbose_mode = MIC_VERBOSE_NONE;


#define mic_log(level, fmt, ...) \
	mic_printf(print, __func__, __LINE__, mic, level, fmt "\n", ##__VA_ARGS__);


static void
mic_printf(log_func_t print, const char* func, int line, struct mic_info* mic, int level, const char *format, ...)
{
	if (!print || g_verbose_mode == MIC_VERBOSE_NONE)
		return;

	va_list args;
	char buffer[4096];
	int buffer_idx = 0;
	int written = 0;

	if (g_verbose_mode & MIC_VERBOSE_DEBUG) {
		written = snprintf(&buffer[buffer_idx], sizeof(buffer) - buffer_idx,
			"<%s:%u> ", func, line);
		if (written < 0) {
			return;
		}
		buffer_idx += written;
	}

	if (g_verbose_mode & MIC_VERBOSE_MIC_NAME) {
		written = snprintf(&buffer[buffer_idx], sizeof(buffer) - buffer_idx,
			"%s: ", mic ? mic->name.c_str() : "main");
		if (written < 0) {
			return;
		}
		buffer_idx += written;
	}

	if (g_verbose_mode & MIC_VERBOSE_NORMAL) {
		va_start(args, format);
		written = vsnprintf(&buffer[buffer_idx], sizeof(buffer) - buffer_idx, format, args);
		va_end(args);
		if (written < 0) {
			return;
		}
	}

	print(level, "%s", buffer);
}

void
mic_set_verbose_mode(int mode)
{
	g_verbose_mode = mode;
}

int
mic_state_to_id(const char* state)
{
	if (!strcmp(state, "error"))
		return MIC_STATE_ERROR;
	else if (!strcmp(state, "ready"))
		return MIC_STATE_READY;
	else if (!strcmp(state, "online"))
		return MIC_STATE_ONLINE;
	else if (!strcmp(state, "online_firmware"))
		return MIC_STATE_ONLINE_FIRMWARE;
	else if (!strcmp(state, "shutdown"))
		return MIC_STATE_SHUTDOWN;
	else if (!strcmp(state, "resetting"))
		return MIC_STATE_RESETTING;
	else if (!strcmp(state, "shutting_down"))
		return MIC_STATE_SHUTTING_DOWN;
	else if (!strcmp(state, "booting"))
		return MIC_STATE_BOOTING;
	else if (!strcmp(state, "booting_firmware"))
		return MIC_STATE_BOOTING_FIRMWARE;

	return MIC_STATE_UNKNOWN;
}

std::string
mic_read_sysfs_file(int fd)
{
	char buffer[128] = {0};
	std::string contents;

	if (lseek(fd, 0, SEEK_SET) < 0) {
		return "";
	}

	ssize_t length;
	while ((length = read(fd, buffer, sizeof(buffer))) > 0) {
		contents.append(buffer, length);
	}

	// remove new line character if exists
	if (contents.size() > 0 && contents.back() == '\n') {
		contents.pop_back();
	}

	return contents;
}
int
mic_get_state_id(struct mic_info* mic, log_func_t print)
{
	if (!mic || mic->name.empty()) {
		mic_log(PWARN, "Invalid argument");
		return MIC_STATE_UNKNOWN;
	}

	std::string state = mpss_readsysfs(mic->name, "state");
	if (state.empty()) {
		mic_log(PWARN, "Cannot read the mic state");
		return MIC_STATE_UNKNOWN;
	}
	return mic_state_to_id(state.c_str());
}

int
mic_boot(struct mic_info* mic, log_func_t print)
{
	int rc = 0;

	if (!mic || mic->name.empty()) {
		mic_log(PWARN, "Invalid argument");
		return EINVAL;
	}

	std::string efi_image_path = mic->config.boot.efiimage;
	if (efi_image_path.empty() || !path_exists(efi_image_path)) {
		mic_log(PWARN, "Boot aborted - invalid EfiImage parameter: '%s'",
			efi_image_path.c_str());
		return EINVAL;
	}

	std::string kernel_image_path = mic->config.boot.osimage;
	if (kernel_image_path.empty() || !path_exists(kernel_image_path)) {
		mic_log(PWARN, "Boot aborted - invalid KernelImage parameter: '%s'",
			kernel_image_path.c_str());
		return EINVAL;
	}

	std::string initramfs_image_path = mic->config.filesrc.base.image;
	if (initramfs_image_path.empty() || !path_exists(initramfs_image_path)) {
		mic_log(PWARN, "Boot aborted - invalid InitRamFsImage parameter: '%s'",
			initramfs_image_path.c_str());
		return EINVAL;
	}

	std::string kernel_cmdline;
	if ((rc = mpss_set_cmdline(mic, &kernel_cmdline)) != 0) {
		mic_log(PWARN, "Boot aborted - cannot set kernel command line: %s", strerror(rc));
		return rc;
	}

	mic_log(PINFO, "Kernel command line: %s", kernel_cmdline.c_str());

	/*
	 * The request_firmware() API that is used in the upstream stack
	 * expects the firmware images to be somewhere under /lib/firmware.
	 * We create /lib/firmware/mic/micX/ and place links to micX specific
	 * images under them.
	 */
	std::string firmware_dir = "/lib/firmware/";
	std::string mic_dir = "mic/" + mic->name;
	std::string os_image_path = firmware_dir + mic_dir;

	mpssut_deltree(os_image_path.c_str());

	if ((rc = mpssut_mksubtree(os_image_path.c_str(), 0, 0, 0755)) != 0) {
		mic_log(PWARN, "Boot aborted - failed to create '%s' directory: %s",
			os_image_path.c_str(), strerror(rc));
		return rc;
	}

	std::string efi_image_link = mic_dir + "/efi.image";
	std::string kernel_image_link = mic_dir + "/kernel.image";
	std::string initramfs_image_link = mic_dir + "/initramfs.image";

	std::string efi_image_link_abs = firmware_dir + efi_image_link;
	std::string kernel_image_link_abs = firmware_dir + kernel_image_link;
	std::string initramfs_image_link_abs = firmware_dir + initramfs_image_link;

	if (symlink(efi_image_path.c_str(), efi_image_link_abs.c_str()) < 0) {
		mic_log(PWARN, "Boot aborted - failed to create symlink '%s' for EFI image: %s",
			efi_image_link_abs.c_str(), strerror(errno));
		return errno;
	}

	if (symlink(kernel_image_path.c_str(), kernel_image_link_abs.c_str()) < 0) {
		mic_log(PWARN, "Boot aborted - failed to create symlink '%s' for Kernel image: %s",
			kernel_image_link_abs.c_str(), strerror(errno));
		return errno;
	}

	if (symlink(initramfs_image_path.c_str(), initramfs_image_link_abs.c_str()) < 0) {
		mic_log(PWARN, "Boot aborted - failed to create symlink '%s' for InitRamFs image: %s",
			initramfs_image_link_abs.c_str(), strerror(errno));
		return errno;
	}

	if ((rc = mpss_setsysfs(mic->name, "config/efi_image", efi_image_link)) != 0) {
		mic_log(PWARN, "Boot aborted - write failed to config/efi_image: %s", strerror(rc));
		return rc;
	}

	if ((rc = mpss_setsysfs(mic->name, "config/kernel_image", kernel_image_link)) != 0) {
		mic_log(PWARN, "Boot aborted - write failed to config/kernel_image: %s", strerror(rc));
		return rc;
	}

	if ((rc = mpss_setsysfs(mic->name, "config/initramfs_image", initramfs_image_link)) != 0) {
		mic_log(PWARN, "Boot aborted - write failed to config/initramfs_image: %s", strerror(rc));
		return rc;
	}

	std::string boot_timeout = std::to_string(mic->config.misc.boottimeout);
	if ((rc = mpss_setsysfs(mic->name, "config/boot_timeout", boot_timeout)) != 0) {
		mic_log(PWARN, "Boot aborted - write failed to config/boot_timeout: %s", strerror(rc));
		return rc;
	}

	std::string shutdown_timeout = std::to_string(mic->config.misc.shutdowntimeout);
	if ((rc = mpss_setsysfs(mic->name, "config/shutdown_timeout", shutdown_timeout)) != 0) {
		mic_log(PWARN, "Boot aborted - write failed to config/shutdown_timeout: %s", strerror(rc));
		return rc;
	}

	if ((rc = mpss_setsysfs(mic->name, "state", MIC_CMD_BOOT_STR)) != 0) {
		mic_log(PWARN, "Boot aborted - command write failed: %s", strerror(rc));
		return rc;
	}

	mic_log(PINFO, "Start booting");

	return 0;
}

int
mic_reset(struct mic_info* mic, int force, log_func_t print)
{
	int rc = 0;

	if (!mic || mic->name.empty()) {
		mic_log(PWARN, "Invalid argument");
		return EINVAL;
	}

	const std::string& command = force ? MIC_CMD_RESET_FORCE_STR : MIC_CMD_RESET_STR;

	if ((rc = mpss_setsysfs(mic->name, "state", command)) != 0) {
		mic_log(PWARN, "Reset aborted - command write failed: %s", strerror(rc));
		return rc;
	}

	mic_log(PINFO, "Start resetting");

	return 0;
}

int
mic_shutdown(struct mic_info* mic, int force, log_func_t print)
{
	int rc = 0;

	if (!mic || mic->name.empty()) {
		mic_log(PWARN, "Invalid argument");
		return EINVAL;
	}

	const std::string& command = force ? MIC_CMD_SHUTDOWN_FORCE_STR : MIC_CMD_SHUTDOWN_STR;

	if ((rc = mpss_setsysfs(mic->name, "state", command)) != 0) {
		mic_log(PWARN, "Shutdown aborted - command write failed: %s", strerror(rc));
		return rc;
	}

	mic_log(PINFO, "Start shutting down");

	return 0;
}


/******************************************************************************
 * Legacy and deprecated API
 ******************************************************************************/

int
boot_linux(int node)
{
	int cnt;
	std::shared_ptr<mic_info> miclist = mpss_get_miclist(&cnt);
	if (!miclist)
		return ENODEV;

	struct mic_info* mic;
	if ((mic = mpss_find_micid_inlist(miclist.get(), node)) == NULL)
		return ENODEV;

	mpss_elist perrs;
	if (mpss_parse_config(mic, perrs))
		return ENODEV;

	return mic_boot(mic, nullptr);
}

int
boot_media(int node, const char* media_path)
{
	int cnt;
	std::shared_ptr<mic_info> miclist = mpss_get_miclist(&cnt);
	if (!miclist)
		return ENODEV;

	struct mic_info* mic;
	if ((mic = mpss_find_micid_inlist(miclist.get(), node)) == NULL)
		return ENODEV;

	std::string os_image_path = "/lib/firmware/" + std::string(media_path);
	if (!path_exists(os_image_path))
		return EEXIST;

	int ret;
	if ((ret = mpss_setsysfs(mic->name, "config/efi_image", media_path)) != 0)
		return ret;

	return mpss_setsysfs(mic->name, "state", MIC_CMD_BOOT_FIRMWARE_STR);
}

const char*
get_state(int node)
{
	const char* states[] = {
		"unknown",
		"error",
		"ready",
		"online",
		"online_firmware",
		"shutdown",
		"resetting",
		"shutting_down",
		"booting",
		"booting_firmware",
	};

	int state_id = MIC_STATE_UNKNOWN;

	int cnt;
	std::shared_ptr<mic_info> miclist = mpss_get_miclist(&cnt);
	if (!miclist)
		return states[state_id];

	struct mic_info* mic;
	if ((mic = mpss_find_micid_inlist(miclist.get(), node)) == NULL)
		return states[state_id];

	state_id = mic_get_state_id(mic, nullptr);

	return states[state_id];
}

int
reset_node(int node)
{
	int cnt;
	std::shared_ptr<mic_info> miclist = mpss_get_miclist(&cnt);
	if (!miclist)
		return ENODEV;

	struct mic_info* mic;
	if ((mic = mpss_find_micid_inlist(miclist.get(), node)) == NULL)
		return ENODEV;

	return mpss_setsysfs(mic->name, "state", MIC_CMD_RESET_STR);
}
