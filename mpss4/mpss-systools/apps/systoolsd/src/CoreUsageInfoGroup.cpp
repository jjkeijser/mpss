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

#include <ctime>
#include <fstream>
#include <vector>

#include <unistd.h>

#include "Daemon.hpp"
#include "info/KernelInfo.hpp"
#include "info/CoreUsageInfoGroup.hpp"
#include "utils.hpp"

CoreUsageInfoGroup::CoreUsageInfoGroup(Services::ptr &services) :
    CachedDataGroupBase<std::vector<CoreCounters>>(900), usage_info(), raw_buf(NULL),
    kernel(0)
{
    if(!(kernel = services->get_kernel_srv()))
        throw std::invalid_argument("NULL DaemonInfoSources->kernel");

    auto core_count = kernel->get_physical_core_count();
    auto threads_per_core = kernel->get_threads_per_core();
    data.resize(core_count * threads_per_core);
    raw_buf = new char[(sizeof(CoreCounters) * core_count * threads_per_core) + sizeof(CoreUsageInfo)];
    usage_info.num_cores = core_count;
    usage_info.clocks_per_sec = sysconf(_SC_CLK_TCK);
    usage_info.threads_per_core = threads_per_core;
}

CoreUsageInfoGroup::~CoreUsageInfoGroup()
{
    if(raw_buf)
    {
        delete [] raw_buf;
        raw_buf = NULL;
    }
}

void CoreUsageInfoGroup::refresh_data()
{
    //update core counters, get system-wide ticks in usage_info.ticks
    CoreCounters sum;
    data = kernel->get_logical_core_usage(&sum, &usage_info.ticks);
    bzero(&usage_info.sum, sizeof(usage_info.sum));
    //update system-wide counters
    usage_info.sum.system = sum.system;
    usage_info.sum.user = sum.user;
    usage_info.sum.nice = sum.nice;
    usage_info.sum.idle = sum.idle;
    usage_info.sum.total = sum.total;
    //update CPU frequency
    usage_info.frequency = kernel->get_cpu_frequency();
    //copy to raw_buf
    memcpy(raw_buf, &usage_info, sizeof(usage_info));
    memcpy(raw_buf + sizeof(usage_info), &data[0], sizeof(CoreCounters) * data.size());
}

size_t CoreUsageInfoGroup::get_size()
{
    //all the per-core counters, plus the CoreUsageInfo struct
    return (sizeof(CoreCounters) * data.size()) + sizeof(CoreUsageInfo);
}

void CoreUsageInfoGroup::copy_data_to(char *buf, size_t *size)
{
    if(!buf || !size)
        throw std::invalid_argument("NULL buf or size");

    if(*size == 0)
        throw std::invalid_argument("size 0");

    (void)CachedDataGroupBase<std::vector<CoreCounters>>::get_data();

    memcpy(buf, raw_buf, *size);

    *size = get_size();
    return;
}

CoreCounters *CoreUsageInfoGroup::get_data(bool force_refresh)
{
    // Force refresh of CoreUsageInfoGroup
    (void)force_refresh;
    (void)CachedDataGroupBase<std::vector<CoreCounters>>::get_data(true);
    return &data[0];
}

void *CoreUsageInfoGroup::get_raw_data(bool force_refresh)
{
    // Force refresh of CoreUsageInfoGroup
    (void)force_refresh;
    (void)CachedDataGroupBase<std::vector<CoreCounters>>::get_data(true);
    return (void*)raw_buf;
}
