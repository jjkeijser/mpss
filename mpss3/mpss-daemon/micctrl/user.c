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
#include <fnmatch.h>
#include <pwd.h>
#include <scif.h>
#include <mpssconfig.h>
#include <libmpsscommon.h>
#include "micctrl.h"
#include "help.h"

void useradd(struct mic_info *miclist, char *name, uid_t uid, gid_t gid, char *home, char *comment,
	     char *app, char *shell, char *sshkeydir, int createhome, int unique);
void userdel(struct mic_info *miclist, char *name, int remove);
void groupadd(struct mic_info *miclist, char *name, gid_t gid);
void groupdel(struct mic_info *miclist, char *name);
void set_passwd(struct mic_info *miclist, char *name, int password, char *envp[]);
void set_sshkeys(struct mic_info *miclist, char *name, char *keydir);

void show_user_info(struct mic_info *miclist);
void update_users(struct mic_info *miclist, int user, int pass, char *shell, int createhome);

int parse_pwfile(char *user, char **pw, uid_t *uid, gid_t *gid, char **name, char **home, char **app);
int parse_shadow(char *user, char **pw, char **lastd, char **minage, char **maxage, char **passwarn,
		 char **inactive, char **expire);
int parse_group(char *group, char **pw, gid_t *gid, char **users);

int add_ssh_info(char *micname, char *src, char *dest, char *micdir, uid_t uid, gid_t gid);

void add_user(char *micname, char *base, FILE *pwfp, FILE *shfp, char *user, uid_t uid, gid_t gid, char *name,
	      char *home, char *app, char *shadow, char *passwd_str, char *shadow_str, int createhome,
	      char *pwname, char *shname);
void add_group(char *micname, FILE *grfp, char *user, gid_t gid, char *group_str, char *grname);
void open_add_group(char *micname, char *base, char *user, gid_t gid, char *group_str);

void ldap_configure(struct mic_info *miclist, char *server, char *base);
int ldap_disable(struct mic_info *mic);
void show_ldap(struct mic_info *miclist);

void nis_configure(struct mic_info *miclist, char *server, char *domain);
int nis_disable(struct mic_info *mic);
void show_nis(struct mic_info *miclist);

struct urpmlist {
	char *name;
	int  found;
};

struct micuser {
	char		*user;
	uid_t		uid;
	gid_t		gid;
	char		*name;
	char		*home;
	char		*app;
	char		*shadow;
	struct micuser	*next;
};

struct micgrp {
	char		*group;
	gid_t		gid;
	char		*users;
	struct micgrp	*next;
};

static unsigned long mpsscookie = 0;

#define PWBUF_SIZE (PATH_MAX + 256)

/*
 * Section 1: Top level parse routines for micctrl root device handling
 */

char *userupdate_help =
"\
micctrl [global options] --userupdate=<mode> [sub options] [Xeon Phi cards]\n\
\n\
The --userupdate options allows the reset of the users in /etc/passwd and\n\
/etc/shadow files on the Xeon Phi File system. Since --initdefaults now has the\n\
--users options it will be used less. However, the system administrator may use\n\
this command to change the user list between runs.\n\
\n\
If --users is set to 'none' the /etc/passwd and /etc/shadow files are removed\n\
and recreated with the minimal set of users required by Linux. The list\n\
includes users root, ssh, nobody, nfsnobody and micuser.\n\
\n\
If --users is set to 'overlay' the /etc/passwd and /etc/shadow files are removed\n\
and recreated with the users from the 'none' option and any regular users found\n\
in the /etc/passwd file of the host.  It should be noted this is the\n\
replacement functionality for what micctrl did in the past with the exception it\n\
no longer scrounges the /home directory for additional users.\n\
\n\
If --users is set to 'merge', the hosts /etc/passwd file is checked for any\n\
users not in the Xeon Phi /etc/passwd file and they are added to the existing\n\
users in the card's /etc/passwd and /etc/shadow files.\n\
\n\
If --users is set to 'nochange' then nothing is done.\n\
\n"
COMMON_GLOBAL
COMMON_HELP
"\
    -a <none | shadow>\n\
    --pass=<none | shadow>\n\
\n\
        The --pass option selects the policy for copying the current passwords\n\
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
        If the policy is set to 'none' the passwords in the /etc/shadow file\n\
        for the Xeon Phi cards will be set to the '*' character.\n\
\n\
    --shell=<shell>\n\
\n\
        It replaces user's default shell used to login.\n\
\n\
    --nocreate\n\
\n\
        Normally --initdefaults creates a home directory for each user it adds\n\
        to the /etc/passwd and /etc/shadow files on the Xeon Phi card file\n\
        system. The --nocreate option disables the creation of a home directory\n\
        and reduces ram file system memory usage when LDAP home directory auto\n\
        mount is enabled or the /home directory is NFS mounted.\n\
\n\
\n";


static struct option userupdate_longopts[] = {
	{"pass", required_argument, NULL, 'p'},
	{"nocreate", no_argument, NULL, 0x1001},
	{"shell", required_argument, NULL, 0x1003},
	{"help", no_argument, NULL, 'h'},
	{0}
};

void
parse_userupdate_args(char *mode, int argc, char *argv[])
{
	struct mic_info *miclist;
	int longidx;
	int c;
	int user = USERS_NOCHANGE;
	int pass = TRUE;
	char *shell = NULL;
	int createhome = TRUE;

	if (mode != NULL) {
		if (!strcasecmp(mode, "none"))
			user = USERS_NONE;
		else if (!strcasecmp(mode, "overlay"))
			user = USERS_OVERWRITE;
		else if (!strcasecmp(mode, "merge"))
			user = USERS_MERGE;
	}

	while ((c = getopt_long(argc, argv, "hvp:", userupdate_longopts, &longidx)) != -1) {
		switch(c) {
		case 'h':
			micctrl_help(userupdate_help);
			exit(0);

		case 'p':
			if (!strcasecmp(optarg, "none")) {
				pass = FALSE;
			} else if (!strcasecmp(optarg, "shadow")) {
				pass = TRUE;
			} else {
				display(PERROR, "pass option must be either a 'none' or 'shadow' string\n");
				EXIT_ERROR(EINVAL);
			}
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
			ARGERROR_EXIT(userupdate_help);
		}
	}

	miclist = create_miclist(&mpssenv, argc, argv, optind, NULL);
	if (user == USERS_NOCHANGE)
		show_user_info(miclist);

	check_rootid();
	update_users(miclist, user, pass, shell, createhome);
	exit(0);
}

/*
 * micctrl --useradd=<username> --uid=<uid> --gid=<gid> [--home=<dir>] \
 * 		[--comment=<string>] [--app=<exec>] [--sshkeys=<keyloc] [MIC list]
 *
 * Add a user to the /etc/passwd and /etc/shadow files for the MIC cards in the list.  Also create
 * the users home directory and populate the ssh key files.
 */
char *useradd_help =
"\
micctrl [global options] --useradd=<username> [sub options] [Xeon Phi cards]\n\
\n\
The --useradd option inserts the specified user into the /etc/passwd and\n\
/etc/shadow files on the Xeon Phi File system.  If no user ID or group ID is\n\
specified the host values for the user will be used. The home directory for the\n\
user will also be created and any ssh key files copied to the Xeon Phi file\n\
system.\n\
\n\
If the state of the Xeon Phi is 'online' it will send a command to add the user\n\
to the running card, create the home directory and transfer the users ssh keys\n\
files.\n\
\n"
COMMON_GLOBAL
COMMON_HELP
"\
    -u <uid>\n\
    --uid=<uid>\n\
\n\
        The --uid option specifies the user ID to be used.  The default is to\n\
        use the same user ID as the user on the host.\n\
\n\
    -g <gid>\n\
    --gid=<gid>\n\
\n\
        The --gid option specifies the group ID to be used.  The default is to\n\
        use the same group ID as the user on the host.\n\
\n\
    -d <homedir>\n\
    --home=<homedir>\n\
\n\
        The --home options specifies a different home directory to be created on\n\
        the Xeon Phi file system.  Normally the /home/<username> directory is\n\
        created.\n\
\n\
    -c <comment>\n\
    --comment=<comment>\n\
\n\
        The command field in the /etc/passwd file is usually copied from the\n\
        host's /etc/passwd file. The --command options allows a different sting\n\
        to be specified.\n\
\n\
    --shell=<shell>\n\
\n\
         It replaces user's shell default shell used to login.\n\
	 If not specified and user exists\n\
	 on the host system shell will be copied from host.\n\
	 Otherwise it will set the default /bin/bash shell.\n\
\n\
    --a <app>\n\
    --app=<app>\n\
\n\
	--app option is deprecated. Please use --shell instead.\n\
\n\
    -k <sshkeydir>\n\
    -sshkeys=<sshkeydir>\n\
\n\
        The ssh keys for the user are normally copied from the\n\
        $HOME/<username>/.ssh directory.  The --sshkeys provides an alternate\n\
        location.\n\
\n\
    --nocreate\n\
\n\
        Using the --nocreate option will cause micctrl to not create a home\n\
        directory for the user.  If the Xeon Phi card will have the /home\n\
        directory NFS mounted the home directory for the user is not needed and\n\
        some memory will be freed for the running system.\n\
\n\
    --none-unique\n\
\n\
        Normally --useradd will not allow the creation of a new user whit the\n\
        user ID as a current user.  This overrides this requirement.\n\
\n";

static struct option useradd_longopts[] = {
	{"uid", required_argument, NULL, 'u'},
	{"gid", required_argument, NULL, 'g'},
	{"home", required_argument, NULL, 'd'},
	{"comment", required_argument, NULL, 'c'},
	{"app", required_argument, NULL, 'a'},
	{"shell", required_argument, NULL, 0x1003},
	{"sshkeys", required_argument, NULL, 'k'},
	{"nocreate", no_argument, NULL, 0x1001},
	{"non-unique", required_argument, NULL, 0x1002},
	{"help", no_argument, NULL, 'h'},
	{0}
};

void
parse_useradd_args(char *name, int argc, char *argv[])
{
	struct mic_info *miclist;
	int longidx;
	int c;
	uid_t uid = NOTSET;
	gid_t gid = NOTSET;
	char *home = NULL;
	char *comment = NULL;
	char *app = NULL;
	char *shell = NULL;
	char *sshkeydir = NULL;
	int createhome = TRUE;
	int unique = TRUE;
	long temp;


	while ((c = getopt_long(argc, argv, "hvu:g:d:c:a:k:", useradd_longopts, &longidx)) != -1) {
		switch(c) {
		case 'h':
			micctrl_help(useradd_help);
			exit(0);

		case 'u':
			temp = atol(optarg);
			if ((temp >= 0) && (temp < (uid_t)-1)) {
				uid = (uid_t)temp;
			}
			else {
				display(PERROR, "Wrong uid value:\'%ld\'\n", temp);
				exit(EINVAL);
			}
			break;

		case 'g':
			temp = atol(optarg);
			if ((temp >= 0) && (temp < (gid_t)-1)) {
				gid = (gid_t)temp;
			}
			else {
				display(PERROR, "Wrong gid value:\'%ld\'\n", temp);
				exit(EINVAL);
			}
			break;

		case 'd':
			home = optarg;
			break;

		case 0x1001:
			createhome = FALSE;
			break;

		case 0x1002:
			unique = FALSE;
			break;

		case 'c':
			comment = optarg;
			break;

		case 0x1003:
			shell = optarg;
			app = NULL;
			break;
		case 'a':
			display(PWARN, "--app option is deprecated, please use --shell instead\n");
			app = optarg;
			break;

		case 'k':
			sshkeydir = mpssut_tilde(&mpssenv, optarg);
			break;

		case 'v':
			bump_display_level();
			break;

		default:
			ARGERROR_EXIT(useradd_help);
		}
	}

	check_rootid();
	miclist = create_miclist(&mpssenv, argc, argv, optind, NULL);
	useradd(miclist, name, uid, gid, home, comment, app, shell, sshkeydir, createhome, unique);
}

/*
 * micctrl --userdel=<username>  [MIC list]
 *
 * Delete the specified user from the /etc/passwd and /etc/shadow files for the specified
 * MIC cards.  Also Delete the associated home directory.
 */
char *userdel_help =
"\
micctrl [global options] --userdel=<username> [sub options] [Xeon Phi cards]\n\
\n\
The --userdel option removes the specified user from the /etc/passwd and\n\
/etc/shadow files on the Xeon Phi File system. If the state of the Xeon Phi\n\
is 'online' it will send a command to remove it on the running card.\n\
\n\
On the host the users home directory is removed.\n\
\n"
COMMON_GLOBAL
COMMON_HELP
"\
    -r\n\
    --remove\n\
\n\
        If the card is online and --remove is selected the users home directory\n\
        on the card will also be removed.\n\
\n";

static struct option userdel_longopts[] = {
	{"help", no_argument, NULL, 'h'},
	{"remove", no_argument, NULL, 'r'},
	{0}
};

void
parse_userdel_args(char *name, int argc, char *argv[])
{
	struct mic_info *miclist;
	int remove = FALSE;
	int longidx;
	int c;

	while ((c = getopt_long(argc, argv, "hrv", userdel_longopts, &longidx)) != -1) {
		switch(c) {
		case 'h':
			micctrl_help(userdel_help);
			exit(0);

		case 'v':
			bump_display_level();
			break;

		case 'r':
			remove = TRUE;
			break;

		default:
			ARGERROR_EXIT(userdel_help);
		}
	}

	check_rootid();
	miclist = create_miclist(&mpssenv, argc, argv, optind, NULL);
	userdel(miclist, name, remove);
}

/*
 * micctrl --groupadd=<groupname> --gid=<gid> [MIC list]
 *
 * Add a group to the /etc/group file for the specified MIC cards.
 */
static struct option groupadd_longopts[] = {
	{"gid", required_argument, NULL, 'g'},
	{"help", no_argument, NULL, 'h'},
	{0}
};

char *groupadd_help =
"\
micctrl [global options] --groupadd=<groupname> [sub options] [Xeon Phi cards]\n\
\n\
The --groupadd option inserts 'groupname' into the /etc/groups file on the Xeon\n\
Phi file system with the specified group ID. If the Xeon Phi is online the group\n\
will also be added from the running system.\n\
\n"
COMMON_GLOBAL
COMMON_HELP
"\
    -g <gid>\n\
    --gid=<gid>\n\
\n\
        The --gid options specifies the group ID to associate with the group\n\
        name.\n\
\n"
;

void
parse_groupadd_args(char *name, int argc, char *argv[])
{
	struct mic_info *miclist;
	int longidx;
	int c;
	gid_t gid = NOTSET;
	long temp;

	while ((c = getopt_long(argc, argv, "hvg:", groupadd_longopts, &longidx)) != -1) {
		switch(c) {
		case 'h':
			micctrl_help(groupadd_help);
			exit(0);

		case 'g':
			temp = atol(optarg);
                        if ((temp >= 0) && (temp < (gid_t)-1)) {
                                gid = (gid_t)temp;
                        }
                        else {
                                display(PERROR, "Wrong gid value:\'%ld\'\n", temp);
                                exit(EINVAL);
                        }
                        break;

		case 'v':
			bump_display_level();
			break;

		default:
			ARGERROR_EXIT(groupadd_help);
		}
	}

	check_rootid();
	miclist = create_miclist(&mpssenv, argc, argv, optind, NULL);
	groupadd(miclist, name, gid);
}

/*
 * micctrl --groupdel=<groupname> [MIC list]
 *
 * Delete a group from the /etc/group file for the specified MIC card list.
 */
char *groupdel_help =
"\
micctrl [global options] --groupdel=<groupname> [sub options] [Xeon Phi cards]\n\
\n\
The --groupdel removes 'groupname' from the /etc/groups file on the Xeon Phi\n\
file system. If the Xeon Phi card is online the group will also be removed from\n\
the running system.\n\
\n"
COMMON_GLOBAL
COMMON_HELP;

static struct option groupdel_longopts[] = {
	{"help", no_argument, NULL, 'h'},
	{0}
};

void
parse_groupdel_args(char *name, int argc, char *argv[])
{
	struct mic_info *miclist;
	int longidx;
	int c;

	while ((c = getopt_long(argc, argv, "hv", groupdel_longopts, &longidx)) != -1) {
		switch(c) {
		case 'h':
			micctrl_help(groupdel_help);
			exit(0);
			break;

		case 'v':
			bump_display_level();
			break;

		default:
			ARGERROR_EXIT(groupdel_help);
		}
	}

	check_rootid();
	miclist = create_miclist(&mpssenv, argc, argv, optind, NULL);
	groupdel(miclist, name);
}

/*
 * micctrl --passwd=<username> ..... [MIC list]
 *
 */
static struct option passwd_longopts[] = {
	{"help", no_argument, NULL, 'h'},
	{"stdin", no_argument, NULL, 's'},
	{"pass", optional_argument, NULL, 'p'},
	{0}
};

char *passwd_help =
"\
micctrl [global options] --passwd=<username> [sub options] [Xeon Phi cards]\n\
\n\
The --passwd options changes the password for a user on the Xeon Phi file\n\
system.\n\
\n\
If the caller is not the root user the 'username' option should not be used and\n\
it will prompt for the current passwd before asking for the new one.\n\
\n\
If the caller is the root user micctrl will prompt for the passwd\n\
\n"
COMMON_GLOBAL
COMMON_HELP
"\
    -s\n\
    --stdin\n\
\n\
        The --stdin options allows the root user to pass a new password in \n\
        the standard input.\n\
	Be careful --stdin option is considered as less secure and should be used only in justified cases.\n\
	It is advised to pass the password using pipe from different program.\n\
	\n\
	Usage examples: \n\
\n\
		micctrl --passwd=username --stdin <<< password\n\
\n\
		echo \"newPassword\" | micctrl --passwd=username --stdin\n\
\n\
			Where echo should be a shell built-in, because if echo resolves to /bin/echo the password can be visible to other users.\n\
\
    -p\n\
    --pass\n\
\n\
        The --pass option is deprecated, please use --stdin instead.\n\
\n";

void
parse_passwd_args(char *name, int argc, char *argv[], char *envp[])
{
	struct mic_info *miclist;
	uid_t ruid = getuid();
	int longidx;
	int c;
	int password = FALSE;

	while ((c = getopt_long(argc, argv, "p:shv", passwd_longopts, &longidx)) != -1) {
		switch (c) {
		case 'h':
			micctrl_help(passwd_help);
			exit(0);
			break;
		case 'p':
			display(PERROR, "--pass option is deprecated, please use --stdin instead\n");
			exit(0);
			break;
		case 's':
			password = TRUE;
			break;

		case 'v':
			bump_display_level();
			break;

		default:
			ARGERROR_EXIT(passwd_help);
		}
	}

	if ((ruid != 0) && (password)) {
		display(PERROR, "Only root can provide password from standard input\n");
		exit(EPERM);
	}

	miclist = create_miclist(&mpssenv, argc, argv, optind, NULL);
	set_passwd(miclist, name, password, envp);
}

static struct option sshkeys_longopts[] = {
	{"help", no_argument, NULL, 'h'},
	{"dir", required_argument, NULL, 'd'},
	{0}
};

char *sshkeys_help =
"\
micctrl [global options] --sshkeys=<username> [sub options] [Xeon Phi cards]\n\
\n\
The --sshkeys option propagates the ssh keys for a user. If called by the root\n\
user the login name of the user must be specified. If called by a non root user\n\
the 'username' parameter will be ignored.\n\
\n\
The ssh keys files to copy are found by listing the ssh directory normally found\n\
in $HOME/.ssh. The --sshkeys will copy any file owned by the specified user. It\n\
will also use add any *.pub files to the 'authorized_keys' file if not already\n\
present.\n\
\n"
COMMON_GLOBAL
COMMON_HELP
"\
    -d <keydir>\n\
    --dir=<keydir>\n\
\n\
        The --dir options sets the directory to copy keys from to 'keydir'\n\
        instead of the default $HOME/.ssh directory.\n\
\n\
        NOTE: The files in 'keydir' must still be owned by the specified user\n\
        name.\n\
\n";

void
parse_sshkeys_args(char *name, int argc, char *argv[])
{
	struct mic_info *miclist;
	int longidx;
	int c;
	char *keydir = NULL;

	while ((c = getopt_long(argc, argv, "hvd:", sshkeys_longopts, &longidx)) != -1) {
		switch (c) {
		case 'h':
			micctrl_help(sshkeys_help);
			exit(0);
			break;

		case 'd':
			keydir = mpssut_tilde(&mpssenv, optarg);
			break;

		case 'v':
			bump_display_level();
			break;

		default:
			ARGERROR_EXIT(sshkeys_help);
		}
	}

	miclist = create_miclist(&mpssenv, argc, argv, optind, NULL);
	set_sshkeys(miclist, name, keydir);
}

char *ldap_help =
"\
micctrl [global options] --ldap=<server> [sub options] [Xeon Phi cards]\n\
\n\
The --ldap option creates the configuration file required to enable LDAP user\n\
authentication for the Xeon Phi Severs.  The 'server' argument specifies the\n\
server to be used to authenticate.  The base sub option must be specified.\n\
\n\
If the 'server' paramter is set to 'disable', use of LDAP will be disabled on\n\
the Xeon Phi cards.\n\
\n\
NOTE: Configuring LDAP must install several RPMs.  The location to find the RPMs\n\
must have been defined by a previous call to 'micctrl --rpmdir'.\n\
\n"
COMMON_GLOBAL
COMMON_HELP
"\
    -b <base>\n\
    --base=<base>\n\
\n\
        The --base provided the LDAP authentication base domain name.\n\
\n";

static struct option ldap_longopts[] = {
	{"base", required_argument, NULL, 'b'},
	{"help", no_argument, NULL, 'h'},
	{0}
};

#define LDAP_PAM	"pam-ldap-1*"
#define LDAP_NSS	"nss-ldap-2*"
#define LDAP_MKHOME	"pam-plugin-mkhomedir-1*"

/* NULL terminated list of LDAP RPMs */
struct urpmlist ldap_rpm_list[] = {
	{LDAP_PAM, FALSE},
	{LDAP_NSS, FALSE},
	{LDAP_MKHOME, FALSE},
	{NULL, FALSE}
};

void
parse_ldap_args(char *server, int argc, char *argv[])
{
	struct mic_info *miclist;
	int cnt;
	int longidx;
	int c;
	char *base = NULL;
	int fail = 0;

	while ((c = getopt_long(argc, argv, "hvb:", ldap_longopts, &longidx)) != -1) {
		switch(c) {
		case 'h':
			micctrl_help(ldap_help);
			exit(0);

		case 'b':
			base = mpssut_tilde(&mpssenv, optarg);
			break;

		case 'v':
			bump_display_level();
			break;

		default:
			ARGERROR_EXIT(ldap_help);
		}
	}

	miclist = create_miclist(&mpssenv, argc, argv, optind, &cnt);

	if (server == NULL) {
		if (base != NULL) {
			display(PERROR, "Both the server identity and base domain must be specified\n");
			exit(EINVAL);
		} else {
			show_ldap(miclist);
		}
	}

	check_rootid();
	if (mpssenv_aquire_lockfile(&mpssenv)) {
		display(PERROR, "Cannot change LDAP configuration - mpssd daemon is running\n");
		exit(EBUSY);
	}

	if (!strcasecmp(server, "disable")) {
		for (; miclist != NULL; miclist = miclist->next) {
        		if (micctrl_parse_config(&mpssenv, miclist, &brlist, &mpssperr, PINFO)) {
                		display(PWARN, "%s: Not configured - skipping\n", miclist->name);
				continue;
			}

			if (ldap_disable(miclist)) {
				display(PWARN, "%s: LDAP was already disabled.\n", miclist->name);
			}
		}
	} else if (base == NULL) {
		display(PERROR, "Base domain not specified\n");
		exit(EINVAL);
	} else {
		ldap_configure(miclist, server, base);
	}

	exit(fail);
}

char *nis_help =
"\
micctrl [global options] --nis=<server> [sub options] [Xeon Phi cards]\n\
\n\
The --nis option creates the configuration file required to enable NIS user\n\
authentication for the Xeon Phi Severs.  The 'server' argument specifies the\n\
server to be used to authenticate.  The 'domain; sub option must be specified.\n\
\n\
If the 'server' paramter is set to 'disable', use of NIS will be disabled on\n\
the Xeon Phi cards.\n\
\n\
NOTE: Configuring NIS must install several RPMs.  The location to find the RPMs\n\
must have been defined by a previous call to 'micctrl --rpmdir'.\n\
\n"
COMMON_GLOBAL
COMMON_HELP
"\
    -d <domain>\n\
    --domain=<domain>\n\
\n\
        The --domain provided the NIS authentication domain name.\n\
\n";

static struct option nis_longopts[] = {
	{"domain", required_argument, NULL, 'd'},
	{"help", no_argument, NULL, 'h'},
	{0}
};

#define NIS_YPTOOLS 	"yp-tools-2*"
#define NIS_YPBIND 	"ypbind-mt-1*"
#define NIS_NSS 	"glibc-extra-nss-2*"
#define NIS_RPC 	"rpcbind-0*"
#define NIS_MKHOME 	"pam-plugin-mkhomedir-1*"

/* NULL terminated list of NIS RPMs */
struct urpmlist nis_rpm_list[] =  {
	{NIS_YPTOOLS, FALSE},
	{NIS_YPBIND, FALSE},
	{NIS_NSS, FALSE},
	{NIS_RPC, FALSE},
	{NIS_MKHOME, FALSE},
	{NULL, FALSE}
};

void
parse_nis_args(char *server, int argc, char *argv[])
{
	struct mic_info *miclist;
	int cnt;
	int longidx;
	int c;
	char *domain = NULL;
	int fail = 0;

	while ((c = getopt_long(argc, argv, "hvd:", nis_longopts, &longidx)) != -1) {
		switch(c) {
		case 'h':
			micctrl_help(nis_help);
			exit(0);

		case 'd':
			domain = optarg;
			break;
		case 'v':
			bump_display_level();
			break;

		default:
			printf("Illegal option '%c'\n", c);
			exit(EINVAL);
		}
	}

	miclist = create_miclist(&mpssenv, argc, argv, optind, &cnt);

	if (server == NULL) {
		if (domain != NULL) {
			display(PERROR, "Both the server identity and domain must be specified\n");
			exit(EINVAL);
		} else {
			show_nis(miclist);
		}
	}

	check_rootid();
	if (mpssenv_aquire_lockfile(&mpssenv)) {
		display(PERROR, "Cannot change NIS configuration - mpssd daemon is running\n");
		exit(EBUSY);
	}

	if (!strcasecmp(server, "disable")) {
		for (; miclist != NULL; miclist = miclist->next) {
        		if (micctrl_parse_config(&mpssenv, miclist, &brlist, &mpssperr, PINFO)) {
                		display(PWARN, "%s: Not configured - skipping\n", miclist->name);
				continue;
			}

			if (nis_disable(miclist)) {
				display(PWARN, "%s: NIS is already disabled.\n", miclist->name);
			}
		}
	} else if (domain == NULL) {
		display(PERROR, "NIS domain name not specified\n");
		exit(EINVAL);
	} else {
		nis_configure(miclist, server, domain);
	}

	exit(fail);
}

/*
 * Section 2: Worker functions for user control
 */
/*
 * Subfunctions of userdel.  If the root device type is RamFS inform the MIC cards in the
 * list to remove the user.
 */
void
deluser_remote(struct mic_info *mic, char *user, char *home, int remove)
{
	struct scif_portID portID = {mic->id + 1, MPSSD_MICCTRL};
	scif_epd_t ep;
	char *state;
	int user_len = strlen(user);
	int home_len = strlen(home);
	unsigned int proto = MICCTRL_DELUSER;

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
		display(PERROR, "%s: [DelUser] Failed to connect to card's mpssd\n", mic->name);
		return;
	}

	scif_send(ep, &proto, sizeof(proto), 0);
	scif_send(ep, &mpsscookie, sizeof(mpsscookie), 0);
	scif_send(ep, &user_len, sizeof(user_len), 0);
	scif_send(ep, user, user_len, 0);
	scif_send(ep, &home_len, sizeof(home_len), 0);
	scif_send(ep, home, home_len, 0);
	scif_send(ep, &remove, sizeof(remove), SCIF_SEND_BLOCK);
	scif_recv(ep, &proto, sizeof(proto), SCIF_RECV_BLOCK);
	scif_close(ep);
}

void
remove_from_shadow(char *base, char *user)
{
	char iname[PATH_MAX];
	char oname[PATH_MAX];
	char *tempname;
	char line[1024];
	FILE *ishfp;
	FILE *oshfp;
	int done = 0;

	if (!mpssut_filename(&mpssenv, NULL, iname, PATH_MAX, "%s/etc/shadow", base) ||
	    ((ishfp = fopen(iname, "r")) == NULL)) {
		display(PERROR, "Shadow file %s not found: %s\n", iname, strerror(errno));
		return;
	}

	if ((tempname = mpssut_tempnam("/etc/shadow")) == NULL) {
		display(PERROR, "internal tempname failure - skipping\n");
		fclose(ishfp);
		return;
	}

	if (mpssut_filename(&mpssenv, NULL, oname, PATH_MAX, "%s%s", base, tempname))
		unlink(oname);

	free(tempname);

	if ((oshfp = fopen(oname, "w")) == NULL) {
		display(PERROR, "1 Cannot open tmp file %s found %s\n", oname, strerror(errno));
		fclose(ishfp);
		return;
	}

	while(fgets(line, 1024, ishfp)) {
		if (strncmp(line, user, strlen(user))) {
			fprintf(oshfp, "%s", line);
		} else {
			done = 1;
		}
	}

	fchmod(fileno(oshfp), 0);
	fclose(oshfp);
	fclose(ishfp);

	if (done) {
		unlink(iname);
		link(oname, iname);
	}

	unlink(oname);
	display(PFS, "User %s removed from %s\n", user, iname);
}

int
remove_user(char *base, char *name, char *phome)
{
	char ipwname[PATH_MAX];
	char opwname[PATH_MAX];
	char hname[PATH_MAX];
	char line[PWBUF_SIZE];
	char user[PWBUF_SIZE];
	char *tempname;
	FILE *ipwfp;
	FILE *opwfp;
	char *pw;
	uid_t uid;
	gid_t gid;
	char *comment;
	char *home;
	char *app;
	int ret = 2; //return value when user is not found - similar to standard linux deluser

	if (!mpssut_filename(&mpssenv, NULL, ipwname, PATH_MAX, "%s/etc/passwd", base) ||
	    ((ipwfp = fopen(ipwname, "r")) == NULL)) {
		display(PERROR, "Passwd file %s not found: %s\n", ipwname, strerror(errno));
		return 1;
	}

	if ((tempname = mpssut_tempnam("/etc/passwd")) == NULL) {
		display(PERROR, "internal tempname failure - skipping\n");
		fclose(ipwfp);
		return 1;
	}

	if (mpssut_filename(&mpssenv, NULL, opwname, PATH_MAX, "%s%s", base, tempname))
		unlink(opwname);

	free(tempname);

	if ((opwfp = fopen(opwname, "w")) == NULL) {
		display(PERROR, "2 Cannot open tmp file %s found %s\n", opwname, strerror(errno));
		fclose(ipwfp);
		return 1;
	}

	while (fgets(line, PWBUF_SIZE, ipwfp)) {
		strncpy(user, line, PWBUF_SIZE - 1);
		user[PWBUF_SIZE - 1] = '\0';
		if (parse_pwfile(user, &pw, &uid, &gid, &comment, &home, &app))
			continue;

		if (strcmp(user, name)) {
			fprintf(opwfp, "%s", line);
			continue;
		}

		if (mpssut_filename(&mpssenv, NULL, hname, PATH_MAX, "%s%s", base, home)) {
			mpssut_deltree(&mpssenv, hname);
			display(PFS, "Remove home directory %s\n", hname);
		}

		remove_from_shadow(base, name);
		strncpy(phome, home, PATH_MAX - 1);
		phome[PATH_MAX - 1] = '\0';
		display(PFS, "User %s removed from %s\n", name, ipwname);
		ret = 0;
	}

	fclose(opwfp);
	fclose(ipwfp);

	if (ret) {
		display(PNORM, "\n");
		display(PERROR, "User %s not found in %s\n", name, ipwname);
		unlink(opwname);
	} else {
		if (mpssut_rename(&mpssenv, opwname, ipwname)) {
				display(PERROR, "Failed to rename temporary file %s\n", ipwname);
				return 1;
		}
	}

	return ret;
}

void
transmit_sshkeys(scif_epd_t ep, char *home, char *base)
{
	int proto = MICCTRL_AU_FILE;
	char keydir[PATH_MAX];
	char srcname[PATH_MAX];
	char destname[PATH_MAX];
	struct dirent *file;
	DIR *dp;
	int fd;
	int name_len;
	int file_len;
	char *filebuf;
	struct stat sbuf;
	uid_t uid;
	gid_t gid;
	int mode;

	if (!mpssut_filename(&mpssenv, NULL, keydir, PATH_MAX, "%s%s/.ssh", base, home))
		return;

	if ((dp = opendir(keydir)) == NULL)
		return;

	while ((file = readdir(dp)) != NULL) {
		if (!strcmp(file->d_name, ".") ||
		    !strcmp(file->d_name, ".."))
			continue;

		snprintf(srcname, PATH_MAX, "%s/%s", keydir, file->d_name);
		snprintf(destname, PATH_MAX, "%s/.ssh/%s", home, file->d_name);
		name_len = strlen(destname);

		if (stat(srcname, &sbuf) != 0)
			continue;

		uid = (int)sbuf.st_uid;
		gid = (int)sbuf.st_gid;
		mode = (int)sbuf.st_mode & 0777;

		if ((filebuf = (char *) malloc(sbuf.st_size + 1)) == NULL)
			continue;

		if ((fd = open(srcname, O_RDONLY)) < 0) {
			free(filebuf);
			continue;
		}

		file_len = read(fd, filebuf, sbuf.st_size);
		close(fd);

		scif_send(ep, &proto, sizeof(proto), 0);
		scif_send(ep, &name_len, sizeof(name_len), 0);
		scif_send(ep, destname, name_len, 0);
		scif_send(ep, &file_len, sizeof(file_len), 0);
		scif_send(ep, filebuf, file_len, 0);
		scif_send(ep, &uid, sizeof(uid), 0);
		scif_send(ep, &gid, sizeof(gid), 0);
		scif_send(ep, &mode, sizeof(mode), 0);
		free(filebuf);
	}

	closedir(dp);
}

/*
 * Sub function to useradd.  If the root device is a ram filesystem and the MIC card is booted
 * then tell the MIC card to add the user.
 */
void
adduser_remote(struct mic_info *mic, char *passwd_str, char *shadow_str, int createhome, char *home)
{
	struct scif_portID portID = {mic->id + 1, MPSSD_MICCTRL};
	scif_epd_t ep;
	char *state;
	int passwd_len = strlen(passwd_str);
	int shadow_len = strlen(shadow_str);
	unsigned int proto = MICCTRL_ADDUSER;

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
		display(PERROR, "%s: [AddUser] Failed to connect to card's mpssd\n", mic->name);
		return;
	}

	scif_send(ep, &proto, sizeof(proto), 0);
	scif_send(ep, &mpsscookie, sizeof(mpsscookie), 0);
	scif_send(ep, &passwd_len, sizeof(passwd_len), 0);
	scif_send(ep, passwd_str, passwd_len, 0);
	scif_send(ep, &shadow_len, sizeof(shadow_len), 0);
	scif_send(ep, shadow_str, shadow_len, SCIF_SEND_BLOCK);

	if (createhome == FALSE) {
		proto = MICCTRL_AU_NOHOME;
		scif_send(ep, &proto, sizeof(proto), SCIF_SEND_BLOCK);
	} else {
		transmit_sshkeys(ep, home, mic->config.filesrc.mic.dir);
	}

	proto = MICCTRL_AU_DONE;
	scif_send(ep, &proto, sizeof(proto), SCIF_SEND_BLOCK);
	scif_recv(ep, &proto, sizeof(proto), SCIF_RECV_BLOCK);
	scif_close(ep);
	return;
}

int
insert_user(char *micname, char *base, char *user, uid_t uid, gid_t gid, char *comment, char *home, char *app,
	    char *sshkeydir, char *passwd_str, char *shadow_str, int createhome, int unique)
{
	char passwdname[PATH_MAX];
	char shadowname[PATH_MAX];
	char line[PWBUF_SIZE];
	FILE *pwfp = NULL;
	FILE *shfp = NULL;
	char *puser;
	char *ppw;
	uid_t puid;
	gid_t pgid;
	char *pname;
	char *phome;
	char *papp;
	int err = 0;

	if (!mpssut_filename(&mpssenv, NULL, passwdname, PATH_MAX, "%s/etc/passwd", base) ||
	    ((pwfp = fopen(passwdname, "a+")) == NULL)) {
		display(PERROR, "Passwd file %s not found %s\n", passwdname, strerror(errno));
		return 1;
	}
	fseek(pwfp, 0, SEEK_SET);

	while (fgets(line, PWBUF_SIZE, pwfp)) {
		puser = line;
		if (parse_pwfile(puser, &ppw, &puid, &pgid, &pname, &phome, &papp))
			continue;

		if (!strcmp(puser, user)) {
			display(PERROR, "User %s already in %s\n", user, passwdname);
			err = 2;
			goto ui_exit;
		}

		if (unique && (puid == uid)) {
			display(PERROR, "User ID %u owned by %s already in %s\n", puid, pname, passwdname);
			err = 3;
			goto ui_exit;
		}
	}
	fseek(pwfp, 0, SEEK_END);

	if (!mpssut_filename(&mpssenv, NULL, shadowname, PATH_MAX, "%s/etc/shadow", base) ||
	    ((shfp = fopen(shadowname, "a+")) == NULL)) {
		display(PERROR, "Shadow file %s not found %s\n", shadowname, strerror(errno));
		goto ui_exit;
	}
	fseek(shfp, 0, SEEK_END);

	add_user(micname, base, pwfp, shfp, user, uid, gid, comment, home, app, NULL, passwd_str,
		shadow_str, createhome, passwdname, shadowname);

	if (createhome) add_ssh_info(micname, sshkeydir, home, base, uid, gid);

	fclose(shfp);
ui_exit:
	fclose(pwfp);
	return err;
}

/*
 * Add a user to the /etc/passwd and /etc/shadow file for all the MIC cards in the list.
 */
void
useradd(struct mic_info *miclist, char *name, uid_t uid, gid_t gid, char *home, char *comment,
	char *app, char *shell, char *sshkeydir, int createhome, int unique)
{
	struct mic_info *mic;
	struct passwd *pass;
	char passwd_str[PWBUF_SIZE];
	char shadow_str[1024];
	char homebuf[PATH_MAX];
	char commentbuf[PATH_MAX];
	char *hostdir;
	struct stat sbuf;
	int err = 0;
	int errcnt = 0;

	if (name == NULL) {
		display(PNORM, "\n");
		display(PERROR, "User name must be specified\n");
		exit(EINVAL);
	}

	if (strlen(name) > MAX_USER_NAMELEN) {
		display(PNORM, "\n");
		display(PERROR, "User name may not exceed 32 characters\n");
		exit(EINVAL);
	}

	if (uid == (uid_t) NOTSET) {
		if ((pass = getpwnam(name)) != NULL) {
			uid = pass->pw_uid;
			gid = pass->pw_gid;
			strncpy(homebuf, pass->pw_dir, PATH_MAX - 1);
			homebuf[PATH_MAX - 1] = '\0';
			home = homebuf;
			strncpy(commentbuf, pass->pw_gecos, PATH_MAX - 1);
			commentbuf[PATH_MAX - 1] = '\0';
			comment = commentbuf;
			app = NULL;
			sshkeydir = NULL;
			goto fillin_userinfo;
		}

		display(PNORM, "\n");
		display(PERROR, "User ID must be specified with the --uid option or be an existing user\n");
		printf("%s\n", useradd_help);
		exit(EINVAL);
	}

	if (gid == (gid_t) NOTSET) {
		display(PNORM, "\n");
		display(PERROR, "User ID must be specified with the --gid option\n");
		printf("%s\n", useradd_help);
		exit(EINVAL);
	}

	if (home == NULL) {
		if ((home = (char *) malloc(strlen("/home/ ") + strlen(name))) == NULL) {
			memory_alloc_error();
		}

		sprintf(home, "/home/%s", name);
	}

	if (comment == NULL) {
		if ((comment = (char *) malloc(strlen("User Account  ") + strlen(name))) == NULL) {
			memory_alloc_error();
		}

		sprintf(comment, "User Account %s", name);
	}

fillin_userinfo:
	if (shell == NULL) {
		if ((app == NULL) || (!strcmp(app, "/bin/csh")))
			app = "/bin/bash";
	} else {
		app = shell;
	}

	if (sshkeydir == NULL) {
		if ((sshkeydir = (char *) malloc(strlen("/home//.ssh ") + strlen(name))) == NULL) {
			memory_alloc_error();
		}

		sprintf(sshkeydir, "/home/%s/.ssh", name);
	} else {
		if (stat(sshkeydir, &sbuf) != 0) {
			display(PNORM, "\n");
			display(PERROR, "%s: ssh key directory '%s' not found\n", name, sshkeydir);
			printf("%s\n", useradd_help);
			exit(EINVAL);
		}
	}

	for (mic = miclist; mic != NULL; mic = mic->next) {
		micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO);

		if (mic->config.filesrc.mic.dir == NULL) {
			display(PERROR, "%s: MicDir not defined - Have you run initdefaults for this card?\n", mic->name);
			continue;
		}

		if ((err = insert_user(mic->name, mic->config.filesrc.mic.dir, name, uid, gid, comment, home, app,
				       sshkeydir, passwd_str, shadow_str, createhome, unique))) {
			errcnt++;
			continue;
		}

		if ((hostdir = micctrl_check_nfs_rootdev(mic)) != NULL)
			insert_user(mic->name, hostdir, name, uid, gid, comment, home, app, sshkeydir,
									NULL, NULL, createhome, unique);
		else
			adduser_remote(mic, passwd_str, shadow_str, createhome, home);
	}

	exit(errcnt);
}

/*
 * Delete the specified user from the /etc/passwd and /etc/shadow files for the MIC cards in the list.
 */
void
userdel(struct mic_info *miclist, char *name, int remove)
{
	struct mic_info *mic;
	char home[PATH_MAX];
	char *hostdir;
	int err = 0;

	if (name == NULL) {
		display(PNORM, "\n");
		display(PERROR, "\nuserdel: user name must be specified\n");
		printf("%s\n", userdel_help);
		exit(EINVAL);
	}

	for (mic = miclist; mic != NULL; mic = mic->next) {
		micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO);

		if (mic->config.filesrc.mic.dir == NULL) {
			display(PERROR, "%s: MicDir not defined - Have you run initdefaults for this card?\n",
					mic->name);
			continue;
		}

		err = remove_user(mic->config.filesrc.mic.dir, name, home);

		if ((hostdir = micctrl_check_nfs_rootdev(mic)) != NULL)
			remove_user(hostdir, name, home);
		else if (err == 0)
			deluser_remote(mic, name, home, remove);
	}

	exit(err);
}

int
insert_group(char *micname, char *base, char *group, gid_t gid, char *group_str)
{
	char groupname[PATH_MAX];
	char line[1024];
	char *pgroup;
	char *ppw;
	gid_t pgid;
	char *pusers;
	FILE *fp;
	int err = 0;

	if (!mpssut_filename(&mpssenv, NULL, groupname, PATH_MAX, "%s/etc/group", base) ||
	    ((fp = fopen(groupname, "a+")) == NULL)) {
		display(PERROR, "Group file %s not found: %s\n", groupname, strerror(errno));
		return 1;
	}

	fseek(fp, 0, SEEK_SET);

	while (fgets(line, 1024, fp)) {
		pgroup = line;
		if (parse_group(pgroup, &ppw, &pgid, &pusers))
			continue;

		if (!strcmp(group, pgroup)) {
			display(PERROR, "Group %s already in %s\n", group, groupname);
			err = 2;
			goto gi_exit;
		}

		if (pgid == gid) {
			display(PERROR, "Group ID %u owned by %s already in %s\n", pgid, pgroup, groupname);
			err = 3;
			goto gi_exit;
		}
	}

	fseek(fp, 0, SEEK_END);
	open_add_group(micname, base, group, gid, group_str);
gi_exit:
	fclose(fp);
	return err;
}

void
addgroup_remote(struct mic_info *mic, char *group_str)
{
	struct scif_portID portID = {mic->id + 1, MPSSD_MICCTRL};
	scif_epd_t ep;
	char *state;
	int group_len = strlen(group_str);
	unsigned int proto = MICCTRL_ADDGROUP;

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
		display(PERROR, "%s: [AddGroup] Failed to connect to card's mpssd\n", mic->name);
		return;
	}

	scif_send(ep, &proto, sizeof(proto), 0);
	scif_send(ep, &mpsscookie, sizeof(mpsscookie), 0);
	scif_send(ep, &group_len, sizeof(group_len), 0);
	scif_send(ep, group_str, group_len, 0);
	scif_recv(ep, &proto, sizeof(proto), SCIF_RECV_BLOCK);
	scif_close(ep);
	return;
}

/*
 * Add a group to the /etc/group file for the MIC cards in the list.
 */
void
groupadd(struct mic_info *miclist, char *name, gid_t gid)
{
	struct mic_info *mic;
	char group_str[1024];
	char *hostdir;
	int err = 0;
	int errcnt = 0;

	if (name == NULL) {
			display(PNORM, "\n");
		display(PERROR, "\ngroupadd: group name must be specified\n");
		printf("%s\n", groupadd_help);
		exit(0);
	}

	if (gid == (gid_t) NOTSET) {
			display(PNORM, "\n");
		display(PERROR, "\ngroupadd %s: group ID must be specified with the --gid option\n", name);
		printf("%s\n", groupadd_help);
		exit(0);
	}

	for (mic = miclist; mic != NULL; mic = mic->next) {
		micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO);
		if ((err = insert_group(mic->name, mic->config.filesrc.mic.dir, name, gid, group_str))) {
			errcnt++;
			continue;
		}

		if ((hostdir = micctrl_check_nfs_rootdev(mic)) != NULL)
			insert_group(mic->name, hostdir, name, gid, NULL);
		else
			addgroup_remote(mic, group_str);
	}

	exit(errcnt);
}

int
remove_group(char *base, char *group)
{
	char iname[PATH_MAX];
	char oname[PATH_MAX];
	char *tempname;
	char buffer[1024];
	char shadow[1024];
	char *end;
	FILE *igrfp;
	FILE *ogrfp;
	int found = 0;

	if (!mpssut_filename(&mpssenv, NULL, iname, PATH_MAX, "%s/etc/group", base) ||
	    ((igrfp = fopen(iname, "a+")) == NULL)) {
		display(PERROR, "Group file %s not found: %s\n", iname, strerror(errno));
		return 1;
	}

	if ((tempname = mpssut_tempnam("/etc/group")) == NULL) {
		display(PERROR, "internal tempname failure - skipping\n");
		fclose(igrfp);
		return 1;
	}

	if (mpssut_filename(&mpssenv, NULL, oname, PATH_MAX, "%s%s", base, tempname))
		unlink(oname);

	free(tempname);

	if ((ogrfp = fopen(oname, "w")) == NULL) {
		display(PERROR, "3 Cannot open tmp file %s found %s\n", oname, strerror(errno));
		fclose(igrfp);
		return 1;
	}

	while (fgets(buffer, 1024, igrfp)) {
		strncpy(shadow, buffer, 1023);
		shadow[1023] = '\0';
		if ((end = strchr(shadow, ':')) == NULL)
			continue;

		*end = '\0';

		if (strcmp(shadow, group)) {
			fprintf(ogrfp, "%s", buffer);
		} else {
			found = 1;
			display(PFS, "Group %s removed from %s\n", group, iname);
		}
	}

	fclose(ogrfp);
	fclose(igrfp);

	if (!found) {
		display(PERROR, "Group %s not found in %s\n", group, iname);
		unlink(oname);
		return 1;
	}

	if (mpssut_rename(&mpssenv, oname, iname)) {
			display(PERROR, "Failed to rename temporary file %s\n", iname);
			return 1;
	}
	return 0;
}

void
delgroup_remote(struct mic_info *mic, char *group)
{
	struct scif_portID portID = {mic->id + 1, MPSSD_MICCTRL};
	scif_epd_t ep;
	char *state;
	int group_len = strlen(group);
	unsigned int proto = MICCTRL_DELGROUP;

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
		display(PERROR, "%s: [DelGroup] Failed to connect to card's mpssd\n", mic->name);
		return;
	}

	scif_send(ep, &proto, sizeof(proto), 0);
	scif_send(ep, &mpsscookie, sizeof(mpsscookie), 0);
	scif_send(ep, &group_len, sizeof(group_len), 0);
	scif_send(ep, group, group_len, 0);
	scif_recv(ep, &proto, sizeof(proto), SCIF_RECV_BLOCK);
	scif_close(ep);
}

/*
 * Delete a group to the /etc/group file for the MIC cards in the list.
 */
void
groupdel(struct mic_info *miclist, char *name)
{
	struct mic_info *mic;
	char *hostdir;
	int err = FALSE;
	int errcnt = 0;

	if (name == NULL) {
		display(PNORM, "\n");
		display(PERROR, "\ngroupdel: group name must be specified\n");
		printf("%s\n", groupdel_help);
		exit(0);
	}

	for (mic = miclist; mic != NULL; mic = mic->next) {
		micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO);

		if ((err = remove_group(mic->config.filesrc.mic.dir, name)))
			errcnt++;

		if ((hostdir = micctrl_check_nfs_rootdev(mic)) != NULL)
			remove_group(hostdir, name);
		else if (err == 0)
			delgroup_remote(mic, name);
	}

	exit(errcnt);
}

void
set_passwd(struct mic_info *miclist, char *name, int password, char *envp[])
{
	pid_t pid;
	int status;
	char *ifargv[6];
	char namestring[128];
	char destdir[PATH_MAX];
	char *micctrl_passwd = "/usr/sbin/micctrl_passwd";

	int i = 0;

	pid = fork();
	if (pid == 0) {
		ifargv[i++] =  micctrl_passwd;

		if( name != NULL ) {
			snprintf(namestring, 127, "--name=%s", name);
			ifargv[i++] = namestring;
		}

		if( password )
			ifargv[i++] = "--stdin";

		if( display_level == PINFO )
			ifargv[i++] = "-v";
		else if ( display_level > PINFO)
			ifargv[i++] = "-vv";

		if (mpssenv.destdir) {
			snprintf(destdir, PATH_MAX, "--destdir=%s", mpssenv.destdir);
			ifargv[i++] = destdir;
		}

		ifargv[i] = NULL;

		execve(micctrl_passwd, ifargv, envp);
		exit(errno);
	}

	waitpid(pid, &status, 0);

	if (WIFEXITED(status))
		exit(WEXITSTATUS(status));

	exit(0);
}

int
copy_sshkeys(char *micname, char *base, uid_t ruid, char *name, char *keydir)
{
	char passname[PATH_MAX];
	char sshdir[PATH_MAX];
	char line[PWBUF_SIZE];
	char *ret;
	FILE *fp;
	char *user;
	char *pw;
	uid_t uid;
	gid_t gid;
	char *comment;
	char *home;
	char *app;

	if (!mpssut_filename(&mpssenv, NULL, passname, PATH_MAX - 1, "%s/etc/passwd", base)) {
		display(PERROR, "Passwd file %s not found: %s\n", passname, strerror(errno));
		return 1;
	}

	if ((fp = fopen(passname, "r")) == NULL) {
		display(PERROR, "Passwd file %s not found: %s\n", passname, strerror(errno));
		return 1;
	}

	while ((ret = fgets(line, PWBUF_SIZE, fp))) {
		user = line;
		if (parse_pwfile(user, &pw, &uid, &gid, &comment, &home, &app)) {
			continue;
		}

		if (!strcmp(user, name))
			break;
	}

	fclose(fp);

	if (ret == NULL) {
		display(PERROR, "User '%s' not in %s\n", name, passname);
		return 1;
	}

	if (ruid && (ruid != uid)) {
		display(PERROR, "User '%s' ID in %s does not match yours\n", name, passname);
		return 1;
	}

	if (mpssut_filename(&mpssenv, NULL, sshdir, PATH_MAX - 1, "%s/%s/.ssh", base, home))
		mpssut_deltree(&mpssenv, sshdir);

	add_ssh_info(micname, keydir, home, base, uid, gid);
	return 0;
}

void
set_sshkeys(struct mic_info *miclist, char *name, char *keydir)
{
	struct mic_info *mic;
	uid_t ruid = getuid();
	struct passwd *hpw;
	char *homedir;
	char keydirbuf[PATH_MAX];
	struct stat sbuf;

	if (ruid != 0) {
		hpw = getpwuid(ruid);
		homedir = hpw->pw_dir;

		if ((name != NULL) && (strcmp(name, hpw->pw_name) != 0)) {
			display(PERROR, "Name argument found. Only root is allowed to use this option\n");
			exit(EPERM);
		}

		name = hpw->pw_name;
	} else {
		if (name == NULL) {
			display(PERROR, "User name in required for setting a password\n");
			exit(1);
		}

		if ((hpw = getpwnam(name)) == NULL) {
			if (keydir == NULL) {
				display(PERROR, "[SshKey] Keydir not specified and user %s not found\n", name);
				exit(1);
			}
		} else {
			homedir = hpw->pw_dir;
		}
	}

	if (keydir == NULL) {
		snprintf(keydirbuf, PATH_MAX, "%s/.ssh", homedir);
		keydir = keydirbuf;
	}

	if ((stat(keydir, &sbuf) != 0) || !S_ISDIR(sbuf.st_mode)) {
		display(PERROR, "[SshKey] Key source %s is not a directory\n", keydir);
		exit(1);
	}

	if (ruid && (sbuf.st_uid != ruid)) {
		display(PERROR, "[SshKey] Key source %s directory does not belong to you\n", keydir);
		exit(1);
	}

	for (mic = miclist; mic != NULL; mic = mic->next) {
		micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO);
		copy_sshkeys(mic->name, mic->config.filesrc.mic.dir, ruid, name, keydir);
	}

	exit(0);
}

// Fill in the information needed for the add_shadow routine.
struct micuser *
create_root_user(void)
{
	struct micuser *ulhead;

	if ((ulhead = (struct micuser *) malloc(sizeof(struct micuser))) == NULL) {
		return NULL;
	}

	memset((void *)ulhead, 0, sizeof(struct micuser));

	if ((ulhead->user = (char *) malloc(strlen("root") + 1)) == NULL) {
		free(ulhead);
		return NULL;
	}
	strcpy(ulhead->user, "root");

	return ulhead;
}

void
add_shadow(struct micuser *ulhead, int pass)
{
	struct micuser *ulist;
	char line[1024];
	char buffer[1024];
	char *user;
	char *pw;
	char *lastd;
	char *minage;
	char *maxage;
	char *passwarn;
	char *inactive;
	char *expire;
	FILE *shfp;

	// Add passwd info
	if ((shfp = fopen("/etc/shadow", "r")) == NULL)
		return;

	ulist = ulhead;
	while (ulist != NULL) {
		fseek(shfp, 0, SEEK_SET);

		while(fgets(line, 1024, shfp)) {
			user = line;
			if (parse_shadow(user, &pw, &lastd, &minage, &maxage, &passwarn, &inactive, &expire))
				continue;

			if (!strcmp(ulist->user, user)) {
				if (!pass)
					pw = "*";

				snprintf(buffer, 1024, "%s:%s:%s:%s:%s:%s:%s:%s:\n",
						user, pw, lastd, minage, maxage, passwarn, inactive, expire);
				if ((ulist->shadow = (char *) malloc(strlen(buffer) + 1)) == NULL)
					goto nextshadow;

				strcpy(ulist->shadow, buffer);
				goto nextshadow;
			}
		}

nextshadow:
		ulist = ulist->next;
	}

	fclose(shfp);
	return;
}

void
ulist_free(struct micuser *ulhead)
{
	struct micuser *ulist;

	while (ulhead) {
		ulist = ulhead;
		ulhead = ulhead->next;
		free(ulist);
	}
}

FILE *
wropen(char *name, int truc, char *label)
{
	FILE *fp;

	if ((fp = fopen(name, "a+")) == NULL) {
		display(PERROR, "[%s] failed to open %s: %s\n", label, name, strerror(errno));
	} else {
		fseek(fp, SEEK_SET, 0);
		if (truc) ftruncate(fileno(fp), 0);
	}

	return fp;
}

struct micuser *
create_base_users(char *micname, char *passname, char *groupname, char *base, FILE *pwfp,
		  FILE *shfp, FILE *grfp, int pass, char *pwname, char *shname, char *grname)
{
	struct micuser *ulhead;

	if ((ulhead = create_root_user()) == NULL)
		return ulhead;

	add_shadow(ulhead, pass);

	add_user(micname, base, pwfp, shfp, "root", 0, 0, "root", "/root", "/bin/bash", ulhead->shadow,
		 NULL, NULL, TRUE, pwname, shname);
	add_ssh_info(micname, "/root/.ssh", "/root", base, 0, 0);

	add_user(micname, base, pwfp, shfp, "sshd", 74, 74,
		 "Privilege-separated SSH", "/tmp", "/bin/false", "sshd:!!:15392::::::\n",
		 NULL, NULL, FALSE, pwname, shname);
	add_user(micname, base, pwfp, shfp, "nobody", 99, 99,
		 "Nobody", "/var/lib/nfs", "/bin/false", "nobody:*:15155:0:99999:7:::\n", NULL, NULL, FALSE, pwname, shname);
	add_user(micname, base, pwfp, shfp, "nfsnobody", 65534, 65534,
		 "Anonymous NFS User", "/var/lib/nfs", "/sbin/nologin", "nfsnobody:!!:15392::::::\n",
		 NULL, NULL, FALSE, pwname, shname);
	add_user(micname, base, pwfp, shfp, "micuser", 400, 400, "MIC User",
		 "/home/micuser", "/bin/false", "micuser:!!:15392::::::\n", NULL, NULL, TRUE, pwname, shname);

	add_group(micname, grfp, "root", 0, NULL, grname);
	add_group(micname, grfp, "sshd", 74, NULL, grname);
	add_group(micname, grfp, "nobody", 99, NULL, grname);
	add_group(micname, grfp, "micuser", 400, NULL, grname);
	add_group(micname, grfp, "nfsnobody", 65534, NULL, grname);

	fseek(pwfp, SEEK_SET, 0);
	fseek(shfp, SEEK_SET, 0);
	fseek(grfp, SEEK_SET, 0);
	return ulhead;
}

struct micuser *
fill_user(char *user, uid_t uid, gid_t gid, char *name, char *home, char *app)
{
	struct micuser *mu;

	if ((mu = (struct micuser *) malloc(sizeof(struct micuser))) == NULL)
		return NULL;

	memset((void *)mu, 0, sizeof(struct micuser));

	if ((mu->user = (char *) malloc(strlen(user) + 1)) != NULL)
		strcpy(mu->user, user);

	mu->uid = uid;
	mu->gid = gid;

	if (name != NULL)
		if ((mu->name = (char *) malloc(strlen(name) + 1)) != NULL)
			strcpy(mu->name, name);

	if (home != NULL)
		if ((mu->home = (char *) malloc(strlen(home) + 1)) != NULL)
			strcpy(mu->home, home);

	if (app != NULL)
		if ((mu->app = (char *) malloc(strlen(app) + 1)) != NULL)
			strcpy(mu->app, app);

	return mu;
}

void
load_current_mic_users(struct micuser *ulhead, FILE *pwfp)
{
	struct micuser *ulist = ulhead->next;
	char line[PWBUF_SIZE];
	char *user;
	char *pw;
	uid_t uid;
	gid_t gid;
	char *name;
	char *home;
	char *app;

	ulist = ulhead;

	// First read the current passwd file information into the users list
	while (fgets(line, PWBUF_SIZE, pwfp)) {
		user = line;
		if (parse_pwfile(user, &pw, &uid, &gid, &name, &home, &app))
			continue;

		if ((ulist->next = fill_user(user, uid, gid, name, home, app)))
			ulist = ulist->next;
	}
}

struct micgrp *
fill_group(char *group, gid_t gid, char *users)
{
	struct micgrp *mg;

	if ((mg = (struct micgrp *) malloc(sizeof(struct micgrp))) == NULL)
		return NULL;

	memset((void *)mg, 0, sizeof(struct micgrp));

	if ((mg->group = (char *) malloc(strlen(group) + 1)) != NULL)
		strcpy(mg->group, group);

	mg->gid = gid;

	if (users != NULL)
		if ((mg->users = (char *) malloc(strlen(users) + 1)) != NULL)
			strcpy(mg->users, users);

	return mg;
}

struct micgrp *
load_current_mic_groups(FILE *grfp)
{
	struct micgrp grhead;
	struct micgrp *glist = &grhead;
	char line[1024];
	char *group;
	char *pw;
	gid_t gid;
	char *users;

	grhead.next = NULL;
	while (fgets(line, 1024, grfp)) {
		group = line;
		if (parse_group(group, &pw, &gid, &users))
			continue;

		if ((glist->next = fill_group(group, gid, users)))
			glist = glist->next;
	}

	fseek(grfp, SEEK_SET, 0);
	return grhead.next;
}

void
new_glist(struct micgrp **glhead, struct micgrp *current, struct micgrp *last, gid_t gid)
{
	struct micgrp *next;
	gid_t foundgid;
	char line[1024];
	char *group;
	char *pw;
	char *users;
	FILE *fp;

	if ((fp = fopen("/etc/group", "r")) == NULL) {
		display(PERROR, "Open /etc/group failed - cannot add entry for GID %u\n", gid);
		return;
	}

	/*
	 * current is not NULL. It is controlled by upper layer (function)
	 */
	while (fgets(line, 1024, fp)) {
		line[strlen(line) - 1] = '\0';
		group = line;
		if(parse_group(group, &pw, &foundgid, &users))
			continue;

		if (foundgid == gid) {
			next = fill_group(group, gid, users);

			if (*glhead == current) {
				*glhead = next;
			}

			if (current == last) {
				next->next = NULL;
				current->next = next;
			} else {
				last->next = next;
				next->next = current;
			}

		}
	}

	fclose(fp);
}

void
update_group_list(struct micgrp **glhead, gid_t gid)
{
	struct micgrp *glist = *glhead;
	struct micgrp *llist = NULL;

	while (glist) {
		if (glist->gid == gid)
			return;

		if (glist->gid > gid) {
			new_glist(glhead, glist, llist, gid);
			return;
		}

		if (glist->next == NULL) {
			new_glist(glhead, glist, glist, gid);
			return;
		}

		llist = glist;
		glist = glist->next;
	}

	return;
}

uid_t
get_min_uid()
{
	static uid_t minuid = NOTSET;

	if (minuid == (uid_t) NOTSET) {
		char line[1024];
		char *name;
		FILE *ifp;

		/*
		 * Get UID from /etc/login.defs file.
		 * Some Linux distribution has set it to 500 and other distributions have value equals 1000.
		 */
		if ((ifp = fopen("/etc/login.defs", "r")) == NULL) {
			display(PWARN, "Cannot open /etc/login.defs.\n");
		} else {

			while (fgets(line, (int)sizeof(line), ifp)) {

				name = line + strspn(line, " \t"); /* skip white spaces */

				if (*name == '\0' || *name == '#')
					continue;                /* skip the line - comment or empty */

				sscanf(name, "UID_MIN %u", &minuid);
				sscanf(name, "UID_MIN \"%u\"", &minuid); /* value may be in quote */
				/* continue because last setting in the file is valid */
			}
			fclose(ifp);
		}

		if (minuid == (uid_t) NOTSET) {
			display(PWARN, "Cannot read minimum user ID from /etc/login.defs. MIN_UID is set to 500.\n");
			minuid = 500;
		}
	}
	return minuid;
}

void
gen_users_from_host(char *micname, char *filename, struct micuser *ulhead, FILE *pwfp, FILE *shfp, int pass)
{
	struct micuser *ulist = ulhead->next;
	char line[PWBUF_SIZE];
	char *user;
	char *pw;
	uid_t uid;
	gid_t gid;
	char *name;
	char *home;
	char *app;
	FILE *hpwfp;

	if ((hpwfp = fopen("/etc/passwd", "r")) == NULL)
		return;

	while (fgets(line, PWBUF_SIZE, hpwfp)) {
		user = line;
		if (parse_pwfile(user, &pw, &uid, &gid, &name, &home, &app))
			continue;

		if ((uid < get_min_uid()) || (uid == 65534))
			continue;

		while (ulist) {
			if (!strcmp(ulist->user, user)) {
				display(PINFO, "%s: User name '%s' from host will not overwrite current card user\n",
						filename, user);
				goto nextline;
			}

			if (ulist->uid == uid) {
				display(PINFO, "%s: Host user ID %u for '%s' will conflict with current card user '%s'\n",
						filename, uid, user, ulist->user);
				goto nextline;
			}

			if (ulist->next == NULL) {
				display(PFS, "%s: Add user '%s' ID %u GID %u to %s\n", micname, user, uid,gid,filename);
				ulist->next = fill_user(user, uid, gid, name, home, app);
				goto nextline;
			}
			ulist = ulist->next;
		}
nextline:
		continue;
	}

	add_shadow(ulhead->next, pass);
	fclose(hpwfp);
}

void
gen_users(char *micname, char *base, int user, int pass, char *shell, int createhome)
{
	struct micuser *ulhead = NULL;
	struct micuser *ulist;
	struct micgrp *glhead;
	struct micgrp *glist;
	char passname[PATH_MAX];
	char shadowname[PATH_MAX];
	char groupname[PATH_MAX];
	char sshdir[PATH_MAX];
	FILE *pwfp = NULL;
	FILE *shfp = NULL;
	FILE *grfp = NULL;
	int pwexist = FALSE;

	if (!(pwexist = mpssut_filename(&mpssenv, NULL, passname, PATH_MAX, "%s/etc/passwd", base)))
		display(PFS, "%s: Created %s\n", micname, passname);

	if (!mpssut_filename(&mpssenv, NULL, shadowname, PATH_MAX, "%s/etc/shadow", base))
		display(PFS, "%s: Created %s\n", micname, shadowname);

	if (!mpssut_filename(&mpssenv, NULL, groupname, PATH_MAX, "%s/etc/group", base))
		display(PFS, "%s: Created %s\n", micname, groupname);

	if (user == USERS_NOCHANGE) {
		if (pwexist)
			return;
		else
			user = USERS_OVERWRITE;
	}

	if (!pwexist && (user == USERS_MERGE))
		user = USERS_OVERWRITE;

	switch (user) {
	case USERS_NONE:
		if ((pwfp = wropen(passname, TRUE, "GenUsers - none")) == NULL)
			return;

		if ((shfp = wropen(shadowname, TRUE, "GenUsers - none")) == NULL)
			goto gu_pwexit;

		if ((grfp = wropen(groupname, TRUE, "GenUsers - none")) == NULL)
			goto gu_shexit;

		ulhead = create_base_users(micname, passname, groupname, base, pwfp, shfp, grfp,
					   pass, passname, shadowname, groupname);
		goto gu_exit;

	case USERS_OVERWRITE:
		if ((pwfp = wropen(passname, TRUE, "GenUsers - overwrite")) == NULL)
			return;

		if ((shfp = wropen(shadowname, TRUE, "GenUsers - overwrite")) == NULL)
			goto gu_pwexit;

		if ((grfp = wropen(groupname, TRUE, "GenUsers - overwrite")) == NULL)
			goto gu_shexit;

		ulhead = create_base_users(micname, passname, groupname, base, pwfp, shfp, grfp,
					   pass, passname, shadowname, groupname);
		break;

	case USERS_MERGE:
		if ((pwfp = wropen(passname, FALSE, "GenUsers - merge")) == NULL)
			return;

		if ((shfp = wropen(shadowname, FALSE, "GenUsers - merge")) == NULL)
			goto gu_pwexit;

		if ((grfp = wropen(groupname, FALSE, "GenUsers - merge")) == NULL)
			goto gu_shexit;

		ulhead = create_root_user();
		break;
	}

	load_current_mic_users(ulhead, pwfp);
	glhead = load_current_mic_groups(grfp);
	gen_users_from_host(micname, passname, ulhead, pwfp, shfp, pass);

	ulist = ulhead->next->next;
	while (ulist != NULL) {
		if (shell != NULL) {
			if (strlen(shell) > strlen(ulist->app)) {
				free(ulist->app);
				if ((ulist->app = (char *) malloc(strlen(shell) + 1)) == NULL) {
					display(PERROR, "%s: Changing shell for user %s failed\n", micname, ulist->user);
					ulist = ulist->next;
					continue;
				}
			strncpy(ulist->app, shell, strlen(shell));
			ulist->app[strlen(shell)] = '\0';
			}
		}
		add_user(micname, base, pwfp, shfp, ulist->user, ulist->uid, ulist->gid,
			 ulist->name, ulist->home, ulist->app, ulist->shadow, NULL, NULL,
			 createhome, passname, shadowname);
		update_group_list(&glhead, ulist->gid);
		snprintf(sshdir, PATH_MAX, "%s/.ssh", ulist->home);
		add_ssh_info(micname, sshdir, ulist->home, base, ulist->uid, ulist->gid);
		ulist = ulist->next;
	}

	fflush(grfp);
	fseek(grfp, SEEK_SET, 0);
	ftruncate(fileno(grfp), 0);
	glist = glhead;
	while (glist) {
		display(PFS, "%s: Add group '%s' GID %u to %s\n", micname, glist->group, glist->gid, groupname);
		fprintf(grfp, "%s:x:%u:%s\n", glist->group, glist->gid, glist->users);
		glist = glist->next;
	}

	fclose(grfp);
gu_exit:
	ulist_free(ulhead);
gu_shexit:
	fchmod(fileno(shfp), 0);
	fclose(shfp);
gu_pwexit:
	fclose(pwfp);
}

void
display_file(char *base, char *name)
{
	char filename[PATH_MAX];
	char line[1024];
	FILE *fp;

	if (!mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s%s", base, name) ||
	    ((fp = fopen(filename, "r")) == NULL)) {
		display(PERROR, "%s not found: %s\n", filename, strerror(errno));
		return;
	}

	display(PNORM, "\n");
	display(PNORM, "%s:\n", filename);
	display(PNORM, "--------------------------------------------------------------\n");

	while (fgets(line, 1024, fp)) {
		display(PNORM, "     %s", line);
	}

	fclose(fp);
}

void
show_user_info(struct mic_info *miclist)
{
	struct mic_info *mic;
	int fail = 0;

	for (mic = miclist; mic != NULL; mic = mic->next) {
		if (micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PERROR) == MIC_PARSE_FAIL) {
			display(PWARN, "%s: Parse configuration failed - skipping\n", mic->name);
			fail++;
			continue;
		}

		switch (mic->config.rootdev.type) {
		case ROOT_TYPE_RAMFS:
			display_file(mic->config.filesrc.mic.dir, "/etc/passwd");
			if (getuid() == 0) display_file(mic->config.filesrc.mic.dir, "/etc/shadow");
			display_file(mic->config.filesrc.mic.dir, "/etc/group");
			break;

		case ROOT_TYPE_STATICRAMFS:
			break;

		case ROOT_TYPE_NFS:
		case ROOT_TYPE_SPLITNFS:
		case ROOT_TYPE_PFS:
			display_file(strchr(mic->config.rootdev.target, ':') + 1, "/etc/passwd");
			if (getuid() == 0) display_file(strchr(mic->config.rootdev.target, ':') + 1, "/etc/shadow");
			display_file(strchr(mic->config.rootdev.target, ':') + 1, "/etc/group");
			break;

		case NOTSET:
			display(PERROR, "%s: RootDev parameter not set - cannot find user information\n", mic->name);
			fail++;
		}
	}

	display(PNORM, "\n");
	exit(fail);
}

void
update_users(struct mic_info *miclist, int user, int pass, char *shell, int createhome)
{
	struct mic_info *mic;
	char *hostdir;

	for (mic = miclist; mic != NULL; mic = mic->next) {
		micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO);
		gen_users(mic->name, mic->config.filesrc.mic.dir, user, pass, shell, createhome);

		if ((hostdir = micctrl_check_nfs_rootdev(mic)) != NULL)
			gen_users(mic->name, hostdir, user, pass, shell, createhome);
	}
}

int
copy_ssh(char *micname, char *sshdir, char *destdir, char *file, uid_t uid, gid_t gid, struct stat *sbuf, char **content)
{
	char srcpath[PATH_MAX];
	char destpath[PATH_MAX];
	char *buf;
	int fd;

	snprintf(destpath, PATH_MAX, "%s/%s", destdir, file);
	unlink(destpath);

	snprintf(srcpath, PATH_MAX, "%s/%s", sshdir, file);
	if (stat(srcpath, sbuf) != 0) {
		*content = NULL;
		return 1;
	}

	if (sbuf->st_uid != uid) {
		display(PERROR, "%s: file %s is not owned by the targeted user with ID %u\n", micname, srcpath, uid);
		*content = NULL;
		return 1;
	}

	if ((buf = (char *) malloc(sbuf->st_size + 1)) == NULL) {
		*content = NULL;
		return 1;
	}

	if ((fd = open(srcpath, O_RDONLY)) < 0) {
		*content = NULL;
		free(buf);
		return 1;
	}

	if (read(fd, buf, sbuf->st_size) != sbuf->st_size) {
		*content = NULL;
		close(fd);
		free(buf);
		return 1;
	}

	close(fd);
	buf[sbuf->st_size] = '\0';

	if ((fd = open(destpath, O_WRONLY | O_CREAT, sbuf->st_mode & 0777)) < 0) {
		*content = NULL;
		free(buf);
		return 1;
	}

	write(fd, buf, sbuf->st_size);
	fchown(fd, uid, gid);
	close(fd);

	display(PFS, "%s: Created %s from %s\n", micname, destpath, srcpath);

	if (content == NULL)
		free(buf);
	else
		*content = buf;

	return 0;
}

int
create_authfile(char *micname, char *sshdir, char *destdir, char *file, uid_t uid, gid_t gid, struct stat *sbuf,
		char *pubkeys[], int pubcnt)
{
	char destpath[PATH_MAX];
	char *authbuf;
	int ostat;
	int acnt = 0;
	int cnt;
	int fd;

	ostat = copy_ssh(micname, sshdir, destdir, file, uid, gid, sbuf, &authbuf);
	snprintf(destpath, PATH_MAX, "%s/%s", destdir, file);
	if ((fd = open(destpath, O_WRONLY | O_APPEND | O_CREAT, sbuf->st_mode & 0777)) < 0) {
		free(authbuf);
		return 1;
	}

	for (cnt = 0; cnt < pubcnt; cnt++) {
		if (ostat || (strstr(authbuf, pubkeys[cnt]) == NULL)) {
			write(fd, pubkeys[cnt], strlen(pubkeys[cnt]));
			acnt++;
		}

		free(pubkeys[cnt]);
	}

	fchown(fd, uid, gid);
	close(fd);
	free(authbuf);

	if (ostat) {
		if (!acnt) {
			unlink(destpath);
			return 1;
		} else {
			display(PFS, "%s: Created %s\n", micname, destpath);
		}
	}

	return 0;
}

int
add_ssh_info(char *micname, char *sshdir, char *home, char *base, uid_t uid, gid_t gid)
{
	char homedir[PATH_MAX];
	char sshpath[PATH_MAX];
	char *pubbuf;
	char *pubkeys[32];
	int pubcnt = 0;
	struct stat sbuf;
	struct dirent *file;
	DIR *dp;
	int err;

	if ((dp = opendir(sshdir)) == NULL)
		return 0;

	if (!mpssut_filename(&mpssenv, NULL, homedir, PATH_MAX, "%s%s/.ssh", base, home)) {
		snprintf(sshpath, PATH_MAX, "%s/.ssh", home);
		if ((err = mpssut_mksubtree(&mpssenv, base, sshpath, uid, gid, 0700))) {
			display(PERROR, "%s: Error creating dir %s: %s\n", micname, sshpath, strerror(err));
			closedir(dp);
			return 2;
		}

		display(PFS, "%s: Created directory %s\n", micname, homedir);
	}

	while ((file = readdir(dp)) != NULL) {
		if ((strlen(file->d_name) < 4) || (strcmp(&file->d_name[strlen(file->d_name) - 4], ".pub")))
			continue;

		if (copy_ssh(micname, sshdir, homedir, file->d_name, uid, gid, &sbuf, &pubbuf))
				continue;
		pubkeys[pubcnt++] = pubbuf;
	}

	closedir(dp);
	if (create_authfile(micname, sshdir, homedir, "authorized_keys", uid, gid, &sbuf, pubkeys, pubcnt))
		return 1;

	return 0;
}

void
add_user(char *micname, char *base, FILE *pwfp, FILE *shfp, char *user, uid_t uid, gid_t gid, char *name,
	 char *home, char *app, char *shadow, char *passwd_str, char *shadow_str, int createhome,
	 char *pwname, char *shname)
{
	char filename[PATH_MAX];
	char line[PWBUF_SIZE];
	char *colon;
	FILE *prfp;
	int tmpfd;
	int err;

	fseek(pwfp, SEEK_SET, 0);
	while (fgets(line, PWBUF_SIZE, pwfp)) {
		if ((colon = strchr(line, ':')) == NULL)
			continue;

		*colon = '\0';

		if (!strcmp(user, line)) {
			return;
		}
	}

	fprintf(pwfp, "%s:x:%u:%u:%s:%s:%s\n", user, uid, gid, name, home, app);
	display(PFS, "%s: Add user %s UID %u GID %u to %s\n", micname, user, uid, gid, pwname);

	if (passwd_str != NULL) snprintf(passwd_str, PWBUF_SIZE, "%s:x:%u:%u:%s:%s:%s\n", user, uid, gid, name, home, app);


	if (shadow != NULL) {
		fprintf(shfp, "%s", shadow);
		if (shadow_str) snprintf(shadow_str, 1024, "%s", shadow);
	} else {
		fprintf(shfp, "%s:*:14914::::::\n", user);
		if (shadow_str) snprintf(shadow_str, 1024, "%s:*:14914::::::\n", user);
	}


	display(PFS, "%s: Add user %s UID %u GID %u to %s\n", micname, user, uid, gid, shname);

	if (createhome == 0)
		return;

	mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s%s", base, home);

	if ((err = mpssut_mksubtree(&mpssenv, base, home, uid, gid, 0700))) {
		display(PWARN, "%s: Create home directory %s failed: %s\n", micname, filename, strerror(err));
		return;
	}

	display(PFS, "%s: Created directory %s\n", micname, filename);

	if (mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s%s/.profile", base, home))
		return;

	if ((prfp = fopen(filename, "w")) == NULL) {
		display(PINFO, "Error opening %s: %s\n", filename, strerror(errno));
		return;
	}

	fprintf(prfp, "PS1='[\\u@\\h \\W]\\$ '\n");
	fprintf(prfp, "export PATH=/usr/bin:/usr/sbin:/bin:/sbin\n");

	tmpfd = fileno(prfp);
	if (tmpfd != -1)
		fchown(tmpfd, uid, gid);
	fclose(prfp);
	display(PFS, "%s: Created %s\n", micname, filename);
}

void
add_group(char *micname, FILE *grfp, char *group, gid_t gid, char *group_str, char *grname)
{
	char line[256];
	char *colon;

	fseek(grfp, SEEK_SET, 0);
	while (fgets(line, 256, grfp)) {
		if ((colon = strchr(line, ':')) == NULL)
			continue;

		*colon = '\0';

		if (!strcmp(group, line))
			return;
	}

	fprintf(grfp, "%s:x:%u:\n", group, gid);
	display(PFS, "%s: Add group %s GID %u to %s\n", micname, group, gid, grname);
	if (group_str != NULL) snprintf(group_str, 1024, "%s:x:%u:\n", group, gid);
}

void
open_add_group(char *micname, char *base, char *group, gid_t gid, char *group_str)
{
	char gname[PATH_MAX];
	FILE *grfp;

	mpssut_filename(&mpssenv, NULL, gname, PATH_MAX, "%s/etc/group", base);
	if ((grfp = fopen(gname, "a+")) == NULL)
		return;

	add_group(micname, grfp, group, gid, group_str, gname);
	fclose(grfp);
}

int
gen_sshkey(char *location, char *type)
{
	char *ifargv[11];
	pid_t pid;
	int status;
	int err = 0;

	fflush(stdout);
	fflush(stderr);

	pid = fork();
	if (pid == 0) {
		fclose(stdout);
		fclose(stderr);
		ifargv[0] = "/usr/bin/ssh-keygen";
		ifargv[1] = "-q";
		ifargv[2] = "-t";
		ifargv[3] = type;
		ifargv[4] = "-f";
		ifargv[5] = location;
		ifargv[6] = "-C";
		ifargv[7] = "";
		ifargv[8] = "-N";
		ifargv[9] = "";
		ifargv[10] = NULL;
		execve("/usr/bin/ssh-keygen", ifargv, NULL);
		exit(errno);
	}

	waitpid(pid, &status, 0);

	if (WIFEXITED(status))
		err = WEXITSTATUS(status);

	return err;
}

struct hkey {
	char *name;
	char *type;
	int optional;
} hkeys[] = {{"ssh_host_key", "rsa1", FALSE},
	     {"ssh_host_rsa_key", "rsa", FALSE},
	     {"ssh_host_dsa_key", "dsa", FALSE},
	     {"ssh_host_ecdsa_key", "ecdsa", TRUE},
	     {NULL, NULL}};

void
gen_hostkey(char *name, char *dir, struct hkey *key)
{
	char keyfile[PATH_MAX];
	char savefile[PATH_MAX];
	int err;

	if (mpssut_filename(&mpssenv, NULL, keyfile, PATH_MAX, "%s/etc/ssh/%s", dir, key->name))
		return;

	if (mpssut_filename(&mpssenv, NULL, savefile, PATH_MAX, "%s/etc/ssh/%s.save", dir, key->name)) {
		if (mpssut_rename(&mpssenv, savefile, keyfile)) {
				display(PERROR, "Failed to rename temporary file %s\n", keyfile);
				return;
		}
		mpssut_filename(&mpssenv, NULL, keyfile, PATH_MAX, "%s/etc/ssh/%s.pub", dir, key->name);
		mpssut_filename(&mpssenv, NULL, savefile, PATH_MAX, "%s/etc/ssh/%s.pub.save", dir, key->name);
		if (mpssut_rename(&mpssenv, savefile, keyfile)) {
				display(PERROR, "Failed to rename temporary file %s\n", keyfile);
				return;
		}
		display(PINFO, "%s: Restored %s from %s\n", name, keyfile, savefile);
	} else {
		if ((err = gen_sshkey(keyfile, key->type))) {
			if (key->optional == FALSE)
				display(PERROR, "%s: Create failed for /etc/ssh/ %s keys: %s\n",
						name, key->type, strerror(err));
		} else {
			display(PFS, "%s: Created %s keys\n", keyfile, key->type);
		}
	}
}

void
gen_hostkeys(struct mic_info *mic)
{
	struct hkey *key = hkeys;

	while (key->name != NULL) {
		gen_hostkey(mic->name, mic->config.filesrc.mic.dir, key);
		key++;
	}
}

/*
 * micctrl --ldap=<server> --base=<domain> [MIC list]
 *
 * Functions for parsing and enabling/disabling LDAP authentication
 * on mic card.
 */

int
_gen_ldap_conf(struct mic_info *mic, char *server, char *base, char *hostdir)
{
	char filename[PATH_MAX];
	FILE *fp;
	char *p;
	int ret = 0;

	if (base == NULL || *base == '\0') {
		display(PERROR, "%s: base is empty\n",
			mic->name);
		ret = EINVAL;
		goto exit;
	}

	mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s/etc/ldap.conf", hostdir);

	if ((fp = fopen(filename, "w")) == NULL) {
		display(PERROR, "%s: Open %s failed: %s\n", mic->name, filename, strerror(errno));
		ret = ENOENT;
		goto exit;
	}

	fprintf(fp, "URI ldap://%s\n", server);

	fputs("BASE dc=", fp);
	for (p = base; *p; p++) {
		if (*p == '.') {
			if (*(p + 1) == '\0') {
				display(PERROR, "%s: base should be XXXXX.XXX.XXX....\n", mic->name);
				ret = EINVAL;
				goto _close_exit;
			}
			fputs(",dc=", fp);
		} else
			fputc(*p, fp);
	}
	fprintf(fp, "\nbind_policy soft\n");

 _close_exit:
	fclose(fp);
	display(PFS, "%s: Created %s\n", mic->name, filename);

exit:
	return ret;
}

int
gen_ldap_conf(struct mic_info *mic, char *server, char *base)
{
	char *hostdir;
	int err;

	if ((err = _gen_ldap_conf(mic, server, base, mic->config.filesrc.mic.dir)) != 0)
		return err;

	if ((hostdir = micctrl_check_nfs_rootdev(mic)) != NULL)
		_gen_ldap_conf(mic, server, base, hostdir);

	return 0;
}

void
usr_nfs_load(struct mic_info *mic, char *hostdir, char *rpmpath, char *rpmname)
{
	char rpmdir[PATH_MAX];
	char destname[PATH_MAX];
	char srcname[PATH_MAX];
	struct stat sbuf;

	if (!mpssut_filename(&mpssenv, NULL, rpmdir, PATH_MAX, "%s/%s", hostdir, "RPMs-to-install")) {
		if (!mpssut_mksubtree(&mpssenv, hostdir, "/RPMs-to-install", 0, 0, 0777))
			display(PFS, "%s: Created directory %s\n", mic->name, rpmdir);
	}

	snprintf(srcname, PATH_MAX, "%s/%s", mic->config.filesrc.k1om_rpms, rpmname);
	snprintf(destname, PATH_MAX, "%s/%s", rpmdir, rpmname);
	stat(srcname, &sbuf);

	if (mpssut_copyfile(destname, srcname, &sbuf))
		display(PWARN, "%s: Failed to copy %s to %s\n", mic->name, srcname, destname);
	else
		display(PFS, "%s: Create %s from %s\n", mic->name, destname, srcname);
}

/*
 * Function: user_rpm_load
 *
 * Takes a NULL terminated char * array containing list of k1om RPM names
 * to be added to the overlay list.
 */

int
user_rpm_load(struct mic_info *mic, struct urpmlist *rpmlist)
{
	DIR *dir = NULL;
	struct dirent *file;
	char rpmpath[PATH_MAX];
	char *hostdir;
	struct urpmlist *rpm;
	int err = 0;

	if (!mic->config.filesrc.k1om_rpms) {
		display(PERROR, "%s: K1omRpms parameter NOT SET - Use 'micctrl --rpmdir'\n", mic->name);
		return 1;
	}

	if ((dir = opendir(mic->config.filesrc.k1om_rpms)) == NULL) {
		display(PERROR, "%s: K1omRpm sdirectory '%s' does not exist\n\n",
				mic->name, mic->config.filesrc.k1om_rpms);
		return 1;
	}

	while ((file = readdir(dir)) != NULL) {
		for (rpm = rpmlist; rpm->name != NULL; rpm++) {
			if (fnmatch(rpm->name, file->d_name, 0) == 0) {
				rpm->found = TRUE;
				snprintf(rpmpath, PATH_MAX, "%s/%s", mic->config.filesrc.k1om_rpms, rpm->name);
				if ((hostdir = micctrl_check_nfs_rootdev(mic)) != NULL)
					usr_nfs_load(mic, hostdir, rpmpath, file->d_name);
				else
					mic_overlay(mic, OVER_TYPE_RPM, rpmpath, NULL, TRUE);
			}
		}
	}

	closedir(dir);

	for (rpm = rpmlist; rpm->name != NULL; rpm++) {
		if (rpm->found == FALSE) {
			display(PERROR, "%s%s not found\n", mic->config.filesrc.k1om_rpms, rpm->name);
			err++;
		}
	}

	return err;
}

int
usr_nfs_unload(struct mic_info *mic, char *hostdir, char *rpmname)
{
	DIR *dir = NULL;
	struct dirent *file;
	char rpmpath[PATH_MAX];

	if ((dir = opendir(mic->config.filesrc.k1om_rpms)) == NULL) {
		display(PERROR, "%s: K1omRpm sdirectory '%s' does not exist\n\n",
				mic->name, mic->config.filesrc.k1om_rpms);
		return 1;
	}

	while ((file = readdir(dir)) != NULL) {
		if (fnmatch(rpmname, file->d_name, 0) == 0) {
			if (mpssut_filename(&mpssenv, NULL, rpmpath, PATH_MAX, "%s/RPMs-to-install/%s",
											hostdir, file->d_name)) {
				unlink(rpmpath);
				display(PFS, "%s: Removed %s\n", mic->name, rpmpath);
			}
		}
	}

	closedir(dir);
	return 0;
}

/*
 * Function: user_rpm_unload
 *
 * Remove the rpms in the supplied list from the cards rpm overlay list.
 */

int
user_rpm_unload(struct mic_info *mic, struct urpmlist *rpm)
{
	char rpmpath[PATH_MAX];
	char *hostdir;

	for (; rpm->name != NULL; rpm++) {
		snprintf(rpmpath, PATH_MAX, "%s/%s", mic->config.filesrc.k1om_rpms, rpm->name);
		if ((hostdir = micctrl_check_nfs_rootdev(mic)) != NULL)
			usr_nfs_unload(mic, hostdir, rpm->name);
		else
			mic_overlay(mic, OVER_TYPE_RPM, rpmpath, NULL, NOTSET);
	}

	return 0;
}

void
ldap_configure(struct mic_info *miclist, char *server, char *base)
{
	struct mic_info *mic;
	char filename[PATH_MAX];
	int fail = 0;

	if (!strlen(server)) {
		display(PERROR, "LDAP Fail: Must specify server\n");
		EXIT_ERROR(EEXIST);
	}

	if (!strlen(base)) {
		display(PERROR, "LDAP Fail: Must specify base\n");
		EXIT_ERROR(EEXIST);
	}

	for (mic = miclist; mic != NULL; mic = mic->next) {
		if (micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO)) {
			display(PWARN, "%s: Not configured - skipping\n", mic->name);
			fail++;
			continue;
		}

		if (user_rpm_load(mic, ldap_rpm_list)) {
			fail++;
			continue;
		}

		// Disable NIS if enabled
		if (mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s/etc/yp.conf", mic->config.filesrc.mic.dir)) {
			if (!nis_disable(mic))
				display(PWARN, "%s: Disabled NIS\n", mic->name);
		}

		// Generate LDAP config files
		if (gen_ldap_conf(mic, server, base)) {
			fail++;
			continue;
		}

		display(PNORM, "%s: LDAP configuration complete\n", mic->name);
	}

	exit(fail);
}

void
display_ldap(struct mic_info *mic, int biggap)
{
	char filename[PATH_MAX];

	if (biggap)
		printf("LDAP:          ");
	else
		printf("LDAP ");

	if (mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s/etc/ldap.conf", mic->config.filesrc.mic.dir))
		printf("Enabled\n");
	else
		printf("Disabled\n");
}

void
show_ldap(struct mic_info *miclist)
{
	struct mic_info *mic;
	int fail = 0;

	printf("\n");

	for (mic = miclist; mic != NULL; mic = mic->next) {
		if (micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PERROR)) {
			printf("%s: Not configured - skipping\n", mic->name);
			fail++;
			continue;
		}

		printf("%s: ", mic->name);
		display_ldap(mic, FALSE);
	}

	printf("\n");
	exit(fail);
}

/*
 * Function: ldap_disable
 *
 * Disables LDAP authentication on mic card. Currently erases ldap.conf
 * as well as removes LDAP rpms from list of rpms to install at boot
 * as requested.
 */

int
ldap_disable(struct mic_info *mic)
{
        char filename[PATH_MAX];
	char *hostdir;

        // Removes ldap.conf file from MIC directory.
        if (!mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s/etc/ldap.conf", mic->config.filesrc.mic.dir))
		return -1;	// LDAP is already disabled. 

        unlink(filename);
	display(PFS, "%s: Removed %s\n", mic->name, filename);

	if ((hostdir = micctrl_check_nfs_rootdev(mic)) != NULL) {
        	 if (mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s/etc/ldap.conf", hostdir)) {
                	unlink(filename);
			display(PFS, "%s: Removed %s\n", mic->name, filename);
		}
	}

	return user_rpm_unload(mic, ldap_rpm_list);
}

/*
 * micctrl --nis=<server> --domain=<domain> [MIC list]
 *
 * Functions for parsing and enabling/disabling NIS authentication
 * on mic card.
 */

int
_gen_yp_conf(struct mic_info *mic, char *server, char *domain, char *hostdir)
{
	char filename[PATH_MAX];
	FILE *fp;

	mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s/etc/yp.conf", hostdir);

	if ((fp = fopen(filename, "w")) == NULL) {
		display(PERROR, "%s: Open %s fail: %s\n", mic->name, filename, strerror(errno));
		return ENOENT;
	}

	fprintf(fp, "domain %s server %s\n", domain, server);
	fclose(fp);
	display(PFS, "%s: Created %s\n", mic->name, filename);
	return 0;
}

int
gen_yp_conf(struct mic_info *mic, char *server, char *domain)
{
	char *hostdir;
	int err;

	if ((err = _gen_yp_conf(mic, server, domain, mic->config.filesrc.mic.dir)) != 0)
		return err;

	if ((hostdir = micctrl_check_nfs_rootdev(mic)) != NULL)
		_gen_yp_conf(mic, server, domain, hostdir);

	return 0;
}

int
_gen_nis_services(struct mic_info *mic, char *hostdir)
{
	char linkname[PATH_MAX];
	int ret = 0;

	if (!mpssut_filename(&mpssenv, NULL, linkname, PATH_MAX, "%s/etc/rc5.d/S98rpcbind",
									mic->config.filesrc.mic.dir)) {
		if ((ret = symlink("../init.d/rpcbind", linkname)))
			return ret;

		display(PFS, "%s: Created %s\n", mic->name, linkname);
	}

	if (!mpssut_filename(&mpssenv, NULL, linkname, PATH_MAX, "%s/etc/rc5.d/S99ypbind",
									mic->config.filesrc.mic.dir)) {
		if ((ret = symlink("../init.d/ypbind", linkname)))
			return ret;

		display(PFS, "%s: Created %s\n", mic->name, linkname);
	}

	if (!mpssut_filename(&mpssenv, NULL, linkname, PATH_MAX, "%s/etc/rc1.d/K02rpcbind",
									mic->config.filesrc.mic.dir)) {
		if ((ret = symlink("../init.d/rpcbind", linkname)))
			return ret;

		display(PFS, "%s: Created %s\n", mic->name, linkname);
	}

	if (!mpssut_filename(&mpssenv, NULL, linkname, PATH_MAX, "%s/etc/rc1.d/K01ypbind",
									mic->config.filesrc.mic.dir)) {
		if ((ret = symlink("../init.d/ypbind", linkname)))
			return ret;

		display(PFS, "%s: Created %s\n", mic->name, linkname);
	}

	return 0;
}

int
gen_nis_services(struct mic_info *mic)
{
	char *hostdir;

	_gen_nis_services(mic, mic->config.filesrc.mic.dir);

	if ((hostdir = micctrl_check_nfs_rootdev(mic)) != NULL)
		_gen_nis_services(mic, hostdir);

	return 0;
}

/*
 * Function: nis_configure
 *
 * Enables NIS authentication on mic card. Creates all neccessary
 * configuration files for NIS and sets NIS rpms to be installed
 * at card boot.
 */

void
nis_configure(struct mic_info *miclist, char *server, char *domain)
{
	struct mic_info *mic = miclist;
	char filename[PATH_MAX];
	int fail = 0;

	for (mic = miclist; mic != NULL; mic = mic->next) {
		if (micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO)) {
			display(PWARN, "%s: Not configured - skipping\n", mic->name);
			fail++;
			continue;
		}

		// Installs NIS rpms on card at boot
		if (user_rpm_load(mic, nis_rpm_list)) {
			fail++;
			continue;
		}

		// Generate NIS config files
		if (gen_yp_conf(mic, server, domain)) {
			fail++;
			continue;
		}

		if (gen_nis_services(mic)) {
			fail++;
			continue;
		}

		// Disable LDAP if enabled
		if (mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s/etc/ldap.conf",
					mic->config.filesrc.mic.dir)) {
			if (!ldap_disable(mic)) {
				display(PWARN, "%s: Disabled LDAP\n", mic->name);
			}
		}

		display(PNORM, "%s: NIS configuration complete\n", mic->name);
	}

	exit(fail);
}

/*
 * fixup function called when doing resetdefaults after removeing 3.1 release and installing a later
 * release.  Due to the old .filelist code patching up actual owers.
 */
void
set_home_owners(char *base)
{
	char passwdname[PATH_MAX];
	char homedir[PATH_MAX];
	struct stat sbuf;
	char line[PWBUF_SIZE];
	FILE *fp = NULL;
	char *user;
	char *pw;
	uid_t uid;
	gid_t gid;
	char *name;
	char *home;
	char *app;

	if (!mpssut_filename(&mpssenv, NULL, passwdname, PATH_MAX, "%s/etc/passwd", base) ||
	    ((fp = fopen(passwdname, "r")) == NULL))
		return;

	while (fgets(line, PWBUF_SIZE, fp)) {
		user = line;
		if (parse_pwfile(user, &pw, &uid, &gid, &name, &home, &app))
			continue;

		if (mpssut_filename(&mpssenv, &sbuf, homedir, PATH_MAX, "%s%s", base, home))
			if ((stat(homedir, &sbuf) != 0) || S_ISDIR(sbuf.st_mode))
				chown(homedir, uid, gid);
	}

	fclose(fp);
}

void
display_nis(struct mic_info *mic, int biggap)
{
	char filename[PATH_MAX];

	if (biggap)
		printf("NIS:          ");
	else
		printf("NIS ");

	if (mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s/etc/yp.conf", mic->config.filesrc.mic.dir))
		printf("Enabled\n");
	else
		printf("Disabled\n");
}

void
show_nis(struct mic_info *miclist)
{
	struct mic_info *mic;
	int fail = 0;

	printf("\n");

	for (mic = miclist; mic != NULL; mic = mic->next) {
		if (micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PERROR)) {
			printf("%s: Not configured - skipping\n", mic->name);
			fail++;
			continue;
		}

		printf("%s: ", mic->name);
		display_nis(mic, FALSE);
	}

	printf("\n");
	exit(fail);
}

/*
 * Function: nis_disable
 *
 * Disables NIS authentication on mic card. Currently erases yp.conf
 * as well as removes NIS rpms from list of rpms to install at boot.
 */

int
nis_disable(struct mic_info *mic)
{
        char filename[PATH_MAX];
	char *hostdir;
	char *micdir = mic->config.filesrc.mic.dir;

        // Removes ldap.conf file from MIC directory.
        if (!mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s/etc/yp.conf", micdir))
		return -1; // NIS is already disabled.  

	unlink(filename);
	display(PFS, "%s: Removed %s\n", mic->name, filename);

	if (mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s/etc/rc5.d/S98rpcbind", micdir)) {
		unlink(filename);
		display(PFS, "%s: Removed %s\n", mic->name, filename);
	}

	if (mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s/etc/rc5.d/S99ypbind", micdir)) {
		unlink(filename);
		display(PFS, "%s: Removed %s\n", mic->name, filename);
	}

	if (mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s/etc/rc1.d/K02rpcbind", micdir)) {
		unlink(filename);
		display(PFS, "%s: Removed %s\n", mic->name, filename);
	}

	if (mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s/etc/rc1.d/K01ypbind", micdir)) {
		unlink(filename);
		display(PFS, "%s: Removed %s\n", mic->name, filename);
	}

	if ((hostdir = micctrl_check_nfs_rootdev(mic)) != NULL) {
        	if (mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s/etc/yp.conf", hostdir)) {
                	unlink(filename);

			if (mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s/etc/rc5.d/S98rpcbind", hostdir)) {
				unlink(filename);
				display(PFS, "%s: Removed %s\n", mic->name, filename);
			}

			if (mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s/etc/rc5.d/S99ypbind", hostdir)) {
				unlink(filename);
				display(PFS, "%s: Removed %s\n", mic->name, filename);
			}

			if (mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s/etc/rc1.d/K02rpcbind", hostdir)) {
				unlink(filename);
				display(PFS, "%s: Removed %s\n", mic->name, filename);
			}

			if (mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s/etc/rc1.d/K01ypbind", hostdir)) {
				unlink(filename);
				display(PFS, "%s: Removed %s\n", mic->name, filename);
			}
		}
	}

	return user_rpm_unload(mic, nis_rpm_list);
}

