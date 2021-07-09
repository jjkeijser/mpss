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

#include <cstring>
#include <fstream>
#include <ios>
#include <set>
#include <sstream>
#include <stdexcept>

#include <unistd.h>

#include "info/KernelInfo.hpp"
#include "info/ProcStatCore.hpp"
#include "SystoolsdException.hpp"
#include "utils.hpp"

#include "systoolsd_api.h"

namespace
{
    const char *const procstat_path = "/proc/stat";
    const char *const cpuinfo_path = "/proc/cpuinfo";
}

KernelInfo::core_mapping KernelInfo::mapping = KernelInfo::map_physical_to_logical_cores();
std::vector<CoreCounters> KernelInfo::logical_core_usage(sysconf(_SC_NPROCESSORS_ONLN));
std::vector<CoreCounters> KernelInfo::physical_core_usage(KernelInfo::get_physical_core_mapping().size());

uint32_t KernelInfo::get_logical_core_count() const
{
    return (uint32_t)sysconf(_SC_NPROCESSORS_ONLN);
}

uint32_t KernelInfo::get_physical_core_count() const
{
    return get_physical_core_mapping().size();
}

uint16_t KernelInfo::get_threads_per_core() const
{
    return get_logical_core_count() / get_physical_core_count();
}

uint32_t KernelInfo::get_cpu_frequency() const
{
    std::ifstream cpuinfo(cpuinfo_path, std::ifstream::in);
    if(!cpuinfo.good())
    {
        throw SystoolsdException(SYSTOOLSD_IO_ERROR, cpuinfo_path);
    }

    const char cpu_mhz_tok[] = "cpu MHz";
    double sum_freq = 0;
    uint16_t freqs_count = 0;

    std::string line;
    std::stringstream ss;
    const size_t size = 128;

    //cat /proc/cpuinfo | grep MHz | awk '{sum += $4; count++} END {print sum/count}'
    while(std::getline(cpuinfo, line))
    {
        if(line.find(cpu_mhz_tok) != std::string::npos)
        {
            double current_freq;
            ss.clear();
            ss.str(line);
            ss.ignore(size, ':');
            ss >> current_freq;
            sum_freq += current_freq;
            freqs_count ++;
        }
    }
    cpuinfo.close();

    return uint32_t(sum_freq / freqs_count); //MHz
}

const KernelInfo::core_mapping &KernelInfo::get_physical_core_mapping()
{
    //use static var, avoid calling map_physical_to_logical_cores() many times
    return mapping;
}

const std::vector<CoreCounters> &KernelInfo::get_physical_core_usage(CoreCounters *aggregate,
        uint64_t *ticks) const
{
    p_get_physical_core_usage(aggregate, physical_core_usage, ticks);
    return physical_core_usage;
}

const std::vector<CoreCounters> &KernelInfo::get_logical_core_usage(CoreCounters *aggregate,
        uint64_t *ticks) const
{
    p_get_logical_core_usage(aggregate, logical_core_usage, ticks);
    return logical_core_usage;
}

KernelInfo::core_mapping KernelInfo::map_physical_to_logical_cores()
{
    KernelInfo::p_core_mapping core_ids;

    std::ifstream cpuinfo(cpuinfo_path, std::ifstream::in);
    if(!cpuinfo.good())
    {
        throw SystoolsdException(SYSTOOLSD_IO_ERROR, cpuinfo_path);
    }

    const char physical_id_tok[] = "physical id";
    const char core_id_tok[] = "core id";
    const char processor_tok[] = "processor";
    uint8_t state = 0;
    uint16_t ph_id = 0;
    uint16_t c_id = 0;
    uint16_t current_lprocessor = 0;

    //get unique pairs of (physical_id, core_id) to identify actual
    //physical cores
    std::string line;
    std::stringstream ss;
    const size_t size = 128;
    while(std::getline(cpuinfo, line))
    {
        switch(state)
        {
            case 0: //read until "processor", save in current_lprocessor, go to state 1
                if(line.find(processor_tok) != std::string::npos)
                {
                    ss.clear(); ss.str(line);
                    ss.ignore(size, ':');
                    ss >> current_lprocessor;
                    state = 1;
                }
                break;
            case 1: //read until "physical id", save the value, go to state 2
                if(line.find(physical_id_tok) != std::string::npos)
                {
                    ss.clear(); ss.str(line);
                    ss.ignore(size, ':');
                    ss >> ph_id;
                    state = 2;
                }
                break;

            case 2: //read until "core id", save the value, go to state 2
                if(line.find(core_id_tok) != std::string::npos)
                {
                    ss.clear(); ss.str(line);
                    ss.ignore(size, ':');
                    ss >> c_id;
                    std::pair<uint16_t, uint16_t> p(ph_id, c_id);
                    auto found = core_ids.insert(KernelInfo::p_core_mapping::value_type(p, std::vector<uint16_t>()));
                    found.first->second.push_back(current_lprocessor);
                    state = 0;
                }
                break;
        }
    }
    cpuinfo.close();

    //Create a core_mapping from p_core_mapping
    core_mapping cmapping;
    core_mapping::size_type i = 0;
    for(auto c = core_ids.begin(); c != core_ids.end(); ++c, ++i)
    {
        cmapping[i] = c->second;
    }

    if(!cmapping.size())
        throw SystoolsdException(SYSTOOLSD_INTERNAL_ERROR, "error creating cores mapping");

    return cmapping;
}

void KernelInfo::p_get_logical_core_usage(CoreCounters *aggregate,
        std::vector<CoreCounters> &lusage, uint64_t *ticks)
{
    if(!aggregate)
        throw std::invalid_argument("NULL CoreCounters");

    std::ifstream procstat(procstat_path);
    if(!procstat)
    {
        throw SystoolsdException(SYSTOOLSD_IO_ERROR, procstat_path);
    }
    procstat.clear();

    ProcStatCoreBase::ptr stat_core = ProcStatCoreBase::get_procstat_core();

    std::string aggregate_line;
    std::getline(procstat, aggregate_line);
    stat_core->reset(aggregate_line);
    aggregate->user = stat_core->user();
    aggregate->nice = stat_core->nice();
    aggregate->system = stat_core->system();
    aggregate->idle = stat_core->idle();
    aggregate->total = stat_core->total();

    if(ticks)
    {
        *ticks = stat_core->total();
    }

    for(auto cusage = lusage.begin(); cusage != lusage.end() && procstat.good(); ++cusage)
    {
        std::string stat_line;
        std::getline(procstat, stat_line);
        stat_core->reset(stat_line);

        cusage->user = stat_core->user();
        cusage->nice = stat_core->nice();
        cusage->system = stat_core->system();
        cusage->idle = stat_core->idle();
        cusage->total = stat_core->total();
    }

    procstat.close();
}

void KernelInfo::p_get_physical_core_usage(CoreCounters *aggregate,
        std::vector<CoreCounters> &pusage, uint64_t *ticks)
{
    //update logical core usage
    p_get_logical_core_usage(aggregate, logical_core_usage, ticks);

    //for each physical core, using its mapping to logical cores
    //get and sum the usage values
    core_mapping::size_type m = 0;
    for(auto cusage = pusage.begin(); cusage != pusage.end(); ++cusage, ++m)
    {
        //reset values
        cusage->user = 0;
        cusage->nice = 0;
        cusage->system = 0;
        cusage->idle = 0;
        cusage->total = 0;

        //get a list of logical cores to access for this physical core
        std::vector<uint16_t> lcore_ids = mapping.at(m);
        //and now access each logical core
        for(auto i = lcore_ids.begin(); i != lcore_ids.end(); ++i)
        {
            CoreCounters &this_logical_core = logical_core_usage.at(*i);
            cusage->user += this_logical_core.user;
            cusage->nice += this_logical_core.nice;
            cusage->system += this_logical_core.system;
            cusage->idle += this_logical_core.idle;
            cusage->total += this_logical_core.total;
        }
    }
}
