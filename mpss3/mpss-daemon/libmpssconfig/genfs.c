/*
 * Constructs a newc-format cpio archive.
 *
 * Copyright 2012 Intel Corporation.
 *
 * This file is a derivative of usr/gen_init_cpio.c from within the
 * Linux kernel source distribution, version 2.6.34; it has been heavily
 * modified (starting in March 2012) for use within the Intel MIC
 * Platform Software Stack.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301
 * USA.
 *
 * Authorship notices present in usr/gen_init_cpio.c:
 *
 * Original work by Jeff Garzik
 *
 * External file lists, symlink, pipe and fifo support by Thayne Harbaugh
 * Hard link support by Luciano Rocha
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
#include <linux/kdev_t.h>
#include "mpssconfig.h"
#include "libmpsscommon.h"

#define SUPPORT_FILELIST

#define MAX_LINE ((2 * PATH_MAX) + 64)

struct filetree {
	int		type;
	char		*name;
	char		*source;
	char		*link;
	int		mode;
	int		uid;
	int		gid;
	char		inode_type;
	unsigned int	major;
	unsigned int	minor;
	struct filetree	*subdir;
	struct filetree *next;
	struct filetree *prev;
};

struct type_list {
	char	*name;
	int	len;
	int	type;
	void	(*func)(int id, char *line, struct filetree *top, char *sourcedir, struct mpss_elist *perrs);
};

//#define MPSSDEBUG

#ifdef MPSSDEBUG
void dump_tree(struct filetree *elem, int level);
#endif

#define MPSS_UNKNOWN	0
#define MPSS_DIR	1
#define MPSS_FILE	2
#define MPSS_LINK	3
#define MPSS_DNODE	4
#define MPSS_PIPE	5
#define MPSS_SOCK	6

void add_dir(int id, char *line, struct filetree *top, char *sourcedir, struct mpss_elist *perrs);
void add_file(int id, char *line, struct filetree *top, char *sourcedir, struct mpss_elist *perrs);
void add_link(int id, char *line, struct filetree *top, char *sourcedir, struct mpss_elist *perrs);
void add_dnode(int id, char *line, struct filetree *top, char *sourcedir, struct mpss_elist *perrs);
void add_pipe(int id, char *line, struct filetree *top, char *sourcedir, struct mpss_elist *perrs);
void add_sock(int id, char *line, struct filetree *top, char *sourcedir, struct mpss_elist *perrs);

int gen_cpio(struct mpss_env *menv, struct mic_info *mic, struct filetree *top, struct mpss_elist *perrs);
int copy_nfs(struct mpss_env *menv, struct mic_info *mic, struct filetree *top, int createusr, struct mpss_elist *perrs);

void add_simple_dir(int id, char *source, char *target, char *lead, struct filetree *top, struct mpss_elist *perrs);
void add_simple_file(int id, char *source, char *target, char *lead, struct filetree *top, struct mpss_elist *perrs);
void add_simple_link(int id, char *source, char *target, char *lead, struct filetree *top, struct mpss_elist *perrs);
void add_simple_nod(int id, char type, char *source, char *target, char *lead, struct filetree *top, struct mpss_elist *perrs);
void add_simple_pipe(int id, char *source, char *target, char *lead, struct filetree *top, struct mpss_elist *perrs);
void add_simple_sock(int id, char *source, char *target, char *lead, struct filetree *top, struct mpss_elist *perrs);

int insert_list(int id, char *path, struct filetree *new_tree, struct filetree *elem, char *source, struct mpss_elist *perrs);
void gunzip_image(char *name);

#define xstr(s) #s
#define str(s) xstr(s)

int
mpssfs_unzip_base_cpio(char *name, char *zfile, char *ofile, struct mpss_elist *perrs)
{
	struct stat sbuf;
	char buf[4096];
	int len;
	int rlen;
	int ifd;
	int ofd;

	if ((stat(zfile, &sbuf) != 0) ||
	    ((ifd = open(zfile, O_RDONLY)) < 0)) {
		add_perr(perrs, PERROR, "%s: error opening base cpio image '%s': %s", name, zfile, strerror(errno));
		return 1;
	}

	len = sbuf.st_size;

	if (stat(ofile, &sbuf) == 0)
		unlink(ofile);

	if ((ofd = open(ofile, O_WRONLY | O_CREAT, 0600)) < 0) {
		add_perr(perrs, PERROR, "%s: error opening image tmpfile '%s': %s", name, ofile, strerror(errno));
		close(ifd);
		return 1;
	}

	while (len > 0) {
		rlen = read(ifd, buf, 4096);
		write(ofd, buf, rlen);
		len -= 4096;
	}

	close(ofd);
	close(ifd);

	gunzip_image(ofile);
	return 0;
}



struct type_list typelist[] = {
	{"dir", 3, MPSS_DIR, add_dir},
	{"file", 4, MPSS_FILE, add_file},
	{"slink", 5, MPSS_LINK, add_link},
	{"nod", 3, MPSS_DNODE, add_dnode},
	{"pipe", 4, MPSS_PIPE, add_pipe},
	{"sock", 4, MPSS_SOCK, add_sock},
	{NULL, 0, 0, NULL}
};

char *
get_type_string(int filetype)
{
	if ((filetype >= MPSS_DIR) && (filetype <= MPSS_SOCK))
		return typelist[filetype - 1].name;

	return "Unknown";
}

int
handle_dir(int id, struct filetree *top, char *sourcedir, char *listfile, struct mpss_elist *perrs)
{
	struct type_list *t;
	FILE *fp;
	char line[MAX_LINE];
	struct stat sbuf;

	if ((fp = fopen(listfile, "r")) == NULL) {
		add_perr(perrs, PWARN, "mic%d failed to open '%s': %s", id, listfile, strerror(errno));
		return 1;
	}

	if (fstat(fileno(fp), &sbuf) < 0) {
		add_perr(perrs, PWARN, "mic%d failed to stat '%s': %s", id, listfile, strerror(errno));
		fclose(fp);
		return 1;
	}

	if (sbuf.st_uid || sbuf.st_gid) {
		add_perr(perrs, PWARN, "mic%d '%s' must be owned by the root user", id, listfile);
		fclose(fp);
		return 1;
	}

	if (sbuf.st_mode & (S_IWGRP | S_IWOTH)) {
		add_perr(perrs, PWARN, "mic%d '%s' must not be writable by other than the root user", id, listfile);
		fclose(fp);
		return 1;
	}

	while (fgets(line, MAX_LINE, fp)) {
		// Simply skip any line that does not start with an interesting value
		t = &typelist[0];
		while (t->name != NULL) {
			if (!strncmp(line, t->name, t->len)) {
				t->func(id, &line[t->len + 1], top, sourcedir, perrs);
			}

			t++;
		}
	}

	fclose(fp);
	return 0;
}

int
handle_file(int id, struct filetree *top, char *source, char *dest, struct mpss_elist *perrs)
{
	struct stat sbuf;
	struct filetree *new_tree;

	if (lstat(source, &sbuf) != 0)
		return 0;

	if (!S_ISREG(sbuf.st_mode))
		return 0;

	if ((new_tree = (struct filetree *) malloc(sizeof(struct filetree))) == NULL)
		return 0;

	new_tree->type = MPSS_FILE;
	if ((new_tree->source = (char *) malloc(strlen(source) + 1)) == NULL) {
		free(new_tree);
		return 0;
	}
	strcpy(new_tree->source, source);

#ifdef SUPPORT_FILELIST
	new_tree->uid = -1;
#endif

	return insert_list(id, &dest[1], new_tree, top, NULL, perrs);
}

int
_handle_rpm(int id, struct filetree *top, char *source, struct mpss_elist *perrs)
{
	struct stat sbuf;
	struct filetree *new_tree;
	char tmp[PATH_MAX];
	char dest[PATH_MAX];

	if (lstat(source, &sbuf) != 0)
		return 1;

	if (!S_ISREG(sbuf.st_mode)) {
		add_perr(perrs, PERROR, "mic%d RPM '%s' is not a file\n", id, source);
		return 1;
	}

	if ((new_tree = (struct filetree *) malloc(sizeof(struct filetree))) == NULL)
		return 1;

	new_tree->type = MPSS_FILE;
	if ((new_tree->source = (char *) malloc(strlen(source) + 1)) == NULL) {
		free(new_tree);
		return 1;
	}
	strcpy(new_tree->source, source);

#ifdef SUPPORT_FILELIST
	new_tree->uid = -1;
#endif
	strncpy(tmp, source, PATH_MAX) ;
	snprintf(dest, PATH_MAX, "RPMs-to-install/%s", basename(source));
	return insert_list(id, dest, new_tree, top, NULL, perrs);
}

int
handle_rpm(int id, struct filetree *top, char *source, struct mpss_elist *perrs)
{
	struct stat sbuf;
	char path[PATH_MAX];
	char base[PATH_MAX];
	char *slash;
	struct dirent *file;
	DIR *dp;
	int errcnt = 0;

	if (*source != '/') {
		add_perr(perrs, PERROR, "mic%d Failed RPM '%s' must start from the '/' directory\n", id, source);
		return 1;
	}

	strncpy(base, source, PATH_MAX);

	if (lstat(source, &sbuf) == 0) {
		if (S_ISDIR(sbuf.st_mode))
			snprintf(base, PATH_MAX, "%s/*.*", source);
	}

	if ((slash = strrchr(base, '/')) == NULL) {
		add_perr(perrs, PERROR, "mic%d Failed RPM '%s' must start from the '/' directory\n", id, source);
		return 1;
	}

	*slash = '\0';
	slash++;

	if ((dp = opendir(base)) == NULL) {
		add_perr(perrs, PERROR, "mic%d Failed to find RPM overlay '%s'\n", id, source);
		return 1;
	}

	while ((file = readdir(dp)) != NULL) {
		if (!strcmp(file->d_name, ".") ||
	 	    !strcmp(file->d_name, ".."))
			continue;

		if (fnmatch(slash, file->d_name, 0) == 0) {
			snprintf(path, PATH_MAX, "%s/%s", base, file->d_name);
			errcnt = _handle_rpm(id, top, path, perrs);
		}
	}

	closedir(dp);
	return errcnt;
}

int
handle_subdir(int id, struct filetree *top, char *source, char *target, char *lead, struct mpss_elist *perrs)
{
	struct stat sbuf;

	if (lstat(source, &sbuf) != 0)
		return 0;

	if (S_ISREG(sbuf.st_mode))
		add_simple_file(id, source, target, lead, top, perrs);
	else if (S_ISCHR(sbuf.st_mode))
		add_simple_nod(id, 'c', source, target, lead, top, perrs);
	else if (S_ISBLK(sbuf.st_mode))
		add_simple_nod(id, 'b', source, target, lead, top, perrs);
	else if (S_ISFIFO(sbuf.st_mode))
		add_simple_pipe(id, source, target, lead, top, perrs);
	else if (S_ISLNK(sbuf.st_mode))
		add_simple_link(id, source, target, lead, top, perrs);
	else if (S_ISSOCK(sbuf.st_mode))
		add_simple_sock(id, source, target, lead, top, perrs);
	else if (S_ISDIR(sbuf.st_mode))
		add_simple_dir(id, source, target, lead, top, perrs);

	return 0;
}

int
handle_simple(int id, struct filetree *top, char *source, char *target, char *lead, struct mpss_elist *perrs)
{
	char path[PATH_MAX];
	char base[PATH_MAX];
	char *slash;
	struct dirent *file;
	DIR *dp;

	if (*source != '/') {
		add_perr(perrs, PERROR, "mic%d Failed overlay '%s' must start from the '/' directory\n", id, source);
		return 1;
	}

	strncpy(base, source, PATH_MAX - 1);
	base[PATH_MAX - 1] = '\0';

	while (strlen(base) && (base[strlen(base) - 1] == '/'))
		base[strlen(base) - 1] = '\0';

	if ((slash = strrchr(base, '/')) == NULL) {
		add_perr(perrs, PERROR, "mic%d Failed RPM '%s' must start from the '/' directory\n", id, source);
		return 1;
	}

	*slash = '\0';
	slash++;

	if ((dp = opendir(base)) == NULL) {
		add_perr(perrs, PERROR, "mic%d Failed to find RPM overlay '%s'\n", id, source);
		return 1;
	}

	while ((file = readdir(dp)) != NULL) {
		if (!strcmp(file->d_name, ".") ||
	 	    !strcmp(file->d_name, ".."))
			continue;

		if (fnmatch(slash, file->d_name, 0) == 0) {
			snprintf(path, PATH_MAX, "%s/%s", base, file->d_name);
			handle_subdir(id, top, path, target, lead, perrs);
		}
	}

	closedir(dp);
	return 0;
}

#ifdef MPSSDEBUG
void
dump_tree(struct filetree *elem, int level)
{
	int i;

	while (elem) {
		for (i = 0; i < level; i++) {
			printf("     ");
		}

		if (elem->type == MPSS_DIR) {
			printf("| %s <==> %s\n", elem->name, elem->source);
			for (i = 0; i < level; i++) {
				printf("     ");
			}
			printf("\\----\\\n");
			dump_tree(elem->subdir, level + 1);
		} else if (elem->type == MPSS_LINK) {
			printf("| %s <==> %s\n", elem->name, elem->source);
		} else if (elem->type == MPSS_DNODE) {
			printf("| %s <==> %s\n", elem->name, elem->source);
		} else if (elem->type == MPSS_PIPE) {
			printf("| %s <==> %s\n", elem->name, elem->source);
		} else if (elem->type == MPSS_SOCK) {
			printf("| %s <==> %s\n", elem->name, elem->source);
		} else {
			printf("| %s <==> %s\n", elem->name, elem->source);
		}

		elem = elem->next;
	}
}
#endif

void
free_list(struct filetree *elem)
{
	struct filetree *freethis;

	while (elem) {
		if (elem->type == MPSS_DIR) {
			free_list(elem->subdir);
		}

		freethis = elem;
		elem = elem->next;
		free(freethis);
	}
}

int
handle_common(struct mpss_env *menv, int id, int type, char *dir, char *list, struct filetree *top, struct mpss_elist *perrs)
{
	char comname[PATH_MAX];
	struct dirent *file;
	DIR *dp;

	switch (type) {
	case SRCTYPE_DIR:
		mpssut_filename(menv, NULL, comname, PATH_MAX, "%s", dir);
		if ((dp = opendir(comname))) {
			while ((file = readdir(dp)) != NULL) {
				if (!strcmp(file->d_name, ".") ||
			 	    !strcmp(file->d_name, ".."))
					continue;

				mpssut_filename(menv, NULL, comname, PATH_MAX, "%s/%s", dir, file->d_name);
				handle_subdir(id, top, comname, "/", "", perrs);
			}

			closedir(dp);
		}

		break;
	case SRCTYPE_FILELIST:
		if (handle_dir(id, top, dir, list, perrs)) {
			return 3;
		}
		break;
	}

	return 0;
}

int
gen_fs_tree(struct mpss_env *menv, struct mic_info *mic, struct filetree *top, struct mpss_elist *perrs)
{
	struct moverdir *dir = mic->config.filesrc.overlays.next;

	top->type = MPSS_DIR;
	if ((top->name = (char *) malloc(2)) == NULL) {
		return 1;
	}
	strcpy(top->name, "/");
	top->source = NULL;
	top->mode = 0555;
	top->uid = 0;
	top->gid = 0;
	top->subdir = NULL;
	top->next = NULL;
	top->prev = NULL;

	if (mic->config.filesrc.base.type == SRCTYPE_DIR) {
		handle_common(menv, mic->id, mic->config.filesrc.base.type, mic->config.filesrc.base.dir,
			      mic->config.filesrc.base.list, top, perrs);
	}

	handle_common(menv, mic->id, mic->config.filesrc.common.type, mic->config.filesrc.common.dir,
		      mic->config.filesrc.common.list, top, perrs);

	while (dir) {
		if (dir->state == FALSE)
			goto nextdir;

		switch (dir->type) {
		case OVER_TYPE_SIMPLE:
			handle_simple(mic->id, top, dir->dir, dir->target, "", perrs);
			break;
		case OVER_TYPE_FILELIST:
			handle_dir(mic->id, top, dir->dir, dir->target, perrs);
			break;
		case OVER_TYPE_FILE:
			handle_file(mic->id, top, dir->dir, dir->target, perrs);
			break;
		case OVER_TYPE_RPM:
			handle_rpm(mic->id, top, dir->dir, perrs);
			break;
		}
nextdir:
		dir = dir->next;
	}

	handle_common(menv, mic->id, mic->config.filesrc.mic.type, mic->config.filesrc.mic.dir,
		      mic->config.filesrc.mic.list, top, perrs);

	return 0;
}

int
mpssfs_gen_initrd(struct mpss_env *menv, struct mic_info *mic, struct mpss_elist *perrs)
{
	struct filetree top;
	int err;

	mpss_clear_elist(perrs);


	if ((err = gen_fs_tree(menv, mic, &top, perrs)) == 0) {
#ifdef MPSSDEBUG
		dump_tree(&top, 0);
#endif
		err = gen_cpio(menv, mic, &top, perrs);
	}

	free_list(top.subdir);
	return err;
}

int
mpssfs_gen_nfsdir(struct mpss_env *menv, struct mic_info *mic, int createusr, struct mpss_elist *perrs)
{
	struct filetree top;
	int err;

	mpss_clear_elist(perrs);

	if ((err = gen_fs_tree(menv, mic, &top, perrs)) == 0) {
#ifdef MPSSDEBUG
		dump_tree(&top, 0);
#endif
		err = copy_nfs(menv, mic, &top, createusr, perrs);
	}

	free_list(top.subdir);
	return err;
}

char *
insert_name(int id, char *name, struct mpss_elist *perrs)
{
	char *new_tree;

	if ((new_tree = (char *) malloc(strlen(name) + 1)) == NULL) {
		add_perr(perrs, PINFO, "mic%d: failed memory alloc file name '%s' skipping", id, name);
		return NULL;
	}

	strcpy(new_tree, name);
	return new_tree;
}

struct filetree *
alloc_and_fill_missing_entry(int id, char *name, struct mpss_elist *perrs)
{
	struct filetree *new_tree;

	if ((new_tree = (struct filetree *) malloc(sizeof(struct filetree))) == NULL) {
		add_perr(perrs, PINFO, "mic%d: memory alloc failed for entry %s - skipping", id, name);
		return NULL;
	}

	memset(new_tree, 0, sizeof(struct filetree));

	if ((new_tree->name = insert_name(id, name, perrs)) == NULL) {
		free(new_tree);
		return NULL;
	}

	new_tree->type = MPSS_DIR;
	new_tree->mode = 0755;
	return new_tree;
}

int
insert_subdir(int id, struct filetree *cur, struct filetree *new_tree, char *name, char *rest,
	      char *source, struct mpss_elist *perrs)
{
	if (rest == NULL) {	// This is the last element in the specified path so just attach the new structure
		if ((new_tree->name = insert_name(id, name, perrs)) == NULL) {
			free(new_tree);
			return 1;
		}

		cur->subdir = new_tree;
		new_tree->next = NULL;
		new_tree->prev = NULL;
		return 0;
	}

	// Intermediate element in the path name has not matched a current element in the file tree.  Create
	// a default DIR type entry and set the permissions to root and 755.
	if ((cur->subdir = alloc_and_fill_missing_entry(id, name, perrs)) == NULL)
		return 1;

	return insert_list(id, rest, new_tree, cur->subdir, source, perrs);
}

int
insert_top(int id, struct filetree *cur, struct filetree *new_tree, char *name, char *rest,
	      char *source, struct mpss_elist *perrs)
{
	struct filetree *tmp;

	if (rest == NULL) {
		if ((new_tree->name = insert_name(id, name, perrs)) == NULL) {
			free(new_tree);
			return 1;
		}

		new_tree->next = cur->subdir;
		new_tree->prev = NULL;
		cur->subdir->prev = new_tree;
		cur->subdir = new_tree;
		return 0;
	}

	if ((tmp = alloc_and_fill_missing_entry(id, name, perrs)) == NULL)
		return 1;

	tmp->next = cur->subdir;
	tmp->prev = NULL;
	cur->subdir->prev = tmp;
	cur->subdir = tmp;
	return insert_list(id, rest, new_tree, tmp, source, perrs);
}

int
insert_before(int id, struct filetree *cur, struct filetree *new_tree, char *name, char *rest,
	      char *source, struct mpss_elist *perrs)
{
	struct filetree *tmp;

	if (rest == NULL) {
		if ((new_tree->name = insert_name(id, name, perrs)) == NULL) {
			free(new_tree);
			return 1;
		}

		new_tree->prev = cur->prev;
		new_tree->next = cur;
		cur->prev->next = new_tree;
		cur->prev = new_tree;
		return 0;
	}

	if ((tmp = alloc_and_fill_missing_entry(id, name, perrs)) == NULL)
		return 1;

	tmp->prev = cur->prev;
	tmp->next = cur;
	cur->prev->next = tmp;
	cur->prev = tmp;
	cur = tmp;
	return insert_list(id, rest, new_tree, tmp, source, perrs);
}

int
overwrite_entry(int id, struct filetree *cur, struct filetree *new_tree, char *name, char *rest,
	        char *source, struct mpss_elist *perrs)
{
	if (rest != NULL) {
		return insert_list(id, rest, new_tree, cur, source, perrs);
	}

	// Entry types have to match
	if (cur->type != new_tree->type) {
		add_perr(perrs, PINFO, "mic%d: overlay %s abort mismatched types changing from %s to %s",
			      id, name, get_type_string(cur->type), get_type_string(new_tree->type));
		return 1;
	}

	// Update the old entry to match and free the new_tree.
	if (cur->source != NULL)
		free(cur->source);

	cur->source = new_tree->source;
	cur->mode = new_tree->mode;
	cur->uid = new_tree->uid;
	cur->gid = new_tree->gid;
	free(new_tree);

	return 0;
}

// This is a complicated function that inserts the new file system element into the tree of already known
// file system entries.  The 'elem' passed in will be the top of the known file tree.  Each portion of the
// path passed in if parsed based on the '/' directory entry specifier and checked to see if it is already
// in the tree.  The last element of the path passed in defines the actual entry wanting to be inserted.
//
// If an intermediate piece is not aleady in the path then an element in the file tree is inserted and marked
// as a directory with default root owner and 755 permissions.  It is better to always already have inserted a
// directory entry.
//
// Most of the code this is no problem since the recursion through the Linux file system being used to find the
// elements will have already inserted the directory.  The exception is the entries being inserted by the "filelist"
// type of entry where it may add a file with a long path without previously adding the intermediate directories.
//
// The code is built to ensure the overlay mechanism works.  This means if an entry is specified twice the second
// one overwrites the entry of the first one.
int
insert_list(int id, char *path, struct filetree *new_tree, struct filetree *elem, char *source, struct mpss_elist *perrs)
{
	struct filetree *tmp;
	char *cur = path;
	char *rest = cur;
	int miss;

	if (strlen(path) == 0)
		return 0;

	if ((rest = strchr(cur, '/')) != NULL) {
		*rest = '\0';
		rest++;
	}

	if (elem->subdir == NULL)	// There are currently no files in this directory.
		return insert_subdir(id, elem, new_tree, cur, rest, source, perrs);

	if (strcmp(cur, elem->subdir->name) < 0)
		return insert_top(id, elem, new_tree, cur, rest, source, perrs);

	elem = elem->subdir;

	while (elem != NULL) {
		miss = strcmp(cur, elem->name);

		if (miss < 0) {	// Insert before the cur element
			return insert_before(id, elem, new_tree, cur, rest, source, perrs);
		} else if (miss == 0) {	// Exact match
			return overwrite_entry(id, elem, new_tree, cur, rest, source, perrs);
		} else {		// Belongs after cur element
			tmp = elem;
			elem = elem->next;
		}
	}

	// Add to tail
	if (rest == NULL) {
		if ((new_tree->name = insert_name(id, path, perrs)) == NULL) {
			free(new_tree);
			return 1;
		}

		tmp->next = new_tree;
		new_tree->prev = tmp;
		new_tree->next = NULL;
		return 0;
	}

	// The entry has a directory in the middle that has not been previously identified.
	if ((tmp->next = alloc_and_fill_missing_entry(id, path, perrs)) == NULL) {
		return 1;
	}

	tmp->next->prev = tmp;
	elem = tmp->next;
	elem->next = NULL;
	return insert_list(id, rest, new_tree, elem, source, perrs);
}

void
add_dir(int id, char *line, struct filetree *top, char *sourcedir, struct mpss_elist *perrs)
{
	struct filetree *new_tree;
	char path[PATH_MAX];

	if ((new_tree = (struct filetree *) malloc(sizeof(struct filetree))) == NULL) {
		return;
	}

	if (sscanf(line, "%" str(PATH_MAX) "s %o %d %d", path, &new_tree->mode, &new_tree->uid, &new_tree->gid) != 4) {
		add_perr(perrs, PWARN, "mic%d Bad attribute line 'dir %s'", id, line);
	}

	// Check for trailing / characters
	while (path[strlen(path) - 1] == '/')
		path[strlen(path) - 1] = '\0';

	new_tree->type = MPSS_DIR;
	new_tree->source = NULL;
	new_tree->subdir = NULL;
	new_tree->next = NULL;
	new_tree->prev = NULL;

	insert_list(id, &path[1], new_tree, top, NULL, perrs);
}

void
simple_path(char *buf, int size, char *target, char *lead, char *filename)
{
	buf[0] = '\0';

	//'3' to store two '/' and '\0'
	if ((strlen(target) + strlen(lead) + strlen(filename) + 3) > PATH_MAX)
		return;

	if (strcmp(target, "/"))
		strcat(buf, target);

	strcat(buf, "/");

	if (strlen(lead)) {
		strcat(buf, lead);
		strcat(buf, "/");
	}

	strcat(buf, filename);

}

void
add_simple_dir(int id, char *source, char *target, char *lead, struct filetree *top, struct mpss_elist *perrs)
{
	char newsource[PATH_MAX];
	char newlead[PATH_MAX];
	struct filetree *new_tree;
	char path[PATH_MAX];
	char *filename = basename(source);
	struct dirent *file;
	DIR *dp;

	if ((new_tree = (struct filetree *) malloc(sizeof(struct filetree))) == NULL) {
		return;
	}

	new_tree->type = MPSS_DIR;
	new_tree->subdir = NULL;
	new_tree->next = NULL;
	new_tree->prev = NULL;

	if ((new_tree->source = (char *) malloc(strlen(source) + 1)) == NULL) {
		free(new_tree);
		return;
	}
	strcpy(new_tree->source, source);

#ifdef SUPPORT_FILELIST
	new_tree->uid = -1;
#endif
	simple_path(path, PATH_MAX, target, lead, filename);
	insert_list(id, &path[1], new_tree, top, NULL, perrs);

	if ((dp = opendir(source)) == NULL) {
		return;
	}

	while ((file = readdir(dp)) != NULL) {
		if (!strcmp(file->d_name, ".") ||
		    !strcmp(file->d_name, "..")) {
			continue;
		}

		snprintf(newsource, PATH_MAX - 1, "%s/%s", source, file->d_name);
		if (strlen(lead))
			snprintf(newlead, PATH_MAX - 1, "%s/%s", lead, basename(source));
		else
			snprintf(newlead, PATH_MAX - 1, "%s", basename(source));
		handle_subdir(id, top, newsource, target, newlead, perrs);
	}

	closedir(dp);
}

void
add_file(int id, char *line, struct filetree *top, char *sourcedir, struct mpss_elist *perrs)
{
	struct filetree *new_tree;
	char path[PATH_MAX];
	char loc[PATH_MAX];

	if ((new_tree = (struct filetree *) malloc(sizeof(struct filetree))) == NULL) {
		return;
	}

	if (sscanf(line, "%" str(PATH_MAX) "s %" str(PATH_MAX) "s %o %d %d",
		   path, loc, &new_tree->mode, &new_tree->uid, &new_tree->gid) != 5) {
		add_perr(perrs, PWARN, "mic%d Bad attribute line 'dir %s'", id, line);
	}

	new_tree->type = MPSS_FILE;
	new_tree->subdir = NULL;
	new_tree->next = NULL;

	if ((new_tree->source = (char *) malloc(strlen(sourcedir) + strlen(loc) + 3)) == NULL) {
		free(new_tree);
		return;
	}
	sprintf(new_tree->source, "%s/%s", sourcedir, loc);

	insert_list(id, &path[1], new_tree, top, NULL, perrs);
}

void
add_simple_file(int id, char *source, char *target, char *lead, struct filetree *top, struct mpss_elist *perrs)
{
	struct filetree *new_tree;
	char path[PATH_MAX];
	char *filename = basename(source);

	if ((new_tree = (struct filetree *) malloc(sizeof(struct filetree))) == NULL) {
		return;
	}

	new_tree->type = MPSS_FILE;

	if ((new_tree->source = (char *) malloc(strlen(source) + 1)) == NULL) {
		free(new_tree);
		return;
	}
	strcpy(new_tree->source, source);

#ifdef SUPPORT_FILELIST
	new_tree->uid = -1;
#endif

	simple_path(path, PATH_MAX, target, lead, filename);
	new_tree->subdir = NULL;
	new_tree->next = NULL;

	insert_list(id, &path[1], new_tree, top, NULL, perrs);
}

void
add_link(int id, char *line, struct filetree *top, char *sourcedir, struct mpss_elist *perrs)
{
	struct filetree *new_tree;
	char path[PATH_MAX];
	char target[PATH_MAX];

	if ((new_tree = (struct filetree *) malloc(sizeof(struct filetree))) == NULL) {
		return;
	}

	if (sscanf(line, "%" str(PATH_MAX) "s %" str(PATH_MAX) "s %o %d %d",
		   path, target, &new_tree->mode, &new_tree->uid, &new_tree->gid) != 5) {
		add_perr(perrs, PWARN, "mic%d Bad attribute line 'slink %s'", id, line);
	}

	new_tree->type = MPSS_LINK;

	if ((new_tree->link = (char *) malloc(strlen(target) + 1)) == NULL) {
		free(new_tree);
		return;
	}

	strcpy(new_tree->link, target);
	new_tree->subdir = NULL;
	new_tree->next = NULL;

	insert_list(id, &path[1], new_tree, top, NULL, perrs);
}

void
add_simple_link(int id, char *source, char *target, char *lead, struct filetree *top, struct mpss_elist *perrs)
{
	struct filetree *new_tree;
	char path[PATH_MAX];
	char rbuf[PATH_MAX];
	char *filename = basename(source);

	if ((new_tree = (struct filetree *) malloc(sizeof(struct filetree))) == NULL) {
		return;
	}

	memset(new_tree, 0, sizeof(struct filetree));
	new_tree->type = MPSS_LINK;

	if ((new_tree->source = (char *) malloc(strlen(source) + 1)) == NULL) {
		free(new_tree);
		return;
	}
	strcpy(new_tree->source, source);

	memset(rbuf, 0, PATH_MAX);
	readlink(source, rbuf, PATH_MAX);
	if ((new_tree->link = (char *) malloc(strlen(rbuf) + 1)) == NULL) {
		free(new_tree->source);
		free(new_tree);
		return;
	}
	memset(new_tree->link, 0, strlen(rbuf) + 1);
	strcpy(new_tree->link, rbuf);

#ifdef SUPPORT_FILELIST
	new_tree->uid = -1;
#endif

	simple_path(path, PATH_MAX, target, lead, filename);
	new_tree->subdir = NULL;
	new_tree->next = NULL;

	insert_list(id, &path[1], new_tree, top, NULL, perrs);
}

void
add_dnode(int id, char *line, struct filetree *top, char *sourcedir, struct mpss_elist *perrs)
{
	struct filetree *new_tree;
	char path[PATH_MAX];

	if ((new_tree = (struct filetree *) malloc(sizeof(struct filetree))) == NULL) {
		return;
	}

	if (sscanf(line, "%" str(PATH_MAX) "s %o %d %d %c %u %u", path, &new_tree->mode, &new_tree->uid,
		   &new_tree->gid, &new_tree->inode_type, &new_tree->major, &new_tree->minor) != 7) {
		add_perr(perrs, PWARN, "mic%d Bad attribute line 'dir %s'", id, line);
	}

	new_tree->type = MPSS_DNODE;
	new_tree->subdir = NULL;
	new_tree->next = NULL;
	new_tree->prev = NULL;

	insert_list(id, &path[1], new_tree, top, NULL, perrs);
}

void
add_simple_nod(int id, char type, char *source, char *target, char *lead, struct filetree *top, struct mpss_elist *perrs)
{
	struct filetree *new_tree;
	char path[PATH_MAX];
	char *filename = basename(source);

	if ((new_tree = (struct filetree *) malloc(sizeof(struct filetree))) == NULL) {
		return;
	}

	new_tree->type = MPSS_DNODE;
	new_tree->source = NULL;
	new_tree->subdir = NULL;
	new_tree->next = NULL;
	new_tree->prev = NULL;

	if ((new_tree->source = (char *) malloc(strlen(source) + 1)) == NULL) {
		free(new_tree);
		return;
	}
	strcpy(new_tree->source, source);

#ifdef SUPPORT_FILELIST
	new_tree->uid = -1;
#endif
	new_tree->inode_type = type;
	simple_path(path, PATH_MAX, target, lead, filename);

	insert_list(id, &path[1], new_tree, top, NULL, perrs);
}

void
add_pipe(int id, char *line, struct filetree *top, char *sourcedir, struct mpss_elist *perrs)
{
	struct filetree *new_tree;
	char path[PATH_MAX];

	if ((new_tree = (struct filetree *) malloc(sizeof(struct filetree))) == NULL) {
		return;
	}

	if (sscanf(line, "%" str(PATH_MAX) "s %o %d %d", path, &new_tree->mode, &new_tree->uid, &new_tree->gid) != 4) {
		add_perr(perrs, PWARN, "mic%d Bad attribute line 'dir %s'", id, line);
	}

	new_tree->type = MPSS_PIPE;
	new_tree->source = NULL;
	new_tree->subdir = NULL;
	new_tree->next = NULL;
	new_tree->prev = NULL;

	insert_list(id, &path[1], new_tree, top, NULL, perrs);
}

void
add_simple_pipe(int id, char *source, char *target, char *lead, struct filetree *top, struct mpss_elist *perrs)
{
	struct filetree *new_tree;
	char path[PATH_MAX];
	char *filename = basename(source);

	if ((new_tree = (struct filetree *) malloc(sizeof(struct filetree))) == NULL) {
		return;
	}

	new_tree->type = MPSS_PIPE;
	new_tree->source = NULL;
	new_tree->subdir = NULL;
	new_tree->next = NULL;
	new_tree->prev = NULL;

	if ((new_tree->source = (char *) malloc(strlen(source) + 1)) == NULL) {
		free(new_tree);
		return;
	}
	strcpy(new_tree->source, source);

#ifdef SUPPORT_FILELIST
	new_tree->uid = -1;
#endif
	simple_path(path, PATH_MAX, target, lead, filename);

	insert_list(id, &path[1], new_tree, top, NULL, perrs);
}

void
add_sock(int id, char *line, struct filetree *top, char *sourcedir, struct mpss_elist *perrs)
{
	struct filetree *new_tree;
	char path[PATH_MAX];

	if ((new_tree = (struct filetree *) malloc(sizeof(struct filetree))) == NULL) {
		return;
	}

	if (sscanf(line, "%" str(PATH_MAX) "s %o %d %d", path, &new_tree->mode, &new_tree->uid, &new_tree->gid) != 4) {
		add_perr(perrs, PWARN, "mic%d Bad attribute line 'dir %s'", id, line);
	}

	new_tree->type = MPSS_SOCK;
	new_tree->source = NULL;
	new_tree->subdir = NULL;
	new_tree->next = NULL;
	new_tree->prev = NULL;

	insert_list(id, &path[1], new_tree, top, NULL, perrs);
}

void
add_simple_sock(int id, char *source, char *target, char *lead, struct filetree *top, struct mpss_elist *perrs)
{
	struct filetree *new_tree;
	char path[PATH_MAX];
	char *filename = basename(source);

	if ((new_tree = (struct filetree *) malloc(sizeof(struct filetree))) == NULL) {
		return;
	}

	new_tree->type = MPSS_SOCK;
	new_tree->source = NULL;
	new_tree->subdir = NULL;
	new_tree->next = NULL;
	new_tree->prev = NULL;

	if ((new_tree->source = (char *) malloc(strlen(source) + 1)) == NULL) {
		free(new_tree);
		return;
	}
	strcpy(new_tree->source, source);

#ifdef SUPPORT_FILELIST
	new_tree->uid = -1;
#endif
	simple_path(path, PATH_MAX, target, lead, filename);

	insert_list(id, &path[1], new_tree, top, NULL, perrs);
}

void
push_rest(char *name, FILE *fp, unsigned int *inode, unsigned int *offset)
{
	unsigned int name_len = strlen(name) + 1;
	unsigned int tmp;

	fputs(name, fp);
	fputc('\0', fp);
	*offset += name_len;

	tmp = name_len + 110;
	while (tmp & 3) {
		fputc('\0', fp);
		(*offset)++;
		tmp++;
	}
}

void
gen_dir(struct mpss_env *menv, struct filetree *d, char *leader, FILE *fp, unsigned int *inode, unsigned int *offset, struct mpss_elist *perrs)
{
	time_t mtime = time(NULL);
	unsigned int name_len;
	char name[PATH_MAX];
	struct stat sbuf;

	name_len = snprintf(name, PATH_MAX, "%s/%s", leader, d->name);

#ifdef SUPPORT_FILELIST
	if (d->uid != -1) {
		sbuf.st_uid = d->uid;
		sbuf.st_gid = d->gid;
		sbuf.st_mode = d->mode;
	} else {
#endif
	if (stat(d->source, &sbuf)) {
		add_perr(perrs, PERROR, "[GenDir] '/%s' Invalid source file %s: %s", d->name, d->source, strerror(errno));
		return;
	}
#ifdef SUPPORT_FILELIST
	}
#endif

	fprintf(fp,"%s%08X%08X%08lX%08lX%08X%08lX%08X%08X%08X%08X%08X%08X%08X",
		"070701",		/* magic */
		(*inode)++,		/* ino */
		(sbuf.st_mode & 0777) | S_IFDIR, /* mode */
		(long) sbuf.st_uid,	/* uid */
		(long) sbuf.st_gid,	/* gid */
		2,			/* nlink */
		(long) mtime,		/* mtime */
		0,			/* filesize */
		3,			/* major */
		1,			/* minor */
		0,			/* rmajor */
		0,			/* rminor */
		name_len,		/* namesize */
		0);			/* chksum */

	*offset += 110;
	push_rest(&name[1], fp, inode, offset);
	return;
}

void
gen_node(struct mpss_env *menv, struct filetree *n, char *leader, FILE *fp, unsigned int *inode, unsigned int *offset, struct mpss_elist *perrs)
{
	time_t mtime = time(NULL);
	unsigned int name_len;
	char name[PATH_MAX];
	struct stat sbuf;

	name_len = snprintf(name, PATH_MAX, "%s/%s", leader, n->name);

#ifdef SUPPORT_FILELIST
	if (n->uid != -1) {
		sbuf.st_uid = n->uid;
		sbuf.st_gid = n->gid;
		sbuf.st_mode = n->mode;
		sbuf.st_rdev = MKDEV(n->major, n->minor);
	} else {
#endif
	if (stat(n->source, &sbuf)) {
		add_perr(perrs, PERROR, "[GenNode] Invalid source file %s: %s", n->source, strerror(errno));
		return;
	}
#ifdef SUPPORT_FILELIST
	}
#endif

	if (n->inode_type == 'b')
		sbuf.st_mode = (sbuf.st_mode & 0777) | S_IFBLK;
	else
		sbuf.st_mode = (sbuf.st_mode & 0777) | S_IFCHR;

	fprintf(fp,"%s%08X%08X%08lX%08lX%08X%08lX%08X%08X%08X%08X%08X%08X%08X",
		"070701",		/* magic */
		(*inode)++,		/* ino */
		sbuf.st_mode,		/* mode */
		(long) sbuf.st_uid, 	/* uid */
		(long) sbuf.st_gid, 	/* gid */
		1,			/* nlink */
		(long) mtime,		/* mtime */
		0,			/* filesize */
		3,			/* major */
		1,			/* minor */
		(unsigned int)MAJOR(sbuf.st_rdev), /* rmajor */
		(unsigned int)MINOR(sbuf.st_rdev), /* rminor */
		name_len,		/* namesize */
		0);			/* chksum */

	*offset += 110;
	push_rest(&name[1], fp, inode, offset);
	return;
}

void
gen_pipe(struct mpss_env *menv, struct filetree *p, char *leader, FILE *fp, unsigned int *inode, unsigned int *offset, struct mpss_elist *perrs)
{
	time_t mtime = time(NULL);
	unsigned int name_len;
	char name[PATH_MAX];
	struct stat sbuf;

	name_len = snprintf(name, PATH_MAX, "%s/%s", leader, p->name);

#ifdef SUPPORT_FILELIST
	if (p->uid != -1) {
		sbuf.st_uid = p->uid;
		sbuf.st_gid = p->gid;
		sbuf.st_mode = p->mode;
	} else {
#endif
	if (stat(p->source, &sbuf)) {
		add_perr(perrs, PERROR, "[GenPipe] Invalid source file %s: %s", p->source, strerror(errno));
		return;
	}
#ifdef SUPPORT_FILELIST
	}
#endif


	fprintf(fp,"%s%08X%08X%08lX%08lX%08X%08lX%08X%08X%08X%08X%08X%08X%08X",
		"070701",		/* magic */
		(*inode)++,		/* ino */
		(sbuf.st_mode & 0777) | S_IFIFO, /* mode */
		(long) sbuf.st_uid, 	/* uid */
		(long) sbuf.st_gid, 	/* gid */
		2,			/* nlink */
		(long) mtime,		/* mtime */
		0,			/* filesize */
		3,			/* major */
		1,			/* minor */
		0,			/* rmajor */
		0,			/* rminor */
		name_len,		/* namesize */
		0);			/* chksum */

	*offset += 110;
	push_rest(&name[1], fp, inode, offset);
	return;
}

void
gen_sock(struct mpss_env *menv, struct filetree *s, char *leader, FILE *fp, unsigned int *inode, unsigned int *offset, struct mpss_elist *perrs)
{
	time_t mtime = time(NULL);
	unsigned int name_len;
	char name[PATH_MAX];
	struct stat sbuf;

	name_len = snprintf(name, PATH_MAX, "%s/%s", leader, s->name);

#ifdef SUPPORT_FILELIST
	if (s->uid != -1) {
		sbuf.st_uid = s->uid;
		sbuf.st_gid = s->gid;
		sbuf.st_mode = s->mode;
	} else {
#endif
	if (stat(s->source, &sbuf)) {
		add_perr(perrs, PERROR, "[GenSock] Invalid source file %s: %s", s->source, strerror(errno));
		return;
	}
#ifdef SUPPORT_FILELIST
	}
#endif

	fprintf(fp,"%s%08X%08X%08lX%08lX%08X%08lX%08X%08X%08X%08X%08X%08X%08X",
		"070701",		/* magic */
		(*inode)++,		/* ino */
		(sbuf.st_mode & 0777) | S_IFSOCK, /* mode */
		(long) sbuf.st_uid, 	/* uid */
		(long) sbuf.st_gid, 	/* gid */
		2,			/* nlink */
		(long) mtime,		/* mtime */
		0,			/* filesize */
		3,			/* major */
		1,			/* minor */
		0,			/* rmajor */
		0,			/* rminor */
		name_len,		/* namesize */
		0);			/* chksum */
	*offset += 110;
	push_rest(&name[1], fp, inode, offset);
	return;
}

void
gen_slink(struct mpss_env *menv, struct filetree *l, char *leader, FILE *fp, unsigned int *inode, unsigned int *offset, struct mpss_elist *perrs)
{
	unsigned int target_len = strlen(l->link) + 1;
	time_t mtime = time(NULL);
	unsigned int name_len;
	char name[PATH_MAX];
	struct stat sbuf;

	name_len = snprintf(name, PATH_MAX, "%s/%s", leader, l->name);

#ifdef SUPPORT_FILELIST
	if (l->uid != -1) {
		sbuf.st_uid = l->uid;
		sbuf.st_gid = l->gid;
		sbuf.st_mode = l->mode;
	} else {
#endif
	if (lstat(l->source, &sbuf)) {
		add_perr(perrs, PERROR, "[GenSlink] Invalid source file %s: %s", l->source, strerror(errno));
		return;
	}
#ifdef SUPPORT_FILELIST
	}
#endif

	fprintf(fp,"%s%08X%08X%08lX%08lX%08X%08lX%08X%08X%08X%08X%08X%08X%08X",
		"070701",		/* magic */
		(*inode)++,		/* ino */
		(sbuf.st_mode & 0777) | S_IFLNK, /* mode */
		(long) sbuf.st_uid, 	/* uid */
		(long) sbuf.st_gid,	/* gid */
		1,			/* nlink */
		(long) mtime,		/* mtime */
		target_len,		/* filesize */
		3,			/* major */
		1,			/* minor */
		0,			/* rmajor */
		0,			/* rminor */
		name_len,		/* namesize */
		0);			/* chksum */
	*offset += 110;

	fputs(&name[1], fp);
	fputc('\0', fp);
	*offset += name_len;

	while (*offset & 3) {
		fputc('\0', fp);
		(*offset)++;
	}

	fputs(l->link, fp);
	fputc('\0', fp);
	*offset += target_len;

	while (*offset & 3) {
		fputc('\0', fp);
		(*offset)++;
	}
}

void
gen_file(struct mpss_env *menv, struct filetree *f, char *leader, FILE *fp, unsigned int *inode, unsigned int *offset,
	 struct mpss_elist *perrs)
{
	char name[PATH_MAX];
	char src[PATH_MAX];
	struct stat sbuf;
	struct stat lbuf;
	time_t mtime = time(NULL);
	int fd = -1;
	char *filebuf = NULL;
	unsigned int name_len;
	ssize_t rsize;;

	snprintf(name, PATH_MAX, "%s/%s", leader, f->name);
	name_len = strlen(name);

	if (!mpssut_filename(menv, NULL, src, PATH_MAX, "%s", f->source)) {
		add_perr(perrs, PERROR, "Invalid source file: %s", f->source);
		return;
	}

	if ((fd = open(src, O_RDONLY)) < 0) {
		add_perr(perrs, PERROR, "Could not open source file: %s", f->source);
		return;
	}

	if (fstat(fd, &sbuf) != 0) {
		add_perr(perrs, PERROR, "CPIO gen %s fstat failed: %s", f->source, strerror(errno));
		close(fd);
		return;
	}

	if (lstat(src, &lbuf) != 0) {
		add_perr(perrs, PERROR, "CPIO gen %s lstat failed: %s", f->source, strerror(errno));
		close(fd);
		return;
	}

	if (S_ISLNK(lbuf.st_mode)) {
		add_perr(perrs, PERROR, "CPIO gen %s file has changed to a symbolic link: %s", f->source, strerror(errno));
		close(fd);
		return;
	}

	if (sbuf.st_size > ((unsigned int)0xffffffff - 1)) {
		add_perr(perrs, PERROR, "CPIO gen %s field width not sufficient for storing file size", f->source);
		close(fd);
		return;
	}

	if ((filebuf = (char *) malloc(sbuf.st_size)) == NULL) {
		add_perr(perrs, PERROR, "CPIO gen %s: malloc faileds", f->source);
		close(fd);
		return;
	}

	if ((rsize = read(fd, filebuf, sbuf.st_size)) != sbuf.st_size) {
		add_perr(perrs, PERROR, "CPIO gen %s: read size %zd does not match stat size %jd",
			 f->source, rsize, sbuf.st_size);
		free(filebuf);
		close(fd);
		return;
	}

#ifdef SUPPORT_FILELIST
	if (f->uid != -1) {
		sbuf.st_uid = f->uid;
		sbuf.st_gid = f->gid;
		sbuf.st_mode = f->mode;
	}
#endif

	fprintf(fp,"%s%08X%08X%08lX%08lX%08X%08lX%08lX%08X%08X%08X%08X%08X%08X",
		"070701",		/* magic */
		(*inode)++,		/* ino */
		(sbuf.st_mode & 0777) | S_IFREG, /* mode */
		(long) sbuf.st_uid, 	/* uid */
		(long) sbuf.st_gid, 	/* gid */
		1,			/* nlink */
		(long) mtime,		/* mtime */
		rsize,			/* filesize */
		3,			/* major */
		1,			/* minor */
		0,			/* rmajor */
		0,			/* rminor */
		name_len,		/* namesize */
		0);			/* chksum */
	*offset += 110;

	fputs(&name[1], fp);
	fputc('\0', fp);
	*offset += name_len;

	while(*offset & 3) {
		fputc('\0', fp);
		(*offset)++;
	}

	fwrite(filebuf, sbuf.st_size, 1, fp);
	*offset += sbuf.st_size;
	while(*offset & 3) {
		fputc('\0', fp);
		(*offset)++;
	}

	free(filebuf);
	close(fd);
}

void
follow_dir(struct mpss_env *menv, struct filetree *elem, char *leader, FILE *fp, unsigned int *inode, unsigned int *offset, struct mpss_elist *perrs)
{
	char *newleader;

	while (elem != NULL) {
		switch (elem->type) {
		case MPSS_DIR:
			gen_dir(menv, elem, leader, fp, inode, offset, perrs);
			if ((newleader = (char *) malloc(strlen(leader) + strlen(elem->name) + 2)) == NULL) {
				return;
			}
			sprintf(newleader, "%s/%s", leader, elem->name);
			follow_dir(menv, elem->subdir, newleader, fp, inode, offset, perrs);
			free(newleader);
			break;

		case MPSS_FILE:
			gen_file(menv, elem, leader, fp, inode, offset, perrs);
			break;

		case MPSS_LINK:
			gen_slink(menv, elem, leader, fp, inode, offset, perrs);
			break;

		case MPSS_DNODE:
			gen_node(menv, elem, leader, fp, inode, offset, perrs);
			break;

		case MPSS_PIPE:
			gen_pipe(menv, elem, leader, fp, inode, offset, perrs);
			break;

		case MPSS_SOCK:
			gen_sock(menv, elem, leader, fp, inode, offset, perrs);
			break;
		}

		elem = elem->next;
	}
}

void
cpio_trailer(FILE *fp, unsigned int *inode, unsigned int *offset)
{
	char *name = "TRAILER!!!";
	unsigned int name_len = strlen(name) + 1;

	fprintf(fp,"%s%08X%08X%08lX%08lX%08X%08lX%08X%08X%08X%08X%08X%08X%08X",
		"070701",		/* magic */
		0,			/* ino */
		0,			/* mode */
		(long) 0,		/* uid */
		(long) 0,		/* gid */
		1,			/* nlink */
		(long) 0,		/* mtime */
		0,			/* filesize */
		0,			/* major */
		0,			/* minor */
		0,			/* rmajor */
		0,			/* rminor */
		name_len,		/* namesize */
		0);			/* chksum */
	*offset += 110;
	push_rest(name, fp, inode, offset);

	while(*offset % 512) {
		fputc('\0', fp);
		(*offset)++;
	}
}

void
gzip_image(char *name)
{
	pid_t pid;
	char *ifargv[3];
	int status;

	pid = fork();
	if (pid == 0) {
		ifargv[0] = "/bin/gzip";
		ifargv[1] = name;
		ifargv[2] = NULL;
		execve("/bin/gzip", ifargv, NULL);
	}

	waitpid(pid, &status, 0);
}

void
gunzip_image(char *name)
{
	pid_t pid;
	char *ifargv[4];

	pid = fork();
	if (pid == 0) {
		ifargv[0] = "/bin/gzip";
		ifargv[1] = "-d";
		ifargv[2] = name;
		ifargv[3] = NULL;
		execve("/bin/gzip", ifargv, NULL);
	}

	waitpid(pid, NULL, 0);
}

int
mpssfs_cpioin(char *cfile, char *cdir, int dousr)
{
	pid_t pid;
	char *ifargv[6];
	int status;

	pid = fork();
	if (pid == 0) {

		if (chdir(cdir) < 0)
			return 1;

		fclose(stdout);
		fclose(stderr);
		ifargv[0] = "/bin/cpio";
		ifargv[1] = "-i";
		ifargv[2] = "-F";
		ifargv[3] = cfile;
		if (dousr) {
			ifargv[4] = "u*";
			ifargv[5] = NULL;
		} else {
			ifargv[4] = NULL;
		}
		execve("/bin/cpio", ifargv, NULL);
	}

	if (waitpid(pid, &status, 0) < 0)
		return status;

	return 0;
}

int
mpssfs_cpioout(char *cfile, char *cdir)
{
	char command[PATH_MAX];
	char cwd[PATH_MAX];

	if (getcwd(cwd, PATH_MAX) < 0)
		return 1;

	if (chdir(cdir) < 0)
		return 1;

	snprintf(command, PATH_MAX, "find . -print | cpio --create --format='newc' --quiet -F %s", cfile);

	if (system(command))
		return 1;

	return 0;
}

int
gen_cpio(struct mpss_env *menv, struct mic_info *mic, struct filetree *top, struct mpss_elist *perrs)
{
	char gzname[PATH_MAX];
	char cpioname[PATH_MAX];
	FILE *cpiofp;
	mode_t oldmask;
	struct stat sbuf;
	unsigned int inode = 721;  // simply somewhere to start
	unsigned int offset = 0;

	if (mpssut_filename(menv, NULL, gzname, PATH_MAX, "%s", mic->config.rootdev.target))
		unlink(gzname);

	if (strcmp(&gzname[strlen(gzname) - 3], ".gz")) {
		add_perr(perrs, PERROR, "mic%s Image file '%s' must end in '.gz'",
				mic->name, mic->config.rootdev.target);
		return 1;
	}

	strcpy(cpioname, gzname);
	cpioname[strlen(cpioname) - 3] = '\0';
	if (stat(cpioname, &sbuf) == 0)
		unlink(cpioname);

	if (mic->config.filesrc.base.type == SRCTYPE_CPIO) {
		mpssfs_unzip_base_cpio(mic->name, mic->config.filesrc.base.image, gzname, perrs);
	}

	oldmask = umask(S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
	if ((cpiofp = fopen(cpioname, "a")) == NULL) {
		umask(oldmask);
		return errno;
	}

	follow_dir(menv, top->subdir, "", cpiofp, &inode, &offset, perrs);
	cpio_trailer(cpiofp, &inode, &offset);

	fclose(cpiofp);
	umask(oldmask);
	gzip_image(cpioname);
	return 0;
}

void
copy_file(struct mpss_env *menv, struct filetree *f, char *leader, struct mpss_elist *perrs)
{
	struct stat sbuf;
	char to[PATH_MAX];
	char *buffer;
	int rsize;
	int fd;

	if (mpssut_filename(menv, NULL, to, PATH_MAX, "%s/%s", leader, f->name))
		unlink(to);

	if ((fd = open(f->source, O_RDONLY)) < 0) {
		add_perr(perrs, PERROR, "Cannot open source file %s: %s", f->source, strerror(errno));
		return;
	}

	if (fstat(fd, &sbuf) != 0) {
		add_perr(perrs, PERROR, "Cannot find source file %s: %s", f->source, strerror(errno));
		close(fd);
		return;
	}

#ifdef SUPPORT_FILELIST
	if (f->uid != -1) {
		sbuf.st_uid = f->uid;
		sbuf.st_gid = f->gid;
		sbuf.st_mode = f->mode;
	}
#endif

	if ((buffer = (char *) malloc(sbuf.st_size + 1)) == NULL) {
		add_perr(perrs, PERROR, "Memory allocation error: %s", strerror(errno));
		close(fd);
		return;
	}

	rsize = read(fd, buffer, sbuf.st_size);
	close(fd);

	if ((fd = open(to, O_WRONLY|O_CREAT, sbuf.st_mode & 0777)) < 0) {
		add_perr(perrs, PERROR, "Cannot open destination file %s: %s", to, strerror(errno));
		free(buffer);
		return;
	}

	write(fd, buffer, rsize);
	fchown(fd, sbuf.st_uid, sbuf.st_gid);
	close(fd);
	free(buffer);
}

void
copy_slink(struct mpss_env *menv, struct filetree *l, char *leader, struct mpss_elist *perrs)
{
	char name[PATH_MAX];
	struct stat sbuf;

	if (mpssut_filename(menv, NULL, name, PATH_MAX, "%s/%s", leader, l->name))
		unlink(name);

#ifdef SUPPORT_FILELIST
	if (l->uid != -1) {
		sbuf.st_uid = l->uid;
		sbuf.st_gid = l->gid;
		sbuf.st_mode = l->mode;
	} else {
#endif
	if (lstat(l->source, &sbuf)) {
		add_perr(perrs, PERROR, "[CopySlink] Invalid source file %s: %s", l->source, strerror(errno));
		return;
	}
#ifdef SUPPORT_FILELIST
	}
#endif

	if (!symlink(l->link, name)) {
		chown(name, sbuf.st_uid, sbuf.st_gid);
		chmod(name, sbuf.st_mode & 0777);
	} else {
		add_perr(perrs, PERROR, "Error making symlink %s: %d", name, errno);
	}
}

void
copy_node(struct mpss_env *menv, struct filetree *n, char *leader, struct mpss_elist *perrs)
{
	char name[PATH_MAX];
	struct stat sbuf;

	if (mpssut_filename(menv, NULL, name, PATH_MAX, "%s/%s", leader, n->name))
		unlink(name);

#ifdef SUPPORT_FILELIST
	if (n->uid != -1) {
		sbuf.st_uid = n->uid;
		sbuf.st_gid = n->gid;
		sbuf.st_mode = n->mode;
		sbuf.st_dev = MKDEV(n->major, n->minor);
	} else {
#endif
	if (stat(n->source, &sbuf)) {
		add_perr(perrs, PERROR, "[CopyNode] Invalid source file %s: %s", n->source, strerror(errno));
		return;
	}
#ifdef SUPPORT_FILELIST
	}
#endif

	if (n->inode_type == 'b')
		sbuf.st_mode = (sbuf.st_mode & 0777) | S_IFBLK;
	else
		sbuf.st_mode = (sbuf.st_mode & 0777) | S_IFCHR;

	if (!mknod(name, sbuf.st_mode, sbuf.st_dev))
		chown(name, sbuf.st_uid, sbuf.st_gid);
	else
		add_perr(perrs, PERROR, "Error making node %s: %s", name, strerror(errno));
}

void
copy_pipe(struct mpss_env *menv, struct filetree *p, char *leader, struct mpss_elist *perrs)
{
	char name[PATH_MAX];
	struct stat sbuf;

	if (mpssut_filename(menv, NULL, name, PATH_MAX, "%s/%s", leader, p->name))
		unlink(name);

#ifdef SUPPORT_FILELIST
	if (p->uid != -1) {
		sbuf.st_uid = p->uid;
		sbuf.st_gid = p->gid;
		sbuf.st_mode = p->mode;
	} else {
#endif
	if (stat(p->source, &sbuf)) {
		add_perr(perrs, PERROR, "[CopyPipe] Invalid source file %s: %s", p->source, strerror(errno));
		return;
	}
#ifdef SUPPORT_FILELIST
	}
#endif

	if (!mknod(name, (sbuf.st_mode & 0777) | S_IFIFO, 0))
		chown(name, sbuf.st_uid, sbuf.st_gid);
	else
		add_perr(perrs, PERROR, "Error making pipe %s: %d", name, errno);
}

void
copy_sock(struct mpss_env *menv, struct filetree *s, char *leader, struct mpss_elist *perrs)
{
	char name[PATH_MAX];
	struct stat sbuf;

	if (mpssut_filename(menv, NULL, name, PATH_MAX, "%s/%s", leader, s->name))
		unlink(name);

#ifdef SUPPORT_FILELIST
	if (s->uid != -1) {
		sbuf.st_uid = s->uid;
		sbuf.st_gid = s->gid;
		sbuf.st_mode = s->mode;
	} else {
#endif
	if (stat(s->source, &sbuf)) {
		add_perr(perrs, PERROR, "[CopySock] Invalid source file %s: %s", s->source, strerror(errno));
		return;
	}
#ifdef SUPPORT_FILELIST
	}
#endif

	if (!mknod(name, (sbuf.st_mode & 0777) | S_IFSOCK, 0))
		chown(name, sbuf.st_uid, sbuf.st_gid);
	else
		add_perr(perrs, PERROR, "Error making node %s: %d", name, errno);
}

void
copy_dir(struct mpss_env *menv, struct filetree *elem, char *leader, int level, struct mpss_elist *perrs)
{
	char name[PATH_MAX];
	char *newleader;
	struct stat sbuf;

	while (elem != NULL) {
		switch (elem->type) {
		case MPSS_DIR:
			if (!mpssut_filename(menv, NULL, name, PATH_MAX, "%s/%s", leader, elem->name)) {
#ifdef SUPPORT_FILELIST
				if (elem->uid != -1) {
					sbuf.st_uid = elem->uid;
					sbuf.st_gid = elem->gid;
					sbuf.st_mode = elem->mode;
				} else {
#endif
				if (stat(elem->source, &sbuf)) {
					add_perr(perrs, PERROR, "[CopyDir] Invalid source file: %s", elem->source);
					return;
				}
#ifdef SUPPORT_FILELIST
				}
#endif
				if (!mkdir(name, sbuf.st_mode & 0777)) {
					chown(name, sbuf.st_uid, sbuf.st_gid);
				} else {
					add_perr(perrs, PERROR, "[CopyDir] Error creating dir %s: %s", name, strerror(errno));
					return;
				}
			}

			if ((newleader = (char *) malloc(strlen(leader) + strlen(elem->name) + 2)) == NULL)
				return;

			sprintf(newleader, "%s/%s", leader, elem->name);
			copy_dir(menv, elem->subdir, newleader, level + 1, perrs);
			free(newleader);
			break;

		case MPSS_FILE:
			copy_file(menv, elem, leader, perrs);
			break;

		case MPSS_LINK:
			copy_slink(menv, elem, leader, perrs);
			break;

		case MPSS_DNODE:
			copy_node(menv, elem, leader, perrs);
			break;

		case MPSS_PIPE:
			copy_pipe(menv, elem, leader, perrs);
			break;

		case MPSS_SOCK:
			copy_sock(menv, elem, leader, perrs);
			break;
		}

		elem = elem->next;
	}
}

int
copy_nfs(struct mpss_env *menv, struct mic_info *mic, struct filetree *top, int createusr, struct mpss_elist *perrs)
{
	char src[PATH_MAX];
	char orig[PATH_MAX];
	char dest[PATH_MAX];
	char *tempname;
	char usrbase[PATH_MAX];
	char *lastslash;
	char *newleader;
	struct stat sbuf;
	struct filetree *elem = top->subdir;
	char *leader = NULL;

	if (mic->config.filesrc.base.type == SRCTYPE_CPIO) {
		if ((tempname = mpssut_tempnam(strchr(mic->config.rootdev.target, ':') + 1)) == NULL) {
			return 1;
		}

		mpssut_filename(menv, NULL, dest, PATH_MAX, "%s.gz", tempname);
		mpssut_filename(menv, NULL, src, PATH_MAX, "%s", tempname);
		free(tempname);
		mpssut_filename(menv, NULL, orig, PATH_MAX, "%s", mic->config.filesrc.base.image);
		mpssfs_unzip_base_cpio(mic->name, orig, dest, perrs);

		mpssut_filename(menv, NULL, dest, PATH_MAX, "%s", strchr(mic->config.rootdev.target, ':') + 1);

		if (createusr) {
			char usrdir[] = {"/usr"};
			mpssut_filename(menv, NULL, usrbase, PATH_MAX, "%s", strchr(mic->config.rootdev.nfsusr, ':') + 1);
			if ((lastslash = strrchr(dest, '/')) != NULL)
				*lastslash = '\0';
			mpssfs_cpioin(src, dest, TRUE);
			if (strlen(dest) < (PATH_MAX - strlen(usrdir))) {
				strcat(dest, usrdir);
			} else {
				add_perr(perrs, PERROR, "[CopyNfs] destination: %s, user base: %s", dest, usrbase);
				return 1;
			}
			rename(dest, usrbase);
		} else {
			mpssut_filename(menv, NULL, dest, PATH_MAX, "%s", strchr(mic->config.rootdev.target, ':') + 1);
			mpssfs_cpioin(src, dest, FALSE);
		}

		unlink(src);
	}

	while (elem != NULL) {
		if (createusr) {
			if ((mic->config.rootdev.type == ROOT_TYPE_SPLITNFS) &&
			    !strcmp(elem->name, "usr")) {
				leader = strchr(mic->config.rootdev.nfsusr, ':') + 1;
				copy_dir(menv, elem->subdir, leader, 1, perrs);
				return 0;
			} else {
				elem = elem->next;
				continue;
			}
		} else {
			if ((mic->config.rootdev.type == ROOT_TYPE_SPLITNFS) &&
			    !strcmp(elem->name, "usr") && leader) {
				if (!mpssut_filename(menv, &sbuf, src, PATH_MAX, "%s/usr", leader)) {
					if (!mkdir(src, elem->mode)) {
#ifdef SUPPORT_FILELIST
						if (elem->uid != -1) {
							sbuf.st_uid = elem->uid;
							sbuf.st_gid = elem->gid;
							sbuf.st_mode = elem->mode;
						} else {
#endif
						if (stat(elem->source, &sbuf)) {
							add_perr(perrs, PERROR, "[CopyDir] Invalid source file: %s",
									elem->source);
							return 1;
						}
#ifdef SUPPORT_FILELIST
						}
#endif

						chown(src, sbuf.st_uid, sbuf.st_gid);
						chmod(src, sbuf.st_mode & 0777);
					} else {
						add_perr(perrs, PERROR, "Error creating dir %s: %d", src, errno);
					}
				}


				elem = elem->next;
				continue;
			} else {
				leader = strchr(mic->config.rootdev.target, ':') + 1;
			}
		}

		switch (elem->type) {
		case MPSS_DIR:
			mpssut_filename(menv, NULL, dest, PATH_MAX, "%s/%s", leader, elem->name);
			if (stat(dest, &sbuf) != 0) {
#ifdef SUPPORT_FILELIST
				if (elem->uid != -1) {
					sbuf.st_uid = elem->uid;
					sbuf.st_gid = elem->gid;
					sbuf.st_mode = elem->mode;
				} else {
#endif
				if (stat(elem->source, &sbuf)) {
					add_perr(perrs, PERROR, "[CopyDir] Invalid source file: %s", elem->source);
					return 1;
				}
#ifdef SUPPORT_FILELIST
				}
#endif
				if (!mkdir(dest, sbuf.st_mode & 0777)) {

					chown(src, sbuf.st_uid, sbuf.st_gid);
				} else {
					add_perr(perrs, PERROR, "Error creating dir %s: %d", dest, errno);
					continue;
				}
			}


			if ((newleader = (char *) malloc(strlen(leader) + strlen(elem->name) + 2)) == NULL)
				continue;

			sprintf(newleader, "%s/%s", leader, elem->name);
			copy_dir(menv, elem->subdir, newleader, 1, perrs);
			free(newleader);
			break;

		case MPSS_FILE:
			copy_file(menv, elem, leader, perrs);
			break;

		case MPSS_LINK:
			copy_slink(menv, elem, leader, perrs);
			break;

		case MPSS_DNODE:
			copy_node(menv, elem, leader, perrs);
			break;

		case MPSS_PIPE:
			copy_pipe(menv, elem, leader, perrs);
			break;

		case MPSS_SOCK:
			copy_sock(menv, elem, leader, perrs);
			break;
		}

		elem = elem->next;
	}

	return 0;
}

