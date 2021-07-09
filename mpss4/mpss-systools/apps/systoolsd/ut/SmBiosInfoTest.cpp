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

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "smbios/BiosInfoStructure.hpp"
#include "smbios/MemoryDeviceStructure.hpp"
#include "smbios/SmBiosInfo.hpp"
#include "ut_utils.hpp"

namespace
{
    const uint8_t end_of_table = 127;
    const uint8_t bios_info_size = 0x12;
    const uint8_t memory_device_size = 0x15;
}

/*
TEST(SmBiosInfoTest, TC_getentrypointoffset_001)
{
    SmBiosInfo smbios;
    const char dummy = 'a';
    ASSERT_THROW(smbios.get_entry_point_offset(NULL, 1), std::invalid_argument);
    ASSERT_THROW(smbios.get_entry_point_offset(&dummy, 0), std::invalid_argument);
    ASSERT_THROW(smbios.get_entry_point_offset(&dummy, 15), std::invalid_argument);

    std::vector<byte> buffer;
    const int64_t expected_offset = 32;

    //entry point is paragrah-aligned, insert 2 parags, then anchor string, then more parags
    std::string pad(16, 'A'); //random pre anchor string bytes
    std::string sm = "_SM_";  //anchor string
    pack_string(buffer, pad);
    pack_string(buffer, pad);
    pack_string(buffer, sm);
    pack_string(buffer, pad);
    ASSERT_EQ(expected_offset, smbios.get_entry_point_offset(reinterpret_cast<char*>(&buffer[0]), buffer.size()));

    //expect -1 when not paragraph-aligned
    buffer.clear();
    pack_string(buffer, pad);
    pack_string(buffer, pad);
    buffer.push_back('c'); //mess up alignment
    pack_string(buffer, sm);
    pack_string(buffer, pad);
    ASSERT_EQ(-1, smbios.get_entry_point_offset(reinterpret_cast<char*>(&buffer[0]), buffer.size()));

    //expect -1 when no _SM_ string found
    buffer.clear();
    pack_string(buffer, pad);
    pack_string(buffer, pad);
    pack_string(buffer, pad);
    pack_string(buffer, pad);
    ASSERT_EQ(-1, smbios.get_entry_point_offset(reinterpret_cast<char*>(&buffer[0]), buffer.size()));
}
*/

TEST(SmBiosInfoTest, TC_loadeps_001)
{
    SmBiosInfo smbios;
    SmBiosEntryPointStructure golden_struct;
    bzero(&golden_struct, sizeof(golden_struct));

    std::string sm = "_SM_";
    std::string dmi = "_DMI_";

    //set some fields
    memcpy(golden_struct.anchor, sm.c_str(), sm.length());
    memcpy(golden_struct.inter_anchor, dmi.c_str(), dmi.length());

    std::vector<byte> buffer;
    pack_struct<SmBiosEntryPointStructure>(buffer, &golden_struct);

    SmBiosEntryPointStructure eps;

    ASSERT_NO_THROW(smbios.load_eps(&eps, reinterpret_cast<char*>(&buffer[0]), buffer.size()));
    ASSERT_STREQ(golden_struct.anchor, eps.anchor);
    ASSERT_STREQ(golden_struct.inter_anchor, eps.inter_anchor);

    ASSERT_THROW(smbios.load_eps(NULL, reinterpret_cast<char*>(&buffer[0]), sm.length()), std::invalid_argument);
    ASSERT_THROW(smbios.load_eps(&eps, NULL, sm.length()), std::invalid_argument);
    ASSERT_THROW(smbios.load_eps(NULL, reinterpret_cast<char*>(&buffer[0]), 0), std::invalid_argument);
    ASSERT_THROW(smbios.load_eps(&eps, reinterpret_cast<char*>(&buffer[0]), sizeof(SmBiosEntryPointStructure) - 2), std::invalid_argument);
}

TEST(SmBiosInfoTest, TC_checksum_001)
{
    SmBiosInfo smbios;
    std::vector<byte> buffer;
    std::string zeros(15, '\0');
    pack_string(buffer, zeros);
    //the next ones will add up to 0
    buffer.push_back(~0);
    buffer.push_back(1);
    ASSERT_TRUE(smbios.checksum(&buffer[0], buffer.size()));
    //add another byte to buffer, it won't add up to 0 :(
    buffer.push_back(3);
    ASSERT_FALSE(smbios.checksum(&buffer[0], buffer.size()));

    SmBiosEntryPointStructure eps;
    const uint8_t eps_length = 0x1f; //as per DMTF SMBIOS spec version 2.7.1
    bzero(&eps, sizeof(eps));
    eps.eps_length = eps_length;
    eps.eps_checksum = (~0) - eps_length + 1;
    ASSERT_TRUE(smbios.checksum(eps));
    eps.smbios_major = 1;
    ASSERT_FALSE(smbios.checksum(eps));

    ASSERT_THROW(smbios.checksum(NULL, 2), std::invalid_argument);
    ASSERT_THROW(smbios.checksum(&buffer[0], 0), std::invalid_argument);
}

TEST(SmBiosInfoTest, TC_interchecksum_001)
{
    SmBiosInfo smbios;
    SmBiosEntryPointStructure eps;
    bzero(&eps, sizeof(eps));

    eps.inter_checksum = ~0;
    eps.bcd_rev = 1;

    ASSERT_TRUE(smbios.inter_checksum(eps));
    eps.bcd_rev += 1;
    ASSERT_FALSE(smbios.inter_checksum(eps));
}

TEST(SmBiosInfoTest, TC_validateeps_001)
{
    //create golden struct
    const uint8_t eps_length = 0x1f;
    SmBiosEntryPointStructure golden;
    bzero(&golden, sizeof(golden));
    //for the regular checksum
    golden.eps_length = eps_length;
    golden.eps_checksum = ~0 - eps_length + 1;
    //for the intermediate checksum, and intermediate anchor string
    const std::string dmi = "_DMI_";
    memcpy(golden.inter_anchor, dmi.c_str(), dmi.length());
    //mind not the overflow
    uint32_t dmi_uint = '_' + 'D' + 'M' + 'I' + '_';
    golden.inter_checksum = ~0 - dmi_uint + 1;

    SmBiosEntryPointStructure eps;
    memcpy(&eps, &golden, sizeof(eps));

    //all good
    SmBiosInfo smbios;
    ASSERT_NO_THROW(smbios.validate_eps(eps));

    //checksum fails
    eps.eps_length += 1;
    ASSERT_THROW(smbios.validate_eps(eps), SystoolsdException);

    //inter_checksum fails, reset struct
    memcpy(&eps, &golden, sizeof(eps));
    eps.inter_checksum += 1;
    ASSERT_THROW(smbios.validate_eps(eps), SystoolsdException);

    //inter_anchor not found, reset struct
    memcpy(&eps, &golden, sizeof(eps));
    eps.inter_anchor[0] = '-';
    ASSERT_THROW(smbios.validate_eps(eps), SystoolsdException);
}

TEST(SmBiosInfoTest, TC_loadheader_001)
{
    SmBiosStructHeader golden;
    bzero(&golden, sizeof(golden));
    golden.type = 1;
    std::vector<byte> buffer;
    pack_struct<SmBiosStructHeader>(buffer, &golden);

    SmBiosInfo smbios;
    SmBiosStructHeader hdr;
    ASSERT_NO_THROW(smbios.load_header(&hdr, reinterpret_cast<char*>(&buffer[0]), buffer.size()));

    ASSERT_THROW(smbios.load_header(NULL, reinterpret_cast<char*>(&buffer[0]), buffer.size()),
            std::invalid_argument);
    ASSERT_THROW(smbios.load_header(&hdr, NULL, buffer.size()),
            std::invalid_argument);
    ASSERT_THROW(smbios.load_header(&hdr, reinterpret_cast<char*>(&buffer[0]), 0),
            std::invalid_argument);
}

TEST(SmBiosInfoTest, TC_getnextstructoffset_001)
{
    //the length of the formatted area starts at the Type field of SmBiosStructHeader.
    //The length of the structure's string-set is not included.
    //Pack the header (hdr), the formatted area (pad) and two random strings for the
    //string section (str1, str2), separated by NULL bytes. Signal the end of the struct
    //with double NULL byte.
    const uint8_t length = 34;
    const uint8_t formatted_area_length = length - sizeof(SmBiosStructHeader);

    SmBiosStructHeader hdr;
    hdr.length = length;
    std::string pad(formatted_area_length, 'A');
    std::string str1 = "some dmi string";
    std::string str2 = "some other dmi string";
    std::vector<byte> buffer;
    pack_struct<SmBiosStructHeader>(buffer, &hdr);
    pack_string(buffer, pad);
    pack_string(buffer, str1);
    buffer.push_back('\0');
    pack_string(buffer, str2);
    buffer.push_back('\0');
    buffer.push_back('\0');

    const long int expected_offset = length + str1.length() + str2.length() + 3; //3 null bytes total
    SmBiosInfo smbios;
    ASSERT_EQ(expected_offset,
            smbios.get_next_struct_offset(hdr, reinterpret_cast<char*>(&buffer[0]), buffer.size()));

    ASSERT_THROW(smbios.get_next_struct_offset(hdr, reinterpret_cast<char*>(&buffer[0]), hdr.length - 1),
            std::invalid_argument);

    //delete last null byte so that end of struct is not found
    buffer.pop_back();
    ASSERT_EQ(-1, smbios.get_next_struct_offset(hdr, reinterpret_cast<char*>(&buffer[0]), buffer.size()));
}

TEST(SmBiosInfoTest, TC_parseandaddstructs_001)
{
    //pack one s_BiosInfoStructure struct, three s_MemoryDeviceStructure
    std::vector<byte> buffer;
    const size_t bios_info_num = 1;
    const size_t memory_device_num = 3;
    s_BiosInfoStructure bios_info;
    s_MemoryDeviceStructure memory_device;
    memset(&bios_info, 'A', sizeof(bios_info));
    memset(&memory_device, 'A', sizeof(memory_device));

    //set string fields to zero
    bios_info.vendor = 0;
    bios_info.bios_version = 0;
    bios_info.bios_release_date = 0;
    memory_device.device_locator = 0;
    memory_device.bank_locator = 0;
    memory_device.manufacturer = 0;
    memory_device.serial_number = 0;
    memory_device.asset_tag = 0;
    memory_device.part_number = 0;

    //set correct lengths so structs are correctly validated
    bios_info.hdr.length = sizeof(s_BiosInfoStructure);
    memory_device.hdr.length = sizeof(s_MemoryDeviceStructure);

    //set correct types so SmBiosStructureBase::create_structure() factory creates the appropriate
    // objects
    bios_info.hdr.type = SmBiosStructureType::BIOS;
    memory_device.hdr.type = SmBiosStructureType::MEMORY_DEVICE;

    pack_struct<s_BiosInfoStructure>(buffer, &bios_info);
    buffer.push_back('\0');
    buffer.push_back('\0');
    for(size_t i = 0; i < memory_device_num; ++i)
    {
        pack_struct<s_MemoryDeviceStructure>(buffer, &memory_device);
        buffer.push_back('\0');
        buffer.push_back('\0');
    }

    //pack an end-of-table header
    SmBiosStructHeader end_of_table_hdr;
    bzero(&end_of_table_hdr, sizeof(end_of_table_hdr));
    end_of_table_hdr.type = end_of_table;
    pack_struct<SmBiosStructHeader>(buffer, &end_of_table_hdr);

    SmBiosInfo smbios;
    ASSERT_NO_THROW(smbios.parse_and_add_structs(reinterpret_cast<char*>(&buffer[0]), buffer.size()));
    ASSERT_EQ(bios_info_num, smbios.get_structures_of_type(SmBiosStructureType::BIOS).size());
    ASSERT_EQ(memory_device_num, smbios.get_structures_of_type(SmBiosStructureType::MEMORY_DEVICE).size());

    //remove the end-of-table header from buffer. The parser should be able to handle that.
    buffer.erase(buffer.end() - sizeof(SmBiosStructHeader), buffer.end());
    //clear structure vectors
    for(auto i = smbios.structures.begin(); i != smbios.structures.end(); ++i)
    {
        i->second.clear();
    }
    ASSERT_NO_THROW(smbios.parse_and_add_structs(reinterpret_cast<char*>(&buffer[0]), buffer.size()));
    ASSERT_EQ(bios_info_num, smbios.get_structures_of_type(SmBiosStructureType::BIOS).size());
    ASSERT_EQ(memory_device_num, smbios.get_structures_of_type(SmBiosStructureType::MEMORY_DEVICE).size());
}
