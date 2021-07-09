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

#ifndef _SYSTOOLS_SYSTOOLSD_REQUESTHANDLERBASE_HPP_
#define _SYSTOOLS_SYSTOOLSD_REQUESTHANDLERBASE_HPP_

#include <memory>

#include "DaemonSession.hpp"
#include "SafeBool.hpp"
#include "SystoolsdException.hpp"
#include "SystoolsdServices.hpp"
#include "WorkItemInterface.hpp"

#include "systoolsd_api.h"

#ifdef UNIT_TESTS
#define PROTECTED public
#else
#define PROTECTED protected
#endif

class Daemon;

class RequestHandlerBase : public micmgmt::WorkItemInterface
{
public:
    RequestHandlerBase(SystoolsdReq &req, DaemonSession::ptr sess, Daemon &owner);
    virtual ~RequestHandlerBase();
    typedef std::shared_ptr<RequestHandlerBase> ptr;
    //Implement WorkItemInterface::Run in base class
    //This will wrap child's handle_request() method call in order
    //to catch any unhandled exception.
    virtual void Run(micmgmt::SafeBool &stop);
    virtual void handle_request() = 0;

//static
public:
    //factory method for request handler objects
    static RequestHandlerBase::ptr create_request(SystoolsdReq &req, DaemonSession::ptr sess, Daemon &owner, Services::ptr &services);

PROTECTED:
    void reply_error(uint16_t card_errno);
    void reply_error(const SystoolsdException &excp);
    bool is_from_root();

protected:
    //keep a hard copy of req and sess, not references
    //Using references in highly multithreaded executions has presented
    //issues
    SystoolsdReq req;
    Daemon &owner;
    DaemonSession::ptr sess;
};

#endif //_SYSTOOLS_SYSTOOLSD_REQUESTHANDLERBASE_HPP_
