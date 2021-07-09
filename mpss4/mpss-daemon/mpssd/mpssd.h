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

#pragma once

#include "mpsstransfer.h"
#include "sync_utils.h"
#include "virtio.h"

#include <mpssconfig.h>
#include <libmpsscommon.h>
#include <mic_ioctl.h>
#include <scif.h>

#include <functional>
#include <list>
#include <map>
#include <mutex>
#include <pthread.h>
#include <stdlib.h>
#include <string>


enum class mpssd_error {
	NONE,
	INVALID_CONFIG,
	NO_DEVICE,
	BOOT_FAILED,
	RESET_FAILED,
	SHUTDOWN_FAILED,
	VIRTIO_INIT_FAILED
};

enum class mpssd_state {
	MIC_ERROR = MIC_STATE_ERROR,
	MIC_READY = MIC_STATE_READY,
	MIC_ONLINE = MIC_STATE_ONLINE,
	MIC_ONLINE_FIRMWARE = MIC_STATE_ONLINE_FIRMWARE,
	MIC_SHUTDOWN = MIC_STATE_SHUTDOWN,
	MIC_RESETTING = MIC_STATE_RESETTING,
	MIC_SHUTTING_DOWN = MIC_STATE_SHUTTING_DOWN,
	MIC_BOOTING = MIC_STATE_BOOTING,
	MIC_BOOTING_FIRMWARE = MIC_STATE_BOOTING_FIRMWARE,
	CTRL_THREAD_INIT,
	CTRL_THREAD_POST_INITIAL_STATE,
	CTRL_THREAD_SHUTDOWN
};

struct jobs {
	unsigned int	jobid;
	int		cnt;
	scif_epd_t	dep;
};

const char* mpssd_error_to_string(mpssd_error error);
const char* mpssd_state_to_string(mpssd_state state);

class mpssd_info;

typedef std::function<void(mpssd_info*)> mpssd_callback;

class mpssd_info {
	std::map<mpssd_state, std::list<mpssd_callback>> m_cbs;

	mic_info* m_mic;
	mic_device_context* m_mdc;

	std::thread m_control_thread;
	std::thread m_card_mon_thread;

	bool m_autobooted;

	mpssd_error m_error;

	shutdown_status m_status;

	bool m_ready_to_notify_systemd;

	scif_epd_t	m_recv_ep;
	scif_epd_t	m_send_ep;

public:
	mpssd_info(mic_info* mic);
	~mpssd_info();

	struct mic_console_info	mic_console;
	struct mic_net_info	mic_net;
	struct mic_virtblk_info	mic_virtblk[BLOCK_MAX_COUNT];

private:
	void add_state_cb(mpssd_state state, mpssd_callback callback);
	void execute_state_cbs(mpssd_state state);
	void execute_state_cbs(int mic_state);

	void set_error(mpssd_error error);
	void save_crash_log();
	void check_root_fs();

	void boot_invoker();
	void reset_invoker();
	void shutdown_invoker();

	void reload_config();
	void start_virtio_threads();
	void stop_virtio_threads();
	void shutdown_handler();
	void ready_handler();
	void error_handler();

	int get_state_id(int fd);

	bool stop_condition(int mic_state);
	void control_thread();
	void card_monitor_thread();

	void set_readiness_to_notify();
	void check_autoboot();
	void clear_execute_on_ready();

public:
	std::string name();
	int id();

	void start_control_thread();
	void start_monitor_thread();
	void request_stop_control_thread();
	void stop_control_thread();
	bool is_ready_to_notify_systemd();

	int establish_connection(scif_epd_t recv_ep, struct scif_portID recvID);
	int send_coi_authentication_data_to_card(unsigned int jobid,
		char *username, char *cookie);
	void close_scif_connection();
};

class mpssd_manager {
	std::map<std::string, mpssd_info*> m_map;
	shutdown_status m_status;

	std::thread m_monitor_thread;
	std::thread m_coi_authenticate_thread;

	unsigned int m_nextjobid;
	std::mutex m_job_mutex;
	std::map<unsigned int, jobs*> m_job;

	struct passwd* find_passwd_entry(uid_t uid);
public:
	mpssd_manager();

	bool add(mic_info* mic);
	mpssd_info *get_card_by_id(int micid);

	void request_stop();
	void wait_for_stop_request();
	void stop();
	void wait_and_notify_systemd();
	void start_authentication_threads();
	void stop_authentication_threads();
	void close_authentication_job(unsigned int jobid);

	void monitor_thread();
	void coi_authenticate_thread();
};

extern mpssd_manager g_manager;

struct mpssd_virtio_log {
	int virtio_device_type;
	int virtio_device_number;
};

extern struct mic_info *miclist;
