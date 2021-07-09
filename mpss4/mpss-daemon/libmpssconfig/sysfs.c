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

#include "mpssconfig.h"
#include "libmpsscommon.h"

#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <pwd.h>
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <unistd.h>

int
mpss_opensysfs(const std::string& name, const char *entry)
{
	char filename[PATH_MAX];
	snprintf(filename, PATH_MAX, "%s/%s/%s", MICSYSFSDIR, name.c_str(), entry);
	return open(filename, O_RDONLY);
}

std::string
mpss_readsysfs(const std::string& name, const char *entry, const char *directory)
{
	char value[PATH_MAX];
	char *valstart = NULL;
	char *valend = NULL;
	int fd;
	size_t len;

	char filename[PATH_MAX];
	snprintf(filename, PATH_MAX, "%s/%s/%s", directory, name.c_str(), entry);

	if ((fd = open(filename, O_RDONLY)) < 0)
		return "";

	len = read(fd, value, sizeof(value) - 1);
	close(fd);

	if (len <= 0)
		return "";

	/*
	* We are deleting leading and trailing whitespaces here
	*/

	value[len] = '\0';
	valstart = value;
	valend = valstart + len - 1;

	while (isspace(*valstart))
		valstart++;

	while (valend >= valstart && isspace(*valend)) {
		*valend = '\0';
		valend--;
	}

	return valstart;
}

int
mpss_setsysfs(const std::string& name, const char *entry, const char *value)
{
	char filename[PATH_MAX];
	char oldvalue[PATH_MAX];
	int fd;

	if (name.empty())
		snprintf(filename, PATH_MAX, "%s/%s", MICSYSFSDIR, entry);
	else
		snprintf(filename, PATH_MAX, "%s/%s/%s", MICSYSFSDIR, name.c_str(), entry);

	if ((fd = open(filename, O_RDWR)) < 0) {
		return errno;
	}

	if (read(fd, oldvalue, sizeof(oldvalue)) < 0) {
		close(fd);
		return errno;
	}

	if (strcmp(value, oldvalue)) {
		if (write(fd, value, strlen(value)) < 0) {
			close(fd);
			return errno;
		}
	}

	close(fd);
	return 0;
}

std::string
find_ssh_key()
{
	struct passwd *hpw = getpwuid(0);
	DIR *dp;
	char *homedir = hpw->pw_dir;
	std::string buffer;

	std::string keydir = homedir + std::string("/.ssh");

	if ((dp = opendir(keydir.c_str())) == NULL)
		return buffer;

	struct dirent *file;
	while ((file = readdir(dp)) != NULL) {
		if ((strlen(file->d_name) < 4) || (strcmp(&file->d_name[strlen(file->d_name) - 4], ".pub")))
			continue;

		std::string srcpath = keydir + std::string("/") + file->d_name;

		std::ifstream ssh_key_file(srcpath.c_str());
		if (ssh_key_file)
			std::getline(ssh_key_file, buffer);

		break;
	}

	closedir(dp);
	return buffer;
}

/*
 * Write the kernel command line to the sysfs based on configuration variables.  If any
 * variable is not set then return an error.
 */
int
mpss_set_cmdline(struct mic_info *mic, std::string *cmdline)
{
	std::ostringstream command_stream;
	std::string stepping = mpss_readsysfs(mic->name, "info/processor_stepping");

	// WA: disable C6 for A0 in both intel_idle and acpi drivers
	if (!strcasecmp(stepping.c_str(), "A0"))
		command_stream << "intel_idle.max_cstate=1 processor.max_cstate=1";

	command_stream << ' ' << "mic_card_id=" << mic->id;
	command_stream << ' ' << "mic_ssh_key=" << '"' << find_ssh_key() << '"';

	if (mic->config.net.card_type == mpss_card_network_type::STATIC) {
		command_stream << ' ' << "mic_net_type=static";
		command_stream << ' ' << "mic_address=" << '"' << mic->config.net.card_setting.ip << '"';
		command_stream << ' ' << "mic_gateway=" << '"' << mic->config.net.card_setting.gateway << '"';
		command_stream << ' ' << "mic_netmask=" << '"' << mic->config.net.card_setting.netmask << '"';
	} else if (mic->config.net.card_type == mpss_card_network_type::DHCP) {
		command_stream << ' ' << "mic_net_type=dhcp";
		command_stream << ' ' << "mic_dhcp_hostname=" << '"' << mic->config.net.card_setting.dhcp_hostname << '"';
	} else if (mic->config.net.card_type == mpss_card_network_type::NONE) {
		command_stream << ' ' << "mic_net_type=none";
	}
	if (mic->config.net.card_type_inet6 == mpss_card_network_type::STATIC) {
		command_stream << ' ' << "mic_net_type6=static";
		command_stream << ' ' << "mic_address6=" << '"' << mic->config.net.card_setting_inet6.ip << '"';
		command_stream << ' ' << "mic_gateway6=" << '"' << mic->config.net.card_setting_inet6.gateway << '"';
		command_stream << ' ' << "mic_netmask6=" << '"' << mic->config.net.card_setting_inet6.netmask << '"';
	} else if (mic->config.net.card_type_inet6 == mpss_card_network_type::DHCP) {
		command_stream << ' ' << "mic_net_type6=dhcp";
	} else if (mic->config.net.card_type_inet6 == mpss_card_network_type::NONE) {
		command_stream << ' ' << "mic_net_type6=none";
	}

	command_stream << ' ' << "mic_blk=" << '"';
	for (int i = 0; i < BLOCK_MAX_COUNT; ++i) {

		if (mic->config.blockdevs[i].dest == BLOCK_NAME_REPO) {
			if (!mic->config.blockdevs[i].source.empty() &&
			    access(mic->config.blockdevs[i].source.c_str(), F_OK)) {
				continue;
			}
		}

		if (!mic->config.blockdevs[i].source.empty())
			command_stream << mic->config.blockdevs[i].dest << ' ';
	}
	command_stream << '"';

	if (!mic->config.boot.extraCmdline.empty())
		command_stream << ' ' << mic->config.boot.extraCmdline;

	int rc;
	if ((rc = mpss_setsysfs(mic->name, "config/kernel_cmdline", command_stream.str())) != 0) {
		return rc;
	}

	if (cmdline != NULL)
		*cmdline = command_stream.str();

	return 0;
}

