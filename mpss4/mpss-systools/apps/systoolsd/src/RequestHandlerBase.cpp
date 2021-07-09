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

#include "Daemon.hpp"
#include "daemonlog.h"
#include "DaemonSession.hpp"
#include "handler/MicBiosRequestHandler.hpp"
#include "handler/PThreshRequestHandler.hpp"
#include "handler/RequestHandler.hpp"
#include "handler/RequestHandlerBase.hpp"
#include "handler/RestartSmba.hpp"
#include "handler/SetRequestHandler.hpp"
#include "handler/SmcRwHandler.hpp"
#include "handler/TurboRequestHandler.hpp"
#include "ScifEp.hpp"
#include "SystoolsdException.hpp"
#include "SystoolsdServices.hpp"

#include <scif.h>

#include "systoolsd_api.h"

RequestHandlerBase::RequestHandlerBase(SystoolsdReq &req_, DaemonSession::ptr sess_, Daemon &owner_) :
    req(req_), owner(owner_), sess(sess_)
{
    //mark this request's session as request_in_progress
    sess->set_request_in_progress(true);
    owner.remove_session(sess);
}

RequestHandlerBase::~RequestHandlerBase()
{
    sess->set_request_in_progress(false);
    owner.add_session(sess);
}

void RequestHandlerBase::Run(micmgmt::SafeBool &stop)
{
    try
    {
        //notify daemon that a request is being executed
        owner.acquire_request();
        handle_request();
    }
    catch(SystoolsdException &excp)
    {
        reply_error(excp.error_nr());
        log(ERROR, excp, "error in handle_request() : %s", excp.what());
    }
    catch(...)
    {
        //attempt to notify client of error
        reply_error(SYSTOOLSD_UNKOWN_ERROR);
        log(ERROR, "unknown error in RequestHandlerBase::Run");
    }

    owner.release_request();
    (void)stop;
    return;
}

RequestHandlerBase::ptr RequestHandlerBase::create_request(SystoolsdReq &req, DaemonSession::ptr sess, Daemon &owner, Services::ptr &services)
{
    uint16_t req_type = req.req_type;
    switch(req_type)
    {
        case READ_SMC_REG:
        case WRITE_SMC_REG:
            return RequestHandlerBase::ptr(new SmcRwHandler(req, sess, owner, services->get_i2c_srv()));
        case SET_PTHRESH_W0:
        case SET_PTHRESH_W1:
            return RequestHandlerBase::ptr(new PThreshRequestHandler(req, sess, owner, services->get_pthresh_srv()));
        case RESTART_SMBA:
            //when RESTART_SMBA, wait for owner's thread pool to become empty
            log(INFO, "waiting for thread pool to become empty to start SMBus retraining...");
            owner.m_request_workers->wait();
            log(INFO, "thread pool empty");
            //RestartSmba does most of its work during construction
            return RequestHandlerBase::ptr(new RestartSmba(req, sess, owner, services->get_i2c_srv()));
        case SET_TURBO:
            return RequestHandlerBase::ptr(new TurboRequestHandler(req, sess, owner, services->get_turbo_srv()));
        case MICBIOS_REQUEST:
            return RequestHandlerBase::ptr(new MicBiosRequestHandler(req, sess, owner, services->get_syscfg_srv()));
        default:
            //fallthrough
            break;
    }

    //if request is a generic "set" request
    if(req_type & SET_REQUEST_MASK)
    {
        return RequestHandlerBase::ptr(new SetRequestHandler(req, sess, owner, services->get_i2c_srv()));
    }

    //default to generic request handler
    return RequestHandlerBase::ptr(new RequestHandler(req, sess, owner));
}

void RequestHandlerBase::reply_error(uint16_t card_errno)
{
    try
    {
        req.card_errno = card_errno;
        sess->get_client()->send((char*)&req, sizeof(req));
    }
    catch(SystoolsdException &excp)
    {
        log(ERROR, excp, "failed replying error %u : %s", card_errno, excp.what());
        sess->get_client()->close();
    }
    catch(...)
    {
        log(ERROR, "unknown error in RequestHandlerBase::reply_error");
        sess->get_client()->close();
    }
}

void RequestHandlerBase::reply_error(const SystoolsdException &excp)
{
    try
    {
        req.card_errno = excp.error_nr();
        sess->get_client()->send((char*)&req, sizeof(req));
    }
    catch(SystoolsdException &excp)
    {
        log(ERROR, excp, "failed replying error %u : %s", excp.error_nr(), excp.what());
        sess->get_client()->close();
    }
    catch(...)
    {
        log(ERROR, "unknown error in RequestHandlerBase::reply_error");
        sess->get_client()->close();
    }
}

bool RequestHandlerBase::is_from_root()
{
    ScifEp::scif_port_id client_id = sess->get_client()->get_port_id();
    return (client_id.second >= SCIF_ADMIN_PORT_END ? false : true);
}
