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

#include <mutex>

#include "DaemonSession.hpp"

DaemonSession::DaemonSession(ScifEp::ptr client_) : client(client_), in_progress(false)
{
}

ScifEp::ptr DaemonSession::get_client() const
{
    return client;
}

void DaemonSession::set_request_in_progress(bool in_progress_)
{
    std::lock_guard<std::mutex> l(rip_mutex);
    in_progress = in_progress_;
}

bool DaemonSession::is_request_in_progress() const
{
    std::lock_guard<std::mutex> l(rip_mutex);
    return in_progress;
}
