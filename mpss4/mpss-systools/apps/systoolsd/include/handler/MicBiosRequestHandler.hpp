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

#ifndef SYSTOOLS_SYSTOOLSD_MICBIOSREQUESTHANDLER_HPP_
#define SYSTOOLS_SYSTOOLSD_MICBIOSREQUESTHANDLER_HPP_

#include "RequestHandlerBase.hpp"

class SyscfgInterface;

class MicBiosRequestHandler : public RequestHandlerBase
{
public:
    MicBiosRequestHandler(struct SystoolsdReq &req, DaemonSession::ptr sess, Daemon &owner,
                          SyscfgInterface *syscfg);
    virtual void handle_request();

private:
    MicBiosRequestHandler();
    MicBiosRequestHandler(const MicBiosRequestHandler&);
    MicBiosRequestHandler &operator=(const MicBiosRequestHandler&);

protected:
    void serve_read_request();
    void serve_write_request();
    void serve_change_password_request();

private:
    SyscfgInterface *m_syscfg;
    MicBiosRequest   m_mbreq;
};
#endif
