/*
 * Copyright (c) 2017, Intel Corporation.
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

#ifndef _SYSTOOLS_SYSTOOLSD_DAEMON_HPP_
#define _SYSTOOLS_SYSTOOLSD_DAEMON_HPP_

#include <atomic>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

#include "info/CachedDataGroupBase.hpp"
#include "handler/RequestHandlerBase.hpp"
#include "DaemonSession.hpp"
#include "ScifEp.hpp"
#include "smbios/SmBiosInfo.hpp"
#include "SystoolsdServices.hpp"
#include "ThreadPool.hpp"
#include "TurboSettings.hpp"

#include "systoolsd_api.h"

#ifdef UNIT_TESTS
#define PRIVATE public
#else
#define PRIVATE private
#endif

class ScifEpInterface;
class I2cBase;
class PThreshInterface;
class KernelInterface;
class SyscfgInterface;

class Daemon
{
public:
    friend class RequestHandlerBase;
    Daemon(ScifEp::ptr ep, Services::ptr services);
    ~Daemon();
    void start();
    void serve_forever();
    void stop();
    const std::map<uint16_t, DataGroupInterface*> &get_data_groups() const;
    static uint8_t max_connections;
    static int scif_port;

PRIVATE:
    //ctors for UTs
    Daemon(ScifEp::ptr ep, Services::ptr services, const std::map<uint16_t, DataGroupInterface*>& data_groups);
    Daemon(ScifEp::ptr ep);
    void listen_for_connections();
    void remove_session(DaemonSession::ptr sess);
    void add_session(DaemonSession::ptr sess);
    void remove_invalid_sessions(int fd = 0);
    std::vector<ScifEp::ptr> get_scif_peers();
    void init_scif(bool reinit=false);
    bool flush_client(ScifEp::ptr &client);
    void notify_error(ScifEp::ptr &client, SystoolsdReq &req, SystoolsdError error);
    void acquire_request();
    void release_request();
    static void handle_signals(int s);
    void wait_for_clients();
    void notify_about_clients();

    //Hide
    Daemon(const Daemon &d);
    Daemon &operator=(const Daemon &d);

PRIVATE:
    std::mutex              m_sessions_mutex;
    std::mutex              m_request_count_mutex;
    std::mutex              m_mutex_ready;
    std::condition_variable m_clients_ready_cv;
    std::thread             m_sessions_thread;

    std::map<int, DaemonSession::ptr>       m_sessions;
    std::map<uint16_t, DataGroupInterface*> m_data_groups;

    ScifEp::ptr                             m_scif;
    std::unique_ptr<micmgmt::ThreadPool>    m_request_workers;
    std::unique_ptr<Services>               m_services;

    std::atomic<bool> m_shutdown;


    uint8_t     m_request_count;
    uint64_t    m_total_requests;
    bool        m_clients_ready;
};

#endif
