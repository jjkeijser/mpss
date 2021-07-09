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

#ifndef SYSTOOLS_SYSTOOLSD_TURBOREQUESTHANDLER_HPP_
#define SYSTOOLS_SYSTOOLSD_TURBOREQUESTHANDLER_HPP_

#include "RequestHandlerBase.hpp"

class TurboSettingsInterface;

class TurboRequestHandler : public RequestHandlerBase
{
public:
    TurboRequestHandler(struct SystoolsdReq &req, DaemonSession::ptr sess, Daemon &owner,
                        TurboSettingsInterface *turbo);
    virtual void handle_request();

private:
    TurboRequestHandler();
    TurboRequestHandler(const TurboRequestHandler&);
    TurboRequestHandler &operator=(const TurboRequestHandler&);

private:
    TurboSettingsInterface *turbo;
};
#endif
