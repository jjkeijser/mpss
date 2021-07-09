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

#ifndef _SYSTOOLS_SYSTOOLSD_SETREQUESTHANDLER_HPP_
#define _SYSTOOLS_SYSTOOLSD_SETREQUESTHANDLER_HPP_

#include "RequestHandlerBase.hpp"

class I2cBase;

class SetRequestHandler : public RequestHandlerBase
{
public:
    SetRequestHandler(struct SystoolsdReq &req, DaemonSession::ptr sess, Daemon &owner, I2cBase *i2c);
    virtual ~SetRequestHandler();
    virtual void handle_request();

private:
    I2cBase *i2c;
};

#endif //_SYSTOOLS_SYSTOOLSD_SETREQUESTHANDLER_HPP_
