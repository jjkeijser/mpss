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

#include "smbios/ProcessorInfoStructure.hpp"
#include "smbios/SmBiosTypes.hpp"

namespace
{
const uint8_t EXPECTED_SIZE = 0x12;
}

ProcessorInfoStructure::ProcessorInfoStructure() : SmBiosStructureBase()
{
}

ProcessorInfoStructure::ProcessorInfoStructure(const s_ProcessorInfoStructure &procstruct,
        const std::string &socket, const std::string &manufacturer,
        const std::string &version, const std::string &serial, const std::string &asset,
        const std::string &part) :
    processor_struct(procstruct), socket_designation(socket), processor_manufacturer(manufacturer),
    processor_version(version), serial_number(serial), asset_tag(asset), part_number(part)
{
}


void ProcessorInfoStructure::validate_and_build(SmBiosStructHeader &hdr, const uint8_t *const buf, size_t len)
{
    check_size(hdr, EXPECTED_SIZE);

    if(len < sizeof(s_ProcessorInfoStructure))
        throw std::invalid_argument("buffer len must be > sizeof(s_ProcessorInfoStructure");

    memcpy(&processor_struct, buf, sizeof(s_ProcessorInfoStructure));

    std::vector<std::string> strings = get_strings(hdr, buf, len);

    //strings are numbered from 1..n in structure
    set_string(socket_designation, strings, processor_struct.socket_designation);
    set_string(processor_manufacturer, strings, processor_struct.processor_manufacturer);
    set_string(processor_version, strings, processor_struct.processor_version);
    set_string(serial_number, strings, processor_struct.serial_number);
    set_string(asset_tag, strings, processor_struct.asset_tag);
    set_string(part_number, strings, processor_struct.part_number);
}

const s_ProcessorInfoStructure &ProcessorInfoStructure::get_processor_struct() const
{
    return processor_struct;
}

const std::string &ProcessorInfoStructure::get_socket_designation() const
{
    return socket_designation;
}

const std::string &ProcessorInfoStructure::get_manufacturer() const
{
    return processor_manufacturer;
}

const std::string &ProcessorInfoStructure::get_processor_version() const
{
    return processor_version;
}

const std::string &ProcessorInfoStructure::get_serial_number() const
{
    return serial_number;
}

const std::string &ProcessorInfoStructure::get_asset_tag() const
{
    return asset_tag;
}

const std::string &ProcessorInfoStructure::get_part_number() const
{
    return part_number;
}
