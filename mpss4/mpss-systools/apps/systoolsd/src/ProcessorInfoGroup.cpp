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

#include "Daemon.hpp"
#include "info/ProcessorInfoGroup.hpp"
#include "smbios/ProcessorInfoStructure.hpp"
#include "SystoolsdException.hpp"
#include "SystoolsdServices.hpp"
#include "utils.hpp"

#include "systoolsd_api.h"

ProcessorInfoGroup::ProcessorInfoGroup(Services::ptr &services) :
    CachedDataGroupBase<ProcessorInfo>(0), smbios(NULL)
{
    if(!(smbios = services->get_smbios_srv()))
        throw std::invalid_argument("NULL DaemonInfoSources->smbios");
}

void ProcessorInfoGroup::refresh_data()
{
    auto processors = SmBiosInfo::cast_to<ProcessorInfoStructure>(smbios->get_structures_of_type(PROCESSOR_INFO));
    //Assuming all processor instances will be the same family and type
    const ProcessorInfoStructure *proc = processors.at(0);

    //get stepping id
    const std::string cpuinfo("/proc/cpuinfo");
    std::ifstream ifs(cpuinfo, std::ifstream::in);
    if(!ifs.good())
    {
        throw SystoolsdException(SYSTOOLSD_IO_ERROR, cpuinfo.c_str());
    }

    int32_t stepping_id = -1;
    uint16_t model = 0;
    const size_t size = 64;
    char buf[size] = {0};
    while(ifs.good())
    {
        ifs.getline(buf, size);
        if(strstr(buf, "stepping"))
        {
            stepping_id = (int32_t)utils::get_u64_from_line(buf);
            continue;
        }
        if(strstr(buf, "model name"))
        {
            //skip model name
            continue;
        }
        if(strstr(buf, "model"))
        {
            model = utils::get_u64_from_line(buf);
            continue;
        }
    }

    ifs.close();

    if(stepping_id == -1)
    {
        throw SystoolsdException(SYSTOOLSD_INTERNAL_ERROR, "failed getting stepping_id");
    }

    data.stepping_id = (uint32_t)stepping_id;
    data.model = model;

    data.family = proc->get_processor_struct().processor_family;
    data.type = proc->get_processor_struct().processor_type;

    //TODO: remove this. The MicDevice SDK retrieves stepping string
    // from host-side sysfs.
    const char stepping[] = "unknown";
    strncpy(data.stepping, stepping, strlen(stepping));

    //get threads per core
    uint16_t enabled_cores = 0;
    uint16_t thread_count = 0;

    //smbios reports enabled cores per processor socket and threads per
    //processor socket. Threads per core will be (threads / cores)
    for(auto i = processors.begin(); i != processors.end(); ++i)
    {
        auto p = *i;
        enabled_cores += p->get_processor_struct().core_enabled;
        thread_count += p->get_processor_struct().thread_count;
    }

    data.threads_per_core = thread_count / enabled_cores;
}

