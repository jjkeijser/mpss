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
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <getopt.h>
#include "micctrl.h"
#include "help.h"

char *base_help =
"\
micctrl [global options] --base=<type> [sub options] [Xeon Phi cards]\n\
\n\
The --base option sets the Base configuration parameter and may be of the types\n\
'cpio' or 'dir'.  The syntax also allows the value of 'default' which will set\n\
the type to 'cpio' and the location to indicate the init ram disk file\n\
installed with the 'mpss-boot-files' RPM.\n\
\n\
If the type is set to CPIO the --new option specifies the CPIO file to use\n\
instead of the default installed init ram disk image.  If the file defined\n\
by new does not exist then the previous file will be copied there.\n\
\n\
If the current type of base is a directory and the caller defines a cpio entry\n\
that does dot exist, micctrl will not know how to create the new target so it\n\
will return an error.\n\
\n\
If the type is set to dir the the '--new' option must point to a directory on\n\
the host file system with the replacement files.  If the directory does not\n\
exist it will be created filled with either the expanded cpio image or the\n\
contents of the directory from the previous configuration.\n\
\n"
COMMON_GLOBAL
HELP_SUBHEAD
HELP_SUBALL
"\
    -n <location>\n\
    --new=<location>\n\
\n\
        The --new option specifies either the location of the new CPIO file or\n\
        directory associated with the specified Base type.\n\
\n";

static struct option dir_longopts[] = {
	{"help", no_argument, NULL, 'h'},
	{"new", required_argument, NULL, 'n'},
	{0}
};

void show_dirs(struct mic_info *miclist);
void setbase(struct mic_info *miclist, char *type, char *new_base);

void
parse_base_args(char *type, int argc, char *argv[])
{
	struct mic_info *miclist;
	int cnt;
	int longidx;
	int c;
	char *new_base = NULL;

	while ((c = getopt_long(argc, argv, "hvn:", dir_longopts, &longidx)) != -1) {
		switch(c) {
		case 'h':
			micctrl_help(base_help);
			exit(0);

		case 'n':
			new_base = mpssut_tilde(&mpssenv, optarg);
			break;

		case 'v':
			bump_display_level();
			break;

		default:
			ARGERROR_EXIT(base_help);
		}
	}

	miclist = create_miclist(&mpssenv, argc, argv, optind, &cnt);

	if (type == NULL)
		show_dirs(miclist);

	check_rootid();
	setbase(miclist, type, new_base);
}

char *commondir_help =
"\
micctrl [global options] --commondir=<commondir> [sub options] [Xeon Phi cards]\n\
\n\
The --commondir option sets the CommonDir configuration parameter to the\n\
specified directory.\n\
\n\
If 'commondir' does not exist the files in the old setting of the CommonDir\n\
parameter will be copied to the new location.\n\
\n\
If there are not more references in the configuration files to the old value\n\
of CommonDir then all the files associated with the old configuration are\n\
removed\n\
\n"
COMMON_GLOBAL
COMMON_HELP;

void setcommondir(struct mic_info *miclist, char *common);

void
parse_commondir_args(char *common, int argc, char *argv[])
{
	struct mic_info *miclist;
	char *comdir = NULL;
	int longidx;
	int c;
 	int cnt;

	if (common != NULL)
		comdir = mpssut_tilde(&mpssenv, common);

	while ((c = getopt_long(argc, argv, "hv", dir_longopts, &longidx)) != -1) {
		switch(c) {
		case 'h':
			micctrl_help(commondir_help);
			exit(0);

		case 'v':
			bump_display_level();
			break;

		default:
			ARGERROR_EXIT(commondir_help);
		}
	}

	miclist = create_miclist(&mpssenv, argc, argv, optind, &cnt);

	if (common == NULL) {
		show_dirs(miclist);
	} else {
		check_rootid();
		setcommondir(miclist, comdir);
	}
}

char *micdir_help =
"\
micctrl [global options] --micdir=<micdir> [sub options] <Xeon Phi card>\n\
\n\
The --micdir option sets the MicDir configuration parameter to the specified\n\
directory. Unlike most options a Xeon Phi card ID must be specified.\n\
\n\
If 'micdir' does not exist the files in the old setting of the  MicDir parameter\n\
will be copied to the new location and the old location removed.\n\
\n"
COMMON_GLOBAL
COMMON_HELP;

void setmicdir(struct mic_info *mic, char *micdir, int cnt);

void
parse_micdir_args(char *micdir, int argc, char *argv[])
{
	struct mic_info *miclist;
	char *mdir = NULL;
	int cnt;
	int longidx;
	int c;

	if (micdir != NULL)
		mdir = mpssut_tilde(&mpssenv, micdir);

	while ((c = getopt_long(argc, argv, "hv", dir_longopts, &longidx)) != -1) {
		switch(c) {
		case 'h':
			micctrl_help(micdir_help);
			exit(0);

		case 'v':
			bump_display_level();
			break;

		default:
			ARGERROR_EXIT(micdir_help);
		}
	}

	miclist = create_miclist(&mpssenv, argc, argv, optind, &cnt);

	if (micdir == NULL) {
		show_dirs(miclist);
	} else {
		check_rootid();
		setmicdir(miclist, mdir, cnt);
	}
}

char *overlay_help =
"\
micctrl [global options] --overlay=<type> [sub options] <Xeon Phi cards>\n\
\n\
The --overlay option indicates a set of files to include on the Xeon Phi file\n\
system by adding 'Overlay' parameters to the configuration files.  There may be\n\
multiple overlays specified.  The 'type' argument specifies the type of overlay\n\
and may be set to 'simple', 'filelist', 'file' or 'rpm'.\n\
\n\
If the type is set to 'simple the --source argument is a directory of files on\n\
host to be placed in the --target directory on the Xeon Phi file systems.  The\n\
files are placed in the cards file system with the same owner and permissions\n\
as the file have on the host.\n\
\n\
If the type is set to 'file' the --source argument is a single file to be placed\n\
on the Xeon Phi file system in the directory provided by the --target option\n\
with the same owner and permissions.\n\
\n\
If the type is set to 'filelist' the --source argument is a directory of files\n\
on the host to be placed in the Xeon Phi file system.  The --target options is\n\
set to a file describing the placing and permissions of each of the files.\n\
Describing the format of the descriptor or 'filelist' file is beyond the space\n\
of this help message.  Refer to the documentation for more information.\n\
\n\
If the type is set to 'rpm' the --source options points to either a RPM file for\n\
machine ID k1om for the Xeon Phi cards or a directory of the RPM files.  The\n\
must be a full path from the root directory ('/').  If is is not directory then\n\
the file part of the path may include wild card characters.  At boot time the\n\
indicated RPMs are copied to the Xeon Phi file system under the directory\n\
'RPMs-to-install'.  The booting init script will check this directory and use\n\
the 'rpm' command it install them.\n\
\n"
COMMON_GLOBAL
COMMON_HELP
"\
    -s <source>\n\
    --source=<source>\n\
\n\
        The --source option is dependent on the type of overlay.  If set to\n\
        'rpm' it indicates either a single RPM or a directory of RPMs to\n\
        install. Otherwise it indicates the source directory of the files to\n\
        place in the file system.\n\
\n\
    -t <target>\n\
    --target=<target>\n\
\n\
        The --target option is dependent on the type of overlay.  If the type is\n\
        'filelist' it points to the 'filelist' file providing the location of\n\
        the files on the file system.  If the type is set to 'rpm' it has no\n\
        meaning.  Otherwise it is set to the directory on the card to place the\n\
        files in.\n\
\n\
    -d <on | off | delete>\n\
    --state=<on | off | delete>\n\
\n\
        The --state option indicates the action to apply to the specified\n\
        overlay.  If set to 'delete' the overlay will be removed from the\n\
        configuration files.  If set to 'on' the indicated overlay will be in\n\
        the Xeon Phi file system after the card has booted.  The setting of\n\
        'off' is a hold over from the 2.1 release and is not recommended to use.\n\
\n";

static struct option overlay_longopts[] = {
	{"help", no_argument, NULL, 'h'},
	{"source", required_argument, NULL, 's'},
	{"target", required_argument, NULL, 't'},
	{"state", required_argument, NULL, 'd'},
	{0}
};

void show_overlays(struct mic_info *miclist);
int overlay(struct mic_info *miclist, char *type, char *source, char *target, char *state);

void
parse_overlay_args(char *type, int argc, char *argv[])
{
	struct mic_info *miclist;
	int cnt;
	int longidx;
	int c;
	char *source = NULL;
	char *target = NULL;
	char *state = NULL;

	while ((c = getopt_long(argc, argv, "hvs:t:d:", overlay_longopts, &longidx)) != -1) {
		switch(c) {
		case 'h':
			micctrl_help(overlay_help);
			exit(0);

		case 's':
			source = mpssut_tilde(&mpssenv, optarg);
			break;

		case 't':
			target = mpssut_tilde(&mpssenv, optarg);
			break;

		case 'd':
			state = optarg;
			break;

		case 'v':
			bump_display_level();
			break;

		default:
			ARGERROR_EXIT(overlay_help);
		}
	}

	miclist = create_miclist(&mpssenv, argc, argv, optind, &cnt);

	if (type == NULL) {
		show_overlays(miclist);
	} else {
		check_rootid();
		overlay(miclist, type, source, target, state);
	}

	exit(0);
}

char *rpmdir_help =
"\
micctrl [global options] --rpmdir=<rpmdir> [sub options] <Xeon Phi cards>\n\
\n\
The --rpmdir option set the K1omRpms configuration parameter to the specified\n\
directory. At this time it is only used by the --ldap and --nis options to find\n\
their required RPMs.  There is currently no default for this options\n\
\n"
COMMON_GLOBAL
COMMON_HELP;

static struct option rpmdir_longopts[] = {
	{"help", no_argument, NULL, 'h'},
	{0}
};

void show_rpmdirs(struct mic_info *miclist);
void setrpmdir(struct mic_info *miclist, char *dir);

void
parse_rpmdir_args(char *rpmdir, int argc, char *argv[])
{
	struct mic_info *miclist;
	char *rdir = NULL;
	int cnt;
	int longidx;
	int c;

	if (rpmdir != NULL) {
		rdir = mpssut_tilde(&mpssenv, rpmdir);

		if (strlen(rpmdir) && (rpmdir[0] != '/')) {
			display(PERROR, "Directory must start with '/'\n");
			putchar('\n');
			micctrl_help(rpmdir_help);
			exit(1);
		}
	}

	while ((c = getopt_long(argc, argv, "hv", rpmdir_longopts, &longidx)) != -1) {
		switch(c) {
		case 'h':
			micctrl_help(rpmdir_help);
			exit(0);

		case 'v':
			bump_display_level();
			break;

		default:
			ARGERROR_EXIT(rpmdir_help);
		}
	}

	miclist = create_miclist(&mpssenv, argc, argv, optind, &cnt);

	if ((rpmdir == NULL) || !strlen(rpmdir)) {
		show_rpmdirs(miclist);
	} else {
		check_rootid();
		setrpmdir(miclist, rdir);
	}
}

void
show_dirs(struct mic_info *miclist)
{
	struct mic_info *mic;

	putchar('\n');

	for (mic = miclist; mic != NULL; mic = mic->next) {
		if (micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO)) {
			display(PWARN, "%s: Not configured - skipping\n", mic->name);
			continue;
		}

		switch (mic->config.filesrc.base.type) {
		case SRCTYPE_CPIO:
			printf("%s:\t  base: CPIO %s\n", mic->name, mic->config.filesrc.base.image);
			break;
		case SRCTYPE_DIR:
			printf("%s:\t  base: DIR %s\n", mic->name, mic->config.filesrc.base.dir);
			break;
		}

		printf("\tcommon: %s\n", mic->config.filesrc.common.dir);
		printf("\t   mic: %s\n", mic->config.filesrc.mic.dir);
	}

	putchar('\n');
	exit(0);
}

void
copydir(char *name, char *from, char *to)
{
	char oldfrom[PATH_MAX];
	char oldto[PATH_MAX];
	char newfrom[PATH_MAX];
	char newto[PATH_MAX];
	char rbuf[PATH_MAX];
	struct stat sbuf;
	char *buf;
	int flen;
	int fd;
	struct dirent *file;
	DIR *dp;

	if (!mpssut_filename(&mpssenv, &sbuf, oldfrom, PATH_MAX, "%s", from))
		return;

	mpssut_filename(&mpssenv, NULL, oldto, PATH_MAX, "%s", to);

	if (S_ISREG(sbuf.st_mode)) {
		if ((flen = mpssut_alloc_and_load(oldfrom, &buf)) < 0) {
			display(PWARN, "%s: Failed to load file %s\n", name, oldfrom);
			return;
		}

		if ((fd = open(oldto, O_WRONLY | O_CREAT, sbuf.st_mode & 0777)) < 0) {
			free(buf);
			return;
		}

		flen = write(fd, buf, flen);
		fchmod(fd, sbuf.st_mode & 0777);
		fchown(fd, sbuf.st_uid, sbuf.st_gid);
		close(fd);
		free(buf);
		return;
	} else if (S_ISCHR(sbuf.st_mode)) {
		mknod(oldto, S_IFCHR | sbuf.st_mode, sbuf.st_rdev);
		chmod(oldto, sbuf.st_mode & 0777);
		chown(oldto, sbuf.st_uid, sbuf.st_gid);
		return;
	} else if (S_ISBLK(sbuf.st_mode)) {
		mknod(oldto, S_IFBLK | sbuf.st_mode, sbuf.st_rdev);
		chmod(oldto, sbuf.st_mode & 0777);
		chown(oldto, sbuf.st_uid, sbuf.st_gid);
		return;
	} else if (S_ISFIFO(sbuf.st_mode)) {
		mknod(oldto, S_IFIFO | sbuf.st_mode, sbuf.st_rdev);
		chmod(oldto, sbuf.st_mode & 0777);
		chown(oldto, sbuf.st_uid, sbuf.st_gid);
		return;
	} else if (S_ISLNK(sbuf.st_mode)) {
		memset(rbuf, 0, PATH_MAX);
		readlink(oldfrom, rbuf, PATH_MAX);
		symlink(rbuf, oldto);
		chmod(oldto, sbuf.st_mode & 0777);
		chown(oldto, sbuf.st_uid, sbuf.st_gid);
		return;
	} else if (S_ISSOCK(sbuf.st_mode)) {
		mknod(oldto, S_IFSOCK | sbuf.st_mode, sbuf.st_rdev);
		return;
	}

	mkdir(oldto, sbuf.st_mode & 0777);
	chown(oldto, sbuf.st_uid, sbuf.st_gid);

	if ((dp = opendir(oldfrom)) == NULL)
		return;

	while ((file = readdir(dp)) != NULL) {
		if (!strcmp(file->d_name, ".") ||
		    !strcmp(file->d_name, "..")) {
			continue;
		}

		snprintf(newfrom, PATH_MAX, "%s/%s", from, file->d_name);
		snprintf(newto, PATH_MAX, "%s/%s", to, file->d_name);
		copydir(name, newfrom, newto);
	}

	closedir(dp);
}

int
set_cpioconfig(struct mic_info *mic, char* cpiofile)
{
	int err;

	display(PINFO, "%s: Setting CPIO base to %s\n", mic->name, cpiofile);
	if ((err = mpss_update_config(&mpssenv, mic->config.name, "Base", NULL, "Base CPIO %s\n", cpiofile))) {
		display(PERROR, "Failed to open config file '%s': %d\n", mic->name, err);
		return 1;
	}

	display(PFS, "%s: Update Base to %s in %s\n", mic->name, cpiofile, mic->config.name);
	return 0;
}

void gzip_image(char *name);

int
set_cpiobase(struct mic_info *miclist, char *newcpio)
{
	struct mic_info *mic;
	char ifilename[PATH_MAX];
	char ofilename[PATH_MAX];
	char defaultbase[PATH_MAX];
	char *tmpname;
	struct stat sbuf;
	char *buffer;
	int failcnt = 0;
	int flen;
	int fd;

	for (mic = miclist; mic != NULL; mic = mic->next) {
		if (micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO)) {
			display(PWARN, "%s: Not configured - skipping\n", mic->name);
			failcnt++;
			continue;
		}

		if ((mic->config.filesrc.base.type == SRCTYPE_CPIO) &&
		    !strcmp(newcpio, mic->config.filesrc.base.image)) {
			display(PWARN, "%s: CPIO file not changed - skipping\n", mic->name);
			continue;
		}

		snprintf(defaultbase, PATH_MAX, "%s/%s", mpssenv.srcdir, DEFAULT_INITRD_KNC);

		if (!strcmp(newcpio, defaultbase)) {
			display(PINFO, "%s: Using default file %s\n", mic->name, newcpio);
			failcnt += set_cpioconfig(mic, newcpio);
			continue;
		}

		if (mpssut_filename(&mpssenv, NULL, ofilename, PATH_MAX, "%s", newcpio)) {
			display(PINFO, "%s: Using existing file %s\n", mic->name, ofilename);
			failcnt += set_cpioconfig(mic, newcpio);
			continue;
		}

		switch (mic->config.filesrc.base.type) {
		case SRCTYPE_DIR:
			if (!mpssut_filename(&mpssenv, &sbuf, ifilename, PATH_MAX, "%s",
									mic->config.filesrc.base.dir)) {
				display(PERROR, "%s: Could not find %s\n", mic->name, mic->config.filesrc.base.dir);
				failcnt++;
				continue;
			}

			if ((tmpname = mpssut_tempnam(ofilename)) == NULL) {
				display(PERROR, "%s: internal tempname failure - skipping\n", mic->name);
				failcnt++;
				continue;
			}

			if (mpssfs_cpioout(tmpname, ifilename)) {
				display(PERROR, "%s: CPIO failed to %s\n", mic->name, ofilename);
				failcnt++;
				continue;
			}

			snprintf(ifilename, PATH_MAX, "%s.gz", tmpname);
			gzip_image(tmpname);

			if (mpssut_rename(&mpssenv, ifilename, ofilename)) {
					display(PERROR, "Failed to rename temporary file %s\n", ofilename);
					failcnt++;
					free(tmpname);
					continue;
			}
			free(tmpname);
			break;

		case SRCTYPE_CPIO:
			if (!mpssut_filename(&mpssenv, &sbuf, ifilename, PATH_MAX, "%s",
									mic->config.filesrc.base.image)) {
				display(PERROR, "%s: Could not find %s\n", mic->name, mic->config.filesrc.base.image);
				failcnt++;
				continue;
			}

			if ((flen = mpssut_alloc_and_load(ifilename, &buffer)) < 0) {
				display(PERROR, "%s: Failed to load %s\n", mic->name, mic->config.filesrc.base.image);
				failcnt++;
				continue;
			}

			if ((fd = open(ofilename, O_WRONLY|O_CREAT, 0644)) < 0) {
				display(PERROR, "%s: Could not open %s\n", mic->name, newcpio);
				free(buffer);
				failcnt++;
				continue;
			}

			write(fd, buffer, flen);
			close(fd);
			free(buffer);
			break;
		default:
			display(PERROR, "%s: Malformed Base entry in config - skipping\n", mic->name);
			failcnt++;
			continue;
		}

		failcnt += set_cpioconfig(mic, newcpio);
	}

	return failcnt;
}

int
set_dirbase(struct mic_info *miclist, char *newdir)
{
	struct mic_info *mic;
	struct mpss_elist mpssperr;
	char cpioname[PATH_MAX];
	char srcname[PATH_MAX];
	char dirname[PATH_MAX];
	char *tmpname1;
	char tmpname2[PATH_MAX];
	char tmpname3[PATH_MAX];
	struct stat sbuf;
	int failcnt = 0;
	int err = 0;

	for (mic = miclist; mic != NULL; mic = mic->next) {
		if (micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO)) {
			display(PWARN, "%s: Not configured - skipping\n", mic->name);
			failcnt++;
			continue;
		}

		switch (mic->config.filesrc.base.type) {
		case SRCTYPE_DIR:
			if (!strcmp(newdir, mic->config.filesrc.base.dir)) {
				display(PWARN, "%s: DIR %s not changed - skipping\n", mic->name, newdir);
				continue;
			}

			if (!mpssut_filename(&mpssenv, NULL, dirname, PATH_MAX, "%s", newdir)) {
				// Create dir and copy files;
				if (!mpssut_filename(&mpssenv, NULL, srcname, PATH_MAX, "%s",
						   mic->config.filesrc.base.dir)) {
					display(PNORM, "%s: old directory %s not found - skipping\n",
						mic->name, srcname);
					failcnt++;
					continue;
				}

				mpssut_copytree(&mpssenv, dirname, mic->config.filesrc.base.dir);
				display(PFS, "%s: Created base directory %s from %s\n", mic->name, dirname,
						mic->config.filesrc.base.dir);
			} else {
				display(PINFO, "%s: DIR %s already exists - not creating\n", mic->name, dirname);
			}
			break;

		case SRCTYPE_CPIO:
			if (!mpssut_filename(&mpssenv, NULL, dirname, PATH_MAX, "%s", newdir)) {
				// Create dir and unpack CPIO file.
				if (!mpssut_filename(&mpssenv, NULL, cpioname, PATH_MAX, "%s",
						   mic->config.filesrc.base.image)) {
					if (stat(mic->config.filesrc.base.image, &sbuf) != 0) {
						display(PERROR, "%s: old CPIO initrd file %s not found - skipping\n",
								mic->name, cpioname);
						failcnt++;
						continue;
					}

					strncpy(cpioname, mic->config.filesrc.base.image, PATH_MAX - 1);
					cpioname[PATH_MAX - 1] = '\0';
				}
				
				snprintf(tmpname2, PATH_MAX, "%s/%s", mpssenv.vardir, mic->name);

				if ((tmpname1 = mpssut_tempnam(tmpname2)) == NULL) {
					display(PERROR, "%s: internal tempname failure - skipping\n", mic->name);
					failcnt++;
					continue;
				}

				unlink(tmpname1);

				if ((err = mpssut_mksubtree(&mpssenv, "", newdir, 0, 0, 0777))) {
					display(PERROR, "%s: Error creating dir %s: %d\n", mic->name, dirname, err);
					failcnt++;
					continue;
				}

				mpssut_filename(&mpssenv, NULL, tmpname3, PATH_MAX, "%s", tmpname1);
				if (mpssut_filename(&mpssenv, NULL, tmpname2, PATH_MAX, "%s.gz", tmpname1))
					unlink(tmpname2);

				free(tmpname1);
				mpssfs_unzip_base_cpio(mic->name, cpioname, tmpname2, &mpssperr);

				if ((err = mpssfs_cpioin(tmpname3, dirname, FALSE))) {
					if (WIFEXITED(err)) {
						display(PERROR, "%s: CPIO in %s to %s failed %s\n", mic->name,
								tmpname3, dirname, strerror(WEXITSTATUS(err)));
						failcnt++;
						continue;
					}

					if (WIFSIGNALED(err)) {
						display(PERROR, "%s: CPIO in %s to %s terminated signel %d\n", mic->name,
								tmpname3, dirname, WTERMSIG(err));
						failcnt++;
						continue;
					}
				}
				unlink(tmpname3);
				display(PFS, "%s: Created base directory %s from %s\n", mic->name, dirname, cpioname);
			} else {
				display(PINFO, "%s: DIR %s already exists - not creating\n", mic->name, dirname);
			}

			break;

		default:
			display(PERROR, "%s: Malformed Base entry in config - skipping\n", mic->name);
			failcnt++;
			continue;
		}

		if ((err = mpss_update_config(&mpssenv, mic->config.name, "Base", NULL, "Base DIR %s\n", newdir))) {
			display(PERROR, "Failed to open config file '%s': %d\n", mic->config.name, err);
			failcnt++;
		} else {
			display(PFS, "%s: Update Base in %s\n", mic->name, mic->config.name);
		}
	}

	return failcnt;
}

void
setbase(struct mic_info *miclist, char *type, char *new_base)
{
	char defaultbase[PATH_MAX];

	if (!strcasecmp(type, "default")) {
		snprintf(defaultbase, PATH_MAX, "%s/%s", mpssenv.srcdir, DEFAULT_INITRD_KNC);
		exit(set_cpiobase(miclist, defaultbase));
	} else if (!strcasecmp(type, "cpio")) {
		if (new_base == NULL) {
			display(PERROR, "Compressed cpio file must be specified as new\n");
			EXIT_ERROR(EINVAL);
		}

		exit(set_cpiobase(miclist, new_base));
	} else if (!strcasecmp(type, "dir")) {
		if (new_base == NULL) {
			display(PERROR, "Base directory must be specified as new\n");
			EXIT_ERROR(EINVAL);
		}

		exit(set_dirbase(miclist, new_base));
	} else {
		display(PERROR, "base must be set to 'default' or 'cpio\n");
		EXIT_ERROR(EINVAL);
	}

	exit(0);
}

struct deletions {
	char		 file[PATH_MAX];
	struct deletions *next;
};

void
setcommondir(struct mic_info *miclist, char *common)
{
	struct mic_info *mic;
	struct mic_info *fulllist;
	struct stat sbuf;
	struct deletions delhead;
	struct deletions *del;
	char fromdir[PATH_MAX];
	char todir[PATH_MAX];
	int copied = FALSE;
	int dodel = TRUE;
	int err;

	delhead.next = NULL;

	if (stat(common, &sbuf) == 0) {
		display(PWARN, "%s will not be overwritten\n", common);
		copied = TRUE;
	}

	for (mic = miclist; mic != NULL; mic = mic->next) {
		if (micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO)) {
			display(PWARN, "%s: Not configured - skipping\n", mic->name);
			continue;
		}

		if (!strcmp(mic->config.filesrc.common.dir, common)) {
			display(PWARN, "%s: not changed - will not copy it or %s\n",
				mic->name, mic->config.filesrc.common.dir);
			continue;
		}

		mpssut_filename(&mpssenv, NULL, todir, PATH_MAX, "%s", common);
		if (!mpssut_filename(&mpssenv, NULL, fromdir, PATH_MAX, "%s", mic->config.filesrc.common.dir)) {
			display(PERROR, "%s: Source directory %s does not exist - cannot copy to %s\n",
				mic->name, fromdir, todir);
			continue;
		}

		del = &delhead;
		while (del->next) {
			del = del->next;
			if (!strcmp(del->file, fromdir))
				goto notnew;
		}

		if ((del->next = (struct deletions *) malloc(sizeof(struct deletions))) != NULL) {
			del = del->next;
			strncpy(del->file, fromdir, PATH_MAX - 1);
			del->file[PATH_MAX - 1] = '\0';
			del->next = NULL;
		}

notnew:
		if (copied == FALSE) {
			copied = TRUE;
			copydir(mic->name, mic->config.filesrc.common.dir, common);
			display(PFS, "%s: Created directory %s from %s\n", mic->name, todir, fromdir);
		}

		if ((err = mpss_update_config(&mpssenv, mic->config.name, "CommonDir", NULL, "CommonDir %s\n", common)))
			display(PERROR, "Failed to open config file '%s': %d\n", mic->config.name, err);
		else
			display(PFS, "%s: Update CommonDir in %s\n", mic->name, mic->config.name);
	}

	if ((fulllist = mpss_get_miclist(&mpssenv, NULL)) == NULL) {
		fulllist = miclist;
	} else {
		mic = fulllist;
		while (mic != NULL) {
			micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO);
			mic = mic->next;
		}
	}

	del = &delhead;
	while (del->next) {
		del = del->next;

		for (mic = fulllist; mic != NULL; mic = mic->next) {
			mpssut_filename(&mpssenv, NULL, todir, PATH_MAX, "%s", common);
			if (!strcmp(del->file, todir)) {
				display(PWARN, "%s: still has common directory %s - will not delete\n", mic->name, todir);
				dodel = FALSE;
			}
		}

		if (dodel) {
			mpssut_deltree(&mpssenv, del->file);
			display(PFS, "Default: Deleted directory %s\n", del->file);
		}
	}

	exit(0);
}

void
setmicdir(struct mic_info *mic, char *micdir, int cnt)
{
	char fromdir[PATH_MAX];
	char todir[PATH_MAX];
	int err;

	if (cnt != 1) {
		display(PERROR, "micctrl --micdir requires specifying a single node instead of %d\n",
			cnt);
		EXIT_ERROR(EINVAL);
	}

	if (micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO)) {
		display(PWARN, "%s: Not configured - skipping\n", mic->name);
		exit(0);
	}

	if (mpssut_filename(&mpssenv, NULL, todir, PATH_MAX, "%s", micdir)) {
		display(PWARN, "%s: Destination directory %s already exists\n", mic->name, todir);
		exit(0);
	}

	if (!mpssut_filename(&mpssenv, NULL, fromdir, PATH_MAX, "%s", mic->config.filesrc.mic.dir)) {
		display(PERROR, "%s: Source directory %s not present\n", mic->name, fromdir);
		exit(0);
	}

	copydir(mic->name, mic->config.filesrc.mic.dir, micdir);
	display(PFS, "%s: Created directory %s from %s\n", mic->name, todir, fromdir);
	mpssut_deltree(&mpssenv, fromdir);
	display(PFS, "%s: Deleted directory %s\n", mic->name, fromdir);

	if ((err = mpss_update_config(&mpssenv, mic->config.name, "MicDir", NULL, "MicDir %s\n", micdir))) {
		display(PERROR, "Failed to open config file '%s': %d\n", mic->config.name, err);
		exit(13);
	} else {
		display(PFS, "%s: Update MicDir in %s\n", mic->name, mic->config.name);
	}

	exit(0);
}

static char *ostrings[] = {"Simple", "Filelist", "File", "RPM"};

void
show_overlays(struct mic_info *miclist)
{
	struct mic_info *mic;
	struct moverdir *dir;

	putchar('\n');

	for (mic = miclist; mic != NULL; mic = mic->next) {
		if (micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO)) {
			display(PWARN, "%s: Not configured - skipping\n", mic->name);
			continue;
		}

		dir = mic->config.filesrc.overlays.next;
		printf("%s:", mic->name);

		while (dir) {
			if (dir->type != OVER_TYPE_RPM)
				printf("\t%s %s %s %s\n", ostrings[dir->type], dir->dir,
					dir->target, dir->state? "on" : "off");
			else
				printf("\t%s %s %s\n", ostrings[dir->type], dir->dir,
					dir->state? "on" : "off");
			dir = dir->next;
		}
	}

	putchar('\n');
	exit(0);
}

char *overtypes[] = {"Simple", "Filelist", "File", "RPM"};
char *overstates[] = {"Off", "On"};

int
add_overlay(struct mic_info *mic, int level, int otype, int ostate, char *source, char *target)
{
	char param[128];
	int err;

	display(PINFO, "Changing overlay state to %s\n", overstates[ostate]);

	if (otype == OVER_TYPE_RPM) {
		snprintf(param, sizeof(param), "Overlay %s %s", overtypes[otype], source);
		if ((err = mpss_update_config(&mpssenv, mic->config.name, param, NULL,
				"Overlay %s %s %s\n", overtypes[otype], source, overstates[ostate])))
			display(PERROR, "Failed to open config file '%s': %d\n", mic->config.name, err);
		else
			display(PFS, "%s: Update Overlay %s in %s\n", mic->name, source, mic->config.name);
	} else {
		snprintf(param, sizeof(param), "Overlay %s %s %s", overtypes[otype], source, target);
		if ((err = mpss_update_config(&mpssenv, mic->config.name, param, NULL,
				"Overlay %s %s %s %s\n", overtypes[otype], source, target, overstates[ostate])))
			display(PERROR, "Failed to open config file '%s': %d\n", mic->config.name, err);
		else
			display(PFS, "%s: Update Overlay in %s\n", mic->name, mic->config.name);
	}
	return 0;
}

int
delete_overlay(struct mic_info *mic, int level, int otype, char *source, char *target)
{
	char param[128];

	if (level) {
		display(PERROR, "%s: Cannot remove overlay from %s.\n", mic->name, mic->config.name);
		return 1;
	}

	if (otype == OVER_TYPE_RPM) {
		snprintf(param, sizeof(param), "Overlay %s %s", overtypes[otype], source);
	} else {
		snprintf(param, sizeof(param), "Overlay %s %s %s", overtypes[otype], source, target);
	}

	mpss_remove_config(&mpssenv, mic, param);
	return 0;
}

int
mic_overlay(struct mic_info *mic, int otype, char *source, char *target, int ostate)
{
	struct moverdir *dir;

	for (dir = mic->config.filesrc.overlays.next; dir != NULL; dir = dir->next) {
		if ((otype == dir->type) &&
		    !strcmp(source, dir->dir) &&
		    ((otype == OVER_TYPE_RPM) || !strcmp(target, dir->target))) {
			if (ostate == NOTSET)
				return delete_overlay(mic, dir->level, otype, source, target);

			if (dir->state != ostate)
				return add_overlay(mic, dir->level, otype, ostate, source, target);

			display(PINFO, "Overlay state not changed\n");
			return 0;
		}
	}

	if (ostate == NOTSET) {
		display(PERROR, "%s: Cannot remove non existent overlay %s %s\n", mic->name, overtypes[otype], source);
		return -1;
	}

	if (otype == OVER_TYPE_RPM) {
		display(PFS, "%s: Update Overlay %s in %s\n", mic->name, source, mic->config.name);
		mpss_update_config(&mpssenv, mic->config.name, NULL, NULL, "Overlay %s %s %s\n",
				   overtypes[otype], source, overstates[ostate]);
	} else {
		display(PFS, "%s: Update Overlay %s in %s\n", mic->name, source, mic->config.name);
		mpss_update_config(&mpssenv, mic->config.name, NULL, NULL, "Overlay %s %s %s %s\n",
				   overtypes[otype], source, target, overstates[ostate]);
	}

	return 0;
}

int
overlay(struct mic_info *miclist, char *type, char *source, char *target, char *state)
{
	struct mic_info *mic;
	int otype;
	int ostate;

	if (!strcasecmp(type, "simple")) {
		otype = OVER_TYPE_SIMPLE;
	} else if (!strcasecmp(type, "filelist")) {
		otype = OVER_TYPE_FILELIST;
	} else if (!strcasecmp(type, "file")) {
		otype = OVER_TYPE_FILE;
	} else if (!strcasecmp(type, "rpm")) {
		otype = OVER_TYPE_RPM;
	} else {
		display(PERROR, "Overlay type %s not valid\n", type);
		EXIT_ERROR(EINVAL);
	}

	if (source == NULL) {
		display(PERROR, "Command paramter 'source' must be set\n");
		EXIT_ERROR(EINVAL);
	}

	if ((otype != OVER_TYPE_RPM) && (target == NULL)) {
		display(PERROR, "Command paramter 'target' must be set\n");
		EXIT_ERROR(EINVAL);
	}

	if (state == NULL) {
		display(PERROR, "Command paramter 'state' must be set\n");
		EXIT_ERROR(EINVAL);
	}

	if (!strcasecmp(state, "on")) {
		ostate = TRUE;
	} else if (!strcasecmp(state, "off")) {
		ostate = FALSE;
	} else if (!strcasecmp(state, "delete")) {
		ostate = NOTSET;
	} else {
		display(PERROR, "Overlay state '%s' not valid\n", state);
		EXIT_ERROR(EINVAL);
	}

	for (mic = miclist; mic != NULL; mic = mic->next) {
		if (micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO)) {
			display(PWARN, "%s: Not configured - skipping\n", mic->name);
			continue;
		}

		mic_overlay(mic, otype, source, target, ostate);
	}

	return 0;
}

void
show_rpmdirs(struct mic_info *miclist)
{
	struct mic_info *mic;

	putchar('\n');

	for (mic = miclist; mic != NULL; mic = mic->next) {
		if (micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO)) {
			display(PWARN, "%s: Not configured - skipping\n", mic->name);
			continue;
		}

		if (mic->config.filesrc.k1om_rpms)
			printf("%s:\tK1om RPM location %s\n", mic->name, mic->config.filesrc.k1om_rpms);
		else
			printf("%s:\tK1om RPM location NOT SET\n", mic->name);
	}

	putchar('\n');
	exit(0);
}

void
setrpmdir(struct mic_info *miclist, char *dir)
{
	struct mic_info *mic;
	char rpmpath[PATH_MAX];
	int err;

	for (mic = miclist; mic != NULL; mic = mic->next) {
		if (micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO)) {
			display(PWARN, "%s: Not configured - skipping\n", mic->name);
			continue;
		}

		if (dir[strlen(dir)-1] == '/') {
			dir[strlen(dir)-1] = '\0';
		}

		if (!mpssut_filename(&mpssenv, NULL, rpmpath, PATH_MAX, "%s", dir))
			display(PWARN, "%s: K1omRpms directory %s does not exist.\n", mic->name, rpmpath);

		if (mic->config.filesrc.k1om_rpms && !strcmp(mic->config.filesrc.k1om_rpms, rpmpath))
			display(PINFO, "%s: K1omRpms parameter %s not changed\n", mic->name, rpmpath);
		else
			if ((err=mpss_update_config(&mpssenv, mic->config.name, "K1omRpms", NULL, "K1omRpms %s\n", rpmpath)))
				display(PERROR, "Failed to open config file '%s': %d\n", mic->config.name, err);
			else
				display(PFS, "%s: Update K1omRpms in %s\n", mic->name, mic->config.name);
	}

	exit(0);
}
