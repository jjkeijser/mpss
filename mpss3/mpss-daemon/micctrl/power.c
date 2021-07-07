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
#include <ctype.h>
#include <mpssconfig.h>
#include <libmpsscommon.h>
#include "micctrl.h"
#include "help.h"

void show_pm(struct mic_info *miclist);
void config_power(char *opt, char *cpufreq, char *corec6, char *pc3, char *pc6, struct mic_info *mic);

char *pm_help =
"\
micctrl [global options] --pm=<action> [sub options] <Xeon Phi card>\n\
\n\
The --pm option sets the PowerManagement configuration parameter. The indicated\n\
action determines the use of the suboptions.\n\
\n\
    default    Set the power managment parameters back to default.  If the\n\
               stepping of the Xeon Phi card can be determined the setting will\n\
               be card specific.  If not they will default to the standard\n\
               settings for a C stepping Xeon Phi card.\n\
\n\
    defaultb   Set the power management parameters for B stepping Xeon Phe\n\
               cards.\n\
\n\
    set        Use the sub options to individually set the power management\n\
               parameters.\n\
\n"
COMMON_GLOBAL
COMMON_HELP
"\
    -f <on | off>\n\
    --cpufreq=<on | off>\n\
\n\
        Set the cpu frequency power managment setting.\n\
\n\
    -c <on | off>\n\
    --corec6=<on | off>\n\
\n\
        Set the corec6 power managment setting.\n\
\n\
    -t <on | off>\n\
    --pc3=<on | off>\n\
\n\
        Set the pc3 power managment setting.\n\
\n\
    -s <on | off>\n\
    --pc6=<on | off>\n\
\n\
        Set the pc6 power managment setting.\n\
\n";

static struct option power_longopts[] = {
	{"help", no_argument, NULL, 'h'},
	{"cpufreq", required_argument, NULL, 'f'},
	{"corec6", required_argument, NULL, 'c'},
	{"pc3", required_argument, NULL, 't'},
	{"pc6", required_argument, NULL, 's'},
	{0}
};

void
parse_power_args(char *opt, int argc, char *argv[])
{
	int longidx;
	int c;
	int cnt;
	char *cpufreq = NULL;
	char *corec6 = NULL;
	char *pc3 = NULL;
	char *pc6 = NULL;
	struct mic_info *miclist;

	while ((c = getopt_long(argc, argv, "hvf:c:t:s:", power_longopts, &longidx)) != -1) {
		switch(c) {
		case 'h':
			micctrl_help(pm_help);
			exit(0);

		case 'f':
			cpufreq = optarg;
			break;

		case 'c':
			corec6 = optarg;
			break;

		case 't':
			pc3 = optarg;
			break;

		case 's':
			pc6 = optarg;
			break;

		case 'v':
			bump_display_level();
			break;

		default:
			ARGERROR_EXIT(pm_help);
		}
	}

	miclist = create_miclist(&mpssenv, argc, argv, optind, &cnt);


	if (cpufreq || corec6 || pc3 || pc6) {
		if ((opt == NULL) || strcmp(opt, "set")) {
			printf("\n");
			display(PERROR, "Setting any one of cpufreq, corec6, pc3 or pc6 requires '--pm=set'\n");
			printf("%s\n", pm_help);
			exit(1);
		}
	}

	if (opt == NULL)
		show_pm(miclist);

	check_rootid();
	config_power(opt, cpufreq, corec6, pc3, pc6, miclist);
}

void
show_pm(struct mic_info *miclist)
{
	struct mic_info *mic = miclist;

	putchar('\n');
	while (mic != NULL) {
		micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO);
		printf("%s: %s\n", mic->name, mic->config.boot.pm);
		mic = mic->next;
	}

	putchar('\n');
	exit(0);
}

#define DEFAULT_PM_C6ON_CONFIG_STRING "cpufreq_on;corec6_on;pc3_on;pc6_on"
#define DEFAULT_PM_C6OFF_CONFIG_STRING "cpufreq_on;corec6_off;pc3_on;pc6_off"
#define DEFAULT_PM_OFF_CONFIG_STRING "cpufreq_on;corec6_off;pc3_off;pc6_off"

void
default_power(char *opt, struct mic_info *miclist)
{
	struct mic_info *mic = miclist;
	char *stepping;
	char *value;
	int err;

	while (mic != NULL) {
		micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO);

		if (!strcasecmp(opt, "defaultb")) {
			value = DEFAULT_PM_C6OFF_CONFIG_STRING;
		} else {
			stepping = mpss_readsysfs(mic->name, "stepping");

			if ((stepping == NULL) || (*stepping >= 'C'))
				value = DEFAULT_PM_C6ON_CONFIG_STRING;
			else
				value = DEFAULT_PM_C6OFF_CONFIG_STRING;

			if (stepping) free(stepping);
		}

		if ((err = mpss_update_config(&mpssenv, mic->config.name, "PowerManagement", NULL,
								"PowerManagement \"%s\"\n", value)))
			display(PERROR, "Failed to open config file '%s': %d\n", mic->config.name, err);

		mic = mic->next;
	}
	exit(0);
}

void
off_power(char *opt, struct mic_info *miclist)
{
	struct mic_info *mic = miclist;

	while (mic != NULL) {
		micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO);
		mpss_update_config(&mpssenv, mic->config.name, "PowerManagement", NULL,
				   "PowerManagement \"%s\"\n", DEFAULT_PM_OFF_CONFIG_STRING);
		mic = mic->next;
	}
	exit(0);
}

void
config_power(char *opt, char *cpufreq, char *corec6, char *pc3, char *pc6, struct mic_info *miclist)
{
	struct mic_info *mic = miclist;
	char *ifreq;
	char *icorec6;
	char *ipc3;
	char *ipc6;

	if (!strcasecmp(opt, "default") || !strcasecmp(opt, "defaultb"))
		default_power(opt, miclist);

	if (!strcasecmp(opt, "off")) {
		off_power(opt, miclist);
	}

	if (strcasecmp(opt, "set")) {
		display(PERROR, "pm must be set to either 'default', 'off' or 'set'\n");
	}

	if (cpufreq) {
		if (strcasecmp(cpufreq, "on") && strcasecmp(cpufreq, "off")) {
			display(PERROR, "cpufreq must be 'on' or 'off'\n");
			EXIT_ERROR(EINVAL);
		}
	}

	if (corec6) {
		if (strcasecmp(corec6, "on") && strcasecmp(corec6, "off")) {
			display(PERROR, "corec6 must be 'on' or 'off'\n");
			EXIT_ERROR(EINVAL);
		}
	}

	if (pc3) {
		if (strcasecmp(pc3, "on") && strcasecmp(pc3, "off")) {
			display(PERROR, "pc3 must be 'on' or 'off'\n");
			EXIT_ERROR(EINVAL);
		}
	}

	if (pc6) {
		if (strcasecmp(pc6, "on") && strcasecmp(pc6, "off")) {
			display(PERROR, "pc6 must be 'on' or 'off'\n");
			EXIT_ERROR(EINVAL);
		}
	}

	while (mic != NULL) {
		micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO);

		/*
		 * if PowerManagement parameter doesn't exist - update config file with default values
		 * and then set the PowerManagement accordingly to values given to micctrl --pm=set option
		 */

		if (mic->config.boot.pm == NULL) {
			char *stepping;
			char *value;

			stepping = mpss_readsysfs(mic->name, "stepping");

			if ((stepping == NULL) || (*stepping >= 'C'))
				value = DEFAULT_PM_C6ON_CONFIG_STRING;
			else
				value = DEFAULT_PM_C6OFF_CONFIG_STRING;

			if (stepping) free(stepping);

			if (mpss_update_config(&mpssenv, mic->config.name, "PowerManagement", NULL, "PowerManagement \"%s\"\n", value)) {
				display(PERROR, "Failed to open config file '%s'\n", mic->config.name);
				goto nextmic;
			}
		}

		micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO);

		ifreq = mic->config.boot.pm;

		if (strncmp(ifreq, "cpufreq_", strlen("cpufreq_"))) {
			display(PERROR, "%s: Parse of old PowerManagement '%s' for 'cpufreq' failed\n",
				mic->name, mic->config.boot.pm);
			goto nextmic;
		}

		if ((icorec6 = strchr(mic->config.boot.pm, ';')) == NULL) {
			goto nextmic;
		}
		*icorec6++ = '\0';
		ifreq += strlen("cpufreq_");

		if (strncmp(icorec6, "corec6_", strlen("corec6_"))) {
			display(PERROR, "%s: Parse of old PowerManagement '%s' for 'corec6' failed\n",
				mic->name, mic->config.boot.pm);
			goto nextmic;
		}

		if ((ipc3 = strchr(icorec6, ';')) == NULL) {
			goto nextmic;
		}
		*ipc3++ = '\0';
		icorec6 += strlen("corec6_");

		if (strncmp(ipc3, "pc3_", strlen("pc3_"))) {
			display(PERROR, "%s: Parse of old PowerManagement '%s' for 'pc3' failed\n",
				mic->name, mic->config.boot.pm);
			goto nextmic;
		}

		if ((ipc6 = strchr(ipc3, ';')) == NULL) {
			goto nextmic;
		}
		*ipc6++ = '\0';
		ipc3 += strlen("pc3_");

		if (strncmp(ipc6, "pc6_", strlen("pc6_"))) {
			display(PERROR, "%s: Parse of old PowerManagement '%s' for 'pc6' failed\n",
				mic->name, mic->config.boot.pm);
			goto nextmic;
		}

		ipc6 += strlen("pc6_");

		if (cpufreq != NULL)
			ifreq = cpufreq;

		if (corec6 != NULL)
			icorec6 = corec6;

		if (pc3 != NULL)
			ipc3 = pc3;

		if (pc6 != NULL)
			ipc6 = pc6;

		mpss_update_config(&mpssenv, mic->config.name, "PowerManagement", NULL,
			"PowerManagement \"cpufreq_%s;corec6_%s;pc3_%s;pc6_%s\"\n", ifreq, icorec6, ipc3, ipc6);
nextmic:
		mic = mic->next;
	}

	exit(0);
}

void
set_pm(struct mic_info *mic, char *pm)
{
	//'Power Management' options are applicable only for KNC
	char *descriptor = "# Control card power state setting\n"
			   "# cpufreq: P state\n"
			   "# corec6: Core C6 state\n"
			   "# pc3: Package C3 state\n"
			   "# pc6: Package C6 state";
	char *value = DEFAULT_PM_C6ON_CONFIG_STRING;
	char *stepping;

	if (pm != NULL) {
		if (strcasecmp(pm, "defaultb")) {
			display(PWARN, "%s: pm value '%s' not understood - "
				       "assuming C0 or better hardware\n", mic->name, pm);
			value = DEFAULT_PM_C6ON_CONFIG_STRING;
		}
	} else {
		stepping = mpss_readsysfs(mic->name, "stepping");
		if (stepping && (stepping[0] >= 'C'))
			value = DEFAULT_PM_C6ON_CONFIG_STRING;
		else
			value = DEFAULT_PM_C6OFF_CONFIG_STRING;

		if (stepping) free(stepping);
	}

	if (mic->config.boot.pm == NULL) {
		if ((mic->config.boot.pm = (char *) malloc(strlen(value) + 1)) == NULL) {
			return;
		}
		strcpy(mic->config.boot.pm, value);
		mpss_update_config(&mpssenv, mic->config.name, NULL, descriptor, "%s %s\n\n",
				   "PowerManagement", value);
	}

	display(PINFO, "%s: PowerManagement %s\n", mic->name, mic->config.boot.pm);
	return;
}

