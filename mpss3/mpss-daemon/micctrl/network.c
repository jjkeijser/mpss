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


void hostkeys(struct mic_info *miclist, char *keydir);
void show_net_config(struct mic_info *miclist);
void config_net(char *type, char *bridge, char *ip, char *gw, char *netbits, char *mtu,
		char *modhost, char *modcard, struct mic_info *miclist, int miccnt);
void show_bridge_list(struct mic_info *mic);
void add_bridge(char *name, char *type, char *ip, char *netbits, char *mtu, struct mic_info *miclist);
void mod_bridge(char *name, char *ip, char *netbits, char *mtu, struct mic_info *miclist);
void del_bridge(char *name, struct mic_info *miclist);
int add_hostbridge(char *name, char *ip, char *netmask, char *mtu, int type);
void update_hostbridge(char *name, char *ip, char *netbits, char *mtu);
void suse_restart_bridge(char *name, int saveresolv);
void setmac(char *micmac, char *hostmac, struct mic_info *miclist);
void gen_ipinfo(struct mic_info *mic, int update);
void rem_from_hosts(char *micid, char *name);
int check_for_brctl(void);

void set_local_netdir(char *netdir);

void do_ifdown(char *label, char *name, int saveresolv);

void common_net_remove(char *name, char *bridge, int modhost);
void common_br_remove(struct mbridge *br);
void common_attach_hostbridge(char *name, char *bridge, char *mac, char *mtu, int saveresolv);
void common_host_addif(char *name, char *ip, char *netbits, char *mtu, char *mac);
void common_remove_bridges(void);

void ubuntu_net_remove(char *name, char *bridge, int modhost);
void ubuntu_br_remove(struct mbridge *br);
void ubuntu_attach_hostbridge(char *name, char *bridge, char *mac, char *mtu, int saveresolv);
void ubuntu_host_addif(char *name, char *ip, char *netbits, char *mtu, char *mac);
void ubuntu_unattach_hostbridge(char *name, char *bridge);
void ubuntu_remove_bridges(void);

struct netinfo {
	char *netdir;
	char *defnetdir;
	char *brctl;
	void (*net_remove)(char *name, char *bridge, int modhost);
	void (*br_remove)(struct mbridge *br);
	void (*attach_hostbridge)(char *name, char *bridge, char *mac, char *mtu, int saveresolv);
	void (*host_addif)(char *name, char *ip, char *netbits, char *mtu, char *mac);
	void (*remove_bridges)(void);
} ni[] = {{REDHAT_NETWORK_DIR,
		REDHAT_NETWORK_DIR,
		"/usr/sbin/brctl",
	  	common_net_remove,
		common_br_remove,
	  	common_attach_hostbridge,
		common_host_addif,
		common_remove_bridges},
	  {REDHAT_NETWORK_DIR,
		REDHAT_NETWORK_DIR,
	  	"/usr/sbin/brctl",
		common_net_remove,
		common_br_remove,
		common_attach_hostbridge,
		common_host_addif,
		common_remove_bridges},
	  {SUSE_NETWORK_DIR,
		SUSE_NETWORK_DIR,
		"/sbin/brctl",
		common_net_remove,
		common_br_remove,
		common_attach_hostbridge,
		common_host_addif,
		common_remove_bridges},
	  {UBUNTU_NETWORK_DIR,
		UBUNTU_NETWORK_DIR,
		"/sbin/brctl",
		ubuntu_net_remove,
		ubuntu_br_remove,
		ubuntu_attach_hostbridge,
		ubuntu_host_addif,
		ubuntu_remove_bridges}
	};

#define MIC_DEFAULT_BIG_MTU	"64512";
#define MIC_DEFAULT_SMALL_MTU	"1500";

#define xstr(s) #s
#define str(s) xstr(s)

/*
 * Section 1: Top level parse routines for micctrl network handling
 */

/*
 * micctrl --hostkeys=<key directory> [MIC list]
 *
 * Use the host key files in the specified directory for the files for on the /etc/ssh
 * directory for the list of MIC cards.  These replace the randomly generated files
 * created by --initdefaults.
 */
char *hostkeys_help =
"\
micctrl [global options] --hostkeys=<dir> [sub options] <Xeon Phi card>\n\
\n\
The --hostkeys copies the ssh host keys from the specified directory to the Xeon\n\
Phi file systems /etc/ssh directory.  This directory contains a set of ssh keys\n\
usually unique to a Linux system used to identify that system for ssh\n\
connections,  There is not coresponding micctrl option to save the keys.\n\
\n"
COMMON_GLOBAL
COMMON_HELP;

static struct option hostkeys_longopts[] = {
	{"help", no_argument, NULL, 'h'},
	{0}
};

void
parse_hostkeys_args(char *keydir, int argc, char *argv[])
{
	struct mic_info *miclist;
	int longidx;
	int c;

	opterr = 0;
	while ((c = getopt_long(argc, argv, "hv", hostkeys_longopts, &longidx)) != -1) {
		switch(c) {
		case 'h':
			micctrl_help(hostkeys_help);
			exit(0);

		case 'v':
			bump_display_level();
			break;

		default:
			ARGERROR_EXIT(hostkeys_help);
		}
	}

	check_rootid();
	miclist = create_miclist(&mpssenv, argc, argv, optind, NULL);
	hostkeys(miclist, keydir);
}

/*
 * micctrl --network=<type> [MIC list]
 *
 * Use the host key files in the specified directory for the files for on the /etc/ssh
 * directory for the list of MIC cards.  These replace the randomly generated files
 * created by --initdefaults.
 */
char *network_help =
"\
micctrl [global options] --network=<type> [sub options] [Xeon Phi cards]\n\
\n\
The --network option is used to defined the network configuration for the Xeon\n\
Phi cards.  The micctrl command divides the possible network toploogies it will\n\
configure into three categories:\n\
\n\
    Static Pair:     The virtual Ethernet interfaces are each on a unique subnet\n\
                     with the card side virtual Ethernet.  This is the default\n\
                     configuration.\n\
\n\
    Static Bridged:  The virtual Ethernet interfaces are connected to a Linux\n\
                     network bridge (see the --addbridge option) with a static\n\
                     IP address.\n\
\n\
    DHCP Bridged:    The virtual Ethernet interfaces are connected to a Linux\n\
                     network bridge with an IP address assigned through the DHCP\n\
                     protocol.\n\
\n\
The --network option can be set to three values:\n\
\n\
    static:          The Xeon Phi interfaces will be configured with static IP\n\
                     addresses.  The --ip sub option must be specified.\n\
\n\
    dhcp:            The network interfaces will use DHCP to retrieve their IP\n\
                     addresses.\n\
\n\
    default:         Set the network interfaces back to their default values of\n\
                     static pair with the default IP addresses starting with\n\
                     172.31.\n\
\n\
If the --network option is called with no argument the current network\n\
configuration will be displayed and the sub options will be ignored.\n\
\n\
For more information see the Documentation.\n\
\n"
COMMON_GLOBAL
COMMON_HELP
NETDIR_HELP_ENTRY
    "    -d <netdir>\n"
NETDIR_HELP_BODY
NET_DISTHELP
"\
    -b <bridge>\n\
    --bridge=<bridge>\n\
\n\
        Attach the network interfaces to the specified bridge.  It must already\n\
        be known by the configuration by adding it with the --addbridge option.\n\
\n\
        If this options is used then the interfaces will inherit their netmask\n\
        and MTU values from the bridge and the --netbits and --ip options will\n\
        be ignored.\n\
\n\
    -i <IP>\n\
    --ip=<IP>\n\
\n\
        Specify a new IP address for the network bridge. In most cases the IP\n\
        address specified will be used on the first Xeon Phi card in the list\n\
        each card after that will increment it by one.  There are other\n\
        configurations possible but detailed explanation would be too long for\n\
        this help content.  Consult the documentation for further information.\n\
\n\
    -n <netbits>\n\
    --netbits=<netbits>\n\
\n\
        If this is a static pair configuration specify a set of netbits for the\n\
        network interface.  The netbits value must be between 8 and 24 and\n\
        specifies the number of upper bits set in the netmask for the interface.\n\
        For example the default value is 24 and corresponds to a netmask of\n\
        FFFFFF00 or 255.255.255.0.\n\
\n\
    -m <MTU>\n\
    --mtu=<MTU>\n\
\n\
        If this is a static pair configuration specify a new MTU for the network\n\
        interfaces.\n\
\n\
    -c <yes | no>>\n\
    --modhost=<yes | no>>\n\
\n\
        If --modhost is set to 'yes' the Xeon Phi cards 'hostname: IP address'\n\
        pair is created in the host's /etc/hosts file with the comment\n\
        '#Generated-by-micctrl'.\n\
\n\
        If --modhost is set to 'no' the entry matching the Xeon Phi IP address\n\
        with the comment '#Generated-by-micctrl' will be removed from the\n\
        host's /etc/hosts file.\n\
\n\
       If not specified the current value does not change and no action will be\n\
       applied.\n\
\n\
    -e <yes | no | <file location>>\n\
    --modcard<yes | no | <file location>>\n\
\n\
        If --modcard is set to 'yes' a /etc/hosts file is created in the MicDir\n\
        directory.\n\
\n\
        If --modcard is set to 'no' the /etc/hosts file the MicDir directory\n\
        will be removed.\n\
\n\
        If --modcard is set to <file location> the given file will be copied\n\
        to MicDir directory.\n\
\n\
       If not specified the current value does not change and no action will be\n\
       applied.\n\
\n\
    -r <yes | no>>\n\
    --hosts=<yes | no>>\n\
\n\
        The --hosts option is deprecated and is the combination of --modhost and\n\
        --modcard together.\n\
\n\
    -g <gateway>\n\
    --gw=<gateway>\n\
\n\
        Set the gateway for the network interfaces.  If it is not specified the\n\
        gateway for the host is read and used.  This would make no sense when\n\
        configuring a system remotely and therefor will often be used with the\n\
        --destdir option.\n\
\n";

static struct option network_longopts[] = {
	{"help", no_argument, NULL, 'h'},
	{"bridge", required_argument, NULL, 'b'},
	{"ip", required_argument, NULL, 'i'},
	{"netbits", required_argument, NULL, 'n'},
	{"mtu", required_argument, NULL, 'm'},
	{"netdir", required_argument, NULL, 'd'},
	{"hosts", required_argument, NULL, 'r'},
	{"modhost", required_argument, NULL, 'c'},
	{"modcard", required_argument, NULL, 'e'},
	{"distrib", required_argument, NULL, 'w'},
	{"gw", required_argument, NULL, 'g'},
	{0}
};

void
parse_network_args(char *type, int argc, char *argv[])
{
	struct mic_info *miclist;
	int miccnt;
	char *bridge = NULL;
	char *ip = NULL;
	char *netbits = NULL;
	char *mtu = NULL;
	char *gw = NULL;
	char *modhost = NULL;
	char *modcard = NULL;
	int changes = FALSE;
	int longidx;
	int c;

	while ((c = getopt_long(argc, argv, "hvb:i:d:c:n:m:w:e:g:r:", network_longopts, &longidx)) != -1) {
		switch(c) {
		case 'h':
			micctrl_help(network_help);
			exit(0);

		case 'b':
			bridge = optarg;
			break;

		case 'i':
			ip = optarg;
			changes = TRUE;
			break;

		case 'c':
			modhost = optarg;
			changes = TRUE;
			break;

		case 'e':
			modcard = optarg;
			changes = TRUE;
			break;

		case 'r':
			display(PWARN, "The --hosts argument is deprecated and replace by the --modhost "
				       "and --modcard arguments.\n");
			display(PWARN, "It will be removed in a future release\n");
			modhost = optarg;
			modcard = optarg;
			changes = TRUE;
			break;

		case 'n':
			netbits = optarg;
			changes = TRUE;
			break;

		case 'm':
			mtu = optarg;
			changes = TRUE;
			break;

		case 'd':
			set_local_netdir(optarg);
			break;

		case 'w':
			set_alt_distrib(optarg);
			break;

		case 'g':
			gw = optarg;
			break;

		case 'v':
			bump_display_level();
			break;

		default:
			ARGERROR_EXIT(network_help);
		}
	}

	miclist = create_miclist(&mpssenv, argc, argv, optind, &miccnt);

	if ((type == NULL) && (changes == FALSE))
		show_net_config(miclist);

	check_rootid();
	if (mpssenv_aquire_lockfile(&mpssenv)) {
		display(PERROR, "Cannot change network configuration - "
				"mpssd daemon is running\n");
		EXIT_ERROR(EBUSY);
	}

	config_net(type, bridge, ip, gw, netbits, mtu, modhost, modcard, miclist, miccnt);
}

char *addbr_help =
"\
micctrl [global options] --addbridge=<name> [sub options] [Xeon Phi cards]\n\
\n\
The --addbridge option allows the system administrator to add the specified\n\
network bridge to the configuration for the Xeon Phi cards.  If the bridge's\n\
configuration file in the host's '<netdir>' directory does not exist micctrl\n\
will create it with the specified parameters.\n\
\n\
WARNING:  If the bridge configuration file already exists then you must use\n\
the correct IP, netbits and MTU options to make the Xeon Phi configuration\n\
match.  Otherwise the network interfaces added to it will have incorrect netmask\n\
and MTU values.\n\
\n"
COMMON_GLOBAL
COMMON_HELP
NETDIR_HELP_ENTRY
    "    -d <netdir>\n"
NETDIR_HELP_BODY
NET_DISTHELP
"\
    -t <type>\n\
    --type=<type>\n\
\n\
	The 'type' parameter must be set to either 'internal' or 'external'.\n\
\n\
        If 'internal' is specified micctrl will create the network control files\n\
         on the system to span a bridge between the Xeon Phi cards. The 'micctrl\n\
        --network' command must then be used to add Xeon Phi cards to the\n\
        bridge.\n\
\n\
	If 'external' is specified micctrl will create the network control file\n\
	if they do not currently exist.  An external bridge connects the Xeon\n\
        Phi cards to a physical Ethernet device. It records the setting in the\n\
        configuration file for its own use with the 'micctrl --network' command.\n\
        It is necessary to other sub options to ensure the values recorded match\n\
        the setting in the current network configuration files.\n\
\n\
    -i <IP>\n\
    --ip=<IP>\n\
\n\
        Specify a new IP address for the network bridge.\n\
\n\
    -n <netbits>\n\
    --netbits=<netbits>\n\
\n\
        Specify a set of netbits for the network bridge.  The netbit value must\n\
        be between 8 and 24 and specifies the number of upper bits set in the\n\
        netmask for the interface.  For example the default value is 24 and\n\
        corresponds to a netmask of FFFFFF00 or 255.255.255.0.\n\
\n\
    -m <MTU>\n\
    --mtu=<MTU>\n\
\n\
        Specify a new MTU for the network bridge.\n\
\n";

static struct option addbridge_longopts[] = {
	{"help", no_argument, NULL, 'h'},
	{"type", required_argument, NULL, 't'},
	{"ip", required_argument, NULL, 'i'},
	{"netbits", required_argument, NULL, 'n'},
	{"mtu", required_argument, NULL, 'm'},
	{"netdir", required_argument, NULL, 'd'},
	{"distrib", required_argument, NULL, 'w'},
	{0}
};

void
parse_addbridge_args(char *name, int argc, char *argv[])
{
	int longidx;
	int c;
	int cnt;
	struct mic_info *miclist;
	char *type = NULL;
	char *ip = NULL;
	char *netbits = NULL;
	char *mtu = NULL;

	while ((c = getopt_long(argc, argv, "hvt:i:n:m:d:w:", addbridge_longopts, &longidx)) != -1) {
		switch(c) {
		case 'h':
			micctrl_help(addbr_help);
			exit(0);

		case 't':
			type = optarg;
			break;

		case 'i':
			ip = optarg;
			break;

		case 'n':
			netbits = optarg;
			break;

		case 'm':
			mtu = optarg;
			break;

		case 'd':
			set_local_netdir(optarg);
			break;

		case 'w':
			set_alt_distrib(optarg);
			break;

		case 'v':
			bump_display_level();
			break;

		default:
			ARGERROR_EXIT(addbr_help);
		}
	}

	miclist = create_miclist(&mpssenv, argc, argv, optind, &cnt);

	if (name == NULL) {
		show_bridge_list(miclist);
	} else {
		check_rootid();
		if (mpssenv_aquire_lockfile(&mpssenv)) {
			display(PERROR, "Cannot change network configuration - "
					"mpssd daemon is running\n");
			EXIT_ERROR(EBUSY);
		}

		add_bridge(name, type, ip, netbits, mtu, miclist);
	}
}

char *delbr_help =
"\
micctrl [global options] --delbridge=<name> [sub options] [Xeon Phi cards]\n\
\n\
The --delbridge option allows the system administrator to remove the specified\n\
network bridge from the configuration for the Xeon Phi cards.  If the bridge\n\
type is marked as 'internal' it will also remove the host side configuration\n\
file found in 'netdir'.\n\
\n"
COMMON_GLOBAL
COMMON_HELP
NETDIR_HELP_ENTRY
    "    -d <netdir>\n"
NETDIR_HELP_BODY
NET_DISTHELP;

static struct option delbridge_longopts[] = {
	{"help", no_argument, NULL, 'h'},
	{"netdir", required_argument, NULL, 'd'},
	{"distrib", required_argument, NULL, 'w'},
	{0}
};

void
parse_delbridge_args(char *name, int argc, char *argv[])
{
	int longidx;
	int c;
	int cnt;
	struct mic_info *miclist;

	while ((c = getopt_long(argc, argv, "hvd:w:", delbridge_longopts, &longidx)) != -1) {
		switch(c) {
		case 'h':
			micctrl_help(delbr_help);
			exit(0);

		case 'd':
			set_local_netdir(optarg);
			break;

		case 'w':
			set_alt_distrib(optarg);
			break;

		case 'v':
			bump_display_level();
			break;

		default:
			ARGERROR_EXIT(delbr_help);
		}
	}

	miclist = create_miclist(&mpssenv, argc, argv, optind, &cnt);

	if (name == NULL) {
		show_bridge_list(miclist);
	} else {
		check_rootid();
		if (mpssenv_aquire_lockfile(&mpssenv)) {
			display(PERROR, "Cannot change network configuration - "
					"mpssd daemon is running\n");
			EXIT_ERROR(EBUSY);
		}

		del_bridge(name, miclist);
	}
}

char *modbr_help =
"\
micctrl [global options] --modbridge=<name> [sub options] [Xeon Phi cards]\n\
\n\
The --modbridge option allows the system administrator to modify the ip, netmask\n\
and MTU for the specified network bridge.\n\
\n"
COMMON_GLOBAL
COMMON_HELP
NETDIR_HELP_ENTRY
    "    -d <netdir>\n"
NETDIR_HELP_BODY
NET_DISTHELP
"\
    -i <IP>\n\
    --ip=<IP>\n\
\n\
        Specify a new IP address for the network bridge.\n\
\n\
    -n <netbits>\n\
    --netbits=<netbits>\n\
\n\
        Specify a set of netbits for the network bridge.  The netbit value must\n\
        be between 8 and 24 and specifies the number of upper bits set in the\n\
        netmask for the interface.  For example the default value is 24 and\n\
        corresponds to a netmask of FFFFFF00 or 255.255.255.0.\n\
\n\
    -m <MTU>\n\
    --mtu=<MTU>\n\
\n\
        Specify a new MTU for the network bridge.\n\
\n";

static struct option modbridge_longopts[] = {
	{"help", no_argument, NULL, 'h'},
	{"ip", required_argument, NULL, 'i'},
	{"netbits", required_argument, NULL, 'n'},
	{"mtu", required_argument, NULL, 'm'},
	{"netdir", required_argument, NULL, 'd'},
	{"distrib", required_argument, NULL, 'w'},
	{0}
};

void
parse_modbridge_args(char *name, int argc, char *argv[])
{
	int longidx;
	int c;
	int cnt;
	struct mic_info *miclist;
	char *ip = NULL;
	char *netbits = NULL;
	char *mtu = NULL;

	while ((c = getopt_long(argc, argv, "hvi:n:m:d:w:", modbridge_longopts, &longidx)) != -1) {
		switch(c) {
		case 'h':
			micctrl_help(modbr_help);
			exit(0);

		case 'i':
			ip = optarg;
			break;

		case 'n':
			netbits = optarg;
			break;

		case 'm':
			mtu = optarg;
			break;

		case 'd':
			set_local_netdir(optarg);
			break;

		case 'w':
			set_alt_distrib(optarg);
			break;

		case 'v':
			bump_display_level();
			break;

		default:
			ARGERROR_EXIT(modbr_help);
		}
	}

	miclist = create_miclist(&mpssenv, argc, argv, optind, &cnt);

	check_rootid();
	if (mpssenv_aquire_lockfile(&mpssenv)) {
		display(PERROR, "Cannot change network configuration - "
				"mpssd daemon is running\n");
		EXIT_ERROR(EBUSY);
	}

	mod_bridge(name, ip, netbits, mtu, miclist);
}

char *mac_help =
"\
micctrl [global options] --mac=<macoption>> [sub options] [Xeon Phi cards]\n\
\n\
The --mac option defines the method of setting the MAC address on both the host\n\
and Xeon Phi card side ends of the virtual Ethernet interface.  There are three\n\
possible definitions of 'macoption'.\n\
\n\
    Serial     If the Xeon Phi cards have a valid serial number, the MAC\n\
               address of both the host and card side ends of the virtual\n\
               Ethernet is assinged by the drivers based on the serial number.\n\
\n\
               This is the value normally assigned by the --initdefault option.\n\
\n\
    Random     The virtual Ethernet drivers at both ends of the connection ask\n\
               the Linux kernel for a random MAC address assignment.\n\
\n\
    XX:XX:XX:XX:XX:XX  Provide a MAC address in the standard format,\n\
               XX:XX:XX:XX:XX:XX, where XX is an ASCII encoded hex digit, it\n\
               will be used as the MAC address.  The specified MAC address will\n\
               be used incrementally across all specified virtual network\n\
               interfaces.  For example: if the last 8-bit value is '08' then\n\
               mic0 host ends in '08' mic0 card ends in '09', mic1 host ends in\n\
               '0A', mic1 card ends in '0B' ....\n\
\n\
               All other values are considered to be errors and the current\n\
               value will not change.\n\
\n"
COMMON_GLOBAL
COMMON_HELP
NETDIR_HELP_ENTRY
    "    -d <netdir>\n"
NETDIR_HELP_BODY
NET_DISTHELP;

static struct option mac_longopts[] = {
	{"help", no_argument, NULL, 'h'},
	{"netdir", required_argument, NULL, 'd'},
	{"distrib", required_argument, NULL, 'w'},
	{0}
};

void
parse_mac_args(char *micmac, int argc, char *argv[])
{
	int longidx;
	int c;
	int cnt;
	struct mic_info *miclist;
	char *hostmac = NULL;

	while ((c = getopt_long(argc, argv, "hvm:d:w:", mac_longopts, &longidx)) != -1) {
		switch(c) {
		case 'h':
			micctrl_help(mac_help);
			exit(0);

		case 'd':
			set_local_netdir(optarg);
			break;

		case 'w':
			set_alt_distrib(optarg);
			break;

		case 'v':
			bump_display_level();
			break;

		default:
			ARGERROR_EXIT(mac_help);
		}
	}

	miclist = create_miclist(&mpssenv, argc, argv, optind, &cnt);

	check_rootid();
	if (mpssenv_aquire_lockfile(&mpssenv)) {
		display(PERROR, "Cannot change network configuration - "
				"mpssd daemon is running\n");
		EXIT_ERROR(EBUSY);
	}

	setmac(micmac, hostmac, miclist);
}

/*
 * Section 2: Worker functions for network administration functions
 */

void
set_local_netdir(char *netdir)
{
	char filename[PATH_MAX];
	int err;

	ni[mpssenv.dist].netdir = mpssut_tilde(&mpssenv, netdir);
	mpssenv.live_update = FALSE;

	if (!mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s", ni[mpssenv.dist].netdir)) {
		if ((err = mpssut_mksubtree(&mpssenv, "", ni[mpssenv.dist].netdir, 0, 0, 0755)))
			display(PERROR, "Cannot create %s: %d\n", ni[mpssenv.dist].netdir, err);
	}
}

void
set_alt_distrib(char *newdist)
{
	char *netdir = NULL;

	if (strcmp(ni[mpssenv.dist].netdir, ni[mpssenv.dist].defnetdir))
		netdir = ni[mpssenv.dist].netdir;

	mpssenv_set_distrib(&mpssenv, newdist);

	if (netdir) set_local_netdir(netdir);
}

char *hostkeynames[] = {
	"ssh_host_dsa_key",
	"ssh_host_dsa_key.pub",
	"ssh_host_ecdsa_key",
	"ssh_host_ecdsa_key.pub",
	"ssh_host_key",
	"ssh_host_key.pub",
	"ssh_host_rsa_key",
	"ssh_host_rsa_key.pub"
};
#define LEN_KEYNAMES (sizeof(hostkeynames) / sizeof(hostkeynames[0]))

int copy_key_file(struct mic_info *mic, char *keydir, char *name);

/*
 * Copy the host key files found in keydir to the /etc/ssh directory on the list of MIC
 * cards.  These replace the files randomly generated by ssh-keygen in the --initdefaults
 * option.
 */
void
hostkeys(struct mic_info *miclist, char *keydir)
{
	struct mic_info *mic;
	struct dirent *file;
	DIR *dp;
	int cnt;
	int fail = 0;

	if (keydir == NULL) {
		display(PERROR, "keydir argument required\n");
		EXIT_ERROR(ENOENT);
	}

	if ((dp = opendir(keydir)) == NULL) {
		display(PERROR, "Cannot find directory: %s\n", keydir);
		EXIT_ERROR(ENXIO);
	}

	for (mic = miclist; mic != NULL; mic = mic->next) {
		cnt = 0;
		if (micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO)) {
			display(PERROR, "%s: Not configured - skipping\n", mic->name);
			fail++;
			continue;
		}

		rewinddir(dp);

		while ((file = readdir(dp)) != NULL) {
			cnt += copy_key_file(mic, keydir, file->d_name);
		}

		if (cnt != LEN_KEYNAMES) {
			display(PERROR, "%s: Warning - only found %d of normal %ld files in %s\n",
				mic->name, cnt, LEN_KEYNAMES, keydir);
		} else {
			display(PNORM, "%s: Host ssh files copied from %s\n", mic->name, keydir);
		}
	}

	closedir(dp);
	exit(fail);
}

/*
 * Subfunction to hostkeys to do the actual copies for the indicated MIC card.
 */
int
copy_key_file(struct mic_info *mic, char *keydir, char *name)
{
	char iname[PATH_MAX];
	char oname[PATH_MAX];
	struct stat sbuf;
	char *buffer;
	unsigned int loop;
	int rlen;
	int fd;

	for (loop = 0; loop < LEN_KEYNAMES; loop++) {
		if (!strcmp(hostkeynames[loop], name)) {
			goto keyfilefound;
		}
	}

	// File in key directory that is not a normal key file name
	return 0;

keyfilefound:

	if (!mpssut_filename(&mpssenv, &sbuf, iname, PATH_MAX, "%s/%s", keydir, name)) {
		display(PERROR, "%s: Could not find %s\n", mic->name, iname);
		return 0;
	}

	if ((fd = open(iname, O_RDONLY)) < 0) {
		display(PERROR, "%s: Could not open %s\n", mic->name, iname);
		return 0;
	}

	if (mpssut_filename(&mpssenv, NULL, oname, PATH_MAX, "%s/etc/ssh/%s", mic->config.filesrc.mic.dir, name))
		unlink(oname);

	if ((buffer = (char *) malloc(sbuf.st_size + 1)) == NULL) {
		display(PERROR, "%s: could not alloc memory\n", mic->name);
		close(fd);
		return 0;
	}

	rlen = read(fd, buffer, sbuf.st_size);
	close(fd);

	if ((fd = open(oname, O_WRONLY|O_CREAT, sbuf.st_mode & 0777)) < 0) {
		display(PERROR, "%s: Could not open %s\n", mic->name, oname);
		free(buffer);
		return 0;
	}

	write(fd, buffer, rlen);
	close(fd);

	if ((mic->config.rootdev.type == ROOT_TYPE_NFS) ||
	    (mic->config.rootdev.type == ROOT_TYPE_SPLITNFS) ||
		(mic->config.rootdev.type == ROOT_TYPE_PFS)) {
		mpssut_filename(&mpssenv, NULL, oname, PATH_MAX, "%s/etc/ssh/%s",
			      strchr(mic->config.rootdev.target, ':') + 1, name);
		if ((fd = open(oname, O_WRONLY|O_CREAT) & 0777) < 0) {
			display(PERROR, "%s: Could not open %s\n", mic->name, oname);
			free(buffer);
			return 0;
		}

		write(fd, buffer, rlen);
		close(fd);
	}

	free(buffer);
	return 1;
}

#define MAC_RUN_SHIFT	1
#define MAC_DATE_SHIFT	16

int
mic_get_mac_from_serial(char *serial, unsigned char *mac, int host)
{
	unsigned long final;
	int y;
	int ww;

	if ((serial == NULL) || (serial[2] != 'K') || (serial[3] != 'C'))
		return 1;

#ifdef __KERNEL__
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,39)
	y = kstrtoul(&serial[7], 10, &final);	// y is to shutup Suse build
#else
	final = simple_strtoul(&serial[7], NULL, 10);
#endif
#else
	final = strtoul(&serial[7], NULL, 10);
#endif
	final = final << MAC_RUN_SHIFT;	/* Card side will add one */

	y = (serial[4] - '1');		/* start year 2012 end year 2016 */
	ww = ((serial[5] - '0') * 10) + (serial[6] - '0');

	final += (y * ww) << MAC_DATE_SHIFT;

	if (host)			/* least bit indicates host MAC */
		final++;

	mac[0] = 0x4c;
	mac[1] = 0x79;
	mac[2] = 0xba;
	mac[3] = (final >> 16) & 0xff;
	mac[4] = (final >> 8) & 0xff;
	mac[5] = final & 0xff;
	return 0;
}

void
get_mac_strings(struct mic_info *mic, char *micmac, char *hostmac, size_t macsize)
{
	char *serial;
	unsigned char mmac[6];
	unsigned char hmac[6];

	if (mic->config.net.hostMac == MIC_MAC_SERIAL) {
		serial = mpss_readsysfs(mic->name, "serialnumber");
		if (!mic_get_mac_from_serial(serial, mmac, 0)) {
			mic_get_mac_from_serial(serial, hmac, 1);
			sprintf(hostmac, "%02x:%02x:%02x:%02x:%02x:%02x",
				hmac[0], hmac[1], hmac[2], hmac[3], hmac[4], hmac[5]);
			sprintf(micmac, "%02x:%02x:%02x:%02x:%02x:%02x",
				mmac[0], mmac[1], mmac[2], mmac[3], mmac[4], mmac[5]);
		} else {
			if (mic->present) {
				sprintf(hostmac, "Set to Serial but serial number invalid");
				sprintf(micmac, "Set to Serial but serial number invalid");
			} else {
				sprintf(hostmac, "Serial");
				sprintf(micmac, "Serial");
			}
		}

		free(serial);
		return;
	}

	if (mic->config.net.hostMac == MIC_MAC_RANDOM) {
		strcpy(micmac, "Random");
		strcpy(hostmac, "Random");
		return;
	}


	if (mic->config.net.micMac == NULL) {
		strcpy(micmac, "Not Configured");
	} else {
		strncpy(micmac, mic->config.net.micMac, macsize - 1);
		micmac[macsize - 1] = '\0';
	}

	if (mic->config.net.hostMac == NULL) {
		strcpy(hostmac, "Not Configured");
	} else {
		strncpy(hostmac, mic->config.net.hostMac, macsize - 1);
		hostmac[macsize - 1] = '\0';
	}
}

void
show_net_config(struct mic_info *miclist)
{
	struct mic_info *mic;
	struct mbridge *br;
	char micmac[64];
	char hostmac[64];

	putchar('\n');
	for (mic = miclist; mic != NULL; mic = mic->next) {
		if (micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO)) {
			printf("%s: Not configured - skipping\n", mic->name);
			continue;
		}

		get_mac_strings(mic, micmac, hostmac, 64);

		switch(mic->config.net.type) {
		case NETWORK_TYPE_STATPAIR:
			printf("%s: StaticPair\n", mic->name);
			printf("    Hostname: %s\n", mic->config.net.hostname);
			printf("    Mic:      %s %s\n", micmac, mic->config.net.micIP);
			printf("    Host:     %s %s\n", hostmac, mic->config.net.hostIP);
			break;
		case NETWORK_TYPE_STATBRIDGE:
			if ((br = mpss_bridge_byname(mic->config.net.bridge, brlist)) == NULL)
				break;

			printf("%s: StaticBridge %s\n", mic->name, mic->config.net.bridge);
			printf("    Hostname: %s\n", mic->config.net.hostname);
			printf("    Mic:      %s %s\n", micmac, mic->config.net.micIP);
			printf("    Host:     %s %s\n", hostmac, br->ip);
			break;
		case NETWORK_TYPE_BRIDGE:
			printf("%s: Bridge %s with DHCP\n", mic->name, mic->config.net.bridge);
			break;
		default:
			printf("%s: Unknown network configuration type\n", mic->name);
		}
	}

	putchar('\n');
	exit(0);
}

void suse_rem_bridge_port(char *name, char *bridge);
void do_ifup(char *label, char *name, int saveresolv);
void do_hostaddbr(char *label, char *bridge);
void do_brctl_delbr(char *bridge);
void do_brctl_addif(char *label, char *bridge, char* intface);
void do_brctl_delif(char *label, char *bridge, char* intface);
void do_hostdelbr(char *label, char *bridge);
void save_resolv(int save, char **tmpname);
void restore_resolv(int save, char *tmpname);

void
net_remove_end(char *name, char *bridge, int modhost)
{
	char hostname[32];

	if (modhost) {
		rem_from_hosts(name, name);
		snprintf(hostname, 31, "host%s", name);
		rem_from_hosts(name, hostname);
	}

	if (bridge != NULL) {
		do_brctl_delif(name, bridge, name);
	}
}

void
ubuntu_net_remove(char *name, char *bridge, int modhost)
{
	char ifilename[PATH_MAX];
	char *ofilename;
	char pattern[256];
	char buffer[256];
	char *netdir = ni[mpssenv.dist].netdir;
	FILE *ifp;
	FILE *ofp;

	mpssut_filename(&mpssenv, NULL, ifilename, PATH_MAX, "%s/interfaces", netdir);

	if ((ofilename = mpssut_tempnam(ifilename)) == NULL) {
		display(PERROR, "%s: internal tempname failure - skipping\n", name);
		return;
	}

	snprintf(pattern, 256, "# %s BEGIN", name);

	do_ifdown(name, name, FALSE);

	if ((ifp = fopen(ifilename, "r")) == NULL) {
		display(PERROR, "%s: Error opening %s: %s\n", name, ifilename, strerror(errno));
		free(ofilename);
		return;
	}

	if ((ofp = fopen(ofilename, "w")) == NULL) {
		display(PERROR, "%s: Error opening %s: %s\n", name, ofilename, strerror(errno));
		free(ofilename);
		fclose(ifp);
		return;
	}

	while (fgets(buffer, 256, ifp)) {
		if (!strncmp(buffer, pattern, strlen(pattern))) {
			snprintf(pattern, 256, "# %s END", name);
			while (fgets(buffer, 256, ifp)) {
				if (!strncmp(buffer, pattern, strlen(pattern)))
					break;
			}
		} else {
			fprintf(ofp, "%s", buffer);
		}
	}

	fclose(ofp);
	fclose(ifp);

	if (mpssut_rename(&mpssenv, ofilename, ifilename)) {
			display(PERROR, "Failed to rename temporary file %s\n", ifilename);
			free(ofilename);
			return;
	}
	free(ofilename);

	ubuntu_unattach_hostbridge(name, bridge);
	net_remove_end(name, bridge, modhost);
}

void
ubuntu_br_remove(struct mbridge *br)
{
	char ifilename[PATH_MAX];
	char *ofilename;
	char pattern[256];
	char buffer[256];
	char *netdir = ni[mpssenv.dist].netdir;
	char *bridge = br->name;
	FILE *ifp;
	FILE *ofp;

	mpssut_filename(&mpssenv, NULL, ifilename, PATH_MAX, "%s/interfaces", netdir);

	if ((ofilename = mpssut_tempnam(ifilename)) == NULL) {
		display(PERROR, "default: internal tempname failure - skipping\n");
		return;
	}

	if ((br->type == BRIDGE_TYPE_EXT) |
	    (br->type == BRIDGE_TYPE_STATICEXT)) {
		display(PINFO, "default: External bridge %s - not removing from '%s'\n", bridge, ifilename);
		free(ofilename);
		return;
	}

	snprintf(pattern, 256, "# %s BEGIN", bridge);

	do_ifdown("default", bridge, FALSE);

	if ((ifp = fopen(ifilename, "r")) == NULL) {
		display(PERROR, "%s: Error opening %s: %s\n", bridge, ifilename, strerror(errno));
		free(ofilename);
		return;
	}

	if ((ofp = fopen(ofilename, "w")) == NULL) {
		display(PERROR, "%s: Error opening %s: %s\n", bridge, ofilename, strerror(errno));
		free(ofilename);
		fclose(ifp);
		return;
	}

	while (fgets(buffer, 256, ifp)) {
		if (strstr(buffer, "auto") && strstr(buffer, bridge)) {
			while (fgets(buffer, 256, ifp)) {
				if (strstr(buffer, "auto"))
					break;
			}
		} else {
			fprintf(ofp, "%s", buffer);
		}
	}

	fclose(ofp);
	fclose(ifp);

	if (mpssut_rename(&mpssenv, ofilename, ifilename)) {
			display(PERROR, "Failed to rename temporary file %s\n", ifilename);
			free(ofilename);
			return;
	}
	free(ofilename);
}

void
common_net_remove(char *name, char *bridge, int modhost)
{
	char filename[PATH_MAX];
	char *netdir = ni[mpssenv.dist].netdir;

	if (mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s/ifcfg-%s", netdir, name)) {
		do_ifdown(name, name, TRUE);
		unlink(filename);
		display(PFS, "%s: Remove %s\n", name, filename);
		sleep(2);
		suse_rem_bridge_port(name, bridge);
	}

	net_remove_end(name, bridge, modhost);
}

void
common_br_remove(struct mbridge *br)
{
	char filename[PATH_MAX];
	char *netdir = ni[mpssenv.dist].netdir;
	char *bridge = br->name;

	if ((br->type == BRIDGE_TYPE_EXT) |
	    (br->type == BRIDGE_TYPE_STATICEXT)) {
		display(PINFO, "default: External bridge - not removing '%s'\n", filename);
		return;
	}

	if (!mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s/ifcfg-%s", netdir, bridge))
		return;

	do_ifdown("default", bridge, FALSE);
	do_hostdelbr("default", bridge);
	unlink(filename);
	display(PFS, "default: Removed '%s'\n", filename);
}

void
suse_add_bridge_port(char *name, char *bridge)
{
	char filename[PATH_MAX];
	char *tempname;
	FILE *ifp;
	FILE *ofp;
	char line[256];
	char *apostrophe;

	if (bridge == NULL)
		return;

	if (mpssenv.dist != DISTRIB_SUSE)
		return;

	mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s/ifcfg-%s", ni[mpssenv.dist].netdir, bridge);

	if ((tempname = mpssut_tempnam(filename)) == NULL) {
		display(PERROR, "%s: internal tempname failure - skipping\n", name);
		return;
	}

	if (((ifp = fopen(filename, "r")) == NULL) ||
	    ((ofp = fopen(tempname, "w")) == NULL)) {
		if (ifp) fclose(ifp);
		display(PWARN, "cannot find %s\n", filename);
		free(tempname);
		return;
	}

	while (fgets(line, 256, ifp)) {
		if (!strncmp(line, "BRIDGE_PORTS=", strlen("BRIDGE_PORTS="))) {
			if ((apostrophe = strchr(line, '\'')) == NULL) {
				fprintf(ofp, "BRIDGE_PORTS='%s'\n", name);
				break;
			}

			if ((apostrophe = strchr(++apostrophe, '\'')) == NULL) {
				fprintf(ofp, "BRIDGE_PORTS='%s'\n", name);
				break;
			}

			*apostrophe = '\0';
			fprintf(ofp, "%s %s'\n", line, name);
		} else {
			fprintf(ofp, "%s", line);
		}
	}

	fclose(ofp);
	fclose(ifp);
	if (mpssut_rename(&mpssenv, tempname, filename)) {
			display(PERROR, "Failed to rename temporary file %s\n", filename);
			free(tempname);
			return;
	}

	free(tempname);
	return;
}

void
suse_rem_bridge_port(char *name, char *bridge)
{
	char filename[PATH_MAX];
	char *tempname;
	FILE *ifp;
	FILE *ofp;
	char line[256];
	char *start;
	char *end;

	if (mpssenv.dist != DISTRIB_SUSE)
		return;

	mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s/ifcfg-%s", ni[mpssenv.dist].netdir, bridge);

	if ((tempname = mpssut_tempnam(filename)) == NULL) {
		display(PERROR, "%s: internal tempname failure - skipping\n", name);
		return;
	}

	if (((ifp = fopen(filename, "r")) == NULL) ||
	    ((ofp = fopen(tempname, "w")) == NULL)) {
		if (ifp) fclose(ifp);
		free(tempname);
		return;
	}

	while (fgets(line, 256, ifp)) {
		if (!strncmp(line, "BRIDGE_PORTS=", strlen("BRIDGE_PORTS="))) {
			if ((start = strchr(line, '\'')) == NULL) {
				fprintf(ofp, "%s", line);
				break;
			}

			*start++ = '\0';
			fprintf(ofp, "%s'", line);
			while (1) {
				if (((end = strchr(start, ' ')) == NULL) &&
				    ((end = strchr(start, '\'')) == NULL)) {
					break;
				}

				*end++ = '\0';
				if (strlen(start)) {
					if (strcmp(start, name))
						fprintf(ofp, "%s ", start);
					else
						display(PFS, "%s: Update %s remove '%s' from BRIDGE_PORTS\n",
								name, filename, name);
				}
				start = end;
			}

			fprintf(ofp, "'\n");
		} else {
			fprintf(ofp, "%s", line);
		}
	}

	fclose(ofp);
	fclose(ifp);
	if (mpssut_rename(&mpssenv, tempname, filename)) {
			display(PERROR, "Failed to rename temporary file %s\n", filename);
			free(tempname);
			return;
	}
	free(tempname);
	return;
}

void
ubuntu_unattach_hostbridge(char *name, char *bridge)
{
	char ifilename[PATH_MAX];
	char *ofilename;
	char buffer[256];
	char new_bridge[256];
	char *micstr;
	FILE *ifp;
	FILE *ofp;

	if (bridge == NULL)
		return;

	snprintf(ifilename, PATH_MAX, "%s/interfaces", ni[mpssenv.dist].netdir);

	if ((ofilename = mpssut_tempnam(ifilename)) == NULL) {
		display(PERROR, "%s: internal tempname failure - skipping\n", name);
		free(ofilename);
		return;
	}

	if ((ifp = fopen(ifilename, "r")) == NULL) {
		display(PERROR, "%s: Error opening %s: %s\n", name, ifilename, strerror(errno));
		free(ofilename);
		return;
	}

	if ((ofp = fopen(ofilename, "w")) == NULL) {
		display(PERROR, "%s: Error opening %s: %s\n", name, ofilename, strerror(errno));
		free(ofilename);
		fclose(ifp);
		return;
	}

	while (fgets(buffer, 256, ifp)) {
		if (strstr(buffer, "iface") && strstr(buffer, bridge)) {
			fprintf(ofp, "%s", buffer);
			while (fgets(buffer, 256, ifp)) {
				if (strstr(buffer, "bridge_ports") != NULL) {
					if ((micstr = strstr(buffer, name)) != NULL) {
						strcpy(new_bridge, micstr + strlen(name));
						strcpy(micstr - 1, new_bridge);
					}
				}
				fprintf(ofp, "%s", buffer);
			}
		} else {
			fprintf(ofp, "%s", buffer);
		}
	}

	fclose(ofp);
	fclose(ifp);
	if (mpssut_rename(&mpssenv, ofilename, ifilename)) {
			display(PERROR, "Failed to rename temporary file %s\n", ifilename);
			free(ofilename);
			return;
	}
	free(ofilename);
}

void
ubuntu_attach_hostbridge(char *name, char *bridge, char *mac, char *mtu, int saveresolv)
{
	char ifilename[PATH_MAX];
	char *ofilename;
	char buffer[256];
	char *netdir = ni[mpssenv.dist].netdir;
	FILE *ifp;
	FILE *ofp;

	snprintf(ifilename, PATH_MAX, "%s/interfaces", netdir);

	if ((ofilename = mpssut_tempnam(ifilename)) == NULL) {
		display(PERROR, "%s: internal tempname failure - skipping\n", name);
		return;
	}

	if ((ifp = fopen(ifilename, "r")) == NULL) {
		display(PERROR, "%s: Error opening %s: %s\n", name, ifilename, strerror(errno));
		free(ofilename);
		return;
	}

	if ((ofp = fopen(ofilename, "w")) == NULL) {
		display(PERROR, "%s: Error opening %s: %s\n", name, ofilename, strerror(errno));
		free(ofilename);
		fclose(ifp);
		return;
	}

	while (fgets(buffer, 256, ifp)) {
		if (strstr(buffer, "iface") && strstr(buffer, bridge)) {
			fprintf(ofp, "%s", buffer);
			while (fgets(buffer, 256, ifp)) {
				if (strstr(buffer, "bridge_ports") != NULL) {
					char tmp[256];
					buffer[sizeof(buffer) - 1] = 0;
					buffer[strlen(buffer) - 1] = 0;
					strncpy(tmp, buffer, sizeof(tmp) > sizeof(buffer) ? sizeof(buffer) : sizeof(tmp));
					snprintf(buffer, sizeof(buffer) - 1, "%s %s\n", tmp, name);
				}
				fprintf(ofp, "%s", buffer);
			}
		} else {
			fprintf(ofp, "%s", buffer);
		}
	}

	fclose(ofp);
	fclose(ifp);
	if (mpssut_rename(&mpssenv, ofilename, ifilename)) {
			display(PERROR, "Failed to rename temporary file %s\n", ifilename);
			free(ofilename);
			return;
	}
	free(ofilename);

	do_brctl_addif(name, bridge, name);
	do_ifup(name, name, FALSE);
	do_ifup(name, bridge, FALSE);
}

void
common_attach_hostbridge(char *name, char *bridge, char *mac, char *mtu, int saveresolv)
{
	char filename[PATH_MAX];
	FILE *fp;

	if (mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s/ifcfg-%s", ni[mpssenv.dist].netdir, name))
		return;

	if ((fp = fopen(filename, "w")) == NULL) {
		display(PERROR, "%s: Failed to open %s: %s\n",name, filename, strerror(errno));
		return;
	}

	display(PINFO, "%s: Attach to bridge %s\n", name, bridge);
	fprintf(fp, "DEVICE=%s\n", name);
	fprintf(fp, "ONBOOT=yes\nNM_CONTROLLED=\"no\"\n");
	fprintf(fp, "BRIDGE=%s\n", bridge);

	if (mpssenv.dist == DISTRIB_SUSE)
		fprintf(fp, "STARTMODE=auto\n");

	if (mtu != NULL)
		fprintf(fp, "MTU=%s\n", mtu);

	if (mac > MIC_MAC_RANDOM) {
		fprintf(fp, "MACADDR=%s\n", mac);
	}

	fclose(fp);
	display(PFS, "%s: Created %s\n", name, filename);

	do_ifup(name, name, saveresolv);
	do_brctl_addif(name, bridge, name);
	do_ifup(name, bridge, FALSE);

	if (mpssenv.dist == DISTRIB_SUSE) {
		suse_add_bridge_port(name, bridge);
	}
}

void
ubuntu_remove_bridges(void)
{
	// TODO
}

void
common_remove_bridges(void)
{
        struct mbridge *br = brlist;
        char filename[PATH_MAX];
        struct stat sbuf;

        if (mpssenv.dist == DISTRIB_UBUNTU)
                return;

        while (br) {
                mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s/ifcfg-%s", ni[mpssenv.dist].netdir, br->name);
                if ((br->type == BRIDGE_TYPE_EXT) |
                    (br->type == BRIDGE_TYPE_STATICEXT)) {
                        display(PFS, "default: External bridge not removing '%s'\n", filename);
                } else {
                        if (stat(filename, &sbuf) == 0) {
                                do_ifdown("default", br->name, FALSE);
                                do_hostdelbr("default", br->name);
                                unlink(filename);
                                display(PFS, "default: Remove %s\n", filename);
                        }
                }

                br = br->next;
        }

        mpss_free_bridgelist(&brlist);
}

void
remove_bridges(void)
{
	ni[mpssenv.dist].remove_bridges();
}

void
add_dhcp(char *bridge, char *ip, char *netbits, char *mtu, struct mic_info *miclist)
{
	struct mic_info *mic;
	struct mbridge *br;
	int fail = 0;
	int err = 0;

	if (check_for_brctl()) {
		display(PERROR, "%s: Failed - required brctl command not installed\n", bridge);
		EXIT_ERROR(ESRCH);
	}

	if (bridge == NULL) {
		display(PERROR, "DHCP requires a bridge to attach to\n");
		EXIT_ERROR(ENODEV);
	}

	if ((br = mpss_bridge_byname(bridge, brlist)) == NULL) {
		display(PERROR, "\nBridge '%s' not defined\n\n", bridge);
		EXIT_ERROR(ESRCH);
	}

	if ((br->type != BRIDGE_TYPE_EXT) &&
	    (br->type != BRIDGE_TYPE_STATICEXT)) {
		display(PERROR, "\nBridge '%s' is not external\n\n", bridge);
		EXIT_ERROR(EINVAL);
	}

	if (netbits != NULL)
		display(PWARN, "Adding an interface to a bridge will use the bridges "
			       "netmask - ignoring netbits %s\n", netbits);
	if (mtu != NULL)
		display(PWARN, "Adding an interface to a bridge will use the bridges "
			       "MTU - ignoring %s\n", mtu);

	if (ip != NULL) {
		display(PWARN, "DHCP config will ignore ip address input '%s'\n\n", ip);
	}

	for (mic = miclist; mic != NULL; mic = mic->next) {
		if ((mic->config.net.type != NETWORK_TYPE_BRIDGE) ||
		    (strcmp(mic->config.net.bridge, bridge))) {
			display(PNORM, "%s: Changing network to external bridge %s\n", mic->name, bridge);
			ni[mpssenv.dist].net_remove(mic->name, mic->config.net.bridge, mic->config.net.modhost);
			mic->config.net.type = NETWORK_TYPE_BRIDGE;

			mpssut_realloc_fill(&mic->config.net.micIP, "dhcp");
			mpssut_realloc_fill(&mic->config.net.bridge, bridge);
			mic->config.net.mtu = br->mtu;
			ni[mpssenv.dist].attach_hostbridge(mic->name, bridge, mic->config.net.hostMac, br->mtu, FALSE);
			gen_ipinfo(mic, 1);

			if ((err = mpss_update_config(&mpssenv, mic->config.name, "Network", NULL,
						      "Network class=Bridge bridge=%s\n", bridge))) {
				display(PERROR, "Failed to open config file '%s': %d\n", mic->config.name, err);
				fail++;
			} else {
				display(PFS, "%s: Update Network in %s\n", mic->name, mic->config.name);
			}
		}
	}

	gen_etchosts(miclist);
	exit(fail);
}

char *ips[256][2];

int
get_ips(char *list, char **ipbuf)
{
	int cnt = 0;
	char *next1 = list;
	char *next2;
	char *buffer;
	int i;

	if ((buffer = (char *) malloc(strlen(list) + 1)) == NULL) {
		*ipbuf = NULL;
		return -1;
	}

	strcpy(buffer, list);
	next1 = buffer;

	while ((next2 = strchr(next1, ':')) != NULL) {
		*next2 = '\0';
		ips[cnt++][0] = next1;
		next1 = next2 + 1;
	}

	ips[cnt++][0] = next1;

	for (i = 0; i < cnt; i++) {
		if ((next1 = strchr(ips[i][0], ',')) != NULL) {
			*next1 = '\0';
			ips[i][1] = next1 + 1;
		} else {
			ips[i][1] = NULL;
		}
	}

	*ipbuf = buffer;
	return cnt;
}

int
check_for_doubles(int cnt)
{
	int i;

	for (i = 0; i < cnt; i++)
		if (ips[i][1] != NULL)
			return i + 1;
	return 0;
}

void
populate_bridgeip(char *ip, int miccnt)
{
	int ip1[4];
	int i;

	if (mpssnet_decode_ipaddr(ip, ip1, 4)) {
		display(PERROR, "The single IP %s to fill must be a full starting address\n", ip);
		EXIT_ERROR(EINVAL);
	}

	for (i = 1; i < miccnt; i++)
		if ((ips[i][0] = (char *) malloc(16)) != NULL) {
			sprintf(ips[i][0], "%d.%d.%d.%d", ip1[0], ip1[1], ip1[2], ip1[3] + i);
		}
}

unsigned int
getmaskfromprefix(char *prefix)
{
	unsigned int mask = 0x0;
	int bits = atoi(prefix);
	int i;

	for (i = 31; i >= 0; i--) {
		if (bits) {
			mask |= (1 << i);
			bits--;
		}
	}

	return mask;
}

unsigned int
getmaskfromIP(int *ip)
{
	unsigned int mask = (ip[0] << 24) + (ip[1] << 16) + (ip[2] << 8) + ip[3];
	return mask;
}

void
verify_ips(struct mbridge *br, int ip_cnt, int bridge)
{
	int brip[4];
	int ip1[4];
	int ip2[4];
	int i;
	int j;
	unsigned int premask;
	unsigned int bridgemask;
	unsigned int newmask;

	if (bridge && strcmp(br->ip, "dhcp")) {
		if (mpssnet_decode_ipaddr(br->ip, brip, 4)) {
			display(PERROR, "Bridge IP %s not valid\n", br->ip);
			EXIT_ERROR(EINVAL);
		}
	}

	for (i = 0; i < ip_cnt; i++) {
		if (mpssnet_decode_ipaddr(ips[i][0], ip1, 4)) {
			display(PERROR, "IP %d (%s) not valid\n", i, ips[i][0]);
			EXIT_ERROR(EINVAL);
		}

		if (bridge) {
			premask = getmaskfromprefix(br->prefix);
			bridgemask = getmaskfromIP(brip) & premask;
			newmask = getmaskfromIP(ip1) & premask;
			if (strcmp(br->ip, "dhcp") && (bridgemask != newmask)) {
				display(PERROR, "IP %s cannot join bridge %s\n",
						ips[i][0], br->ip);
				EXIT_ERROR(EACCES);
			}
		} else {
			for (j = 0; j < 3; j++) {
				if (mpssnet_decode_ipaddr(ips[i][1], ip2, 4)) {
					display(PERROR, "IP %d (%s) not valid\n", i, ips[i][0]);
					EXIT_ERROR(EINVAL);
				}

				if (ip1[j] != ip2[j]) {
					display(PERROR, "IPs %s and %s are not on the same subnet\n",
							ips[i][0], ips[i][1]);
					EXIT_ERROR(EACCES);
				}
			}
		}
	}
}

void
update_etchosts(struct mic_info *miclist, struct mic_info *mic)
{
	char filename[PATH_MAX];
	char *tempname;
	char line[256];
	char ip[32];
	char name[32];
	char other[32];
	char *rest;
	FILE *ifp;
	FILE *ofp;
	int cnt;

	mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s/etc/hosts", miclist->config.filesrc.mic.dir);

	if ((tempname = mpssut_tempnam(filename)) == NULL) {
		display(PERROR, "%s: internal tempname failure - skipping\n", name);
		return;
	}

	if (((ifp = fopen(filename, "r")) == NULL) ||
	    ((ofp = fopen(tempname, "w")) == NULL)) {
		free(tempname);
		if (ifp) fclose(ifp);
		return;
	}

	while (fgets(line, 256, ifp)) {
		if ((cnt = sscanf(line, "%" str(32) "s %" str(32) "s %" str(32) "s",
				  ip, name, other)) < 2)
			continue;

		if (strcmp(name, mic->name)) {
			fprintf(ofp, "%s", line);
		} else {
			if (cnt > 2) {
				rest = line + strlen(name) + strlen(ip) + 2;
				fprintf(ofp, "%s\t%s %s\n", mic->config.net.micIP, name, rest);
			} else {
				fprintf(ofp, "%s\t%s\n", mic->config.net.micIP, name);
			}
		}
	}

	fclose(ofp);
	fclose(ifp);
	if (mpssut_rename(&mpssenv, tempname, filename)) {
			display(PERROR, "Failed to rename temporary file %s\n", filename);
	}
	free(tempname);
	return;
}

void
copy_etchost(struct mic_info *mic)
{
	int ifd;
	int ofd;
	char filename[PATH_MAX];
	char buffer[4096];
	struct stat sbuf;
	ssize_t read_n;


	if (mic->config.net.type == -1)
		return;

	if (mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s/etc/hosts", mic->config.filesrc.mic.dir)) {
		unlink(filename);
	}

	if ((ifd = open(mic->config.net.modcard, O_RDONLY)) == -1) {
		display(PERROR, "%s: Failed to open hosts file '%s': %s\n", mic->name, mic->config.net.modcard, strerror(errno));
		return;
	}

	if (fstat(ifd, &sbuf) == -1) {
		display(PERROR, "%s: Failed to stat hosts file '%s': %s\n", mic->name, mic->config.net.modcard, strerror(errno));
		close(ifd);
		return;
	}

	if ((ofd = open(filename, O_WRONLY | O_CREAT | O_EXCL, sbuf.st_mode)) == -1) {
		display(PERROR, "%s: Failed to create file '%s': %s\n", mic->name, filename, strerror(errno));
		close(ifd);
		return;
	}

	while ((read_n = read(ifd, buffer, sizeof(buffer))) > 0) {
		if (write(ofd, buffer, read_n) != read_n) {
			close(ifd);
			close(ofd);
			display(PERROR, "%s: Failed to copy %s to %s\n", mic->name, mic->config.net.modcard, filename);
			return;
		}
	}

	display(PFS, "%s: Created %s from %s\n", mic->name, filename, mic->config.net.modcard);
	close(ifd);
	close(ofd);
}

void
gen_etchost(struct mic_info *mic, struct mic_info *hostlist)
{
	char filename[PATH_MAX];
	char nfsname[PATH_MAX];
	struct mic_info *miclist;
	struct mbridge *br;
	struct utsname unamebuf;
	FILE *fp;
	FILE *nfsfp = NULL;
	char *header;

	if (mic->config.net.type == -1)
		return;

	if (mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s/etc/hosts", mic->config.filesrc.mic.dir)) {
		unlink(filename);
		header = "Update";
	} else {
		header = "Create";
	}

	// Gen cards /etc/hosts file
	if ((fp = fopen(filename, "w")) == NULL) {
		display(PWARN, "%s: gen hosts error opening %s: %s\n", mic->name,
			       filename, strerror(errno));
		return;
	}

	fprintf(fp, "127.0.0.1\tlocalhost.localdomain localhost\n");
	fprintf(fp, "::1\t\tlocalhost.localdomain localhost\n\n");

	if ((mic->config.rootdev.type == ROOT_TYPE_NFS) ||
	    (mic->config.rootdev.type == ROOT_TYPE_SPLITNFS) ||
		(mic->config.rootdev.type == ROOT_TYPE_PFS)) {
		mpssut_filename(&mpssenv, NULL, nfsname, PATH_MAX, "%s/etc/hosts",
			      strchr(mic->config.rootdev.target, ':') + 1);

		if ((nfsfp = fopen(nfsname, "w")) == NULL) {
			display(PWARN, "%s: Error opening %s: %s\n", mic->name,
				nfsname, strerror(errno));
		} else {
			fprintf(nfsfp, "127.0.0.1\tlocalhost.localdomain localhost\n");
			fprintf(nfsfp, "::1\t\tlocalhost.localdomain localhost\n\n");
		}
	}

	uname(&unamebuf);

	switch (mic->config.net.type) {
	case NETWORK_TYPE_STATPAIR:
		if (mic->config.net.hostIP == NULL)
			break;

		if (strncmp(unamebuf.nodename, "localhost", 8))
			fprintf(fp, "%s\thost %s\n", mic->config.net.hostIP, unamebuf.nodename);
		else
			fprintf(fp, "%s\thost\n", mic->config.net.hostIP);

		if (nfsfp) {
			if (strncmp(unamebuf.nodename, "localhost", 8))
				fprintf(nfsfp, "%s\thost %s\n", mic->config.net.hostIP,
					unamebuf.nodename);
			else
				fprintf(nfsfp, "%s\thost\n", mic->config.net.hostIP);
		}
		break;

	case NETWORK_TYPE_STATBRIDGE:
		if ((br = mpss_bridge_byname(mic->config.net.bridge, brlist)) == NULL) {
			display(PWARN, "%s: Bridge '%s' not defined - skipping generation of cards "
				"/etc/hosts file\n", mic->name, mic->config.net.bridge);
			break;
		}

		if (strncmp(unamebuf.nodename, "localhost", 8))
			fprintf(fp, "%s\thost %s\n", br->ip, unamebuf.nodename);
		else
			fprintf(fp, "%s\thost\n", br->ip);

		if (nfsfp) {
			if (strncmp(unamebuf.nodename, "localhost", 8))
				fprintf(nfsfp, "%s\thost %s\n", br->ip, unamebuf.nodename);
			else
				fprintf(nfsfp, "%s\thost\n", br->ip);
		}
		break;

	default:
		goto closehosts;
	}

	for (miclist = hostlist; miclist != NULL; miclist = miclist->next) {
		if (miclist->config.valid != TRUE)
			continue;

		fprintf(fp, "%s\t%s %s\n", miclist->config.net.micIP,
			    miclist->config.net.hostname, miclist->name);

		if (nfsfp)
			fprintf(nfsfp, "%s\t%s %s\n", miclist->config.net.micIP,
				    miclist->config.net.hostname, miclist->name);

		if (strcmp(mic->name, miclist->name) && miclist->config.net.micIP) {
			update_etchosts(miclist, mic);
		}
	}

closehosts:
	fclose(fp);
	display(PFS, "%s: %s %s\n", mic->name, header, filename);
	if (nfsfp) {
		fclose(nfsfp);
		display(PFS, "%s: %s %s\n", mic->name, header, nfsname);
	}
}

void
delete_etchost(struct mic_info *mic)
{
	char filename[PATH_MAX];

	if (mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s/etc/hosts", mic->config.filesrc.mic.dir)) {
		unlink(filename);
	}
}

void
gen_etchosts(struct mic_info *miclist)
{
	struct mic_info *mic;
	struct mic_info *full_list;

	if ((full_list = mpss_get_miclist(&mpssenv, NULL)) == NULL) {
		full_list = miclist;
	} else {
		for (mic = full_list; mic != NULL; mic = mic->next) {
			micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO);
		}
	}

	for (mic = full_list; mic != NULL; mic = mic->next) {
		if (mic->config.valid != TRUE)
			continue;

		if ((mic->config.net.modcard == NULL) || (!strcmp(mic->config.net.modcard, "yes")))
			gen_etchost(mic, full_list);
		else if (!strcmp(mic->config.net.modcard, "no"))
			delete_etchost(mic);
		else
			copy_etchost(mic);
	}
}

void
rem_from_hosts(char *micid, char *name)
{
	char *tempname;
	char line[256];
	char IPaddr[32];
	char names[3][32];
	FILE *ifp;
	FILE *ofp;
	int cnt;
	int i;
	char comment[] = "#Generated-by-micctrl";
	int auto_entry = FALSE;
	int entry_found = FALSE;

	if (!mpssenv.live_update)
		return;

	if ((tempname = mpssut_tempnam("/etc/hosts")) == NULL) {
		display(PERROR, "%s: internal tempname failure - skipping\n", name);
		return;
	}

	if (((ifp = fopen("/etc/hosts", "r")) == NULL) ||
	    ((ofp = fopen(tempname, "w")) == NULL)) {
		display(PINFO, "%s: Cannot process /etc/hosts\n", micid);
		if (ifp) fclose(ifp);
		free(tempname);
		return;
	}

	while (fgets(line, 256, ifp)) {
		entry_found = FALSE;
		auto_entry = FALSE;
		line[strlen(line) - 1] = '\0';
		cnt = sscanf(line, "%" str(32) "s %" str(32) "s %" str(32) "s %" str(32) "s",
			     IPaddr, names[0], names[1], names[2]);
		if (cnt < 2)
			continue;

		for (i = 0; i < cnt - 1; i++) {
			if (!strcmp(names[i], name)) {
				entry_found = TRUE;
			}
			if (!strcmp(names[i], comment)) {
				auto_entry = TRUE;
			}
		}

		// Print current line only if it is not an auto-generated entry in hosts.
		if (!(entry_found && auto_entry)){
			fprintf(ofp, "%s\n", line);
		}
	}

	fclose(ofp);
	fclose(ifp);
	if (mpssut_rename(&mpssenv, tempname, "/etc/hosts")) {
			display(PERROR, "Failed to rename temporary file %s\n", "/etc/hosts");
			free(tempname);
			return;
	}
	free(tempname);
	display(PFS, "%s: Update /etc/hosts remove %s\n", micid, name);
	return;
}

void
add_to_hosts(char *micid, char *hostname, char *ip)
{
	char *tempname;
	char line[256];
	char IPaddr[32];
	char names[3][32];
	FILE *ifp;
	FILE *ofp;
	int cnt;
	int i;
	char comment[] = "#Generated-by-micctrl";
	int entry_found = FALSE;

	if (!mpssenv.live_update)
		return;

	if (!strcmp(ip, "dhcp"))
		return;

	if ((tempname = mpssut_tempnam("/etc/hosts")) == NULL) {
		display(PERROR, "%s: internal tempname failure - skipping\n", micid);
		return;
	}

	if (((ifp = fopen("/etc/hosts", "r")) == NULL) ||
	    ((ofp = fopen(tempname, "w")) == NULL)) {
		display(PINFO, "%s: Cannot process /etc/hosts\n", micid);
		if (ifp) fclose(ifp);
		free(tempname);
		return;
	}

	while (fgets(line, 256, ifp)) {
		if (*line == '#')
			goto write_line;

		cnt = sscanf(line, "%" str(32) "s %" str(32) "s %" str(32) "s %" str(32) "s",
			     IPaddr, names[0], names[1], names[2]);
		if (cnt < 2)
			goto write_line;

		for (i = 0; i < cnt - 1; i++) {
			if (!strcmp(names[i], hostname)) {
				if (strstr(line, comment) && !strcmp(IPaddr, ip)) {
					display(PINFO, "%s: Removing conflicting existing /etc/hosts entry: %s",
						micid, line);
					fprintf(ofp, "#");
				} else if (strcmp(IPaddr, ip)) {
					display(PWARN, "%s: Will not update /etc/hosts due to conflicting entry: %s",
						micid, line);
					entry_found = TRUE;
				} else {
					display(PINFO, "%s: Using existing /etc/hosts entry: %s", micid, line);
					entry_found = TRUE;
				}

				break;
			}
		}

write_line:
		fprintf(ofp, "%s", line);
	}

	// Adding new auto-generated entry to end of hosts file.
	if (entry_found == FALSE) {
		fprintf(ofp, "%s\t%s %s %s\n", ip, hostname, micid, comment);
		display(PFS, "%s: Update /etc/hosts with %s %s\n", micid, ip, hostname);
	}

	// Finish file copy.
	fclose(ofp);
	fclose(ifp);
	if (mpssut_rename(&mpssenv, tempname, "/etc/hosts")) {
			display(PERROR, "Failed to rename temporary file %s\n", "/etc/hosts");
	}
	free(tempname);
	return;
}

void
get_gateway(struct mic_info *mic)
{
	char buf[256];
	char ifname[32];
	unsigned int dest;
	unsigned int gateway;
	unsigned int flags;
	FILE *fp;

	if ((fp = fopen("/proc/net/route", "r")) == NULL) {
		mic->config.net.gateway = NULL;
		return;
	}

	while (fgets(buf, 256, fp)) {
		if (sscanf(buf, "%" str(32) "s %x %x %x", ifname, &dest, &gateway, &flags) < 4)
			continue;

		if ((flags & 0x2) && (dest == 0)) {
			snprintf(buf, 256, "%u.%u.%u.%u", gateway & 0xff, (gateway >> 8) & 0xff,
					(gateway >> 16) & 0xff, gateway >> 24);
			mpssut_realloc_fill(&mic->config.net.gateway, buf);
			break;
		}
	}

	fclose(fp);
}

void mic_add_if_to_bridge(struct mic_info *mic, struct mbridge *br, char *bridge, char *gw, int mh, char *mc, int ip_cnt);

void
add_if_to_bridge(char *bridge, char *ip, char *gw, char *netbits, char *mtu,
		 int mhost, char *mcard, struct mic_info *miclist, int miccnt)
{
	struct mic_info *mic;
	struct mic_info *hostlist;
	struct mbridge *br;
	int ip_cnt;
	int err;
	char *ipbuf;

	if (bridge == NULL) {
		display(PERROR, "Static bridge requires a bridge to attach to\n");
		EXIT_ERROR(ENODEV);
	}

	if ((br = mpss_bridge_byname(bridge, brlist)) == NULL) {
		display(PERROR, "Bridge '%s' not defined\n", bridge);
		EXIT_ERROR(ESRCH);
	}

	if (ip == NULL) {
		display(PERROR, "Static bridge requires an ip address\n");
		EXIT_ERROR(EINVAL);
	}

	if (netbits != NULL)
		display(PERROR, "Adding an interface to a bridge will use the bridges "
				"netmask - ignoring netbits %s\n", netbits);
	if (mtu != NULL)
		display(PERROR, "Adding an interface to a bridge will use the bridges "
				"MTU - ignoring %s\n", mtu);

	ip_cnt = get_ips(ip, &ipbuf);

	if ((err = check_for_doubles(ip_cnt)) > 0) {
		display(PERROR, "IP %d specifies more than one IP address\n", err);
		EXIT_ERROR(EINVAL);
	}

	if (miccnt != ip_cnt) {
		if (ip_cnt != 1) {
			display(PERROR, "IP %d count does not match MIC list\n", ip_cnt);
			EXIT_ERROR(ESRCH);
		}

		populate_bridgeip(ip, miccnt);
		ip_cnt = miccnt;
	}

	verify_ips(br, ip_cnt, 1);

	if ((hostlist = mpss_get_miclist(&mpssenv, NULL)) == NULL)
		hostlist = miclist;

	for (mic = miclist, ip_cnt = 0; mic != NULL; mic = mic->next, ip_cnt++)
		mic_add_if_to_bridge(mic, br, bridge, gw, mhost, mcard, ip_cnt);

	free(ipbuf);

	gen_etchosts(miclist);
}

void
mic_add_if_to_bridge(struct mic_info *mic, struct mbridge *br, char *bridge, char *gw, int mhost, char *mcard, int ip_cnt)
{
	int save_gateway = TRUE;
	int err;

	if ((mic->config.net.type == NETWORK_TYPE_STATBRIDGE) &&
	    !strcmp(mic->config.net.bridge, bridge) &&
	    !strcmp(mic->config.net.micIP, ips[ip_cnt][0])) {
		display(PNORM, "%s: Is already part of bridge %s at %s\n", mic->name, bridge, ips[ip_cnt][0]);
		return;
	}

	display(PNORM, "%s: Changing network to static bridge %s at %s\n", mic->name, bridge, ips[ip_cnt][0]);
	ni[mpssenv.dist].net_remove(mic->name, mic->config.net.bridge, mic->config.net.modhost);
	mic->config.net.type = NETWORK_TYPE_STATBRIDGE;

	mpssut_realloc_fill(&mic->config.net.micIP, ips[ip_cnt][0]);
	mpssut_realloc_fill(&mic->config.net.bridge, bridge);

	if (gw != NULL) {
		mpssut_realloc_fill(&mic->config.net.gateway, gw);
	} else if (mic->config.net.gateway == NULL) {
		save_gateway = FALSE;
		get_gateway(mic);
	}

	mic->config.net.mtu = br->mtu;
	ni[mpssenv.dist].attach_hostbridge(mic->name, bridge, mic->config.net.hostMac, mic->config.net.mtu, TRUE);
	gen_ipinfo(mic, 1);

	if (mhost == -1) {
		display(PINFO, "%s: Use current modhost %s\n", mic->name,
				mic->config.net.modhost? "yes" : "no");
		mhost = mic->config.net.modhost;
	} else {
		mic->config.net.modhost = mhost;
	}

	if (mcard != NULL) {
		display(PINFO, "%s: Use current modcard %s\n", mic->name,
				mic->config.net.modcard);
		mcard = mic->config.net.modcard;
	} else {
		mic->config.net.modcard = mcard;
	}

	if (save_gateway) {
		if ((err = mpss_update_config(&mpssenv, mic->config.name, "Network", NULL,
				"Network class=StaticBridge bridge=%s micip=%s "
				"modhost=%s modcard=%s gw=%s\n", bridge, ips[ip_cnt][0],
				mhost? "yes" : "no", mcard? mcard : "yes",
				mic->config.net.gateway)))
			display(PERROR, "Failed to open config file '%s': %d\n", mic->config.name, err);
		else
			display(PFS, "%s: Update Network in %s\n", mic->name, mic->config.name);
	} else {
		if ((err = mpss_update_config(&mpssenv, mic->config.name, "Network", NULL,
				"Network class=StaticBridge bridge=%s micip=%s "
				"modhost=%s modcard=%s\n", bridge, ips[ip_cnt][0],
				mhost? "yes" : "no", mcard? mcard : "yes")))
			display(PERROR, "Failed to open config file '%s': %d\n", mic->config.name, err);
		else
			display(PFS, "%s: Update Network in %s\n", mic->name, mic->config.name);
	}

	if (mhost && mic->config.net.micIP && mpssenv.live_update) {
		add_to_hosts(mic->name, mic->config.net.hostname, mic->config.net.micIP);
	}
}

#define DEFAULT_SP_IP "172.31"

void
populate_stpairip(char *ip, int miccnt)
{
	int ip1[4];
	int i;

	if (!mpssnet_decode_ipaddr(ip, ip1, 4)) {
		if (miccnt == 1)
			return;
		display(PERROR, "The single IP %s to expand must be the starting 2 quads\n", ip);
		EXIT_ERROR(EINVAL);
	}

	if (!mpssnet_decode_ipaddr(ip, ip1, 3) || mpssnet_decode_ipaddr(ip, ip1, 2)) {
		display(PERROR, "The single IP %s to expand must be the starting 2 quads\n", ip);
		EXIT_ERROR(EINVAL);
	}

	for (i = 0; i < miccnt; i++) {
		if ((ips[i][0] = (char *) malloc(16)) != NULL)
			sprintf(ips[i][0], "%d.%d.%d.1", ip1[0], ip1[1], i + 1);
		if ((ips[i][1] = (char *) malloc(16)) != NULL)
			sprintf(ips[i][1], "%d.%d.%d.254", ip1[0], ip1[1], i + 1);
	}
}

void
fill_singles(int cnt)
{
	int ip1[4];
	int ip2[4];
	int i;

	if (cnt > 255)
		return;

	if ((cnt == 1) && mpssnet_decode_ipaddr(ips[0][0], ip1, 4)) {
		if (mpssnet_decode_ipaddr(ips[0][0], ip1, 2)) {
			display(PERROR, "1 Invalid IP %s\n", ips[0][0]);
			EXIT_ERROR(EINVAL);
		}

		if ((ips[0][0] = (char *) malloc(16)) != NULL)
			sprintf(ips[0][0], "%d.%d.1.1", ip1[0], ip1[1]);
		if ((ips[0][1] = (char *) malloc(16)) != NULL)
			sprintf(ips[0][1], "%d.%d.1.254", ip1[0], ip1[1]);
		return;
	}

	for (i = 0; i < cnt; i++) {
		if (mpssnet_decode_ipaddr(ips[i][0], ip1, 4)) {
			if (mpssnet_decode_ipaddr(ips[i][0], ip1, 3)) {
				display(PERROR, "2 Invalid IP %s\n", ips[i][0]);
				EXIT_ERROR(EINVAL);
			}

			if ((ips[i][0] = (char *) malloc(16)) != NULL)
				sprintf(ips[i][0], "%d.%d.%d.1", ip1[0], ip1[1], ip1[2]);
			if ((ips[i][1] = (char *) malloc(16)) != NULL)
				sprintf(ips[i][1], "%d.%d.%d.254", ip1[0], ip1[1], ip1[2]);
		} else {
			if (ips[i][1] == NULL) {
				display(PERROR, "IP %s does not have a pair\n", ips[i][0]);
				EXIT_ERROR(EINVAL);
			}

			if (mpssnet_decode_ipaddr(ips[i][1], ip2, 4)) {
				display(PERROR, "IP %s not valid\n", ips[i][1]);
				EXIT_ERROR(EINVAL);
			}

			if ((ip1[0] != ip2[0]) || (ip1[1] != ip2[1]) ||
			    (ip1[2] != ip2[2]) || (ip1[3] == ip2[3])) {
				display(PERROR, "Bad IP pair %s and %s\n", ips[i][0], ips[i][1]);
				EXIT_ERROR(EINVAL);
			}
		}
	}
}

void
ubuntu_host_addif(char *name, char *ip, char *netbits, char *mtu, char *mac)
{
	char filename[PATH_MAX];
	char netmask[32];
	FILE *fp;

	snprintf(filename, PATH_MAX, "%s/interfaces", ni[mpssenv.dist].netdir);
	if ((fp = fopen(filename, "a")) == NULL) {
		display(PERROR, "%s: Error opening %s: %s\n", name, filename, strerror(errno));
		return;
	}

	mpssnet_genmask(netbits, netmask);
	fprintf(fp, "# %s BEGIN\n", name);
	fprintf(fp, "auto %s\n", name);
	fprintf(fp, "iface %s inet static\n", name);
	fprintf(fp, "\taddress %s\n", ip);
	fprintf(fp, "\tnetmask %s\n", netmask);

	if (mtu)
		fprintf(fp, "\tmtu %s\n", mtu);

	fprintf(fp, "# %s END\n", name);
	fclose(fp);
	display(PFS, "%s: Created %s\n", name, filename);

	do_ifup(name, name, FALSE);
}

void
common_host_addif(char *name, char *ip, char *netbits, char *mtu, char *mac)
{
	char filename[PATH_MAX];
	char netmask[32];
	FILE *fp;
	char *header;

	if (mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s/ifcfg-%s", ni[mpssenv.dist].netdir, name)) {
		unlink(filename);
		header = "Updated";
	} else {
		header = "Created";
	}

	if ((fp = fopen(filename, "w")) == NULL) {
		display(PERROR, "%s: Error opening %s: %s\n", name, filename, strerror(errno));
		return;
	}

	mpssnet_genmask(netbits, netmask);
	fprintf(fp, "DEVICE=\"%s\"\n", name);
	fprintf(fp, "TYPE=Ethernet\nONBOOT=yes\nNM_CONTROLLED=\"no\"\n");
	fprintf(fp, "BOOTPROTO=static\n");
	fprintf(fp, "IPADDR=%s\n", ip);
	fprintf(fp, "NETMASK=%s\n", netmask);

	if (mtu != NULL)
		fprintf(fp, "MTU=%s\n", mtu);

	if (mac > MIC_MAC_RANDOM) {
		fprintf(fp, "MACADDR=%s\n", mac);
	}

	if (mpssenv.dist == DISTRIB_SUSE)
		fprintf(fp, "STARTMODE=auto\n");

	fclose(fp);
	display(PFS, "%s: %s %s\n", name, header, filename);

	do_ifup(name, name, TRUE);
}

int
update_bridge(struct mic_info *mic, int offset, int miccnt, char *ip, char *gw, char *netbits,
	      char *mtu, struct mic_info *miclist, int mhost, char *mcard)
{
	struct mbridge *br;
	int update = FALSE;
	int save_gateway = TRUE;
	char *ipbuf;
	int ip_cnt;
	int err;

	if (netbits != NULL) {
		display(PWARN, "%s: Ignoring netbits parameter, use --modbridge\n", mic->name);
	}

	if (mtu != NULL) {
		display(PWARN, "%s: Ignoring mtu parameter, use --modbridge\n", mic->name);
	}

	if ((br = mpss_bridge_byname(mic->config.net.bridge, brlist)) == NULL) {
		display(PWARN, "%s: Cannot find bridge %s\n", mic->name, mic->config.net.bridge);
		return 1;
	}

	if (ip != NULL) {
		ip_cnt = get_ips(ip, &ipbuf);

		if ((err = check_for_doubles(ip_cnt)) > 0) {
			display(PERROR, "IP %d specifies more than one IP address\n", err);
			return 1;
		}

		if (miccnt != ip_cnt) {
			if (ip_cnt != 1) {
				display(PERROR, "IP %d count does not match MIC list\n", ip_cnt);
				return 1;
			}

			populate_bridgeip(ip, miccnt);
			ip_cnt = miccnt;
		}

		verify_ips(br, ip_cnt, 1);

		if (strcmp(mic->config.net.micIP, ips[offset][0]) ) {
			mpssut_realloc_fill(&mic->config.net.micIP, ips[offset][0]);
			update = TRUE;
		}

		free(ipbuf);
	}

	if (mic->config.net.modhost != mhost) {
		mic->config.net.modhost = mhost;
		update = TRUE;
	}

	if (mic->config.net.modcard != mcard) {
		mic->config.net.modcard = mcard;
		update = TRUE;
	}

	if ((gw != NULL ) &&
	    ((mic->config.net.gateway == NULL) || !strcmp(mic->config.net.gateway, gw))) {
		mpssut_realloc_fill(&mic->config.net.gateway, gw);
		update = TRUE;
	} else {
		save_gateway = FALSE;
		get_gateway(mic);
	}

	if (update) {
		rem_from_hosts(mic->name, mic->name);
		mic->config.net.mtu = br->mtu;
		gen_ipinfo(mic, 1);
		display(PFS, "%s: Update Network in %s\n", mic->name, mic->config.name);

		if (save_gateway)
			mpss_update_config(&mpssenv, mic->config.name, "Network", NULL,
				"Network class=StaticBridge bridge=%s micip=%s "
				"modhost=%s modcard=%s gw=%s\n",
				mic->config.net.bridge, mic->config.net.micIP,
				mic->config.net.modhost? "yes" : "no",
				mic->config.net.modcard,
				mic->config.net.gateway);
		else
			mpss_update_config(&mpssenv, mic->config.name, "Network", NULL,
				"Network class=StaticBridge bridge=%s micip=%s "
				"modhost=%s modcard=%s\n",
				mic->config.net.bridge, mic->config.net.micIP,
				mic->config.net.modhost? "yes" : "no",
				mic->config.net.modcard);

		if (mic->config.net.modhost && mic->config.net.micIP && mpssenv.live_update) {
			add_to_hosts(mic->name, mic->config.net.hostname, mic->config.net.micIP);
		}
	} else {
		display(PNORM,"%s: Network configuration does not change\n", mic->name);
	}

	return 0;
}

int mic_add_static(struct mic_info *mic, int offset, char *type, char *bridge, char *ip, char *gw, char *netbits,
		   char *mtu, int mhost, char *mcard, struct mic_info *miclist, int miccnt);

void
add_static(char *type, char *bridge, char *ip, char *gw, char *netbits, char *mtu,
	   char *modhost, char *modcard, struct mic_info *miclist, int miccnt)
{
	struct mic_info *mic;
	char *ipbuf = NULL;
	int ip_cnt;
	int fail = 0;
	int bits;
	int offset = 0;
	int mhost = NOTSET;
	char *mcard = NULL;

	if (modhost != NULL) {
		if (!strcmp(modhost, "yes")) {
			mhost = TRUE;
		} else if (!strcmp(modhost, "no")) {
			mhost = FALSE;
		} else {
			display(PERROR, "modhost value must be 'yes' or 'no'\n");
			EXIT_ERROR(EINVAL);
		}
	}

	if (modcard != NULL) {
			mcard = modcard;
	}

	if (bridge != NULL) {
		if (check_for_brctl()) {
			display(PERROR, "%s: Failed - required brctl command not installed\n", bridge);
			EXIT_ERROR(ESRCH);
		}

		add_if_to_bridge(bridge, ip, gw, netbits, mtu, mhost, mcard, miclist, miccnt);
		return;
	}

	if (netbits != NULL) {
		bits = atoi(netbits);
		if ((bits > 24) || (bits < 8)) {
			display(PERROR, "netbits must be between 8 and 24\n");
			EXIT_ERROR(EINVAL);
		}
	}

	if (ip != NULL) {
		ip_cnt = get_ips(ip, &ipbuf);

		if (miccnt != ip_cnt) {
			if (ip_cnt != 1) {
				display(PERROR, "IP %d count does not match MIC list\n", ip_cnt);
				EXIT_ERROR(ESRCH);
			}

			populate_stpairip(ips[0][0], miccnt);
			ip_cnt = miccnt;
		}

		fill_singles(ip_cnt);
		verify_ips(NULL, ip_cnt, 0);
	}

	for (mic = miclist, offset = 0; mic != NULL; mic = mic->next, offset++)
		fail += mic_add_static(mic, offset, type, bridge, ip, gw, netbits, mtu, mhost, mcard, miclist, miccnt);

	if (ipbuf) free(ipbuf);
	gen_etchosts(miclist);
	exit(fail);
}

int
mic_add_static(struct mic_info *mic, int offset, char *type, char *bridge, char *ip, char *gw, char *netbits, char *mtu,
	   int mhost, char *mcard, struct mic_info *miclist, int miccnt)
{
	char string1[256];
	char string2[256];
	int update = 0;

	if (mic->config.valid == NOTSET) {
		display(PWARN, "%s: Skipping -- MIC card not configured\n", mic->name);
		return 1;
	}

	if ((type == NULL) && (mic->config.net.type > NETWORK_TYPE_STATPAIR))
		return update_bridge(mic, offset, miccnt, ip, gw, netbits, mtu, miclist, mhost, mcard);

	snprintf(string1, 256, "%s: Changing", mic->name);
	if (ip != NULL) {
		if ((mic->config.net.micIP != NULL) && !strcmp(mic->config.net.micIP, ips[offset][0]))
			display(PINFO, "%s: MIC IP %s does not change\n", mic->name, mic->config.net.micIP);
		else
			mpssut_realloc_fill(&mic->config.net.micIP, ips[offset][0]);

		if ((mic->config.net.hostIP != NULL) && !strcmp(mic->config.net.hostIP, ips[offset][1]))
			display(PINFO, "%s: MIC IP %s does not change\n", mic->name, mic->config.net.hostIP);
		else
			mpssut_realloc_fill(&mic->config.net.hostIP, ips[offset][1]);

		if ((mic->config.net.micIP == NULL) || (mic->config.net.hostIP == NULL))
			return 1;

		mic->config.net.type = NETWORK_TYPE_STATPAIR;
		update = 1;
		snprintf(string2, sizeof(string2), "%s to static pair %s : %s", string1,
			 mic->config.net.micIP, mic->config.net.hostIP);
			strncpy(string1, string2, sizeof(string1));

	} else if (mic->config.net.type != NETWORK_TYPE_STATPAIR) {
		display(PERROR, "%s: Use --modbridge to change settings\n",
			mic->name);
		return 1;
	}

	if (mic->config.net.prefix == NULL) {
		if (netbits == NULL)
			netbits = "24";

		mpssut_realloc_fill(&mic->config.net.prefix, netbits);
		snprintf(string2, sizeof(string2), "%s new-prefix:%s", string1, netbits);
		update = 1;
	} else if (netbits && strcmp(netbits, mic->config.net.prefix)) {
		mpssut_realloc_fill(&mic->config.net.prefix, netbits);
		snprintf(string2, sizeof(string2), "%s update-prefix:%s", string1, netbits);
		update = 1;
	} else {
		netbits = mic->config.net.prefix;
		snprintf(string2, sizeof(string2), "%s prefix:%s", string1, netbits);
	}

	strncpy(string1, string2, sizeof(string1));

	if (mic->config.net.mtu == NULL) {
		if (mtu == NULL)
			mtu = MIC_DEFAULT_BIG_MTU;

		mpssut_realloc_fill(&mic->config.net.mtu, mtu);
		snprintf(string2, sizeof(string2), "%s new-mtu:%s", string1, mic->config.net.mtu);
		update = 1;
	} else if (mtu && strcmp(mtu, mic->config.net.mtu)) {
		mpssut_realloc_fill(&mic->config.net.mtu, mtu);
		snprintf(string2, sizeof(string2), "%s update-mtu:%s", string1, mic->config.net.mtu);
		update = 1;
	} else {
		mtu = mic->config.net.mtu;
		snprintf(string2, sizeof(string2), "%s mtu:%s", string1, mic->config.net.mtu);
	}

	strncpy(string1, string2, sizeof(string1));

	if (mic->config.net.modhost == NOTSET) {
		if (mhost == NOTSET)
			mhost = TRUE;

		snprintf(string2, sizeof(string2), "%s new-modhost:%s", string1, mhost? "yes" : "no");
		update = 1;
	} else if ((mhost != NOTSET) && (mic->config.net.modhost != mhost)) {
		snprintf(string2, sizeof(string2), "%s modif-modhost:%s", string1, mhost? "yes" : "no");
		update = 1;
	} else {
		mhost = mic->config.net.modhost;
		snprintf(string2, sizeof(string2), "%s modhost:%s", string1, mhost? "yes" : "no");
	}

	strncpy(string1, string2, sizeof(string2));

	if (mic->config.net.modcard == NULL) {
		if (mcard == NULL)
			mcard = "yes";

		snprintf(string2, sizeof(string2), "%s new-modcard:%s", string1, mcard);
		update = 1;
	} else if ((mcard != NULL) && (mic->config.net.modcard != mcard)) {
		snprintf(string2, sizeof(string2), "%s modify-modcard:%s", string1, mcard);
		update = 1;
	} else {
		mcard = mic->config.net.modcard;
		snprintf(string2, sizeof(string2), "%s modcard:%s", string1, mcard);
	}

	strncpy(string1, string2, sizeof(string2));

	if (update) {
		display(PNORM, "%s\n", string1);
		ni[mpssenv.dist].net_remove(mic->name, mic->config.net.bridge, mic->config.net.modhost);
		mic->config.net.type = NETWORK_TYPE_STATPAIR;
		mic->config.net.gateway = mic->config.net.hostIP;

		ni[mpssenv.dist].host_addif(mic->name, mic->config.net.hostIP, netbits, mic->config.net.mtu,
			  mic->config.net.hostMac);
		gen_ipinfo(mic, 1);

		display(PFS, "%s: Update Network in %s\n", mic->name, mic->config.name);
		mpss_update_config(&mpssenv, mic->config.name, "Network", NULL,
			"Network class=StaticPair micip=%s hostip=%s modhost=%s  modcard=%s netbits=%s mtu=%s\n",
			mic->config.net.micIP, mic->config.net.hostIP,
			mhost? "yes" : "no", mcard? mcard : "yes", netbits, mtu);

		if (mhost && mic->config.net.micIP && mpssenv.live_update) {
			add_to_hosts(mic->name, mic->config.net.hostname, mic->config.net.micIP);
		}
	} else {
		display(PNORM, "%s: Network configuration does not change\n", mic->name);
	}

	return 0;
}

void
config_net(char *type, char *bridge, char *ip, char *gw, char *netbits, char *mtu,
	   char *modhost, char *modcard, struct mic_info *miclist, int miccnt)
{
	struct mic_info *mic;

	for (mic = miclist; mic != NULL; mic = mic->next)
		micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO);

	if ((type == NULL) || !strcasecmp(type, "static")) {
		add_static(type, bridge, ip, gw, netbits, mtu, modhost, modcard, miclist, miccnt);
	} else if (!strcasecmp(type, "default")) {
		add_static(type, NULL, DEFAULT_SP_IP, NULL, "24", "64512", "yes", "yes", miclist, miccnt);
	} else if (!strcasecmp(type, "dhcp")) {
		add_dhcp(bridge, ip, netbits, mtu, miclist);
	} else {
		display(PERROR, "Network type '%s' must be 'static' or 'dhcp'\n", type);
		EXIT_ERROR(EINVAL);
	}

	exit(0);
}

void
show_bridge_list(struct mic_info *miclist)
{
	struct mic_info *mic;
	struct mbridge *br;
	char micmac[32];
	char hostmac[32];

	for (mic = miclist; mic != NULL; mic = mic->next)
		micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO);

	br = brlist;

	if (br == NULL) {
		printf("\nNo Bridges configured\n\n");
		exit(0);
	}

	putchar('\n');
	while (br != NULL) {
		if (br->type == BRIDGE_TYPE_EXT)
			printf("%s:\tExternal\n", br->name);
		else
			printf("%s:\t%s %s\n", br->name, mpssnet_brtypes(br->type), br->ip);

		br = br->next;
	}

	putchar('\n');
	for (mic = miclist; mic != NULL; mic = mic->next) {
		micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO);
		get_mac_strings(mic, micmac, hostmac, 32);

		switch(mic->config.net.type) {
		case NETWORK_TYPE_STATPAIR:
			printf("%s: StaticPair\n", mic->name);
			printf("    Mic:  %s %s\n", micmac, mic->config.net.micIP);
			printf("    Host: %s %s\n", hostmac, mic->config.net.hostIP);
			break;
		case NETWORK_TYPE_STATBRIDGE:
			printf("%s: StaticBridge=%s\n", mic->name, mic->config.net.bridge);
			printf("    IP:       %s\n", mic->config.net.micIP);
			printf("    Mic MAC:  %s\n", micmac);
			printf("    Host MAC: %s\n", hostmac);
			break;
		case NETWORK_TYPE_BRIDGE:
			printf("%s: Bridge %s with DHCP\n", mic->name, mic->config.net.bridge);
			break;
		default:
			printf("%s: Unknown network configuration type\n", mic->name);
		}
	}

	exit(0);
}

#define MIC_DEFAULT_BIG_MTU	"64512";
#define MIC_DEFAULT_SMALL_MTU	"1500";

char *br_tnames[] = {"None", "Internal", "External", "External"};

void
add_bridge(char *name, char *type, char *ip, char *netbits, char *mtu, struct mic_info *miclist)
{
	struct mic_info *mic;
	struct mbridge *br;
	struct mbridge *br2;
	int br_type;
	char *dname = NULL;
	int ip1[4];
	int bits;

	if (check_for_brctl()) {
		display(PERROR, "%s: Failed - required brctl command not installed\n", name);
		EXIT_ERROR(ESRCH);
	}

	if (type == NULL) {
		display(PINFO, "%s: Bridge default type Internal\n", name);
		type = "internal";
	}

	if (ip == NULL) {
		display(PERROR, "Must specify ip address\n");
		EXIT_ERROR(EINVAL);
	}

	if (netbits == NULL) {
		display(PINFO, "%s: Bridge default netbits 24\n", name);
		netbits = "24";
	} else {
		bits = atoi(netbits);
		if ((bits > 30) || (bits < 8)) {
			display(PWARN, "Netbits value %s is invalid - using default value 24\n",
				netbits);
			netbits = "24";
		}
	}

	if (!strcasecmp(type, "internal")) {
		if (mtu != NULL) {
			// TODO: Verify MTU size valid
		} else {
			mtu = MIC_DEFAULT_BIG_MTU;
			display(PINFO, "%s: Bridge default mtu %s\n", name, mtu);
		}

		if (!strcasecmp(ip, "dhcp")) {
			display(PERROR, "DHCP is an invalid IP address for an internal bridge\n");
			EXIT_ERROR(EINVAL);
		}

		if (!mpssnet_decode_ipaddr(ip, ip1, 4)) {
			br_type = BRIDGE_TYPE_INT;
		} else {
			display(PERROR, "Invalid IP address: %s\n", ip);
			EXIT_ERROR(EINVAL);
		}
	} else if (!strcasecmp(type, "external")) {
		if (mtu != NULL) {
			// TODO: Verify MTU size valid
		} else {
			mtu = MIC_DEFAULT_SMALL_MTU;
			display(PINFO, "%s: Bridge default mtu %s\n", name, mtu);
		}

		if (!strcmp(ip, "dhcp")) {
			br_type = BRIDGE_TYPE_EXT;
		} else if (!mpssnet_decode_ipaddr(ip, ip1, 4)) {
			br_type = BRIDGE_TYPE_STATICEXT;
		} else {
			display(PERROR, "IP %s not valid\n", ip);
			EXIT_ERROR(EINVAL);
		}
	} else {
		display(PERROR, "Unknown bridge type: %s\n", type);
		EXIT_ERROR(EINVAL);
	}

	for (mic = miclist; mic != NULL; mic = mic->next) {
		micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO);
		if (dname == NULL) dname = mic->config.dname;
	}

	if (dname == NULL) {
		display(PERROR, "No coprocessor card found nor specified in the command line\n");
		exit(1);
	}

	if ((br = mpss_bridge_byname(name, brlist)) != NULL) {
		display(PERROR, "Bridge %s already exists\n\n", name);
		exit(1);
	}

	if ((br_type != BRIDGE_TYPE_EXT) &&
	    (br2 = mpss_bridge_byname(name, brlist)) != br) {
		display(PERROR, "Bridge %s already has IP address %s\n", br2->name, ip);
		EXIT_ERROR(EBUSY);
	}

	display(PFS, "Default: Update Bridge in %s\n", dname);
	mpss_update_config(&mpssenv, dname, NULL, NULL, "%s %s %s %s %s %s\n\n", "Bridge",
			   name, br_tnames[br_type], ip, netbits, mtu);
	add_hostbridge(name, ip, netbits, mtu, br_type);
	exit(0);
}

void
mod_bridge(char *name, char *ip, char *netbits, char *mtu, struct mic_info *miclist)
{
	struct mic_info *mic;
	struct mbridge *br;
	char entry[128];
	char *dname = NULL;
	int diff = 0;
	int err;
	int bits;
	int br_type = BRIDGE_TYPE_UNKNOWN;
	int ip1[4];

	if (check_for_brctl()) {
		display(PERROR, "%s: Failed - required brctl command not installed\n", name);
		EXIT_ERROR(ESRCH);
	}

	if (name == NULL) {
		display(PERROR, "Bridge name must be specified\n");
		EXIT_ERROR(EINVAL);
	}

	for (mic = miclist; mic != NULL; mic = mic->next)
		micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO);

	if ((br = mpss_bridge_byname(name, brlist)) == NULL) {
		display(PERROR, "Bridge '%s' unknown\n", name);
		EXIT_ERROR(EINVAL);
	}

	if ((ip == NULL) && (mtu == NULL) && (netbits == NULL)) {
		display(PERROR, "Must specify at least one of ip, netbits or mtu value to modify\n");
		EXIT_ERROR(EINVAL);
	}

	if (ip != NULL) {
		switch (br->type) {
		case BRIDGE_TYPE_INT:
			if (!strcasecmp(ip, "dhcp")) {
				display(PERROR, "DHCP is invalid for IP address for an internal "
						"bridge\n");
				EXIT_ERROR(EINVAL);
			}

			if (mpssnet_decode_ipaddr(ip, ip1, 4)) {
				display(PERROR, "%s: Invalid IP address: %s\n", br->name, ip);
				EXIT_ERROR(EINVAL);
			}

			br_type = br->type;
			break;

		case BRIDGE_TYPE_EXT:
			if (strcmp(ip, "dhcp")) {
				if (!mpssnet_decode_ipaddr(ip, ip1, 4)) {
					display(PINFO, "%s: Changing bridge type to static external\n",
							br->name);
					br_type = BRIDGE_TYPE_STATICEXT;
				} else {
					display(PERROR, "%s: Invalid IP address: %s\n", br->name, ip);
					EXIT_ERROR(EINVAL);
				}
			}

			break;

		case BRIDGE_TYPE_STATICEXT:
			if (!strcmp(ip, "dhcp")) {
				display(PINFO, "%s: Changing bridge type to external\n", br->name);
				br_type = BRIDGE_TYPE_EXT;
			} else {
				if (mpssnet_decode_ipaddr(ip, ip1, 4)) {
					display(PERROR, "%s: Invalid IP address: %s\n", br->name, ip);
					EXIT_ERROR(EINVAL);
				}
			}

			break;
		}

		if (!strcmp(ip, br->ip)) {
			display(PINFO, "IP %s value does not change\n", ip);
		} else {
			display(PINFO, "default: Update bridge %s IP %s\n", br->name, ip);
			diff++;
		}

		if (ip != br->ip) {
			for (mic = miclist; mic != NULL; mic = mic->next) {
				if (dname == NULL) dname = mic->config.dname;
				if ((mic->config.net.type == NETWORK_TYPE_STATBRIDGE) ||
				    (mic->config.net.type == NETWORK_TYPE_BRIDGE)) {
					if (!strcmp(br->name, mic->config.net.bridge)) {
						display(PWARN, "%s: Ensure %s is consistent with new "
							       "IP address\n", mic->name, br->name);
					}
				}
			}
		}
	} else {
		br_type = br->type;
		ip = br->ip;
	}

	if (netbits != NULL) {
		bits = atoi(netbits);
		if ((bits > 30) || (bits < 8)) {
			display(PERROR, "Netbits value %s is invalid\n", netbits);
			EXIT_ERROR(EINVAL);
		}

		if (!strcmp(netbits, br->prefix)) {
			display(PINFO, "Netbits value does not change\n");
		} else {
			display(PINFO, "default: Update bridge %s netbit %s\n", br->name, netbits);
			diff++;
		}
	} else {
		netbits = br->prefix;
	}

	if (mtu != NULL) {
		if (br->mtu && !strcmp(mtu, br->mtu)) {
			display(PINFO, "MTU value does not change\n");
		} else {
			display(PINFO, "default: Update bridge %s MTU %s\n", br->name, mtu);
			diff++;
		}
	} else {
		mtu = br->mtu;
	}

	if (diff == 0) {
		display(PNORM, "Neither ip, netbits nor mtu are changed\n");
		exit(0);
	}

	do_ifdown(name, name, FALSE);
	snprintf(entry, 128, "Bridge %s", name);

	if (mtu != NULL) {
		if ((err = mpss_update_config(&mpssenv, dname, entry, NULL, "Bridge %s %s %s %s %s\n", name,
				(br_type == BRIDGE_TYPE_INT)? "Internal" : "External", ip, netbits, mtu)))
			display(PERROR, "Failed to open config file '%s': %d\n", mic->config.name, err);
		else
			display(PFS, "Default: Update Bridge in %s\n", dname);
	} else {
		if ((err = mpss_update_config(&mpssenv, dname, entry, NULL, "Bridge %s %s %s %s\n", name,
				(br_type == BRIDGE_TYPE_INT)? "Internal" : "External", ip, netbits)))
			display(PERROR, "Failed to open config file '%s': %d\n", mic->config.name, err);
		else
			display(PFS, "Default: Update Bridge in %s\n", dname);
	}

	update_hostbridge(name, ip, netbits, mtu);

	for (mic = miclist; mic != NULL; mic = mic->next) {
		if (((mic->config.net.type != NETWORK_TYPE_STATBRIDGE) &&
		     (mic->config.net.type != NETWORK_TYPE_BRIDGE)) ||
		    strcmp(mic->config.net.bridge, br->name))
			continue;

		ni[mpssenv.dist].attach_hostbridge(mic->name, name, mic->config.net.hostMac, mtu, TRUE);
		mic->config.net.prefix = netbits;
		mic->config.net.mtu = mtu;
		gen_ipinfo(mic, 1);
	}

	exit(0);
}

int
add_hostbridge(char *name, char *ip, char *netbits, char *mtu, int type)
{
	char filename[PATH_MAX];
	char mask[32];
	FILE *fp;

	mpssnet_genmask(netbits, mask);

	if (mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s/ifcfg-%s", ni[mpssenv.dist].netdir, name)) {
		if (type != BRIDGE_TYPE_INT) {
			if (mtu != NULL) {
				display(PINFO, "default: Bridge %s external IP %s netbits %s netmask %s mtu %s\n",
					name, ip, netbits, mask, mtu);
			} else {
				display(PINFO, "default: Bridge %s external IP %s netbits %s netmask %s\n",
					name, ip, netbits, mask);
			}
			display(PWARN, "default: External bridge %s already configured - "
				       "ensure configuration matches\n", name);
			return 0;
		}

		display(PERROR, "default: Internal bridge %s OS configuration file %s exists\n", name, filename);
		display(PERROR, "default: Remove and rerun this command\n");
		return 0;
	}

	if ((fp = fopen(filename, "w")) == NULL)
		return 0;

	fprintf(fp, "DEVICE=%s\n", name);
	fprintf(fp, "TYPE=Bridge\nONBOOT=yes\nDELAY=0\nNM_CONTROLLED=\"no\"\n");

	if (!strcmp(ip, "dhcp")) {
		fprintf(fp, "BOOTPROTO=dhcp\n");
	} else {
		fprintf(fp, "BOOTPROTO=static\n");
		fprintf(fp, "IPADDR=%s\n", ip);
	}

	fprintf(fp, "NETMASK=%s\n", mask);

	if (mpssenv.dist == DISTRIB_SUSE) {
		fprintf(fp, "BRIDGE=yes\n");
		fprintf(fp, "STARTMODE=auto\n");
		fprintf(fp, "BRIDGE_PORTS=''\n");
	}

	if (mtu != NULL)
		fprintf(fp, "MTU=%s\n", mtu);

	fclose(fp);
	display(PFS, "default: Create %s\n", filename);
	do_ifup(name, name, TRUE);
	if (mtu != NULL)
		display(PINFO, "default: Bridge %s internal IP %s netbits %s netmask %s mtu %s\n", name, ip, netbits, mask, mtu);
	else
		display(PINFO, "default: Bridge %s internal IP %s netbits %s netmask %s\n", name, ip, netbits, mask);
	return 0;
}

void
update_hostbridge(char *name, char *ip, char *netbits, char *mtu)
{
	char ifilename[PATH_MAX];
	char *ofilename;
	char buffer[256];
	char mask[32];
	FILE *ifp;
	FILE *ofp;

	if (!mpssut_filename(&mpssenv, NULL, ifilename, PATH_MAX, "%s/ifcfg-%s", ni[mpssenv.dist].netdir, name)) {
		display(PERROR, "%s: Cannot open %s\n", name, ifilename);
		EXIT_ERROR(ENOENT);
	}

	if ((ofilename = mpssut_tempnam(ifilename)) == NULL) {
		display(PERROR, "%s: internal tempname failure - skipping\n", name);
		EXIT_ERROR(ENOENT);
	}

	if ((ifp = fopen(ifilename, "r")) == NULL) {
		display(PERROR, "%s: Cannot open %s\n", name, ifilename);
		free(ofilename);
		EXIT_ERROR(ENOENT);
	}

	if ((ofp = fopen(ofilename, "w")) == NULL) {
		display(PERROR, "%s: Cannot open %s - %s\n", name, ofilename, strerror(errno));
		free(ofilename);
		EXIT_ERROR(ENOENT);
	}

	mpssnet_genmask(netbits, mask);

	while (fgets(buffer, 256, ifp)) {
		if (!strncmp(buffer, "IPADDR", strlen("IPADDR")))
			fprintf(ofp, "IPADDR=%s\n", ip);
		else if (!strncmp(buffer, "MTU", strlen("MTU")) && mtu)
			fprintf(ofp, "MTU=%s\n", mtu);
		else if (!strncmp(buffer, "NETMASK", strlen("NETMASK")))
			fprintf(ofp, "NETMASK=%s\n", mask);
		else
			fprintf(ofp, "%s", buffer);
	}

	fclose(ifp);
	fclose(ofp);
	if (mpssut_rename(&mpssenv, ofilename, ifilename)) {
			display(PERROR, "Failed to rename temporary file %s\n", ifilename);
			free(ofilename);
			EXIT_ERROR(EIO);
	}
	free(ofilename);
	display(PFS, "%s: Update %s\n", name, ifilename);

	if (mpssenv.dist == DISTRIB_SUSE) {
		suse_restart_bridge(name, TRUE);
	} else {
		do_ifup("default", name, TRUE);
	}
}

int
delete_config(char *name, char *param)
{
	char ifilename[PATH_MAX];
	char *ofilename;
	char buffer[256];
	int last = 0;
	FILE *icfp;
	FILE *ocfp;

	if (!mpssut_filename(&mpssenv, NULL, ifilename, PATH_MAX, "%s/%s.conf", mpssenv.confdir, name) ||
	    ((icfp = fopen(ifilename, "r")) == NULL)) {
		display(PINFO, "%s: delete config cannot open %s\n", name, ifilename);
		return 1;
	}

	if ((ofilename = mpssut_tempnam(ifilename)) == NULL) {
		display(PERROR, "%s: internal tempname failure - skipping\n", name);
		fclose(icfp);
		return 1;
	}

	if ((ocfp = fopen(ofilename, "w")) == NULL) {
		display(PERROR, "%s: delete config cannot open %s: %s\n", name, ofilename, strerror(errno));
		fclose(icfp);
		free(ofilename);
		EXIT_ERROR(EBADF);
	}

	while (fgets(buffer, 256, icfp)) {
		if (!strncmp(buffer, "\n", strlen("\n")) && (last == 1))
			last = 0;
		else if (strncmp(buffer, param, strlen(param)))
			fprintf(ocfp, "%s", buffer);
		else
			last = 1;
	}

	fclose(icfp);
	fclose(ocfp);
	if (mpssut_rename(&mpssenv, ofilename, ifilename)) {
			display(PERROR, "Failed to rename temporary file %s\n", ifilename);
			free(ofilename);
			return 1;
	}
	free(ofilename);
	return 0;
}

void
remove_old_net(struct mic_info *mic)
{
	if (delete_config("default", "# If NetworkBridgeName is not defined the use of a static pair"))
		return;

	delete_config("default", "# is used by default");

	delete_config("default", "# Bridge names starting with \"mic\" will be created by the ");
	delete_config("default", "# assumed to already exist");
	delete_config("default", "#BridgeName");
	delete_config("default", "BridgeName");

	delete_config("default", "# Define the first 2 quads of the network address");
	delete_config("default", "# Static pair configurations will fill in the second 2 quad");
	delete_config("default", "# configuration files can override the defaults with ");
	delete_config("default", "Subnet");

	delete_config(mic->name, "BridgeName");
	delete_config(mic->name, "#BridgeName");
	delete_config(mic->name, "Subnet");
	delete_config(mic->name, "MicIPaddress");
	delete_config(mic->name, "HostIPaddress");
}

void
set_bridge_config(struct mic_info *miclist)
{
	struct mbridge *br = brlist;

	while (br) {
		if (br->type == BRIDGE_TYPE_INT)
			add_hostbridge(br->name, br->ip, br->prefix, br->mtu, BRIDGE_TYPE_INT);

		do_ifup("default", br->name, TRUE);
		br = br->next;
	}
}

void
set_spnet(struct mic_info *mic, char *mip, char *hip, int modhost, char *modcard)
{
	int err;
	char network[1034];
	char tmp[1024];

	display(PINFO, "%s: Network Static Pair MIC %s Host %s\n", mic->name, mip, hip);
	mic->config.net.type = NETWORK_TYPE_STATPAIR;

	mic->config.net.micIP = mpssut_alloc_fill(mip);
	mic->config.net.hostIP = mpssut_alloc_fill(hip);
	mic->config.net.gateway = mic->config.net.hostIP;
	mic->config.net.prefix = mpssut_alloc_fill("24");

	ni[mpssenv.dist].host_addif(mic->name, hip, mic->config.net.prefix, mic->config.net.mtu, mic->config.net.hostMac);
	gen_ipinfo(mic, 1);

	mic->config.net.modhost = modhost;
	mic->config.net.modcard = modcard;

	if (mic->config.net.modhost && mpssenv.live_update)
		add_to_hosts(mic->name, mic->config.net.hostname, mic->config.net.micIP);


	snprintf(tmp, sizeof(tmp), "Network class=StaticPair micip=%s hostip=%s netbits=24 "
		"modhost=%s modcard=%s", mip, hip, modhost? "yes" : "no", modcard? modcard : "yes");

	snprintf(network, sizeof(network), "%s mtu=64512\n\n", tmp);

	if ((err = mpss_update_config(&mpssenv, mic->config.name, "Network", NULL, "%s", network)))
		display(PERROR, "Failed to open config file '%s': %d\n", mic->config.name, err);
	else
		display(PFS, "%s: Update Network in %s\n", mic->name, mic->config.name);
}

void
set_net_config(struct mic_info *mic, struct mic_info *miclist, int modhost, char *modcard)
{
	struct mbridge *br;
	char ip[2][16];

	if (modhost != NOTSET)
		mic->config.net.modhost = modhost;

	if (modcard != NULL)
		mic->config.net.modcard = modcard;

	switch (mic->config.net.type) {
	case NETWORK_TYPE_STATPAIR:
		ni[mpssenv.dist].net_remove(mic->name, mic->config.net.bridge, mic->config.net.modhost);
		ni[mpssenv.dist].host_addif(mic->name, mic->config.net.hostIP, mic->config.net.prefix,
			  mic->config.net.mtu, mic->config.net.hostMac);
		gen_ipinfo(mic, 1);

		if (mic->config.net.modhost && mic->config.net.micIP && mpssenv.live_update)
			add_to_hosts(mic->name, mic->config.net.hostname, mic->config.net.micIP);
		break;

	case NETWORK_TYPE_STATBRIDGE:
	case NETWORK_TYPE_BRIDGE:
		ni[mpssenv.dist].net_remove(mic->name, mic->config.net.bridge, mic->config.net.modhost);

		if ((br = mpss_bridge_byname(mic->config.net.bridge, brlist)) != NULL)
			ni[mpssenv.dist].attach_hostbridge(mic->name, mic->config.net.bridge,
						  mic->config.net.hostMac, br->mtu, FALSE);
		gen_ipinfo(mic, 1);

		if (mic->config.net.modhost && mic->config.net.micIP && mpssenv.live_update)
			add_to_hosts(mic->name, mic->config.net.hostname, mic->config.net.micIP);

		break;

	default:
		if (!mpssenv.live_update) {
			if (!mpssut_mksubtree(&mpssenv, "", ni[mpssenv.dist].netdir, 0, 0, 0755))
				display(PFS, "%s: Created directory %s\n", mic->name, ni[mpssenv.dist].netdir);
		}

		snprintf(ip[0], 16, "172.31.%d.1", mic->id + 1);
		snprintf(ip[1], 16, "172.31.%d.254", mic->id + 1);
		set_spnet(mic, ip[0], ip[1], mic->config.net.modhost, mic->config.net.modcard);
		break;

	}
}

void fstab_delusr(char *base, struct mic_info *mic);
void fstab_add(char *id, char *base, char *edir, char *mdir, char *server, char *nfs_opt);

void
mod_nfshost(struct mic_info *mic)
{
	struct mbridge *br;
	char *nfsname;
	char *usrname;
	char tmpnfs[PATH_MAX];
	char tmpusr[PATH_MAX];
	char *hostIP = NULL;

	switch (mic->config.net.type) {
	case NETWORK_TYPE_STATPAIR:
		hostIP = mic->config.net.hostIP;
		break;
	case NETWORK_TYPE_STATBRIDGE:
		if ((br = mpss_bridge_byname(mic->config.net.bridge, brlist)) == NULL)
			return;

		hostIP = br->ip;
		break;
	case NETWORK_TYPE_BRIDGE:
		return;
	}

	nfsname = strchr(mic->config.rootdev.target, ':') + 1;
	tmpnfs[sizeof(tmpnfs) - 1] = 0;
	snprintf(tmpnfs, sizeof(tmpnfs) - 1, "%s:%s", hostIP, nfsname);
	mpssut_realloc_fill(&mic->config.rootdev.target, tmpnfs);

	if (mic->config.rootdev.type == ROOT_TYPE_SPLITNFS) {
		fstab_delusr(mic->config.filesrc.mic.dir, mic);
		if ((usrname = strchr(mic->config.rootdev.nfsusr, ':') + 1) != NULL) {
			fstab_delusr(nfsname, mic);

			snprintf(tmpusr, PATH_MAX, "%s:%s", hostIP, usrname);
			mpssut_realloc_fill(&mic->config.rootdev.nfsusr, tmpusr);
			display(PFS, "%s: Update RootDevice in %s\n", mic->name, mic->config.name);
			mpss_update_config(&mpssenv, mic->config.name, "RootDevice", NULL, "RootDevice SplitNFS %s %s\n",
					 mic->config.rootdev.target, mic->config.rootdev.nfsusr);

			fstab_add(mic->name, mic->config.filesrc.mic.dir, usrname, "/usr", hostIP, NULL);
			fstab_add(mic->name, nfsname, usrname, "/usr", hostIP, NULL);
		}
	} else {
		display(PFS, "%s: Update RootDevice in %s\n", mic->name, mic->config.name);
		mpss_update_config(&mpssenv, mic->config.name, "RootDevice", NULL, "RootDevice NFS %s\n",
					mic->config.rootdev.target);
	}

}

char *loopif =
	"# /etc/network/interfaces -- configuration file for ifup(8), ifdown(8)\n\n"
	"# The loopback interface\n"
	"auto lo\n"
	"iface lo inet loopback\n\n"
	"# MIC virtual interface\n";

void
gen_ipinfo(struct mic_info *mic, int update)
{
	char filename[PATH_MAX];
	char nfsname[PATH_MAX];
	char netmask[32];
	struct mbridge *br;
	FILE *fp;
	FILE *nfsfp = NULL;
	char *header;
	char *eth_if;

	if (mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s/etc/network/interfaces",
								mic->config.filesrc.mic.dir)) {
		unlink(filename);
		header = "Update";
	} else {
		mpssut_mksubtree(&mpssenv, mic->config.filesrc.mic.dir, "/etc/network", 0, 0, 0755);
		header = "Create";
	}

	if ((fp = fopen(filename, "w")) == NULL) {
		display(PWARN, "%s: Error opening %s: %s\n", mic->name,
			filename, strerror(errno));
		return;
	}

	if ((mic->config.rootdev.type == ROOT_TYPE_NFS) ||
	    (mic->config.rootdev.type == ROOT_TYPE_SPLITNFS) ||
		(mic->config.rootdev.type == ROOT_TYPE_PFS)) {
		mpssut_filename(&mpssenv, NULL, nfsname, PATH_MAX, "%s/etc/network/interfaces",
			      strchr(mic->config.rootdev.target, ':') + 1);

		if ((nfsfp = fopen(nfsname, "w")) == NULL)
			display(PWARN, "%s: Error opening %s: %s\n", mic->name,
				nfsname, strerror(errno));
		else
			mod_nfshost(mic);

	}

	eth_if = "mic0";

	fprintf(fp, "%sauto %s\n", loopif, eth_if);
	if (nfsfp)
		fprintf(nfsfp, "%sauto %s\n", loopif, eth_if);

	switch (mic->config.net.type) {
	case NETWORK_TYPE_STATBRIDGE:
		fprintf(fp, "iface %s inet static\n", eth_if);
		fprintf(fp, "\taddress %s\n", mic->config.net.micIP);
		if ((br = mpss_bridge_byname(mic->config.net.bridge, brlist)) != NULL) {
			if (br->type == BRIDGE_TYPE_EXT)
				fprintf(fp, "\tgateway %s\n", mic->config.net.gateway);
			else
				fprintf(fp, "\tgateway %s\n", br->ip);

			mpssnet_genmask(br->prefix, netmask);
			fprintf(fp, "\tnetmask %s\n", netmask);
			if (br->mtu != NULL)
				fprintf(fp, "\tmtu %s\n", br->mtu);
			if (nfsfp) {
				fprintf(nfsfp, "iface %s inet static\n", eth_if);
				fprintf(nfsfp, "\taddress %s\n", mic->config.net.micIP);
				fprintf(nfsfp, "\tgateway %s\n", br->ip);
				mpssnet_genmask(br->prefix, netmask);
				fprintf(nfsfp, "\tnetmask %s\n", netmask);
				if (br->mtu != NULL)
					fprintf(nfsfp, "\tmtu %s\n", br->mtu);
			}
		}
		break;

	case NETWORK_TYPE_STATPAIR:
		fprintf(fp, "iface %s inet static\n", eth_if);
		fprintf(fp, "\taddress %s\n", mic->config.net.micIP);
		fprintf(fp, "\tgateway %s\n", mic->config.net.hostIP);
		if (nfsfp) {
			fprintf(nfsfp, "iface %s inet static\n", eth_if);
			fprintf(nfsfp, "\taddress %s\n", mic->config.net.micIP);
			fprintf(nfsfp, "\tgateway %s\n", mic->config.net.hostIP);
		}

		if (mic->config.net.prefix != NULL) {
			mpssnet_genmask(mic->config.net.prefix, netmask);
			fprintf(fp, "\tnetmask %s\n", netmask);
			if (nfsfp)
				fprintf(nfsfp, "\tnetmask %s\n", netmask);
		} else {
			fprintf(fp, "\tnetmask 255.255.255.0\n");
			if (nfsfp)
				fprintf(nfsfp, "\tnetmask 255.255.255.0\n");
		}

		if (mic->config.net.mtu != NULL) {
			fprintf(fp, "\tmtu %s\n", mic->config.net.mtu);
			if (nfsfp)
				fprintf(nfsfp, "\tmtu %s\n", mic->config.net.mtu);
		}
		break;

	case NETWORK_TYPE_BRIDGE:
		fprintf(fp, "iface %s inet dhcp\n", eth_if);
		if (nfsfp)
			fprintf(nfsfp, "iface %s inet dhcp\n", eth_if);
		if ((br = mpss_bridge_byname(mic->config.net.bridge, brlist)) != NULL) {
			if (br->mtu == NULL)
				br->mtu = "1500";

			fprintf(fp, "\tpre-up /bin/ip link set $IFACE mtu %s\n", br->mtu);
			fprintf(fp, "\thostname %s\n", mic->config.net.hostname);
			if (nfsfp) {
				fprintf(nfsfp, "\tpre-up /bin/ip link set $IFACE mtu %s\n", br->mtu);
				fprintf(nfsfp, "\thostname %s\n", mic->config.net.hostname);
			}
		}

		break;
	}

	if ((mic->config.net.hostMac != MIC_MAC_SERIAL) &&
	    (mic->config.net.hostMac != MIC_MAC_RANDOM)) {
		fprintf(fp, "\thwaddress ether %s\n", mic->config.net.micMac);
		if (nfsfp)
			fprintf(nfsfp, "\thwaddress ether %s\n", mic->config.net.micMac);
	}

	fclose(fp);
	display(PFS, "%s: %s %s\n", mic->name, header, filename);

	if (nfsfp) {
		fclose(nfsfp);
		display(PFS, "%s: %s %s\n", mic->name, header, nfsname);
	}
}

void
gen_broadcast(char *ipaddr, char *prefix, char *broadcast)
{
	char ipbuf[32];
	unsigned int netmask = 0xffffff00;
	unsigned char ip[4];
	unsigned int *iip = (unsigned int *)&ip[0];
	unsigned char broad[4];
	unsigned int *br = (unsigned int *)&broad[0];
	char *end;
	char *next;

	strncpy(ipbuf, ipaddr, 32);

	if (prefix)
		netmask = ~0x0 << (32 - atoi(prefix));

	end = strchr(ipbuf, '.');
	*end = '\0';
	ip[3] = atoi(ipbuf);
	next = end + 1;

	end = strchr(next, '.');
	*end = '\0';
	ip[2] = atoi(next);
	next = end + 1;

	end = strchr(next, '.');
	*end = '\0';
	ip[1] = atoi(next);
	next = end + 1;

	ip[0] = atoi(next);
	*br = *iip | ~netmask;
	sprintf(broadcast, "%d.%d.%d.%d", broad[3], broad[2], broad[1], broad[0]);
}

void
save_resolv(int save, char **tmpname)
{
	struct stat sbuf;
	char *buf;
	int len;
	int fd;

	if (!save)
		return;

	if ((*tmpname = mpssut_tempnam("/etc/resolv.conf")) == NULL)
		return;

	if (stat("/etc/resolv.conf", &sbuf) != 0)
		return;

	if ((buf = (char *) malloc(sbuf.st_size + 1)) == NULL) {
		return;
	}

	if ((fd = open("/etc/resolv.conf", O_RDONLY)) < 0) {
		free(buf);
		return;
	}

	if ((len = read(fd, buf, sbuf.st_size)) != sbuf.st_size) {
		free(buf);
		close(fd);
		return;
	}

	close(fd);

	if ((fd = open(*tmpname, O_WRONLY|O_CREAT, sbuf.st_mode & 0777)) < 0) {
		free(buf);
		return;
	}

	write(fd, buf, len);
	close(fd);
}

void
restore_resolv(int save, char *tmpname)
{
	if (!save)
		return;

	if (mpssenv.dist == DISTRIB_UBUNTU)
		return;

	if (mpssut_rename(&mpssenv, tmpname, "/etc/resolv.conf")) {
		display(PFS, "Failed to update file %s\n", "/etc/resolv.conf");
		free(tmpname);
		return;
	}

	display(PFS, "Update file %s\n", "/etc/resolv.conf");
	free(tmpname);
}

void
suse_restart_bridge(char *name, int saveresolv)
{
	pid_t pid;
	char *ifargv[4];
	char *tmpname;

	if (!mpssenv.live_update)
		return;

	save_resolv(saveresolv, &tmpname);

	pid = fork();
	if (pid == 0) {
		fclose(stdout);
		fclose(stderr);
		fopen("/dev/null", "w+");
		ifargv[0] = "/etc/rc.d/network";
		ifargv[1] = "restart";
		ifargv[2] = name;
		ifargv[3] = NULL;
		execve("/etc/rc.d/network", ifargv, NULL);
		exit(errno);
	}

	waitpid(pid, NULL, 0);
	restore_resolv(saveresolv, tmpname);
}

void
do_ifup(char *label, char *name, int saveresolv)
{
	pid_t pid;
	char *ifargv[3];
	char *tmpname;

	if (!mpssenv.live_update)
		return;

	display(PNET, "%s: ifup %s\n", label, name);
	save_resolv(saveresolv, &tmpname);
	fflush(stdout);
	fflush(stderr);

	pid = fork();
	if (pid == 0) {
		fclose(stdout);
		fclose(stderr);
		fopen("/dev/null", "w+");
		ifargv[0] = "/sbin/ifup";
		ifargv[1] = name;
		ifargv[2] = NULL;
		execve("/sbin/ifup", ifargv, NULL);
		exit(errno);
	}

	waitpid(pid, NULL, 0);
	restore_resolv(saveresolv, tmpname);
}

void
do_ifdown(char *label, char *name, int saveresolv)
{
	pid_t pid;
	char *ifargv[3];
	char *tmpname;

	if (!mpssenv.live_update)
		return;

	display(PNET, "%s: ifdown %s\n", label, name);
	save_resolv(saveresolv, &tmpname);
	fflush(stdout);
	fflush(stderr);

	pid = fork();
	if (pid == 0) {
		fclose(stdout);
		fclose(stderr);
		fopen("/dev/null", "w+");
		ifargv[0] = "/sbin/ifdown";
		ifargv[1] = name;
		ifargv[2] = NULL;
		execve("/sbin/ifdown", ifargv, NULL);
		exit(errno);
	}

	waitpid(pid, NULL, 0);
	restore_resolv(saveresolv, tmpname);
}

void
do_hwaddr(char *name, char *mac)
{
	pid_t pid;
	char *ifargv[6];

	if (!mpssenv.live_update)
		return;

	fflush(stdout);
	fflush(stderr);

	pid = fork();
	if (pid == 0) {
		fclose(stdout);
		fclose(stderr);
		fopen("/dev/null", "w+");
		ifargv[0] = "/sbin/ifconfig";
		ifargv[1] = name;
		ifargv[2] = "hw";
		ifargv[3] = "ether";
		ifargv[4] = mac;
		ifargv[5] = NULL;
		execve("/sbin/ifconfig", ifargv, NULL);
		exit(errno);
	}

	waitpid(pid, NULL, 0);
}

void
do_brctl_addif(char *label, char *bridge, char* interface)
{
	pid_t pid;
	char *ifargv[5];
	char *brctl = ni[mpssenv.dist].brctl;

	if (!mpssenv.live_update)
		return;

	display(PNET, "%s: brctl addif %s %s\n", label, bridge, interface);
	fflush(stdout);
	fflush(stderr);

	pid = fork();
	if (pid == 0) {
		fclose(stdout);
		fclose(stderr);
		ifargv[0] = brctl;
		ifargv[1] = "addif";
		ifargv[2] = bridge;
		ifargv[3] = interface;
		ifargv[4] = NULL;
		execve(brctl, ifargv, NULL);
		exit(errno);
	}

	waitpid(pid, NULL, 0);
}

void
do_brctl_delif(char *label, char *bridge, char* interface)
{
	pid_t pid;
	char *ifargv[5];
	char *brctl = ni[mpssenv.dist].brctl;

	if (!mpssenv.live_update)
		return;

	display(PNET, "%s: brctl delif %s %s\n", label, bridge, interface);
	fflush(stdout);
	fflush(stderr);

	pid = fork();
	if (pid == 0) {
		fclose(stdout);
		fclose(stderr);
		ifargv[0] = brctl;
		ifargv[1] = "delif";
		ifargv[2] = bridge;
		ifargv[3] = interface;
		ifargv[4] = NULL;
		execve(brctl, ifargv, NULL);
		exit(errno);
	}

	waitpid(pid, NULL, 0);
}

void
do_hostaddbr(char *label, char *bridge)
{
	pid_t pid;
	char *ifargv[4];
	char *brctl = ni[mpssenv.dist].brctl;

	if (!mpssenv.live_update)
		return;

	display(PNET, "%s: brctl addbr %s\n", label, bridge);
	fflush(stdout);
	fflush(stderr);

	pid = fork();
	if (pid == 0) {
		fclose(stdout);
		fclose(stderr);
		ifargv[0] = brctl;
		ifargv[1] = "addbr";
		ifargv[2] = bridge;
		ifargv[3] = NULL;
		execve(brctl, ifargv, NULL);
		exit(errno);
	}

	waitpid(pid, NULL, 0);
}

void
do_hostdelbr(char *label, char *bridge)
{
	pid_t pid;
	char *ifargv[4];
	char *brctl = ni[mpssenv.dist].brctl;

	if (!mpssenv.live_update)
		return;

	display(PNET, "%s: brctl delbr %s\n", label, bridge);
	fflush(stdout);
	fflush(stderr);

	pid = fork();
	if (pid == 0) {
		fclose(stdout);
		fclose(stderr);
		ifargv[0] = brctl;
		ifargv[1] = "delbr";
		ifargv[2] = bridge;
		ifargv[3] = NULL;
		execve(brctl, ifargv, NULL);
		exit(errno);
	}

	waitpid(pid, NULL, 0);
}

void
del_bridge(char *bridge, struct mic_info *miclist)
{
	struct mic_info *mic;
	struct mbridge *br;
	char config[64];
	int fail = 0;

	if (check_for_brctl()) {
		display(PERROR, "%s: Failed - required brctl command not installed\n", bridge);
		EXIT_ERROR(ESRCH);
	}

	for (mic = miclist; mic != NULL; mic = mic->next)
		micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO);

	if ((br = mpss_bridge_byname(bridge, brlist)) == NULL) {
		display(PERROR, "default: Bridge '%s' not in configuration\n", bridge);
		exit(EEXIST);
	}

	for (mic = miclist; mic != NULL; mic = mic->next) {
		if (micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO))
			continue;

		if ((mic->config.net.type == NETWORK_TYPE_STATBRIDGE) ||
		    (mic->config.net.type == NETWORK_TYPE_BRIDGE)) {
			if (!strcmp(bridge, mic->config.net.bridge)) {
				display(PERROR, "%s: attached to '%s'\n", mic->name, bridge);
				fail++;
			}
		}
	}

	if (fail != 0)
		exit(fail);

	ni[mpssenv.dist].br_remove(br);
	display(PNORM, "default: Bridge %s removed\n", bridge);
	snprintf(config, 63, "Bridge %s", bridge);
	delete_config("default", config);
	exit(0);
}

void
remove_network(struct mic_info *mic)
{
	ni[mpssenv.dist].net_remove(mic->name, mic->config.net.bridge, mic->config.net.modhost);
}

void
setserialmac(struct mic_info *miclist)
{
	struct mic_info *mic;
	struct mbridge *br;
	unsigned char hmac[6];
	char micmac[32];
	char hostmac[32];
	char *serial;
	int fail = 0;

	for (mic = miclist; mic != NULL; mic = mic->next) {
		if (micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO)) {
			display(PWARN, "%s: Not configured - skipping\n", mic->name);
			continue;
		}

		if (mic->config.net.hostMac == MIC_MAC_SERIAL) {
			display(PINFO, "%s: Mac address already from serial number\n", mic->name);
			continue;
		}

		mic->config.net.hostMac = MIC_MAC_SERIAL;
		serial = mpss_readsysfs(mic->name, "serialnumber");

		if (mic_get_mac_from_serial(serial, hmac, 0)) {
			display(PERROR, "%s: Invalid serial number '%s'\n", mic->name, serial);
			fail++;
			continue;
		}
		free(serial);

		gen_ipinfo(mic, 1);
		do_ifdown(mic->name, mic->name, FALSE);

		if (mic->config.net.type == NETWORK_TYPE_STATPAIR) {
			ni[mpssenv.dist].host_addif(mic->name, mic->config.net.hostIP, mic->config.net.prefix,
				  mic->config.net.mtu, mic->config.net.hostMac);
		} else {
			if ((br = mpss_bridge_byname(mic->config.net.bridge, brlist)) != NULL) {
				ni[mpssenv.dist].attach_hostbridge(mic->name, mic->config.net.bridge,
						  mic->config.net.hostMac, br->mtu, FALSE);
			}
		}

		get_mac_strings(mic, micmac, hostmac, 32);
		do_hwaddr(mic->name, hostmac);
		display(PFS, "%s: Update MacAddrs in %s\n", mic->name, mic->config.name);
		mpss_update_config(&mpssenv, mic->config.name, "MacAddrs", NULL, "MacAddrs Serial\n");
	}

	exit(fail);
}

void
setrandommac(struct mic_info *miclist)
{
	struct mic_info *mic;
	struct mbridge *br;
	int fail = 0;
	int err;

	for (mic = miclist; mic != NULL; mic = mic->next) {
		if (micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO)) {
			display(PWARN, "%s: Not configured - skipping\n", mic->name);
			continue;
		}

		if (mic->config.net.hostMac == MIC_MAC_RANDOM) {
			display(PINFO, "%s: Mac address already random\n", mic->name);
			continue;
		}

		mic->config.net.hostMac = MIC_MAC_RANDOM;
		gen_ipinfo(mic, 1);

		do_ifdown(mic->name, mic->name, FALSE);

		if (mic->config.net.type == NETWORK_TYPE_STATPAIR) {
			ni[mpssenv.dist].host_addif(mic->name, mic->config.net.hostIP, mic->config.net.prefix,
				  mic->config.net.mtu, mic->config.net.hostMac);
		} else {
			if ((br = mpss_bridge_byname(mic->config.net.bridge, brlist)) != NULL) {
				ni[mpssenv.dist].attach_hostbridge(mic->name, mic->config.net.bridge,
						  mic->config.net.hostMac, br->mtu, FALSE);
			}
		}

		if ((err = mpss_update_config(&mpssenv, mic->config.name, "MacAddrs", NULL, "MacAddrs Random\n"))) {
			display(PERROR, "Failed to open config file '%s': %d\n", mic->config.name, err);
			fail++;
		} else {
			display(PFS, "%s: Update MacAddrs in %s\n", mic->name, mic->config.name);
		}
	}

	exit(fail);
}

int
verify_mac(char *mac)
{
	int i;
	char firststring[3];
	int first;

	if (strlen(mac) != 17)
		return 1;

	strncpy(firststring, mac, 2);
	firststring[2] = '\0';
	first = strtoul(firststring, NULL, 16);

	if (first & 0x1)
		return 2;

	if (!(first & 0x2) && first != 0x4c)
		return 3;

	for (i = 0; i < 17; i++) {
		if (!((i + 1) % 3)) {
			if (mac[i] != ':')
				return 1;
		} else {
			if (!isxdigit(mac[i]))
				return 1;
		}
	}

	return 0;
}

void
show_macs(struct mic_info *miclist)
{
	struct mic_info *mic;
	char micmac[32];
	char hostmac[32];

	putchar('\n');
	for (mic = miclist; mic != NULL; mic = mic->next) {
		if (micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO)) {
			printf("%s: Not configured - skipping\n", mic->name);
			continue;
		}

		get_mac_strings(mic, micmac, hostmac, 32);

		printf("%s: Host %s Mic %s\n", mic->name, micmac, hostmac);
	}

	putchar('\n');
	exit(0);
}

void
setmac(char *micmac, char *hostmac, struct mic_info *miclist)
{
	struct mic_info *mic;
	struct mbridge *br;
	char mictmpmac[32];
	char hosttmpmac[32];
	char *oldhostmac;
	unsigned int last;
	unsigned int offset = 0;
	int err;

	if (micmac == NULL)
		show_macs(miclist);

	if (!strcasecmp(micmac, "serial")) {
		setserialmac(miclist);
	}

	if (!strcasecmp(micmac, "random")) {
		setrandommac(miclist);
	}

	if ((err = verify_mac(micmac)) == 1) {
		display(PERROR, "Mic MAC address '%s' invalid format\n", micmac);
		EXIT_ERROR(EINVAL);
	}

	if (err == 2) {
		display(PERROR, "Multicast not allowed\n");
		EXIT_ERROR(EINVAL);
	}

	if (err == 3) {
		display(PERROR, "Specify MAC address with local bit set\n");
		EXIT_ERROR(EINVAL);
	}

	if (hostmac != NULL) {
		if (verify_mac(hostmac)) {
			display(PERROR, "Host MAC address '%s' invalid format\n", hostmac);
			EXIT_ERROR(EINVAL);
		}

		exit(0);
	}

	for (mic = miclist; mic != NULL; mic = mic->next) {
		if (micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO)) {
			display(PWARN, "%s: Not configured - skipping\n", mic->name);
			continue;
		}

		oldhostmac = mic->config.net.hostMac;

		strncpy(mictmpmac, micmac, 18);
		last = strtoul(&micmac[15], NULL, 16) + offset++;
		if (last > 0xff) {
			last &= 0xff;
			display(PWARN, "%s: Not enough space for all the MAC addresses (MAC ending with 0xFF isn't the last one).\n", mic->name);
			display(PWARN, "	Addressing will be wrapped around.\n");
		}

		snprintf(&mictmpmac[15], 32 - 16, "%02x", last);

		strncpy(hosttmpmac, micmac, 18);
		last = strtoul(&micmac[15], NULL, 16) + offset++;
		if (last > 0xff) {
			last &= 0xff;
			display(PWARN, "%s: Not enough space for all the MAC addresses (MAC ending with 0xFF isn't the last one).\n", mic->name);
			display(PWARN, "	Addressing will be wrapped around.\n");
                }

		snprintf(&hosttmpmac[15], 32 - 16, "%02x", last);

		mic->config.net.hostMac = hosttmpmac;
		mic->config.net.micMac = mictmpmac;
		gen_ipinfo(mic, 1);

		if (mic->config.net.hostMac != oldhostmac) {
			do_ifdown(mic->name, mic->name, FALSE);

			if (mic->config.net.type == NETWORK_TYPE_STATPAIR) {
				ni[mpssenv.dist].host_addif(mic->name, mic->config.net.hostIP, mic->config.net.prefix,
					  mic->config.net.mtu, mic->config.net.hostMac);
			} else {
				if ((br = mpss_bridge_byname(mic->config.net.bridge, brlist)) != NULL) {
					ni[mpssenv.dist].attach_hostbridge(mic->name, mic->config.net.bridge,
							  mic->config.net.hostMac, br->mtu, FALSE);
				}
			}

			do_hwaddr(mic->name, mic->config.net.hostMac);
		}

		mpss_update_config(&mpssenv, mic->config.name, "MacAddrs", NULL, "MacAddrs %s %s\n",
				   hosttmpmac, mictmpmac);

		display(PINFO, "%s: Added MacAddrs: %s in %s\n", mic->name, hosttmpmac, mic->config.name);
		display(PINFO, "%s: Added MacAddrs: %s in %s\n", mic->name, mictmpmac, mic->config.name);
	}

	exit(0);
}

void
fill_mac(char **macaddr)
{
	unsigned char mac[6];
	char buf[32];
	FILE *rfp;

	if ((rfp = fopen("/dev/urandom", "r")) == NULL) {
		display(PERROR, "Failed to open dev random %d\n", errno);
		return;
	}

	fread(mac, sizeof(char), 6, rfp);
	fclose(rfp);

	mac[0] &= 0xfe;	// clear multicast bit
	mac[0] |= 0x02;	// set local assignment bit

	snprintf(buf, 32, "%02x:%02x:%02x:%02x:%02x:%02x",
			  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	*macaddr = mpssut_alloc_fill(buf);
}

void
set_mic_mac(struct mic_info *mic)
{
	char *serial;
	unsigned char mac[6];

	if (mic->config.net.micMac)
		return;

	if (mic->config.persist.micMac == NULL) {
		serial = mpss_readsysfs(mic->name, "serialnumber");
		if (mic_get_mac_from_serial(serial, mac, 0))
			fill_mac(&mic->config.net.micMac);

		free(serial);
		return;
	}

	mpssut_realloc_fill(&mic->config.net.micMac, mic->config.persist.micMac);
}

void
set_host_mac(struct mic_info *mic)
{
	char *serial;
	unsigned char mac[6];

	if (mic->config.net.hostMac)
		return;

	if (mic->config.persist.hostMac == NULL) {
		serial = mpss_readsysfs(mic->name, "serialnumber");
		if (mic_get_mac_from_serial(serial, mac, 0) == 0) {
			mic->config.net.hostMac = MIC_MAC_SERIAL;
		} else {
			fill_mac(&mic->config.net.hostMac);
		}

		free(serial);
		return;
	}

	mic->config.net.hostMac = mic->config.persist.hostMac;
}

void
set_macaddrs(struct mic_info *mic, char *mac)
{
	char *descriptor = "# MAC address configuration";

	if (mac == NULL) {
		if ((mic->config.net.hostMac == MIC_MAC_SERIAL) ||
		    (mic->config.net.hostMac == MIC_MAC_RANDOM))
			return;

		if (mic->config.net.hostMac && mic->config.net.micMac)
			return;

		set_mic_mac(mic);
		set_host_mac(mic);
	} else {
		if (!strcasecmp(mac, "serial")) {
			mic->config.net.hostMac = MIC_MAC_SERIAL;
		} else if (!strcasecmp(mac, "random")) {
			mic->config.net.hostMac = MIC_MAC_RANDOM;
		} else {
			set_mic_mac(mic);
			set_host_mac(mic);
		}
	}

	display(PFS, "%s: Update MacAddrs in %s\n", mic->name, mic->config.name);

	if (mic->config.net.hostMac == MIC_MAC_SERIAL)
		mpss_update_config(&mpssenv, mic->config.name, NULL, descriptor, "%s %s\n\n", "MacAddrs", "Serial");
	else if (mic->config.net.hostMac == MIC_MAC_RANDOM)
		mpss_update_config(&mpssenv, mic->config.name, NULL, descriptor, "%s %s\n\n", "MacAddrs", "Random");
	else
		mpss_update_config(&mpssenv, mic->config.name, NULL, descriptor, "%s %s %s\n\n", "MacAddrs",
				   mic->config.net.hostMac, mic->config.net.micMac);
}

int
check_for_brctl(void)
{
	struct stat sbuf;

	if (!mpssenv.live_update)
		return 0;

	return stat(ni[mpssenv.dist].brctl, &sbuf);
}

