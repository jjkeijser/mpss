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

#ifndef _SYSTOOLS_SYSTOOLSD_MEMORYDEVICESTRUCTURE_HPP_
#define _SYSTOOLS_SYSTOOLSD_MEMORYDEVICESTRUCTURE_HPP_

#include <cstdint>

#include "SmBiosStructureBase.hpp"

enum MemUnit
{
    kilo,
    mega,
    unknown
};

//Don't allow compiler to pad SMBIOS structs
#pragma pack(push)
#pragma pack(1)
struct s_MemoryDeviceStructure
{
    SmBiosStructHeader hdr;
    uint16_t physical_memory_array_handle;
    uint16_t memory_error_information_handle;
    uint16_t total_width;
    uint16_t data_width;
    uint16_t size; //if 0, no device installed in the socket
    uint8_t form_factor;
    uint8_t device_set;
    uint8_t device_locator; //string
    uint8_t bank_locator; //string
    uint8_t memory_type;
    uint16_t type_detail;
    uint16_t speed; //in MHz. If 0, unknown
    uint8_t manufacturer; //string
    uint8_t serial_number; //string
    uint8_t asset_tag; //string
    uint8_t part_number; //string
    uint8_t attributes;
    uint32_t extended_size;
    uint16_t clock_speed; //in MHz. If 0, unkown.
    uint16_t min_voltage; //millivolts
    uint16_t max_voltage;
};
#pragma pack(pop)

class MemoryDeviceStructure : public SmBiosStructureBase
{
public:
    MemoryDeviceStructure();

    //this constructor is meant to be used by FakeSmBiosInfo.
    //For regular implementations, use the default constructor
    //and call validate_and_build() to populate the object's data.
    MemoryDeviceStructure(const s_MemoryDeviceStructure &mem, const std::string &dev_locator,
            const std::string &bank_locator, const std::string &manuf, const std::string &serial,
            const std::string &asset, const std::string &part);

    virtual void validate_and_build(SmBiosStructHeader &hdr, const uint8_t *const buf, size_t len);
    const s_MemoryDeviceStructure &get_memory_device_struct() const;
    const std::string &get_device_locator() const;
    const std::string &get_bank_locator() const;
    const std::string &get_manufacturer() const;
    const std::string &get_serial_number() const;
    const std::string &get_asset_tag() const;
    const std::string &get_part_number() const;
    uint64_t get_memsize_base() const;
    MemUnit memsize_unit() const;

private:
    s_MemoryDeviceStructure memory_device;
    std::string device_locator;
    std::string bank_locator;
    std::string manufacturer;
    std::string serial_number;
    std::string asset_tag;
    std::string part_number;
    MemUnit units;

};


#endif //_SYSTOOLS_SYSTOOLSD_MEMORYDEVICESTRUCTURE_HPP_
