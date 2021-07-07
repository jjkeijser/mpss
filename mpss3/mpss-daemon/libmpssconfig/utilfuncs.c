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

char *
mpssut_alloc_fill(char *old_string)
{
	char *new_string;

	if (old_string == NULL)
		return NULL;

	if ((new_string = (char *) malloc(strlen(old_string) + 1)) == NULL)
		return NULL;

	strcpy(new_string, old_string);
	return new_string;
}

int
mpssut_realloc_fill(char **addr, char *new_addr)
{
	if (*addr != NULL)
		free(*addr);

	*addr = mpssut_alloc_fill(new_addr);
	return 0;
}

int
mpssut_filename(struct mpss_env *menv, struct stat *sbuf, char *buffer, int maxlen, char *format, ...)
{
	va_list args;
	struct stat tmpsbuf;
	struct stat *asbuf;

	if (sbuf == NULL)
		asbuf = &tmpsbuf;
	else
		asbuf = sbuf;

	if (menv->destdir != NULL)
		snprintf(buffer, maxlen, "%s", menv->destdir);
	else
		*buffer = '\0';

	va_start(args, format);
	vsnprintf(&buffer[strlen(buffer)], maxlen - strlen(buffer), format, args);
	va_end(args);

	if (lstat(buffer, asbuf) == 0)
		return 1;

	return 0;
}

char *
mpssut_configname(struct mpss_env *menv, char *name)
{
	char buffer[PATH_MAX];
	char *buf;

	mpssut_filename(menv, NULL, buffer, PATH_MAX, "%s/%s.conf", menv->confdir, name);

	if ((buf = (char *) malloc(strlen(buffer) + 1)) != NULL) {
		strcpy(buf, buffer);
	}

	return buf;
}

int
mpssut_mkdir(struct mpss_env *menv, char *dir, uid_t uid, gid_t gid, int mode)
{
	char dirname[PATH_MAX];

	if (mpssut_filename(menv, NULL, dirname, PATH_MAX, "%s", dir))
		return EEXIST;

	if (mkdir(dirname, mode))
		return errno;

	chown(dirname, uid, gid);
	return 0;
}

struct specials {
	char *name;
	uid_t uid;
	gid_t gid;
	int   mode;
} spdirs[] = {
	{"/bin", 0, 0, 0755},
	{"/dev", 0, 0, 0755},
	{"/etc", 0, 0, 0755},
	{"/etc/network", 0, 0, 0755},
	{"/home", 0, 0, 0755},
	{"/lib", 0, 0, 0755},
	{"/lib64", 0, 0, 0755},
	{"/sbin", 0, 0, 0755},
	{"/tmp", 0, 0, 0755},
	{"/usr", 0, 0, 0755},
	{"/usr/bin", 0, 0, 0755},
	{"/usr/lib64", 0, 0, 0755},
	{"/usr/sbin", 0, 0, 0755},
	{"/usr/share", 0, 0, 0755},
	{"/var", 0, 0, 0755},
	{NULL, 0, 0, 0}
};

int
_mpssut_mksubtree(struct mpss_env *menv, char *base, char *dir, uid_t uid, gid_t gid, int mode)
{
	char buffer[PATH_MAX];
	char *slash;
	struct specials *sp = spdirs;
	int err;

	if (mpssut_filename(menv, NULL, buffer, PATH_MAX, "%s%s", base, dir))
		return 0;

	while (sp->name) {
		if (!strcmp(sp->name, dir)) {
			uid = sp->uid;
			gid = sp->gid;
			mode = sp->mode;
		}
		sp++;
	}

	snprintf(buffer, PATH_MAX, "%s", dir);

	if ((slash = strrchr(buffer, '/')) != buffer) {
		if (slash == NULL)
			return ENOENT;

		*slash = '\0';

		switch ((err = _mpssut_mksubtree(menv, base, buffer, uid, gid, mode))) {
		case 0:
			break;
		case EEXIST:
			return 0;
		default:
			return err;
		}
	}

	snprintf(buffer, PATH_MAX, "%s%s", base, dir);

	if ((err = mpssut_mkdir(menv, buffer, uid, gid, mode)))
		return err;

	return 0;
}

int
mpssut_mksubtree(struct mpss_env *menv, char *base, char *dir, uid_t uid, gid_t gid, int mode)
{
	char dirname[PATH_MAX];

	if (mpssut_filename(menv, NULL, dirname, PATH_MAX, "%s%s", base, dir))
		return EEXIST;

	return _mpssut_mksubtree(menv, base, dir, uid, gid, mode);
}

int
mpssut_rmsubtree(struct mpss_env *menv, char *base, char *dir)
{
	char buffer[PATH_MAX];
	char *slash;
	char dirname[PATH_MAX];
	struct specials *sp = spdirs;

	if (*dir != '/')
		return EINVAL;

	while (sp->name) {
		if (!strcmp(sp->name, dir))
			return 0;
		sp++;
	}

	snprintf(buffer, PATH_MAX, "%s", dir);

	if ((slash = strrchr(buffer, '/')) == dir)
		return 0;

	*slash = '\0';

	if (!mpssut_filename(menv, NULL, dirname, PATH_MAX, "%s%s", base, dir))
		return 1;

	if (rmdir(dirname))
		return 0;

	mpssut_rmsubtree(menv, base, buffer);
	return 0;
}

int
mpssut_mktree(struct mpss_env *menv, char *dir, uid_t uid, gid_t gid, int mode)
{
	char buffer[PATH_MAX];
	char *slash;

	if (*dir != '/')
		return EINVAL;

	snprintf(buffer, PATH_MAX, "%s", dir);

	if ((slash = strrchr(buffer, '/')) == dir)
		return 0;

	*slash = '\0';
	mpssut_mktree(menv, buffer, uid, gid, mode);
	mpssut_mkdir(menv, dir, uid, gid, mode);
	return 0;
}

int
mpssut_deltree(struct mpss_env *menv, char *dir)
{
	char filename[PATH_MAX];
	char subdirname[PATH_MAX];
	struct stat sbuf;
	struct dirent *file;
	DIR *dp;

	if ((dp = opendir(dir)) == NULL) {
		unlink(dir);
		return 0;
	}

	while ((file = readdir(dp)) != NULL) {
		if (strcmp(file->d_name, ".") &&
		    strcmp(file->d_name, "..")) {
			snprintf(filename, PATH_MAX, "%s/%s", dir, file->d_name);
			lstat(filename, &sbuf);
			if (S_ISDIR(sbuf.st_mode)) {
				snprintf(subdirname, PATH_MAX, "%s/%s", dir, file->d_name);
				mpssut_deltree(menv, subdirname);
			} else {
				snprintf(filename, PATH_MAX, "%s/%s", dir, file->d_name);
				unlink(filename);
			}
		}
	}

	rmdir(dir);
	closedir(dp);
	return 0;
}

int
mpssut_copyfile(char *to, char *from, struct stat *sbuf)
{
	char *buffer;
	int rsize;
	int fd;

	if ((buffer = (char *) malloc(sbuf->st_size + 1)) == NULL)
		return 1;

	if ((fd = open(from, O_RDONLY)) < 0) {
		free(buffer);
		return 1;
	}

	rsize = read(fd, buffer, sbuf->st_size);
	close(fd);

	if ((fd = open(to, O_WRONLY|O_CREAT, sbuf->st_mode & 0777)) < 0) {
		free(buffer);
		return 1;
	}

	write(fd, buffer, rsize);
	fchown(fd, sbuf->st_uid, sbuf->st_gid);
	close(fd);
	free(buffer);
	return 0;
}

int
mpssut_mknode(char *to, struct stat *sbuf)
{
	if (mknod(to, sbuf->st_mode, sbuf->st_rdev))
		return 1;

	chown(to, sbuf->st_uid, sbuf->st_gid);
	return 0;
}

int
mpssut_mklink(char *to, char *from, struct stat *sbuf)
{
	char lbuf[PATH_MAX];

	memset(lbuf, 0, PATH_MAX);

	if (readlink(from, lbuf,PATH_MAX) < 0)
		return 1;

	if (symlink(lbuf, to))
		return 2;

	chown(to, sbuf->st_uid, sbuf->st_gid);
	return 0;
}

int
mpssut_copytree(struct mpss_env *menv, char *to, char *from)
{
	char toname[PATH_MAX];
	char fromname[PATH_MAX];
	char tofile[PATH_MAX];
	char fromfile[PATH_MAX];
	struct stat sbuf;
	struct dirent *file;
	DIR *dp;

	if (mpssut_filename(menv, NULL, toname, PATH_MAX, "%s", to))
		return 1;

	if (!mpssut_filename(menv, &sbuf, fromname, PATH_MAX, "%s", from))
		return 2;

	if ((dp = opendir(fromname)) == NULL)
		return 3;

	mpssut_mksubtree(menv, "/", to, sbuf.st_uid, sbuf.st_gid, sbuf.st_mode & 0777);

	while ((file = readdir(dp)) != NULL) {
		if (!strcmp(file->d_name, ".") ||
		    !strcmp(file->d_name, ".."))
			continue;

		snprintf(fromfile, PATH_MAX, "%s/%s", from, file->d_name);
		snprintf(tofile, PATH_MAX, "%s/%s", to, file->d_name);

		lstat(fromfile, &sbuf);
		if (S_ISDIR(sbuf.st_mode))
			mpssut_copytree(menv, tofile, fromfile);
		else if (S_ISREG(sbuf.st_mode))
			mpssut_copyfile(tofile, fromfile, &sbuf);
		else if (S_ISLNK(sbuf.st_mode))
			mpssut_mklink(tofile, fromfile, &sbuf);
		else
			mpssut_mknode(tofile, &sbuf);
	}

	closedir(dp);
	return 0;
}

char *
mpssut_tilde(struct mpss_env *menv, char *name)
{
	char *newname;

	if (*name == '~') {
		if (menv->home == NULL)
			return NULL;

		if ((newname = (char *) malloc(strlen(menv->home) + strlen(name) + 2)) == NULL)
			return NULL;

		sprintf(newname, "%s%s", menv->home, &name[1]);
	} else {
		if ((newname = (char *) malloc(strlen(name) + 1)) == NULL)
			return NULL;

		sprintf(newname, "%s", name);
	}

	return newname;
}

char *
mpssut_tempnam(char *name)
{
	char cdir[PATH_MAX];
	char *tag;

	snprintf(cdir, PATH_MAX, "%s", name);

	if ((tag = strrchr(cdir, '/')) == NULL) {
		return tempnam(NULL, name);
	} else {
		*tag = '\0';
		tag++;
		return tempnam(cdir, tag);
	}

}

int
mpssut_alloc_and_load(char *filename, char **buf)
{
	struct stat sbuf;
	int flen;
	int fd;

	lstat(filename, &sbuf);

	if (!S_ISREG(sbuf.st_mode))
		return 0;

	if ((*buf = (char *) malloc(sbuf.st_size + 1)) == NULL)
		return 0;

	if ((fd = open(filename, O_RDONLY)) < 0) {
		free(buf);
		return 0;
	}

	flen = read(fd, *buf, sbuf.st_size);
	close(fd);
	return flen;
}

/*
 * mpssut_rename(struct mpss_env *menv, char* input, char* output)
 * - makes backup of output file if exists
 * - tries to rename input file into output file with rename() syscall
 * - if rename fails, it creates output file and copies content of input file into output file and removes input file
 * - if direct copying fails it restores the backup of output file
 */

int
mpssut_rename(struct mpss_env *menv, char* input, char* output) {
	int ifd;
	int ofd;
	char buffer[4096];
	char savefile[PATH_MAX];
	char destfile[PATH_MAX];
	ssize_t read_n;
	struct stat buf;

	if ((input == NULL) || (output == NULL))
		return 1;

	// check if destination file exists, if not, just proceed with renaming temporary file into destination file
	snprintf(destfile, PATH_MAX, "%s", output);
	snprintf(savefile, PATH_MAX, "%s.orig", destfile);

	// if old saved file exists remove it
	if (!stat(savefile, &buf)) {
		unlink(savefile);
	}

	// if destination file exists move it to .orig file to allow recovery when rename fails
	if (rename(destfile, savefile)) {
		// failed to backup output file, break here
		return 1;
	}

	// performs actual rename
	if (rename(input, destfile)) {
		// if rename failed, perform a copy manually
		if ((ifd = open(input, O_RDONLY)) == -1) {
			rename(savefile, destfile); // this is OK to fail, backup file exists
			return 1;
		}

		if (fstat(ifd, &buf) == -1) {
			rename(savefile, destfile); // this is OK to fail, backup file exists
			close(ifd);
			return 1;
		}

		if ((ofd = open(output, O_WRONLY | O_CREAT | O_EXCL, buf.st_mode)) == -1) {
			rename(savefile, destfile); // this is OK to fail, backup file exists
			close(ifd);
			return 1;
		}

		while ((read_n = read(ifd, buffer, sizeof(buffer))) > 0) {
			if (write(ofd, buffer, read_n) != read_n) {
				rename(savefile, destfile); // this is OK to fail, backup file exists
				close(ifd);
				close(ofd);
				return EIO;
			}
		}
		close(ifd);
		close(ofd);
		unlink(input); // removes input file after successful copy
	}

	unlink(savefile); // removes backup file after success
	return 0;
}
