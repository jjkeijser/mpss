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
#include "handler/TurboRequestHandler.hpp"
#include "TurboSettings.hpp"

TurboRequestHandler::TurboRequestHandler(struct SystoolsdReq &req, DaemonSession::ptr sess, Daemon &owner,
                                         TurboSettingsInterface *turbo_) :
    RequestHandlerBase(req, sess, owner), turbo(turbo_)
{
    if(!turbo)
        throw std::invalid_argument("NULL turbo");
}

void TurboRequestHandler::handle_request()
{
    if(!is_from_root())
    {
        reply_error(SYSTOOLSD_INSUFFICIENT_PRIVILEGES);
        return;
    }

    char c = req.data[0];
    //anything other than 0 will be treated as true
    bool do_enable = (c == '\0' ? false : true);

    turbo->set_turbo_enabled(do_enable);

    reply_error(0);

    //refresh turbo information after changing the settings
    owner.get_data_groups().at(GET_TURBO_INFO)->force_refresh();
}
