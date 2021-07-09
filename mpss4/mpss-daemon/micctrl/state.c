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

#include "micctrl.h"
#include "help.h"

#include <libmpsscommon.h>
#include <mic.h>
#include <mpssconfig.h>

#include <fcntl.h>
#include <list>
#include <stdlib.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

static int boot(struct mic_info *miclist, int timeout, bool wait_for_state);
static int reset(struct mic_info *miclist, int timeout, bool force, bool wait_for_state);
static int shutdown(struct mic_info *miclist, int timeout, bool force, bool wait_for_state);
static int reboot(struct mic_info *miclist, int timeout, bool wait_for_state);
static int status(struct mic_info *miclist, bool verbose_mode);
static int wait(std::list<mic_info*>& mic_list, int timeout, int target_state);


/*
 * Section 1: Top level parse routines for state manipulation
 */

/*
 * This function parses command line for 4 different commands:
 *
 * micctrl --boot [-w] [-t <timout>] [MIC list]
 *
 * Boot the specified MIC coprocessors and optionally wait for completion
 *
 *     micctrl --reset [-w] [-f] [-t <timout>] [MIC list]
 *
 * Reset the specified MIC coprocessors and optionally wait for completion
 *
 *     micctrl --shutdown [-w] [-f] [-t <timout>] [MIC list]
 *
 * Shutdown the specified MIC coprocessors and optionally wait for completion
 *
 *     micctrl --reboot [-w] [-t <timout>] [MIC list]
 *
 * Reboot the specified MIC coprocessors and optionally wait for completion
 */
const char *state_help[] = {
/* Boot Help */
"\
micctrl -b [sub options] [list of coprocessors]\n\
micctrl --boot [sub options] [list of coprocessors]\n\
\n\
The --boot command boots all or selected processors. Coprocessors must be in the\n\
'ready to boot' or 'shutdown' state to boot successfully. Once booted,\n\
coprocessors' state is changed to 'online'.\n\
\n\
        PersistentFS: Coprocessors use a persistent storage file system stored\n\
                      in a file on the host. All reads and writes performed on\n\
                      the coprocessor are handled by the block device and\n\
                      transferred over PCIe to the host, where actual file\n\
                      read/write operations are performed. All changes to the\n\
                      coprocessor file system are preserved between coprocessor\n\
                      reboots.\n\
\n"
COMMON_HELP
"\
    -w\n\
    --wait\n\
\n\
        Wait for the --boot command to finish before exiting micctrl.\n\
\n\
    -t <timeout>\n\
    --timeout=<timeout>\n\
\n\
        Set the time out value in seconds for the --wait option.\n\
\n",

/* Reset Help */
"\
micctrl -r [sub options] [list of coprocessors]\n\
micctrl --reset [sub options] [list of coprocessors]\n\
\n\
The --reset command resets all or specified coprocessors that are in a stable\n\
state. Once completed their state is changed to 'ready to boot'.\n\
\n"
COMMON_HELP
"\
    -w\n\
    --wait\n\
\n\
        Wait for the --reset command to finish before exiting micctrl.\n\
\n\
    -t <timeout>\n\
    --timeout=<timeout>\n\
\n\
        Set the time out value in seconds for the --wait option.\n\
\n\
    -f\n\
    --force\n\
\n\
        Force reset regardless of the coprocessors' state. Normally the driver\n\
        will not reset coprocessors if they are not in a stable state and\n\
        micctrl will return an error. Please note, that force reset may corrupt\n\
        the coprocessor file system.\n\
\n"
,
/* Shutdown Help */
"\
micctrl -S [sub options] [list of coprocessors]\n\
micctrl --shutdown [sub options] [list of coprocessors]\n\
\n\
The --shutdown command shuts down all or specified coprocessors that are in a\n\
'online' or 'ready' state. Once completed their state is changed to 'shutdown'.\n\
\n"
COMMON_HELP
"\
    -w\n\
    --wait\n\
\n\
        Wait for the --shutdown command to finish before exiting micctrl.\n\
\n\
    -t <timeout>\n\
    --timeout=<timeout>\n\
\n\
        Set the time out value in seconds for the --wait option.\n\
\n\
    -f\n\
    --force\n\
\n\
        Force shut down regardless of the coprocessors' state. Normally the\n\
        driver will not shut down coprocessors if they are not in\n\
        a 'online' or 'ready' state and micctrl will return an error.\n\
        Please note, that force shutdown may corrupt the coprocessor file system.\n\
\n",
/* Reboot Help */
"\
micctrl -R [sub options] [list of coprocessors]\n\
micctrl --reboot [sub options] [list of coprocessors]\n\
\n\
The --reboot command reboots all or specified coprocessors that are in the\n\
'online' or 'error' state.\n\
\n"
COMMON_HELP
"\
    -w\n\
    --wait\n\
\n\
        Wait for the --reboot command to finish before exiting micctrl.\n\
\n\
    -t <timeout>\n\
    --timeout=<timeout>\n\
\n\
        Set the time out value in seconds for the --wait option.\n\
\n"
};

void run_command_state(po::variables_map &vm, const std::vector<std::string> &options, int mode)
{
	bool force = false;
	bool wait_for_state = false;
	int timeout = 0;

	if (vm.count("help")) {
		micctrl_help(state_help[mode]);
		exit(0);
	}
	if (vm.count("wait")) {
		timeout = -1;
		wait_for_state = true;
	}

	po::options_description state_options;
	state_options.add_options()
			("force,f", "")
			("timeout,t", po::value<int>());

	po::store(po::command_line_parser(options).options(state_options).run(), vm);

	if (vm.count("force"))
		force = true;

	if (vm.count("timeout")) {
		// 30 minutes is a long time to wait
		timeout = vm["timeout"].as<int>();
		if (timeout < 0 || timeout > (30 * 60)) {
			display(PERROR, "Timeouts longer than 30 minutes are not allowed\n");
			exit(EXIT_FAILURE);
		}
	}

	std::shared_ptr<mic_info> miclist = create_miclist(vm);

	int rc = 0;

	switch (mode) {
	case STATE_BOOT:
		rc = boot(miclist.get(), timeout, wait_for_state);
		break;
	case STATE_RESET:
		rc = reset(miclist.get(), timeout, force, wait_for_state);
		break;
	case STATE_SHUTDOWN:
		rc = shutdown(miclist.get(), timeout, force, wait_for_state);
		break;
	case STATE_REBOOT:
		rc = reboot(miclist.get(), timeout, wait_for_state);
		break;
	}

	exit(rc > 0 ? EXIT_FAILURE : 0);
}

/*
 * micctrl --wait [-t <timeout> [MIC list]
 *
 * Wait for one or more MIC coprocessors to not be in the 'resetting' or 'booting' states
 */
const char *wait_help =
"\
micctrl -w [sub options] [list of coprocessors]\n\
micctrl --wait [sub options] [list of coprocessors]\n\
\n\
The --wait command waits for the previous state change command to complete and\n\
then exits micctrl.\n\
\n"
COMMON_HELP
"\
    -t <timeout>\n\
    --timeout=<timeout>\n\
\n\
        Set the time out value in seconds for the --wait command.\n\
\n";

void run_command_wait(po::variables_map &vm, const std::vector<std::string> &options)
{
	int timeout = -1;

	if (vm.count("help")) {
		micctrl_help(wait_help);
		exit(0);
	}

	po::options_description wait_options;
	wait_options.add_options()
			("timeout,t", po::value<int>());

	po::store(po::command_line_parser(options).options(wait_options).run(), vm);

	if (vm.count("timeout")) {
		timeout = vm["timeout"].as<int>();

		// 30 mitnues is a long time to wait
		if (timeout < 0 || timeout > (30 * 60)) {
			display(PERROR, "Timeouts longer than 30 minutes are not allowed\n");
			exit(EXIT_FAILURE);
		}
	}

	std::shared_ptr<mic_info> miclist = create_miclist(vm);

	std::list<mic_info*> mic_list;
	for (auto& mic: miclist) {
		mic_list.push_back(&mic);
	}

	int rc = wait(mic_list, timeout, MIC_STATE_UNKNOWN);
	exit(rc > 0 ? EXIT_FAILURE : 0);
}

/*
 * micctrl --status [MIC list]
 *
 * Display the status (state) of the listed MIC coprocessors
 */
const char *status_help =
"\
micctrl -s [sub options] [list of coprocessors]\n\
micctrl --status [sub options] [list of coprocessors]\n\
\n\
The --status command displays the current state of all or specified coprocessors.\n\
\n"
COMMON_HELP;

void run_command_status(po::variables_map &vm, const std::vector<std::string> &options)
{
	if (vm.count("help")) {
		micctrl_help(status_help);
		exit(0);
	}

	po::options_description status_options;
	po::store(po::command_line_parser(options).options(status_options).run(), vm);

	int rc = status(create_miclist(vm).get(), vm.count("verbose"));
	exit(rc > 0 ? EXIT_FAILURE : 0);
}

static const char*
state_to_string(int state)
{
	switch (state) {
	case MIC_STATE_ERROR:
		return "error";
	case MIC_STATE_READY:
		return "ready to boot";
	case MIC_STATE_ONLINE:
		return "online";
	case MIC_STATE_ONLINE_FIRMWARE:
		return "online firmware";
	case MIC_STATE_SHUTDOWN:
		return "shutdown";
	case MIC_STATE_RESETTING:
		return "resetting";
	case MIC_STATE_SHUTTING_DOWN:
		return "shutting down";
	case MIC_STATE_BOOTING:
		return "booting";
	case MIC_STATE_BOOTING_FIRMWARE:
		return "booting firmware";
	}

	return "unknown";
}

static bool
is_state_stable(struct mic_info* mic, int state)
{
	std::string execute_on_ready = mpss_readsysfs(mic->name, "config/execute_on_ready");

	switch(state) {
	case MIC_STATE_RESETTING:
	case MIC_STATE_SHUTTING_DOWN:
	case MIC_STATE_BOOTING:
	case MIC_STATE_BOOTING_FIRMWARE:
		return false;
	case MIC_STATE_READY:
		if (execute_on_ready != MIC_CMD_NONE_STR) {
			return false;
		}
	}
	return true;
}

/*
 * Check if attempt to write command to sysfs success.
 * EALREADY error means that command is currently being executed (or it's already in target state)
 */
static bool
is_failed_to_set_command(int rc) {
	if (rc && rc != EALREADY) {
		return true;
	}
	return false;
}

static void
display_current_card_state(struct mic_info* mic)
{
	int state = mic_get_state_id(mic, &display);
	std::string state_text = state_to_string(state);

	display(PNORM, "%s: %s\n", mic->name.c_str(), state_text.c_str());
}

/*
 * Boot the list of MIC coprocessors.  If a timeout value is specified then wait for coprocessors to
 * complete booting.  If the timeout is exceeded then print an error and exit.
 */
static int
boot(struct mic_info* miclist, int timeout, bool wait_for_state)
{
	int fail = 0;
	std::list<mic_info*> mic_list;

	if (!mpssenv_aquire_lockfile()) {
		display(PERROR, "Cannot boot coprocessors - MPSS daemon is not running\n");
		exit(EXIT_FAILURE);
	}

	for (auto& mic: miclist) {
		if ((micctrl_parse_config(&mic, mpssperr, PINFO))) {
			display(PERROR, "%s: boot aborted - configuration file not valid\n", mic.name.c_str());
			fail++;
			continue;
		}

		int state = mic_get_state_id(&mic, &display);
		if (state == MIC_STATE_SHUTDOWN) {
			int rc = mpss_setsysfs(mic.name, "config/execute_on_ready", MIC_CMD_BOOT_STR);
			if (rc) {
				display(PERROR, "%s: boot failed with error '%s'\n", mic.name.c_str(), strerror(rc));
				fail++;
				continue;
			}

			rc = mic_reset(&mic, false, NULL);
			if (is_failed_to_set_command(rc)) {
				display(PERROR, "%s: shutdown to ready failed with error '%s'\n", mic.name.c_str(), strerror(rc));
				fail++;
				continue;
			}
		} else {
			int rc = mic_boot(&mic, NULL);
			if (is_failed_to_set_command(rc)) {
				display(PERROR, "%s: boot failed with error: '%s'\n", mic.name.c_str(), strerror(rc));
				fail++;
				continue;
			}
		}

		display_current_card_state(&mic);
		mic_list.push_back(&mic);
	}

	if (timeout || wait_for_state) {
		int rc = wait(mic_list, timeout, MIC_STATE_ONLINE);
		fail += rc;
	}

	return fail;
}

/*
 * Reset the list of MIC coprocessors.  If a timeout value is specified then wait for coprocessors to
 * complete booting.  If the timeout is exceeded then print an error and exit.
 */
static int
reset(struct mic_info* miclist, int timeout, bool force, bool wait_for_state)
{
	int fail = 0;
	std::list<mic_info*> mic_list;

	for (auto& mic: miclist) {
		int rc = mic_reset(&mic, force, NULL);
		if (is_failed_to_set_command(rc)) {
			display(PERROR, "%s: reset failed with error '%s'\n", mic.name.c_str(), strerror(rc));
			fail++;
			continue;
		}

		display_current_card_state(&mic);
		mic_list.push_back(&mic);
	}

	if (timeout || wait_for_state) {
		int rc = wait(mic_list, timeout, MIC_STATE_READY);
		fail += rc;
	}

	return fail;
}

/*
 * Shutdown the list of MIC coprocessors.  If a timeout value is specified then wait for coprocessors to
 * complete booting.  If the timeout is exceeded then print an error and exit.
 */
static int
shutdown(struct mic_info* miclist, int timeout, bool force, bool wait_for_state)
{
	int fail = 0;
	std::list<mic_info*> mic_list;

	for (auto& mic: miclist) {
		int rc = mic_shutdown(&mic, force, NULL);
		if (is_failed_to_set_command(rc)) {
			display(PERROR, "%s: shutdown failed with error '%s'\n", mic.name.c_str(), strerror(rc));
			fail++;
			continue;
		}

		display_current_card_state(&mic);
		mic_list.push_back(&mic);
	}

	if (timeout || wait_for_state) {
		int rc = wait(mic_list, timeout, MIC_STATE_SHUTDOWN);
		fail += rc;
	}

	return fail;
}

/*
 * Reboot the list of MIC coprocessors.  If a timeout value is specified then wait for coprocessors to
 * complete booting.  If the timeout is exceeded then print an error and exit.
 */
static int
reboot(struct mic_info* miclist, int timeout, bool wait_for_state)
{
	std::list<mic_info*> mic_list;
	int fail = 0;

	if (!mpssenv_aquire_lockfile()) {
		display(PERROR, "Cannot reboot coprocessors - MPSS daemon is not running\n");
		return 1;
	}

	for (auto& mic: miclist) {
		int state = mic_get_state_id(&mic, NULL);
		if (state != MIC_STATE_ONLINE && state != MIC_STATE_ERROR) {
			display(PNORM, "%s: not online/error state - skipping reboot\n", mic.name.c_str());
			fail++;
			continue;
		}

		int rc = mpss_setsysfs(mic.name, "config/execute_on_ready", MIC_CMD_BOOT_STR);
		if (rc) {
			display(PERROR, "%s: reboot failed with error '%s'\n", mic.name.c_str(), strerror(rc));
			fail++;
			continue;
		}

		rc = mic_reset(&mic, false, NULL);
		if (is_failed_to_set_command(rc)) {
			display(PERROR, "%s: reboot failed with error '%s'\n", mic.name.c_str(), strerror(rc));
			fail++;
			continue;
		}

		mic_list.push_back(&mic);
		display(PNORM, "%s: rebooting\n", mic.name.c_str());
	}

	if (timeout || wait_for_state) {
		int rc = wait(mic_list, timeout, MIC_STATE_ONLINE);
		fail += rc;
	}

	return fail;
}

/*
 * Display the status (state) of the MIC coprocessors.
 */
static int
status(struct mic_info* miclist, bool verbose_mode)
{
	int fail = 0;

	for (auto& mic: miclist) {
		int state = mic_get_state_id(&mic, NULL);
		if (state == MIC_STATE_UNKNOWN) {
			fail++;
			display(PERROR, "%s: state failed - non existent MIC device\n", mic.name.c_str());
			continue;
		}

		if (verbose_mode) {
			std::string state_text = state_to_string(state);

			std::string state_msg = mpss_readsysfs(mic.name, "state_msg");
			if (!state_msg.empty())
				state_text += " (" + state_msg + ")";

			printf("%s: \n", mic.name.c_str());

			println_indent(FIRST_LEVEL_INDENT, "state:", state_text.c_str());

			std::string post_code = mpss_readsysfs(mic.name, "spad/post_code");
			println_indent(FIRST_LEVEL_INDENT, "post code:", post_code.c_str());
		} else {
			printf("%s: %s\n", mic.name.c_str(), state_to_string(state));
		}
	}

	return fail;
}

/*
 * Wait for the list of MIC coprocessors to be in the stable state.
 */
static int
wait(std::list<mic_info*>& mic_list, int timeout, int target_state)
{
	struct timeval start_time, end_time;
	struct mic_waiting_info {
		mic_info* info;
		int fd;
		int prev_state;
	};

	std::list<struct mic_waiting_info> mic_wait_list;
	std::vector<struct pollfd> pfds_v;

	gettimeofday(&start_time, NULL);

	for (auto itr = mic_list.begin(); itr != mic_list.end(); ++itr) {
		struct mic_waiting_info mic_wait;
		auto mic = *itr;

		mic_wait.info = mic;

		std::string mic_state_file = MICSYSFSDIR"/" + mic->name + "/" + "state";
		int fd = open(mic_state_file.c_str(), O_RDONLY);

		struct pollfd pfd;
		pfd.fd = fd;
		pfd.events = POLLERR | POLLPRI;

		pfds_v.push_back(pfd);
		mic_wait.fd = fd;
		mic_wait.prev_state = MIC_STATE_UNKNOWN;

		mic_wait_list.push_back(mic_wait);
	}

	int fail = 0;
	int mic_list_size = mic_wait_list.size();
	while (!mic_wait_list.empty()) {
		int poll_status = poll(pfds_v.data(), mic_list_size, 1000);
		bool is_timeout = false;

		if (timeout != -1) {
			gettimeofday(&end_time, NULL);
			if (end_time.tv_sec - start_time.tv_sec > timeout) {
				is_timeout = true;
			}
		}

		if (poll_status < 0) {
			if (errno != EINTR) {
				display(PERROR, "poll failed: %s\n", strerror(errno));
			}
			if (!is_timeout) {
				continue;
			}
		}

		for (auto itr = mic_wait_list.begin(); itr != mic_wait_list.end();) {
			auto& mic = *itr;

			std::string value = mic_read_sysfs_file(mic.fd);
			if (value.empty()) {
				display(PERROR, "read failed: %s\n", strerror(errno));
				itr = mic_wait_list.erase(itr);
				fail++;
				continue;
			}
			int state = mic_state_to_id(value.c_str());

			if (state == MIC_STATE_UNKNOWN) {
				display(PWARN, "%s: not detected - skipping\n", mic.info->name.c_str());
				itr = mic_wait_list.erase(itr);
				fail++;
				continue;
			}

			if (state == MIC_STATE_ERROR) {
				std::string msg = mpss_readsysfs(mic.info->name, "state_msg");
				display(PNORM, "%s: %s (%s)\n", mic.info->name.c_str(), state_to_string(state), msg.c_str());
				itr = mic_wait_list.erase(itr);
				fail++;
				continue;
			}

			if (state == target_state || (target_state == MIC_STATE_UNKNOWN && is_state_stable(mic.info, state))) {
				display(PNORM, "%s: %s\n", mic.info->name.c_str(), state_to_string(state));
				itr = mic_wait_list.erase(itr);
				continue;
			}
			if (is_timeout) {
				display(PNORM, "%s: timeout occured in state '%s'\n",
					mic.info->name.c_str(), state_to_string(state));
				itr = mic_wait_list.erase(itr);
				fail++;
				continue;
			}
			if (mic.prev_state != MIC_STATE_UNKNOWN && mic.prev_state != state) {
				display(PNORM, "%s: %s\n", mic.info->name.c_str(), state_to_string(state));
			}
			mic.prev_state = state;
			itr = ++itr;
		}
	}
	return fail;
}
