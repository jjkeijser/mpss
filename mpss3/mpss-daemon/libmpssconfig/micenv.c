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
#include "mpssconfig.h"
#include "libmpsscommon.h"

int
mpssenv_init(struct mpss_env *menv)
{
	int err;

	memset(menv, 0, sizeof(struct mpss_env));
	mpssut_realloc_fill(&menv->home, getenv("HOME"));

	menv->live_update = TRUE;

	if ((err = mpssenv_set_distrib(menv, NULL)))
		return err;

	if ((err = mpssenv_set_destdir(menv, NULL)))
		return err;

	if ((err = mpssenv_set_configdir(menv, NULL)))
		return err;

	if ((err = mpssenv_set_vardir(menv, NULL)))
		return err;

	if ((err = mpssenv_set_srcdir(menv, NULL)))
		return err;

	return SETENV_SUCCESS;
}

int
mpssenv_set_distrib(struct mpss_env *menv, char *dist)
{
	struct stat sbuf;

	if (dist != NULL) {
		menv->live_update = FALSE;

		if (!strcasecmp(dist, "redhat"))
			menv->dist = DISTRIB_REDHAT;
		else if (!strcasecmp(dist, "suse"))
			menv->dist = DISTRIB_SUSE;
		else if (!strcasecmp(dist, "ubuntu"))
			menv->dist = DISTRIB_UBUNTU;
		else
			return (SETENV_FUNC_DIST << 16) | SETENV_CMDLINE_DIST;
	} else if ((dist = getenv("MPSS_DIST")) != NULL) {
		menv->live_update = FALSE;

		if (!strcasecmp(dist, "redhat"))
			menv->dist = DISTRIB_REDHAT;
		else if (!strcasecmp(dist, "suse"))
			menv->dist = DISTRIB_SUSE;
		else if (!strcasecmp(dist, "ubuntu"))
			menv->dist = DISTRIB_UBUNTU;
		else
			return (SETENV_FUNC_DIST << 16) | SETENV_ENV_DIST;
	} else {
		if ((stat(REDHAT_NETWORK_DIR, &sbuf) == 0) && S_ISDIR(sbuf.st_mode))
			menv->dist = DISTRIB_REDHAT;
		else if ((stat(SUSE_NETWORK_DIR, &sbuf) == 0) && S_ISDIR(sbuf.st_mode))
			menv->dist = DISTRIB_SUSE;
		else if ((stat(UBUNTU_NETWORK_DIR, &sbuf) == 0) && S_ISDIR(sbuf.st_mode))
			menv->dist = DISTRIB_UBUNTU;
		else
			return (SETENV_FUNC_DIST << 16) | SETENV_PROBE_DIST;
	}

	switch(menv->dist) {
	case DISTRIB_REDHAT:
	case DISTRIB_SUSE:
		menv->lockfile = LSB_LOCK_FILENAME;
		break;
	case DISTRIB_UBUNTU:
		menv->lockfile = UBUNTU_LOCK_FILENAME;
		break;
	}

	return SETENV_SUCCESS;
}

int
mpssenv_set_configdir(struct mpss_env *menv, char *cdir)
{
	struct stat sbuf;
	char *confdir;
	char *value;
	char buffer[4096];
	int fd;

	if (cdir != NULL) {
		confdir = mpssut_tilde(menv, cdir);
		if (stat(confdir, &sbuf) != 0)
			return SETENV_CMDLINE_EXIST;

		if (!S_ISDIR(sbuf.st_mode))
			return SETENV_CMDLINE_ISDIR;

		mpssut_realloc_fill(&menv->confdir, confdir);
		free(confdir);
	} else if ((value = getenv("MPSS_CONFIGDIR")) != NULL) {
		if (stat(value, &sbuf) != 0)
			return (SETENV_FUNC_CONFDIR << 16) | SETENV_ENV_EXIST;

		if (!S_ISDIR(sbuf.st_mode))
			return (SETENV_FUNC_CONFDIR << 16) | SETENV_ENV_ISDIR;

		mpssut_realloc_fill(&menv->confdir, value);
	} else if (stat(MPSS_CONFIG_FILE, &sbuf) == 0) {
		if ((fd = open(MPSS_CONFIG_FILE, O_RDONLY)) < 0)
			return (SETENV_FUNC_CONFDIR << 16) | SETENV_CONFFILE_ACCESS;

		if (read(fd, buffer, 4096) < 0) {
			close(fd);
			return (SETENV_FUNC_CONFDIR << 16) | SETENV_CONFFILE_ACCESS;
		}

		close(fd);

		if ((value = strchr(buffer, '=')) == NULL)
			return (SETENV_FUNC_CONFDIR << 16) | SETENV_CONFFILE_FORMAT;

		*value++ = '\0';

		if (strcmp(buffer, "MPSS_CONFIGDIR"))
			return (SETENV_FUNC_CONFDIR << 16) | SETENV_CONFFILE_CONTENT;

		if (value[strlen(value) - 1] == '\n')
			value[strlen(value) - 1] = '\0';

		if (stat(value, &sbuf) != 0)
			return (SETENV_FUNC_CONFDIR << 16) | SETENV_CONFFILE_EXIST;

		if (!S_ISDIR(sbuf.st_mode))
			return (SETENV_FUNC_CONFDIR << 16) | SETENV_CONFFILE_ISDIR;

		mpssut_realloc_fill(&menv->confdir, value);
	} else {
		mpssut_realloc_fill(&menv->confdir, DEFAULT_CONFDIR);
	}

	return SETENV_SUCCESS;
}

int
mpssenv_set_destdir(struct mpss_env *menv, char *ddir)
{
	struct stat sbuf;
	char *destdir;
	char *value;
	int ret = SETENV_SUCCESS;

	if (ddir != NULL) {
		destdir = mpssut_tilde(menv, ddir);
		if (stat(destdir, &sbuf) != 0)
			ret = (SETENV_FUNC_DESTDIR << 16) | SETENV_CMDLINE_EXIST;
		else if (!S_ISDIR(sbuf.st_mode))
			ret = (SETENV_FUNC_DESTDIR << 16) | SETENV_CMDLINE_ISDIR;

		mpssut_realloc_fill(&menv->destdir, destdir);
		menv->live_update = FALSE;
		free(destdir);
		return ret;
	} else if ((value = getenv("MPSS_DESTDIR")) != NULL) {
		if (stat(value, &sbuf) != 0)
			return (SETENV_FUNC_DESTDIR << 16) | SETENV_ENV_EXIST;

		if (!S_ISDIR(sbuf.st_mode))
			return (SETENV_FUNC_DESTDIR << 16) | SETENV_ENV_ISDIR;

		mpssut_realloc_fill(&menv->destdir, value);
		menv->live_update = FALSE;
	} else {
		menv->destdir = NULL;
	}

	return SETENV_SUCCESS;
}

int
mpssenv_set_vardir(struct mpss_env *menv, char *vdir)
{
	struct stat sbuf;
	char *vardir;
	char *value;

	if (vdir != NULL) {
		vardir = mpssut_tilde(menv, vdir);
		if (stat(vardir, &sbuf) != 0)
			return (SETENV_FUNC_VARDIR << 16) | SETENV_CMDLINE_EXIST;

		if (!S_ISDIR(sbuf.st_mode))
			return (SETENV_FUNC_VARDIR << 16) | SETENV_CMDLINE_ISDIR;

		mpssut_realloc_fill(&menv->vardir, vardir);
		free(vardir);
	} else if ((value = getenv("MPSS_VARDIR")) != NULL) {
		if (stat(value, &sbuf) != 0)
			return (SETENV_FUNC_VARDIR << 16) | SETENV_CMDLINE_EXIST;

		if (!S_ISDIR(sbuf.st_mode))
			return (SETENV_FUNC_VARDIR << 16) | SETENV_CMDLINE_ISDIR;

		mpssut_realloc_fill(&menv->vardir, value);
	} else {
		mpssut_realloc_fill(&menv->vardir, DEFAULT_VARDIR);
	}

	return SETENV_SUCCESS;
}

int
mpssenv_set_srcdir(struct mpss_env *menv, char *sdir)
{
	struct stat sbuf;
	char *srcdir;
	char *value;

	if (sdir != NULL) {
		srcdir = mpssut_tilde(menv, sdir);
		if (stat(srcdir, &sbuf) != 0)
			return (SETENV_FUNC_SRCDIR << 16) | SETENV_CMDLINE_EXIST;

		if (!S_ISDIR(sbuf.st_mode))
			return (SETENV_FUNC_SRCDIR << 16) | SETENV_CMDLINE_ISDIR;

		mpssut_realloc_fill(&menv->srcdir, srcdir);
		free(srcdir);
	} else if ((value = getenv("MPSS_SRCDIR")) != NULL) {
		if (stat(value, &sbuf) != 0)
			return (SETENV_FUNC_SRCDIR << 16) | SETENV_CMDLINE_EXIST;

		if (!S_ISDIR(sbuf.st_mode))
			return (SETENV_FUNC_SRCDIR << 16) | SETENV_CMDLINE_ISDIR;

		mpssut_realloc_fill(&menv->srcdir, value);
	} else {
		mpssut_realloc_fill(&menv->srcdir, DEFAULT_SRCDIR);
	}

	return SETENV_SUCCESS;
}

char *
mpssenv_errtype(int err)
{
	switch (err >> 16) {
	case SETENV_FUNC_DIST:
		return "Distribution";
	case SETENV_FUNC_CONFDIR:
		return "Confdir";
	case SETENV_FUNC_DESTDIR:
		return "Destdir";
	case SETENV_FUNC_VARDIR:
		return "Vardir";
	case SETENV_FUNC_SRCDIR:
		return "Srcdir";
	default:
		return "Invalid type code";
	}
}

char *
mpssenv_errstr(int err)
{
	switch (err & 0xffff) {
	case SETENV_SUCCESS:
		return "Success";
	case SETENV_CMDLINE_EXIST:
		return "Directory from command line does not exist";
	case SETENV_CMDLINE_ISDIR:
		return "Directory from command line not a directory";
	case SETENV_ENV_EXIST:
		return "Directory from environment does not exist";
	case SETENV_ENV_ISDIR:
		return "Directory from environment not a directory";
	case SETENV_CONFFILE_ACCESS:
		return "File "MPSS_CONFIG_FILE" cannot be accessed";
	case SETENV_CONFFILE_FORMAT:
		return "Directory from environment not a directory";
	case SETENV_CONFFILE_CONTENT:
		return "Directory from environment not a directory";
	case SETENV_CONFFILE_EXIST:
		return "Directory from environment not a directory";
	case SETENV_CONFFILE_ISDIR:
		return "Directory from environment not a directory";
	case SETENV_CMDLINE_DIST:
		return "from command line unknown";
	case SETENV_ENV_DIST:
		return "from environment unknown";
	case SETENV_PROBE_DIST:
		return "from system probe unknown";
	default:
		return "Unknown error code";
	}
}

int
mpssenv_aquire_lockfile(struct mpss_env *menv)
{
	struct flock fl;
	int lockfd;

	if ((lockfd = open(menv->lockfile, O_WRONLY | O_CREAT, 0777)) < 0)
		return EBADF;

	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 0;

	if (fcntl(lockfd, F_SETLK, &fl))
		return EEXIST;

	return 0;
}

void
mpssenv_clean(struct mpss_env *menv)
{
	free(menv->home);
	menv->home = NULL;

	free(menv->confdir);
	menv->confdir = NULL;

	free(menv->destdir);
	menv->destdir = NULL;

	free(menv->vardir);
	menv->vardir = NULL;

	free(menv->srcdir);
	menv->srcdir = NULL;
}
