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
#include "I2cAccess.hpp"
#include "info/DeviceInfoGroup.hpp"
#include "Popen.hpp"
#include "PopenInterface.hpp"
#include "smbios/SmBiosInfo.hpp"
#include "smbios/BiosInfoStructure.hpp"
#include "smbios/SystemInfoStructure.hpp"
#include "SystoolsdException.hpp"

#include "systoolsd_api.h"

using std::string;
using std::ifstream;

namespace
{
    const uint8_t part_number = 0x18;
    const uint8_t manufacture_date = 0x19;
    const uint8_t serialno = 0x15;
    const uint8_t card_tdp = 0x1E;
    const uint8_t fwu_cap = 0xE0;
    const uint8_t cpu_id = 0x1C;
    const uint8_t pci_smba = 0x07;
    const uint8_t fw_version = 0x11;
    const uint8_t exe_domain = 0x12;
    const uint8_t sts_selftest = 0x13;
    const uint8_t boot_fw_version = 0x16;
    const uint8_t hw_revision = 0x14;
}

DeviceInfoGroup::DeviceInfoGroup(Services::ptr &services) :
    CachedDataGroupBase<DeviceInfo>(0), i2c(NULL), smbios(NULL),
    popen(new Popen)
{
    if(!(i2c = services->get_i2c_srv()))
        throw std::invalid_argument("NULL DaemonInfoSources->i2c");

    if(!(smbios = services->get_smbios_srv()))
        throw std::invalid_argument("NULL DaemonInfoSources->smbios");
}

void DeviceInfoGroup::refresh_data()
{
    data.card_tdp = i2c->read_u32(card_tdp) & 0xFFFF;
    data.fwu_cap = i2c->read_u32(fwu_cap);
    data.cpu_id = i2c->read_u32(cpu_id);
    data.pci_smba = i2c->read_u32(pci_smba);
    data.fw_version = i2c->read_u32(fw_version);
    data.exe_domain = i2c->read_u32(exe_domain);
    data.sts_selftest = i2c->read_u32(sts_selftest);
    data.boot_fw_version = i2c->read_u32(boot_fw_version);
    data.hw_revision = i2c->read_u32(hw_revision);

    i2c->read_bytes(manufacture_date, data.manufacture_date, sizeof(data.manufacture_date));
    i2c->read_bytes(part_number, data.part_number, sizeof(data.part_number));
    i2c->read_bytes(serialno, data.serialno, sizeof(data.serialno));

    auto bios_info = SmBiosInfo::cast_to<BiosInfoStructure>(smbios->get_structures_of_type(BIOS)).at(0);
    auto sysinfo = SmBiosInfo::cast_to<SystemInfoStructure>(smbios->get_structures_of_type(SYSTEM_INFO)).at(0);

    //copy sizeof(data.field) -1, to keep null terminators
    strncpy(data.bios_version, bios_info->get_bios_version().c_str(), sizeof(data.bios_version) - 1);
    strncpy(data.bios_release_date, bios_info->get_bios_release_date().c_str(), sizeof(data.bios_release_date) - 1);
    //the following fields will not be null-terminated. Clients receiving this struct must take care
    // of these fields accordingly.
    strncpy(data.uuid, (char*)sysinfo->get_system_info_struct().uuid, sizeof(data.uuid)); //watch no -1 here.

    popen->run("uname -r -o");
    if(popen->get_retcode())
    {
        throw SystoolsdException(SYSTOOLSD_IO_ERROR, "popen failed");
    }
    std::string os_version = popen->get_output();
    if(os_version.empty())
    {
        throw SystoolsdException(SYSTOOLSD_IO_ERROR, "failed getting OS version");
    }
    //os_version comes from popen(), so it has a trailing newline char.
    //Remove it.
    os_version.erase(os_version.size() - 1);
    strncpy(data.os_version, os_version.c_str(), sizeof(data.os_version) - 1);
}

void DeviceInfoGroup::set_popen(PopenInterface *new_popen)
{
    popen.reset(new_popen);
}

