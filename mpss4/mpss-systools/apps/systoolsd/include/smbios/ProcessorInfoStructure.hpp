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

#ifndef _SYSTOOLS_SYSTOOLSD_PROCESSORINFOSTRUCTURE_HPP_
#define _SYSTOOLS_SYSTOOLSD_PROCESSORINFOSTRUCTURE_HPP_

#include "SmBiosStructureBase.hpp"

struct SmBiosStructHeader;

//Don't allow compiler to pad SMBIOS structs
#pragma pack(push)
#pragma pack(1)
struct s_ProcessorInfoStructure
{
    struct SmBiosStructHeader hdr;
    uint8_t socket_designation; //string
    uint8_t processor_type;
    uint8_t processor_family;
    uint8_t processor_manufacturer; //string
    uint64_t processor_id;
    uint8_t processor_version; //string
    uint8_t voltage;
    uint16_t external_clock;
    uint16_t max_speed;
    uint16_t current_speed; //note: current at boot time!
    uint8_t status;
    uint8_t processor_upgrade;
    uint16_t l1_cache_handle;
    uint16_t l2_cache_handle;
    uint16_t l3_cache_handle;
    uint8_t serial_number; //string
    uint8_t asset_tag; //string
    uint8_t part_number; //string
    uint8_t core_count;
    uint8_t core_enabled;
    uint8_t thread_count;
    uint16_t processor_characteristics;
    //more unneeded fields follow
};
#pragma pack(pop)

class ProcessorInfoStructure : public SmBiosStructureBase
{
public:
    ProcessorInfoStructure();

    //this constructor is meant to be used by FakeSmBiosInfo.
    //For regular implementations, use the default constructor
    //and call validate_and_build() to populate the object's data.
    ProcessorInfoStructure(const s_ProcessorInfoStructure &procstruct, const std::string &socket,
            const std::string &manufacturer, const std::string &version, const std::string &serial,
            const std::string &asset, const std::string &part);

    virtual void validate_and_build(SmBiosStructHeader &hdr, const uint8_t *const buf, size_t len);
    const s_ProcessorInfoStructure &get_processor_struct() const;
    const std::string &get_socket_designation() const;
    const std::string &get_manufacturer() const;
    const std::string &get_processor_version() const;
    const std::string &get_serial_number() const;
    const std::string &get_asset_tag() const;
    const std::string &get_part_number() const;

private:
    s_ProcessorInfoStructure processor_struct;
    std::string socket_designation;
    std::string processor_manufacturer;
    std::string processor_version;
    std::string serial_number;
    std::string asset_tag;
    std::string part_number;
};

#endif //_SYSTOOLS_SYSTOOLSD_PROCESSORINFOSTRUCTURE_HPP_
