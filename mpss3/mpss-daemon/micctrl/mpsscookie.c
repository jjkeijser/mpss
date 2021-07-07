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
#include <limits.h>
#include <scif.h>
#include <pwd.h>
#include <mpssconfig.h>

int
mpss_sync_cookie(unsigned long *cookie, uid_t uid)
{
	struct scif_portID micID = {0, MPSSD_CRED};
	char cookiename[PATH_MAX];
	struct passwd *pass;
	unsigned int ack;
	scif_epd_t ep;
	int fd;

	if (*cookie)
		return 0;

	if ((ep = scif_open()) < 0) {
		return COOKIE_FAIL_CONNECT;
	}

	if (scif_connect(ep, &micID) < 0) {
		scif_close(ep);
		return COOKIE_FAIL_CONNECT;
	}

	if (scif_send(ep, &uid, sizeof(uid), SCIF_SEND_BLOCK) < 0) {
		scif_close(ep);
		return COOKIE_FAIL_SEND;
	}

	if (scif_recv(ep, &ack, sizeof(ack), SCIF_RECV_BLOCK) < 0) {
		scif_close(ep);
		return COOKIE_FAIL_RECV;
	}

	scif_close(ep);

	if ((pass = getpwuid(uid)) == NULL)
		return COOKIE_FAIL_UID;

	snprintf(cookiename, PATH_MAX, "%s/.mpsscookie", pass->pw_dir);
	if ((fd = open(cookiename, O_RDONLY)) < 0) {
		return COOKIE_FAIL_READ;
	}

	read(fd, cookie, sizeof(unsigned long));
	close(fd);
	return COOKIE_SUCCESS;
}

