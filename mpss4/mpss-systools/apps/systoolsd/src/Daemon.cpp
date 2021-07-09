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

#include <algorithm>
#include <condition_variable>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

#include <signal.h>
#include <unistd.h>

#include "info/CoresInfoGroup.hpp"
#include "info/CoreUsageInfoGroup.hpp"
#include "Daemon.hpp"
#include "daemonlog.h"
#include "handler/RequestHandler.hpp"
#include "handler/RequestHandlerBase.hpp"
#include "info/DeviceInfoGroup.hpp"
#include "info/DiagnosticsInfoGroup.hpp"
#include "info/FwUpdateInfoGroup.hpp"
#include "info/KernelInfo.hpp"
#include "info/MemoryInfoGroup.hpp"
#include "info/MemoryUsageInfoGroup.hpp"
#include "info/PowerThresholdsInfoGroup.hpp"
#include "info/PowerUsageInfoGroup.hpp"
#include "info/ProcessorInfoGroup.hpp"
#include "info/SmbaInfoGroup.hpp"
#include "info/SystoolsdInfoGroup.hpp"
#include "info/ThermalInfoGroup.hpp"
#include "info/TurboInfoGroup.hpp"
#include "info/VoltageInfoGroup.hpp"
#include "I2cAccess.hpp"
#include "PThresh.hpp"
#include "ScifImpl.hpp"
#include "Syscfg.hpp"
#include "SystoolsdServices.hpp"
#include "SystoolsdException.hpp"
#include "WorkItemInterface.hpp"

#include <scif.h>

#include "systoolsd_api.h"

#ifdef UNIT_TESTS
#include "mocks.hpp"
#endif

using micmgmt::ThreadPool;

uint8_t Daemon::max_connections = 32;
int Daemon::scif_port = SCIF_BT_PORT_0;

Daemon::Daemon(ScifEp::ptr scif, Services::ptr services) :
    m_scif(scif), m_services(std::move(services)), m_shutdown(false),
    m_request_count(0), m_total_requests(0), m_clients_ready(0)
{
    if(!m_services)
        throw std::invalid_argument("services");

    if(!m_scif)
        throw std::invalid_argument("NULL scif");

    m_request_workers = std::unique_ptr<ThreadPool>(new ThreadPool(5));


    // initialize m_data_groups
    m_data_groups =
    {
        {GET_MEMORY_UTILIZATION, new MemoryUsageInfoGroup()},
        {GET_CORES_INFO, new CoresInfoGroup(m_services)},
        {GET_CORE_USAGE, new CoreUsageInfoGroup(m_services)},
        {GET_DEVICE_INFO, new DeviceInfoGroup(m_services)},
        {GET_SMBA_INFO, new SmbaInfoGroup(m_services)},
        {GET_MEMORY_INFO, new MemoryInfoGroup(m_services)},
        {GET_PROCESSOR_INFO, new ProcessorInfoGroup(m_services)},
        {GET_POWER_USAGE, new PowerUsageInfoGroup(m_services)},
        {GET_THERMAL_INFO, new ThermalInfoGroup(m_services)},
        {GET_VOLTAGE_INFO, new VoltageInfoGroup(m_services)},
        {GET_DIAGNOSTICS_INFO, new DiagnosticsInfoGroup(m_services)},
        {GET_FWUPDATE_INFO, new FwUpdateInfoGroup(m_services)},
        {GET_PTHRESH_INFO, new PowerThresholdsInfoGroup(m_services)},
        {GET_TURBO_INFO, new TurboInfoGroup(m_services)},
        {GET_SYSTOOLSD_INFO, new SystoolsdInfoGroup()}
    };
}

Daemon::Daemon(ScifEp::ptr scif) :
    m_scif(scif), m_shutdown(false), m_request_count(0),
     m_total_requests(0), m_clients_ready(0)
{
    if(!m_scif)
        throw std::invalid_argument("NULL scif");

    m_request_workers = std::unique_ptr<ThreadPool>(new ThreadPool(5));
}

Daemon::Daemon(ScifEp::ptr scif, Services::ptr services,
               const std::map<uint16_t, DataGroupInterface*> &data_groups) :
    m_data_groups(data_groups), m_scif(scif), m_services(std::move(services)),
    m_shutdown(false), m_request_count(0), m_total_requests(0),
    m_clients_ready(0)
{
    if(!m_scif)
        throw std::invalid_argument("NULL scif");

    m_request_workers = std::unique_ptr<ThreadPool>(new ThreadPool(5));
}

Daemon::~Daemon()
{
    //TODO: use unique_ptr
    for(auto &data_group : m_data_groups)
        delete data_group.second;
}

void Daemon::start()
{
    init_scif();
}

void Daemon::stop()
{
    {
        std::lock_guard<std::mutex> l(m_mutex_ready);
        m_clients_ready = true;
        m_clients_ready_cv.notify_all();
    }

    if(m_shutdown == true)
    {
        log(DEBUG, "daemon instance has already been stopped");
        return;
    }

    m_shutdown = true;

    log(DEBUG, "waiting for worker threads...");
    m_request_workers->wait();

    log(DEBUG, "waiting for sessions thread...");
    if(m_sessions_thread.joinable())
    {
        m_sessions_thread.join();
    }

}

const std::map<uint16_t, DataGroupInterface*> &Daemon::get_data_groups() const
{
    return m_data_groups;
}

void Daemon::serve_forever()
{
    //Check if daemon has been started by checking SCIF endpoint details
    ScifEp::scif_port_id id = m_scif->get_port_id();
    //if no port is assigned, then SCIF has not been initialized
    if(id.second == 0)
    {
        throw std::logic_error("daemon instance has not been started");
    }

    //Start listening thread for new incoming connections
    m_sessions_thread = std::thread(&Daemon::listen_for_connections, this);

    //Monitor requests from clients
    log(INFO, "ready...");
    while(m_shutdown == false)
    {
        remove_invalid_sessions();
        wait_for_clients();

        std::vector<ScifEp::ptr> ready;
        try
        {
            auto peers = get_scif_peers();
            if(!peers.size())
            {
                continue;
            }
            timeval tv;
            tv.tv_usec = 0;
            tv.tv_sec = 1;
            ready = m_scif->select_read(peers, &tv);
        }
        catch(std::runtime_error &rerror)
        {
            //break from loop, daemon is shutting down
            log(WARNING, "daemon is shutting down: %s", rerror.what());
            break;
        }

        //Iterate over requests
        for(auto ready_ep = ready.begin(); ready_ep != ready.end(); ++ready_ep)
        {
            ScifEp::ptr client = *ready_ep;
            DaemonSession::ptr sess = m_sessions[client->get_epd()];
            if(sess->is_request_in_progress())
            {
                continue;
            }
            SystoolsdReq req = {0, 0, 0, 0, {0}};
            int bytes_read = 0;
            int expected_bytes = sizeof(req);
            //Read request.
            bytes_read = client->recv((char*)&req, (int)expected_bytes);
            if(bytes_read == -1)
            {
                //Something went wrong, probably client has closed the connection
                log(WARNING, "bytes_read = -1");
                remove_invalid_sessions(client->get_epd());
                continue;
            }

            /* By design, systoolsd does not support queued requests,
             * and obviously does not accept buffers larger that the request size
             */
            if(flush_client(client)) continue;

            if(bytes_read == expected_bytes)
            {
                auto handler = RequestHandlerBase::create_request(req, sess, *this, m_services);
                m_request_workers->addWorkItem(handler);
                log(DEBUG, "added request handler (type %u) to work queue", req.req_type);
            }
            else
            {
                log(DEBUG, "inval struct");
                notify_error(client, req, SYSTOOLSD_INVAL_STRUCT);
            }
        }
    }
    return;
}

void Daemon::listen_for_connections()
{
    int ret_val = -1;
    int scif_fd = m_scif->get_epd();
    int expected_ready = 1; //only one fd to watch
    while(m_shutdown == false)
    {
        scif_pollepd epd = { scif_fd, SCIF_POLLIN, 0 };
        long timeout = 1000;
        if((ret_val = m_scif->poll(&epd, expected_ready, timeout)) == -1)
        {
            //SCIF connection died, reset it
            init_scif(true);
            continue;
        }

        if(ret_val == 0) //timeout reached
            continue;

        if( (ret_val == expected_ready) && (epd.revents & SCIF_POLLIN) )
        {
            //scif endpoint received a new connection
            try{
                auto sess = std::make_shared<DaemonSession>(m_scif->accept());
                add_session(sess);
                remove_invalid_sessions();
                auto id = sess->get_client()->get_port_id();
                log(INFO, "accepted client %d:%d with epd %d", id.first,
                                                              id.second,
                                                              sess->get_client()->get_epd());
            }
            catch(std::runtime_error &rerror)
            {
                //accept() failed, "too many open clients"
                //call remove_invalid_sessions() and then continue
                //syslog error
                log(WARNING, "failed accepting: %s", rerror.what());
                remove_invalid_sessions();
                continue;
            }
        }
    }
    log(INFO, "listen_for_connections() exiting...");
}

void Daemon::add_session(DaemonSession::ptr sess)
{
    std::lock_guard<std::mutex> lock(m_sessions_mutex);
    m_sessions[sess->get_client()->get_epd()] = sess;
    notify_about_clients();
}

void Daemon::remove_session(DaemonSession::ptr sess)
{
    std::lock_guard<std::mutex> lock(m_sessions_mutex);
    m_sessions.erase(sess->get_client()->get_epd());
}

void Daemon::remove_invalid_sessions(int fd)
{
    std::lock_guard<std::mutex> lock(m_sessions_mutex);

    //if empty m_sessions map...
    if(!m_sessions.size())
    {
        std::unique_lock<std::mutex> l(m_mutex_ready);
        m_clients_ready = false;
        return;
    }

    if(fd)
    {
        m_sessions.erase(fd);
        log(DEBUG, "removed invalid session with fd: %d", fd);
        return;
    }

    //look for sessions that are broken/finished
    std::vector<scif_pollepd> fds;
    for(auto i = m_sessions.begin(); i != m_sessions.end(); ++i)
    {
        scif_pollepd f;
        f.epd = i->first;
        f.events = SCIF_POLLHUP | SCIF_POLLERR | SCIF_POLLNVAL;
        fds.push_back(f);
    }

    int ret_val = m_scif->poll(&fds[0], fds.size(), 0);
    if(ret_val < 0)
    {
        return;
    }

    for(auto f = fds.begin(); f != fds.end(); ++f)
    {
        short int errmask = SCIF_POLLHUP | SCIF_POLLERR | SCIF_POLLNVAL;
        if(f->revents & errmask)
        {
            m_sessions.erase(f->epd);
            log(DEBUG, "removed invalid session with epd: %d", f->epd);
        }
    }
}

std::vector<ScifEp::ptr> Daemon::get_scif_peers()
{
    std::lock_guard<std::mutex> lock(m_sessions_mutex);
    std::vector<ScifEp::ptr> peers;
    for(auto session = m_sessions.begin(); session != m_sessions.end(); ++session)
    {
        peers.push_back(session->second->get_client());
    }
    return peers;
}

//when reinit==true, scif endpoint will be reset
void Daemon::init_scif(bool reinit)
{
    if(reinit)
        m_scif->reset();

    try
    {
        m_scif->bind(Daemon::scif_port);
    }
    catch(SystoolsdException &excp)
    {
        throw SystoolsdException(SYSTOOLSD_SCIF_ERROR, "could not bind, port busy");
    }
    m_scif->listen(max_connections);
    log(INFO, "listening on port %d with backlog of size %d", Daemon::scif_port, max_connections);
}

bool Daemon::flush_client(ScifEp::ptr &client)
{
    char buf = '\0';
    if(client->recv(&buf, 1, false) > 0)
    {
        //protocol mistmatch, close connection
        client->close();
        log(DEBUG, "removing client with epd %d: protocol mistmatch", client->get_epd());
        return true;
    }
    return false;
}

void Daemon::notify_error(ScifEp::ptr &client, SystoolsdReq &req, SystoolsdError error)
{
    try
    {
        req.card_errno = error;
        client->send((char*)&req, sizeof(SystoolsdReq));
        remove_invalid_sessions();
        log(DEBUG, "notifying error to client with epd %d", client->get_epd());
    }
    catch(SystoolsdException &excp)
    {
        log(ERROR, excp, "failed notifying error %d to peer: %s", error, excp.what());
    }
}

void Daemon::acquire_request()
{
    std::lock_guard<std::mutex> l(m_request_count_mutex);
    if(m_request_count == 32)
    {
        throw SystoolsdException(SYSTOOLSD_TOO_BUSY, "systoolsd too busy");
    }
    m_request_count += 1;
    m_total_requests += 1;
}

void Daemon::release_request()
{
    std::lock_guard<std::mutex> l(m_request_count_mutex);
    m_request_count -= 1;
}

void Daemon::wait_for_clients()
{
    //block thread until notified about clients...
    std::unique_lock<std::mutex> l(m_mutex_ready);
    while(!m_clients_ready)
    {
        log(DEBUG, "sleeping, no active sessions...");
        m_clients_ready_cv.wait(l);
    }
}

void Daemon::notify_about_clients()
{
    //notify worker thread about the existence of new sessions
    std::lock_guard<std::mutex> l(m_mutex_ready);
    log(DEBUG, "waking up threads");
    m_clients_ready = true;
    m_clients_ready_cv.notify_one();
}
