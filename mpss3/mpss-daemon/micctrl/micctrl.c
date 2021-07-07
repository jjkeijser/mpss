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
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <getopt.h>
#include <execinfo.h>
#include "micctrl.h"

void set_segv_trap(void);

struct mpss_env mpssenv;
struct mbridge *brlist = NULL;
struct mpss_elist mpssperr;

static struct option main_longopts[] = {
	{"configdir", required_argument, NULL, 'c'},
	{"destdir", required_argument, NULL, 'd'},
	{"help", no_argument, NULL, 'h'},
	{"boot", no_argument, NULL, 'b'},
	{"reset", no_argument, NULL, 'r'},
	{"shutdown", no_argument, NULL, 'S'},
	{"reboot", no_argument, NULL, 'R'},
	{"status", no_argument, NULL, 's'},
	{"wait", no_argument, NULL, 'w'},
	{"initdefaults", no_argument, NULL, 0x1001},
	{"resetconfig", no_argument, NULL, 0x1002},
	{"resetdefaults", no_argument, NULL, 0x1003},
	{"cleanconfig", no_argument, NULL, 0x1004},
	{"useradd", optional_argument, NULL, 0x1006},
	{"adduser", optional_argument, NULL, 0x1006},
	{"userdel", optional_argument, NULL, 0x1007},
	{"deluser", optional_argument, NULL, 0x1007},
	{"passwd", optional_argument, NULL, 0x1008},
	{"groupadd", optional_argument, NULL, 0x1009},
	{"addgroup", optional_argument, NULL, 0x1009},
	{"groupdel", optional_argument, NULL, 0x100a},
	{"delgroup", optional_argument, NULL, 0x100a},
	{"rootdev", optional_argument, NULL, 0x100b},
	{"updatenfs", no_argument, NULL, 0x100c},
	{"addnfs", optional_argument, NULL, 0x100d},
	{"remnfs", optional_argument, NULL, 0x100e},
	{"updateusr", no_argument, NULL, 0x100f},
	{"updateramfs", no_argument, NULL, 0x1010},
	{"hostkeys", optional_argument, NULL, 0x1011},
	{"network", optional_argument, NULL, 0x1012},
	{"addbridge", optional_argument, NULL, 0x1013},
	{"delbridge", optional_argument, NULL, 0x1014},
	{"modbridge", optional_argument, NULL, 0x1015},
	{"service", optional_argument, NULL, 0x1016},
	{"overlay", optional_argument, NULL, 0x1017},
	{"base", optional_argument, NULL, 0x1018},
	{"commondir", optional_argument, NULL, 0x1019},
	{"micdir", optional_argument, NULL, 0x101a},
	{"osimage", optional_argument, NULL, 0x101b},
	{"autoboot", optional_argument, NULL, 0x101c},
	{"mac", optional_argument, NULL, 0x101d},
	{"pm", optional_argument, NULL, 0x101e},
	{"config", no_argument, NULL, 0x101f},
	{"cgroup", no_argument, NULL, 0x1020},
	{"ldap", optional_argument, NULL, 0x1021},
	{"userupdate", optional_argument, NULL, 0x1022},
	{"rpmdir", optional_argument, NULL, 0x1023},
	{"syslog", optional_argument, NULL, 0x1024},
	{"sshkeys", optional_argument, NULL, 0x1025},
	{"nis", optional_argument, NULL, 0x1026},
	{0}
};

char *main_help = {
"\
micctrl [global options] main command [sub options] [list of Xeon Phi cards]\n\
\n\
The 'micctrl' utility provides a multi-purpose toolbox of options to assist\n\
the system administrator in configuring and controlling the Xeon Phi cards in\n\
the system.  Each micctrl command consists of 4 parts: global options, the\n\
main command, sub options to the main command and a list of Xeon Phi cards.\n\
\n\
Each micctrl command must specify a main option to indicate the particular \n\
functionality for the call to micctrl.  Most main options will use a number of\n\
sub options.\n\
\n\
The list of Xeon Phi cards is optional and always consists of the Xeon Phi card\n\
identifiers with the format micID where ID is an instance number (for example:\n\
mic0, mic1, etc.).\n\
\n\
The global options are optional and each replaces a default value.  The Global\n\
options indicate a group of actions and if used must be used by each additional\n\
call to micctrl.  A global option setting will always precede the main command.\n\
\n\
Global Options\n\
    Configuration files location:\n\
\n\
    /etc/mpss - default value\n\
    /etc/sysconfig/mpss.conf        - file contains replacement value\n\
    export MPSS_CONFIGDIR=<confdir> - environment variable\n\
    -c <confdir>                    - command line option\n\
    --configdir=<confdir>           - command line option\n\
\n\
        By default the mpssd daemon and micctrl utility use and modify\n\
        configuration information in the /etc/mpss directory.  The system\n\
        administrator may specify an alternate location by either setting\n\
        the MPSS_CONFIGDIR parameter in the /etc/sysconfig/mpss.conf directory,\n\
        setting the the MPSS_CONFIGDIR environment parameter or using the '-c'\n\
        or '--configdir command line options.\n\
\n\
        The locations are used in the order command line parameter overrides the\n\
        environment variable, which overrides the configuration file, which\n\
        overrides the default value.\n\
\n\
        Using the environment variable or command line options for changing the\n\
        configuration files location allows the system administrator to create\n\
        different unique configurations.  To select the particular configuration\n\
        to be used by the MPSS service the system administrator needs only set\n\
        the location in the /etc/sysconfig/mpss.conf file and and restart the\n\
        service.\n\
\n\
        The 'micctrl -b' command with the --configdir global parameter can be\n\
        used to reboot a Xeon Phi card to use a new configuration.\n\
\n\
        If the configuration directory is indicated by using the command line\n\
        parameter then each call to micctrl should use the same option again to\n\
        affect the same configuration.  This differs from the setting the value\n\
        in either /etc/sysconfig/mpss.conf configuration file or the environment\n\
        variable.\n\
\n\
    Destination directory:\n\
\n\
    export MPSS_DESTDIR=<destdir> - environment variable\n\
    -d <destdir>                  - command line option\n\
    --destdir=<destdir>           - command line option\n\
\n\
        Setting the destination directory prepends all files produced by micctrl\n\
        with the specified 'destdir' directory instead of the default directory\n\
        '/'.  This allows the system administrator to create complete\n\
        configurations including all files generated by micctrl.\n\
\n\
        This option works much like the DESTDIR specification in Makefiles.  It\n\
        places complete configurations in a directory to be installed somewhere\n\
        else later.\n\
\n\
        If 'destdir' is indicated, micctrl does not attempt to change the\n\
        status of the network on the calling machine.  This differs from micctrl\n\
        network setup commands without it which call the system ifup, ifdown and\n\
        brctl utilities to instantiate the network changes.\n\
\n\
        For example, a central node on a cluster may create a configuration for\n\
        another node.  The 'destdir' directory created could be copied to the\n\
        target node or it may be copied to a file system intended to be pushed\n\
        to a particular node in the cluster.  Then on the target node the system\n\
        would restart the network service and the MPSS system.\n\
\n\
        Micctrl may also be called with 'destdir' set to the top level directory\n\
        of the change root environment and files are created ready for the file\n\
        system to be pushed to that host.\n\
\n\
        It is also possible to change root to the target directory and execute\n\
        micctrl with the '--destdir=/' command ready for the push later to the\n\
        target host.\n\
\n\
Sub options\n\
\n\
Most sub options are unique to any micctrl main command.  To see the sub options\n\
for any particular main option use micctrl with the main command and the '-h'\n\
option.  For example to get help for the micctrl command for creating Xeon Phi\n\
configuration files with default values use the command:\n\
\n\
        micctrl --initdefaults -h\n\
\n\
    Common Sub Options:\n\
\n\
        -h           Ignore all other sub options and print help for specified\n\
                     main option.\n\
\n\
        -v           Increment the level of verbosity of output.  By default\n\
                     micctrl will display information about errors and warnings.\n\
                     Specifying the -v option bumps the warning level up.\n\
                     Specifying -v once adds display of additional informational\n\
                     messages.  The -v option twice ('-v -v' or -vv) adds\n\
                     of changes to all files micctrl is creating, deleting or\n\
                     changing.  The -v option three times adds information on\n\
                     calls to the host's networking utilities.\n\
\n\
Main commands:\n\
\n\
Micctrl provides groups of functionality specified my any one main command.  The\n\
command currently supported are:\n\
\n\
    State modification commands:\n\
\n\
        The state modification commands differ from other groups of main\n\
        because they have short versions and they only apply to a host with\n\
        Xeon Phi cards installed.  Indicating to boot a Xeon Phi card on a\n\
        cluster node where it does not exist has not meaning\n\
\n\
        -b\n\
        --boot       Boot the list of Xeon Phi cards if the are currently in the\n\
                     'ready' state.\n\
\n\
        -r\n\
        --reset      Reset the list of Xeon Phi cards.\n\
\n\
        -S\n\
        --shutdown   Send the shutdown command to the list of Xeon Phi Cards in\n\
                     the 'online' state.\n\
\n\
        -R\n\
        --reboot     Send the reboot command to the list of Xeon Phi Cards in\n\
                     'online' state.\n\
\n\
        -s\n\
        --status     Display the status (state) of the Xeon Phi cards.\n\
\n\
        -w\n\
        --wait       Wait for the list of Xeon Phi cards to not be either the\n\
                     'resetting' or 'booting' states.\n\
\n\
    Configuration file initialization commands:\n\
\n\
        --initdefaults  Initialize the defaults in the Xeon Phi configuration\n\
                     files.\n\
\n\
        --resetdefaults  Reset the configuration parameters back to default\n\
                     in the Xeon Phi configuration files.\n\
\n\
        --resetconfig  Instantiate changes to the Xeon Phi configuration files\n\
\n\
        --cleanconfig  Completely remove all configuration information for the\n\
                     list of Xeon Phi cards.\n\
\n\
        --config     Display in human readable format the current configuration\n\
                     information for the Xeon Phi cards.\n\
\n\
    User list control commands:\n\
\n\
        --userupdate Reset the user list for the Xeon Phi file system.\n\
\n\
        --useradd\n\
        --adduser    Add the specified user to the Xeon Phi cards file system.\n\
\n\
        --userdel\n\
        --deluser    Remove the specified user for the Xeon Phi file system.\n\
\n\
        --groupadd\n\
        --addgroup   Add the specified group to the Xeon Phi file system.\n\
\n\
        --groupdel\n\
        --delgroup   Delete the specified group from the Xeon Phi file system\n\
\n\
        --passwd     Change the password of the specified user for the Xeon Phi\n\
                     file system.\n\
\n\
         --ldap      Setup or disable LDAP support for the Xeon Phi cards.\n\
\n\
         --nis       Setup or disable NIS support for the Xeon Phi cards.\n\
\n\
    Network setup commands:\n\
\n\
        --addbridge  Create a network bride to connect Xeon Phi card\n\
                     networking interfaces.\n\
\n\
        --modbridge  Modify the parameters for a network bridge instance.\n\
\n\
        --delbridge  Remove a network bridge from the Xeon Phi configuration.\n\
\n\
        --network    Specify network information for the Xeon Phi cards.\n\
\n\
        --hostkeys   Restore the host SSH keys previously saved back to the\n\
                     Xeon Phi cards /etc/ssh directory.\n\
\n\
        --sshkeys    Update the ssh keys for the specified user.\n\
\n\
        --mac        Set the Xeon Phi cards MAC address.\n\
\n\
    File system location commands:\n\
\n\
        --base       Set the BaseDir parameter specifying the location of the\n\
                     files defining the general file system for the Xeon Phi\n\
                     cards supplied by the mpss-boot-files RPM.\n\
\n\
        --micdir     Set the MicDir parameter specifying the location of the\n\
                     Phi card files generated by micctrl to define the unique\n\
                     information for that card.\n\
\n\
        --commondir  Set the CommonDir parameter specifying the location of the\n\
                     files to be shared by the Xeon Phi cards.\n\
\n\
        --overlay    Indicate the location of a set of files to be included in\n\
                     the Xeon Phi file system.\n\
\n\
        --rpmdir     Set the directory to expect the --ldap and --nis options\n\
                     to find its required RPMs in.\n\
\n\
    Root device commands:\n\
\n\
        --rootdev    Specify the root device type and location for the Xeon\n\
                     Phi cards.\n\
\n\
        --updateramfs  If the root device type indicates to use a ram disk\n\
                     as its root device, rebuild the image file specified by the\n\
                     RootDev parameter from configuration parameters.\n\
\n\
        --updatenfs  If the root device type indicates to use a NFS remote mount\n\
                     as its root device, update the specified exported directory\n\
                     indicated by the RootDev configuration parameter.\n\
\n\
        --updateusr  If the root device indicates to use a split NFS remote\n\
                     mount, update the shared /usr NFS export.\n\
\n\
    NFS mounts:\n\
\n\
        --addnfs     Add the specified NFS export to the Xeon Phi cards\n\
                     /etc/fstab files.\n\
\n\
        --remnfs     Remove the specified NFS mount directory from the Xeon Phi\n\
                     cards /etc/fstab files.\n\
\n\
    Power management commands:\n\
\n\
        --pm         Set the PowerManagement configuration parameter for the\n\
                     list of Xeon Phi cards.\n\
\n\
    Miscellaneous other commands:\n\
\n\
        --osimage    Set the OSimage parameter to the locations to find the\n\
                     Linux kernel to boot the card and its matching system map\n\
                     file instead of the default versions in the mpss-boot-file\n\
                     RPM.\n\
\n\
        --autoboot   Set the BootOnStart parameter to indicate whether the list\n\
                     if Xeon Phi cards should be automatically booted or not\n\
                     when the MPSS service is started.\n\
\n\
        --syslog     Set the syslog destination for the Xeon Phi cards.\n\
\n\
        --service    Modify the service information for Xeon Phi cards.  NOTE:\n\
                     This has been deprecated and is not recommended to be use.\n\
\n\
        --cgroup     Sets the configuration for the cgroup memory information.\n\
"};


/*
 *
 * Main parses the first argument of the command line.  Micctrl always takes at least
 * one argument to define the particular functionality to perform.  The rest of the
 * command line is then passed to a sub parser function to determine if the rest of
 * the command is correct.
 */
int
main(int argc, char *argv[], char *envp[])
{
	int longidx;
	int err;
	int c;

	umask(022);
	set_segv_trap();

	if ((err = mpssenv_init(&mpssenv))) {
		display(PERROR, "MPSS Environment init: %s %s\n",
				mpssenv_errtype(err), mpssenv_errstr(err));
		exit(1);
	}

	while ((c = getopt_long(argc, argv, "hbrSRswc:d:", main_longopts, &longidx)) != -1) {
		switch(c) {
		case 'h':
			goto usage;
			break;

		case 'c':
			if ((err = mpssenv_set_configdir(&mpssenv, optarg))) {
				display(PERROR, "Unable to set confdir %s: %s\n", optarg,
						mpssenv_errstr(err));
				exit(1);
			}
			break;

		case 'd':
			if ((err = mpssenv_set_destdir(&mpssenv, optarg))) {
				if (mkdir(mpssenv.destdir, 0777) < 0) {
					display(PERROR, "Failed to create directory %s: %s\n",
							optarg, strerror(errno));
					exit(1);
				}

				if ((err = mpssenv_set_destdir(&mpssenv, optarg))) {
					display(PERROR, "Unable to set destdir %s: %s\n",
							optarg, mpssenv_errstr(err));
					exit(1);
				}
			}
			break;

		case 'b':
			parse_state_args(STATE_BOOT, argc, argv);
			break;

		case 'r':
			parse_state_args(STATE_RESET, argc, argv);
			break;

		case 'S':
			parse_state_args(STATE_SHUTDOWN, argc, argv);
			break;

		case 'R':
			parse_state_args(STATE_REBOOT, argc, argv);
			break;

		case 's':
			parse_status_args(argc, argv);
			break;

		case 'w':
			parse_wait_args(argc, argv);
			break;


		case 0x1001:
			parse_config_args(CONFIG_INITDEFS, argc, argv);
			break;

		case 0x1002:
			parse_config_args(CONFIG_RESETCONF, argc, argv);
			break;

		case 0x1003:
			parse_config_args(CONFIG_RESETDEFS, argc, argv);
			break;

		case 0x1004:
			parse_config_args(CONFIG_CLEANCONF, argc, argv);
			break;

		case 0x1006:
			parse_useradd_args(optarg, argc, argv);
			break;

		case 0x1007:
			parse_userdel_args(optarg, argc, argv);
			break;

		case 0x1008:
			parse_passwd_args(optarg, argc, argv, envp);
			break;

		case 0x1009:
			parse_groupadd_args(optarg, argc, argv);
			break;

		case 0x100a:
			parse_groupdel_args(optarg, argc, argv);
			break;

		case 0x100b:
			parse_rootdev_args(optarg, argc, argv);
			break;

		case 0x100c:
			parse_update_args(UPDATE_NFS, argc, argv);
			break;

		case 0x100d:
			parse_addnfs_args(optarg, argc, argv);
			break;

		case 0x100e:
			parse_remnfs_args(optarg, argc, argv);
			break;

		case 0x100f:
			parse_update_args(UPDATE_USR, argc, argv);
			break;

		case 0x1010:
			parse_update_args(UPDATE_RAM, argc, argv);
			break;

		case 0x1011:
			parse_hostkeys_args(optarg, argc, argv);
			break;

		case 0x1012:
			parse_network_args(optarg, argc, argv);
			break;

		case 0x1013:
			parse_addbridge_args(optarg, argc, argv);
			break;

		case 0x1014:
			parse_delbridge_args(optarg, argc, argv);
			break;

		case 0x1015:
			parse_modbridge_args(optarg, argc, argv);
			break;

		case 0x1016:
			parse_service_args(optarg, argc, argv);
			break;

		case 0x1017:
			parse_overlay_args(optarg, argc, argv);
			break;

		case 0x1018:
			parse_base_args(optarg, argc, argv);
			break;

		case 0x1019:
			parse_commondir_args(optarg, argc, argv);
			break;

		case 0x101a:
			parse_micdir_args(optarg, argc, argv);
			break;

		case 0x101b:
			parse_osimage_args(optarg, argc, argv);
			break;

		case 0x101c:
			parse_autoboot_args(optarg, argc, argv);
			break;

		case 0x101d:
			parse_mac_args(optarg, argc, argv);
			break;

		case 0x101e:
			parse_power_args(optarg, argc, argv);
			break;

		case 0x101f:
			parse_showconfig_args(argc, argv);
			break;

		case 0x1020:
			parse_cgroup_args(argc, argv);
			break;

		case 0x1021:
			parse_ldap_args(optarg, argc, argv);
			break;

		case 0x1022:
			parse_userupdate_args(optarg, argc, argv);
			break;

		case 0x1023:
			parse_rpmdir_args(optarg, argc, argv);
			break;

		case 0x1024:
			parse_syslog_args(optarg, argc, argv);
			break;

		case 0x1025:
			parse_sshkeys_args(optarg, argc, argv);
			break;

		case 0x1026:
			parse_nis_args(optarg, argc, argv);
			break;

		default:
			exit(1);
		}
	}

usage:
	micctrl_help(main_help);
	return 0;
}

void
segv_handler(int sig, siginfo_t *siginfo, void *context)
{
	void *addrs[100];
	char **funcs;
	int cnt;
	int i;

	cnt = backtrace(addrs, 100);
	funcs = backtrace_symbols(addrs, cnt);

	for (i = 0; i < cnt; i++) {
		printf("%s\n", funcs[i]);
	}
	exit(0);
}

void
set_segv_trap(void)
{
	struct sigaction act;

	memset(&act, 0, sizeof(act));
	act.sa_sigaction = segv_handler;
	act.sa_flags = SA_SIGINFO;
	sigaction(SIGSEGV, &act, NULL);
}

