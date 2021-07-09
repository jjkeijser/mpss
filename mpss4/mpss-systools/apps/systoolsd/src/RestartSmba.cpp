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
#include "handler/RestartSmba.hpp"
#include "I2cAccess.hpp"
#include "SystoolsdException.hpp"

RestartSmba::RestartSmba(struct SystoolsdReq &req, DaemonSession::ptr sess, Daemon &owner, I2cBase *i2c_) : RequestHandlerBase(req, sess, owner), error(SYSTOOLSD_UNKOWN_ERROR), i2c(i2c_)
{
    if(!i2c)
    {
        throw std::invalid_argument("NULL i2c");
    }

    try{
        if(!is_from_root())
        {
            log(INFO, "SMB retraining denied");
            error = SYSTOOLSD_INSUFFICIENT_PRIVILEGES;
            return;
        }

        //check if restart is in progress
        if(i2c->is_device_busy().is_busy)
        {
            log(INFO, "restart is in progress");
            error = SYSTOOLSD_RESTART_IN_PROGRESS;
            return;
        }
        uint8_t *addr = (uint8_t*)&req.data[0];
        log(INFO, "resetting SMBus address to 0x%x", *addr);
        i2c->restart_device(*addr);
        log(INFO, "SMB retraining started...");
        error = 0;
    }
    catch(SystoolsdException &excp)
    {
        log(ERROR, excp, "RestartSmba: %s", excp.what());
        error = excp.error_nr();
    }
    catch(...)
    {
        error = SYSTOOLSD_UNKOWN_ERROR;
    }
    return;
}

RestartSmba::~RestartSmba()
{

}

void RestartSmba::handle_request()
{
    reply_error(error);
}
