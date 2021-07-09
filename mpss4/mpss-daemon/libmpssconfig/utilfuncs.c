/*
 * Copyright (c) 2016, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <mpssconfig.h>
#include <libmpsscommon.h>

#include <dirent.h>
#include <fcntl.h>
#include <linux/types.h>
#include <list>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

int
mpssut_filename(char *buffer, int maxlen, const char *format, ...)
{
	va_list args;
	struct stat tmpsbuf;

	va_start(args, format);
	vsnprintf(buffer, maxlen, format, args);
	va_end(args);

	if (lstat(buffer, &tmpsbuf) == 0)
		return 1;

	return 0;
}

std::string
mpssut_configname(const std::string& name)
{
	return DEFAULT_CONFDIR "/" + name + ".conf";
}

static int
mpssut_mkdir(char *dir, uid_t uid, gid_t gid, int mode)
{
	char dirname[PATH_MAX];

	if (mpssut_filename(dirname, PATH_MAX, "%s", dir))
		return EEXIST;

	if (mkdir(dirname, mode))
		return errno;

	chown(dirname, uid, gid);
	return 0;
}

struct specials {
	const char *name;
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
_mpssut_mksubtree(const char *dir, uid_t uid, gid_t gid, int mode)
{
	char buffer[PATH_MAX];
	char *slash;
	struct specials *sp = spdirs;
	int err;

	if (mpssut_filename(buffer, PATH_MAX, "%s", dir))
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

		switch ((err = _mpssut_mksubtree(buffer, uid, gid, mode))) {
		case 0:
			break;
		case EEXIST:
			return 0;
		default:
			return err;
		}
	}

	snprintf(buffer, PATH_MAX, "%s", dir);

	if ((err = mpssut_mkdir(buffer, uid, gid, mode)))
		return err;

	return 0;
}

int
mpssut_mksubtree(const char *dir, uid_t uid, gid_t gid, int mode)
{
	char dirname[PATH_MAX];
	if (mpssut_filename(dirname, PATH_MAX, "%s", dir))
		return EEXIST;

	return _mpssut_mksubtree(dir, uid, gid, mode);
}

int
mpssut_deltree(const char *dir)
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
				mpssut_deltree(subdirname);
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

std::string
get_user_home()
{
	const char *home = getenv("HOME");
	return home ? home : "";
}

/*
 * mpssut_rename(char* input, char* output)
 * - makes backup of output file if exists
 * - tries to rename input file into output file with rename() syscall
 * - if rename fails, it creates output file and copies content of input file into output file and removes input file
 * - if direct copying fails it restores the backup of output file
 */

int
mpssut_rename(const char* input, const char* output) {
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
	if (mpssut_filename(destfile, PATH_MAX, "%s", output)) {
		if (mpssut_filename(savefile, PATH_MAX, "%s.orig", destfile)) {
			unlink(savefile);
		}
		// if destination file exists move it to .orig file to allow recovery when rename fails
		if (rename(destfile, savefile)) {
			// failed to backup output file, break here
			return 1;
		}
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

static const uint8_t HEADER_PATTERN = 0xFF;
static const uint8_t HEADER_PATTERN_SIZE = 8;
static const int MAX_INT_24_BITS = 16777215;

/**
 * Log format:
 *  * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *  * |               BEGIN PATTERN (FF)              |
 *  * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *  * |                ORDINAL NUMBER                 |
 *  * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *  * |                 LOG CONTENT                   |
 *  * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *  * |                 END PATTERN (0)               |
 *  * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * where:
 * - begin pattern (always FF) - 1 byte
 * - ordinal number - 3 bytes
 * - end pattern (always 0) - 1 byte
 *
 */
static void mpss_parse_card_log(char *buf, size_t size, mpss_elist& perrs)
{
	uint32_t tmp;

	//max number of the message id, which can be saved in a 24 bits
	uint32_t first_log_id = MAX_INT_24_BITS;
	unsigned int first_log_index = 0;
	unsigned int messages_size = 0;

	bool is_msg_id_end = false;
	bool is_header_found = false;

	std::list<std::string> message_list;

	for (size_t i = 0; i < size; i++) {
		if ((buf[i] & HEADER_PATTERN) == HEADER_PATTERN) {
			//found header pattern, so take 24 bits as a message id
			tmp = *(uint32_t *)&buf[i];
			uint32_t msg_id = tmp >> HEADER_PATTERN_SIZE;

			if (msg_id == MAX_INT_24_BITS) {
				is_msg_id_end = true;
			}

			if (!is_msg_id_end && msg_id < first_log_id) {
				first_log_id = msg_id;
				first_log_index = messages_size;
			}
			is_header_found = true;
			//add 4 (message header) to the loop,
			i += 3;
			continue;
		}

		if (buf[i] != 0 && is_header_found) {
			is_header_found = false;
			message_list.emplace_back(&buf[i]);
			messages_size++;
			i += strlen(&buf[i]);
		}
	}

	/*
	 * Log on the list may not be finished with the \n sign,
	 * and those logs without \n sign, have to be printed in the same line
	 * as the log with the \n.
	 * Also we remove the \n sign from the log, because it is added
	 * when displaying logs from perrs list.
	 *
	 * List is divided into two parts, which are separated by the
	 * first_log_index variable - index of the first message (by time)
	 * to be displayed
	 */
	std::string complete_string;

	auto iter = message_list.begin();
	std::advance(iter, first_log_index);
	while (!message_list.empty()) {
		auto message = *iter;

		if (message.back() == '\n') {
			complete_string.append(message);
			complete_string.pop_back();
			perrs.add(PINFO, "%s", complete_string.c_str());
			complete_string.clear();
		} else {
			complete_string.append(message);
		}
		iter = message_list.erase(iter);

		//if there's end of list and there are still messages
		//we have to start from the beginning
		if (iter == message_list.end() && !message_list.empty()) {
			iter = message_list.begin();
		}
	}
}

int mpssut_print_card_log(struct mic_info *mic, mpss_elist& perrs)
{
	ssize_t rlen;
	char buffer[4096];
	size_t buffer_size = sizeof(buffer);
	char log_buf[PATH_MAX];
	int ifd;

	snprintf(log_buf, sizeof(log_buf), "/sys/class/mic/%s/log_buf",
		mic->name.c_str());

	if ((ifd = open(log_buf, O_RDONLY)) < 0) {
		return errno;
	}

	std::vector<char> v;

	while ((rlen = read(ifd, buffer, buffer_size)) > 0) {
		v.insert(v.end(), buffer, buffer + rlen);
	}

	if (rlen == -1)
		return errno;

	mpss_parse_card_log(&v[0], v.size(), perrs);
	close(ifd);

	return 0;
}
