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

#ifndef _SYSTOOLS_SYSTOOLSD_SMBIOSTYPES_HPP_
#define _SYSTOOLS_SYSTOOLSD_SMBIOSTYPES_HPP_

#include <cstdint>

#pragma pack(push)
#pragma pack(1)
struct SmBiosEntryPointStructure
{
    char anchor[4]; //_SM_
    uint8_t eps_checksum;
    uint8_t eps_length;
    uint8_t smbios_major;
    uint8_t smbios_minor;
    uint16_t max_struct_size;
    uint8_t ep_rev;
    char formatted_area[5];
    char inter_anchor[5]; //_DMI_
    char inter_checksum;
    uint16_t struct_table_length;
    uint32_t struct_table_address;
    uint16_t total_structs;
    uint8_t bcd_rev;
};

struct SmBiosStructHeader
{
    uint8_t type;
    uint8_t length;
    uint16_t handle;
};
#pragma pack(pop)

enum SmBiosStructureType {
    BIOS=0,
    SYSTEM_INFO=1,
    PROCESSOR_INFO=4,
    MEMORY_DEVICE=17
};

#endif //_SYSTOOLS_SYSTOOLSD_SMBIOSTYPES_HPP_

