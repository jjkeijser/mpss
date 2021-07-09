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

#include "Daemon.hpp"
#include "handler/PThreshRequestHandler.hpp"
#include "info/DataGroupInterface.hpp"
#include "PThresh.hpp"
#include "SystoolsdException.hpp"

#include "daemonlog.h"

PThreshRequestHandler::PThreshRequestHandler(struct SystoolsdReq &req, DaemonSession::ptr sess, Daemon &owner, PThreshInterface *pthresh_):
    RequestHandlerBase(req, sess, owner), pthresh(pthresh_)
{
    if(!pthresh)
        throw std::invalid_argument("NULL pthresh");
}

PThreshRequestHandler::~PThreshRequestHandler()
{

}

/*
 * These requests have the following process:
 *      - A PWindow object will be created based on the request information
 *      - A SystoolsdReq with card_errno=0 will be sent back, indicating that
 *        the client may proceed sending a PowerWindowInfo (an ACK).
 *      - A PowerWindowInfo is received from the client. Check the number of
 *        bytes. If bytes_received != sizeof(PowerWindowInfo), close the
 *        connection.
 *      - Attempt to set the values in the PowerWindowInfo using the
 *        PWindow object.
 *      - If all goes well, send the final SystoolsdReq header with
 *        card_errno=0.
 * */
void PThreshRequestHandler::handle_request()
{
    if(!is_from_root())
    {
        reply_error(SYSTOOLSD_INSUFFICIENT_PRIVILEGES);
        return;
    }

    std::shared_ptr<PWindowInterface> pwindow;

    try
    {
        //get corresponding window and acknowledge request
        pwindow = pthresh->window_factory(req.req_type == SET_PTHRESH_W0 ? 0 : 1);
        req.card_errno = 0;
        sess->get_client()->send((char*)&req, sizeof(req));
    }
    catch(SystoolsdException &excp)
    {
        log(ERROR, excp, "error sending ack response for power threshold req %u: %s",
                req.req_type, excp.what());
        reply_error(excp);
        return;
    }

    //recv a PowerWindowInfo from client
    PowerWindowInfo window_info;
    bzero(&window_info, sizeof(window_info));
    try
    {
        int bytes_recv = sess->get_client()->recv((char*)&window_info, sizeof(window_info));
        if(bytes_recv != sizeof(window_info))
        {
            //protocol mismatch
            log(WARNING, "expecting %u bytes for PowerWindowInfo, received %d bytes",
                    sizeof(window_info), bytes_recv);
            sess->get_client()->close();
            return;
        }
        //set window values
        if(window_info.threshold != std::numeric_limits<uint32_t>::max())
            pwindow->set_pthresh(window_info.threshold);
        if(window_info.time_window != std::numeric_limits<uint32_t>::max())
            pwindow->set_time_window(window_info.time_window);
    }
    catch(SystoolsdException &excp)
    {
        log(ERROR, excp);
        reply_error(excp);
        return;
    }

    //force refresh on power thresholds info group
    owner.get_data_groups().at(GET_PTHRESH_INFO)->force_refresh();
    req.card_errno = 0;
    sess->get_client()->send((char*)&req, sizeof(req));

}
