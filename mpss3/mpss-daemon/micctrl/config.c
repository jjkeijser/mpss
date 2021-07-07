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
#include <mpssconfig.h>
#include <scif.h>
#include <libmpsscommon.h>
#include "micctrl.h"
#include "help.h"

/*
 * micctrl --config
 *
 * Display the current configuration information for the MIC card list in a
 * human readable format.
 */
char *showconfig_help =
"\
micctrl [global options] --config [Xeon Phi cards]\n\
\n\
The 'micctrl --config' displays a readble view of the current configuration.\n\
\n"
COMMON_GLOBAL
COMMON_HELP;

static struct option showconfig_longopts[] = {
	{"help", no_argument, NULL, 'h'},
	{0}
};

void display_config(struct mic_info *miclist);

void
parse_showconfig_args(int argc, char *argv[])
{
	struct mic_info *miclist;
	int longidx;
	int c;

	while ((c = getopt_long(argc, argv, "hv", showconfig_longopts, &longidx)) != -1) {
		switch(c) {
		case 'h':
			micctrl_help(showconfig_help);
			exit(0);

		case 'v':
			bump_display_level();
			break;

		default:
			ARGERROR_EXIT(showconfig_help);
		}
	}

	miclist = create_miclist(&mpssenv, argc, argv, optind, NULL);
	display_config(miclist);
}

char *
check_set(char *addr)
{
	if (addr == NULL)
		return "Not configured";

	return addr;
}

/*
 * Display current configuration parameters in a human readable format.
 */
void
display_config(struct mic_info *miclist)
{
	struct mic_info *mic;
	struct mbridge *br;
	char netmask[16];
	char micmac[64];
	char hostmac[64];
	int err = 0;
	int ret = 0;

	putchar('\n');
	mic = miclist;
	while (mic != NULL) {
		printf("%s:\n", mic->name);
		printf("=============================================================\n");

		if ((ret = micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO))) {
			err++;

			if (ret < MIC_PARSE_ERRORS) {
				display(PERROR, "Not configured\n");
				goto nextmic;
			}
		}

		printf("    Config Version: %d.%d\n\n", (mic->config.version >> 16) & 0xff, mic->config.version & 0xff);
		printf("    Linux Kernel:   %s\n", check_set(mic->config.boot.osimage));
		printf("    Map File:       %s\n", check_set(mic->config.boot.systemmap));
		printf("    Family:         %s\n", family_to_str(mic->config.family));
		printf("    MPSSVersion:    %s\n", mpss_version_to_str(mic->config.mpss_version));

		printf("    BootOnStart:    %s\n",
			(mic->config.boot.onstart == TRUE)? "Enabled" : "Disabled");
		printf("    Shutdowntimeout: %s seconds\n", check_set(mic->config.misc.shutdowntimeout));

		printf("\n    ExtraCommandLine: %s\n", check_set(mic->config.boot.extraCmdline));
		printf("    PowerManagment: %s\n", check_set(mic->config.boot.pm));

		printf("\n    ");
		report_rootdev(mic);

		get_mac_strings(mic, micmac, hostmac, 64);

		switch (mic->config.net.type) {
		case NETWORK_TYPE_STATPAIR:
			printf("\n    Network:       Static Pair\n");
			printf("        Hostname:  %s\n", check_set(mic->config.net.hostname));
			printf("        MIC IP:    %s\n", check_set(mic->config.net.micIP));
			printf("        Host IP:   %s\n", check_set(mic->config.net.hostIP));

			if (mic->config.net.prefix == NULL)
				mic->config.net.prefix = "24";

			mpssnet_genmask(mic->config.net.prefix, netmask);
			printf("        Net Bits:  %s\n", check_set(mic->config.net.prefix));
			printf("        NetMask:   %s\n", netmask);

			if (mic->config.net.mtu)
				printf("        MtuSize:   %s\n", check_set(mic->config.net.mtu));

			break;

		case NETWORK_TYPE_STATBRIDGE:
			printf("\n    Network:       Static bridge %s\n", check_set(mic->config.net.bridge));
			printf("        MIC IP:    %s\n", check_set(mic->config.net.micIP));

			if ((br = mpss_bridge_byname(mic->config.net.bridge, brlist)) != NULL) {
				printf("        Host IP:   %s\n", check_set(br->ip));

				mpssnet_genmask(br->prefix, netmask);
				printf("        Net Bits:  %s\n", check_set(br->prefix));
				printf("        NetMask:   %s\n", netmask);
				printf("        MtuSize:   %s\n", check_set(br->mtu));
			}

			printf("        Hostname:  %s\n", check_set(mic->config.net.hostname));
			break;
		case NETWORK_TYPE_BRIDGE:
			printf("\n    Network:       Bridge %s with DHCP\n", check_set(mic->config.net.bridge));

			if ((br = mpss_bridge_byname(mic->config.net.bridge, brlist)) != NULL) {
				mpssnet_genmask(br->prefix, netmask);
				printf("        Net Bits:  %s\n", check_set(br->prefix));
				printf("        NetMask:   %s\n", netmask);
				printf("        MtuSize:   %s\n", check_set(br->mtu));
			}

			printf("        Hostname:  %s\n", check_set(mic->config.net.hostname));
			break;
		default:
			printf("%s: Unknown network configuration type\n", mic->name);
		}
		printf("        MIC MAC:   %s\n", micmac);
		printf("        Host MAC:  %s\n", hostmac);

		printf("\n    ");
		display_ldap(mic, TRUE);
		printf("     ");
		display_nis(mic, TRUE);

		printf("\n    Cgroup:\n");
		printf("        Memory:    %s\n", (mic->config.cgroup.memory == TRUE)? "Enabled" : "Disabled");

		printf("\n    Console:        %s\n", check_set(mic->config.boot.console));

		printf("    VerboseLogging: %s\n",
			(mic->config.boot.verbose == TRUE)? "Enabled" : "Disabled");

		if (mic->config.misc.crashdumpDir != NULL) 
			printf("    CrashDump:      %s %sGB\n", check_set(mic->config.misc.crashdumpDir),
								check_set(mic->config.misc.crashdumplimitgb));
		else
			printf("    CrashDump:      Not Enabled\n");
nextmic:
		putchar('\n');
		mic = mic->next;
	}

	exit(err);
}

