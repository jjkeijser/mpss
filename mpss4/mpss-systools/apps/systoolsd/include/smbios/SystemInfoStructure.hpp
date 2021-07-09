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

#ifndef _SYSTOOLS_SYSTOOLSD_SYSTEMINFO_HPP_
#define _SYSTOOLS_SYSTOOLSD_SYSTEMINFO_HPP_

#include <cstdint>
#include <string>

#include "SmBiosStructureBase.hpp"

//Don't allow compiler to pad SMBIOS structs
#pragma pack(push)
#pragma pack(1)
struct s_SystemInfoStructure
{
    SmBiosStructHeader hdr;
    uint8_t manufacturer; //string
    uint8_t product_name; //string
    uint8_t version; //string
    uint8_t serial_number; //string
    uint8_t uuid[16];
    uint8_t wake_up_type;
    uint8_t sku_number; //string
    uint8_t family; //string
};
#pragma pack(pop)

class SystemInfoStructure : public SmBiosStructureBase
{
public:
    SystemInfoStructure();

    //this constructor is meant to be used by FakeSmBiosInfo.
    //For regular implementations, use the default constructor
    //and call validate_and_build() to populate the object's data.
    SystemInfoStructure(const s_SystemInfoStructure &system_info_, const std::string &manufacturer_,
        const std::string &product_name_, const std::string &version_,
        const std::string &serial_number_, const std::string &sku_number_,
        const std::string &family_);

    virtual void validate_and_build(SmBiosStructHeader &hdr, const uint8_t *const buf, size_t len);
    const s_SystemInfoStructure &get_system_info_struct() const;
    const std::string &get_manufacturer() const;
    const std::string &get_product_name() const;
    const std::string &get_version() const;
    const std::string &get_serial_number() const;
    const std::string &get_sku_number() const;
    const std::string &get_family() const;

private:
    s_SystemInfoStructure system_info;
    std::string manufacturer;
    std::string product_name;
    std::string version;
    std::string serial_number;
    std::string sku_number;
    std::string family;
};

#endif //_SYSTOOLS_SYSTOOLSD_SYSTEMINFO_HPP_
