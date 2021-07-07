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
#include <stdarg.h>
#include <errno.h>
#include <sys/types.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "micctrl.h"

/*
 * Create the list of mic cards in the system.  If no cards are given on the command line
 * it calls the from_probe() function to get the list.  Otherwise it builds a list of
 * the cards from the command line list.
 */
struct mic_info *
create_miclist(struct mpss_env *menv, int argc, char *argv[], int optind, int *miccnt)
{
	struct mic_info *miclist;
	struct mic_info *mic;
	char sysfsdir[PATH_MAX];
	char tempname[16]; // pretty round number - enough for storing mic name with id value of max integer
	struct stat sbuf;
	int cnt = 0;


	// If no card list is specified then use all the cards.  This requires the mic.ko
	// driver to be loaded.
	if (optind == argc) {
		miclist = mpss_get_miclist(menv, &cnt);
		goto listdone;
	}

	if ((miclist = (struct mic_info *) malloc(sizeof(struct mic_info))) == NULL) {
		fprintf(stderr, "Internal memory allocation failure\n");
		exit(0x1010);
	}

	mic = miclist;
	memset(mic, 0, sizeof(struct mic_info));

	for (; optind < argc; optind++) {
		sscanf(argv[optind], "mic%d", &mic->id);
		snprintf(tempname, sizeof(tempname), "mic%d", mic->id);
		tempname[sizeof(tempname) - 1] = '\0';

		if ((mic->id < 0) || (mic->id > 255) || (strcmp(tempname, argv[optind]))) {
			printf("\n\tError: Invalid device name \"%s\"\n", argv[optind]);
			printf("\tInfo:  Valid device name is composed from two parts: \"mic\" and number from 0 - 255\n");
			exit(0x1002);
		}

		mic->present = FALSE;
		mic->name = mpssut_alloc_fill(tempname);
		mpss_clear_config(&mic->config);
		mic->config.name = mpssut_configname(menv, mic->name);
		mic->config.dname = mpssut_configname(menv, "default");

		snprintf(sysfsdir, PATH_MAX, "%s/%s", MICSYSFSDIR, mic->name);
		if (menv->live_update && (stat(sysfsdir, &sbuf) == 0))
			mic->present = TRUE;

		cnt++;

		if ((optind + 1) == argc) {
			mic->next = NULL;
		} else {
			if ((mic->next = (struct mic_info *) malloc(sizeof(struct mic_info))) == NULL) {
				fprintf(stderr, "Internal memory allocation failure\n");
				exit(0x1010);
			}

			mic = mic->next;
			memset(mic, 0, sizeof(struct mic_info));
		}
	}

listdone:
	if (cnt == 0) {
		display(PWARN, "No Mic cards found or specified on command line\n");
		exit(0);
	}

	if (miccnt != NULL) {
		*miccnt = cnt;
	}

	return miclist;
}

int
micctrl_parse_config(struct mpss_env *menv, struct mic_info *mic,
		     struct mbridge **brlist, struct mpss_elist *perrs, int errlev)
{
	int err = mpss_parse_config(menv, mic, brlist, perrs);
	int errors = perrs->ecount[PERROR];

	if ((errlev < PERROR) || (errlev > PINFO))
		errlev = PINFO;

	mpss_print_elist(perrs, errlev, display);
	mpss_clear_elist(&mpssperr);

	switch (err) {
	case 0:
		if (errors)
			return MIC_PARSE_ERRORS;

		return MIC_PARSE_SUCCESS;

	default:
		return MIC_PARSE_FAIL;
	}

	return MIC_PARSE_EMPTY;
}

char *
micctrl_check_nfs_rootdev(struct mic_info *mic)
{
	char *hostdir = NULL;

	if ((mic->config.rootdev.type == ROOT_TYPE_NFS) ||
	    (mic->config.rootdev.type == ROOT_TYPE_SPLITNFS)) {
		if ((hostdir = strchr(mic->config.rootdev.target, ':')) == NULL)
			display(PERROR, "%s: NFS root malformed target '%s'\n", mic->name,  mic->config.rootdev.target);
		else
			hostdir++;
	}

	return hostdir;
}

#define SERROR	"  \033[31m[Error]\033[0m "
#define SWARN	"\033[33m[Warning]\033[0m "
#define SNORM	"          "
#define SINFO	"   \033[32m[Info]\033[0m "
#define SFS	"\033[34m[Filesys]\033[0m "
#define SNET	"\033[35m[Network]\033[0m "

char *clev[] = {SERROR, SWARN, SNORM, SINFO, SFS, SNET};
unsigned char display_level = PNORM;

void
display(unsigned char level, char *format, ...)
{
	va_list args;
	char buffer[4096];

	if ((level > display_level) || (level > PNET))
		return;

	va_start(args, format);
	vsnprintf(buffer, 4096, format, args);
	va_end(args);

	printf("%s", clev[level]);
	printf("%s", buffer);
	fflush(stdout);
}

char *dis_indent = "          ";
char *
display_indent(void)
{
	return dis_indent;
}

void
bump_display_level(void)
{
	display_level++;
}

/*
 * Used by routines that must run under root ID.
 */
void
check_rootid(void)
{
	uid_t euid = geteuid();
	char permfile[PATH_MAX];
	struct passwd *pass;
	struct stat sbuf;
	char *tmpname;
	uid_t nobodyid = 0;
	int fd;

	if (euid != 0) {
		display(PERROR, "Only root is allowed to use this option\n");
		exit(EPERM);
	}

	// If destdir is set then we need to make sure files get created with the correct user id,  This
	// should prevent the root_squash NFS problem.
	if (mpssenv.destdir == NULL)
		return;

	mpssut_filename(&mpssenv, NULL, permfile, PATH_MAX, "/permfile");
	tmpname = mpssut_tempnam(permfile);

	if ((fd = open(tmpname, O_RDWR | O_CREAT)) < 0) {
		free(tmpname);
		display(PERROR, "Failed permissions test - cannot determine if %s is secure\n", mpssenv.destdir);
		exit(errno);
	}

	fstat(fd, &sbuf);
	close(fd);
	unlink(tmpname);
	free(tmpname);

	if ((sbuf.st_uid == 0) && (sbuf.st_gid == 0)) {
		// Change working directory to DESTDIR.  This will keep any mount present with micctrl runs.
		if (chdir(mpssenv.destdir) < 0) {
			display(PERROR, "Failed chdir to %s: %s\n", mpssenv.destdir, strerror(errno));
			exit(errno);
		}

		return;
	}

	while ((pass = getpwent()) != NULL) {
		if (!strcmp(pass->pw_name, "nfsnobody")) {
			nobodyid = pass->pw_uid;
			break;
		}
	}

	endpwent();

	if (nobodyid && (nobodyid == sbuf.st_uid))
		display(PERROR, "Files created under %s result in the owner 'nfsnobody' - Ensure this directory "
				"in not a NFS mount with 'root_squash' set.\n", mpssenv.destdir);
	else
		display(PERROR, "Files created under %s do not result in the owner 'root'.  Cannot proceed\n",
				mpssenv.destdir);

	exit(1);
}

/*
 * Should never get here
 */
void
memory_alloc_error(void)
{
	fprintf(stderr, "Internal memory allocation failure\n");
	exit(0x1010);
}

