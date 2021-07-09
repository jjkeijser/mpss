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

#include <cstring>
#include <map>

#include "daemonlog.h"
#include "handler/SetRequestHandler.hpp"
#include "I2cAccess.hpp"
#include "ScifEp.hpp"
#include "SystoolsdException.hpp"

#include <scif.h>

namespace
{
    std::map<uint16_t, uint8_t> req_to_cmd = {
        {SET_PWM_ADDER, 0x4b},
        {SET_LED_BLINK, 0x60}
    };
}

SetRequestHandler::SetRequestHandler(struct SystoolsdReq &req, DaemonSession::ptr sess, Daemon &owner, I2cBase *i2c_) :
    RequestHandlerBase(req, sess, owner), i2c(i2c_)
{
    if(!i2c)
        throw std::invalid_argument("NULL i2c");
}

SetRequestHandler::~SetRequestHandler()
{

}

void SetRequestHandler::handle_request()
{
    union U
    {
        char buf[4];
        uint32_t sum;
    };

    U *u = (U*)req.data;
    uint32_t to_write = u->sum;
    uint8_t smc_cmd = 0;

    //check if req_type is supported
    try
    {
        smc_cmd = req_to_cmd.at(req.req_type);
    }
    catch(std::out_of_range &ofr)
    {
        log(DEBUG, "unsupported 'set' request type %u", req.req_type);
        reply_error(SYSTOOLSD_UNSUPPORTED_REQ);
        return;
    }

    //now, check if request is coming from a root port
    if(!is_from_root())
    {
        reply_error(SYSTOOLSD_INSUFFICIENT_PRIVILEGES);
        return;
    }

    //perform write operation
    try
    {
        i2c->write_u32(smc_cmd, to_write);
    }
    catch(SystoolsdException &excp)
    {
        log(ERROR, excp, "SetRequestHandler::handle_request() : %s", excp.what());
        reply_error(excp.error_nr());
        return;
    }

    try
    {
        req.card_errno = 0;
        sess->get_client()->send((char*)&req, sizeof(req));
    }
    catch(SystoolsdException &excp)
    {
        log(ERROR, excp, "error sending ack reponse for req %u", req.req_type);
        sess->get_client()->close();
    }
}
