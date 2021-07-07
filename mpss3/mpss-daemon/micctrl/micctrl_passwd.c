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
#include <crypt.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <getopt.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <scif.h>
#include <mpssconfig.h>
#include <libmpsscommon.h>
#include "micctrl.h"

int get_salt(char *salt, int len);
int change_pw(char *basedir, uid_t ruid, char *name, char *newpassword, char *oldpassword, char *passwd_str);
int passwd_policy_check(char *passwd);
void display(unsigned char level, char *format, ...);

int parse_pwfile(char *user, char **pw, uid_t *uid, gid_t *gid, char **name, char **home, char **app);
int parse_shadow(char *user, char **pw, char **lastd, char **minage, char **maxage, char **passwarn,
		 char **inactive, char **expire);
void pw_remote(struct mic_info *mic, char *user, char *passwd_str);

struct mbridge *brlist = NULL;

struct mpss_env mpssenv;
struct mpss_elist mpssperr;

unsigned long mpsscookie = 0;

#define PWBUF_SIZE (PATH_MAX + 256)

static struct option passwd_longopts[] = {
	{"name", required_argument, NULL, 'n'},
	{"configdir", required_argument, NULL, 'c'},
	{"stdin", no_argument, NULL, 's'},
	{"destdir", required_argument, NULL, 'd'},
	{0}
};

int
main(int argc, char **argv)
{
	struct mic_info *miclist;
	struct mic_info *mic;
	char salt[] = "$6$xxxxxxxxxxxxxxxx";
	char password0[128];
	char *password1;
	char *password2;
	char prompt[128];
	char *entry;
	char *name = NULL;
	char *password = NULL;
	char newentry[128];
	char oldpassword[128];
	char passwd_str[PWBUF_SIZE];
	char *hostdir;
	uid_t ruid;
	int longidx;
	int fail = 0;
	int cnt;
	int err;
	int c;
	int character;
	unsigned int i;

	mpssenv_init(&mpssenv);

	if ((err = mpssenv_set_configdir(&mpssenv, NULL))) {
		display(PERROR, "Unable to set default confdir: %s\n", mpssenv_errstr(err));
		exit(1);
	}

	if ((err = mpssenv_set_destdir(&mpssenv, NULL))) {
		display(PERROR, "Unable to set default destdir: %s\n", mpssenv_errstr(err));
		exit(1);
	}

	if ((err = mpssenv_set_vardir(&mpssenv, NULL))) {
		display(PERROR, "Unable to set default vardir: %s\n", mpssenv_errstr(err));
		exit(1);
	}

	while ((c = getopt_long(argc, argv, "n:s:c:d:v", passwd_longopts, &longidx)) != -1) {
		switch (c) {
		case 'n':
			name=optarg;
			break;
		case 's':
			for ( i=0; (character = getchar())!= '\n' && character != EOF && i < ((sizeof(password0))-1); ++i)
				password0[i] = character;
			password0[i] = '\0';
			password=password0;
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
				display(PERROR, "Unable to set destdir %s: %s\n", optarg,
						mpssenv_errstr(err));
				exit(1);
			}

			break;

		case 'v':
			bump_display_level();
			break;

		default:
			EXIT_ERROR(EINVAL);
		}
	}

	if ((ruid = getuid()) != 0) {
		if (password != NULL) {
                        display(PERROR, "Only root can provide password from standard input\n");
                        exit(EPERM);
                }
		if (name != NULL)
			display(PWARN, "Non root user - ignore name argument\n");

		name = getlogin();
		snprintf(prompt, 127, "\n\t[%s] Old Password:", name);
		password1 = getpass(prompt);
		strncpy(oldpassword, password1, 127);
		oldpassword[127] = '\0';
	} else {
		if (name == NULL) {
			display(PERROR, "User name is required for setting a password\n");
			exit(1);
		}
	}

	while (password == NULL) {
		snprintf(prompt, 127, "\n\t[%s] New Password:", name);
		password1 = getpass(prompt);
		strncpy(password0, password1, 127);
		password0[127] = '\0';

		snprintf(prompt, 127, "\t[%s] New Password Again:", name);
		password2 = getpass(prompt);
		putchar('\n');

		if (strcmp(password0, password2)) {
			display(PERROR, "Passwords do not match - try again\n");
		} else {
			password = password0;
		}
	}

	if (passwd_policy_check(password))
		exit(1);

	get_salt(&salt[3], strlen(salt) - 3);
	entry = crypt(password, salt);
	strncpy(newentry, entry, 127);

	miclist = create_miclist(&mpssenv, argc, argv, optind, &cnt);
	mic = miclist;
	while (mic != NULL) {
		micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO);
		display(PNORM, "%s: Changing password for %s\n", mic->name, name);

		if (change_pw(mic->config.filesrc.mic.dir, ruid, name, newentry, oldpassword, passwd_str)) {
			fail++;
		} else {
			if ((hostdir = micctrl_check_nfs_rootdev(mic)) != NULL) {
				if (change_pw(hostdir, ruid, name, newentry, oldpassword, NULL))
					fail++;
			} else {
				pw_remote(mic, name, passwd_str);
			}
		}

		mic = mic->next;
	}

	exit(fail);
}

int
passwd_policy_check(char *passwd)
{
	/*
	* TODO: Add more complex password policy check here
	*/

	if ((passwd == NULL) || ((strlen(passwd))) == 0) {
		display(PERROR, "No password supplied\n");
		return 1;
	}

	return 0;
}

void
pw_remote(struct mic_info *mic, char *user, char *passwd_str)
{
	struct scif_portID portID = {mic->id + 1, MPSSD_MICCTRL};
	scif_epd_t ep;
	char *state;
	int user_len = strlen(user);
	int passwd_len = strlen(passwd_str);
	int proto = MICCTRL_CHANGEPW;

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
	scif_send(ep, &user_len, sizeof(user_len), 0);
	scif_send(ep, user, user_len, 0);
	scif_send(ep, &passwd_len, sizeof(passwd_len), 0);
	scif_send(ep, passwd_str, passwd_len, 0);
	scif_recv(ep, &proto, sizeof(proto), SCIF_RECV_BLOCK);
	scif_close(ep);
	return;
}

int
get_salt(char *salt, int len)
{
	FILE *fp;
	int i;

	if ((fp = fopen("/dev/urandom", "r")) == NULL) {
		display(PERROR, "Failed to open dev random %d\n", errno);
		return 1;
	}

	for (i = 0; i < len; i++) {
again:
		fread(&salt[i], sizeof(char), 1, fp);
		if (!isalnum(salt[i]) && (salt[i] != '.') && (salt[i] != '/'))
			goto again;
	}

	fclose(fp);
	return 0;
}

int
change_pw(char *base, uid_t ruid, char *name, char *newpassword, char *oldpassword, char *passwd_str)
{
	char ifilename[PATH_MAX];
	char *ofilename;
	char line[PWBUF_SIZE];
	char *ret;
	FILE *ifp;
	FILE *ofp;
	char *user;
	char *pw;
	uid_t uid;
	gid_t gid;
	char *comment;
	char *home;
	char *app;
	char *lastd;
	char *minage;
	char *maxage;
	char *passwarn;
	char *inactive;
	char *expire;
	char *oldentry;
	char *oldcrypt;
	char *saltend;
	int ofd;
	int fd;
	struct statfs etc_stat;
	struct statfs var_stat;

	/*
	* We are doing statfs to check if the file system of /etc/ is the same as file system where shadow and passwd files are in destdir
	* if those are different -> the rename operation will fail, additionally this allows to avoid executing symlinks to different file system
	* (for example to /proc/ or /sys/) with root privileges
	*/

	statfs("/etc/", &etc_stat);

	if (!mpssut_filename(&mpssenv, NULL, ifilename, PATH_MAX - 1, "%s/etc/passwd", base)) {
		display(PERROR, "Passwd file %s not found: %s\n", ifilename, strerror(errno));
		return 1;
	}

	if ((ifp = fopen(ifilename, "r")) == NULL) {
		display(PERROR, "Passwd file %s not found: %s\n", ifilename, strerror(errno));
		return 1;
	}

	if ((fd = fileno(ifp)) == -1) {
		display(PERROR, "Passwd file %s not found: %s\n", ifilename, strerror(errno));
		fclose(ifp);
		return 1;
	}

	if (fstatfs(fd, &var_stat) == -1) {
		display(PERROR, "Passwd file %s not found: %s\n", ifilename, strerror(errno));
		fclose(ifp);
		return 1;
	}

	if (memcmp(&var_stat.f_fsid, &etc_stat.f_fsid, sizeof(var_stat.f_fsid))) {
		display(PERROR, "File system of %s doesn't match filesystem of /etc/\n", ifilename);
		fclose(ifp);
		return 1;
	}

	while ((ret = fgets(line, PWBUF_SIZE, ifp))) {
		user = line;
		if (parse_pwfile(user, &pw, &uid, &gid, &comment, &home, &app)) {
			continue;
		}

		if (!strcmp(user, name))
			break;
	}

	fclose(ifp);
	if (ret == NULL) {
		display(PERROR, "User '%s' not in %s\n", name, ifilename);
		return 1;
	}

	if ((ruid != uid) && (ruid != 0)) {
		display(PERROR, "Your uid does not match %s's from %s\n", name, ifilename);
		return 1;
	}

	if (!mpssut_filename(&mpssenv, NULL, ifilename, PATH_MAX - 1, "%s/etc/shadow", base)) {
		display(PERROR, "Failed to open %s - cannot modify user %s\n", ifilename, name);
		return 1;
	}

	if ((ifp = fopen(ifilename, "r")) == NULL) {
		display(PERROR, "Failed to open %s - cannot modify user %s\n", ifilename, name);
		return 1;
	}

	if ((fd = fileno(ifp)) == -1) {
		display(PERROR, "Shadow file %s not found: %s\n", ifilename, strerror(errno));
		fclose(ifp);
		return 1;
	}

	if (fstatfs(fd, &var_stat) == -1) {
		display(PERROR, "Shadow file %s not found: %s\n", ifilename, strerror(errno));
		fclose(ifp);
		return 1;
	}

	if ((ofilename = mpssut_tempnam("/etc/shadow")) == NULL) {
		display(PERROR, "internal tempname failure - skipping\n");
		fclose(ifp);
		return 1;
	}

	if ((ofd = open(ofilename, O_CREAT | O_EXCL | O_WRONLY, 0)) < 0) {
		display(PERROR, "Failed to open %s - cannot modify user %s\n", ofilename, name);
		free(ofilename);
		fclose(ifp);
		return 1;
	}

	if ((ofp = fdopen(ofd, "w")) == NULL) {
		display(PERROR, "Failed to open %s - cannot modify user %s\n", ofilename, name);
		free(ofilename);
		fclose(ifp);
		close(ofd);
		return 1;
	}

	if (memcmp(&var_stat.f_fsid, &etc_stat.f_fsid, sizeof(var_stat.f_fsid))) {
		display(PERROR, "File system of %s doesn't match filesystem of /etc/\n", ifilename);
		free(ofilename);
		fclose(ifp);
		close(ofd);
		return 1;
	}

	while ((ret = fgets(line, PWBUF_SIZE, ifp))) {
		line[strlen(line) - 1] = '\0';
		parse_shadow(user, &pw, &lastd, &minage, &maxage, &passwarn, &inactive, &expire);

		if (!strcmp(user, name)) {
			if (ruid != 0) {
				saltend = strchr(&pw[3], '$');
				*saltend = '\0';
				oldentry = crypt(oldpassword, pw);
				oldcrypt = strchr(&oldentry[3], '$') + 1;
				if (strcmp(saltend + 1, oldcrypt)) {
					display(PERROR, "%s: Old passwords do not match %s\n", ifilename, name);
					fclose(ifp);
					fclose(ofp);
					unlink(ofilename);
					free(ofilename);
					return 1;
				}
			}

			fprintf(ofp, "%s:%s:%s:%s:%s:%s:%s:%s:\n", user, newpassword, lastd, minage, maxage,
				     passwarn, inactive, expire);
			if (passwd_str != NULL)
				snprintf(passwd_str, PWBUF_SIZE, "%s:%s:%s:%s:%s:%s:%s:%s:\n", user, newpassword, lastd, minage, maxage,
						    passwarn, inactive, expire);
		} else {
			fprintf(ofp, "%s:%s:%s:%s:%s:%s:%s:%s:\n", user, pw, lastd, minage, maxage,
				     passwarn, inactive, expire);
		}
	}

	display(PFS, "Update %s\n", ifilename);
	fclose(ifp);
	fclose(ofp);

	if (mpssut_rename(&mpssenv, ofilename, ifilename)) {
			display(PERROR, "Failed to rename temporary file %s\n", ofilename);
			unlink(ofilename);
			free(ofilename);
			return 1;
	}

        free(ofilename);
	return 0;
}
