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

#ifndef SYSTOOLS_SYSTOOLSD_PTHRESHSETREQUESTHANDLER_HPP
#define SYSTOOLS_SYSTOOLSD_PTHRESHSETREQUESTHANDLER_HPP

#include "RequestHandlerBase.hpp"

class PThreshInterface;

class PThreshRequestHandler : public RequestHandlerBase
{
public:
    PThreshRequestHandler(struct SystoolsdReq &req, DaemonSession::ptr sess, Daemon &owner, PThreshInterface *pthresh);
    virtual ~PThreshRequestHandler();
    virtual void handle_request();

private:
    PThreshInterface *pthresh;
};

#endif
