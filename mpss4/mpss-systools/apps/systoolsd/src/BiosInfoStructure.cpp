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

#include "smbios/BiosInfoStructure.hpp"
#include "smbios/SmBiosTypes.hpp"

namespace
{
const uint8_t EXPECTED_SIZE = 0x12;
}

BiosInfoStructure::BiosInfoStructure() : SmBiosStructureBase()
{
}

BiosInfoStructure::BiosInfoStructure(const s_BiosInfoStructure &bios_struct_, const std::string &vendor_,
                  const std::string &bios_version_, const std::string &bios_release_date_) :
    bios_struct(bios_struct_), vendor(vendor_), bios_version(bios_version_),
    bios_release_date(bios_release_date_)
{
}

void BiosInfoStructure::validate_and_build(SmBiosStructHeader &hdr, const uint8_t *const buf, size_t len)
{
    check_size(hdr, EXPECTED_SIZE);

    if(len < sizeof(s_BiosInfoStructure))
        throw std::invalid_argument("buffer len must be > sizeof(s_BiosInfoStructure");

    memcpy(&bios_struct, buf, sizeof(s_BiosInfoStructure));

    std::vector<std::string> strings = get_strings(hdr, buf, len);

    //strings are numbered from 1..n in structure
    set_string(vendor, strings, bios_struct.vendor);
    set_string(bios_version, strings, bios_struct.bios_version);
    set_string(bios_release_date, strings, bios_struct.bios_release_date);
}

const s_BiosInfoStructure &BiosInfoStructure::get_bios_struct() const
{
    return bios_struct;
}

const std::string &BiosInfoStructure::get_vendor() const
{
    return vendor;
}

const std::string &BiosInfoStructure::get_bios_version() const
{
    return bios_version;
}

const std::string &BiosInfoStructure::get_bios_release_date() const
{
    return bios_release_date;
}

