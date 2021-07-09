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

#ifndef _SYSTOOLS_SYSTOOLSD_DAEMONSESSION_HPP_
#define _SYSTOOLS_SYSTOOLSD_DAEMONSESSION_HPP_

#include <mutex>
#include <string>

#include "ScifEp.hpp"

class DaemonSession
{
public:
    DaemonSession(ScifEp::ptr client);
    ScifEp::ptr get_client() const;
    void set_request_in_progress(bool in_progress);
    bool is_request_in_progress() const;

public:
    typedef std::shared_ptr<DaemonSession> ptr;


private:
    ScifEp::ptr client;
    bool in_progress;
    mutable std::mutex rip_mutex;

private:
    DaemonSession();
    DaemonSession(const DaemonSession&);
    DaemonSession &operator=(const DaemonSession&);
};

#endif //SYSTOOLS_SYSTOOLSD_DAEMONSESSION_HPP_
