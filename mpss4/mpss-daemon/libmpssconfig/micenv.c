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

#include "mpssconfig.h"
#include "libmpsscommon.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>


static bool
path_exists(const char* path)
{
	struct stat sbuf;
	return stat(path, &sbuf) == 0;
}

bool
path_exists(const std::string& path)
{
	return path_exists(path.c_str());
}

int
mpssenv_aquire_lockfile()
{
	struct flock fl;
	int lockfd;

	if ((lockfd = open(LSB_LOCK_FILENAME, O_WRONLY | O_CREAT, 0777)) < 0)
		return EBADF;

	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 0;

	if (fcntl(lockfd, F_SETLK, &fl))
		return EEXIST;

	return 0;
}

/*
 * checks if lock file is present and locked;
 * returns 0 when file present and locked;
 * returns non-zero otherwise.
 */
int
mpssenv_check_lockfile()
{
	int lock_fd;
	struct stat lock_state;
	struct flock lock_flock;
	int result = 0;

	/* can't open lock file */
	if ((lock_fd = open(LSB_LOCK_FILENAME, O_RDONLY)) < 0)
		return -1;

	/* fstat error; check returned errno for more info */
	if (fstat(lock_fd, &lock_state)) {
		result = errno;
		goto error;
	}

	/* not a regular file */
	if (!S_ISREG(lock_state.st_mode)) {
		result = -2;
		goto error;
	}

	/* fcntl error; check returned errno for more info */
	if (fcntl(lock_fd, F_GETLK, &lock_flock)) {
		result = errno;
		goto error;
	}

	/* lock file exists, but it's not locked */
	if (lock_flock.l_type != F_WRLCK) {
		result = -3;
	}

error:
	close(lock_fd);
	return result;
}
