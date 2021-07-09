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

#include "mpssd.h"

#include "sync_utils.h"
#include "utils.h"
#include "virtio.h"

#include <mic.h>

#include <fcntl.h>
#include <getopt.h>
#include <pwd.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <poll.h>

#define LOGFILE_NAME "/var/log/mpssd"


mpssd_manager g_manager;

static std::shared_ptr<mic_info> mics;
struct mic_info *miclist;

FILE *logfp;


void
quit_sig_handler(int, siginfo_t*, void*)
{
	g_manager.request_stop();
}

void
setsighandlers()
{
	struct sigaction act;

	memset(&act, 0, sizeof(act));
	act.sa_sigaction = quit_sig_handler;
	act.sa_flags = SA_SIGINFO;
	sigaction(SIGTERM, &act, NULL);
	sigaction(SIGINT, &act, NULL);
}

struct option longopts[] = {
	{"help", 0, NULL, 'h'},
	{0}
};

const char *hs = {
	"\nmpssd -h\n"
	"mpssd --help\n\n"
	"    Display help messages for the mpssd daemon\n\n"
	"mpssd \n\n"
	"    This is the MPSS stack MIC card daemon.  When started it parses the MIC\n"
	"    card configuration files in the " DEFAULT_CONFDIR " directory and sets\n"
	"    various parameters in the /sys/class/mic sysfs entries.  These entries\n"
	"    include the kernel command line paramters to pass to the embedded Linux OS\n"
	"    running on the MIC card.\n\n"
	"    The mpssd daemon then indicates the cards marked with 'AutoBoot'\n"
	"    enabled should start booting.  When complete it waits for the MIC cards\n"
	"    boot process to request their file systems to be downloaded.\n\n"
	"    To stop the mpssd daemon, a signal 3 (QUIT) is sent to the daemon's pid.\n"
};

void
usage()
{
	printf("%s\n", hs);
}

void
parse_cmd_args(int argc, char *argv[])
{
	int longidx;
	int c;

	while ((c = getopt_long(argc, argv, "h", longopts, &longidx)) != -1) {
		switch(c) {
		case 'h':
			usage();
			exit(0);

		default:
			fprintf(stderr, "Unknown argument %d\n", c);
			usage();
			exit(EINVAL);
		}
	}

	if (optind != argc) {
		fprintf(stderr, "Too many arguments '%s'\n", optarg);
		usage();
		exit(EINVAL);
	}
}

void
start_daemon(void)
{
	int err;

	if ((err = mpssenv_aquire_lockfile())) {
		fprintf(stderr, "Error acquiring lockfile %s: %s\n", LSB_LOCK_FILENAME, strerror(err));
		exit(1);
	}

	for (auto& mic: miclist) {
		g_manager.add(&mic);
	}
	g_manager.start_authentication_threads();
	g_manager.wait_and_notify_systemd();

	g_manager.wait_for_stop_request();
	g_manager.stop();
}

int
main(int argc, char* argv[])
{
	if (load_mic_modules()) {
		fprintf(stderr, "cannot load mic modules\n");
		exit(1);
	}

	parse_cmd_args(argc, argv);
	setsighandlers();

	if (logfp != stderr) {
		if ((logfp = fopen(LOGFILE_NAME, "a")) == NULL) {
			fprintf(stderr, "cannot open logfile '%s'\n", LOGFILE_NAME);
			exit(EBADF);
		}

	}

	mic_set_verbose_mode(MIC_VERBOSE_NORMAL | MIC_VERBOSE_DEBUG);

	mpssd_log(PINFO, "MPSS Daemon start");
	display_modules_version();

	mics = mpss_get_miclist(NULL);
	miclist = mics.get();
	if (mics == NULL) {
		mpssd_log(PINFO, "MIC module not loaded");
		exit(2);
	}

	start_daemon();

	return 0;
}

const char*
mpssd_error_to_string(mpssd_error error)
{
	switch (error) {
	case mpssd_error::NONE:
		return "NONE";
	case mpssd_error::INVALID_CONFIG:
		return "INVALID_CONFIG";
	case mpssd_error::NO_DEVICE:
		return "NO_DEVICE";
	case mpssd_error::BOOT_FAILED:
		return "BOOT_FAILED";
	case mpssd_error::RESET_FAILED:
		return "RESET_FAILED";
	case mpssd_error::SHUTDOWN_FAILED:
		return "SHUTDOWN_FAILED";
	case mpssd_error::VIRTIO_INIT_FAILED:
		return "VIRTIO_INIT_FAILED";
	}

	return NULL;
}

const char*
mpssd_state_to_string(mpssd_state state)
{
	switch (state) {
	case mpssd_state::MIC_ERROR:
		return "MIC_ERROR";
	case mpssd_state::MIC_READY:
		return "MIC_READY";
	case mpssd_state::MIC_ONLINE:
		return "MIC_ONLINE";
	case mpssd_state::MIC_ONLINE_FIRMWARE:
		return "MIC_ONLINE_FIRMWARE";
	case mpssd_state::MIC_SHUTDOWN:
		return "MIC_SHUTDOWN";
	case mpssd_state::MIC_RESETTING:
		return "MIC_RESETTING";
	case mpssd_state::MIC_SHUTTING_DOWN:
		return "MIC_SHUTTING_DOWN";
	case mpssd_state::MIC_BOOTING:
		return "MIC_BOOTING";
	case mpssd_state::MIC_BOOTING_FIRMWARE:
		return "MIC_BOOTING_FIRMWARE";
	case mpssd_state::CTRL_THREAD_INIT:
		return "CTRL_THREAD_INIT";
	case mpssd_state::CTRL_THREAD_SHUTDOWN:
		return "CTRL_THREAD_SHUTDOWN";
	case mpssd_state::CTRL_THREAD_POST_INITIAL_STATE:
		return "CTRL_THREAD_POST_INITIAL_STATE";
	};

	return NULL;
}

mpssd_info::mpssd_info(mic_info* mic)
	: m_mic(mic)
	, m_mdc(nullptr)
	, m_autobooted(false)
	, m_error(mpssd_error::NONE)
	, m_ready_to_notify_systemd(false)
	, m_recv_ep(-1)
	, m_send_ep(-1)
{
	add_state_cb(mpssd_state::CTRL_THREAD_INIT, std::bind(&mpssd_info::reload_config, this));
	add_state_cb(mpssd_state::CTRL_THREAD_INIT, std::bind(&mpssd_info::clear_execute_on_ready, this));

	add_state_cb(mpssd_state::CTRL_THREAD_POST_INITIAL_STATE, std::bind(&mpssd_info::set_readiness_to_notify, this));

	add_state_cb(mpssd_state::MIC_READY, std::bind(&mpssd_info::stop_virtio_threads, this));
	add_state_cb(mpssd_state::MIC_READY, std::bind(&mpssd_info::check_autoboot, this));
	add_state_cb(mpssd_state::MIC_READY, std::bind(&mpssd_info::ready_handler, this));

	add_state_cb(mpssd_state::MIC_BOOTING, std::bind(&mpssd_info::reload_config, this));
	add_state_cb(mpssd_state::MIC_BOOTING, std::bind(&mpssd_info::check_root_fs, this));
	add_state_cb(mpssd_state::MIC_BOOTING, std::bind(&mpssd_info::start_virtio_threads, this));

	add_state_cb(mpssd_state::MIC_SHUTDOWN, std::bind(&mpssd_info::stop_virtio_threads, this));
	add_state_cb(mpssd_state::MIC_SHUTDOWN, std::bind(&mpssd_info::check_autoboot, this));
	add_state_cb(mpssd_state::MIC_SHUTDOWN, std::bind(&mpssd_info::shutdown_handler, this));

	add_state_cb(mpssd_state::MIC_ERROR, std::bind(&mpssd_info::error_handler, this));
	add_state_cb(mpssd_state::MIC_ERROR, std::bind(&mpssd_info::clear_execute_on_ready, this));

	add_state_cb(mpssd_state::MIC_RESETTING, std::bind(&mpssd_info::check_autoboot, this));

	/*
	 * At this stage all virtio threads should be already closed.
	 * On the other hand, due to some errors it might be not true.
	 * Therefore, just in case, make sure all virtio threads are stopped.
	 */
	add_state_cb(mpssd_state::CTRL_THREAD_SHUTDOWN, std::bind(&mpssd_info::clear_execute_on_ready, this));
	add_state_cb(mpssd_state::CTRL_THREAD_SHUTDOWN, std::bind(&mpssd_info::stop_virtio_threads, this));
}

mpssd_info::~mpssd_info()
{
	mpssd_log(PINFO, "Destroying %s main card object", name().c_str());
	delete m_mdc;
	m_mdc = nullptr;
}

void
mpssd_info::add_state_cb(mpssd_state state, mpssd_callback callback)
{
	m_cbs[state].push_back(callback);
}

void
mpssd_info::execute_state_cbs(mpssd_state state)
{
	mpssd_log(PINFO, "Execute callbacks for state: %s", mpssd_state_to_string(state));

	auto cbs_it = m_cbs.find(state);
	if (cbs_it == m_cbs.end()) {
		return;
	}

	for (auto& func : cbs_it->second) {
		func(this);
	}
}

void
mpssd_info::execute_state_cbs(int mic_state)
{
	if (mic_state < 0 || mic_state > MIC_STATE_LAST) {
		mpssd_log(PERROR, "Invalid state: %d", mic_state);
		return;
	}

	execute_state_cbs(static_cast<mpssd_state>(mic_state));
}

void
mpssd_info::set_error(mpssd_error error)
{
	mpssd_log(PERROR, "Error: %s", mpssd_error_to_string(error));
	m_error = error;
}

void
mpssd_info::save_crash_log()
{
	mpss_elist mpss_logs;

	mpssut_print_card_log(m_mic, mpss_logs);
	mpssd_elist_log(mpss_logs, &mpsslog);
}

void
mpssd_info::check_root_fs()
{
	std::string root_fs_image_path = m_mic->config.rootdev.target;

	struct stat buffer;
	if (stat(root_fs_image_path.c_str(), &buffer) != 0) {
		mpssd_log(PWARN, "Root file system image '%s' does not exist", root_fs_image_path.c_str());
		return;
	}

	std::string tool = "/usr/sbin/fsck -nv";
	std::string command = tool + " " + root_fs_image_path + " 2> /dev/null";

	mpssd_log(PINFO, "Executing file system check on '%s'", root_fs_image_path.c_str());
	mpssd_log(PINFO, "---- '%s' begin ----", tool.c_str());

	mpss_elist mpss_logs;
	exec_command(command, mpss_logs);
	mpssd_elist_log(mpss_logs, &mpsslog);

	mpssd_log(PINFO, "---- '%s' end ----", tool.c_str());
}

void
mpssd_info::boot_invoker()
{
	reload_config();

	mpssd_log(PINFO, "Starting BOOT procedure");
	if (mic_boot(m_mic, &mpsslog)) {
		set_error(mpssd_error::BOOT_FAILED);
	}
}

void
mpssd_info::reset_invoker()
{
	mpssd_log(PINFO, "Starting RESET procedure");
	if (mic_reset(m_mic, false, &mpsslog)) {
		set_error(mpssd_error::RESET_FAILED);
	}
}

void
mpssd_info::shutdown_invoker()
{
	mpssd_log(PINFO, "Starting SHUTDOWN procedure");
	if (mic_shutdown(m_mic, false, &mpsslog)) {
		set_error(mpssd_error::SHUTDOWN_FAILED);
	}
}

void
mpssd_info::reload_config()
{
	int rc;
	mpss_elist mpss_logs;

	if ((rc = mpss_parse_config(m_mic, mpss_logs)) != 0) {
		mpssd_log(PERROR, "Boot aborted - configuration file not valid: %s", strerror(rc));
		mpssd_elist_log(mpss_logs, &mpsslog);
		set_error(mpssd_error::INVALID_CONFIG);
	}
}

void
mpssd_info::check_autoboot()
{
	mpssd_log(PINFO, "Check autoboot");
	if (!m_autobooted && m_mic->config.boot.onstart == TRUE) {
		mpss_setsysfs(m_mic->name, "config/execute_on_ready", MIC_CMD_BOOT_STR);
	}
}

void
mpssd_info::clear_execute_on_ready()
{
	mpssd_log(PINFO, "Clear execute_on_ready");
	mpss_setsysfs(m_mic->name, "config/execute_on_ready", MIC_CMD_NONE_STR);
}

void
mpssd_info::set_readiness_to_notify()
{
	m_ready_to_notify_systemd = true;
}

void
mpssd_info::start_virtio_threads()
{
	mpssd_log(PINFO, "Start creating virtio threads");

	if (m_mdc) {
		mpssd_log(PERROR, "Virtio threads already created");
		set_error(mpssd_error::VIRTIO_INIT_FAILED);
		return;
	}

	m_mdc = new mic_device_context();

	m_mdc->m_vcon_thread = std::thread(virtio_console, m_mdc, m_mic);
	m_mdc->m_vnet_thread = std::thread(virtio_net, m_mdc, m_mic);

	/* block devices */
	for (int i = 0; i < BLOCK_MAX_COUNT; ++i) {
		if (!m_mic->config.blockdevs[i].source.empty()) {
			if (!setup_virtio_block(m_mic, m_mdc, i)) {
				continue;
			}

			mpssd_log(PINFO, "Launching thread for %d block device", i);

			m_mdc->m_vblk_vector.emplace_back(virtio_block, m_mdc, m_mic, i);
		}
	}

	mpssd_log(PINFO, "Finish creating virtio threads");
}

void
mpssd_info::stop_virtio_threads()
{
	mpssd_log(PINFO, "Wait for stop of all virtio threads");

	if (m_mdc) {
		m_mdc->shutdown();
		delete m_mdc;
		m_mdc = nullptr;
	}

	mpssd_log(PINFO, "All virtio threads have been stopped");
}

void
mpssd_info::shutdown_handler()
{
	mpssd_log(PINFO, "The SHUTDOWN state handler");

	std::string execute_on_ready = mpss_readsysfs(m_mic->name, "config/execute_on_ready");
	if (execute_on_ready == MIC_CMD_BOOT_STR) {
		mpssd_log(PINFO, "Start resetting the card, so it could be autobooted");
		reset_invoker();
	}
}

void
mpssd_info::ready_handler()
{
	mpssd_log(PINFO, "The READY state handler");

	/* Check whether the card is supposed to be rebooted. */
	std::string execute_on_ready = mpss_readsysfs(m_mic->name, "config/execute_on_ready");
	if (execute_on_ready == MIC_CMD_BOOT_STR) {
		m_autobooted = true;
		mpssd_log(PINFO, "Start booting the card");
		boot_invoker();
		return;
	}
}

void
mpssd_info::error_handler()
{
	mpssd_log(PINFO, "The ERROR state handler");
	save_crash_log();
}

int
mpssd_info::get_state_id(int fd)
{
	std::string state_value = mic_read_sysfs_file(fd);
	if (state_value.empty()) {
		mpssd_log(PERROR, "read failed: %s\n", strerror(errno));
		return MIC_STATE_UNKNOWN;
	}
	return mic_state_to_id(state_value.c_str());
}

bool
mpssd_info::stop_condition(int mic_state)
{
	if (mic_state == MIC_STATE_UNKNOWN) {
		mpssd_log(PERROR, "Unknown state - shutting down the control thread");
		return false;
	}

	if (m_status.is_shutdown_requested()) {
		switch (mic_state) {
		case MIC_STATE_ONLINE:
			shutdown_invoker();
			return true;
		case MIC_STATE_ERROR:
		case MIC_STATE_SHUTDOWN:
		case MIC_STATE_READY:
		case MIC_STATE_BOOTING_FIRMWARE:
		case MIC_STATE_ONLINE_FIRMWARE:
			return false;
		case MIC_STATE_BOOTING:
		case MIC_STATE_RESETTING:
		case MIC_STATE_SHUTTING_DOWN:
			return true;
		}
	}

	return true;
}

void
mpssd_info::control_thread()
{
	set_thread_name(name().c_str(), "control");

	mpssd_log(PINFO, "Start the control thread");

	std::string mic_state_file = MICSYSFSDIR"/" + m_mic->name + "/" + "state";

	int fd = open(mic_state_file.c_str(), O_RDONLY);
	if (fd < 0) {
		mpssd_log(PERROR, "Opening file %s failed: %s",
			mic_state_file.c_str(), strerror(errno));
		set_error(mpssd_error::NO_DEVICE);
		return;
	}

	/*
	 * Execute all init procedures.
	 */
	execute_state_cbs(mpssd_state::CTRL_THREAD_INIT);

	/*
	 * Read initial state.
	 */
	int state_id = get_state_id(fd);
	int state_old_id = MIC_STATE_UNKNOWN;

	/*
	 * Start tracking state changes.
	 */
	struct pollfd polld;
	polld.fd = fd;
	polld.events = POLLERR | POLLPRI;

	while (stop_condition(state_id)) {
		if (state_id != state_old_id) {
			execute_state_cbs(state_id);

			/* After initial state has been changed, we have to execute handler. */
			if (state_old_id == MIC_STATE_UNKNOWN) {
				execute_state_cbs(mpssd_state::CTRL_THREAD_POST_INITIAL_STATE);
			}
			state_old_id = state_id;
		}

		int poll_status = poll(&polld, 1, 1000);

		if (poll_status == 0) {
			continue;
		}

		if (poll_status < 0) {
			if (errno != EINTR) {
				mpssd_log(PERROR, "poll failed: %s", strerror(errno));
			}
			continue;
		}

		state_id = get_state_id(fd);
	}

	/*
	 * Execute all shutdown procedures.
	 */
	execute_state_cbs(mpssd_state::CTRL_THREAD_SHUTDOWN);

	close(fd);

	mpssd_log(PINFO, "Exit the control thread");
}

std::string
mpssd_info::name()
{
	return m_mic ? m_mic->name : "";
}

int
mpssd_info::id()
{
	return m_mic ? m_mic->id : -1;
}

void
mpssd_info::start_control_thread()
{
	mpssd_log(PINFO, "Create the %s control thread", name().c_str());
	m_control_thread = std::thread(&mpssd_info::control_thread, this);
}

void
mpssd_info::request_stop_control_thread()
{
	mpssd_log(PINFO, "Request stop the %s control thread", name().c_str());
	m_status.request_shutdown();
}

void
mpssd_info::stop_control_thread()
{
	request_stop_control_thread();

	mpssd_log(PINFO, "Wait for stop of the %s control thread", name().c_str());
	m_control_thread.join();
	mpssd_log(PINFO, "The %s control thread has been stopped", name().c_str());
}

bool
mpssd_info::is_ready_to_notify_systemd()
{
	return m_ready_to_notify_systemd;
}

void
mpssd_info::start_monitor_thread()
{
	m_card_mon_thread = std::thread(&mpssd_info::card_monitor_thread, this);
	m_card_mon_thread.detach();
}

void
mpssd_info::close_scif_connection()
{
	if (m_recv_ep != -1) {
		scif_close(m_recv_ep);
		m_recv_ep = -1;
	}
	if (m_send_ep != -1) {
		scif_close(m_send_ep);
		m_send_ep = -1;
	}
}

mpssd_manager::mpssd_manager()
	: m_nextjobid(100)
{}

bool
mpssd_manager::add(mic_info* mic)
{
	if (!mic || mic->name.empty()) {
		mpssd_log(PERROR, "Invalid argument");
		return false;
	}

	if (m_map.find(mic->name) != m_map.end()) {
		mpssd_log(PWARN, "The %s card has been already added", mic->name.c_str());
		return false;
	}

	int mic_state = mic_get_state_id(mic, &mpsslog);
	if (mic_state == MIC_STATE_UNKNOWN) {
		mpssd_log(PWARN, "The %s card is not listed in the SysFs interface", mic->name.c_str());
		return false;
	}

	mpssd_info* mpssdi = new mpssd_info(mic);
	mic->data = mpssdi;

	m_map[mic->name] =  mpssdi;

	mpssdi->start_control_thread();

	return true;
}

mpssd_info *
mpssd_manager::get_card_by_id(int micid)
{
	for (auto iter : m_map) {
		if ((iter.second)->id() == micid) {
			return iter.second;
		}
	}
	return NULL;
}

void
mpssd_manager::request_stop()
{
	mpssd_log(PINFO, "Request stop");
	m_status.request_shutdown();
}

void
mpssd_manager::wait_for_stop_request()
{
	mpssd_log(PINFO, "Wait for stop request");
	m_status.wait_for_shutdown();
}

void
mpssd_manager::wait_and_notify_systemd()
{
	mpssd_log(PINFO, "Wait for all cards to be processed");
	while (true) {
		bool card_ready = true;
		for (auto iter : m_map) {
			mpssd_info* mpssdi = iter.second;

			if (!mpssdi->is_ready_to_notify_systemd()) {
				card_ready = false;
				break;
			}
		}
		if (card_ready) {
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	notify_systemd();
	mpssd_log(PINFO, "All cards are processed - notify systemd");
}

void
mpssd_manager::stop()
{
	mpssd_log(PINFO, "Request stop of all cards threads");
	for (auto iter : m_map) {
		mpssd_info* mpssdi = iter.second;
		mpssdi->request_stop_control_thread();
	}

	mpssd_log(PINFO, "Wait for stop of all cards threads");
	for (auto iter : m_map) {
		mpssd_info* mpssdi = iter.second;
		mpssdi->stop_control_thread();
	}

	/*
	 * monitor_thread is using mpssdi instance, so we can
	 * remove it when daemon threads are not active anymore.
	 */
	stop_authentication_threads();
	for (auto iter : m_map) {
		mpssd_info* mpssdi = iter.second;
		delete mpssdi;
	}

	mpssd_log(PINFO, "All cards threads have been stopped");

	m_map.clear();
}

void
mpssd_manager::start_authentication_threads()
{
	m_monitor_thread = std::thread(&mpssd_manager::monitor_thread, this);
	m_coi_authenticate_thread = std::thread(&mpssd_manager::coi_authenticate_thread, this);
}

void
mpssd_manager::stop_authentication_threads()
{
	m_monitor_thread.join();
	m_coi_authenticate_thread.join();
}
