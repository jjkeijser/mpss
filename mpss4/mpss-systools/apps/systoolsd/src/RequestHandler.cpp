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
#include <sstream>
#include <stdexcept>

#include "DaemonSession.hpp"
#include "Daemon.hpp"
#include "handler/RequestHandler.hpp"
#include "SystoolsdException.hpp"


#include "daemonlog.h"

RequestHandler::RequestHandler(struct SystoolsdReq &req, DaemonSession::ptr sess, Daemon &owner) :
    RequestHandlerBase(req, sess, owner)
{
}

RequestHandler::~RequestHandler()
{

}

void RequestHandler::handle_request()
{
    //If invalid request type
    auto &data_groups = owner.get_data_groups();
    if(data_groups.find(req.req_type) == data_groups.end())
    {
        //reply with error
        reply_error(SYSTOOLSD_UNSUPPORTED_REQ);
        return;
    }

    service_request();

    return;
}


void RequestHandler::service_request()
{
    DataGroupInterface *data_group = NULL;
    try
    {
        data_group = (DataGroupInterface*)owner.get_data_groups().at(req.req_type);
    }
    catch(std::out_of_range &ofr)
    {
        log(DEBUG, "unsupported 'get' request type %u", req.req_type);
        reply_error(SYSTOOLSD_UNSUPPORTED_REQ);
        return;
    }

    try
    {
        //Get data group based on request
        size_t size = data_group->get_size();
        void *data = data_group->get_raw_data();
        req.card_errno = 0;
        req.length = size;

        //Send SystoolsdReq struct back to client with updated fields
        sess->get_client()->send((char*)&req, sizeof(req));
        sess->get_client()->send((char*)data, size);
    }
    catch(SystoolsdException &excp)
    {
        log(WARNING, excp, "error serving request: %s", excp.what());
        reply_error(excp);
    }
    catch(...)
    {
        log(ERROR, "unknown error");
        reply_error(SYSTOOLSD_UNKOWN_ERROR);
    }
}

