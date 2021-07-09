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

#include "Daemon.hpp"
#include "info/MemoryInfoGroup.hpp"
#include "smbios/MemoryDeviceStructure.hpp"
#include "smbios/SmBiosInfo.hpp"
#include "Syscfg.hpp"
#include "SystoolsdException.hpp"

#include "systoolsd_api.h"

MemoryInfoGroup::MemoryInfoGroup(Services::ptr &services) :
    CachedDataGroupBase<MemoryInfo>(0), smbios(NULL)
{
    if(!(smbios = services->get_smbios_srv()))
        throw std::invalid_argument("NULL DaemonInfoSources->smbios");
    if(!(syscfg = services->get_syscfg_srv()))
        throw std::invalid_argument("NULL DaemonInfoSources->syscfg");
}

void MemoryInfoGroup::refresh_data()
{
    auto devices = SmBiosInfo::cast_to<MemoryDeviceStructure>(smbios->get_structures_of_type(MEMORY_DEVICE));

    data.total_size = 0;
    for(auto i = devices.begin(); i != devices.end(); ++i)
    {
        auto device = *i;
        auto size = device->get_memsize_base();
        switch(device->memsize_unit())
        {
            case MemUnit::kilo:
                size = size / 1024;
                break;
            case MemUnit::mega:
                break;
            case MemUnit::unknown:
                continue;
            default:
                //will not happen
                continue;
        }
        data.total_size += size;
    }

    //iterate over the list again and grab other details from
    //the first memory device installed in the socket
    const MemoryDeviceStructure *device = NULL;
    for(auto i_device = devices.begin(); i_device != devices.end(); ++i_device)
    {
        device = *i_device;
        //if size is 0, no memory is installed in the socket
        if(device->get_memory_device_struct().size)
            break;
        else
            device = NULL;
    }

    if(!device)
    {
        throw SystoolsdException(SYSTOOLSD_IO_ERROR, "no memory devices detected");
    }

    data.speed = device->get_memory_device_struct().speed;
    data.type = device->get_memory_device_struct().memory_type;
    data.frequency = device->get_memory_device_struct().clock_speed;
    data.ecc_enabled = syscfg->ecc_enabled() ? 1 : 0;
    strncpy(data.manufacturer, device->get_manufacturer().c_str(), sizeof(data.manufacturer) - 1);
}

