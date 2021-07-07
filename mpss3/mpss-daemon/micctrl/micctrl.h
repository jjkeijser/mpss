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

#include <scif.h>
#include <mpssconfig.h>
#include <libmpsscommon.h>

/* Global data defined in micctrl.c and used all functions */
extern struct mpss_env mpssenv;
extern struct mpss_elist mpssperr;
extern struct mbridge *brlist;

/* Used in micctrl.c to find all the various command line parser function for options */
void parse_state_args(int mode, int argc, char *argv[]);
#define STATE_BOOT	0
#define STATE_RESET	1
#define STATE_SHUTDOWN	2
#define STATE_REBOOT	3
void parse_wait_args(int argc, char *argv[]);
void parse_status_args(int argc, char *argv[]);
void parse_useradd_args(char *name, int argc, char *argv[]);
void parse_userdel_args(char *name, int argc, char *argv[]);
void parse_groupadd_args(char *name, int argc, char *argv[]);
void parse_groupdel_args(char *name, int argc, char *argv[]);
void parse_passwd_args(char *name, int argc, char *argv[], char *envp[]);
void parse_rootdev_args(char *type, int argc, char *argv[]);
void parse_addnfs_args(char *expdir, int argc, char *argv[]);
void parse_remnfs_args(char *mountdir, int argc, char *argv[]);
void parse_update_args(int mode, int argc, char *argv[]);
#define UPDATE_NFS	0
#define UPDATE_USR	1
#define UPDATE_RAM	2
void parse_hostkeys_args(char *keydir, int argc, char *argv[]);
void parse_config_args(int mode, int argc, char *argv[]);
#define CONFIG_INITDEFS		0
#define CONFIG_RESETCONF	1
#define CONFIG_RESETDEFS	2
#define CONFIG_CLEANCONF	3
void parse_showconfig_args(int argc, char *argv[]);
void parse_network_args(char *type, int argc, char *argv[]);
void parse_addbridge_args(char *type, int argc, char *argv[]);
void parse_delbridge_args(char *type, int argc, char *argv[]);
void parse_modbridge_args(char *type, int argc, char *argv[]);
void parse_service_args(char *service, int argc, char *argv[]);
void parse_overlay_args(char *service, int argc, char *argv[]);
void parse_base_args(char *base, int argc, char *argv[]);
void parse_commondir_args(char *commondir, int argc, char *argv[]);
void parse_micdir_args(char *micdir, int argc, char *argv[]);
void parse_osimage_args(char *osimage, int argc, char *argv[]);
void parse_efiimage_args(char *efiimage, int argc, char *argv[]);
void parse_autoboot_args(char *autoboot, int argc, char *argv[]);
void parse_mac_args(char *micmac, int argc, char *argv[]);
void parse_power_args(char *opt, int argc, char *argv[]);
void parse_cgroup_args(int argc, char *argv[]);
void parse_ldap_args(char *server, int argc, char *argv[]);
void parse_nis_args(char *server, int argc, char *argv[]);
void parse_userupdate_args(char *mode, int argc, char *argv[]);
void parse_rpmdir_args(char *rpmdir, int argc, char *argv[]);
void parse_syslog_args(char *rpmdir, int argc, char *argv[]);
void parse_sshkeys_args(char *rpmdir, int argc, char *argv[]);

/* Utility functions from utilfuncs.c */
int micctrl_parse_config(struct mpss_env *menv, struct mic_info *mic,
			 struct mbridge **brlist, struct mpss_elist *perrs, int errlev);
#define MIC_PARSE_SUCCESS	0
#define MIC_PARSE_EMPTY		1
#define MIC_PARSE_FAIL		2
#define MIC_PARSE_ERRORS	3
char *micctrl_check_nfs_rootdev(struct mic_info *mic);

struct mic_info *create_miclist(struct mpss_env *menv, int argc, char *argv[], int optind, int *mic_cnt);
void check_rootid(void);
void memory_alloc_error(void);

/* Supplied by rootdev.c for init.c and config.c */
void report_rootdev(struct mic_info *mic);
void set_rootdev(struct mic_info *mic);

/* Supplied by dirs.c to allow ldap and nis code in user.c to specify RPMs to include */
int mic_overlay(struct mic_info *mic, int otype, char *source, char *target, int ostate);

/* Supplied by user.c for init routines in init.c */
void gen_hostkeys(struct mic_info *mic);
void set_home_owners(char *base);
void display_file(char *base, char *name);

void gen_users(char *name, char *base, int user, int pass, char *shell, int createhome);
#define USERS_NOCHANGE	0
#define USERS_NONE	1
#define USERS_OVERWRITE	2
#define USERS_MERGE	3

#define MAX_USER_NAMELEN 32	// The Linux useradd command limits to this length.

/* Supplied by network.c for init.c and config.c */
void gen_etchosts(struct mic_info *mic_list);
void remove_network(struct mic_info *mic);
void remove_bridges(void);
void set_net_config(struct mic_info *mic, struct mic_info *miclist, int modhosts, char *modcard);
void set_bridge_config(struct mic_info *miclist);
void get_mac_strings(struct mic_info *mic, char *micmac, char *hostmac, size_t macsize);
void set_pm(struct mic_info *mic, char *pm);
void set_macaddrs(struct mic_info *mic, char *mac);
void set_local_netdir(char *netdir);
void set_alt_distrib(char *newdist);

/* help.c */
void micctrl_help(char *buffer);

/* Message display functions from utilfuncs.c */
#define PFS	4	// Display levels for micctrl not known by the perr library code
#define PNET	5

extern unsigned char display_level;

void display(unsigned char level, char *format, ...) __attribute__((format(printf, 2, 3)));
char *display_indent(void);
void bump_display_level(void);

/* Code for specifying error exits.  TODO: Make this universal and clean it up */
#define EXIT_ERROR(err) exit((err) | 0x80);

static inline void 
argerror2(char *opt, char *help)
{
	display(PERROR, "Illegal option '%s'\n", opt);
	printf("%s\n", help);
	EXIT_ERROR(EINVAL);
}

#define ARGERROR_EXIT(help) argerror2(argv[optind - 1], help)

