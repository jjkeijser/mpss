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
#include <stdexcept>

#include "smbios/SmBiosStructureBase.hpp"
#include "smbios/SystemInfoStructure.hpp"

namespace
{
const uint8_t EXPECTED_SIZE = 0x1b;
}

SystemInfoStructure::SystemInfoStructure() : SmBiosStructureBase()
{
}

SystemInfoStructure::SystemInfoStructure(const s_SystemInfoStructure &system_info_, const std::string &manufacturer_,
        const std::string &product_name_, const std::string &version_,
        const std::string &serial_number_, const std::string &sku_number_,
        const std::string &family_) :
    system_info(system_info_), manufacturer(manufacturer_), product_name(product_name_),
    version(version_), serial_number(serial_number_), sku_number(sku_number_),
    family(family_)
{
}


void SystemInfoStructure::validate_and_build(SmBiosStructHeader &hdr, const uint8_t *const buf, size_t len)
{
    check_size(hdr, EXPECTED_SIZE);
    if(len < sizeof(s_SystemInfoStructure))
    {
        throw std::invalid_argument("buffer length must be >= sizeof(s_SystemInfoStructure)");
    }

    memcpy(&system_info, buf, sizeof(s_SystemInfoStructure));

    std::vector<std::string> strings = get_strings(hdr, buf, len);

    set_string(manufacturer, strings, system_info.manufacturer);
    set_string(product_name, strings, system_info.product_name);
    set_string(version, strings, system_info.version);
    set_string(serial_number, strings, system_info.serial_number);
    set_string(sku_number, strings, system_info.sku_number);
    set_string(family, strings, system_info.family);
}

const s_SystemInfoStructure &SystemInfoStructure::get_system_info_struct() const
{
    return system_info;
}

const std::string &SystemInfoStructure::get_manufacturer() const
{
    return manufacturer;
}

const std::string &SystemInfoStructure::get_product_name() const
{
    return product_name;
}

const std::string &SystemInfoStructure::get_version() const
{
    return version;
}

const std::string &SystemInfoStructure::get_serial_number() const
{
    return serial_number;
}

const std::string &SystemInfoStructure::get_sku_number() const
{
    return sku_number;
}

const std::string &SystemInfoStructure::get_family() const
{
    return family;
}

