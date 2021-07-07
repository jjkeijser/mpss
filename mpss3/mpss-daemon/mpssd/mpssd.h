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

#ifndef __MPSSD_H_
#define __MPSSD_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/poll.h>
#include <sys/statvfs.h>
#include <pthread.h>
#include <signal.h>
#include <getopt.h>
#include <execinfo.h>
#include <pwd.h>
#include <scif.h>
#include <mpssconfig.h>
#include <libmpsscommon.h>
#include "mpsstransfer.h"

#define LOGFILE_NAME "/var/log/mpssd"
void mpsslog(unsigned char level, char *format, ...) __attribute__((format(printf, 2, 3)));
void *init_mic(void *arg);
struct mpssd_info {
	char		*state;
	scif_epd_t	recv_ep;
	scif_epd_t	send_ep;
	scif_epd_t	cred_ep;
	pthread_mutex_t	pth_lock;
	pthread_mutex_t reset_lock;
	pthread_cond_t	reset_cond;
	pthread_t	boot_pth;
	pthread_t	state_pth;
	pthread_t	stop_pth;
	pthread_t	crash_pth;
	pthread_t	monitor_pth;
	pthread_t       config_thread;
	pid_t           pid;
	struct mic_console_info mic_console;
	struct mic_net_info     mic_net;
	struct mic_virtblk_info mic_virtblk;
	int             restart;
	int		reset_done;
	int             boot_on_resume;
};
extern struct mic_info *miclist;

#endif /* __MPSSD_H_ */
