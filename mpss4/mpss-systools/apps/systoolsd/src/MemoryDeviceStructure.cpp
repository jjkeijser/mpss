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
#include <string>
#include <vector>

#include "smbios/MemoryDeviceStructure.hpp"

namespace
{
    const uint8_t EXPECTED_SIZE = 0x15;
}

MemoryDeviceStructure::MemoryDeviceStructure() : units(MemUnit::kilo)
{
}

MemoryDeviceStructure::MemoryDeviceStructure(const s_MemoryDeviceStructure &mem, const std::string &dev_locator,
        const std::string &bank_locator_, const std::string &manuf, const std::string &serial,
        const std::string &asset, const std::string &part) :
    memory_device(mem), device_locator(dev_locator), bank_locator(bank_locator_),
    manufacturer(manuf), serial_number(serial), asset_tag(asset), part_number(part),
    units(MemUnit::mega)
{
}

void MemoryDeviceStructure::validate_and_build(SmBiosStructHeader &hdr, const uint8_t *const buf, size_t len)
{
    check_size(hdr, EXPECTED_SIZE);

    if(len < sizeof(s_MemoryDeviceStructure))
    {
        throw std::invalid_argument("buffer length must be > sizeof(s_MemoryDeviceStructure)");
    }

    memcpy(&memory_device, buf, sizeof(s_MemoryDeviceStructure));

    //from DMTF spec: if size value is 0 no memory device is installed; if size is 0xffff
    // it is unknown; the most significant bit specifies the units: if 0 it is megabytes,
    // if 1 it is kilobytes.
    if(memory_device.size == 0 || memory_device.size == 0xFFFF)
    {
        memory_device.size = 0;
        units = MemUnit::unknown;
    }
    else
    {
        auto is_kb = memory_device.size & 0x8000;
        units = (is_kb ? MemUnit::kilo : MemUnit::mega);
    }


    std::vector<std::string> strings = get_strings(hdr, buf, len);

    set_string(device_locator, strings, memory_device.device_locator);
    set_string(bank_locator, strings, memory_device.bank_locator);
    set_string(manufacturer, strings, memory_device.manufacturer);
    set_string(serial_number, strings, memory_device.serial_number);
    set_string(asset_tag, strings, memory_device.asset_tag);
    set_string(part_number, strings, memory_device.part_number);
}

const s_MemoryDeviceStructure &MemoryDeviceStructure::get_memory_device_struct() const
{
    return memory_device;
}

const std::string &MemoryDeviceStructure::get_device_locator() const
{
    return device_locator;
}

const std::string &MemoryDeviceStructure::get_bank_locator() const
{
    return bank_locator;
}

const std::string &MemoryDeviceStructure::get_manufacturer() const
{
    return manufacturer;
}

const std::string &MemoryDeviceStructure::get_serial_number() const
{
    return serial_number;
}

const std::string &MemoryDeviceStructure::get_asset_tag() const
{
    return asset_tag;
}

const std::string &MemoryDeviceStructure::get_part_number() const
{
    return part_number;
}

uint64_t MemoryDeviceStructure::get_memsize_base() const
{
    return memory_device.size & 0x7FFF;
}

MemUnit MemoryDeviceStructure::memsize_unit() const
{
    return units;
}
