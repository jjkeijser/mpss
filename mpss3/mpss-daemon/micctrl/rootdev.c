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

void rootdev(struct mic_info *miclist, int miccnt, char *type, char *target, char *nfsusr, char *server, int options);
void addnfs(struct mic_info *miclist, char *expdir, char *mountdir, char *server, char *nfs_opt);
void remnfs(struct mic_info *miclist, char *mountdir);
void updatenfs(struct mic_info *miclist);
void updateusr(struct mic_info *miclist);
void updateramfs(struct mic_info *miclist);

#define ROOTDEV_CREATE 0x1
#define ROOTDEV_DELETE 0x2

void delete_oldroot(struct mic_info *mic);
void set_ramfsroot(struct mic_info *mic, int type, char *target);
void set_nfsroot(struct mic_info *mic, int type, char *target, char *usr, char *server, int create);

char *descriptor = "# Root device for MIC card";

/*
 * Section 1: Top level parse routines for micctrl root device handling
 */

/*
 * micctrl --rootdev=<type> [--target=<targetname>] [--usr=<usrname>] [--create] [--delete] [MIC list]
 *
 * Set the root device information for th list of MIC cards.
 */
char *rootdev_help = {
"\
micctrl [global options] --rootdev=<type> [sub options] [List of Xeon Phi cards]\n\
\n\
The 'micctrl --rootdev' command modifies the RootDev parameter in the Xeon Phi\n\
configuration files. The RootDev parameter describes to the mpssd daemon and\n\
'micctrl -b' utilities how to provide the root file system to the booting card.\n\
The root file system may be of two types: ram disk or NFS mount.\n\
The 'micctrl --rootdev' command allows the system administrator to specify a\n\
configuration for the Xeon Phi card's root device. The root device may be of\n\
physical types: ram disk or NFS export.\n\
\n\
The rootdev option is called with on optional 'type' specifier and if not\n\
specified it prints the current configuration. Otherwise it is set one of the\n\
following strings:\n\
\n\
        RamFS:        Use a dynamically built ram disk image built at boot time\n\
                      from the files specified by the Base, CommonDir, Micdir\n\
                      and overlay configuration parameters.\n\
\n\
        StaticRamFS:  Use a statically built ram disk image. The image will not\n\
                      be built at boot time but provided by the system\n\
                      administrator or it may created by micctrl from the Base,\n\
                      CommonDir, MicDir and Overlay parameters with the\n\
                      --updateramfs main command. If a static ram file system\n\
                      is used, the card will boot more than a second faster by\n\
                      avoiding the file system generation phase.\n\
\n\
        NFS:          Mount a NFS export as the root partition. The booting card\n\
                      will be passed network setup information and will ensure\n\
                      the network is enabled before mounting the export.\n\
\n\
        SplitNFS:     The root file system is mounted the same as the NFS\n\
                      option. The /usr directory is expected to be a separate\n\
                      NFS export in the cards /etc/fstab and be shared between\n\
                      multiple Xeon Phi cards.\n\
\n\
        PersistentFS: Use a persistent storage filesystem stored in a file\n\
		      on the host. All reads and writes performed on the card\n\
		      are handled by the block device and transferred over PCIe\n\
		      to the host, where actual file read/write operations are\n\
		      performed. All changes into the card filesystem\n\
		      are preserved between card reboot.\n\
\n"
COMMON_GLOBAL
COMMON_HELP
VARDIR_HELP
"\
    -t <target>\n\
    --target=<target>\n\
\n\
        The --target option provides the location of the file system to be\n\
        used for the root device during boot of the Xeon Phi cards. Its value\n\
        ordefault value is dependent on the file system type.\n\
\n\
        If the file system type is set to 'ramfs' the --target option will\n\
        specify the image file created from the Base, MicDir, CommonDir and\n\
        Overlay at boot time. If not --target is specified the default file is\n\
        set to to <vardir>/<micID>.image.gz where micID is the name of the Xeon\n\
        Phi card instance (i.e. mic0, mic1, etc.).\n\
\n\
        If the file system type is set to 'staticramfs' the --target option will\n\
        specify a compressed CPIO file provided to the MPSS system. If no\n\
        --target is provided the same rules for the file name as in the 'ramfs'\n\
        above applies.\n\
\n\
        IF the file system type is set to 'nfs' or 'splitnfs' the --target\n\
        options specifies the NFS export to mount as the root file system. It\n\
        must be a valid NFS mount string in the format '<hostname>:<directory>'.\n\
        If the 'hostname' portion is omitted it will use the IP address of the\n\
        host. If the --target option is omitted it will be set to the string\n\
        '<hostIP>:<vardir>/<micID>.export.\n\
\n\
    -u </usr location>\n\
    --usr=</usr location>\n\
\n\
        If the root type is 'splitnfs' the --usr option specifies the location\n\
        of the export to be mounted as the /usr directory on the Xeon Phi cards.\n\
        If another root type is specified it has not effect.\n\
\n\
        If no --usr is specified '<hostIP>:<vardir>/usr.export will be used as\n\
        the default location.\n\
\n\
    -c\n\
    --create\n\
\n\
        If --create is specified and the type is changed to NFS the specified\n\
        'target' export directory will be created from the current Base,\n\
        CommonDir, Overlays and MicDir parameters. It creates them on the\n\
        current host system and will not attempt to copy them to another\n\
        'hostIP'. It will not add it to the hosts exports list and this must\n\
        be done by the system administrator.\n\
\n\
        If the --create option is not specified the file system may be created\n\
        later using the --updatenfs option. If splitnfs was specified the /usr\n\
        mount may be created using the --updateusr micctrl option.\n\
\n\
    -d\n\
    --delete\n\
\n\
        The --delete option removes the old files associated with the change.\n\
        If the old version is the default installed by the MPSS RPMs it will not\n\
        be removed.\n\
\n\
    -s <server>\n\
    --server=<server>\n\
\n\
        Since the --target option supports the standard NFS mount specification\n\
        the --server option is not longer needed and has been deprecated. It is\n\
        only present for backwards compatibility.\n\
\n"
};

static struct option rootdev_longopts[] = {
	{"vardir", required_argument, NULL, 0x1001},
	{"target", required_argument, NULL, 't'},
	{"usr", required_argument, NULL, 'u'},
	{"create", no_argument, NULL, 'c'},
	{"delete", no_argument, NULL, 'd'},
	{"server", required_argument, NULL, 's'},
	{"help", no_argument, NULL, 'h'},
	{0}
};

void
parse_rootdev_args(char *type, int argc, char *argv[])
{
	struct mic_info *miclist;
	int miccnt;
	int longidx;
	int c;
	int err;
	int options = 0;
	char *target = NULL;
	char *nfsusr = NULL;
	char *server = NULL;

	while ((c = getopt_long(argc, argv, "hcdvt:u:s:", rootdev_longopts, &longidx)) != -1) {
		switch(c) {
		case 'h':
			micctrl_help(rootdev_help);
			exit(0);
			break;

		case 0x1001:
			if ((err = mpssenv_set_vardir(&mpssenv, optarg))) {
				display(PERROR, "Unable to set vardir %s: %s\n", optarg,
						mpssenv_errstr(err));
				exit(EEXIST);
			}

			break;

		case 't':
			target = optarg;
			break;

		case 's':
			server = optarg;
			break;

		case 'u':
			nfsusr = optarg;
			break;

		case 'c':
			options |= ROOTDEV_CREATE;
			break;

		case 'd':
			options |= ROOTDEV_DELETE;
			break;

		case 'v':
			bump_display_level();
			break;

		default:
			ARGERROR_EXIT(rootdev_help);
		}
	}

	check_rootid();
	miclist = create_miclist(&mpssenv, argc, argv, optind, &miccnt);
	rootdev(miclist, miccnt, type, target, nfsusr, server, options);
}

/*
 * micctrl --addnfs=<nfs export> --dir=<mount dir> [MIC list]
 *
 * Add a NFS mount point to the /etc/fstab in the list of MIC cards.
 */
char *addnfs_help =
"\
micctrl [global options] --addnfs=<NFS export> [sub options] [Xeon Phi cards]\n\
\n\
The 'micctrl --addnfs' command adds the specified NFS export to the Xeon Phi\n\
card's /etc/fstab file. 'NFS export' indicates the export to be mounted. It\n\
should be in the format '<hostname>:<export directory>'.\n\
\n\
If the 'hostname' portion of the NFS export is not specified the IP address of\n\
the host will be added.\n\
\n\
There is no configuration parameter associated with the --addnfs command.\n\
\n"
COMMON_GLOBAL
COMMON_HELP
"\
    -d <mountdir>\n\
    --dir=<mountdir>\n\
\n\
        The export will be mounted on the 'mountdir' directory. The micctrl\n\
        utility also ensures the mount directory is created on the cards file\n\
        system image.\n\
\n\
    -o <optionlist>\n\
    --options=<optionlist>\n\
\n\
        The --options option specifes the list of options for an NFS mount. It\n\
        must be a comma separated list in the format the /etc/fstab needs in\n\
        it fs_mntops field. For particular mount options consult the NFS(5) man\n\
        page.\n\
    -s <server>\n\
    --server=<server>\n\
\n\
        Since the --addnfs option supports the standard NFS mount specification\n\
        the --server option is not longer needed and has been deprecated. It is\n\
        only present for backwards compatibility.\n\
\n";

static struct option addnfs_longopts[] = {
	{"dir", required_argument, NULL, 'd'},
	{"server", required_argument, NULL, 's'},
	{"options", required_argument, NULL, 'o'},
	{"help", no_argument, NULL, 'h'},
	{0}
};

void
parse_addnfs_args(char *expdir, int argc, char *argv[])
{
	struct mic_info *miclist;
	int longidx;
	int c;
	char *mountdir = NULL;
	char *server = NULL;
	char *nfs_opt = NULL;

	while ((c = getopt_long(argc, argv, "hvd:s:o:", addnfs_longopts, &longidx)) != -1) {
		switch(c) {
		case 'h':
			micctrl_help(addnfs_help);
			exit(0);

		case 'd':
			mountdir = mpssut_tilde(&mpssenv, optarg);
			break;

		case 's':
			server = optarg;
			break;

		case 'o':
			nfs_opt = optarg;
			break;

		case 'v':
			bump_display_level();
			break;

		default:
			ARGERROR_EXIT(addnfs_help);
		}
	}

	check_rootid();
	miclist = create_miclist(&mpssenv, argc, argv, optind, NULL);
	addnfs(miclist, expdir, mountdir, server, nfs_opt);
}

/*
 * micctrl --remnfs=<mount dir> [MIC list]
 *
 * Remove a NFS mount point to the /etc/fstab in the list of MIC cards.
 *
 * It should be noted the code does not remove the mount directory as we cannot tell if this
 * is what an administrator would actually want.
 */
char *remnfs_help =
"\
micctrl [global options] --remnfs=<mountdir> [sub options] [Xeon Phi cards]\n\
\n\
The 'micctrl --remnfs' command removes a NFS mount specification from the Xeon\n\
Phi card's /etc/fstab file. The 'mountdir' value is the directory name the mount\n\
is for and must be a full directory path.\n\
\n"
COMMON_GLOBAL
COMMON_HELP;

static struct option remnfs_longopts[] = {
	{"help", no_argument, NULL, 'h'},
	{0}
};

void
parse_remnfs_args(char *mountdir, int argc, char *argv[])
{
	struct mic_info *miclist;
	int longidx;
	int c;

	while ((c = getopt_long(argc, argv, "hv", remnfs_longopts, &longidx)) != -1) {
		switch(c) {
		case 'h':
			micctrl_help(remnfs_help);
			exit(0);

		case 'v':
			bump_display_level();
			break;

		default:
			ARGERROR_EXIT(remnfs_help);
		}
	}

	check_rootid();
	miclist = create_miclist(&mpssenv, argc, argv, optind, NULL);
	remnfs(miclist, mountdir);
}

/*
 * This function parses command line for 3 different commands:
 *
 * micctrl --updatenfs [MIC list]
 *
 * If the root device is NFS based the update the information on the nfs exports for the MIC
 * card list.  This uses all the files in BaseDir, CommonDir, MicDir and all Overlays.  Only
 * copies a file if it currently does not exist.
 *
 * micctrl --updateusr [MIC list]
 *
 * If the root device is SplitNFS type then this uses the above information to update the common
 * /usr mount.
 *
 * micctrl --updateusr [MIC list]
 *
 * IF the the root device is type RamFS or StaticRamFS for the update of the ram file system image
 * file.  Really only usefull for StaticRamFS.
 */
char *update_help[] = {
/* Updatenfs Help */
"\
micctrl [global options] --updatenfs [sub options] [Xeon Phi cards]\n\
\n\
If the RootDev parameter indicates the root device is a NFS mount the\n\
--updatenfs command is used to update it with the files provided by the Base,\n\
CommonDir, Overlay and MicDir parameters. This provides a mechanism for a new\n\
release to update the NFS export for the root file system with the files in the\n\
updated init ram disk image.\n\
\n\
If the export directory does not exist it creates it and fills it in from the same parameters.\n\
\n"
COMMON_GLOBAL
COMMON_HELP,
/* Updateusr Help */
"\
micctrl [global options] --updateusr [sub options] [Xeon Phi cards]\n\
\n\
If the RootDev parameter indicates the root device is a split NFS mount the\n\
--updateusr command is used to update the /usr shared mount with the files\n\
provided by the Base, CommonDir, Overlay and MicDir parameters. This provides\n\
a mechanism for a new release to update the /usr NFS export for Xeon Phi cards.\n\
\n\
If the /usr export directory does not exist it creates it and fills it in from\n\
the same parameters.\n\
\n"
COMMON_GLOBAL
COMMON_HELP,
/* Updateramfs Help */
"\
micctrl [global options] --updateramfs [sub options] [Xeon Phi cards]\n\
\n\
If the RootDev parameter indicates the root device is a is ramfs the\n\
--updateramfs command is used to recreate the init ram disk image for the Xeon\n\
Phi cards. In reality it is really only useful if the static ramfs option was\n\
chosen.\n\
\n\
If the export directory does not exist it creates it and fills it in from the same parameters.\n\
\n"
COMMON_GLOBAL
COMMON_HELP
};

static struct option updatenfs_longopts[] = {
	{"help", no_argument, NULL, 'h'},
	{0}
};

void
parse_update_args(int mode, int argc, char *argv[])
{
	struct mic_info *miclist;
	int longidx;
	int c;

	while ((c = getopt_long(argc, argv, "hv", updatenfs_longopts, &longidx)) != -1) {
		switch(c) {
		case 'h':
			micctrl_help(update_help[mode]);
			exit(0);

		case 'v':
			bump_display_level();
			break;

		default:
			ARGERROR_EXIT(update_help[mode]);
		}
	}

	check_rootid();
	miclist = create_miclist(&mpssenv, argc, argv, optind, NULL);

	switch (mode) {
	case UPDATE_NFS:
		updatenfs(miclist);
	case UPDATE_USR:
		updateusr(miclist);
	case UPDATE_RAM:
		updateramfs(miclist);
	}
}

/*/
 * Section 2: Worker functions for file system manipulation
 */
char *root_types[] = {"ramfs", "staticramfs", "nfs", "splitnfs"};

void report_rootdevs(struct mic_info *miclist);
void fstab_add(char *id, char *base, char *edir, char *mdir, char *server, char *nfs_opt);
int fstab_remove(char *id, char *base, char *dir);
int fstab_check(char *base, char *dir);
void fstab_delusr(char *base, struct mic_info *mic);
void add_dir_list(char *dir);
int nfs_opt_check(char *nfs_opt);

/*
 * Set the root device to the indicated type.  Types may be RamFs, StaticRamFS, NFS,
 * SplitNFS or InitRD.
 *
 * The target indicates where the root device information is stored.  The ram filesystem
 * options specify the location of the compressed cpio file for download.  The NFS options
 * specify the NFS export directory.  InitRD does not have a target.
 *
 * If the type was SplitNFS then nfsusr is the location of the NFS export for the shared /usr
 * mount point.
 */
void
rootdev(struct mic_info *miclist, int miccnt, char *type, char *target, char *usr, char *server, int options)
{
	struct mic_info *mic;
	char *hostdir = NULL;

	if (type == NULL) {
		report_rootdevs(miclist);
	}

	if (target != NULL) {
		if (miccnt != 1) {
			display(PERROR, "Cannot specify a target location if not specifying a single card\n");
			exit(1);
		}

		if ((hostdir = strchr(target, ':')) == NULL) {
			hostdir = target;
		} else {
			if (server != NULL) {
				display(PWARN, "Server option '%s' ignored for target '%s'\n", server, target);
			}

			server = target;
			*hostdir = '\0';
			hostdir++;
		}

		if (*hostdir != '/') {
			display(PERROR, "Target '%s' location must start from the '/' directory\n", hostdir);
			exit(1);
		}
	}

	mic = miclist;
	while (mic != NULL) {
		micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO);

		if (!strcmp(type, root_types[mic->config.rootdev.type])) {
			display(PNORM, "%s: Requested root type is same as current.\n", mic->name);
			mic = mic->next;
			continue;
		}

		if (options & ROOTDEV_DELETE)
			delete_oldroot(mic);

		mpss_remove_config(&mpssenv, mic, descriptor);
		mpss_remove_config(&mpssenv, mic, "RootDevice");

		if (mic->config.rootdev.type == ROOT_TYPE_SPLITNFS) {
			if (mic->config.filesrc.mic.dir) {
				fstab_remove(mic->name, mic->config.filesrc.mic.dir, "/usr");
				fstab_delusr(mic->config.filesrc.mic.dir, mic);
			}

			if (mic->config.rootdev.target) {
				fstab_remove(mic->name, mic->config.rootdev.target, "/usr");
				fstab_delusr(mic->config.rootdev.target, mic);
			}
		}

		if (mic->config.rootdev.target) {
			free(mic->config.rootdev.target);
			mic->config.rootdev.target = NULL;
		}

		if (!strcasecmp(type, "ramfs")) {
			set_ramfsroot(mic, ROOT_TYPE_RAMFS, target);
		} else if (!strcasecmp(type, "staticramfs")) {
			set_ramfsroot(mic, ROOT_TYPE_STATICRAMFS, target);
		} else if (!strcasecmp(type, "nfs")) {
			set_nfsroot(mic, ROOT_TYPE_NFS, hostdir, NULL, server, options & ROOTDEV_CREATE);
		} else if (!strcasecmp(type, "splitnfs")) {
			if (mic->config.filesrc.mic.dir == NULL) {
				display(PERROR, "%s: MicDir not set, cannot add /usr fstab entry\n",
						mic->name);
				goto nextmic;
			}

			set_nfsroot(mic, ROOT_TYPE_SPLITNFS, hostdir, usr, server, options & ROOTDEV_CREATE);
		} else {
			display(PERROR, "Uknown root device type '%s'\n", type);
		}

nextmic:
		mic = mic->next;
	}

	exit(0);
}

/*
 * report_rootdev and report_rootdevs are subfuntions to rootdev().  If type is not specified
 * then print a list of the root device information for the specified MIC cards.
 *
 * The report_rootdev function must not be declared static because it is used by the config
 * function in micctrl.c
 */
static char *ostrings[] = {"Simple", "Filelist", "File", "RPM"};

void
report_rootdev(struct mic_info *mic)
{
	struct moverdir *dir;

	switch(mic->config.rootdev.type) {
	case ROOT_TYPE_RAMFS:
		printf("Root Device:   Dynamic Ram Filesystem %s from:\n", mic->config.rootdev.target);

		if ((mic->config.filesrc.base.type == SRCTYPE_CPIO) && mic->config.filesrc.base.image)
			printf("\tBase:      CPIO %s\n", mic->config.filesrc.base.image);
		else if ((mic->config.filesrc.base.type == SRCTYPE_DIR) && mic->config.filesrc.base.dir)
			printf("\tBase:      DIR %s\n", mic->config.filesrc.base.dir);
		else
			printf("\tBase:      Not initialized - this MIC card will not boot\n");

		dir = mic->config.filesrc.overlays.next;
		while(dir) {
			if (dir->type != OVER_TYPE_RPM)
				printf("\tOverlay:   %s %s %s %s\n", ostrings[dir->type], dir->dir,
					dir->target, dir->state? "on" : "off");
			else
				printf("\tOverlay:   %s %s %s\n", ostrings[dir->type], dir->dir,
					dir->state? "on" : "off");
			dir = dir->next;
		}

		if (mic->config.filesrc.common.type == SRCTYPE_FILELIST)
			printf("\tCommonDir: Filelist %s\n", mic->config.filesrc.common.list);
		else if (mic->config.filesrc.common.type == SRCTYPE_DIR)
			printf("\tCommonDir: Directory %s\n", mic->config.filesrc.common.dir);
		else
			printf("\tCommonDir: Not initialized - this MIC card will not boot\n");

		if (mic->config.filesrc.mic.type == SRCTYPE_FILELIST)
			printf("\tMicdir:    Filelist %s\n", mic->config.filesrc.mic.list);
		else if (mic->config.filesrc.mic.type == SRCTYPE_DIR)
			printf("\tMicdir:    Directory %s\n", mic->config.filesrc.mic.dir);
		else
			printf("\tMicdir:    Not initialized - this MIC card will not boot\n");

		break;

	case ROOT_TYPE_STATICRAMFS:
		printf("Root Device:   Static Ram Filesystem %s\n", mic->config.rootdev.target);
		break;

	case ROOT_TYPE_NFS:
		printf("Root Device:   NFS from export host:%s\n", mic->config.rootdev.target);
		break;

	case ROOT_TYPE_SPLITNFS:
		printf("Root Device:   Split NFS\n");
		printf("\t/:    host:%s\n", mic->config.rootdev.target);
		printf("\t/usr: host:%s\n", mic->config.rootdev.nfsusr);
		break;

	case ROOT_TYPE_PFS:
		printf("Root Device:   Persistent Filesystem %s\n", mic->config.rootdev.target);
		printf("\tBase:		CPIO %s\n", mic->config.filesrc.base.image);
		break;
	}
}

void
report_rootdevs(struct mic_info *miclist)
{
	struct mic_info *mic;

	putchar('\n');
	mic = miclist;
	while (mic != NULL) {
		micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO);

		printf("%s ", mic->name);
		report_rootdev(mic);
		putchar('\n');
		mic = mic->next;
	}

	exit(0);
}

/*
 * Sub function doing the actual add of a NFS entry in /etc/fstab.
 */
void
fstab_add(char *id, char *base, char *edir, char *mdir, char *server, char *nfs_opt)
{
	char fstabname[PATH_MAX];
	FILE *fp;

	mpssut_filename(&mpssenv, NULL, fstabname, PATH_MAX, "%s/etc/fstab", base);
	if (fstab_check(base, mdir)) {
		display(PWARN, "Modified existing NFS entry for MIC card path '%s'\n", mdir);
		fstab_remove(id, base, mdir);
	}

	if ((fp = fopen(fstabname, "a+")) == NULL) {
		return;
	}

	display(PFS, "%s: Update %s with NFS mount %s:%s at %s\n", id, fstabname, server, edir, mdir);
	if (nfs_opt != NULL)
		fprintf(fp, "%s:%s\t%s\tnfs\t\t%s\t\t1 1\n", server, edir, mdir, nfs_opt);
	else
		fprintf(fp, "%s:%s\t%s\tnfs\t\tdefaults\t\t1 1\n", server, edir, mdir);

	fclose(fp);
}

/*
 * Check to see of an entry is already in /etc/fstab for a MIC card.
 */
int
fstab_check(char *base, char *dir)
{
	char fstabname[PATH_MAX];
	char line[256];
	char *start;
	char *end;
	char *dir_end;
	FILE *fp;


	mpssut_filename(&mpssenv, NULL, fstabname, PATH_MAX, "%s/etc/fstab", base);
	if ((fp = fopen(fstabname, "r")) == NULL) {
		return 0;
	}

	dir_end = dir + (strlen(dir) - 1);
	while (dir_end > dir && *dir_end == '/') {
		dir_end--;
	}

	while (fgets(line, 256, fp)) {
		start = line;
		while ((*start != ' ') && (*start != '\t') && (*start != '\n')) {
			if (*start == '\n')
				goto nextline;
			start++;
		}

		while (((*start == ' ') || (*start == '\t')) && (*start != '\n')) {
			if (*start == '\n')
				goto nextline;
			start++;
		}

		end = start;
		while ((*end != ' ') && (*end != '\t') && (*end != '\n')) {
			if (*end == '\n')
				goto nextline;
			end++;
		}

		if ((*end == ' ') || (*end == '\t')) {
			end--;
		}

		while (*end == '/') {
			end--;
		}

		if ((int)(dir_end - dir) != (int)(end - start)) {
			goto nextline;
		}

		if (strncmp(dir, start, (int)(end - start) + 1))
			goto nextline;

		fclose(fp);
		return 1;
nextline:
		start = line;
	}

	fclose(fp);
	return 0;
}

/*
 * Sub function doing the actual removal of a NFS entry in /etc/fstab.
 */
int
fstab_remove(char *id, char *base, char *dir)
{
	char ifstabname[PATH_MAX];
	char ofstabname[PATH_MAX];
	char line[256];
	char *start;
	char *end;
	char *dir_end;
	int found = FALSE;
	FILE *ifp;
	FILE *ofp;

	mpssut_filename(&mpssenv, NULL, ifstabname, PATH_MAX, "%s/etc/fstab", base);
	if ((ifp = fopen(ifstabname, "r")) == NULL) {
		return 1;
	}

	mpssut_filename(&mpssenv, NULL, ofstabname, PATH_MAX, "%s/etc/fstab.save", base);
	if ((ofp = fopen(ofstabname, "w")) == NULL) {
		fclose(ifp);
		return 1;
	}

	dir_end = dir + (strlen(dir) - 1);
	while (dir_end > dir && *dir_end == '/') {
		dir_end--;
	}

	while (fgets(line, 256, ifp)) {
		start = line;
		while ((*start != ' ') && (*start != '\t') && (*start != '\n')) {
			if (*start == '\n')
				goto nextline;
			start++;
		}

		while (((*start == ' ') || (*start == '\t')) && (*start != '\n')) {
			if (*start == '\n')
				goto nextline;
			start++;
		}

		end = start;
		while ((*end != ' ') && (*end != '\t') && (*end != '\n')) {
			if (*end == '\n')
				goto nextline;
			end++;
		}

		if ((*end == ' ') || (*end == '\t')) {
			end--;
		}

		while (*end == '/') {
			end--;
		}

		if ((int)(dir_end - dir) != (int)(end - start)) {
			goto nextline;
		}

		if (strncmp(dir, start, (int)(end - start) + 1)) {
			goto nextline;
		}

		found = TRUE;
		continue;
nextline:
		fprintf(ofp, "%s", line);
	}

	fclose(ifp);
	fclose(ofp);

	if (!found) {
		display(PERROR, "%s: %s NFS %s entry not found\n", id, ifstabname, dir);
		unlink(ofstabname);
		return 1;
	} else {
		display(PFS, "%s: Update %s remove NFS %s entry\n", id, ifstabname, dir);
		unlink(ifstabname);
		link(ofstabname, ifstabname);
		unlink(ofstabname);
		return 0;
	}
}

#define xstr(s) #s
#define str(s) xstr(s)

/*
 * Remove the /usr NFS mount from the /etc/fstab file for the MIC cards.
 */
void
fstab_delusr(char *base, struct mic_info *mic)
{
	char ifstabname[PATH_MAX];
	char ofstabname[PATH_MAX];
	char hoststring[PATH_MAX];
	char exp[PATH_MAX];
	char mount[PATH_MAX];
	char line[256];
	FILE *ifp;
	FILE *ofp;
	int cnt;

	mpssut_filename(&mpssenv, NULL, ofstabname, PATH_MAX, "%s/etc/fstab.tmp", base);
	if (mpssut_filename(&mpssenv, NULL, ifstabname, PATH_MAX, "%s/etc/fstab", base)) {
		snprintf(hoststring, PATH_MAX, "host:%s", mic->config.rootdev.nfsusr);
		if ((ifp = fopen(ifstabname, "r")) == NULL)
			return;

		if ((ofp = fopen(ofstabname, "w")) == NULL) {
			fclose(ifp);
			return;
		}

		while (fgets(line, 256, ifp)) {
			if ((cnt = sscanf(line, "%" str(PATH_MAX) "s %" str(PATH_MAX) "s",
										exp, mount)) < 2)
				continue;

			if (strcmp(mount, "/usr"))
				fprintf(ofp, "%s", line);
		}

		fclose(ofp);
		fclose(ifp);

		unlink(ifstabname);
		link(ofstabname, ifstabname);
		unlink(ofstabname);
	}
}

/*
 * Checks if NFS options for specified fstab entry are valid.
 */

#define NFS_EDUP 2
#define NFS_EINVAL 3
#define NFS_MAXOPT_COUNT 100

int nfs_opt_check(char *nfs_opt)
{

	char *const opt_list[] = {
		"rsize",
		"wsize",
		"timeo",
		"soft",
		"hard",
		"retrans",
		"ac",
		"noac",
		"acregmin",
		"acregmax",
		"acdirmin",
		"acdirmax",
		"actimeo",
		"bg",
		"fg",
		"retry",
		"sec",
		"sharecache",
		"nosharecache",
		"resvport",
		"noresvport",
		"lookupcache",
		"proto",
		"udp",
		"tcp",
		"port",
		"mountport",
		"mountproto",
		"mounthost",
		"mounthost",
		"namlen",
		"nfsvers",
		"vers",
		"lock",
		"nolock",
		"intr",
		"nointr",
		"cto",
		"nocto",
		"acl",
		"noacl",
		"rdirplus",
		"nordirplus",
		"clientaddr",
		"async",
		"atime",
		"noatime",
		"auto",
		"noauto",
		"context",
		"fscontext",
		"defcontext",
		"rootcontext",
		"defaults",
		"dev",
		"nodev",
		"diratime",
		"nodiratime",
		"dirsync",
		"exec",
		"noexec",
		"group",
		"iversion",
		"noiversion",
		"mand",
		"nomand",
		"_netdev",
		"nofail",
		"relatime",
		"norelatime",
		"strictatime",
		"suid",
		"nosuid",
		"owner",
		"remount",
		"ro",
		"_rnetdev",
		"rw",
		"sync",
		"user",
		"nouser",
		"users",
		NULL
	};

	int opt_count[NFS_MAXOPT_COUNT] = { 0 };
	char *subopts;
	char *value;
	int opt = 0;
	int ret = 0;

	if ((subopts = (char *) malloc(strlen(nfs_opt) + 1)) == NULL) {
		memory_alloc_error();
	}
	strcpy(subopts, nfs_opt);

	while (*subopts != '\0' && !ret) {
		opt = getsubopt(&subopts, opt_list, &value);
		if (opt == -1)
			ret = NFS_EINVAL;
		else {
			opt_count[opt] += 1;
			if (opt_count[opt] > 1)
				ret = NFS_EDUP;
		}
	}

	switch (ret) {
		case NFS_EDUP:
			display(PERROR, "Duplicate NFS option '%s' specified.\n", opt_list[opt]);
			break;
		case NFS_EINVAL:
			display(PERROR, "Unknown '%s' NFS option specified.\n", value);
			break;
	}

	return ret;
}

void add_nfsmount(struct mic_info *mic, char *target, char *server, char *mntdir, char *nfs_opt);

/*
 * Add an NFS mount point to the /etc/fstab in the list of MIC cards.
 *
 * The expdir is the name of the export from the server and mountdir is the mount
 * directory on the card side.
 *
 */
void
addnfs(struct mic_info *miclist, char *expdir, char *mountdir, char *server, char *nfs_opt)
{
	struct mic_info *mic;
	char *nfsdir;
	int ret = 0;

	if (expdir == NULL) {
		display(PERROR, "A NFS export directory must be specified\n");
		exit(1);
	}

	if ((nfsdir = strchr(expdir, ':')) == NULL) {
		nfsdir = expdir;
	} else {
		nfsdir++;
	}

	if (mountdir == NULL) {
		display(PERROR, "A directory to mount the NFS export must be specified\n");
		exit(1);
	}

	if (nfs_opt){
		ret = nfs_opt_check(nfs_opt);
		if (ret)
			exit(ret);
	}

	mic = miclist;
	while (mic != NULL) {
		add_nfsmount(mic, expdir, server, mountdir, nfs_opt);
		mic = mic->next;
	}

	exit(0);
}

int fill_nfsinfo(struct mic_info *mic, int type, char *string, char *server,
		 char **nfsserver, char **dir);

void
add_nfsmount(struct mic_info *mic, char *target, char *server, char *mountdir, char *nfs_opt)
{
	char buffer[PATH_MAX];
	char *nfsserver;
	char *nfsdir;
	char *hostdir;

	micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO);

	strncpy(buffer, target, PATH_MAX - 1);
	buffer[PATH_MAX - 1] = '\0';
	if (fill_nfsinfo(mic, 0, buffer, server, &nfsserver, &nfsdir))
		return;

	if (nfs_opt && nfs_opt_check(nfs_opt))
		return;

	fstab_add(mic->name, mic->config.filesrc.mic.dir, nfsdir, mountdir, nfsserver, nfs_opt);
	mpssut_mksubtree(&mpssenv, mic->config.filesrc.mic.dir, mountdir, 0, 0, 0755);

	if ((hostdir = micctrl_check_nfs_rootdev(mic)) != NULL) {
		fstab_add(mic->name, hostdir, nfsdir, mountdir, nfsserver, nfs_opt);
		mpssut_mksubtree(&mpssenv, hostdir, mountdir, 0, 0, 0755);
	}
}

/*
 * Remove a NFS mount from the /etc/fstab files on the list of MIC cards.
 *
 * mountdir is used to find the indicated mount point in the fstab file.  This codes
 * does not remove the directory name from the file system as it cannot tell if there are
 * other files in the directory being hid by the mount.
 */

void
remnfs(struct mic_info *miclist, char *mountdir)
{
	struct mic_info *mic;
	char *target;
	int errcnt = 0;
	int curerr;

	if (mountdir == NULL) {
		display(PERROR, "NFS mountdir must be specified\n");
		EXIT_ERROR(EEXIST);
	}

	mic = miclist;
	while (mic != NULL) {
		curerr = FALSE;
		micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO);
		if (fstab_remove(mic->name, mic->config.filesrc.mic.dir, mountdir)) {
			errcnt++;
			curerr = TRUE;
		} else {
			mpssut_rmsubtree(&mpssenv, mic->config.filesrc.mic.dir, mountdir);
		}

		if ((target = micctrl_check_nfs_rootdev(mic)) != NULL) {
			if (fstab_remove(mic->name, target, mountdir)) {
				if (curerr == FALSE)
					errcnt++;
			} else {
				mpssut_rmsubtree(&mpssenv, target, mountdir);
			}
		}
		mic = mic->next;
	}

	exit(errcnt);
}

/*
 * If the root file system is a NFS type then this will update the export directory based
 * on the information found in BaseDir, CommonDir, MicDir and all Overlays.
 */
void
updatenfs(struct mic_info *miclist)
{
	struct mic_info *mic;
	char tmpname[PATH_MAX];
	char *rootpath;

	mic = miclist;
	while (mic != NULL) {
		micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO);

		if ((rootpath = micctrl_check_nfs_rootdev(mic)) == NULL) {
			mic = mic->next;
			continue;
		}

		if (!mpssut_filename(&mpssenv, NULL, tmpname, PATH_MAX, "%s", rootpath)) {
			display(PINFO, "%s: New creation.  Do not forget to add '%s'"
			       "to the NFS exports file\n", mic->name, rootpath);
			mpssut_mktree(&mpssenv, rootpath, 0, 0, 0755);
		} else {
			display(PINFO, "Updating NFS root directory %s\n", rootpath);
		}

		mpssfs_gen_nfsdir(&mpssenv, mic, 0, &mpssperr);
		mpss_print_elist(&mpssperr, PINFO, display);
		mpss_clear_elist(&mpssperr);
		display(PFS, "%s: Created NFS dir %s\n", mic->name, rootpath);
		mic = mic->next;
	}

	exit(0);
}

/*
 * Add a directory during file system update.
 */
void
add_dir_list(char *dir)
{
	char dirdir[PATH_MAX];
	char dirsave[PATH_MAX];
	char destdir[PATH_MAX];
	struct stat sbuf;

	if (!strcmp(dir, "/"))
		return;

	if (stat(dir, &sbuf) == 0)
		return;

	if (strlen(dir) >= sizeof(dirsave))
		display(PERROR, "Warning: Truncating directory path %s\n", dir);
	strncpy(dirsave, dir, sizeof(dirsave) - 1);
	dirsave[sizeof(dirsave) - 1] = '\0';

	if (strlen(dirname(dir)) >= sizeof(dirdir))
		display(PERROR, "Warning: Truncating directory path %s\n", dirname(dir));
	strncpy(dirdir, dirname(dir), sizeof(dirdir) - 1);
	dirdir[sizeof(dirdir) - 1] = '\0';

	add_dir_list(dirdir);
	mpssut_filename(&mpssenv, NULL, destdir, PATH_MAX, "%s", dirsave);
	mkdir(destdir, 0755);
}

/*
 * If the root file system is Split NFS type then this will update the shared /usr directory
 * based on the information found in BaseDir, CommonDir, MicDir and all Overlays.
 */
void
updateusr(struct mic_info *miclist)
{
	struct mic_info *smic;
	struct mic_info *mic = NULL;
	char nfsusr[PATH_MAX];

	smic = miclist;
	while (smic != NULL) {
		micctrl_parse_config(&mpssenv, smic, &brlist, &mpssperr, PINFO);

		if (smic->config.rootdev.type != ROOT_TYPE_SPLITNFS) {
			smic = smic->next;
			continue;
		}

		mic = smic;
		break;
	}

	if (mic == NULL) {
		display(PERROR, "Error: updateusr did not find any MIC cards configured "
				"with SplitNFS\n");
		exit(1);
	}

	display(PNORM, "Updating /usr export based on %s\n", mic->name);

	if (!mpssut_filename(&mpssenv, NULL, nfsusr, PATH_MAX, "%s", strchr(mic->config.rootdev.nfsusr, ':') + 1)) {
		display(PNORM, "%s: Common /usr does not exist - creating %s\n", mic->name, nfsusr);

		mpssut_mktree(&mpssenv, nfsusr, 0, 0, 0755);
		mpssfs_gen_nfsdir(&mpssenv, mic, 1, &mpssperr);
		mpss_print_elist(&mpssperr, PINFO, display);
		mpss_clear_elist(&mpssperr);
		display(PFS, "%s: Created NFS /usr dir %s\n", mic->name, nfsusr);
	}

	exit(0);
}

/*
 * If the root file system is StaticRamFs type then this recreate the compressed cpio
 * image file used to download and create the ram file system based on the information
 * found in BaseDir, CommonDir, MicDir and all Overlays.
 */
void
updateramfs(struct mic_info *miclist)
{
	struct mic_info *mic = NULL;

	mic = miclist;
	while (mic != NULL) {
		micctrl_parse_config(&mpssenv, mic, &brlist, &mpssperr, PINFO);

		if ((mic->config.rootdev.type != ROOT_TYPE_RAMFS) &&
		    (mic->config.rootdev.type != ROOT_TYPE_STATICRAMFS)) {
			display(PERROR, "%s: RootDevice must be RamFS or StaticRamFS\n", mic->name);
		} else {
			display(PNORM, "%s: Generating static ram file system image %s\n",
			       mic->name, mic->config.rootdev.target);
			mpssfs_gen_initrd(&mpssenv, mic, &mpssperr);
			display(PFS, "%s: Created %s\n", mic->name, mic->config.rootdev.target);
			mpss_print_elist(&mpssperr, PINFO, display);
			mpss_clear_elist(&mpssperr);
		}

		mic = mic->next;
	}

	exit(0);
}

void
delete_oldroot(struct mic_info *mic)
{
	char command[PATH_MAX];
	char tmpdir[PATH_MAX];
	char filename[PATH_MAX];

	switch(mic->config.rootdev.type) {
	case ROOT_TYPE_RAMFS:
	case ROOT_TYPE_STATICRAMFS:
		if ((mic->config.rootdev.target != NULL) &&
		     mpssut_filename(&mpssenv, NULL, filename, PATH_MAX, "%s", mic->config.rootdev.target))
			unlink(filename);
		break;
	case ROOT_TYPE_SPLITNFS:
		if ((mic->config.rootdev.nfsusr) &&
		    mpssut_filename(&mpssenv, NULL, tmpdir, PATH_MAX, "%s",
					  strchr(mic->config.rootdev.nfsusr, ':') + 1)) {
			snprintf(command, PATH_MAX - 1, "rm -fr %s\n", tmpdir);
			system(command);
		}
		mic->config.rootdev.nfsusr = NULL;
	case ROOT_TYPE_NFS:
		if ((mic->config.rootdev.target) &&
		    mpssut_filename(&mpssenv, NULL, tmpdir, PATH_MAX, "%s",
					strchr(mic->config.rootdev.target, ':') + 1)) {
			snprintf(command, PATH_MAX - 1, "rm -fr %s\n", tmpdir);
			system(command);
		}

		mic->config.rootdev.target = NULL;
		break;
	case ROOT_TYPE_PFS:
		if ((mic->config.rootdev.target) &&
		    mpssut_filename(&mpssenv, NULL, tmpdir, PATH_MAX, "%s",mic->config.rootdev.target)) {
			snprintf(command, PATH_MAX - 1, "rm -fr %s\n", tmpdir);
			system(command);
		}
		break;
	}
}

char *fstypes[] = {"RamFS", "StaticRamFS", "NFS", "SplitNFS", "PFS"};

void
set_ramfsroot(struct mic_info *mic, int type, char *target)
{
	char targetname[PATH_MAX];

	mic->config.rootdev.type = type;
	if (target == NULL) {
		snprintf(targetname, PATH_MAX, "%s/%s.image.gz", mpssenv.vardir, mic->name);
		mpssut_realloc_fill(&mic->config.rootdev.target, targetname);
	} else {
		mpssut_realloc_fill(&mic->config.rootdev.target, target);
	}

	display(PFS, "%s: Update RootDevice in %s\n", mic->name, mic->config.name);
	mpss_update_config(&mpssenv, mic->config.name, NULL, descriptor, "%s %s %s\n\n",
			   "RootDevice", fstypes[type], mic->config.rootdev.target);
}

int
fill_nfsinfo(struct mic_info *mic, int type, char *mount, char *server, char **nfsserver, char **dir)
{
	struct mbridge *br = NULL;
	char *nserver = NULL;
	char *ndir;

	if ((ndir = strchr(mount, ':')) != NULL) {
		*ndir = '\0';
		ndir++;

		if (server != NULL) {
			if (!strcmp(mount, server)) {
				display(PERROR, "NFS server argument specifies a different server than "
						"the target argument (%s : %s)\n", server, ndir);
				return 1;
			}
		}

		switch (mic->config.net.type) {
		case NETWORK_TYPE_STATPAIR:
			display(PWARN, "%s: Server %s may not be reachable if the interface "
				       "is not routed out of the host\n",
				        mic->name, mount);
			break;

		case NETWORK_TYPE_STATBRIDGE:
			if ((br = mpss_bridge_byname(mic->config.net.bridge, brlist)) == NULL)
				return 1;

			if (br->type == BRIDGE_TYPE_INT)
				display(PWARN, "%s: Server %s may not be reachable if the internal "
					       "bridge %s is not routed out of the host\n",
					        mic->name, mount, br->name);

			break;

		case NETWORK_TYPE_BRIDGE:
			break;
		}

		nserver = mount;
	} else {
		ndir = mount;

		switch (mic->config.net.type) {
		case NETWORK_TYPE_STATPAIR:
			if (server != NULL) {
				display(PWARN, "%s: Server %s may not be reachable if the interface "
					       "is not routed out of the host\n",
					        mic->name, mount);
				nserver = server;
			} else {
				nserver = mic->config.net.hostIP;
			}
			break;

		case NETWORK_TYPE_STATBRIDGE:
			if ((br = mpss_bridge_byname(mic->config.net.bridge, brlist)) == NULL)
				return 1;

			if ((br->type == BRIDGE_TYPE_INT) && (server != NULL))
				display(PWARN, "%s: Server %s may not be reachable if the internal "
					       "bridge %s is not routed out of the host\n",
					        mic->name, server, br->name);

			nserver = server;
			break;

		case NETWORK_TYPE_BRIDGE:
			if (server == NULL) {
				display(PERROR, "%s: DHCP requires a server to be " "specified\n",
						mic->name);
				return 1;
			}

			nserver = server;
		}
	}

	*nfsserver = nserver;
	*dir = ndir;
	return 0;
}

void
set_nfsroot(struct mic_info *mic, int type, char *target, char *usr, char *server, int create)
{
	char tmptarget[PATH_MAX];
	char tmpusr[PATH_MAX];
	char tmpdir[PATH_MAX];
	char *nfsserver;
	char *nfsdir;
	char *usrserver;
	char *usrdir;
	struct stat sbuf;
	char *nfs_opt = NULL;

	if (target == NULL) {
		snprintf(tmptarget, PATH_MAX - 1, "%s/%s.export", mpssenv.vardir, mic->name);
		target = tmptarget;
	}

	if (fill_nfsinfo(mic, type, target, server, &nfsserver, &nfsdir))
		return;

	snprintf(tmpdir, PATH_MAX, "%s:%s", nfsserver, nfsdir);
	mic->config.rootdev.target = mpssut_alloc_fill(tmpdir);
	mic->config.rootdev.nfsusr = NULL;

	if (type == ROOT_TYPE_SPLITNFS) {
		if (usr == NULL) {
			snprintf(tmpusr, PATH_MAX - 1, "%s/usr.export", mpssenv.vardir);
			usr = tmpusr;
		}

		if (fill_nfsinfo(mic, type, usr, server, &usrserver, &usrdir))
			return;

		snprintf(tmpdir, PATH_MAX, "%s:%s", usrserver, usrdir);
		mpssut_realloc_fill(&mic->config.rootdev.nfsusr, tmpdir);
		mpssut_filename(&mpssenv, NULL, tmpdir, PATH_MAX, "%s", mic->config.filesrc.mic.dir);
		fstab_add(mic->name, tmpdir, usrdir, "/usr", nfsserver, nfs_opt);
		display(PFS, "%s: Update RootDevice in %s\n", mic->name, mic->config.name);
		mpss_update_config(&mpssenv, mic->config.name, NULL, descriptor, "%s %s %s %s\n\n",
				   "RootDevice", fstypes[type], mic->config.rootdev.target, mic->config.rootdev.nfsusr);
	} else {
		display(PFS, "%s: Update RootDevice in %s\n", mic->name, mic->config.name);
		mpss_update_config(&mpssenv, mic->config.name, NULL, descriptor, "%s %s %s\n\n",
				   "RootDevice", fstypes[type], mic->config.rootdev.target);
	}

	if (!create)
		return;

	if (mpssut_filename(&mpssenv, NULL, tmpdir, PATH_MAX, "%s", nfsdir))
		display(PINFO, "%s: Updating existing directory %s: %s\n", mic->name, nfsdir, strerror(errno));

	mpssut_mktree(&mpssenv, nfsdir, 0, 0, 0755);
	mpssfs_gen_nfsdir(&mpssenv, mic, FALSE, &mpssperr);
	mpss_print_elist(&mpssperr, PINFO, display);
	mpss_clear_elist(&mpssperr);
	display(PFS, "%s: Created NFS dir %s\n", mic->name, nfsdir);

	if ((type != ROOT_TYPE_SPLITNFS) || (stat(usrdir, &sbuf) == 0))
		return;

	display(PINFO, "%s: Common /usr does not exist - creating %s\n", mic->name, usrdir);

	if (mpssut_filename(&mpssenv, NULL, tmpdir, PATH_MAX, "%s", usrdir))
		display(PWARN, "%s: Updating existing directory %s: %s\n", mic->name, usrdir, strerror(errno));

	mpssut_mktree(&mpssenv, usrdir, 0, 0, 0755);
	mpssfs_gen_nfsdir(&mpssenv, mic, TRUE, &mpssperr);
	mpss_print_elist(&mpssperr, PINFO, display);
	mpss_clear_elist(&mpssperr);
	display(PFS, "%s: Created NFS /usr dir %s\n", mic->name, usrdir);
}

// TODO: Need to add actual NFS configuration back into this.  Taken out because it was broken.
void
set_rootdev(struct mic_info *mic)
{
	char *descriptor = "# Root device for MIC card";
	char value[1024];

	switch (mic->config.rootdev.type) {
	case ROOT_TYPE_RAMFS:
		display(PINFO, "%s: RootDevice RamFS %s\n", mic->name, mic->config.rootdev.target);
		return;

	case ROOT_TYPE_STATICRAMFS:
		display(PINFO, "%s: RootDevice StaticRamFS %s\n", mic->name, mic->config.rootdev.target);
		return;

	case ROOT_TYPE_NFS:
	case ROOT_TYPE_SPLITNFS:
		display(PINFO, "%s: RootDevice NFS %s: Ensure export is available\n",
				mic->name, mic->config.rootdev.target);
		return;
	case ROOT_TYPE_PFS:
		display(PINFO, "%s: RootDevice Persistent FS %s\n", mic->name, mic->config.rootdev.target);
		return;

	default:
		mic->config.rootdev.type = ROOT_TYPE_RAMFS;
		snprintf(value, 1024, "%s/%s.image.gz", mpssenv.vardir, mic->name);
		mic->config.rootdev.target = mpssut_alloc_fill(value);
		display(PFS, "%s: Update RootDevice in %s\n", mic->name, mic->config.name);
		mpss_update_config(&mpssenv, mic->config.name, NULL, descriptor, "%s %s %s\n\n",
				   "RootDevice", "Ramfs", value);
		display(PINFO, "%s: RootDevice RAMFS %s\n", mic->name, value);
		break;
	}

	return;
}

