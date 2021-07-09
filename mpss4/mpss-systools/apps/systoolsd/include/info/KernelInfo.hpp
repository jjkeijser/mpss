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

#ifndef _SYSTOOLS_SYSTOOLSD_KERNELINFO_HPP_
#define _SYSTOOLS_SYSTOOLSD_KERNELINFO_HPP_

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "systoolsd_api.h"

class KernelInterface
{
public:
    virtual ~KernelInterface() { }
    virtual uint32_t get_logical_core_count() const = 0;
    virtual uint32_t get_physical_core_count() const = 0;
    virtual uint16_t get_threads_per_core() const = 0;
    virtual uint32_t get_cpu_frequency() const = 0;
    virtual const std::vector<CoreCounters> &get_physical_core_usage(CoreCounters *aggregate,
            uint64_t *ticks=NULL) const = 0;
    virtual const std::vector<CoreCounters> &get_logical_core_usage(CoreCounters *aggregate,
            uint64_t *ticks=NULL) const = 0;
};

class KernelInfo : public KernelInterface
{
public: //functions
    uint32_t get_logical_core_count() const;
    uint32_t get_physical_core_count() const;
    uint16_t get_threads_per_core() const;
    uint32_t get_cpu_frequency() const;
    const std::vector<CoreCounters> &get_physical_core_usage(CoreCounters *aggregate,
            uint64_t *ticks=NULL) const;
    const std::vector<CoreCounters> &get_logical_core_usage(CoreCounters *aggregate,
            uint64_t *ticks=NULL) const;

    static std::map<uint16_t, std::vector<uint16_t>> map_physical_to_logical_cores();

public: //helper types

    //alias for map of vectors
    //this map will contain:
    //  key : physical core id
    //  value : vector of logical cores mapped to the physical core
    typedef std::map<uint16_t, std::vector<uint16_t>> core_mapping;

private: //helper types
    //alias for pairs to represent core_id.
    //core_id is a tuple of (processor socket, core id)
    typedef std::pair<uint16_t, uint16_t> core_id;

    //a more complex version of core_mapping type.
    //This map will contain:
    //  key : pair comprisedn of (processor socket, core id)
    //  value : vector of logical cores mapped to physical cores
    typedef std::map<std::pair<uint16_t, uint16_t>, std::vector<uint16_t>> p_core_mapping;

private: //static member
    static core_mapping mapping;
    static std::vector<CoreCounters> logical_core_usage;
    static std::vector<CoreCounters> physical_core_usage;

private:
    static void p_get_logical_core_usage(CoreCounters *aggregate,
            std::vector<CoreCounters> &lusage, uint64_t *ticks);
    static void p_get_physical_core_usage(CoreCounters *aggregate,
            std::vector<CoreCounters> &pusage, uint64_t *ticks);
    static const std::map<uint16_t, std::vector<uint16_t>> &get_physical_core_mapping();
};


#endif //_SYSTOOLS_SYSTOOLSD_KERNELINFO_HPP_
