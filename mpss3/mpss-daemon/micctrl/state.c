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
#include <libmpsscommon.h>
#include "micctrl.h"
#include "help.h"

static void boot(struct mic_info *miclist, int timeout, char *ip);
static void boot_wait(struct mic_info *miclist, int options, int timeout, int parseconfig);
static void reset(struct mic_info *miclist, int timeout, int force, int ignore);
static void micshutdown(struct mic_info *miclist, int timeout);
static void micreboot(struct mic_info *miclist, int timeout, char *ip);
static void status(struct mic_info *miclist);

#ifdef MIC_IS_EMULATION
#define BOOT_WAIT_TIMEOUT (INT_MAX)
#else
#define BOOT_WAIT_TIMEOUT (0)
#endif

#define OPTION_REBOOT	0x1
#define OPTION_SHUTDOWN	0x2

/*
 * Section 1: Top level parse routines for state manipulation
 */

/*
 * This function parses command line for 4 different commands:
 *
 * micctrl --boot [-w] [-t <timout>] [MIC list]
 *
 * Boot the specified MIC cards and optionally wait for completion
 *
 *     micctrl --reset [-w] [-f] [-t <timout>] [MIC list]
 *
 * Reset the specified MIC cards and optionally wait for completion
 *
 *     micctrl --shutdown [-w] [-t <timout>] [MIC list]
 *
 * Shutdown the specified MIC cards and optionally wait for completion
 *
 *     micctrl --reboot [-w] [-t <timout>] [MIC list]
 *
 * Reboot the specified MIC cards and optionally wait for completion
 */
char *state_help[] = {
/* Boot Help */
"\
micctrl [global options] -b [sub options] <Xeon Phi card>\n\
micctrl [global options] --boot [sub options] <Xeon Phi card>\n\
\n\
The --boot option uses configuration parameters to prepare and boot Xeon Phi\n\
cards.  Its exact process is dependent on the root device type specified by the\n\
RootDevice parameter and its associated type.\n\
\n\
    RamFS    Use the Base, CommonDir, Overlay and MicDir parameters to create a\n\
             ramdisk image for the root file system.  Creates the kernel command\n\
        line in the sysfs file system and writes to the state entry in the sysfs\n\
        file system to tell the mic driver to boot the card with the specified\n\
        Linux kernel image and ram disk image.\n\
\n\
    StaticRamFS   Create the kernel command line and write it to the sysfs and\n\
                  write to the sysfs state entry to tell the mic driver to boot\n\
        boot the miccard with the Linux kernel image and the provided ram disk\n\
        image.\n\
\n\
    NFS and SplitNFS   Create the kernel command line from the RootDevice and\n\
                       Network parameters and write it to the sysfs.  Write to\n\
        the sysfs state entry to indicate to boot using the Linux kernel image\n\
        and the default init ram disk image provided by the mpss-boot-files RPM.\n\
\n"
COMMON_GLOBAL
COMMON_HELP
"\
    -w\n\
    --wait\n\
\n\
        Wait for boot to complete before exiting the micctrl command.\n\
\n\
    -t <timeout>\n\
    --timeout=<timeout>\n\
\n\
        If the -w option is specified then this will set the time out value for\n\
        waiting to the specified number of seconds.  The default is to wait\n\
        forever.\n\
\n\
    --ip=<ip>\n\
\n\
	If the -n option is specified then value is used as a MIC card\n\
	IP address.\n\
	This opion is valid only for KNL cards.\n\
\n",

/* Reset Help */
"\
micctrl [global options] -r [sub options] <Xeon Phi card>\n\
micctrl [global options] --reset [sub options] <Xeon Phi card>\n\
\n\
The --reset option requests the Xeon Phi cards to reset.\n\
\n"
COMMON_HELP
"\
    -w\n\
    --wait\n\
\n\
        Wait for reset to complete before exiting the micctrl command.\n\
\n\
    -t <timeout>\n\
    --timeout=<timeout>\n\
\n\
        If the -w option is specified then this will set the time out value for\n\
        waiting to the specified number of seconds.  The default is to wait\n\
        forever.\n\
\n\
    -f\n\
    --force\n\
\n\
         Force the Xeon Phi cards to go through the reset process.  Normally the\n\
         driver will not reset the cards if they are already in the resetted or\n\
         'ready' state and micctrl will return an error.\n\
    -i\n\
    --ignore\n\
\n\
        If one or more of the cards are already in the 'ready' or reset state\n\
        do not return an error for the reset request.\n\
\n"
,
/* Shutdown Help */
"\
micctrl [global options] -S [sub options] <Xeon Phi card>\n\
micctrl [global options] --shutdown [sub options] <Xeon Phi card>\n\
\n\
The --shutdown option requests the Xeon Phi cards to shutdown.\n\
\n"
COMMON_HELP
"\
    -w\n\
    --wait\n\
\n\
        Wait for shutdown to complete before exiting the micctrl command.\n\
\n\
    -t <timeout>\n\
    --timeout=<timeout>\n\
\n\
        If the -w option is specified then this will set the time out value for\n\
        waiting to the specified number of seconds.  The default is to wait\n\
        forever.\n\
\n",
/* Reboot Help */
"\
micctrl [global options] -R [sub options] <Xeon Phi card>\n\
micctrl [global options] --reboot [sub options] <Xeon Phi card>\n\
\n\
The --reboot option requests the Xeon Phi cards to reboot.\n\
\n"
COMMON_HELP
"\
    -w\n\
    --wait\n\
\n\
        Wait for reboot to complete before exiting the micctrl command.\n\
\n\
    -t <timeout>\n\
    --timeout=<timeout>\n\
\n\
        If the -w option is specified then this will set the time out value for\n\
        waiting to the specified number of seconds.  The default is to wait\n\
        forever.\n\
\n\
    --ip=<ip>\n\
\n\
	If the -n option is specified then value is used as a MIC card\n\
	IP address.\n\
	This opion is valid only for KNL cards.\n\
\n"
};

static struct option state_longopts[] = {
	{"wait", no_argument, NULL, 'w'},
	{"timeout", required_argument, NULL, 't'},
	{"force", no_argument, NULL, 'f'},
	{"ignore", no_argument, NULL, 'i'},
	{"help", no_argument, NULL, 'h'},
	{"ip", required_argument, NULL, 0x1000},
	{0}
};

void
parse_state_args(int mode, int argc, char *argv[])
{
	struct mic_info *miclist;
	int miccnt;
	int longidx;
	int c;
	int wait = FALSE;
	int force = FALSE;
	int ignore = FALSE;
	int timeout = BOOT_WAIT_TIMEOUT;
	char *ip = NULL;

	while ((c = getopt_long(argc, argv, "hwfivtn:", state_longopts, &longidx)) != -1) {
		switch(c) {
		case 'h':
			micctrl_help(state_help[mode]);
			exit(0);
			break;

		case 'w':
			wait = TRUE;
			break;

		case 'f':
			if (mode != STATE_RESET) {
				printf("Illegal option '%c'\n", c);
				exit(1);
			}
			force = TRUE;
			break;

		case 'i':
			if (mode != STATE_RESET) {
				printf("Illegal option '%c'\n", c);
				exit(1);
			}
			ignore = TRUE;
			break;

		case 't':
			timeout = atoi(optarg);

			// 30 minutes is a long time to wait
		        if (timeout < 0 || timeout > (30 * 60)) {
				display(PERROR, "Timeouts longer than 30 minutes are not allowed\n");
				exit(3);
			}
			break;

		case 'v':
			bump_display_level();
			break;
		case 0x1000:
			ip = optarg;
			break;

		default:
			ARGERROR_EXIT(state_help[mode]);
		}
	}

	if (mpssenv.destdir != NULL) {
		display(PERROR, "Function not available with DESTDIR set\n");
		EXIT_ERROR(EINVAL);
	}
		
	check_rootid();
	miclist = create_miclist(&mpssenv, argc, argv, optind, &miccnt);

	if (ip != NULL) {
		ARGERROR_EXIT(state_help[mode]);
	}

	if ((wait || (mode == STATE_REBOOT)) && (timeout == 0)) {
		timeout = 90 + (60 * (miccnt - 1));
		if (timeout > 600)
			timeout = 600;
		if (timeout < 300)
			timeout = 300;
	}

	switch (mode) {
	case STATE_BOOT:
		boot(miclist, timeout, ip);
	case STATE_RESET:
		reset(miclist, timeout, force, ignore);
	case STATE_SHUTDOWN:
		micshutdown(miclist, timeout);
	case STATE_REBOOT:
		micreboot(miclist, timeout, ip);
	}
}

/*
 * micctrl --wait [-t <timeout> [MIC list]
 *
 * Wait for one or more MIC cards to not be in the 'resetting' or 'booting' states
 */
char *wait_help =
"\
micctrl [global options] -w [sub options] <Xeon Phi card>\n\
micctrl [global options] --wait [sub options] <Xeon Phi card>\n\
\n\
The --wait option waits for the prevous state change commnd to complete.  If\n\
--boot, --reset, --shutdown or --reboot command did not specify to wait this\n\
will do the same process..\n\
\n"
COMMON_HELP
"\
    -t <timeout>\n\
    --timeout=<timeout>\n\
\n\
        If the -w option is specified then this will set the time out value for\n\
        waiting to the specified number of seconds.  The default is to wait\n\
        forever.\n\
\n";

static struct option wait_longopts[] = {
	{"timeout", required_argument, NULL, 't'},
	{"help", no_argument, NULL, 'h'},
	{0}
};

void
parse_wait_args(int argc, char *argv[])
{
	struct mic_info *miclist;
	int miccnt;
	int longidx;
	int c;
	int timeout = BOOT_WAIT_TIMEOUT;

	while ((c = getopt_long(argc, argv, "hvt:", wait_longopts, &longidx)) != -1) {
		switch(c) {
		case 'h':
			micctrl_help(wait_help);
			exit(0);

		case 't':
			timeout = atoi(optarg);

			// 30 minutes is a long time to wait
		        if (timeout < 0 || timeout > (30 * 60)) {
				display(PERROR, "Timeouts longer than 30 minutes are not allowed\n");
				exit(3);
			}
			break;

		case 'v':
			bump_display_level();
			break;

		default:
			ARGERROR_EXIT(wait_help);
		}
	}

	if (mpssenv.destdir != NULL) {
		display(PERROR, "Function not available with DESTDIR set\n");
		EXIT_ERROR(EINVAL);
	}
		
	miclist = create_miclist(&mpssenv, argc, argv, optind, &miccnt);

	if (timeout == 0) {
		timeout = 90 + (60 * (miccnt - 1));
		if (timeout > 600)
			timeout = 600;
		if (timeout < 300)
			timeout = 300;
	}

	boot_wait(miclist, 0, timeout, TRUE);
}

/*
 * micctrl --status [MIC list]
 *
 * Display the status (state) of the listed MIC cards
 */
char *status_help =
"\
micctrl [global options] -s [sub options] <Xeon Phi card>\n\
micctrl [global options] --status [sub options] <Xeon Phi card>\n\
\n\
The --status option displays the state of the Xeon Phi cards in the system\n\
\n"
COMMON_HELP;

static struct option status_longopts[] = {
	{"help", no_argument, NULL, 'h'},
	{0}
};

void
parse_status_args(int argc, char *argv[])
{
	struct mic_info *miclist;
	int longidx;
	int c;

	while ((c = getopt_long(argc, argv, "hv", status_longopts, &longidx)) != -1) {
		switch(c) {
		case 'h':
			micctrl_help(status_help);
			exit(0);

		case 'v':
			bump_display_level();
			break;

		default:
			ARGERROR_EXIT(status_help);
		}
	}

	if (mpssenv.destdir != NULL) {
		display(PERROR, "Function not available with DESTDIR set\n");
		EXIT_ERROR(EINVAL);
	}
		
	miclist = create_miclist(&mpssenv, argc, argv, optind, NULL);
	status(miclist);
}

/*
 * Section 2: Worker functions for state manipulation
 */

static void
boot(struct mic_info *miclist, int timeout, char *ip)
{
	struct mic_info *mic;
	char boot_string[PATH_MAX];
	char cmdline[2048];
	struct stat sbuf;
	char *initrd = NULL;
	char *errmsg;
	int fail = 0;
	char *sysfs_rd;

	if (!mpssenv_aquire_lockfile(&mpssenv)) {
		display(PERROR, "Cannot boot cards - mpssd daemon is not running\n");
		exit(1);
	}

	for (mic = miclist; mic != NULL; mic = mic->next) {
		if (micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO)) {
			display(PERROR, "%s: Boot aborted - no configuation file present\n", mic->name);
			fail++;
			continue;
		}

		if (mic->config.boot.osimage == NULL) {
			display(PERROR, "%s: Boot aborted - OsImage parameter not set\n", mic->name);
			fail++;
			continue;
		}

		if (stat(mic->config.boot.osimage, &sbuf) != 0) {
			display(PERROR, "%s: Boot aborted - %s not found\n", mic->name, mic->config.boot.osimage);
			fail++;
			continue;
		}

		if (verify_bzImage(&mpssenv, mic->config.boot.osimage, mic->name)) {
			display(PERROR, "%s: Boot aborted - %s is not a k1om Linux bzImage\n",
					mic->name, mic->config.boot.osimage);
			fail++;
			continue;
		}

		if ((sysfs_rd = mpss_readsysfs(mic->name, "state")) == NULL) {
			display(PERROR, "%s: Boot aborted - non existent MIC device\n", mic->name);
			fail++;
			continue;
		}

		if ((errmsg = mpss_set_cmdline(mic, brlist, cmdline, ip)) != NULL) {
			display(PERROR, "%s: Boot aborted - Setting kernel command line failed: %s\n",
				mic->name, errmsg);
			fail++;
			continue;
		}

		switch (mic->config.rootdev.type) {
		case ROOT_TYPE_RAMFS:
			if (stat(mic->config.rootdev.target, &sbuf) == 0)
				unlink(mic->config.rootdev.target);
			mpssfs_gen_initrd(&mpssenv, mic, &mpssperr);
			mpss_print_elist(&mpssperr, PINFO, display);
			mpss_clear_elist(&mpssperr);
		case ROOT_TYPE_STATICRAMFS:
			initrd = mic->config.rootdev.target;
			break;

		case ROOT_TYPE_NFS:
		case ROOT_TYPE_SPLITNFS:
		case ROOT_TYPE_PFS:
			initrd = mic->config.filesrc.base.image;
			break;
		}

		display(PINFO, "%s: Command line: %s\n", mic->name, cmdline);
		display(PNORM, "%s: booting %s\n", mic->name, mic->config.boot.osimage);

		if (initrd == NULL) {
			display(PERROR, "%s Boot aborted - initial ramdisk not set", mic->name);
			fail++;
			continue;
		}

		snprintf(boot_string, PATH_MAX, "boot:linux:%s:%s", mic->config.boot.osimage, initrd);

		if (mpss_setsysfs(mic->name, "state", boot_string)) {
			sysfs_rd = mpss_readsysfs(mic->name, "state");
			display(PERROR, "%s Boot failed - card state %s\n", mic->name, sysfs_rd);
			free(sysfs_rd);
			fail++;
		}
	}

	if (timeout)
		boot_wait(miclist, 0, timeout, FALSE);

	exit(fail);
}

/*
 * Reset the list of MIC cards.  If a timeout value is specified then wait for the cards to
 * complete booting.  If the timeout is exceeded then print an error and exit.
 */
static void
reset(struct mic_info *miclist, int timeout, int force, int ignore)
{
	struct mic_info *mic;
	char *state;
	int fail = 0;
	int err;

	for (mic = miclist; mic != NULL; mic = mic->next) {
		if (force)
			err = mpss_setsysfs(mic->name, "state", "reset:force");
		else
			err = mpss_setsysfs(mic->name, "state", "reset");

		if ((state = mpss_readsysfs(mic->name, "state")) == NULL) {
			display(PERROR, "%s: Reset aborted - non existent MIC device\n", mic->name);
			fail++;
			continue;
		}

		if ((err == EACCES) && !strcmp(state, "ready")) {
			if (!ignore) {
				display(PERROR, "%s Reset failed - card currently in the ready state.\n%s%s\n",
					mic->name, display_indent(), "Try the -f or --force option\n");
				fail++;
			} else {
				display(PERROR, "%s: Already reset\n", mic->name);
			}
		} else if (err) {
			display(PERROR, "%s Reset failed - card state %s\n", mic->name, state);
			fail++;
		} else {
			display(PNORM, "%s: resetting\n", mic->name);
		}

		free(state);
	}

	if (timeout)
		boot_wait(miclist, 0, timeout, FALSE);

	exit(fail);
}

/*
 * Shutdown the list of MIC cards.  If a timeout value is specified then wait for the cards to
 * complete booting.  If the timeout is exceeded then print an error and exit.
 */
static void
micshutdown(struct mic_info *miclist, int timeout)
{
	struct mic_info *mic;
	char *state;
	int fail = 0;
	int err;

	for (mic = miclist; mic != NULL; mic = mic->next) {
		err = mpss_setsysfs(mic->name, "state", "shutdown");
		if (err == 1) {
			display(PERROR, "%s: Shutdown failed - non existent MIC device\n", mic->name);
			fail++;
		} else if (err) {
			state = mpss_readsysfs(mic->name, "state");
			display(PERROR, "%s Shutdown failed - card state %s\n", mic->name, state);
			free(state);
			fail++;
		} else {
			display(PNORM, "%s: Shutting down\n", mic->name);
		}
	}

	if (timeout)
		boot_wait(miclist, OPTION_SHUTDOWN, timeout, FALSE);

	exit(fail);
}

/*
 * Reboot the list of MIC cards.  If a timeout value is specified then wait for the cards to
 * complete booting.  If the timeout is exceeded then print an error and exit.
 */
static void
micreboot(struct mic_info *miclist, int timeout, char *ip)
{
	struct mic_info *mic;
	char *state;
	int fail = 0;
	int err;

	for (mic = miclist; mic != NULL; mic = mic->next) {
		err = mpss_setsysfs(mic->name, "state", "shutdown");
		if (err == 1) {
			display(PERROR, "%s: Reboot failed - non existent MIC device\n", mic->name);
			fail++;
		} else if (err) {
			state = mpss_readsysfs(mic->name, "state");
			display(PERROR, "%s Reset failed - card state %s\n", mic->name, state);
			free(state);
			fail++;
		} else {
			display(PNORM, "%s: shutting down\n", mic->name);
		}
	}

	boot_wait(miclist, OPTION_REBOOT, timeout, FALSE);
	boot(miclist, timeout, ip);
	exit(fail);
}

/*
 * Display the status (state) of the MIC cards.
 */
static void
status(struct mic_info *miclist)
{
	struct mic_info *mic;
	char *state;
	char *mode;
	char *image;
	int fail = 0;

	for (mic = miclist; mic != NULL; mic = mic->next) {
		state = mpss_readsysfs(mic->name, "state");
		if (state == NULL) {
			fail++;
			display(PERROR, "%s: State failed - non existent MIC device\n", mic->name);
		} else if (!strcmp(state, "online") ||
			   !strcmp(state, "booting")) {

			mode = mpss_readsysfs(mic->name, "mode");
			image = mpss_readsysfs(mic->name, "image");

			printf("%s: %s (mode: %s image: %s)\n", mic->name, state, mode, image);
			free(mode);
			free(image);
			free(state);
		} else {
			printf("%s: %s\n", mic->name, state);
			free(state);
		}
	}

	exit(fail);
}

/*
 * Wait for the list of MIC cards to be in either the "Ready" or "Boot" states.
 * Do not forget to handle the temperary transition through Ready to Booting.
 */
static char *states[256];

static void
boot_wait(struct mic_info *miclist, int options, int timeout, int parseconfig)
{
	struct mic_info *mic;
	int bad_state = 1;

	for (mic = miclist; mic != NULL; mic = mic->next) {
		if (parseconfig)
			micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO);

		if ((states[mic->id] = mpss_readsysfs(mic->name, "state")) != NULL) {
			if (!strcmp(states[mic->id], "shutdown") ||
			    (options & OPTION_REBOOT) ||
			    (options & OPTION_SHUTDOWN)) {
				/* zero timeout implies forced reset and default timeout */
				if (mic->config.misc.shutdowntimeout && atoi(mic->config.misc.shutdowntimeout))
					timeout = atoi(mic->config.misc.shutdowntimeout);
			}
			free(states[mic->id]);
		} else {
			display(PERROR, "%s: Wait failed - non existent MIC device\n", mic->name);
		}
	}

	while (bad_state) {
		bad_state = 0;

		for (mic = miclist; mic != NULL; mic = mic->next) {
			if ((states[mic->id] = mpss_readsysfs(mic->name, "state")) == NULL)
				continue;

			if (!strcmp(states[mic->id], "ready")) {
				sleep(2);
				free(states[mic->id]);
				states[mic->id] = mpss_readsysfs(mic->name, "state");
			}

			if (!strcmp(states[mic->id], "booting")) {
				bad_state++;
			} else if (!strcmp(states[mic->id], "resetting")) {
				bad_state++;
			} else if (!strcmp(states[mic->id], "shutdown")) {
				bad_state++;
			} else if (!strcmp(states[mic->id], "shutting_down")) {
				bad_state++;
			}
		}

		if (bad_state) {
			for (mic = miclist; mic != NULL; mic = mic->next)
				if (states[mic->id] != NULL)
					free(states[mic->id]);
		}

		sleep(1);
		/* -ve timeout implies an infinite wait */
		if (timeout > 0)
			timeout--;
		if (!timeout)
			break;
	}

	if (!timeout && bad_state) {
		display(PERROR, "Timeout booting MIC, check your installation\n");
		exit(EAGAIN);
	}

	bad_state = 0;

	for (mic = miclist; mic != NULL; mic = mic->next) {
		if (states[mic->id]) {
			if (strcmp(states[mic->id], "online") && strcmp(states[mic->id], "ready"))
				bad_state++;

			display(PNORM, "%s: %s\n", mic->name, states[mic->id]);
		}
	}

	if ((options & OPTION_REBOOT) && !bad_state)
		return;
	else
		exit(bad_state);
}
