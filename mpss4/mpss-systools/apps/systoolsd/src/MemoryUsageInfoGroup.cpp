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

#include <fstream>
#include <stdexcept>
#include <string>

#include "info/MemoryUsageInfoGroup.hpp"
#include "utils.hpp"

using std::string;
using std::ifstream;

namespace
{
    const uint64_t timeout = 900; //900ms for MemoryUsageInfoGroup refresh
    const char meminfo_path[] = "/proc/meminfo"; //Path to /proc/meminfo system file
}

MemoryUsageInfoGroup::MemoryUsageInfoGroup() :
    CachedDataGroupBase<MemoryUsageInfo>(timeout)
{
}

// Computation borrowed from free by procps-ng-3.3.11:
// Value                /proc/meminfo
// ---------------------------------------------------------------------------
// total                MemTotal
// free                 MemFree
// buffers              Buffers
// cached               Cached + Slab
// used                 MemTotal - MemFree - Buffers - Cached - Slab

void MemoryUsageInfoGroup::refresh_data()
{
    string line;
    ifstream meminfo(meminfo_path);
    bzero(&data, sizeof(data));
    while(std::getline(meminfo, line))
    {
        if(line.find("MemTotal:") == 0)
        {
            data.total = utils::get_u64_from_line(line);
        }

        if(line.find("MemFree:") == 0)
        {
            data.free = utils::get_u64_from_line(line);
        }

        if(line.find("Buffers:") == 0)
        {
            data.buffers = utils::get_u64_from_line(line);
        }

        // data.cached = Cached + Slab, as shown in /proc/meminfo
        if(line.find("Cached:") == 0)
        {
            data.cached += utils::get_u64_from_line(line);
        }

        if(line.find("Slab:") == 0)
        {
            data.cached += utils::get_u64_from_line(line);
        }
    }

    long int mem_used = data.total - data.free - data.buffers - data.cached;

    if(mem_used < 0)
    {
        mem_used = data.total - data.free;
    }

    data.used = static_cast<uint32_t>(mem_used);
}
