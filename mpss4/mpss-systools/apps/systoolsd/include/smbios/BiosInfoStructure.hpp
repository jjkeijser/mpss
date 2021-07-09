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

#include "smbios/SmBiosStructureBase.hpp"

struct SmBiosStructHeader;

//Don't allow compiler to pad SMBIOS structs
#pragma pack(push)
#pragma pack(1)
struct s_BiosInfoStructure
{
    struct SmBiosStructHeader hdr;
    uint8_t vendor; //string
    uint8_t bios_version; //string
    uint16_t bios_starting_address_segment;
    uint8_t bios_release_date; //string
    uint8_t bios_rom_size; //size(n) where 64K * (n+1) is the size of physical device containing the BIOS, in bytes
    uint64_t bios_characteristics;
    //more values defined in standard that we won't need
};
#pragma pack(pop)

class BiosInfoStructure : public SmBiosStructureBase
{
public:
    BiosInfoStructure();

    //this constructor is meant to be used by FakeSmBiosInfo.
    //For regular implementations, use the default constructor
    //and call validate_and_build() to populate the object's data.
    BiosInfoStructure(const s_BiosInfoStructure &bios_struct_, const std::string &vendor_,
                      const std::string &bios_version_, const std::string &bios_release_date_);

    virtual void validate_and_build(SmBiosStructHeader &hdr, const uint8_t *const buf, size_t len);
    const s_BiosInfoStructure &get_bios_struct() const;
    const std::string &get_vendor() const;
    const std::string &get_bios_version() const;
    const std::string &get_bios_release_date() const;

    typedef std::shared_ptr<BiosInfoStructure> ptr;

private:
    s_BiosInfoStructure bios_struct;
    std::string vendor;
    std::string bios_version;
    std::string bios_release_date;
};
