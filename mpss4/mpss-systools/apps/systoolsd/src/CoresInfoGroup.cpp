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

#include <unistd.h>

#include "Daemon.hpp"
#include "info/CoresInfoGroup.hpp"
#include "info/KernelInfo.hpp"
#include "SystoolsdException.hpp"
#include "smbios/ProcessorInfoStructure.hpp"
#include "smbios/SmBiosInfo.hpp"
#include "utils.hpp"

#include "systoolsd_api.h"

using std::string;
using std::ifstream;


CoresInfoGroup::CoresInfoGroup(Services::ptr &services) :
CachedDataGroupBase<CoresInfo>(0), kernel(0), smbios(0)
{
    if(!(kernel = services->get_kernel_srv()))
        throw std::invalid_argument("NULL DaemonInfoSources->kernel");

    if(!(smbios = services->get_smbios_srv()))
        throw std::invalid_argument("NULL DaemonInfoSources->smbios");
}

void CoresInfoGroup::refresh_data()
{
    data.num_cores = kernel->get_physical_core_count();
    data.clocks_per_sec = sysconf(_SC_CLK_TCK);
    data.threads_per_core = kernel->get_threads_per_core();

    auto proc = SmBiosInfo::cast_to<ProcessorInfoStructure>(smbios->get_structures_of_type(PROCESSOR_INFO)).at(0);
    data.cores_voltage = proc->get_processor_struct().voltage;
    data.cores_freq = proc->get_processor_struct().current_speed;
}

