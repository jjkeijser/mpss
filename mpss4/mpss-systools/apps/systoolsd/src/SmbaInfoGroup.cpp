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
#include "I2cAccess.hpp"
#include "info/SmbaInfoGroup.hpp"

SmbaInfoGroup::SmbaInfoGroup(Services::ptr &services) :
    CachedDataGroupBase<struct SmbaInfo>(100), i2c(NULL)
{
    if(!(i2c = services->get_i2c_srv()))
        throw std::invalid_argument("NULL DaemonInfoSources->i2c");
}

void SmbaInfoGroup::refresh_data()
{
    auto busy_info = i2c->is_device_busy();
    if(busy_info.is_busy)
    {
        data.is_busy = 1;
        //SMBA_RESTART_WAIT_MS defined in shared/systoolsd_api.h
        data.ms_remaining = SMBA_RESTART_WAIT_MS - busy_info.elapsed_busy;
    }
    else
    {
        data.is_busy = 0;
        data.ms_remaining = 0;
    }
}

const struct SmbaInfo &SmbaInfoGroup::get_data(bool force_refresh)
{
    //Don't let compiler complain about unused arguments...
    (void)force_refresh;
    CachedDataGroupBase<struct SmbaInfo>::get_data(true);
    return data;
}

void *SmbaInfoGroup::get_raw_data(bool force_refresh)
{
    //Don't let compiler complain about unused arguments...
    (void)force_refresh;
    CachedDataGroupBase<struct SmbaInfo>::get_data(true);
    return static_cast<void*>(&data);
}
