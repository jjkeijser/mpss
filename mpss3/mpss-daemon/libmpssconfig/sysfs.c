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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <libgen.h>
#include <pthread.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>
#include <sys/dir.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <limits.h>
#include <syslog.h>
#include <getopt.h>
#include <linux/kdev_t.h>
#include "mpssconfig.h"
#include "libmpsscommon.h"

int
mpss_opensysfs(char *name, char *entry)
{
	char filename[PATH_MAX];

	snprintf(filename, PATH_MAX - 1, "%s/%s/%s", MICSYSFSDIR, name, entry);
	return open(filename, O_RDONLY);
}

char *
mpss_readsysfs(char *name, char *entry)
{
	char filename[PATH_MAX];
	char value[PATH_MAX];
	char *valstart = NULL;
	char *valend = NULL;
	char *string = NULL;
	int fd;
	size_t len;

	snprintf(filename, PATH_MAX - 1, "%s/%s/%s", MICSYSFSDIR, name, entry);
	if ((fd = open(filename, O_RDONLY)) < 0) {
		return NULL;
	}

	if ((len = read(fd, value, sizeof(value) - 1)) <= 0) {
		goto readsys_ret;
	}

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

	if ((string = (char *) malloc(strlen(valstart) + 1)) != NULL)
		strcpy(string, valstart);

readsys_ret:
	close(fd);
	return string;
}

int
mpss_setsysfs(char *name, char *entry, char *value)
{
	char filename[PATH_MAX];
	char oldvalue[PATH_MAX];
	int fd;

	if (name == NULL)
		snprintf(filename, PATH_MAX - 1, "%s/%s", MICSYSFSDIR, entry);
	else
		snprintf(filename, PATH_MAX - 1, "%s/%s/%s", MICSYSFSDIR, name, entry);

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

char *
get_verbose(struct mic_info *mic)
{
	if (mic->config.boot.verbose != TRUE)
		return "quiet";

	return "";
}

static char *
get_rootdev(struct mic_info *mic, struct mbridge *brlist)
{
	char *cmdline = NULL;
	struct mbridge *br;
	char mtu[32];
	char netmask[32];
	char *netbits;

	switch (mic->config.rootdev.type) {
	case ROOT_TYPE_RAMFS:
	case ROOT_TYPE_STATICRAMFS:
		if ((cmdline = (char *) malloc(strlen("root=ramfs") + 1)) == NULL)
			return NULL;

		strcpy(cmdline, "root=ramfs");
		break;
	case ROOT_TYPE_PFS:
		if ((cmdline = (char *) malloc(strlen("root=/dev/vda") + 1)) == NULL)
			return NULL;
		strcpy(cmdline, "root=/dev/vda");
		break;

	case ROOT_TYPE_NFS:
	case ROOT_TYPE_SPLITNFS:
	{
		char *pattern = "root=nfs: ip=XXX.XXX.XXX.XXX hwaddr=00:00:00:00:00:00 netmask=ffffffff mtu=XXXXXX   ";
		int cmdlinesize = 0;

		if ((br = mpss_bridge_byname(mic->config.net.bridge, brlist)) != NULL) {
			if (br->mtu)
				snprintf(mtu, 31, " mtu=%s", br->mtu);
			else
				strcpy(mtu, " mtu=24");

			if (br->prefix)
				netbits = br->prefix;
			else
				netbits = "24";

		} else {
			netbits = "24";
			mtu[0] = '\0';
		}

		mpssnet_genmask(netbits, netmask);

		cmdlinesize = strlen(pattern) + strlen(mic->config.rootdev.target);
		if ((cmdline = (char *) malloc(cmdlinesize)) == NULL)
			return NULL;

		snprintf(cmdline, cmdlinesize, "root=nfs:%s ip=%s netmask=%s %s", mic->config.rootdev.target,
				 mic->config.net.micIP, netmask, mtu);

		if (mic->config.net.hostMac > MIC_MAC_RANDOM) {
			if ((cmdlinesize - strlen(cmdline)) > (strlen(" hwaddr=") + strlen(mic->config.net.micMac))) {
				strcat(cmdline, " hwaddr=");
				strcat(cmdline, mic->config.net.micMac);
			}
		}

		break;
	}
	}

	return cmdline;
}

static char *
get_console(struct mic_info *mic)
{
	char *cmd_string;

	if (mic->config.boot.console == NULL)
		return NULL;

	if ((cmd_string = (char *) malloc(strlen(mic->config.boot.console) + strlen("console=") + 1)) == NULL)
		return NULL;

	sprintf(cmd_string, "console=%s", mic->config.boot.console);
	return cmd_string;
}

static int
get_pm(struct mic_info *mic, char *cmd_string, size_t size)
{
	if (mic->config.boot.pm == NULL)
		return ESRCH;

	snprintf(cmd_string, size, "micpm=%s", mic->config.boot.pm);
	return 0;
}

char *
get_cgroup(struct mic_info *mic)
{
	char *cmd_string;

	if (mic->config.cgroup.memory == TRUE)
		return NULL;

	if ((cmd_string = (char *) malloc(strlen("cgroup_disable=memory") + 1)) == NULL)
		return NULL;

	sprintf(cmd_string, "cgroup_disable=memory");
	return cmd_string;
}

/*
 * Write the kerrnel command line to the sysfs based on configuration variables.  If any
 * variable is not set then return an error.
 */
char *
mpss_set_cmdline(struct mic_info *mic, struct mbridge *brlist, char *cmdline, char *ip)
{
	char *verboseline = NULL;
	char *rootdev = NULL;
	char *console = NULL;
	char *cgroup = NULL;
	/* cmdline1 and cmdline2 must have the same size */
	char cmdline1[2048];
	char cmdline2[2048];
	char pm[1024];
	int err;
	size_t cmdlinesize = sizeof(cmdline1);

	verboseline = get_verbose(mic);
	snprintf(cmdline1, cmdlinesize, "%s", verboseline);

	if ((rootdev = get_rootdev(mic, brlist)) == NULL) {
		return "RootDevice parameter invalid";
	}

	snprintf(cmdline2, cmdlinesize, "%s %s", cmdline1, rootdev);
	free(rootdev);

	if ((console = get_console(mic)) != NULL) {
		snprintf(cmdline1, cmdlinesize, "%s %s", cmdline2, console);
		free(console);
	} else {
		strncpy(cmdline1, cmdline2, cmdlinesize);
	}

	if ((cgroup = get_cgroup(mic)) != NULL) {
		snprintf(cmdline2, cmdlinesize, "%s %s", cmdline1, cgroup);
		free(cgroup);
	} else {
		strncpy(cmdline2, cmdline1, cmdlinesize);
	}

	if (mic->config.boot.extraCmdline != NULL)
		snprintf(cmdline1, cmdlinesize, "%s %s", cmdline2, mic->config.boot.extraCmdline);
	else
		strncpy(cmdline1, cmdline2, cmdlinesize);

	err = get_pm(mic, pm, sizeof(pm));
	switch(err) {
	case 0:
		snprintf(cmdline2, cmdlinesize, "%s %s", cmdline1, pm);
		break;
	case ESRCH:
		strncpy(cmdline2, cmdline1, cmdlinesize);
		break;
	}

	if (ip != NULL)
		snprintf(cmdline1, cmdlinesize, "%s ip=%s", cmdline2, ip);
	else
		strncpy(cmdline1, cmdline2, cmdlinesize);

	if ((err = mpss_setsysfs(mic->name, "cmdline", cmdline1)) != 0) {
		return "Failed write command line to sysfs";
	}

	if (cmdline != NULL)
		strncpy(cmdline, cmdline1, cmdlinesize);

	return NULL;
}


