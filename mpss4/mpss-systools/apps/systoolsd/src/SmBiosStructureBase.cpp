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

#include <memory>
#include <sstream>
#include <stdexcept>

#include "smbios/BiosInfoStructure.hpp"
#include "smbios/MemoryDeviceStructure.hpp"
#include "smbios/ProcessorInfoStructure.hpp"
#include "smbios/SmBiosStructureBase.hpp"
#include "smbios/SystemInfoStructure.hpp"
#include "SystoolsdException.hpp"

#include "systoolsd_api.h"

std::shared_ptr<SmBiosStructureBase> SmBiosStructureBase::create_structure(SmBiosStructureType type)
{
    switch(type)
    {
        case SmBiosStructureType::BIOS:
            return SmBiosStructureBase::ptr(std::make_shared<BiosInfoStructure>());
        case SmBiosStructureType::SYSTEM_INFO:
            return SmBiosStructureBase::ptr(std::make_shared<SystemInfoStructure>());
        case SmBiosStructureType::PROCESSOR_INFO:
            return SmBiosStructureBase::ptr(std::make_shared<ProcessorInfoStructure>());
        case SmBiosStructureType::MEMORY_DEVICE:
            return SmBiosStructureBase::ptr(std::make_shared<MemoryDeviceStructure>());
        default:
            throw std::invalid_argument("unsupported SMBIOS structure type");
    }
}

/*
 * get_strings()
 * Returns a vector of strings from the SMBIOS structure pointed to by parameter buf. Note that
 *    buf must point to the beginning of where paramenter hdr resides.
 */
std::vector<std::string> SmBiosStructureBase::get_strings(SmBiosStructHeader &hdr, const uint8_t *const buf, size_t len)
{
    if(!buf || len == 0)
    {
        throw std::invalid_argument("buffer NULL/0-size buffer");
    }

    if(! (len >= (size_t)hdr.length))
    {
        throw std::invalid_argument("buffer length must be >= hdr.length");
    }
    uint8_t *strings_begin_at = (uint8_t*)buf + hdr.length;
    std::vector<std::string> strings_vector;
    if(strings_begin_at[0] == '\0' && strings_begin_at[1] == '\0')
        return strings_vector;

    uint8_t *p = strings_begin_at;
    while(p < buf + len - 1)
    {
        std::string s((char*)p);
        strings_vector.push_back(s);
        p += s.length() + 1;
    }
    return strings_vector;
}

/*
 * set_string()
 * Sets str_to_set to string which-1 as positioned in strings vector.
 * Note: which-1 is used because SMBIOS structures specify strings starting from 1, while
 *       vectors are 0-indexed.
 */
void SmBiosStructureBase::set_string(std::string &str_to_set, std::vector<std::string> &strings, uint8_t which)
{
    try
    {
        str_to_set = strings.at(which - 1);
    }
    catch(std::out_of_range &ofr)
    {
        str_to_set = "Not specified"; //structure does not define a string for this
    }
}


void SmBiosStructureBase::check_size(SmBiosStructHeader &hdr, uint8_t expected_size)
{
    if(hdr.length < expected_size)
    {
        std::stringstream ss;
        ss << "malformed structure of type " << hdr.type;
        throw SystoolsdException(SYSTOOLSD_INTERNAL_ERROR, ss.str().c_str());
    }
}
