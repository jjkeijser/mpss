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
#include <ctype.h>
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

void initdefaults(struct mic_info *miclist, int user, int pass, char *shell, int modhost, char *modcard, char *pm,
		  char *mac, int createhome);
void resetconfig(struct mic_info *miclist, int user, int pass, char *shell, int modhost, char *modcard, char *pm,
		 char *mac, int createhome);
void resetdefaults(struct mic_info *miclist, int user, int pass, char *shell, int modhost, char *modcard, char *pm,
		   char *mac, int createhome);
void cleanup(struct mic_info *miclist);

/*
 * This function parses command line for 4 different commands:
 *
 * micctrl --initdefaults [MIC list]
 *
 * Create the initial configuration files.  Creates a common default.conf
 * file and then one for each card in the system.
 *
 * micctrl --resetconfig [MIC list]
 *
 * Parse the configuration files and make sure the configuration matches.  This
 * is still the only way to populate several configuration parameters including
 * network.  Resetconfig will also check for deprecated parameters and promote
 * them to current settings to match.
 *
 * micctrl --resetdefaults [MIC list]
 *
 * Reset back to defualt configuration.
 *
 * micctrl --cleanconfig [MIC list]
 *
 * Remove all configuration information.  This should always be done before
 * regressing to older releases.
 */
#define INIT_COMMON_HELPHEADER \
"\
To improve flexibility of the --initdefaults, --resetdefaults and --resetconfig\n\
options a series of sub options has been introduced.  The sub options all have\n\
default values matching older versions of the same option.\n\
\n"

#define INIT_COMMONSUBS \
"\
    -u <none | overlay | merge | nochange>\n\
    --users=<none | overlay | merge | nochange>\n\
\n\
        The --users option specifies the methodology of user creation in the\n\
        Xeon Phi cards /etc/passwd and /etc/shadow files located in the MicDir\n\
        directory.  If not set the default value is 'nochange'.\n\
\n\
        If --users is set to 'nochange' in most cases this indicates to not\n\
        have --initdefaults modify the /etc/passwd and /etc/shadows files.\n\
        The exception to this rule is if no configuration for the Xeon Phi card\n\
        currently exists the default value is set to 'overlay'.\n\
\n\
        If --users is set to 'none' the /etc/passwd and /etc/shadow files are\n\
        removed and recreated with the minimal set of users required by Linux.\n\
        The list includes users root, ssh, nobody, nfsnobody and micuser.\n\
\n\
        If --users is set to 'overlay' the /etc/passwd and /etc/shadow files are\n\
        removed and recreated with the users from the 'none' option and\n\
        any regular users found in the /etc/passwd file of the host.  It should\n\
        be noted this is the replacement functionality for what micctrl did in\n\
        the past with the exception it no longer scrounges the /home directory\n\
        for additional users.\n\
\n\
        If --users is set to 'merge', the hosts /etc/passwd file is checked for\n\
        any users not in the Xeon Phi /etc/passwd file and they added to the\n\
        existing users in the card's /etc/passwd and /etc/shadow files.\n\
\n\
    -a <none | shadow>\n\
    --pass=<none | shadow>\n\
\n\
        The --pass option selects the policy for copying the current pass words\n\
        from the host /etc/shadow file to the Xeon Phi /etc/shadow file. It\n\
        applies to the users specified by the --users option.\n\
\n\
	If the host is a Red Hat based system and the policy is not specified\n\
        the default of 'shadow' will be used.  If the host is a Suse based\n\
        system the the default will be set to 'none'.\n\
\n\
        If the policy is set to 'shadow' the host's /etc/shadow file will be\n\
        parsed and the values for the users will be written to the cards\n\
        /etc/shadow file.  It should be noted this value is disabled on Suse\n\
        host systems since they use Blow Fish encryption and it is not supported\n\
        on the card.\n\
\n\
        If the policy is set to 'none' the pass words in the /etc/shadow file\n\
        for the Xeon File cards will be set to the '*' character.\n\
\n\
    --shell=<shell>\n\
\n\
        The name of the user´s login shell. When not specified shell is copied\n\
	from host's /etc/passwd, otherwise this replaces user's default shell used to login.\n\
\n\
    -d <distrib>\n\
    --distrib=<distrib>\n\
\n\
        The --distrib option will take the values 'redhat' or 'suse'. If it is\n\
        not specified the type of distribution of the host is used.  This option\n\
        would normally be used in conjunction with other options such as\n\
        --destdir for configurations to be pushed to other systems.\n\
\n\
    -c <yes | no>\n\
    --modhost=<yes | no>\n\
\n\
        If --modhost is set to 'yes' the Xeon Phi cards 'hostname: IP address'\n\
        pair is created in the host's /etc/host file with the comment\n\
        '#Generated-by-micctrl'.\n\
\n\
        If --modhost is set to 'no' host's /etc/hosts file will not be changed.\n\
\n\
        The default value is 'yes' if modhost is not specified. Note specifying\n\
        modhost has no effect if the configuration files already exist. To\n\
        change the value use the micctrl --modbridge or --network options.\n\
\n\
    -e <yes | no | <file location>>\n\
    --modcard=<yes | no | <file location>>\n\
\n\
        If --modcard is set to 'yes' --initdefaults creates the /etc/hosts file\n\
        in the MicDir directory.\n\
\n\
        If --modcard is set to 'no' --initdefaults does not create the\n\
        /etc/hosts file in the MicDir directory.\n\
\n\
	If --modcard is set to a <file location> --initdefaults copies\n\
        given file to /etc/hosts file in the MicDir directory.\n\
\n\
        The default value is 'yes' if modcard is not specified. Note specifying\n\
        modcard has no effect if the configuration files already exist. To\n\
        change the value use the micctrl --modbridge or --network options.\n\
\n\
    --nocreate\n\
\n\
        Normally --initdefaults creates a home directory for each user it adds\n\
        to the /etc/passwd and /etc/shadow files on the Xeon Phi card file\n\
        system. The --nocreate option disables the creation of a home directory\n\
        and reduces ram file system memory usage when LDAP home directory auto\n\
        mount is enabled or the /home directory is NFS mounted.\n\
\n\
    -p <default | defaultb>\n\
    --pm=<default | defaultb>\n\
\n\
        Depending on the stepping of the Xeon Phi card there are two sets of\n\
        default power management settings.  The --pm option is only useful with\n\
        the 'defaultb' value to indicate to force the setting to support the 'B'\n\
        steppings of the card.  Not specifying --pm causes micctrl to ask the\n\
        mic.ko driver for the stepping of the card and if it fails will set\n\
        power management to 'C' stepping default values.\n\
\n"

char *config_help[] = {
/* Initdefaults Help */
"\
micctrl [global options] --initdefaults [sub options] [List of Xeon Phi cards]\n\
\n\
The 'micctrl --initdefaults' command has two purposes.  If not configuration\n\
currently exists it creates the initial values in the MPSS configuration files\n\
and the initial file system additions uniquely identifying the particular\n\
instance for a Xeon Phi card. If a configuration already exists and the MPSS\n\
software is updated it updates the configuration files from the old release to\n\
the newer release. This means if a particular configuration parameter format has\n\
changed, or the file system results have changed, it will update them to the\n\
newest values or formats.\n\
\n\
The micctrl --initdefaults command will not change existing configuration settings,\n\
with the following exception: The MPSS configuration file format is versioned,\n\
with the version indicated by a Version parameter in the configuration file.\n\
If a configuration already exists, then micctrl --initdefaults will update\n\
the configuration format if necessary. The semantics of the updated configuration\n\
should be invariant.\n\
\n"
INIT_COMMON_HELPHEADER
COMMON_GLOBAL
COMMON_HELP
VARDIR_HELP
SRCDIR_HELP
NETDIR_HELP_ENTRY
    "    -n <netdir>\n"
NETDIR_HELP_BODY
INIT_COMMONSUBS,
/* Resetconfig Help */
"\
micctrl [global options] --resetconfig [sub options] [List of Xeon Phi cards]\n\
\n\
The 'micctrl --resetconfig' command in intended to allow the system\n\
administrator to make configuration changes and it that parameter needs other\n\
to complete it perform them.  With the exception of the Bride, Network and\n\
RootDevice parameters  the changes will not need a --resetconfig.  The micctrl\n\
command now offers other options for setting and correctly populating the\n\
MPSS configuration parameters and the use of --resetconfig has been deprecated\n\
and will be removed at sometime.\n\
\n\
Using --resetconfig may also be problematic and cause other issues.  For example\n\
every time it is executed it resets the network configuration.  If it is\n\
resetting to a new configuration entered by the system administrator it will not\n\
have adequate information to tear down the old configuration and may leave\n\
incomplete parts laying around.\n\
\n"
INIT_COMMON_HELPHEADER
COMMON_GLOBAL
COMMON_HELP
VARDIR_HELP
SRCDIR_HELP
NETDIR_HELP_ENTRY
    "    -n <netdir>\n"
NETDIR_HELP_BODY
INIT_COMMONSUBS,
/* Resetdefaults Help */
"\
micctrl [global options] --resetdefaults [sub options] [List of Xeon Phi cards]\n\
\n\
The --resetdefaults command attempts to restore configuration parameters and the\n\
associated Xeon Phi file systems back to the default state.  It shutdowns down\n\
the current network, removes a few files in the cards /etc/ directory, removes\n\
the old configuration files and call the --initdefaults option.\n\
\n\
It tries to leave an system admin or user created files alone.  If it is wished\n\
to truly remove and reinitialize a Xeon Phi cards configuration the use micctrl\n\
with the --cleanconfig and then the --initdefaults options.\n\
"
INIT_COMMON_HELPHEADER
COMMON_GLOBAL
COMMON_HELP
VARDIR_HELP
SRCDIR_HELP
NETDIR_HELP_ENTRY
    "    -n <netdir>\n"
NETDIR_HELP_BODY
INIT_COMMONSUBS,
/* Cleanconfig Help */
"\
micctrl [global options] --cleanconfig [List of Xeon Phi cards]\n\
\n\
The --cleanconfig command removes the configuration associated with a Xeon Phi\n\
card.\n\
\n\
    Valid configuration file found:\n\
\n\
       If a valid configuration is found a series of steps are performed.\n\
\n\
       1.  Shutdown the network and remove configuration file in the host's\n\
           network configuration directory.\n\
       2.  Remove all the fines in the directory defined by the MicDir\n\
           configuration parameter.  Warning this will also remove all the files\n\
           in that directory not created by micctrl.\n\
       3.  Remove the ramfs file or the NFS export directory associated with NFS\n\
           type in the RootDev configuration parameter.\n\
       4.  Remove the configuration file in /etc/mpss directory (or the\n\
           directory defined by the --configdir option).\n\
       5.  If there are no more configuration files in the /etc/mpss directory\n\
           then remove the contents of the directory defined by the CommonDir\n\
           parameter and remove the default.conf file from the /etc/mpss\n\
           directory.\n\
\n\
    No valid configuration files\n\
\n\
       If no valid configuration file is found for the card a more brute force\n\
       method of clean up is performed based on some assumptions.\n\
\n\
       1.  Append the Xeon Phi card identifier (mic0, mic1, etc.) to the\n\
           '/var/mpss' directory name (or the name defined by --vardir) and\n\
           remove the entire contents of the directory.\n\
       2.  Append the identfier to '/var/mpss' and then append '.image.gz'\n\
           again and remove it from the file system.\n\
       3.  Append the identfier to '/var/mpss' and then append '.export' again\n\
           and remove it from the file system.\n\
       4.  Remove the configuration file in /etc/mpss directory (or the\n\
           directory defined by the --configdir option).\n\
       5.  If there are no more configuration files in the /etc/mpss directory\n\
           then remove the contents of the directory defined by the CommonDir\n\
           parameter and remove the default.conf file from the /etc/mpss\n\
           directory.\n\
\n\
Sub Options:\n\
\n"
VARDIR_HELP
SRCDIR_HELP
NETDIR_HELP_ENTRY
    "    -n <netdir>\n"
NETDIR_HELP_BODY
};

static struct option config_longopts[] = {
	{"help", no_argument, NULL, 'h'},
	{"vardir", required_argument, NULL, 't'},
	{"netdir", required_argument, NULL, 'n'},
	{"srcdir", required_argument, NULL, 's'},
	{"users", required_argument, NULL, 'u'},
	{"pass", required_argument, NULL, 'a'},
	{"modhost", required_argument, NULL, 'c'},
	{"modcard", required_argument, NULL, 'e'},
	{"distrib", required_argument, NULL, 'd'},
	{"pm", required_argument, NULL, 'p'},
	{"mac", required_argument, NULL, 'm'},
	{"nocreate", no_argument, NULL, 0x1001},
	{"shell", required_argument, NULL, 0x1003},
	{0}
};

void
parse_config_args(int mode, int argc, char *argv[])
{
	struct mic_info *miclist;
	char *pm = NULL;
	char *mac = NULL;
	char *shell = NULL;
	int user = USERS_NOCHANGE;
	int pass = TRUE;
	int modhost = NOTSET;
	char *modcard = NULL;
	int createhome = TRUE;
	int longidx;
	int err;
	int c;

	while ((c = getopt_long(argc, argv, "hvt:n:s:c:d:p:m:e:a:u:", config_longopts, &longidx)) !=-1) {
		switch(c) {
		case 'h':
			micctrl_help(config_help[mode]);
			exit(0);

		case 't':
			if ((err = mpssenv_set_vardir(&mpssenv, optarg))) {
				display(PERROR, "Unable to set vardir %s: %s\n", optarg,
						mpssenv_errstr(err));
				exit(EEXIST);
			}

			break;

		case 'n':
			set_local_netdir(mpssut_tilde(&mpssenv, optarg));
			break;

		case 's':
			if ((err = mpssenv_set_srcdir(&mpssenv, optarg))) {
				display(PERROR, "Unable to set srcdir %s: %s\n", optarg,
						mpssenv_errstr(err));
				exit(EEXIST);
			}

			break;

		case 'd':
			set_alt_distrib(optarg);
			break;

		case 'u':
			if (!strcasecmp(optarg, "none")) {
				user = USERS_NONE;
			} else if (!strcasecmp(optarg, "overlay")) {
				user = USERS_OVERWRITE;
			} else if (!strcasecmp(optarg, "merge")) {
				user = USERS_MERGE;
			} else if (!strcasecmp(optarg, "nochange")) {
				user = USERS_NOCHANGE;
			} else {
				display(PERROR, "user option must be 'none', 'overlay', 'merge' or 'nochange'\n");
				EXIT_ERROR(EINVAL);
			}
			break;
		case 'a':
			if (!strcasecmp(optarg, "none")) {
				pass = FALSE;
			} else if (!strcasecmp(optarg, "shadow")) {
				pass = TRUE;
			} else {
				display(PERROR, "pass option must be either a 'none' or 'shadow' string\n");
				EXIT_ERROR(EINVAL);
			}
			break;

		case 'c':
			if (!strcasecmp(optarg, "yes")) {
				modhost = TRUE;
			} else if (!strcasecmp(optarg, "no")) {
				modhost = FALSE;
			} else {
				display(PERROR, "modhost option must be a yes or no string\n");
				EXIT_ERROR(EINVAL);
			}
			break;

		case 'e':
			modcard = optarg;
			break;

		case 'p':
			pm = optarg;
			break;

		case 'm':
			mac = optarg;
			break;

		case 0x1001:
			createhome = FALSE;
			break;
		case 0x1003:
			shell = optarg;
			break;

		case 'v':
			bump_display_level();
			break;

		default:
			ARGERROR_EXIT(config_help[mode]);
		}
	}

	check_rootid();

	if (mpssenv_aquire_lockfile(&mpssenv)) {
		display(PERROR, "initdefaults, resetconfig, resetdefaults and cleanconfig are "
				"not available when mpssd daemon is running.\n");
		exit(EPERM);
	}

	miclist = create_miclist(&mpssenv, argc, argv, optind, NULL);

	switch (mode) {
	case CONFIG_INITDEFS:
		initdefaults(miclist, user, pass, shell, modhost, modcard, pm, mac, createhome);
		break;
	case CONFIG_RESETCONF:
		resetconfig(miclist, user, pass, shell,  modhost, modcard, pm, mac, createhome);
		break;
	case CONFIG_RESETDEFS:
		resetdefaults(miclist, user, pass, shell,  modhost, modcard, pm, mac, createhome);
		break;
	case CONFIG_CLEANCONF:
		cleanup(miclist);
	}

	exit(0);
}


int create_default_config_files(struct mic_info *mic);
void check_1_1(struct mic_info *mic);
void set_base(struct mic_info *mic);
void set_family(struct mic_info *mic);
void set_mpss_version(struct mic_info *mic);
void set_commondir(struct mic_info *mic);
void set_micdir(struct mic_info *mic);
void set_hostname(struct mic_info *mic);
void check_config(struct mic_info *mic);
void set_verbose(struct mic_info *mic);
void set_osimage(struct mic_info *mic);
void set_bootonstart(struct mic_info *mic);
void set_shutdown_timeout(struct mic_info *mic);
void set_crashdump(struct mic_info *mic);
void gen_fstab(struct mic_info *mic);
void gen_resolvconf(struct mic_info *mic);
void gen_nsswitch(struct mic_info *mic);
void gen_common_auth(struct mic_info *mic);
void gen_common_account(struct mic_info *mic);
void gen_common_session(struct mic_info *mic);
void gen_localtime(struct mic_info *mic);
void set_extracmdline(struct mic_info *mic);
void set_console(struct mic_info *mic);
void set_cgroup(struct mic_info *mic);
void check_services(struct mic_info *mic);


/*
 * Create the initial defaults for all cards in the system.
 */
void
_initdefaults(struct mic_info *miclist, int user, int pass, char *shell, int modhost, char *modcard, char *pm, char *mac, int createhome)
{
	struct mic_info *mic;

	mic = miclist;
	while (mic != NULL) {
		if (!mic->config.valid) {
			mic = mic->next;
			continue;
		}

		create_default_config_files(mic);
		if (micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PERROR) == MIC_PARSE_FAIL) {
			display(PWARN, "%s: Parse configuration failed - skipping\n", mic->name);
			mic = mic->next;
			continue;
		}

		check_config(mic);
		set_base(mic);
		set_family(mic);
		set_mpss_version(mic);
		set_commondir(mic);
		set_micdir(mic);

		mic = mic->next;
	}

	/* TODO:  This should only be neccessary for the resetconfig option */
	set_bridge_config(miclist);

	mic = miclist;
	while (mic != NULL) {
		if (!mic->config.valid) {
			mic = mic->next;
			continue;
		}

		set_hostname(mic);
		set_macaddrs(mic, mac);
		set_net_config(mic, miclist, modhost, modcard);
		mic = mic->next;
	}

	mic = miclist;
	while (mic != NULL) {
		if (!mic->config.valid) {
			mic = mic->next;
			continue;
		}

		set_verbose(mic);
		set_osimage(mic);
		set_bootonstart(mic);
		set_shutdown_timeout(mic);
		set_crashdump(mic);
		gen_fstab(mic);
		gen_users(mic->name, mic->config.filesrc.mic.dir, user, pass, shell, createhome);
		gen_resolvconf(mic);
		gen_nsswitch(mic);
		gen_common_auth(mic);
		gen_common_account(mic);
		gen_common_session(mic);
		gen_hostkeys(mic);
		gen_localtime(mic);
		set_extracmdline(mic);
		set_rootdev(mic);
		set_console(mic);
		set_pm(mic, pm);
		set_cgroup(mic);
		check_services(mic);
		mic = mic->next;
	}

	gen_etchosts(miclist);
}

/*
 * Create the initial defaults for all cards in the system.
 */
void
initdefaults(struct mic_info *miclist, int user, int pass, char *shell, int modhost, char *modcard, char *pm, char *mac, int createhome)
{
	struct mic_info *mic;

	mic = miclist;
	while (mic != NULL) {
		mpss_clear_config(&mic->config);
		mic = mic->next;
	}

	_initdefaults(miclist, user, pass, shell, modhost, modcard, pm, mac, createhome);
}

/*
 * Set all configuration information to parameters in the configuration files.  Should
 * be run after a MPSS stack update to ensure deprecated parameters get changed into
 * current ones.
 */
void delete_defaults(struct mic_info *miclist);
void delete_conffile(char *name, int backup);
void delete_conffiles(struct mic_info *miclist);
int check_no_configs(void);

void
resetconfig(struct mic_info *miclist, int user, int pass, char *shell, int modhost, char *modcard, char *pm, char *mac, int createhome)
{
	delete_defaults(miclist);
	_initdefaults(miclist, user, pass, shell, modhost, modcard, pm, mac, createhome);
	exit(0);
}

/*
 * Reset all configuration parameters back to the defaults.
 */
void
resetdefaults(struct mic_info *miclist, int user, int pass, char *shell, int modhost, char *modcard, char *pm, char *mac, int createhome)
{
	delete_defaults(miclist);
	delete_conffiles(miclist);
	_initdefaults(miclist, user, pass, shell, modhost, modcard, pm, mac, createhome);
	exit(0);
}

/*
 * Completely cleanup the MIC configuration.  Also removes all files in MicDir.
 */
void
cleanup(struct mic_info *miclist)
{
	struct mic_info *mic;
	char filename[PATH_MAX];
	char *path;
	int err;

	mic = miclist;
	while (mic != NULL) {
		if ((err = micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PERROR)) != MIC_PARSE_SUCCESS) {
			if (mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s/%s", mpssenv.vardir, mic->name)) {
				display(PFS, "%s: Remove directory %s\n", mic->name, filename);
				snprintf(filename, PATH_MAX, "%s/%s", mpssenv.vardir, mic->name);
				mpssut_deltree(&mpssenv, filename);
			}

			if (mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s/%s.image.gz",
										mpssenv.vardir, mic->name)) {
				display(PFS, "%s: Remove %s\n", mic->name, filename);
				unlink(filename);
			}

			if (mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s/%s.export",
										mpssenv.vardir, mic->name)) {
				display(PFS, "%s: Remove %s\n", mic->name, filename);
				mpssut_deltree(&mpssenv, filename);
			}
		} else {
			remove_network(mic);

			if (mic->config.filesrc.mic.dir != NULL)
				if (mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s",
										mic->config.filesrc.mic.dir)) {
					display(PFS, "%s: Remove directory %s\n", mic->name, filename);
					mpssut_deltree(&mpssenv, filename);
				}

			if (mic->config.rootdev.target != NULL) {
				if ((path = strchr(mic->config.rootdev.target, ':')) == NULL)
					path = mic->config.rootdev.target;
				else
					path++;

				if (mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s", path)) {
					display(PFS, "%s: Remove directory %s\n", mic->name, filename);
					mpssut_deltree(&mpssenv, filename);
				}
			}

			if (mic->config.rootdev.nfsusr != NULL) {
				if ((path = strchr(mic->config.rootdev.nfsusr, ':') + 1) != NULL) {
					if (mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s", path)) {
						display(PFS, "%s: Remove directory %s\n", mic->name, filename);
						mpssut_deltree(&mpssenv, filename);
					}
				}
			}
		}

		delete_conffile(mic->name, FALSE);
		mic = mic->next;
	}

	remove_bridges();

	if (check_no_configs()) {
		delete_conffile("default", FALSE);

		if (mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s/%s", mpssenv.vardir, "common")) {
			display(PFS, "%s: Remove directory %s\n", "default", filename);
			mpssut_deltree(&mpssenv, filename);
		}
	}
}

/*
 * Returns true if no config are valid config file names of the pattern mic<id>.conf where
 * id is all digits.
 */
int
check_no_configs(void)
{
	char confdir[PATH_MAX];
	char *id;
	char *ext;
	struct dirent *file;
	DIR *dp;
	int ret = TRUE;
	int nondigit = FALSE;

	mpssut_filename(&mpssenv, NULL, confdir, PATH_MAX, "%s", mpssenv.confdir);

	if ((dp = opendir(confdir))) {
		while ((file = readdir(dp)) != NULL) {
			ext = &file->d_name[strlen(file->d_name) - 5];

			if (ext < file->d_name )
				continue;

			if (strncmp(file->d_name, "mic", 3))
				continue;

			if (strncmp(ext, ".conf", 5))
				continue;

			for (id = file->d_name + 3; id < ext; id++)
				if (!isdigit(*id)) nondigit = TRUE;

			if (nondigit == FALSE)
				ret = FALSE;
		}

		closedir(dp);
	}

	return ret;
}

// List of automatically configured files to be recreated during a reset
char *acf[] = {
	"/etc/fstab",
	"/etc/hosts",
	"/etc/localtime",
	"/etc/resolv.conf",
	"/etc/nsswitch.conf",
	"/etc/hostname",
	"/etc/network/interfaces"
};
#define LEN_ACF (sizeof(acf) / sizeof(acf[0]))

// List of automatically configured files to be remain persistent during a reset
char *pf[] = {
	"/etc/ssh/ssh_host_dsa_key",
	"/etc/ssh/ssh_host_dsa_key.pub",
	"/etc/ssh/ssh_host_ecdsa_key",
	"/etc/ssh/ssh_host_ecdsa_key.pub",
	"/etc/ssh/ssh_host_key",
	"/etc/ssh/ssh_host_key.pub",
	"/etc/ssh/ssh_host_rsa_key",
	"/etc/ssh/ssh_host_rsa_key.pub"
};
#define LEN_PF (sizeof(pf) / sizeof(pf[0]))

/*
 * Work horse function for delete_defaults.  Cleans out the MicDir information for
 * a MIC card.
 */
void
_delete_defaults(struct mic_info *mic)
{
	char filename[PATH_MAX];
	char sfilename[PATH_MAX];
	unsigned int loop;

	for (loop = 0; loop < LEN_ACF; loop++) {
		if (!mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s%s", mic->config.filesrc.mic.dir, acf[loop]))
			continue;

		unlink(filename);
		display(PFS, "%s: Remove %s\n", mic->name, filename);
	}

	for (loop = 0; loop < LEN_PF; loop++) {
		if (!mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s%s", mic->config.filesrc.mic.dir, pf[loop]))
			continue;

		// If file is there then move it to ,save
		if (mpssut_filename(&mpssenv, NULL, sfilename, PATH_MAX, "%s%s.save",
				  mic->config.filesrc.mic.dir, pf[loop]))
			unlink(sfilename);

		if (mpssut_rename(&mpssenv, filename, sfilename)) {
				display(PERROR, "Failed to rename temporary file %s\n", sfilename);
				continue;
		}
		display(PFS, "%s: Save %s as %s\n", mic->name, filename, sfilename);
	}

	// Since the defaults will no longer use the common and mic? .filelist files get rid of them
	if (mic->config.filesrc.mic.list != NULL) {
		if (mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s", mic->config.filesrc.mic.list)) {
			display(PWARN, "%s: MicDir parameter no longer requires 'filelist' option. Removing %s\n",
					mic->name, filename);
			unlink(filename);
		}

		mic->config.filesrc.mic.type = SRCTYPE_DIR;
		set_home_owners(mic->config.filesrc.mic.dir);
	}

	if (mic->config.filesrc.common.list != NULL) {
		if (mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s", mic->config.filesrc.common.list)) {
			display(PWARN, "%s: CommonDir parameter no longer requires 'filelist' option. Removing %s\n",
					mic->name, filename);
			unlink(filename);
		}

		mic->config.filesrc.common.type = SRCTYPE_DIR;
	}
}

/*
 * Keep the mac information to make it highly persistent.  When the new configation is
 * created it will check for the saved values instead of randomly creating a new
 * MAC address.
 */
void
_save_mac(struct mic_info *mic)
{
	if (mic->config.persist.micMac)
		free(mic->config.persist.micMac);

	if (mic->config.net.micMac) {
		mic->config.persist.micMac = mic->config.net.micMac;
		mic->config.net.micMac = NULL;
	}

	if (mic->config.persist.hostMac)
		free(mic->config.persist.hostMac);

	if (mic->config.net.hostMac) {
		mic->config.persist.hostMac = mic->config.net.hostMac;
		mic->config.net.hostMac = NULL;
	}
}

/*
 * Delete all the configuration information from the MicDir directory.  Then rerun
 * init_config to get the card list back.
 */
void
delete_defaults(struct mic_info *miclist)
{
	struct mic_info *mic;

	mic = miclist;
	while (mic != NULL) {
		switch (micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PERROR)) {
		case MIC_PARSE_FAIL:
			display(PWARN, "%s: Parse configuration failed - skipping\n", mic->name);
			mic = mic->next;
			continue;
		case MIC_PARSE_EMPTY:
			display(PWARN, "%s: Not configured - skipping deletes\n", mic->name);
			goto nextmic;
		}

		_save_mac(mic);
		remove_network(mic);
		_delete_defaults(mic);
		mpss_clear_config(&mic->config);
nextmic:
		mic = mic->next;
	}

	remove_bridges();
}

/*
 * Delete the configuration files.
 */
void
delete_conffiles(struct mic_info *miclist)
{
	struct mic_info *mic;

	mic = miclist;
	while (mic != NULL) {
		if (mic->config.valid == FALSE) {
			mpss_clear_config(&mic->config);
		}

		delete_conffile(mic->name, TRUE);
		mic = mic->next;
	}

	if (check_no_configs()) {
		delete_conffile("default", TRUE);
	}
}

void
delete_conffile(char *name, int backup)
{
	char filename[PATH_MAX];
	char origname[PATH_MAX];

	if (!mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s/%s.conf", mpssenv.confdir, name))
		return;

	if (mpssut_filename(&mpssenv, NULL, origname, PATH_MAX, "%s/%s.conf.orig", mpssenv.confdir, name))
		unlink(origname);

	if (backup) {
		if (mpssut_rename(&mpssenv, filename, origname)) {
				display(PERROR, "Failed to rename temporary file %s\n", origname);
				return;
		}
		display(PFS, "%s: Rename %s to %s\n", name, filename, origname);
	} else {
		unlink(filename);
		display(PFS, "%s: Remove %s\n", name, filename);
	}
}

char *defconf =
	"# Common /etc files for all embedded Linux file systems\n"
	"CommonDir %s/common\n\n"
	"# MIC Shutdown timeout - Wait for orderly shutdown to complete\n"
	"# via service MPSS stop/unload and micctrl --shutdown or --reboot and --wait\n"
	"# +ve integer -> Time in seconds to wait for shutdown to complete before forcing reset\n"
	"# -ve integer -> Infinite wait for orderly shutdown to complete\n"
	"# 0           -> Forced shutdown or reset. NOT RECOMMENDED!\n"
	"ShutdownTimeout %d\n\n"
	"# Storage location and size for MIC kernel crash dumps\n"
	"CrashDump %s %d\n\n";

char *kncextracmd =
	"# MIC Console\n"
	"Console \"hvc0\"\n\n"
	"ExtraCommandLine \"highres=off noautogroup\"\n\n";

char *micconfhead =
	"# Include configuration common to all MIC cards\n"
	"Include default.conf\n\n"
	"# Include all additional functionality configuration files by default\n"
	"Include \"conf.d/*.conf\"\n\n";

#define CDCF_SUCCESS		0
#define CDCF_DEFAULT_OPEN_FAIL	1
#define CDCF_MIC_OPEN_FAIL	2

int
create_default_config_files(struct mic_info *mic)
{
	char filename[PATH_MAX];
	FILE *fp;

	if (!mpssut_mksubtree(&mpssenv, "", mpssenv.confdir, 0, 0, 0755))
		display(PFS, "%s: Created directory %s\n", mic->name, mpssenv.confdir);

	if (!mpssut_mksubtree(&mpssenv, "", mpssenv.vardir, 0, 0, 0755))
		display(PFS, "%s: Created directory %s\n", mic->name, mpssenv.vardir);

	if (!mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s/default.conf", mpssenv.confdir)) {
		if ((fp = fopen(filename, "w")) == NULL) {
			display(PERROR, "%s: Open fail %s: %s\n", mic->name, filename, strerror(errno));
			return CDCF_DEFAULT_OPEN_FAIL;
		}

		fprintf(fp, defconf, mpssenv.vardir, DEF_SHUTDOWN_TIMEOUT, DEF_CRASHDUMP_DIR, DEF_CRASHDUMP_LIMITGB);
		fprintf(fp, kncextracmd);
		fclose(fp);
		display(PFS, "%s: Created %s\n", mic->name, filename);
	} else {
		display(PINFO, "%s: Using existing %s\n", mic->name, filename);
	}

	if (mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s/%s.conf", mpssenv.confdir, mic->name)) {
		display(PINFO, "%s: Using existing %s\n", mic->name, filename);
		return CDCF_SUCCESS;
	}

	if ((fp = fopen(filename, "w")) == NULL) {
		display(PERROR, "%s: Open fail %s: %s\n", mic->name, filename, strerror(errno));
		return CDCF_MIC_OPEN_FAIL;
	}

	fprintf(fp, "Version %d %d\n\n", CURRENT_CONFIG_MAJOR, CURRENT_CONFIG_MINOR);
	fprintf(fp, "%s", micconfhead);
	fclose(fp);
	display(PFS, "%s: Created %s version %d.%d\n", mic->name, filename,
							 CURRENT_CONFIG_MAJOR, CURRENT_CONFIG_MINOR);
	return CDCF_SUCCESS;
}

void
set_base(struct mic_info *mic)
{
	char *descriptor = "# Base filesystem for embedded Linux file system";
	char imagename[PATH_MAX];

	if (mic->config.filesrc.base.image == NULL) {
		snprintf(imagename, PATH_MAX, "%s/%s", mpssenv.srcdir, DEFAULT_INITRD_KNC);
		mic->config.filesrc.base.type = SRCTYPE_CPIO;
		mic->config.filesrc.base.image = mpssut_alloc_fill(imagename);
		mpss_update_config(&mpssenv, mic->config.name, NULL, descriptor, "%s %s %s\n\n",
				   "Base", "CPIO", mic->config.filesrc.base.image);
	}

	display(PINFO, "%s: File System Base %s\n", mic->name, mic->config.filesrc.base.image);

}

void
set_family(struct mic_info *mic)
{
	char *descriptor = "# Family type of MIC card";

	if (mic->config.family == MIC_FAMILY_UNKNOWN_VALUE) {
		mic->config.family = MIC_FAMILY_KNC_VALUE;
		mpss_update_config(&mpssenv, mic->config.name, NULL, descriptor, "%s %s\n\n",
				   "Family", family_to_str(mic->config.family));
	}

	display(PINFO, "%s: MIC Family %s\n", mic->name, family_to_str(mic->config.family));

}

void
set_mpss_version(struct mic_info *mic)
{
	char *descriptor = "# MPSS version";

	if (mic->config.mpss_version == MPSS_VERSION_UNKNOWN_VALUE) {
		mic->config.mpss_version = MPSS_VERSION_3_VALUE;
		mpss_update_config(&mpssenv, mic->config.name, NULL, descriptor, "%s %s\n\n",
				   "MPSSVersion", mpss_version_to_str(mic->config.mpss_version));
	}

	display(PINFO, "%s: MPSSVersion %s\n", mic->name, mpss_version_to_str(mic->config.mpss_version));

}

void
set_commondir(struct mic_info *mic)
{
	char *descriptor = "# Common /etc files for all embedded Linux file systems";
	char commonname[PATH_MAX];

	if (mic->config.filesrc.common.dir == NULL) {
		snprintf(commonname, PATH_MAX, "%s/common", mpssenv.vardir);
		mic->config.filesrc.common.dir = mpssut_alloc_fill(commonname);
		mpss_update_config(&mpssenv, mic->config.name, NULL, descriptor, "%s %s\n\n",
				   "CommonDir", mic->config.filesrc.common.dir);
	}

	display(PINFO, "%s: Common files at %s\n", mic->name, mic->config.filesrc.common.dir);

	if (mpssut_mkdir(&mpssenv, mic->config.filesrc.common.dir, 0, 0, 0755) == 0) {
		mpssut_filename(&mpssenv, NULL, commonname, PATH_MAX, "%s", mic->config.filesrc.common.dir);
		display(PFS, "%s: Created directory %s\n", mic->name, commonname);
	}
}

void
set_micdir(struct mic_info *mic)
{
	char *descriptor = "# Unique per card files for embedded Linux file system";
	char micname[PATH_MAX];

	if (mic->config.filesrc.mic.dir == NULL) {
		snprintf(micname, PATH_MAX, "%s/%s", mpssenv.vardir, mic->name);
		mic->config.filesrc.mic.dir = mpssut_alloc_fill(micname);
		mpss_update_config(&mpssenv, mic->config.name, NULL, descriptor, "%s %s\n\n",
				   "MicDir", mic->config.filesrc.mic.dir);
	}

	display(PINFO, "%s: Unique files at %s\n", mic->name, mic->config.filesrc.mic.dir);

	if (mpssut_mkdir(&mpssenv, mic->config.filesrc.mic.dir, 0, 0, 0755) == 0) {
		mpssut_filename(&mpssenv, NULL, micname, PATH_MAX, "%s", mic->config.filesrc.mic.dir);
		display(PFS, "%s: Created directory %s\n", mic->name, micname);
	}

	snprintf(micname, PATH_MAX, "%s/etc", mic->config.filesrc.mic.dir);
	if (mpssut_mkdir(&mpssenv, micname, 0, 0, 0755) == 0) {
		mpssut_filename(&mpssenv, NULL, micname, PATH_MAX, "%s/etc", mic->config.filesrc.mic.dir);
		display(PFS, "%s: Created directory %s\n", mic->name, micname);
	}

	snprintf(micname, PATH_MAX, "%s/etc/init.d", mic->config.filesrc.mic.dir);
	if (mpssut_mkdir(&mpssenv, micname, 0, 0, 0755) == 0) {
		mpssut_filename(&mpssenv, NULL, micname, PATH_MAX, "%s/etc/init.d", mic->config.filesrc.mic.dir);
		display(PFS, "%s: Created directory %s\n", mic->name, micname);
	}

	snprintf(micname, PATH_MAX, "%s/etc/rc1.d", mic->config.filesrc.mic.dir);
	if (mpssut_mkdir(&mpssenv, micname, 0, 0, 0755) == 0) {
		mpssut_filename(&mpssenv, NULL, micname, PATH_MAX, "%s/etc/rc1.d", mic->config.filesrc.mic.dir);
		display(PFS, "%s: Created directory %s\n", mic->name, micname);
	}

	snprintf(micname, PATH_MAX, "%s/etc/rc5.d", mic->config.filesrc.mic.dir);
	if (mpssut_mkdir(&mpssenv, micname, 0, 0, 0755) == 0) {
		mpssut_filename(&mpssenv, NULL, micname, PATH_MAX, "%s/etc/rc5.d", mic->config.filesrc.mic.dir);
		display(PFS, "%s: Created directory %s\n", mic->name, micname);
	}

	snprintf(micname, PATH_MAX, "%s/etc/network", mic->config.filesrc.mic.dir);
	if (mpssut_mkdir(&mpssenv, micname, 0, 0, 0755) == 0) {
		mpssut_filename(&mpssenv, NULL, micname, PATH_MAX, "%s/etc/network", mic->config.filesrc.mic.dir);
		display(PFS, "%s: Created directory %s\n", mic->name, micname);
	}

	snprintf(micname, PATH_MAX, "%s/etc/ssh", mic->config.filesrc.mic.dir);
	if (mpssut_mkdir(&mpssenv, micname, 0, 0, 0755) == 0) {
		mpssut_filename(&mpssenv, NULL, micname, PATH_MAX, "%s/etc/ssh", mic->config.filesrc.mic.dir);
		display(PFS, "%s: Created directory %s\n", mic->name, micname);
	}

	snprintf(micname, PATH_MAX, "%s/etc/pam.d", mic->config.filesrc.mic.dir);
	if (mpssut_mkdir(&mpssenv, micname, 0, 0, 0755) == 0) {
		mpssut_filename(&mpssenv, NULL, micname, PATH_MAX, "%s/etc/pam.d", mic->config.filesrc.mic.dir);
		display(PFS, "%s: Created directory %s\n", mic->name, micname);
	}

	snprintf(micname, PATH_MAX, "%s/home", mic->config.filesrc.mic.dir);
	if (mpssut_mkdir(&mpssenv, micname, 0, 0, 0755) == 0) {
		mpssut_filename(&mpssenv, NULL, micname, PATH_MAX, "%s/home", mic->config.filesrc.mic.dir);
		display(PFS, "%s: Created directory %s\n", mic->name, micname);
	}
}

void
set_hostname(struct mic_info *mic)
{
	struct utsname unamebuf;
	char hostname[PATH_MAX];
	char *dot;
	FILE *fp;
	char *descriptor = "# Hostname to assign to MIC card";

	if (mic->config.net.hostname != NULL)
		goto hostnamedisplay;

	uname(&unamebuf);

	if ((dot = strchr(unamebuf.nodename, '.')) != NULL)
		*dot++ = '\0';

	hostname[PATH_MAX - 1] = 0;
	if (!strcmp(unamebuf.nodename, "localhost")) {
		// Sys admin has not assigned a hostname - shame shame shame
		snprintf(hostname, PATH_MAX, "%s.local", mic->name);
		mic->config.net.hostname = mpssut_alloc_fill(hostname);
		goto hostnameappend;
	}

	if (dot)
		snprintf(hostname, PATH_MAX, "%s-%s.%s", unamebuf.nodename, mic->name, dot);
	else
		snprintf(hostname, PATH_MAX, "%s-%s", unamebuf.nodename, mic->name);

	mic->config.net.hostname = mpssut_alloc_fill(hostname);

hostnameappend:
	mpss_update_config(&mpssenv, mic->config.name, NULL, descriptor, "%s %s\n\n",
			   "HostName", mic->config.net.hostname);

hostnamedisplay:
	display(PINFO, "%s: Hostname %s\n", mic->name, mic->config.net.hostname);

	// Gen cards hostname file if it does not exist.
	if (mpssut_filename(&mpssenv, NULL, hostname, PATH_MAX, "%s/etc/hostname",  mic->config.filesrc.mic.dir))
		return;

	if ((fp = fopen(hostname, "w")) == NULL) {
		display(PERROR, "%s: Open fail %s: %s\n", mic->name, hostname, strerror(errno));
		return;
	}

	fprintf(fp, "%s\n", mic->config.net.hostname);
	fclose(fp);

	display(PFS, "%s: Created %s\n", mic->name, hostname);
}

void
set_verbose(struct mic_info *mic)
{
	char *descriptor = "# MIC OS Verbose messages to console";

	if (mic->config.boot.verbose == -1) {
		mic->config.boot.verbose = FALSE;
		mpss_update_config(&mpssenv, mic->config.name, NULL, descriptor, "%s %s\n\n",
				   "VerboseLogging", "Disabled");
	}

	display(PINFO, "%s: Verbose mode %s\n", mic->name, (mic->config.boot.verbose == TRUE)? "Enabled" : "Disabled");
	return;
}

void
set_osimage(struct mic_info *mic)
{
	char *descriptor = "# MIC OS image";
	char bzname[PATH_MAX];
	char mapname[PATH_MAX];

	snprintf(bzname, PATH_MAX, "%s/%s", mpssenv.srcdir, DEFAULT_BZIMAGE_KNC);
	snprintf(mapname, PATH_MAX, "%s/%s", mpssenv.srcdir, DEFAULT_SYSMAP_KNC);

	if (mic->config.boot.osimage == NULL) {
		mic->config.boot.osimage = mpssut_alloc_fill(bzname);
		mic->config.boot.systemmap = mpssut_alloc_fill(mapname);
		mpss_update_config(&mpssenv, mic->config.name, NULL, descriptor, "%s %s %s\n\n", "OSimage",
				   mic->config.boot.osimage, mic->config.boot.systemmap);
	}

	display(PINFO, "%s: Linux OS image %s\n%s      System Map %s\n", mic->name,
		mic->config.boot.osimage, display_indent(), mic->config.boot.osimage);
}

void
set_bootonstart(struct mic_info *mic)
{
	char *descriptor = "# Boot MIC card when MPSS stack is started";

	if (mic->config.boot.onstart == -1) {
		mic->config.boot.onstart = TRUE;
		mpss_update_config(&mpssenv, mic->config.name, NULL, descriptor, "%s %s\n\n", "BootOnStart", "Enabled");
	}

	display(PINFO, "%s: Boot On Start %s\n", mic->name,
			(mic->config.boot.onstart == TRUE)? "Enabled" : "Disabled");
}

void
set_shutdown_timeout(struct mic_info *mic)
{
	char * descriptor = "# MIC Shutdown timeout - Wait for orderly shutdown to complete\n"
		"# via service MPSS stop/unload and micctrl --shutdown or --reboot and --wait\n"
		"# +ve integer -> Time in seconds to wait for shutdown to complete before forcing reset\n"
		"# -ve integer -> Infinite wait for orderly shutdown to complete\n"
		"# 0           -> Forced shutdown or reset. NOT RECOMMENDED!";
	char value[32];

	if (!mic->config.misc.shutdowntimeout) {
		snprintf(value, 32, "%d", DEF_SHUTDOWN_TIMEOUT);
		mic->config.misc.shutdowntimeout = mpssut_alloc_fill(value);
		mpss_update_config(&mpssenv, mic->config.name, NULL, descriptor, "%s %s\n\n", "ShutdownTimeout", value);
	}

	display(PINFO, "%s: Shutdown Timeout %s\n", mic->name, mic->config.misc.shutdowntimeout);
}

void
set_crashdump(struct mic_info *mic)
{
	char *descriptor = "# Storage location for MIC kernel crash dumps";
	char value[32];

	if (!mic->config.misc.crashdumpDir) {
		mic->config.misc.crashdumpDir = mpssut_alloc_fill(DEF_CRASHDUMP_DIR);
		snprintf(value, 32, "%d", DEF_CRASHDUMP_LIMITGB);
		mic->config.misc.crashdumplimitgb = mpssut_alloc_fill(value);
		mpss_update_config(&mpssenv, mic->config.name, NULL, descriptor, "%s %s %s\n\n", "CrashDump",
				   mic->config.misc.crashdumpDir, mic->config.misc.crashdumplimitgb);
	}

	display(PINFO, "%s: MIC Crash Dump at %s size %s\n", mic->name,
			mic->config.misc.crashdumpDir, mic->config.misc.crashdumplimitgb);
}

// TODO: How does this interact with NFS Root setup.
char *deffstab =
	"rootfs\t\t/\t\tauto\t\tdefaults\t\t1  1\n"
	"proc\t\t/proc\t\tproc\t\tdefaults\t\t0  0\n"
	"devpts\t\t/dev/pts\tdevpts\t\tmode=0620,gid=5\t\t0  0\n";

void
gen_fstab(struct mic_info *mic)
{
	char filename[PATH_MAX];
	FILE *fsfp = NULL;

	if (mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s/etc/fstab", mic->config.filesrc.mic.dir))
		return;

	if ((fsfp = fopen(filename, "w")) == NULL) {
		display(PERROR, "%s: Open fail %s: %s\n", mic->name, filename, strerror(errno));
		return;
	}

	fprintf(fsfp, "%s", deffstab);
	fclose(fsfp);
	display(PFS, "%s: Created %s\n", mic->name, filename);
}

#define RESOLVCONF_SRC "/etc/resolv.conf"

void
gen_resolvconf(struct mic_info *mic)
{
	char filename[PATH_MAX];
	struct stat sbuf;
	char *buffer;
	int rsize;
	int ifd;
	int ofd;

	if (mic->config.net.type == NETWORK_TYPE_BRIDGE) {
		// DHCP will create the resolve.conf file all on its own.
		return;
	}

	if (mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s/etc/resolv.conf", mic->config.filesrc.mic.dir))
		return;

	// Gen cards /etc/resolv.conf file
	if ((ofd = open(filename, O_WRONLY | O_CREAT, 0644)) < 0) {
		display(PWARN, "%s: Open fail %s: %s\n", mic->name, filename, strerror(errno));
		return;
	}

	if (stat(RESOLVCONF_SRC, &sbuf) != 0) {
		display(PWARN, "%s: Open fail %s: %s\n%s      Cannot copy to %s\n", mic->name, RESOLVCONF_SRC,
				strerror(errno), display_indent(), filename);
		close(ofd);
		return;
	}

	// Gen cards /etc/resolv.conf file
	if ((ifd = open(RESOLVCONF_SRC, O_RDONLY)) < 0) {
		display(PWARN, "%s: Open fail %s: %s\n%s      Cannot copy to %s\n", mic->name, RESOLVCONF_SRC,
				strerror(errno), display_indent(), filename);
		close(ofd);
		return;
	}

	if ((buffer = (char *) malloc(sbuf.st_size)) == NULL) {
		close(ifd);
		close(ofd);
		return;
	}

	rsize = read(ifd, buffer, sbuf.st_size);
	close(ifd);
	write(ofd, buffer, rsize);
	close(ofd);
	free(buffer);
	display(PFS, "%s: Created %s\n", mic->name, filename);
}

char *default_nsswitch =
	"passwd:         compat ldap nis\n"
	"group:          compat ldap nis\n"
	"shadow:         compat ldap nis\n\n"
	"hosts:          files dns\n"
	"networks:       files\n\n"
	"protocols:      db files\n"
	"services:       db files\n"
	"ethers:         db files\n"
	"rpc:            db files\n\n"
	"netgroup:       nis\n"
;

void
gen_nsswitch(struct mic_info *mic)
{
	char filename[PATH_MAX];
	FILE *fp;

	if (mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s/etc/nsswitch.conf", mic->config.filesrc.mic.dir))
		return;

	if ((fp = fopen(filename, "w")) == NULL) {
		display(PERROR, "%s: Open fail %s: %s\n", mic->name, filename, strerror(errno));
		return;
	}

	fprintf(fp, "%s", default_nsswitch);
	fclose(fp);
	display(PFS, "%s: Created %s\n", mic->name, filename);
}

char *default_common_auth =
	"#\n"
	"# /etc/pam.d/common-auth - authentication settings common to all services\n"
	"#\n"
	"# This file is included from other service-specific PAM config files,\n"
	"# and should contain a list of the authentication modules that define\n"
	"# the central authentication scheme for use on the system\n"
	"# (e.g., /etc/shadow, LDAP, Kerberos, etc.).  The default is to use the\n"
	"# traditional Unix authentication mechanisms.\n"
	"\n"
	"auth\trequired\t\t\tpam_permit.so\n"
	"auth\tsufficient\t\t\tpam_ldap.so\n"
	"auth\t[success=1 default=ignore]\tpam_unix.so nullok try_first_pass\n"
	"auth\trequisite\t\t\tpam_deny.so\n"
;

void
gen_common_auth(struct mic_info *mic)
{
	char filename[PATH_MAX];
	FILE *fp;

	if (mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s/etc/pam.d/common-auth", mic->config.filesrc.mic.dir))
		return;

	if ((fp = fopen(filename, "w")) == NULL) {
		display(PERROR, "%s: Open fail %s: %s\n", mic->name, filename, strerror(errno));
		return;
	}

	fprintf(fp, "%s", default_common_auth);
	fclose(fp);
	display(PFS, "%s: Created %s\n", mic->name, filename);
}

char *default_common_account =
	"#\n"
	"# /etc/pam.d/common-account - authorization settings common to all services\n"
	"#\n"
	"# This file is included from other service-specific PAM config files,\n"
	"# and should contain a list of the authorization modules that define\n"
	"# the central access policy for use on the system.  The default is to\n"
	"# only deny service to users whose accounts are expired in /etc/shadow.\n"
	"#\n"
	"# As of pam 1.0.1-6, this file is managed by pam-auth-update by default.\n"
	"# To take advantage of this, it is recommended that you configure any\n"
	"# local modules either before or after the default block, and use\n"
	"# pam-auth-update to manage selection of other modules.  See\n"
	"# pam-auth-update(8) for details.\n"
	"#\n\n"
	"# here are the per-package modules (the \"Primary\" block)\n"
	"account\t[success=1 new_authtok_reqd=done default=ignore]\tpam_unix.so \n"
	"# here's the fallback if no module succeeds\n"
	"account\trequisite\t\t\tpam_deny.so\n"
	"# prime the stack with a positive return value if there isn't one already;\n"
	"# this avoids us returning an error just because nothing sets a success code\n"
	"# since the modules above will each just jump around\n"
	"account\trequired\t\t\tpam_permit.so\n"
	"# and here are more per-package modules (the \"Additional\" block)\n"
	"# end of pam-auth-update config\n"
;

void
gen_common_account(struct mic_info *mic)
{
	char filename[PATH_MAX];
	FILE *fp;

	if (mpssut_filename(&mpssenv, NULL, filename, PATH_MAX,
					"%s/etc/pam.d/common-account", mic->config.filesrc.mic.dir))
		return;

	if ((fp = fopen(filename, "w")) == NULL) {
		display(PERROR, "%s: Open fail %s: %s\n", mic->name, filename, strerror(errno));
		return;
	}

	fprintf(fp, "%s", default_common_account);
	fclose(fp);
	display(PFS, "%s: Created %s\n", mic->name, filename);
}
	
char *default_common_session =
	"#\n"
	"# /etc/pam.d/common-session - session-related modules common to all services\n"
	"#\n"
	"# This file is included from other service-specific PAM config files,\n"
	"# and should contain a list of modules that define tasks to be performed\n"
	"# at the start and end of sessions of *any* kind (both interactive and\n"
	"# non-interactive).\n"
	"#\n\n"
	"# here are the per-package modules (the \"Primary\" block)\n"
	"session\t[default=1]\t\t\tpam_permit.so\n"
	"# here's the fallback if no module succeeds\n"
	"session\trequisite\t\t\tpam_deny.so\n"
	"# prime the stack with a positive return value if there isn't one already;\n"
	"# this avoids us returning an error just because nothing sets a success code\n"
	"# since the modules above will each just jump around\n"
	"session\trequired\t\t\tpam_permit.so\n"
	"# and here are more per-package modules (the \"Additional\" block)\n"
	"session\trequired\tpam_unix.so \n"
	"session\toptional\tpam_mkhomedir.so skel=/etc/skel umask=0077\n"
;

void
gen_common_session(struct mic_info *mic)
{
	char filename[PATH_MAX];
	FILE *fp;

	if (mpssut_filename(&mpssenv, NULL, filename, PATH_MAX,
					"%s/etc/pam.d/common-session", mic->config.filesrc.mic.dir))
		return;

	if ((fp = fopen(filename, "w")) == NULL) {
		display(PERROR, "%s: Open fail %s: %s\n", mic->name, filename, strerror(errno));
		return;
	}

	fprintf(fp, "%s", default_common_session);
	fclose(fp);
	display(PFS, "%s: Created %s\n", mic->name, filename);
}

#define LOCALTIME_SRC "/etc/localtime"
void
gen_localtime(struct mic_info *mic)
{
	char filename[PATH_MAX];
	struct stat sbuf;
	char *buffer;
	int filesize;
	int rsize;
	int ifd;
	int ofd;

	if (mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s/etc/localtime", mic->config.filesrc.mic.dir))
		return;

	if ((ofd = open(filename, O_WRONLY | O_CREAT, 0644)) < 0) {
		display(PINFO, "%s: Cannot open %s\n", mic->name, filename);
		return;
	}

	if (stat(LOCALTIME_SRC, &sbuf) != 0) {
		display(PWARN, "%s: Open fail %s: %s\n%s      Cannot copy to %s\n", mic->name, LOCALTIME_SRC,
			strerror(errno), display_indent(), filename);
		close(ofd);
		return;
	}

	if ((ifd = open(LOCALTIME_SRC, O_RDONLY)) < 0) {
		display(PWARN, "%s: Open fail %s: %s\n%s      Cannot copy to %s\n", mic->name, LOCALTIME_SRC,
			strerror(errno), display_indent(), filename);
		close(ofd);
		return;
	}

	filesize = sbuf.st_size + 1;

	if ((buffer = (char *) malloc(filesize)) == NULL) {
		close(ofd);
		close(ifd);
		return;
	}

	rsize = read(ifd, buffer, filesize);
	close(ifd);

	write(ofd, buffer, rsize);
	close(ofd);
	free(buffer);
	display(PFS, "%s: Created %s\n", mic->name, filename);
}

void
set_extracmdline(struct mic_info *mic)
{
	if (mic->config.boot.extraCmdline == NULL) {
		mic->config.boot.extraCmdline = mpssut_alloc_fill("");
	}

	display(PINFO, "%s: ExtraCommandLine '%s'\n", mic->name, mic->config.boot.extraCmdline);
}

#define DEFAULT_VCONS "hvc0"

void
set_console(struct mic_info *mic)
{
	//'Console' option is applicable only for KNC
	char *descriptor = "# MIC Console";

	if (mic->config.boot.console == NULL) {
		mic->config.boot.console = mpssut_alloc_fill(DEFAULT_VCONS);
		mpss_update_config(&mpssenv, mic->config.name, NULL, descriptor, "%s %s\n\n", "Console", DEFAULT_VCONS);
	}

	display(PINFO, "%s: Console %s\n", mic->name, mic->config.boot.console);
	return;
}

void
set_cgroup(struct mic_info *mic)
{

	if (mic->config.cgroup.memory < 0) {
		mpss_update_config(&mpssenv, mic->config.name, NULL, NULL, "%s %s\n\n", "Cgroup", "memory=disabled");
		mic->config.cgroup.memory = 0;
	}

	display(PINFO, "%s: Cgroup memory=%s\n", mic->name, (mic->config.cgroup.memory == TRUE)? "enabled" : "disabled");
	return;
}

#define TIMEZONE_SRC "/etc/localtime"

void
check_timezone(struct mic_info *mic)
{
	char filename[PATH_MAX];
	struct stat sbuf;
	char *buffer;
	int rsize;
	int ifd;
	int ofd;

	if (mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s/etc/localtime", mic->config.filesrc.mic.dir))
		return;

	if ((ofd = open(filename, O_WRONLY | O_CREAT, 0644)) < 0) {
		display(PWARN, "%s: Open fail %s: %s\n", mic->name, filename, strerror(errno));
		return;
	}

	if (stat(TIMEZONE_SRC, &sbuf) != 0) {
		display(PWARN, "%s: Open fail %s: %s\n", mic->name, TIMEZONE_SRC, strerror(errno));
		close(ofd);
		return;
	}

	if ((ifd = open(TIMEZONE_SRC, O_RDONLY)) < 0) {
		display(PWARN, "%s: Open fail %s: %s\n", mic->name, TIMEZONE_SRC, strerror(errno));
		close(ofd);
		return;
	}

	if ((buffer = (char *) malloc(sbuf.st_size)) == NULL) {
		close(ifd);
		close(ofd);
		return;
	}

	rsize = read(ifd, buffer, sbuf.st_size);
	close(ifd);
	write(ofd, buffer, rsize);
	close(ofd);
	free(buffer);
	display(PFS, "%s: Created %s\n", mic->name, filename);
}

void
check_services(struct mic_info *mic)
{
	struct mservice *serv = &mic->config.services;
	char startname[PATH_MAX];
	char stopname[PATH_MAX];
	char linkname[PATH_MAX];

	check_timezone(mic);

	while (serv->next != NULL) {
		serv = serv->next;

		if (mpssut_filename(&mpssenv, NULL, startname, PATH_MAX, "%s/etc/rc5.d/S%02d%s",
				  mic->config.filesrc.mic.dir, serv->start, serv->name))
			unlink(startname);

		if (mpssut_filename(&mpssenv, NULL, stopname, PATH_MAX, "%s/etc/rc5.d/K%02d%s",
				  mic->config.filesrc.mic.dir, serv->stop, serv->name))
			unlink(stopname);

		snprintf(linkname, PATH_MAX, "../init.d/%s", serv->name);


		if (serv->on) {
			symlink(linkname, startname);
			chown(startname, 0, 0);
		} else {
			symlink(linkname, stopname);
			chown(stopname, 0, 0);
		}
	}
}

void
check_1_1(struct mic_info *mic)
{
	if (mic->config.user.method != NULL) {
		display(PNORM, "%s: UserAuthentication parameter deprecated and non functional - Removing\n", mic->name);
		mpss_remove_config(&mpssenv, mic, "# MIC User Authentication");
		mpss_remove_config(&mpssenv, mic, "# This parameter deprecates the UserIDs parameter.  If both are");
		mpss_remove_config(&mpssenv, mic, "# specified UserAuthentication will override UserIDs");
		mpss_remove_config(&mpssenv, mic, "UserAuthentication");
	}

	if (mic->config.filesrc.mic.type == SRCTYPE_FILELIST) {
		display(PWARN, "%s: MicDir parameter w/ %s filelist argument deprecated\n"
			       "%s     Consider a cleanconfig - initdefaults cycle to recreate\n",
			        mic->name, mic->config.filesrc.mic.list, "           ");
	}

	if (mic->config.filesrc.common.type == SRCTYPE_FILELIST) {
		display(PWARN, "%s: CommonDir parameter w/ %s filelist argument deprecated\n"
			       "%s     Consider a cleanconfig - initdefaults cycle to recreate\n",
			        mic->name, mic->config.filesrc.common.list, "           ");
	}
}

void
check_config(struct mic_info *mic)
{
	int err;

	if (mic->config.version == MPSS_CURRENT_CONFIG_VER)
		return;

	display(PINFO, "%s: Updateing Version parameter to %d.%d\n", mic->name,
			CURRENT_CONFIG_MAJOR, CURRENT_CONFIG_MINOR);
	if ((err = mpss_update_config(&mpssenv, mic->config.name, "Version", NULL, "Version %d %d\n",
				      CURRENT_CONFIG_MAJOR, CURRENT_CONFIG_MINOR)))
		display(PERROR, "Failed to open config file '%s': %d\n", mic->config.name, err);

	check_1_1(mic);
}

