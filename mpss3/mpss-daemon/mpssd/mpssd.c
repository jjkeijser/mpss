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

#include "mpssd.h"

struct mic_info *miclist;

void display_help(void);
void parse_cmd_args(int argc, char *argv[]);
void setsighandlers(void);
void start_daemon(void);
void *boot_mic(void *);
void *mic_state(void *arg);
void set_log_buf_info(struct mic_info *mic);
void mpsslog_dump(char *buf, int len);
void *mic_monitor(void *arg);
void * mic_credentials(void *arg);

char *pidfilename;
int lockfd;

struct mpss_env mpssenv;
pthread_mutex_t	start_lock = PTHREAD_MUTEX_INITIALIZER;
volatile int start_count = 0;
pid_t start_pid = 0;

pthread_mutex_t	shutdown_lock = PTHREAD_MUTEX_INITIALIZER;
int shutdown_count = 0;

FILE *logfp;

struct mbridge *brlist = NULL;

pthread_t mon_pth;
pthread_t cred_pth;


int
main(int argc, char *argv[])
{
	pid_t pid;

	mpssenv_init(&mpssenv);
	parse_cmd_args(argc, argv);
	setsighandlers();

	if (logfp != stderr) {
		if ((logfp = fopen(LOGFILE_NAME, "a+")) == NULL) {
			fprintf(stderr, "cannot open logfile '%s'\n", LOGFILE_NAME);
			exit(EBADF);
		}

	}

	mpsslog(PINFO, "MPSS Daemon start\n");

	if ((miclist = mpss_get_miclist(&mpssenv, NULL)) == NULL) {
		mpsslog(PINFO, "MIC module not loaded\n");
		exit(2);
	}

	if (logfp == stderr) {
		start_daemon();
	} else {
		start_pid = getpid();
		switch ((pid = fork())) {
		case -1:
			fprintf(stderr, "cannot fork: %s\n", strerror(errno));
			exit(ENOEXEC);
		case 0:
			start_daemon();
		default:
			pause();
		}
	}

	exit(0);
}

void
usage(int ret_code)
{
	exit(ret_code);
}

struct option longopts[] = {
	{"help", 0, NULL, 'h'},
	{"local", 0, NULL, 'l'},
	{"directory", 1, NULL, 'd'},
	{"pidfile", 1, NULL, 'p'},
	{0}
};

void
parse_cmd_args(int argc, char *argv[])
{
	int longidx;
	int err;
	int c;

	while ((c = getopt_long(argc, argv, "hld:p:", longopts, &longidx)) != -1) {
		switch(c) {
		case 'l':		// Local mode - don't fork and log to screen
			logfp = stderr;
			break;
		case 'h':
			display_help();

		case 'd':
			if ((err = mpssenv_set_configdir(&mpssenv, optarg))) {
				fprintf(stderr, "Unable to set confdir %s: %s\n", optarg, 
						mpssenv_errstr(err));
				exit(1);
			}
			break;

		case 'p':
			if ((pidfilename = (char *) malloc(strlen(optarg) + 1)) == NULL) {
				return;
			}
			strcpy(pidfilename, optarg);
			break;

		default:
			fprintf(stderr, "Unknown argument %c\n", c);
			usage(EINVAL);
		}
	}

	if (optind != argc) {
		fprintf(stderr, "Uknown argement '%s'\n", optarg);
		usage(EINVAL);
	}
}

char *hs = {
	"\nmpssd -h\n"
	"mpssd --help\n\n"
	"    Display help messages for the mpssd daemon\n\n"
	"mpssd <-l> <--local> <-d configdir> <--directory=configdir>\n\n"
	"    This is the MPSS stack MIC card daemon.  When started it parses the MIC\n"
	"    card configuration files in the " DEFAULT_CONFDIR " directory and sets\n"
	"    various parameters in the /sys/class/mic sysfs entries.  These entries\n"
	"    include the kernel command line paramters to pass to the embedded Linux OS\n"
	"    running on the MIC card.\n\n"
	"    The mpssd daemon then indicates the cards marked with 'BootOnStart'\n"
	"    enabled should start booting.  When complete it waits for the MIC cards\n"
	"    boot process to request their file systems to be downloaded.\n\n"
	"    To stop the mpssd daemon, a signal 3 (QUIT) is sent to the daemon's pid.\n\n"
	"    If the '-l' or '--local' options are not specified the mpssd will fork\n"
	"    a child and detach the parent to wait for file system download requests.\n"
	"    It keeps a log in /var/log/mpssd.\n\n"
	"    If local mode is indicated with the '-l' or '--local' options, the log\n"
	"    messages are displayed to stderr and mpssd runs in foreground instead of\n"
	"    forking into background mode.\n\n"
	"    The default directory for finding configuration parameters is\n"
	"    " DEFAULT_CONFDIR ".  The '-d' or '--directory' options allow the\n"
	"    configuration directory to be set to another location for testing\n"
	"    variants of configuration files.\n"
};

void
display_help(void)
{
	printf("%s\n", hs);
	exit(0);
}

void *
shutdown_mic(void *arg)
{
	struct mic_info *mic = (struct mic_info *)arg;
	struct mpssd_info *mpssdi = (struct mpssd_info *)mic->data;
	int timeout = atoi(mic->config.misc.shutdowntimeout);
	char *state = mpss_readsysfs(mic->name, "state");
	char *mode;
	int err;

	mode = mpss_readsysfs(mic->name, "mode");

	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

	while (pthread_mutex_lock(&mpssdi->pth_lock) != 0);

	if (mpssdi->boot_pth)
		pthread_cancel(mpssdi->boot_pth);

	if (mpssdi->monitor_pth)
		pthread_cancel(mpssdi->monitor_pth);

	if (mpssdi->state_pth)
		pthread_cancel(mpssdi->state_pth);

	if (mpssdi->crash_pth)
		pthread_cancel(mpssdi->crash_pth);

	while (pthread_mutex_unlock(&mpssdi->pth_lock) != 0);

	if (state == NULL) {
		mpsslog(PERROR, "%s: Failed to read state of card - Leaving in current state.\n", mic->name);
	} else {
		if (!timeout || strcmp(state, "online")) {
			mpsslog(PINFO, "%s: Forced Reset\n", mic->name);
		} else if ((mode == NULL) || (strcmp(mode, "linux"))) {
			if ((err = mpss_setsysfs(mic->name, "state", "reset")) != 0) {
				mpsslog(PERROR, "%s: Failed to set state of card - Leaving in current state: %s\n",
					mic->name, strerror(err));
			} else {
				mpsslog(PINFO, "%s: Resetting online mode %s\n", mic->name, mode);
			}
		} else {
			mpsslog(PINFO, "%s: Shutting down. Timeout %d\n", mic->name, timeout);
			if ((err = mpss_setsysfs(mic->name, "state", "shutdown")) != 0) {
				mpsslog(PERROR, "%s: Failed to set state of card - Leaving in current state: %s\n",
					mic->name, strerror(err));
			} else {
				mpsslog(PINFO, "%s: Shutting down. Timeout %d\n", mic->name, timeout);
				/* In KNL, mpssd resets the card after a shutdown. This is different from
				 * KNC production stack in which the driver reset the card.
				 * The timedwait here is signaled by the mic_config thread after performing
				 * the reset
				 */
			}
		}
	}

	while (pthread_mutex_lock(&shutdown_lock) != 0);
	if (--shutdown_count == 0)
		exit(0);

	while (pthread_mutex_unlock(&shutdown_lock) != 0);
	while (pthread_mutex_lock(&mpssdi->pth_lock) != 0);
	mpssdi->stop_pth = 0;
	while (pthread_mutex_unlock(&mpssdi->pth_lock) != 0);
	pthread_exit(NULL);
}

void
quit_handler(int sig, siginfo_t *siginfo, void *context)
{
	struct mic_info *mic;
	struct mpssd_info *mpssdi;
	struct mpss_elist mpssperr;
	char *state;

	shutdown_count = 0;
	while (pthread_mutex_lock(&shutdown_lock) != 0);

	mpsslog(PINFO, "MPSS Stack Shutting down\n");
	for (mic = miclist; mic != NULL; mic = mic->next) {
		mpssdi = (struct mpssd_info *)mic->data;

		if (mic->present == FALSE) {
			mpsslog(PWARN, "%s: Configured but not present - skipping\n", mic->name);
			continue;
		}

		if ((state = mpss_readsysfs(mic->name, "state")) == NULL) {
			mpsslog(PERROR, "%s: Failed to read state - not waiting for card ready\n", mic->name);
			while (pthread_mutex_lock(&mpssdi->pth_lock) != 0);
			pthread_create(&mpssdi->stop_pth, NULL, shutdown_mic, mic);
			while (pthread_mutex_unlock(&mpssdi->pth_lock) != 0);
			continue;
		}

		mpss_parse_config(&mpssenv, mic, &brlist, &mpssperr);
		mpss_clear_elist(&mpssperr);
		if (strcmp(state, "ready") && strcmp(state, "resetting")) {
			shutdown_count++;
			while (pthread_mutex_lock(&mpssdi->pth_lock) != 0);
			pthread_create(&mpssdi->stop_pth, NULL, shutdown_mic, mic);
			while (pthread_mutex_unlock(&mpssdi->pth_lock) != 0);
		}
		free(state);
	}

	while (pthread_mutex_unlock(&shutdown_lock) != 0);

	if (shutdown_count == 0)
		exit(0);
	else
		pause();
}

void
parent_handler(int sig, siginfo_t *siginfo, void *context)
{
	if (start_pid == getpid())
		exit(0);
}

void
segv_handler(int sig, siginfo_t *siginfo, void *context)
{
	struct mic_info *mic;
	struct mpssd_info *mpssdi;
	void *addrs[100];
	char **funcs;
	void *joinval;
	int cnt;
	int i;

	cnt = backtrace(addrs, 100);
	funcs = backtrace_symbols(addrs, cnt);

	for (mic = miclist; mic != NULL; mic = mic->next) {
		mpssdi = (struct mpssd_info *)mic->data;

		if (mpssdi->boot_pth && (mpssdi->boot_pth != pthread_self())) {
			pthread_cancel(mpssdi->boot_pth);
			pthread_join(mpssdi->boot_pth, &joinval);
		}

		if (mpssdi->monitor_pth && (mpssdi->monitor_pth != pthread_self())) {
			pthread_cancel(mpssdi->monitor_pth);
			pthread_join(mpssdi->monitor_pth, &joinval);
		}

		if (mpssdi->state_pth && (mpssdi->state_pth != pthread_self())) {
			pthread_cancel(mpssdi->state_pth);
			pthread_join(mpssdi->state_pth, &joinval);
		}

		if (mpssdi->crash_pth && (mpssdi->crash_pth != pthread_self())) {
			pthread_cancel(mpssdi->crash_pth);
			pthread_join(mpssdi->crash_pth, &joinval);
		}

		if (mpssdi->stop_pth && (mpssdi->stop_pth != pthread_self())) {
			pthread_cancel(mpssdi->stop_pth);
			pthread_join(mpssdi->stop_pth, &joinval);
		}
	}

	mpsslog(PNORM, "<<<<<<<< mpssd: segmentation violation - dumping stack >>>>>>>>\n");
	for (i = 0; i < cnt; i++) {
		mpsslog(PNORM, "%s\n", funcs[i]);
	}
	mpsslog(PNORM, "<<<<<<<<<<<<<<<<<<<<<<<<<<<<>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
	exit(0);
}

void
save_oops(struct mic_info *mic)
{
	struct mpssd_info *mpssdi = (struct mpssd_info *)mic->data;
	char buffer[4096];
	char oopsname[PATH_MAX];
	int headerdone = 0;
	int rlen;
	int fd;

	if (strcmp(mpssdi->state, "resetting"))
		return;

	snprintf(oopsname, sizeof(oopsname), "/proc/mic_ramoops/%s_prev",
								mic->name);
	if ((fd = open(oopsname, O_RDONLY)) < 0) {
		mpsslog(PINFO, "%s: open oopsname %s failed %s\n",
			mic->name, oopsname, strerror(errno));
		return;
	}

	while ((rlen = read(fd, buffer, 4096))) {
		if (!headerdone) {
			mpsslog(PINFO, "%s: Resetting MIC found oops:\n", mic->name);
			headerdone = 1;
		}
		mpsslog_dump(buffer, rlen);
	}
	close(fd);
}

void
setsighandlers(void)
{
	struct sigaction act;

	memset(&act, 0, sizeof(act));
	act.sa_sigaction = quit_handler;
	act.sa_flags = SA_SIGINFO;
	sigaction(SIGQUIT, &act, NULL);
	sigaction(SIGINT, &act, NULL);

	memset(&act, 0, sizeof(act));
	act.sa_sigaction = parent_handler;
	act.sa_flags = SA_SIGINFO;
	sigaction(SIGHUP, &act, NULL);

	memset(&act, 0, sizeof(act));
	act.sa_sigaction = segv_handler;
	act.sa_flags = SA_SIGINFO;
	sigaction(SIGSEGV, &act, NULL);
}

int
check_fs_params(struct mic_info *mic)
{
	struct stat sbuf;

	if ((mic->config.filesrc.base.type == SRCTYPE_CPIO) &&
	    stat(mic->config.filesrc.base.image, &sbuf) != 0) {
		mpsslog(PINFO, "%s: Boot aborted - Base image '%s' not found\n",
			       mic->name, mic->config.filesrc.base.image);
		return 1;
	}

	if ((mic->config.filesrc.base.type == SRCTYPE_DIR) &&
	    stat(mic->config.filesrc.base.dir, &sbuf) != 0) {
		mpsslog(PINFO, "%s: Boot aborted - Base directory '%s' not found\n",
			       mic->name, mic->config.filesrc.base.image);
		return 1;
	}

	if (mic->config.filesrc.common.dir == NULL) {
		mpsslog(PINFO, "%s: Boot aborted - CommonDir not configured\n", mic->name);
		return 1;
	}

	if (stat(mic->config.filesrc.common.dir, &sbuf) != 0) {
		mpsslog(PINFO, "%s: Boot aborted - CommonDir '%s' not found\n",
			       mic->name, mic->config.filesrc.common.dir);
		return 1;
	}

	if (mic->config.filesrc.mic.dir == NULL) {
		mpsslog(PINFO, "%s: Boot aborted - MicDir not configured\n", mic->name);
		return 1;
	}

	if (stat(mic->config.filesrc.mic.dir, &sbuf) != 0) {
		mpsslog(PINFO, "%s: Boot aborted - MicDir '%s' not found\n",
			       mic->name, mic->config.filesrc.mic.dir);
		return 1;
	}

	return 0;
}

void
start_daemon(void)
{
	struct mic_info *mic;
	struct mpssd_info *mpssdi;
	int err;
	int sc;

	if ((err = mpssenv_aquire_lockfile(&mpssenv))) {
		fprintf(stderr, "Error aquiring lockfile %s: %s\n", mpssenv.lockfile, strerror(err));
		exit(1);
	}

	pthread_create(&mon_pth, NULL, mic_monitor, NULL);
	pthread_create(&cred_pth, NULL, mic_credentials, NULL);

	while (pthread_mutex_lock(&start_lock) != 0);

	for (mic = miclist; mic != NULL; mic = mic->next) {
		if ((mic->data = malloc(sizeof(struct mpssd_info))) == NULL) {
			fprintf(stderr, "Catastrophic memory allocation error: %s\n", strerror(err));
			exit(1);
		}

		memset(mic->data, 0, sizeof(struct mpssd_info));
		mpssdi = (struct mpssd_info *)mic->data;

		if ((mpssdi->state = mpss_readsysfs(mic->name, "state")) == NULL) {
			mpsslog(PERROR, "%s: Ignoring  - Critical failure reading state sysfs entry\n", mic->name);
			continue;
		}

		pthread_mutex_init(&mpssdi->pth_lock, NULL);
		pthread_mutex_init(&mpssdi->reset_lock, NULL);
		pthread_cond_init(&mpssdi->reset_cond, NULL);
		start_count++;
		while (pthread_mutex_lock(&mpssdi->pth_lock) != 0);
		pthread_create(&mpssdi->state_pth, NULL, mic_state, mic);
		pthread_create(&mpssdi->boot_pth, NULL, boot_mic, mic);
		while (pthread_mutex_unlock(&mpssdi->pth_lock) != 0);
	}

	while (pthread_mutex_unlock(&start_lock) != 0);
	sc = start_count;
	while (sc) {
		while (pthread_mutex_lock(&start_lock) != 0);
		sc = start_count;
		while (pthread_mutex_unlock(&start_lock) != 0);
		sleep(1);
	}

	if (start_pid)
		kill(start_pid, SIGHUP);

	while (pause());
}

void *
boot_mic(void *arg)
{
	struct mic_info *mic = (struct mic_info *)arg;
	struct mpssd_info *mpssdi = (struct mpssd_info *)mic->data;
	struct mpss_elist mpssperr;
	struct stat sbuf;
	char *initrd = NULL;
	char *state;
	char *save;
	char *errmsg;
	char boot_string[PATH_MAX];
	char cmdline[2048];
	int shutdown_wait = 180;// Do not wait for shutdown more than 180 secs.
	int reset_wait = 180;	// Do not wait for reset more than 180 secs.
	int err = 0;
	char *shutdown_str;

	if ((err = mpss_parse_config(&mpssenv, mic, &brlist, &mpssperr))) {
		mpsslog(PINFO, "%s: Boot aborted - no configuation file present: %s\n", mic->name, strerror(err));
		goto bootexit;
	}

	mpss_print_elist(&mpssperr, PWARN, mpsslog);
	mpss_clear_elist(&mpssperr);

	if (check_fs_params(mic))
		goto bootexit;

	if (mic->config.boot.osimage == NULL) {
		mpsslog(PINFO, "%s: Boot aborted - OsImage parameter not set\n", mic->name);
		goto bootexit;
	}

	if (verify_bzImage(&mpssenv, mic->config.boot.osimage, mic->name)) {
		mpsslog(PINFO, "%s: Boot aborted - %s is not a valid Linux bzImage\n",
				mic->name, mic->config.boot.osimage);
		goto bootexit;
	}

	if ((errmsg = mpss_set_cmdline(mic, brlist, cmdline, NULL)) != NULL) {
		mpsslog(PINFO, "%s: Boot aborted - %s\n", mic->name, errmsg);
		goto bootexit;
	}

	mpsslog(PINFO, "%s: Command line: %s\n", mic->name, cmdline);

	set_log_buf_info(mic);

	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

	if (mic->config.boot.onstart != TRUE) {
		mpsslog(PINFO, "%s: Not set to autoboot\n", mic->name);
		goto bootmic_done;
	}

	switch (mic->config.rootdev.type) {
	case ROOT_TYPE_RAMFS:
		if (stat(mic->config.rootdev.target, &sbuf) == 0)
			unlink(mic->config.rootdev.target);
		mpsslog(PINFO, "%s: Generate %s\n", mic->name, mic->config.rootdev.target);
		mpssfs_gen_initrd(&mpssenv, mic, &mpssperr);
		mpss_print_elist(&mpssperr, PWARN, mpsslog);
		mpss_clear_elist(&mpssperr);
	case ROOT_TYPE_STATICRAMFS:
		initrd = mic->config.rootdev.target;
		break;

	case ROOT_TYPE_NFS:
	case ROOT_TYPE_SPLITNFS:
	case ROOT_TYPE_PFS:
		initrd = mic->config.filesrc.base.image;
		break;
	}

	if (initrd == NULL) {
		mpsslog(PERROR, "%s Boot aborted - initial ramdisk not set", mic->name);
		goto bootmic_done;
	}

	snprintf(boot_string, PATH_MAX, "boot:linux:%s:%s", mic->config.boot.osimage, initrd);

	if ((state = mpss_readsysfs(mic->name, "state")) == NULL) {
		mpsslog(PERROR, "%s: Cannot access state sysfs entry - skipping\n", mic->name);
		goto bootmic_done;
	}

	shutdown_str = "shutdown";

	while (!strcmp(state, shutdown_str)) {
		save = state;

		if ((state = mpss_readsysfs(mic->name, "state")) == NULL) {
			mpsslog(PWARN, "%s: Wait for shutdown failed to read state sysfs - try again\n", mic->name);
			state = save;
		} else {
			free(save);
		}

		if (!shutdown_wait--) {
			mpsslog(PWARN, "%s: Wait for shutdown timed out\n", mic->name);
			goto bootmic_done;
		}
		mpsslog(PINFO, "%s: Waiting for shutdown to complete\n", mic->name);
		sleep(1);
	}

	while (!strcmp(state, "resetting")) {
		save = state;
		if ((state = mpss_readsysfs(mic->name, "state")) == NULL) {
			mpsslog(PWARN, "%s: Wait for reset failed to read state sysfs - try again\n", mic->name);
			state = save;
		} else {
			free(save);
		}

		if (!reset_wait--) {
			mpsslog(PINFO, "%s: Wait for reset timed out\n", mic->name);
			goto bootmic_done;
		}
		mpsslog(PINFO, "%s: Waiting for reset to complete\n", mic->name);
		sleep(1);
	}

	if (strcmp(state, "ready")) {
		mpsslog(PINFO, "%s: Current state \"%s\" cannot boot card\n", mic->name, state);
		free(state);
		goto bootmic_done;
	}

	free(state);

	if ((err = mpss_setsysfs(mic->name, "state", boot_string)) != 0) {
		mpsslog(PINFO, "%s: Booting failed - cannot set state: %s\n", mic->name, strerror(err));
	} else {
		mpsslog(PINFO, "%s: Booting %s initrd %s\n", mic->name, mic->config.boot.osimage, initrd);
	}

bootmic_done:
	while (pthread_mutex_lock(&mpssdi->pth_lock) != 0);
	mpssdi->boot_pth = 0;
	while (pthread_mutex_unlock(&mpssdi->pth_lock) != 0);
bootexit:
	while (pthread_mutex_lock(&start_lock) != 0);
	start_count--;
	while (pthread_mutex_unlock(&start_lock) != 0);
	pthread_exit(NULL);
}

ssize_t
get_dir_size(struct mic_info *mic, char *dirpath)
{
	struct dirent *tmp;
	DIR *dir;
	char path[PATH_MAX];
	struct stat data;
	size_t result = 0;

	if ((dir = opendir(dirpath)) == NULL) {
		mpsslog(PINFO, "%s: Could not open dir %s\n", mic->name, dirpath);
		return -1;
	}

	while ((tmp = readdir(dir))) {
		if (!strcmp(tmp->d_name, ".") || !strcmp(tmp->d_name, ".."))
			continue;

		snprintf(path, PATH_MAX - 1, "%s/%s", dirpath, tmp->d_name);

		if (lstat(path, &data) < 0) {
			mpsslog(PINFO, "%s: Couldn't lstat %s: %s\n", mic->name, path, strerror(errno));
			continue;
		}

		if (S_ISDIR(data.st_mode) && !S_ISLNK(data.st_mode)) {
			ssize_t dirsize;
			strcat(path, "/");
			if ((dirsize = get_dir_size(mic, path)) < 0) {
				mpsslog(PINFO, "%s: getting directory size failed %s %s\n",
					mic->name, path, strerror(errno));
				return dirsize;
			}
			result += dirsize;
		} else if (S_ISREG(data.st_mode)) {
			result += data.st_size;
		}
	}

	closedir(dir);

	return result;
}

pid_t
gzip(char *name)
{
	pid_t pid;
	char *ifargv[3];

	pid = fork();
	if (pid == 0) {
		ifargv[0] = "/bin/gzip";
		ifargv[1] = name;
		ifargv[2] = NULL;
		execve("/bin/gzip", ifargv, NULL);
	}

	return pid;
}

int
autoreboot(struct mic_info *mic)
{
	char value[4096];
	int fd, len, ret;
	char pathname[PATH_MAX] = "/sys/devices/virtual/mic/scif/watchdog_auto_reboot";

	if ((fd = open(pathname, O_RDONLY)) < 0) {
		mpsslog(PINFO, "%s: Failed to open %s %s\n", mic->name, pathname, strerror(errno));
		return 0;
	}

	if ((len = read(fd, value, sizeof(value) - 1)) < 0) {
		mpsslog(PINFO, "%s: Failed to read %s: %s\n", mic->name, pathname, strerror(errno));
		ret = 0;
		goto readsys_ret;
	}

	value[len] = '\0';

	ret = atoi(value);
	mpsslog(PINFO, "%s: autoreboot %d\n", mic->name, ret);
readsys_ret:
	close(fd);
	return ret;
}

#define CD_READ_CHUNK	(1024 * 1024 * 1024ULL)
#define CD_MIN_DISK	(32 * 1024 * 1024 * 1024ULL)
#define CD_DIR		mic->config.misc.crashdumpDir
#define CD_LIMIT	mic->config.misc.crashdumplimitgb

void *
save_crashdump(void *arg)
{
	struct mic_info *mic = (struct mic_info *)arg;
	struct mpssd_info *mpssdi = (struct mpssd_info *)mic->data;
	int cdfd = -1;
	int procfd = -1;
	void *addr = NULL;
	ssize_t bytes;
	ssize_t total_bytes = 0;
	ssize_t dirlimit;
	ssize_t diractual;
	struct tm *tm = NULL;
	char pathname[PATH_MAX];
	time_t t;
	pid_t pid1 = 0;
	char *state;
	char *save;
	struct stat sbuf;
	struct statvfs vbuf;
	int err;

	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

	if ((dirlimit = atoi(CD_LIMIT) * (1024 * 1024 * 1024ULL)) == 0) {
		mpsslog(PWARN, "%s: [SaveCrashDump] Dump disabled\n", mic->name);
		goto reboot;
	}

	if (stat(CD_DIR, &sbuf) < 0) {
		if (mkdir(CD_DIR, 0755) < 0) {
			mpsslog(PWARN, "%s: [SaveCrashDump] Avborted - create directory %s failed: %s\n",
					mic->name, CD_DIR, strerror(errno));
			goto reboot;
		}

		diractual = dirlimit;
	} else {	/* Check size of crash directory with configured limits */
		if ((diractual = get_dir_size(mic, CD_DIR)) < 0) {
			mpsslog(PINFO, "%s: [SaveCrashDump] Avborted - get directory %s size failed: %s\n",
					mic->name, CD_DIR, strerror(errno));
			goto reboot;
		}
	}

	if (diractual > dirlimit) {
		mpsslog(PINFO, "%s: [SaveCrashDump] Avborted - %s current size 0x%lx configured limit 0x%lx\n",
			mic->name, CD_DIR, diractual, dirlimit);
		goto reboot;
	}

	/* Open core dump file with time details embedded in file name */
	time(&t);
	if ((tm = localtime(&t)) == 0) {
		mpsslog(PERROR, "%s: [SaveCrashdump] Aborted - get system date failed\n", mic->name);
		goto reboot;
	}

	/* Create crash directories if not done already */
	snprintf(pathname, PATH_MAX - 1, "%s/%s", CD_DIR, mic->name);
	if (mkdir(pathname, 0755) && errno != EEXIST) {
		mpsslog(PERROR, "%s: [SaveCrashDump] Aborted - create directory %s failed %s\n",
					mic->name, pathname, strerror(errno));
		goto reboot;
	}

	if (statvfs(pathname, &vbuf) < 0) {
		mpsslog(PERROR, "%s: [SaveCrashDump] Aborted - cannot read free disk size of %s: %s\n",
					mic->name, pathname, strerror(errno));
		goto reboot;
	}

	if (CD_MIN_DISK > (vbuf.f_bsize * vbuf.f_bfree)) {
		mpsslog(PERROR, "%s: [SaveCrashDump] Aborted - free disk space less than required 32Gb\n", mic->name);
		goto reboot;
	}

	/* Open vmcore entry for crashed card */
	snprintf(pathname, PATH_MAX - 1, "/proc/mic_vmcore/%s", mic->name);
	if ((procfd = open(pathname, O_RDONLY)) < 0) {
		mpsslog(PERROR, "%s: [SaveCrashdump] Aborted - open %s failed: %s\n",
					mic->name, pathname, strerror(errno));
		goto reboot;
	}

	snprintf(pathname, PATH_MAX - 1, "%s/%s/vmcore-%d-%d-%d-%d:%d:%d", CD_DIR, mic->name,
			tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
			tm->tm_hour, tm->tm_min, tm->tm_sec);

	if ((cdfd = open(pathname, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR)) < 0) {
		mpsslog(PERROR, "%s: [SaveCrashDump] Aborted - open %s failed %s\n",
					mic->name, pathname, strerror(errno));
		goto cleanup1;
	}

	mpsslog(PINFO, "%s: [SaveCrashDump] Capturing uOS kernel crash dump\n", mic->name);

	/* Read from the proc entry and write to the core dump file */
	do {
		if (lseek(cdfd, CD_READ_CHUNK, SEEK_CUR) < 0) {
			mpsslog(PERROR, "%s: [SaveCrashDump] Aborted lseek failed %s\n", mic->name, strerror(errno));
			remove(pathname);
			goto cleanup2;
		}

		bytes = write(cdfd, "", 1);
		if (bytes != 1) {
			mpsslog(PERROR, "%s: [SaveCrashDump] Aborted write failed %s\n", mic->name, strerror(errno));
			remove(pathname);
			goto cleanup2;
		}

		if ((addr = mmap(NULL, CD_READ_CHUNK, PROT_READ|PROT_WRITE,
					MAP_SHARED, cdfd, total_bytes)) == MAP_FAILED) {
			mpsslog(PERROR, "%s: [SaveCrasdDump] Aborted mmap failed %s\n", mic->name, strerror(errno));
			remove(pathname);
			goto cleanup2;
		}

		if ((bytes = read(procfd, addr, CD_READ_CHUNK)) < 0) {
			mpsslog(PERROR, "%s: [SaveCrashDump] Aborted read failed %s\n", mic->name, strerror(errno));
			remove(pathname);
			munmap(addr, CD_READ_CHUNK);
			goto cleanup2;
		}

		total_bytes += bytes;
		munmap(addr, CD_READ_CHUNK);
		if (ftruncate(cdfd, total_bytes + 1) < 0) {
			mpsslog(PERROR, "%s: [SaveCrashDump] Aborted ftruncate failed %s\n", mic->name, strerror(errno));
			remove(pathname);
			goto cleanup2;
		}
	} while (bytes == CD_READ_CHUNK);

	mpsslog(PNORM, "%s: [SaveCrashDump] Completed raw dump size 0x%lx\n", mic->name, total_bytes);
	mpsslog(PNORM, "%s: [SaveCrashDump] Gzip started\n", mic->name);
	pid1 = gzip(pathname); /* Initiate compression of the file and reset MIC in parallel */


cleanup2:
	close(cdfd);

cleanup1:
	close(procfd);

reboot:
	if ((err = mpss_setsysfs(mic->name, "state", "reset:force")) != 0) {
		mpsslog(PINFO, "%s: [SaveCrashDump] Failed to set state sysfs - cannot reset: %s\n",
				mic->name, strerror(err));
		goto done;
	}

	if ((state = mpss_readsysfs(mic->name, "state")) == NULL) {
		mpsslog(PINFO, "%s: [SaveCrashDump] Failed to read state sysfs - state of reset unknown\n", mic->name);
		goto done;
	}

	while (strcmp(state, "ready") && strcmp(state, "reset failed")) {
		if (!strcmp(state, "online") || !strcmp(state, "booting")) {
			mpsslog(PINFO, "%s: [SaveCrashDump] External entity has already rebooted card\n", mic->name);
			free(state);
			goto done;
		}

		mpsslog(PINFO, "%s: [SaveCrashDump] Waiting for reset\n", mic->name);
		sleep(2);
		save = state;

		if ((state = mpss_readsysfs(mic->name, "state")) == NULL) {
			mpsslog(PWARN, "%s: [SaveCrashDump] wait for ready failed to read state sysfs - try again\n",
					mic->name);
			state = save;
		} else {
			free(save);
		}
	}

	if (strcmp(state, "ready")) {
		mpsslog(PERROR, "%s: [SaveCrashDump] Failed to reset card.  Aborting reboot\n", mic->name);
		free(state);
		goto done;
	}

	if (pid1 && (pid1 < 0 || ((waitpid(pid1, NULL, 0)) < 0)))
		remove(pathname);

	if (autoreboot(mic)) {
		while (pthread_mutex_lock(&start_lock) != 0);
		start_count++;
		while (pthread_mutex_lock(&mpssdi->pth_lock) != 0);
		pthread_create(&mpssdi->boot_pth, NULL, boot_mic, mic);
		while (pthread_mutex_unlock(&mpssdi->pth_lock) != 0);
		while (pthread_mutex_unlock(&start_lock) != 0);
	}
done:
	pthread_exit(NULL);
}

void *
mic_state(void *arg)
{
	struct mic_info *mic = (struct mic_info *)arg;
	struct mpssd_info *mpssdi = (struct mpssd_info *)mic->data;
	char *state = NULL;
	int fd, ret;
	struct pollfd ufds[1];
	char value[4096];

	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

	if ((fd = mpss_opensysfs(mic->name, "state")) < 0) {
		mpsslog(PINFO, "%s: opening sysfs 'state' entry failed %s\n",
			mic->name, strerror(errno));
		goto error;
	}

	while (1) {
		ret = lseek(fd, 0, SEEK_SET);
		if (ret < 0) {
			mpsslog(PINFO, "%s: Failed to seek to file start: %s\n",
				mic->name, strerror(errno));
			goto close_error;
		}
		if ((ret = read(fd, value, sizeof(value))) < 0) {
			mpsslog(PINFO, "%s: Failed to read sysfs entry 'state': %s\n",
				mic->name, strerror(errno));
			goto close_error;
		}

		if (mpssdi->state != NULL) {
			if ((state = mpss_readsysfs(mic->name, "state")) == NULL) {
				mpsslog(PWARN, "%s: State change indicated but failed to read state sysfs\n", mic->name);
			} else if (strcmp(mpssdi->state, state)) {
				mpsslog(PINFO, "%s: State %s -> %s\n", mic->name, mpssdi->state, state);
				free(mpssdi->state);
				mpssdi->state = state;
				if (!strcmp(mpssdi->state, "lost")) {
					while (pthread_mutex_lock(&mpssdi->pth_lock) != 0);
					pthread_create(&mpssdi->crash_pth, NULL, save_crashdump, mic);
					while (pthread_mutex_unlock(&mpssdi->pth_lock) != 0);
				}
				save_oops(mic);
			} else {
				free(state);
			}
		}

repoll:
		ufds[0].fd = fd;
		ufds[0].events = POLLERR | POLLPRI;
		if ((ret = poll(ufds, 1, -1)) < 0) {
			if ((errno == EINTR) || (errno == ERESTART))
				goto repoll;

			mpsslog(PINFO, "%s: poll failed %d: %s\n", mic->name, errno, strerror(errno));
			goto close_error;
		}
	}

close_error:
	close(fd);
error:
	while (pthread_mutex_lock(&mpssdi->pth_lock) != 0);
	mpssdi->state_pth = 0;
	while (pthread_mutex_unlock(&mpssdi->pth_lock) != 0);
	pthread_exit(NULL);
}

void
mpsslog(unsigned char level, char *format, ...)
{
	va_list args;
	sigset_t blockmask;
	sigset_t freemask;
	char buffer[4096];
	time_t t;
	char *ts;

	if (logfp == NULL)
		return;

	sigemptyset(&freemask);
	sigemptyset(&blockmask);
	sigaddset(&blockmask, SIGHUP);
	sigaddset(&blockmask, SIGQUIT);
	sigaddset(&blockmask, SIGINT);
	sigaddset(&blockmask, SIGIO);
	pthread_sigmask(SIG_BLOCK, &blockmask, &freemask);

	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);

	time(&t);
	ts = ctime(&t);
	ts[strlen(ts) - 1] = '\0';
	fprintf(logfp, "%s: %s", ts, buffer);
	fflush(logfp);

	pthread_sigmask(SIG_SETMASK, &freemask, NULL);
}

void
mpsslog_dump(char *buf, int len)
{
	fwrite(buf, len, 1, logfp);
}

void
set_log_buf_info(struct mic_info *mic)
{
	int fd;
	int err;
	off_t len;
	char *map;
	char *temp;
	char log_addr[17] = {'\0'};
	char log_size[17] = {'\0'};

	if (mic->config.boot.systemmap == NULL) {
		mpsslog(PINFO, "%s: System map not correctly configured in OSimage parameter\n", mic->name);
		return;
	}

	if ((fd = open(mic->config.boot.systemmap, O_RDONLY)) < 0) {
		mpsslog(PINFO, "%s: Opening System.map failed: %s\n", mic->name, strerror(errno));
		return;
	}

	if ((len = lseek(fd, 0, SEEK_END)) < 0) {
		mpsslog(PINFO, "%s: Reading System.map size failed: %s\n", mic->name, strerror(errno));
		goto close_return;
	}

	if ((map = (char *) mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED) {
		mpsslog(PINFO, "%s: mmap of System.map failed: %s\n", mic->name, strerror(errno));
		goto close_return;
	}

	if (!(temp = strstr(map, "__log_buf"))) {
		mpsslog(PINFO, "%s: __log_buf not found: %s\n", mic->name, strerror(errno));
		goto unmap_return;
	}

	strncpy(log_addr, temp - 19, 16);
	if ((err = mpss_setsysfs(mic->name, "log_buf_addr", log_addr)) != 0) {
		mpsslog(PINFO, "%s: failed set log_buf_addr sysfs: %s\n", mic->name, strerror(err));
		goto unmap_return;
	}

	if (!(temp = strstr(map, "log_buf_len"))) {
		mpsslog(PINFO, "%s: log_buf_len not found: %s\n", mic->name, strerror(errno));
		goto unmap_return;
	}
	strncpy(log_size, temp - 19, 16);

	if ((err = mpss_setsysfs(mic->name, "log_buf_len", log_size)) != 0) {
		mpsslog(PINFO, "%s: failed set log_buf_len sysfs: %s\n", mic->name, strerror(err));
		goto unmap_return;
	}

	mpsslog(PINFO, "%s: Debug log buffer addr %s len @ %s\n", mic->name, log_addr, log_size);

unmap_return:
	munmap(map, len);
close_return:
	close(fd);
}

struct jobs {
	unsigned int	jobid;
	int		cnt;
	scif_epd_t	dep;
	struct jobs	*next;
} gjobs;

pthread_mutex_t	jobs_lock = PTHREAD_MUTEX_INITIALIZER;
unsigned int nextjobid = 100;

void *
monitor(void *arg)
{
	struct mic_info *mic = (struct mic_info *)arg;
	struct mpssd_info *mpssdi = (struct mpssd_info *)mic->data;
	unsigned int proto;
	unsigned int jobid;
	struct pollfd pfds[1];
	struct jobs *jlist;
	struct jobs *job = NULL;
	uint16_t stopID;

	while (1) {
		pfds[0].fd = mpssdi->recv_ep;
		pfds[0].events = POLLIN | POLLERR | POLLPRI;
		poll(pfds, 1, -1);

		if (scif_recv(mpssdi->recv_ep, &proto, sizeof(proto), SCIF_RECV_BLOCK) < 0) {
			if (errno == ECONNRESET) {
				mpsslog(PERROR, "%s: MIC card mpssd daemon disconnect: %s\n", mic->name,strerror(errno));
				scif_close(mpssdi->recv_ep);
				scif_close(mpssdi->send_ep);
				mpssdi->recv_ep = -1;
				mpssdi->send_ep = -1;
				pthread_exit((void *)1);
			}
			continue;
		}

		switch (proto) {
		case REQ_CREDENTIAL_ACK:
		case REQ_CREDENTIAL_NACK:
			scif_recv(mpssdi->recv_ep, &jobid, sizeof(jobid), SCIF_RECV_BLOCK);

			while (pthread_mutex_lock(&jobs_lock) != 0);
			jlist = &gjobs;
			while (jlist->next) {
				if (jlist->next->jobid == jobid) {
					job = jlist->next;

					if (--job->cnt == 0) {
						jlist->next = job->next;
						while (pthread_mutex_unlock(&jobs_lock) != 0);

						proto = CRED_SUCCESS;
						scif_send(job->dep, &proto, sizeof(proto), 0);
						scif_close(job->dep);
						continue;
					}
					break;
				}

				jlist = jlist->next;
			}

			while (pthread_mutex_unlock(&jobs_lock) != 0);
			break;

		case MONITOR_STOPPING:
			scif_recv(mpssdi->recv_ep, &stopID, sizeof(stopID), SCIF_RECV_BLOCK);
			mpsslog(PERROR, "%s: card mpssd daemon exiting\n", mic->name);
			scif_close(mpssdi->recv_ep);
			scif_close(mpssdi->send_ep);
			mpssdi->recv_ep = -1;
			mpssdi->send_ep = -1;
			pthread_exit((void *)0);
		}
	}
}

void *
mic_monitor(void *arg)
{
	struct mic_info *mic;
	struct mpssd_info *mpssdi;
	pthread_attr_t attr;
	struct scif_portID sendID = {0, MPSSD_MONSEND};
	struct scif_portID recvID;
	scif_epd_t lep;
	scif_epd_t recv_ep;
	scif_epd_t send_ep;
	unsigned int proto;
	uint16_t send_port;
	uint16_t remote_port = 0;
	int err;

	if ((lep = scif_open()) < 0) {
		mpsslog(PINFO, "Cannot open mpssd monitor SCIF listen port: %s\n", strerror(errno));
		pthread_exit((void *)1);
	}

	if (scif_bind(lep, MPSSD_MONRECV) < 0) {
		mpsslog(PINFO, "Cannot bind to mpssd monitor SCIF PORT: %s\n", strerror(errno));
		pthread_exit((void *)1);
	}

	if (scif_listen(lep, 16) < 0) {
		mpsslog(PINFO, "Set Listen on mpssd monitor SCIF PORT fail: %s\n", strerror(errno));
		pthread_exit((void *)1);
	}

	while (1) {
		if (scif_accept(lep, &recvID, &recv_ep, SCIF_ACCEPT_SYNC)) {
			if (errno != EINTR)
				mpsslog(PINFO, "Wait for card connect failed: %s\n", strerror(errno));
			sleep(1);
			continue;
		}

		if ((mic = mpss_find_micid_inlist(miclist, recvID.node - 1)) == NULL) {
			mpsslog(PINFO, "Cannot configure - node %d does not seem to exist\n",
				       recvID.node - 1);
			scif_close(recv_ep);
			continue;
		}

		mpssdi = (struct mpssd_info *)mic->data;

		if ((send_ep = scif_open()) < 0) {
			fprintf(logfp, "Failed to open SCIF: %s\n", strerror(errno));
			scif_close(recv_ep);
			pthread_exit((void *)1);
		}
		mpssdi->send_ep = send_ep;

		if ((err = scif_recv(recv_ep, &proto, sizeof(proto), SCIF_RECV_BLOCK)) != sizeof(proto)) {
			mpsslog(PINFO, "%s: MIC card mpssd daemon startup connection error %s\n",
					mic->name, strerror(errno));
			scif_close(recv_ep);
			mpssdi->recv_ep = -1;
			continue;
		}

		switch (proto) {
		case MONITOR_START:
			sendID.node = mic->id + 1;
			while ((send_port = scif_connect(send_ep, &sendID)) < 0) {
				fprintf(logfp, "Failed to connect to monitor thread on card: %s\n",
					strerror(errno));
				sleep(1);
			}

			// Over reliable connection, mpssd tells us which port number it uses
			// to talk back to us. If this port matches actual recv_ep remote port
			// then we know that recv_ep and send_ep reference the same client.
			// We also know that send_ep, references mpssd on mic, as port we
			// connect to on that endpoint requires privliges to listen on.
			if (scif_recv(send_ep, &remote_port, sizeof(remote_port), SCIF_RECV_BLOCK) < 0) {
				mpsslog(PINFO, "%s: MIC card mpssd daemon handshake error %s\n",
					mic->name, strerror(errno));
				scif_close(send_ep);
				scif_close(recv_ep);
				continue; // go back to next iteration of while(1), we cannot break the while loop because hosts mpssd can connect with multiple mic cards
			}

			if (remote_port != recvID.port || sendID.node != recvID.node) {
				mpsslog(PINFO, "%s: Failed to authenticate connection with mic mpssd\n",
					mic->name);
				scif_close(send_ep);
				scif_close(recv_ep);
				continue; // go back to next iteration of while(1), we cannot break the while loop because hosts mpssd can connect with multiple mic cards

			}

			// Similarily, provide info for the client, so that he can also verify
			// that both connections send_ep & recv_ep belong to us.
			if (scif_send(recv_ep, &send_port, sizeof(send_port), SCIF_SEND_BLOCK) < 0) {
				mpsslog(PINFO, "%s: MIC card mpssd daemon handshake error %s\n",
					mic->name, strerror(errno));
				scif_close(send_ep);
				scif_close(recv_ep);
				continue; // go back to next iteration of while(1), we cannot break the while loop because hosts mpssd can connect with multiple mic cards

			}

			mpssdi->recv_ep = recv_ep;
			pthread_attr_init(&attr);
			pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
			pthread_create(&mpssdi->monitor_pth, &attr, monitor, mic);
			proto = MONITOR_START_ACK;
			scif_send(send_ep, &proto, sizeof(proto), SCIF_RECV_BLOCK);
			mpsslog(PINFO, "%s: Monitor connection established\n", mic->name);
			break;
		}
	}
}

#define MPSS_COOKIE_SIZE 8

int
get_cookie(struct passwd *pass, char *cookie)
{
	char cookiename[PATH_MAX];
	struct stat sbuf;
	int createcookie = TRUE;
	int len;
	int fd;
	int err = -1;

	snprintf(cookiename, PATH_MAX, "%s/.mpsscookie", pass->pw_dir);

	if (setegid(pass->pw_gid) < 0) {
		mpsslog(PERROR, "%s Cannot create: Failed to setegid to gid %d : %s\n",
				cookiename, pass->pw_gid, strerror(errno));
		return -1;
	}

	if (seteuid(pass->pw_uid) < 0) {
		mpsslog(PERROR, "%s Cannot create: Failed to seteuid to uid %d : %s\n",
				cookiename, pass->pw_uid, strerror(errno));
		setegid(0);
		return -1;
	}

	if (lstat(cookiename, &sbuf) == 0) {
		if (S_ISLNK(sbuf.st_mode)) {
			if (unlink(cookiename) < 0) {
				mpsslog(PERROR, "%s Cannot create: is a link and removal failed: %s\n",
					cookiename, strerror(errno));
				goto cookie_done;
			}

			mpsslog(PERROR, "%s: Is a link - remove and recreate\n", cookiename);

		} else if (sbuf.st_nlink != 1) {
			if (unlink(cookiename) < 0) {
				mpsslog(PERROR, "%s Cannot create: has more than one hard link and "
						"removal failed: %s\n", cookiename, strerror(errno));
				goto cookie_done;
			}

			mpsslog(PERROR, "%s: Too many hard links - remove and recreate\n", cookiename);

		} else {
			createcookie = FALSE;
		}
	}

	if (!createcookie) {
		if ((fd = open(cookiename, O_RDONLY)) < 0) {
			mpsslog(PERROR, "Failed to open %s: %s\n", cookiename, strerror(errno));
			goto cookie_done;
		}

		if ((len = read(fd, cookie, MPSS_COOKIE_SIZE)) != MPSS_COOKIE_SIZE) {
			if (unlink(cookiename) < 0) {
				mpsslog(PERROR, "Cannot create cookie file %s bad size and removal failed: %s\n",
					cookiename, strerror(errno));
				goto cookie_done;
			}

			mpsslog(PERROR, "%s: Bad size remove and recreate\n", cookiename);
			createcookie = TRUE;
		}
		close(fd);
	}

	if (createcookie) {
		if ((fd = open("/dev/urandom", O_RDONLY)) < 0) {
			mpsslog(PERROR, "Create cookie %s failed to open dev random: %s\n", cookiename, strerror(errno));
			goto cookie_done;
		}

		len = read(fd, cookie, MPSS_COOKIE_SIZE);
		close(fd);

		if ((fd = open(cookiename, O_WRONLY|O_CREAT)) < 0) {
			mpsslog(PERROR, "Failed to open %s: %s\n", cookiename, strerror(errno));
			goto cookie_done;
		}

		write(fd, cookie, len);
		fchmod(fd, S_IRUSR);
		fchown(fd, pass->pw_uid, pass->pw_gid);
		close(fd);
	}

	err = 0;

cookie_done:
	seteuid(0);
	setegid(0);
	return err;
}

void *
mic_credentials(void *arg)
{
	struct mic_info *mic;
	struct mpssd_info *mpssdi;
	struct jobs *job;
	struct jobs *jlist;
	struct scif_portID portID;
	struct passwd *pass;
	char *username = NULL;
	char cookie[MPSS_COOKIE_SIZE];
	int len;
	unsigned int proto;
	scif_epd_t lep;
	scif_epd_t dep;
	uid_t uid;
	int err;

	if ((lep = scif_open()) < 0) {
		mpsslog(PINFO, "Cannot open mpssd credentials SCIF listen port: %s\n",
			       strerror(errno));
		pthread_exit((void *)1);
	}

	if (scif_bind(lep, MPSSD_CRED) < 0) {
		mpsslog(PINFO, "Cannot bind to mpssd credentials SCIF PORT: %s\n", strerror(errno));
		pthread_exit((void *)1);
	}

	if (scif_listen(lep, 16) < 0) {
		mpsslog(PINFO, "Set Listen on mpssd credentials SCIF PORT fail: %s\n", strerror(errno));
		pthread_exit((void *)1);
	}

	while (1) {
		if (scif_accept(lep, &portID, &dep, SCIF_ACCEPT_SYNC)) {
			if (errno != EINTR) {
				mpsslog(PINFO, "Wait for credentials request fail: %s\n", strerror(errno));
				scif_close(dep);
			}
			continue;
		}

		if ((err = scif_recv(dep, &uid, sizeof(uid), SCIF_RECV_BLOCK)) != sizeof(uid)) {
			mpsslog(PINFO, "Credential connect recieve error %s\n", strerror(errno));
			scif_close(dep);
			continue;
		}

		username = NULL;
		while ((pass = getpwent()) != NULL) {
			if (uid == pass->pw_uid) {
				username = pass->pw_name;
				break;
			}
		}

		endpwent();

		if (username == NULL) {
			mpsslog(PERROR, "User request unknown UID %d\n", uid);
			proto = CRED_FAIL_UNKNOWNUID;
			scif_send(dep, &proto, sizeof(proto), 0);
			scif_close(dep);
			continue;
		};

		if (get_cookie(pass, cookie) < 0) {
			proto = CRED_FAIL_READCOOKIE;
			scif_send(dep, &proto, sizeof(proto), 0);
			scif_close(dep);
			continue;
		}

		if ((job = (struct jobs *) malloc(sizeof(struct jobs))) == NULL) {
			proto = CRED_FAIL_MALLOC;
			scif_send(dep, &proto, sizeof(proto), 0);
			scif_close(dep);
			continue;
		}

		job->jobid = nextjobid++;
		job->dep = dep;
		job->cnt = 0;
		len = strlen(username);

		while (pthread_mutex_lock(&jobs_lock) != 0);

		for (mic = miclist; mic != NULL; mic = mic->next) {
			mpssdi = (struct mpssd_info *)mic->data;

			if (mpssdi->send_ep != -1) {
				job->cnt++;
				proto = REQ_CREDENTIAL;
				if ((scif_send(mpssdi->send_ep, &proto, sizeof(proto), 0)) < 0) {
					if (errno == ECONNRESET) {
						job->cnt--;
						continue;
					}
				}

				scif_send(mpssdi->send_ep, &job->jobid, sizeof(job->jobid), 0);
				scif_send(mpssdi->send_ep, &len, sizeof(len), 0);
				scif_send(mpssdi->send_ep, username, len, 0);
				len = sizeof(cookie);
				scif_send(mpssdi->send_ep, &len, sizeof(len), 0);
				scif_send(mpssdi->send_ep, cookie, len, SCIF_SEND_BLOCK);
			}
		}

		if (job->cnt == 0) {
			proto = CRED_SUCCESS;
			scif_send(job->dep, &proto, sizeof(proto), 0);
			scif_close(job->dep);
		} else {
			jlist = &gjobs;
			while (jlist->next)
				jlist = jlist->next;

			jlist->next = job;
			job->next = NULL;
		}
		while (pthread_mutex_unlock(&jobs_lock) != 0);
	}
}
