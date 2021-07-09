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
#include "I2cBase.hpp"

#include "handler/SmcRwHandler.hpp"

SmcRwHandler::SmcRwHandler(struct SystoolsdReq &req, DaemonSession::ptr sess, Daemon &owner, I2cBase *i2c_) : RequestHandlerBase(req, sess, owner), i2c(i2c_)
{
    if(!i2c)
        throw std::invalid_argument("NULL i2c");

}

void SmcRwHandler::handle_request()
{
    if(!is_from_root())
    {
        log(INFO, "arbitrary read/write SMC operation denied");
        reply_error(SYSTOOLSD_INSUFFICIENT_PRIVILEGES);
        return;
    }

    // Validate fields of req struct
    if(req.length <= 0 || req.length > SYSTOOLSDREQ_MAX_DATA_LENGTH)
    {
        log(DEBUG, "invalid length for arbitrary read/write SMC operation");
        reply_error(SYSTOOLSD_INVAL_STRUCT);
        return;
    }

    try
    {

        if(req.req_type == READ_SMC_REG)
        {
            bzero(req.data, SYSTOOLSDREQ_MAX_DATA_LENGTH);
            i2c->read_bytes(req.extra, req.data, req.length);
        }
        else if(req.req_type == WRITE_SMC_REG)
        {
            i2c->write_bytes(req.extra, req.data, req.length);
        }
        else
        {
            log(ERROR, "unknown read/write SMC operation");
            throw SystoolsdException(SYSTOOLSD_INTERNAL_ERROR, "unknown req type");
        }

    }
    catch(const SystoolsdException &excp)
    {
        log(WARNING, excp, "SMC op failed: %s", excp.what());
        reply_error(excp);
    }

    reply_error(0);

}
