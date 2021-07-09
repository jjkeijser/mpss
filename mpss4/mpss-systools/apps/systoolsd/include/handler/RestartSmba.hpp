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

#ifndef SYSTOOLS_SYSTOOLSD_RESTARTSMBA_HPP
#define SYSTOOLS_SYSTOOLSD_RESTARTSMBA_HPP

#include "RequestHandlerBase.hpp"

#ifdef UNIT_TESTS
#define PRIVATE public
#else
#define PRIVATE private
#endif

// Note about RestartSmba:
// This class must do its work in construction time. This is because
// we can't have this type of operation running from within a thread pool.
// During construction, the class will keep track of failures in member error.
// The state saved in member error will be notified to clients in the call
// to handle_request(), usually performed when running in the thread pool.
class RestartSmba : public RequestHandlerBase
{
public:
    RestartSmba(struct SystoolsdReq &req, DaemonSession::ptr sess, Daemon &owner, I2cBase *i2c);
    virtual ~RestartSmba();
    virtual void handle_request();

PRIVATE:
    uint16_t error;

private:
    RestartSmba();
    I2cBase *i2c;
};

#endif //SYSTOOLS_SYSTOOLSD_RESTARTSMBA_HPP
