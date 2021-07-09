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
#include "info/PowerThresholdsInfoGroup.hpp"
#include "PThresh.hpp"

PowerThresholdsInfoGroup::PowerThresholdsInfoGroup(Services::ptr &services) :
    CachedDataGroupBase<struct PowerThresholdsInfo>(300), pthresh(NULL)
{
    if(!(pthresh = services->get_pthresh_srv()))
        throw std::invalid_argument("NULL DaemonInfoSources->i2c");
}

void PowerThresholdsInfoGroup::refresh_data()
{
    data.max_phys_power = pthresh->get_max_phys_pwr();
    data.low_threshold = pthresh->get_low_pthresh();
    data.hi_threshold = pthresh->get_high_pthresh();
    data.w0 = pthresh->window_factory(0)->get_window_info();
    data.w1 = pthresh->window_factory(1)->get_window_info();
}
