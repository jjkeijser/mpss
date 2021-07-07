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
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>
#include "micctrl.h"
#include "help.h"

static unsigned long mpsscookie = 0;

char *autoboot_help =
"\
micctrl [global options] --autoboot=<yes | no> [sub options] [Xeon Phi cards]\n\
\n\
The --autoboot option sets the value of BootOnStart in the Xeon Phi\n\
configuration files.  If set to yes the card will be booted when the mpss\n\
service is started.  If set to it will not boot.\n\
\n\
If the autoboot value is not specified a the current settings will be displayed\n\
\n"
COMMON_GLOBAL
COMMON_HELP;

static struct option autoboot_longopts[] = {
	{"help", no_argument, NULL, 'h'},
	{0}
};

void show_autoboot(struct mic_info *miclist);
void setautoboot(struct mic_info *miclist, char *boot);

void
parse_autoboot_args(char *autoboot, int argc, char *argv[])
{
	struct mic_info *miclist;
	int cnt;
	int longidx;
	int c;

	while ((c = getopt_long(argc, argv, "hv", autoboot_longopts, &longidx)) != -1) {
		switch(c) {
		case 'h':
			micctrl_help(autoboot_help);
			exit(0);

		case 'v':
			bump_display_level();
			break;

		default:
			ARGERROR_EXIT(autoboot_help);
		}
	}

	miclist = create_miclist(&mpssenv, argc, argv, optind, &cnt);

	if (autoboot == NULL) {
		show_autoboot(miclist);
	} else {
		check_rootid();
		setautoboot(miclist, autoboot);
	}
}

char *cgroup_help =
"\
micctrl [global options] --cgroup [sub options] [Xeon Phi cards]\n\
\n\
The --cgroup option sets parameters for Linux kernel cgroup handling.  At this\n\
time it only supports setting the memory option to enable or disabled.  By\n\
default the Linux kernel on the Xeon Phi cards has cgroup memory enabled.  Since\n\
this reduces the amount of memory availability many system administrators will\n\
want to disable it.\n\
\n\
If no sub options are specified the current settings will be displayed.\n\
\n"
COMMON_GLOBAL
COMMON_HELP
"\
    -m <enable | disable>\n\
    --memory=<enable | disable>\n\
\n\
        This is what determines the setting of the cgroup memory parameters.\n\
\n"
;

static struct option cgroup_longopts[] = {
	{"help", no_argument, NULL, 'h'},
	{"memory", required_argument, NULL, 'm'},
	{0}
};

void show_cgroup(struct mic_info *miclist);
void setcgroup(struct mic_info *miclist, char *cmem);

void
parse_cgroup_args(int argc, char *argv[])
{
	struct mic_info *miclist;
	int cnt;
	int longidx;
	int c;
	char *cmem = NULL;

	while ((c = getopt_long(argc, argv, "hmv", cgroup_longopts, &longidx)) != -1) {
		switch(c) {
		case 'h':
			micctrl_help(cgroup_help);
			exit(0);

		case 'm':
			cmem = optarg;
			break;

		case 'v':
			bump_display_level();
			break;

		default:
			ARGERROR_EXIT(cgroup_help);
		}
	}

	miclist = create_miclist(&mpssenv, argc, argv, optind, &cnt);

	if (cmem == NULL) {
		show_cgroup(miclist);
	} else {
		check_rootid();
		setcgroup(miclist, cmem);
	}
}

char *osimage_help =
"\
micctrl [global options] --osimage=<bzimage> [sub options] [Xeon Phi cards]\n\
\n\
The --osimage option sets the Linux kernel image to boot to 'bzimage'. The\n\
initdefaults would have set the default image to point to the image installed\n\
with the mpss-boot-files RPM.\n\
\n\
A Linux kernel image is built with a matching system map file which holds values\n\
used by the mpssd daemon and should be indicated at the same time.\n\
\n\
If no osimage is specified the current settings will be displayed.\n\
\n"
COMMON_GLOBAL
COMMON_HELP
"\
    -s <sysmapfile>\n\
    --sysmap=<sysmapfile>\n\
\n\
        Indicate the system map file matching the bzimage file for the Linux\n\
        kernel.\n\
\n";

static struct option osimage_longopts[] = {
	{"help", no_argument, NULL, 'h'},
	{"sysmap", required_argument, NULL, 's'},
	{0}
};

void show_images(struct mic_info *miclist);
void setimage(struct mic_info *miclist, char *image, char *sysmap);

void
parse_osimage_args(char *osimage, int argc, char *argv[])
{
	struct mic_info *miclist;
	char *sysmap = NULL;
	int cnt;
	int longidx;
	int c;

	while ((c = getopt_long(argc, argv, "hvs:", osimage_longopts, &longidx)) != -1) {
		switch(c) {
		case 'h':
			micctrl_help(osimage_help);
			exit(0);

		case 's':
			sysmap = mpssut_tilde(&mpssenv, optarg);
			break;

		case 'v':
			bump_display_level();
			break;

		default:
			ARGERROR_EXIT(osimage_help);
		}
	}

	miclist = create_miclist(&mpssenv, argc, argv, optind, &cnt);

	if (osimage == NULL) {
		show_images(miclist);
	} else {
		check_rootid();
		setimage(miclist, osimage, sysmap);
	}
}

char *service_help =
"\
micctrl [global options] --service=<name> [sub options] [Xeon Phi cards]\n\
\n\
The --service option was a big part of the 2.X releases.  It may or may not have\n\
any effect in the 3.X releases and should no longer be used.\n\
\n\
The purpose of the --service option was to proved 'chkconfig' functionality for\n\
the Xeon Phi cards.  It requires specifying the start and stop level along with\n\
an indicator whether it sould be enabled or not.\n\
\n\
If a service name is not specified then print the current list of service\n\
settings.\n\
\n"
COMMON_GLOBAL
COMMON_HELP
"\
    -s <startlevel>\n\
    --start=<startlevel>\n\
\n\
        The 'startlevel' value in the link name will be set to this value. It\n\
        must be between the value 1 and 100.\n\
\n\
    -e <stoplevel>\n\
    --stop=<stoplevel>\n\
\n\
        The 'stoplevel' value in the link name will be set to this value. It\n\
        must be between the value 1 and 100.\n\
\n\
    -d <state>\n\
    --state=<on | off>\n\
\n\
        If state is set to 'on' the start link files in the Xeon Phi card's\n\
        /etc/rc5.d directory will be created.  If set to 'off' the remove the\n\
        link files.\n\
\n";

static struct option service_longopts[] = {
	{"help", no_argument, NULL, 'h'},
	{"start", required_argument, NULL, 's'},
	{"stop", required_argument, NULL, 'e'},
	{"state", required_argument, NULL, 'd'},
	{0}
};

void show_services(struct mic_info *miclist);
void config_service(struct mic_info *miclist, char *service, unsigned int start, unsigned int stop, char *state);

void
parse_service_args(char *service, int argc, char *argv[])
{
	struct mic_info *miclist;
	int cnt;
	int longidx;
	int c;
	unsigned int start = 0;
	unsigned int stop = 0;
	char *state = NULL;

	while ((c = getopt_long(argc, argv, "hvs:e:d:", service_longopts, &longidx)) != -1) {
		switch(c) {
		case 'h':
			micctrl_help(service_help);
			exit(0);

		case 's':
			start = atoi(optarg);
			break;

		case 'e':
			stop = atoi(optarg);
			break;

		case 'd':
			state = optarg;
			break;

		case 'v':
			bump_display_level();
			break;

		default:
			ARGERROR_EXIT(service_help);
		}
	}

	miclist = create_miclist(&mpssenv, argc, argv, optind, &cnt);

	if (service == NULL) {
		show_services(miclist);
	} else {
		check_rootid();
		if (mpssenv_aquire_lockfile(&mpssenv)) {
			display(PERROR, "Cannot change service - mpssd daemon is running\n");
			EXIT_ERROR(EBUSY);
		}

		config_service(miclist, service, start, stop, state);
	}
}

char *syslog_help =
"\
micctrl [global options] --syslog=<file|buffer|remote> [sub opts] [Xeon Phis]\n\
\n\
The --syslog option creates the syslog configuration file on the Xeon Phi\n\
file system.  The possible types correspond directly to Linux syslog types.\n\
\n\
    file:   Set the syslog to print to a log file.  The default log file is\n\
            /var/log/messages.\n\
\n\
    buffer: Set the syslog to only be available from the kmesg buffer.\n\
\n\
    remote: Set the syslog to use TCP to another host system.  The default host\n\
            will be set to 'host:514' to indicate the host and its default\n\
            port..\n\
\n\
            NOTE: You must enable the target host to recieve remote syslog\n\
                  messages.  For example in Red Hat edit the\n\
                  /etc/rsyslog.conf file and uncomment the lone under\n\
                  '# Provides TCP syslog reception'.\n\
\n\
If the type is not specified the current settings will be displayed.\n\
\n"
COMMON_GLOBAL
COMMON_HELP
"\
    -f <logfile>\n\
     --logfile=<logfile>\n\
\n\
        If the type is set to 'file' use this file instead of the default\n\
        /var/log/messages file.\n\
\n\
    -s <hostname>\n\
    --host=<hostname>\n\
\n\
        If the type is set to 'remote' use the specified host and port ID pair\n\
        instead of the default 'host:514' set.\n\
\n\
    -l <loglevel>\n\
     --loglevel=<loglevel>\n\
\n\
        Set the log level.  For information on log levels check the syslog man\n\
        page.\n\
\n";

static struct option syslog_longopts[] = {
	{"help", no_argument, NULL, 'h'},
	{"host", required_argument, NULL, 's'},
	{"logfile", required_argument, NULL, 'f'},
	{"loglevel", required_argument, NULL, 'l'},
	{0}
};

void setsyslog(struct mic_info *miclist, char *type, char *host, char *logfile, int loglevel);
void show_syslog(struct mic_info *miclist);

void
parse_syslog_args(char *type, int argc, char *argv[])
{
	struct mic_info *miclist;
	char *host = NULL;
	char *logfile = NULL;
	int loglevel = 0;
	int cnt;
	int longidx;
	int c;

	while ((c = getopt_long(argc, argv, "hvs:f:l:", syslog_longopts, &longidx)) != -1) {
		switch(c) {
		case 'h':
			micctrl_help(syslog_help);
			exit(0);

		case 's':
			host = optarg;
			break;

		case 'f':
			logfile = optarg;
			break;

		case 'l':
			loglevel = atoi(optarg);
			break;

		case 'v':
			bump_display_level();
			break;

		default:
			ARGERROR_EXIT(syslog_help);
		}
	}

	miclist = create_miclist(&mpssenv, argc, argv, optind, &cnt);

	if (type == NULL) {
		show_syslog(miclist);
	} else {
		check_rootid();
		setsyslog(miclist, type, host, logfile, loglevel);
	}
}

void
show_autoboot(struct mic_info *miclist)
{
	struct mic_info *mic;

	putchar('\n');

	for (mic = miclist; mic != NULL; mic = mic->next) {
		if (micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO)) {
			display(PWARN, "%s: Not configured - skipping\n", mic->name);
			continue;
		}

		printf("%s:\tBootOnStart: %s\n", mic->name,
		       (mic->config.boot.onstart == TRUE)? "enabled" : "disabled");
	}

	putchar('\n');
	exit(0);
}

void
setautoboot(struct mic_info *miclist, char *boot)
{
	struct mic_info *mic;
	int autoboot = TRUE;
	int err;

	if (!strcasecmp(boot, "yes")) {
		autoboot = TRUE;
	} else if (!strcasecmp(boot, "no")) {
		autoboot = FALSE;
	} else {
		display(PERROR, "Autoboot must be passed a 'yes' or 'no'\n");
		EXIT_ERROR(EINVAL);
	}

	for (mic = miclist; mic != NULL; mic = mic->next) {
		if (micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO)) {
			display(PWARN, "%s: Not configured - skipping\n", mic->name);
			continue;
		}

		if (autoboot == mic->config.boot.onstart) {
			display(PINFO, "%s: BootOnStart not changed\n", mic->name);
			continue;
		}

		if ((err = mpss_update_config(&mpssenv, mic->config.name, "BootOnStart", NULL, "BootOnStart %s\n",
										autoboot? "Enabled" : "Disabled")))
			display(PERROR, "Failed to open config file '%s': %d\n", mic->config.name, err);
		else
			display(PFS, "%s: Update AutoBoot in %s\n", mic->name, mic->config.name);
	}

	exit(0);
}

void
show_cgroup(struct mic_info *miclist)
{
	struct mic_info *mic;

	putchar('\n');

	for (mic = miclist; mic != NULL; mic = mic->next) {
		if (micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO)) {
			display(PWARN, "%s: Not configured - skipping\n", mic->name);
			continue;
		}

		printf("%s:\tcgroup: memory=%s\n", mic->name, mic->config.cgroup.memory? "enabled" : "disabled");
	}

	putchar('\n');
	exit(0);
}

void
setcgroup(struct mic_info *miclist, char *cmem)
{
	struct mic_info *mic;
	int en = FALSE;
	int err;

	if (!strcasecmp(cmem, "enable")) {
		en = TRUE;
	} else if (!strcasecmp(cmem, "disable")) {
		en = FALSE;
	} else {
		display(PERROR, "Cgroup memory cannot be set to '%s'\n", cmem);
	}

	for (mic = miclist; mic != NULL; mic = mic->next) {
		if (micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO)) {
			display(PWARN, "%s: Not configured - skipping\n", mic->name);
			continue;
		}

		if (mic->config.cgroup.memory == en) {
			display(PINFO, "%s: Setting Cgroup memory not changed\n", mic->name);
			continue;
		}

		display(PINFO, "%s: Setting Cgroup memory to %s\n", mic->name, cmem);
		if ((err = mpss_update_config(&mpssenv, mic->config.name, "Cgroup", NULL, "Cgroup memory=%sd\n", cmem)))
			display(PERROR, "Failed to open config file '%s': %d\n", mic->config.name, err);
		else
			display(PFS, "%s: Update Cgroup in %s\n", mic->name, mic->config.name);
	}

	exit(0);
}

void
show_images(struct mic_info *miclist)
{
	struct mic_info *mic;

	putchar('\n');

	for (mic = miclist; mic != NULL; mic = mic->next) {
		if (micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO)) {
			display(PWARN, "%s: Not configured - skipping\n", mic->name);
			continue;
		}

		printf("%s:\tosimage: %s %s\n", mic->name, mic->config.boot.osimage, mic->config.boot.systemmap);
	}

	putchar('\n');
	exit(0);
}

void
setimage(struct mic_info *miclist, char *image, char *sysmap)
{
	struct mic_info *mic;
	char filename[PATH_MAX];
	struct stat sbuf;
	int err;

	if (sysmap == NULL) {
		display(PERROR, "System map file must be specified\n");
		EXIT_ERROR(EINVAL);
	}

	for (mic = miclist; mic != NULL; mic = mic->next) {
		if (micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO)) {
			display(PWARN, "%s: Not configured - skipping\n", mic->name);
			continue;
		}

		if (!mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s", image)) {
			if (stat(image, &sbuf) != 0)
				display(PWARN, "%s: image '%s' does not exist\n", mic->name, image);
		}

		if (!mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s", sysmap)) {
			if (stat(sysmap, &sbuf) != 0)
				display(PWARN, "%s: sysmap '%s' does not exist\n", mic->name, sysmap);
		}

		if ((err = mpss_update_config(&mpssenv, mic->config.name, "OSimage", NULL,
									"OSimage %s %s\n", image, sysmap)))
			display(PERROR, "Failed to open config file '%s': %d\n", mic->config.name, err);
		else
			display(PFS, "%s: Update OSimage in %s\n", mic->name, mic->config.name);
	}

	exit(0);
}

void
show_services(struct mic_info *miclist)
{
	struct mic_info *mic;
	struct mservice *serv;

	printf("\n\tService\t\tStart\tStop\tState\n");
	printf("       ----------------------------------------\n");
	for (mic = miclist; mic != NULL; mic = mic->next) {
		if (micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO)) {
			display(PWARN, "%s: Not configured - skipping\n", mic->name);
			continue;
		}

		serv = mic->config.services.next;
		printf("%s:", mic->name);

		while (serv) {
			printf("\t%s\t", serv->name);
			if (strlen(serv->name) < 8) printf("\t");
			printf("%d\t%d\t%s\n", serv->start, serv->stop, serv->on? "on" : "off");
			serv = serv->next;
		}

		putchar('\n');
	}

	putchar('\n');
	exit(0);
}

void
mod_service(struct mic_info *mic, struct mservice *serv, unsigned int start, unsigned int stop, char *state)
{
	char startname[PATH_MAX];
	char stopname[PATH_MAX];
	char linkname[PATH_MAX];
	int changed = 0;
	char param[128];
	char *hostdir;
	int err;
	int on;

	if ((start > 100) || (start < 1)) {
		display(PERROR, "Start value must be in the range 1 - 100\n");
		EXIT_ERROR(EINVAL);
	} else if (serv->start != start) {
		changed++;
	} else {
		start = serv->start;
	}

	if (stop > 100) {
		display(PERROR, "Stop value must be in the range 1 - 100\n");
		EXIT_ERROR(EINVAL);
	} else if ((stop > 0) && (serv->stop != stop)) {
		changed++;
	} else {
		stop = serv->stop;
	}

	if (state != NULL) {
		if (!strcmp(state, "on")) {
			on = TRUE;
		} else if (!strcmp(state, "off")) {
			on = FALSE;
		} else {
			display(PERROR, "Invalid state '%s': - must be on or off\n", state);
			EXIT_ERROR(EINVAL);
		}

		if (serv->on != on) {
			changed++;
		}
	} else {
		on = serv->on;
	}

	snprintf(param, 128, "Service %s", serv->name);
	if ((err = mpss_update_config(&mpssenv, mic->config.name, param, NULL, "Service %s %d %d %s\n",
				      serv->name, start, stop, on? "on" : "off"))) {
		display(PERROR, "Failed to open config file '%s': %d\n", mic->config.name, err);
		return;
	} else {
		display(PFS, "%s: Update %s in %s\n", mic->name, param, mic->config.name);
	}

	if (mpssut_filename(&mpssenv, NULL, startname, PATH_MAX, "%s/etc/rc5.d/S%02d%s",
			  mic->config.filesrc.mic.dir, serv->start, serv->name))
		unlink(startname);

	if (mpssut_filename(&mpssenv, NULL, stopname, PATH_MAX, "%s/etc/rc5.d/K%02d%s",
			  mic->config.filesrc.mic.dir, serv->stop, serv->name))
		unlink(stopname);

	snprintf(linkname, PATH_MAX, "../init.d/%s", serv->name);
	mpssut_mksubtree(&mpssenv, mic->config.filesrc.mic.dir, "/etc/rc5.d", 0, 0, 0755);

	if (on) {
		symlink(linkname, startname);
		chown(startname, 0, 0);
	} else {
		symlink(linkname, stopname);
		chown(stopname, 0, 0);
	}

	if ((hostdir = micctrl_check_nfs_rootdev(mic)) == NULL)
		return;

	if (mpssut_filename(&mpssenv, NULL, startname, PATH_MAX, "%s/etc/rc5.d/S%02d%s",
			  strchr(mic->config.rootdev.target, ':') + 1, serv->start, serv->name))
		unlink(startname);

	if (mpssut_filename(&mpssenv, NULL, stopname, PATH_MAX, "%s/etc/rc5.d/K%02d%s",
			  strchr(mic->config.rootdev.target, ':') + 1, serv->stop, serv->name))
		unlink(stopname);

	snprintf(linkname, PATH_MAX, "../init.d/%s", serv->name);
	mpssut_mksubtree(&mpssenv, hostdir, "/etc/rc5.d", 0, 0, 0755);

	if (on) {
		symlink(linkname, startname);
		chown(startname, 0, 0);
	} else {
		symlink(linkname, stopname);
		chown(stopname, 0, 0);
	}
}

void
config_service(struct mic_info *miclist, char *service, unsigned int start, unsigned int stop, char *state)
{
	struct mic_info *mic;
	struct mservice *serv;
	char filename[PATH_MAX];
	char startname[PATH_MAX];
	char stopname[PATH_MAX];
	char linkname[PATH_MAX];
	char *hostdir;
	FILE *fp;

	for (mic = miclist; mic != NULL; mic = mic->next) {
		if (micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO)) {
			display(PWARN, "%s: Not configured - skipping\n", mic->name);
			continue;
		}

		serv = &mic->config.services;
		while (serv->next != NULL) {
			serv = serv->next;
			if (!strcmp(serv->name, service)) {
				mod_service(mic, serv, start, stop, state);
				continue;
			}
		}

		if (start == 0) {
			display(PERROR, "Setting a service requires the start parameter\n");
			EXIT_ERROR(EINVAL);
		}

		if ((start < 1) || (start > 100)) {
			display(PERROR, "Start value must be in the range 1 - 100\n");
			EXIT_ERROR(EINVAL);
		}

		if (stop == 0) {
			stop = 100 - start;
			display(PINFO, "Setting stop parameter to %d\n", stop);
		} else if ((stop < 1) || (stop > 100)) {
			display(PERROR, "Stop value must be in the range 1 - 100\n");
			EXIT_ERROR(EINVAL);
		}

		if (state == NULL) {
			state = "on";
		} else if (!strcmp(state, "on") || !strcmp(state, "off")) {

		} else {
			display(PERROR, "State value must be 'on' or 'off'\n");
			EXIT_ERROR(EINVAL);
		}

		if (!mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s/%s.conf", mpssenv.confdir, mic->name)) {
			display(PERROR, "[Service] Update config unable to open %s\n", filename);
			continue;
		}

		if ((fp = fopen(filename, "a")) == NULL) {
			display(PERROR, "[Service] Update config unable to open %s\n", filename);
			continue;
		}

		fprintf(fp, "Service %s %d %d %s\n", service, start, stop, state);
		fclose(fp);

		if (!mpssut_filename(&mpssenv, NULL, startname, PATH_MAX, "%s/etc/rc5.d", mic->config.filesrc.mic.dir)) {
			mkdir(startname, 0755);
			chown(startname, 0, 0);
		}

		if (mpssut_filename(&mpssenv, NULL, startname, PATH_MAX, "%s/etc/rc5.d/S%02d%s",
				  mic->config.filesrc.mic.dir, start, service))
			unlink(startname);

		if (mpssut_filename(&mpssenv, NULL, stopname, PATH_MAX, "%s/etc/rc5.d/K%02d%s",
				  mic->config.filesrc.mic.dir, stop, service))
			unlink(stopname);

		snprintf(linkname, PATH_MAX, "../init.d/%s", service);

		if (!strcmp(state, "on")) {
			symlink(linkname, startname);
			chown(startname, 0, 0);
		} else {
			symlink(linkname, stopname);
			chown(stopname, 0, 0);
		}

		if ((hostdir = micctrl_check_nfs_rootdev(mic)) == NULL)
			continue;

		if (!mpssut_filename(&mpssenv, NULL, startname, PATH_MAX, "%s/etc/rc5.d", hostdir)) {
			mkdir(startname, 0755);
			chown(startname, 0, 0);
		}

		if (mpssut_filename(&mpssenv, NULL, startname, PATH_MAX, "%s/etc/rc5.d/S%02d%s", hostdir, start, service))
			unlink(startname);

		if (mpssut_filename(&mpssenv, NULL, stopname, PATH_MAX, "%s/etc/rc5.d/K%02d%s", hostdir, stop, service))
			unlink(stopname);

		snprintf(linkname, PATH_MAX, "../init.d/%s", service);

		if (!strcmp(state, "on")) {
			symlink(linkname, startname);
			chown(startname, 0, 0);
		} else {
			symlink(linkname, stopname);
			chown(stopname, 0, 0);
		}
	}

	exit(0);
}

char *syslog1 =
	"# This configuration file is used by the busybox syslog init script,\n"
	"# /etc/init.d/syslog[.busybox] to set syslog configuration at start time.\n";

char *syslog2 =
	"REDUCE=no                       # reduce-size logging\n"
	"DROPDUPLICATES=no               # whether to drop duplicate log entries\n"
	"#ROTATESIZE=0                   # rotate log if grown beyond X [kByte]\n"
	"#ROTATEGENS=3                   # keep X generations of rotated logs\n"
	"BUFFERSIZE=64                   # size of circular buffer [kByte]\n"
	"FOREGROUND=no                   # run in foreground (don't use!)\n";

void
syslog_buffer(char *micname, char *base, int loglevel)
{
	char filename[PATH_MAX];
	FILE *fp;
	char *comment = "";

	if (loglevel == 0) {
		loglevel = 5;
		comment = "#";
	}

	if (mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s/etc/syslog-startup.conf", base))
		unlink(filename);

	if ((fp = fopen(filename, "w")) == NULL) {
		display(PERROR, "\n[Syslog] Cannot open %s: %s\n", filename, strerror(errno));
		return;
	}

	fprintf(fp, "%s", syslog1);
	fprintf(fp, "DESTINATION=buffer\t\t# log destinations (buffer file remote)\n");
	fprintf(fp, "LOGFILE=/var/log/messages\t# where to log (file)\n");
	fprintf(fp, "#REMOTE=loghost:514\t\t# where to log (syslog remote)\n");
	fprintf(fp, "%s", syslog2);
	fprintf(fp, "%sLOGLEVEL=%d\t\t\t# local log level (between 1 and 8)\n", comment, loglevel);
	fclose(fp);
	display(PFS, "%s: Update %s to buffer mode log level %d\n", micname, filename, loglevel);
}

void
syslog_file(char *micname, char *base, char *logfile, int loglevel)
{
	char filename[PATH_MAX];
	FILE *fp;
	char *comment = "";

	if (loglevel == 0) {
		loglevel = 5;
		comment = "#";
	}

	if (logfile == NULL)
		logfile = "/var/log/messages";

	if (mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s/etc/syslog-startup.conf", base))
		unlink(filename);

	if ((fp = fopen(filename, "w")) == NULL) {
		display(PERROR, "\n[Syslog] Cannot open %s: %s\n", filename, strerror(errno));
		return;
	}

	fprintf(fp, "%s", syslog1);
	fprintf(fp, "DESTINATION=file\t\t# log destinations (buffer file remote)\n");
	fprintf(fp, "LOGFILE=%s\t# where to log (file)\n", logfile);
	fprintf(fp, "#REMOTE=loghost:514\t\t# where to log (syslog remote)\n");
	fprintf(fp, "%s", syslog2);
	fprintf(fp, "%sLOGLEVEL=%d\t\t\t# local log level (between 1 and 8)\n", comment, loglevel);
	fclose(fp);
	display(PFS, "%s: Update %s to file mode %s level %d\n", micname, filename, logfile, loglevel);
}

void
syslog_remote(char *micname, char *base, char *host, int loglevel)
{
	char filename[PATH_MAX];
	FILE *fp;
	char *comment = "";
	char *port;

	if (loglevel == 0) {
		loglevel = 5;
		comment = "#";
	}

	if (host == NULL) {
		display(PERROR, "[Syslog] Must specify host rsyslog connection\n");
		return;
	}

	if ((port = strchr(host, ':')) == NULL) {
		port = "514";
	} else {
		*port = '\0';
		port++;
	}

	if (mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s/etc/syslog-startup.conf", base))
		unlink(filename);

	if ((fp = fopen(filename, "w")) == NULL) {
		display(PERROR, "\n[Syslog] Cannot open %s: %s\n", filename, strerror(errno));
		return;
	}

	fprintf(fp, "%s", syslog1);
	fprintf(fp, "DESTINATION=remote\t\t# log destinations (buffer file remote)\n");
	fprintf(fp, "LOGFILE=/var/log/messages\t# where to log (file)\n");
	fprintf(fp, "REMOTE=%s:%s\t\t# where to log (syslog remote)\n", host, port);
	fprintf(fp, "%s", syslog2);
	fprintf(fp, "%sLOGLEVEL=%d\t\t\t# local log level (between 1 and 8)\n", comment, loglevel);
	fclose(fp);
	display(PFS, "%s: Update %s to remote mode %s:%s level %d\n", micname, filename, host, port, loglevel);
}

void
syslog_sendfile(scif_epd_t ep, char *base)
{
	unsigned int proto = MICCTRL_SYSLOG_FILE;
	char filename[PATH_MAX];
	struct stat sbuf;
	char *filebuf;
	int file_len;
	int fd;
	int err;

	if (!mpssut_filename(&mpssenv, &sbuf, filename, PATH_MAX, "%s/etc/syslog-startup.conf", base))
		return;

	if ((filebuf = (char *) malloc(sbuf.st_size + 1)) == NULL)
		return;

	if ((fd = open(filename, O_RDONLY)) < 0) {
		free(filebuf);
		return;
	}

	file_len = read(fd, filebuf, sbuf.st_size);
	close(fd);
	filebuf[file_len] = '\0';

	scif_send(ep, &proto, sizeof(proto), 0);
	scif_send(ep, &mpsscookie, sizeof(mpsscookie), SCIF_SEND_BLOCK);
	scif_send(ep, &file_len, sizeof(file_len), 0);
	err = scif_send(ep, filebuf, file_len, SCIF_SEND_BLOCK);
	scif_recv(ep, &proto, sizeof(proto), SCIF_RECV_BLOCK);
	free(filebuf);
}

void
syslog_setremote(struct mic_info *mic)
{
	struct scif_portID portID = {mic->id + 1, MPSSD_MICCTRL};
	scif_epd_t ep;
	char *state;
	char *hostdir;
	unsigned int proto = MICCTRL_SYSLOG_RESET;

	if (mpssenv.live_update == FALSE)
		return;

	if ((state = mpss_readsysfs(mic->name, "state")) == NULL)
		return;

	if (strcmp(state, "online")) {
		free(state);
		return;
	}

	free(state);
	mpss_sync_cookie(&mpsscookie, (uid_t)0);

	if ((ep = scif_open()) < 0) {
		display(PERROR, "Failed to open SCIF: %s\n", strerror(errno));
		return;
	}

	if (scif_connect(ep, &portID) < 0) {
		display(PERROR, "%s: [SysLog] Failed to connect to card's mpssd\n", mic->name);
		return;
	}

	if ((hostdir = micctrl_check_nfs_rootdev(mic)) == NULL) {
		syslog_sendfile(ep, mic->config.filesrc.mic.dir);
	} else {
		scif_send(ep, &proto, sizeof(proto), 0);
		scif_send(ep, &mpsscookie, sizeof(mpsscookie), SCIF_SEND_BLOCK);
		scif_recv(ep, &proto, sizeof(proto), SCIF_RECV_BLOCK);
	}

	scif_close(ep);
}

void
setsyslog(struct mic_info *miclist, char *type, char *host, char *logfile, int loglevel)
{
	struct mic_info *mic;
	char *hostdir;

	if ((loglevel < 0) || (loglevel > 8)) {
		display(PNORM, "\n");
		display(PERROR, "\n[Syslog] level must between 0 and 8\n");
		printf("%s\n", syslog_help);
		exit(0);
	}

	for (mic = miclist; mic != NULL; mic = mic->next) {
		micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO);
		hostdir = micctrl_check_nfs_rootdev(mic);

		if (!strcmp(type, "buffer")) {
			syslog_buffer(mic->name, mic->config.filesrc.mic.dir, loglevel);
			if (hostdir)
				syslog_buffer(mic->name, hostdir, loglevel);
		} else if (!strcmp(type, "file")) {
			syslog_file(mic->name, mic->config.filesrc.mic.dir, logfile, loglevel);
			if (hostdir)
				syslog_file(mic->name, hostdir, logfile, loglevel);
		} else if (!strcmp(type, "remote")) {
			if (host == NULL)
				host = "host";

			syslog_remote(mic->name, mic->config.filesrc.mic.dir, host, loglevel);
			if (hostdir)
				syslog_remote(mic->name, hostdir, host, loglevel);
		} else {
			display(PNORM, "\n");
			display(PERROR, "\n[Syslog] Must be set to buffer, file or remote\n");
			printf("%s\n", syslog_help);
			exit(0);
		}

		syslog_setremote(mic);
	}

	if (!strcmp(type, "remote"))
		display(PNORM, "Ensure the rsyslog settings on the target host have been configured\n");

	exit(0);
}

void
show_syslog(struct mic_info *miclist)
{
	struct mic_info *mic;
	char filename[PATH_MAX];

	for (mic = miclist; mic != NULL; mic = mic->next) {
		if (micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO)) {
			display(PWARN, "%s: Not configured - skipping\n", mic->name);
			continue;
		}

		if (!mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s/etc/syslog-startup.conf", mic->config.filesrc.mic.dir))
			printf("%s: Default", mic->name);
		else
			display_file(mic->config.filesrc.mic.dir, "/etc/syslog-startup.conf");

		putchar('\n');
	}

	putchar('\n');
	exit(0);
}

