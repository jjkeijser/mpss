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

#ifndef _SYSTOOLS_SYSTOOLSD_REQUESTHANDLER_HPP_
#define SYSTOOLS_SYSTOOLSD_REQUESTHANDLER_HPP_

#include "handler/RequestHandlerBase.hpp"
#include "info/DataGroupInterface.hpp"
#include "ScifEp.hpp"

#include "systoolsd_api.h"

#ifdef UNIT_TESTS
#define PROTECTED public
#else
#define PROTECTED protected
#endif

class SafeBool;
class Daemon;

class RequestHandler : public RequestHandlerBase
{
public:
    RequestHandler(struct SystoolsdReq &req, DaemonSession::ptr sess, Daemon &owner);
    virtual ~RequestHandler();
    virtual void handle_request();

PROTECTED:
    void service_request();
};

#endif //SYSTOOLS_SYSTOOLSD_REQUESTHANDLER_HPP_
