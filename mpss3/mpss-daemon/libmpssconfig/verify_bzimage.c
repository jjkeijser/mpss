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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <wait.h>
#include <mpssconfig.h>
#include <libmpsscommon.h>

static void do_gunzip(char *name);

/*
 * verify_bzImage looks at key values in the file it is passed.  The values used
 * here were determined by looking at the values the "file" command uses.  In addition
 * this code actually unpacks the image and checks the elf header for the K1om machine type.
 */
int
verify_bzImage(struct mpss_env *menv, char *name, char *tag)
{
	char bzname[PATH_MAX];
	char tmpname[PATH_MAX];
	unsigned char buf[4096];
	unsigned char *found;
	char *temp = NULL;
	char *linuxHdr;
	off_t off;
	int ifd;
	int ofd;
	int num;
	int err = 0;

	if (!mpssut_filename(menv, NULL, bzname, PATH_MAX, "%s", name))
		return VBZI_ACCESS;

	if ((ifd = open(bzname, O_RDONLY)) < 0)
		return VBZI_ACCESS;

	if (read(ifd, buf, 4096) < 0) {
		close(ifd);
		err = VBZI_ACCESS;
		goto close_exit;
	}

	// First check checks for a Linux kernel
	linuxHdr = (char *)&buf[514];
	linuxHdr[4] = '\0';
	if (strcmp(linuxHdr, "HdrS")) {
		err = VBZI_FAIL_FORMAT;
		goto close_exit;
	}

	// Second check checks for x86 boot executable
	if ((buf[510] != 0x55) || ((unsigned char)buf[511] != 0xaa)) {
		err = VBZI_FAIL_FORMAT;
		goto close_exit;
	}

	// Must defines a bzImage type
	if (buf[529] != 1) {
		err = VBZI_FAIL_FORMAT;
		goto close_exit;
	}

	lseek(ifd, 0, SEEK_SET);
	while ((num = read(ifd, buf, 4096))) {
		if (num < 0) {
			err = VBZI_FAIL_TYPE;
			goto close_exit;
		}

		if ((found = (unsigned char *) memchr(buf, 0x1f, num)) != NULL) {
			if ((found[1] == 0x8b) && (found[2] == 0x8))
				goto write_tmpfile;
		
			// The signature may be elsewhere in this block
			off = lseek(ifd, 0, SEEK_CUR) - num;
			off += (found - buf) + 1;
			lseek(ifd, off, SEEK_SET);
		}
	}

	err = VBZI_FAIL_TYPE;
	goto close_exit;

write_tmpfile:
	if ((temp = tempnam(menv->vardir, tag)) == NULL) {
		err = VBZI_FAIL_OTHER;
		goto close_exit;
	}

	if (!mpssut_filename(menv, NULL, tmpname, PATH_MAX, "%s.gz", temp))
		unlink(tmpname);

	if ((ofd = open(tmpname, O_WRONLY|O_CREAT, 0777)) < 0) {
		err = VBZI_FAIL_TYPE;
		goto close_exit;
	}

	write(ofd, found, 4096 - (found - buf));

	while ((num = read(ifd, buf, 4096)) > 0)
		write(ofd, buf, num);

	close(ifd);
	close(ofd);

	do_gunzip(tmpname);

	mpssut_filename(menv, NULL, tmpname, PATH_MAX, "%s", temp);

	if ((ifd = open(tmpname, O_RDONLY)) < 0) {
		unlink(tmpname);
		err = VBZI_FAIL_TYPE;
		goto close_exit;
	}

	if ((read(ifd, buf, 40) < 0)) {
		unlink(tmpname);
		err = VBZI_FAIL_TYPE;
		goto close_exit;
	}

	/* Check for machine type (Xeon = 0x3e, k10m = 0xb5) */
	if (!(((unsigned char)buf[18] == 0x3e) || (unsigned char)buf[18] == 0xb5))
		err = VBZI_FAIL_TYPE;

	unlink(tmpname);
close_exit:
	if (temp) free(temp);
	close(ifd);
	return err;
}

static void
do_gunzip(char *name)
{
	pid_t pid;
	char *ifargv[4];

	pid = fork();
	if (pid == 0) {
		fclose(stdout);
		fclose(stderr);
		ifargv[0] = "/bin/gzip";
		ifargv[1] = "-d";
		ifargv[2] = name;
		ifargv[3] = NULL;
		execve("/bin/gzip", ifargv, NULL);
	}

	waitpid(pid, NULL, 0);
}

