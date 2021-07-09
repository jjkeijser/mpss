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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "smbios/BiosInfoStructure.hpp"
#include "smbios/MemoryDeviceStructure.hpp"
#include "smbios/ProcessorInfoStructure.hpp"
#include "smbios/SystemInfoStructure.hpp"
#include "SystoolsdException.hpp"
#include "ut_utils.hpp"

//typedefs to match naming convention used in SMBIOS spec
typedef uint8_t byte;
typedef uint16_t word;
typedef uint64_t qword;

struct s_TestStructure
{
    SmBiosStructHeader hdr;
    byte field1;
    byte str1;
    byte str2;
};

class TestStructure : public SmBiosStructureBase
{
public:
    void validate_and_build(SmBiosStructHeader &hdr, const uint8_t *const buf, size_t len)
    {
        return;
    }

private:
    s_TestStructure teststruct;
    std::string str1;
    std::string str2;
};

//////////////////////////////////////////
//  BiosInfoStructure UTs
/////////////////////////////////////////
namespace BiosInfoStructureTest
{
    const std::string vendor_str = "NiceBios";
    const std::string bios_version_str = "2.6";
    const std::string bios_release_date_str = "last night";

    const byte type = 0;
    const byte length = sizeof(s_BiosInfoStructure);
    const byte vendor = 1;
    const byte bios_version = 2;
    const byte bios_release_date = 3;

    std::vector<byte> init_struct()
    {
        s_BiosInfoStructure data;
        bzero(&data, sizeof(data));
        data.hdr.type = type;
        data.hdr.length = length;
        data.vendor = vendor;
        data.bios_version = bios_version;
        data.bios_release_date = bios_release_date;

        std::vector<byte> buffer;
        //pack the BiosInfoStructure
        pack_struct<s_BiosInfoStructure>(buffer, &data);

        //pack the strings
        pack_string(buffer, vendor_str);
        buffer.push_back('\0');
        pack_string(buffer, bios_version_str);
        buffer.push_back('\0');
        pack_string(buffer, bios_release_date_str);
        buffer.push_back('\0');
        buffer.push_back('\0');

        return buffer;
    }

    TEST(BiosInfoStructureTest, TC_validate_001)
    {
        auto buf = init_struct();
        BiosInfoStructure info;
        SmBiosStructHeader hdr = get_header(buf);
        ASSERT_NO_THROW(info.validate_and_build(hdr, &buf[0], buf.size()));
        ASSERT_EQ(vendor_str, info.get_vendor());
        ASSERT_EQ(bios_version_str, info.get_bios_version());
        ASSERT_EQ(bios_release_date_str, info.get_bios_release_date());
        ASSERT_EQ(length, info.get_bios_struct().hdr.length);
    }

    TEST(BiosInfoStructureTest, TC_validate_throw_001)
    {
        auto buf = init_struct();
        SmBiosStructHeader *phdr = reinterpret_cast<SmBiosStructHeader*>(&buf[0]);
        //invalid length for this struct
        phdr->length = 8;
        BiosInfoStructure info;
        ASSERT_THROW(info.validate_and_build(*phdr, &buf[0], buf.size()), SystoolsdException);
        //reset buf
        buf = init_struct();
        phdr = reinterpret_cast<SmBiosStructHeader*>(&buf[0]);
        ASSERT_THROW(info.validate_and_build(*phdr, &buf[0], sizeof(s_BiosInfoStructure) - 2),
                std::invalid_argument);
    }
}

//////////////////////////////////////////
//  MemoryDeviceStructure UTs
/////////////////////////////////////////
namespace MemoryDeviceStructureTest
{
    const std::string device_locator_str = "SIMM 3";
    const std::string bank_locator_str = "Bank 0";
    const std::string manufacturer_str = "NiceMemories";
    const std::string serial_number_str = "this is a SN, really";
    const std::string asset_tag_str = "memory asset";
    const std::string part_number_str = "1";

    const byte type = 17;
    const byte length = sizeof(s_MemoryDeviceStructure);
    const byte device_locator = 1;
    const byte bank_locator = 2;
    const byte manufacturer = 3;
    const byte serial_number = 4;
    const byte asset_tag = 5;
    const byte part_number = 6;

    std::vector<byte> init_struct()
    {
        s_MemoryDeviceStructure data;
        bzero(&data, sizeof(data));
        data.hdr.type = type;
        data.hdr.length = length;
        data.device_locator = device_locator;
        data.bank_locator = bank_locator;
        data.manufacturer = manufacturer;
        data.serial_number = serial_number;
        data.asset_tag = asset_tag;
        data.part_number = part_number;

        std::vector<byte> buffer;
        pack_struct<s_MemoryDeviceStructure>(buffer, &data);
        pack_string(buffer, device_locator_str);
        buffer.push_back('\0');
        pack_string(buffer, bank_locator_str);
        buffer.push_back('\0');
        pack_string(buffer, manufacturer_str);
        buffer.push_back('\0');
        pack_string(buffer, serial_number_str);
        buffer.push_back('\0');
        pack_string(buffer, asset_tag_str);
        buffer.push_back('\0');
        pack_string(buffer, part_number_str);
        buffer.push_back('\0');
        buffer.push_back('\0');

        return buffer;
    }

    TEST(MemoryDeviceStructureTest, TC_validate_001)
    {
        auto buf = init_struct();
        MemoryDeviceStructure info;
        SmBiosStructHeader hdr = get_header(buf);
        ASSERT_NO_THROW(info.validate_and_build(hdr, &buf[0], buf.size()));
        ASSERT_EQ(device_locator_str, info.get_device_locator());
        ASSERT_EQ(bank_locator_str, info.get_bank_locator());
        ASSERT_EQ(manufacturer_str, info.get_manufacturer());
        ASSERT_EQ(serial_number_str, info.get_serial_number());
        ASSERT_EQ(asset_tag_str, info.get_asset_tag());
        ASSERT_EQ(part_number_str, info.get_part_number());
        ASSERT_EQ(length, info.get_memory_device_struct().hdr.length);
        ASSERT_EQ(0, info.get_memory_device_struct().size);

        buf = init_struct();
        s_MemoryDeviceStructure *pinfo = NULL;
        pinfo = reinterpret_cast<s_MemoryDeviceStructure*>(&buf[0]);
        pinfo->size = 0xFFFF;
        ASSERT_NO_THROW(info.validate_and_build(hdr, &buf[0], buf.size()));
        ASSERT_EQ(0, info.get_memory_device_struct().size);

        const uint16_t expected_base = 256;
        //256 KB, set MSb
        uint16_t expected_raw = expected_base | 0x8000;

        pinfo = reinterpret_cast<s_MemoryDeviceStructure*>(&buf[0]);
        pinfo->size = expected_raw;
        ASSERT_NO_THROW(info.validate_and_build(hdr, &buf[0], buf.size()));
        ASSERT_EQ(expected_raw, info.get_memory_device_struct().size);
        ASSERT_EQ(expected_base, info.get_memsize_base());
        ASSERT_EQ(MemUnit::kilo, info.memsize_unit());

        //256 MB, unset MSb
        expected_raw = expected_base;
        pinfo = reinterpret_cast<s_MemoryDeviceStructure*>(&buf[0]);
        pinfo->size = expected_raw;
        ASSERT_NO_THROW(info.validate_and_build(hdr, &buf[0], buf.size()));
        ASSERT_EQ(expected_raw, info.get_memory_device_struct().size);
        ASSERT_EQ(expected_base, info.get_memsize_base());
        ASSERT_EQ(MemUnit::mega, info.memsize_unit());
    }

    TEST(MemoryDeviceStructureTest, TC_validate_throw_001)
    {
        auto buf = init_struct();
        SmBiosStructHeader *phdr = reinterpret_cast<SmBiosStructHeader*>(&buf[0]);
        //invalid length for this struct
        phdr->length = 8;
        MemoryDeviceStructure info;
        ASSERT_THROW(info.validate_and_build(*phdr, &buf[0], buf.size()), SystoolsdException);
        //reset buf
        buf = init_struct();
        phdr = reinterpret_cast<SmBiosStructHeader*>(&buf[0]);
        ASSERT_THROW(info.validate_and_build(*phdr, &buf[0], sizeof(s_MemoryDeviceStructure) - 2),
                std::invalid_argument);
    }
}

//////////////////////////////////////////
////  ProcessorInfoStructure UTs
///////////////////////////////////////////
namespace ProcessorInfoStructureTest
{
    const std::string socket_designation_str = "socket_designation";
    const std::string processor_manufacturer_str = "processor_manufacturer";
    const std::string processor_version_str = "processor_version";
    const std::string serial_number_str = "serial_number";
    const std::string asset_tag_str = "asset_tag";
    const std::string part_number_str = "part_number";

    const byte type = 4;
    const byte length = sizeof(s_ProcessorInfoStructure);
    const byte socket_designation = 1;
    const byte processor_manufacturer = 2;
    const byte processor_version = 3;
    const byte serial_number = 4;
    const byte asset_tag = 5;
    const byte part_number = 6;

    std::vector<byte> init_struct()
    {
        s_ProcessorInfoStructure data;
        bzero(&data, sizeof(data));
        data.hdr.type = type;
        data.hdr.length = length;
        data.socket_designation = socket_designation;
        data.processor_manufacturer = processor_manufacturer;
        data.processor_version = processor_version;
        data.serial_number = serial_number;
        data.asset_tag = asset_tag;
        data.part_number = part_number;

        std::vector<byte> buffer;
        pack_struct<s_ProcessorInfoStructure>(buffer, &data);

        pack_string(buffer, socket_designation_str);
        buffer.push_back('\0');
        pack_string(buffer, processor_manufacturer_str);
        buffer.push_back('\0');
        pack_string(buffer, processor_version_str);
        buffer.push_back('\0');
        pack_string(buffer, serial_number_str);
        buffer.push_back('\0');
        pack_string(buffer, asset_tag_str);
        buffer.push_back('\0');
        pack_string(buffer, part_number_str);
        buffer.push_back('\0');
        buffer.push_back('\0');
        return buffer;
    }

    TEST(ProcessorInfoStructureTest, TC_validate_001)
    {
        auto buf = init_struct();
        ProcessorInfoStructure info;
        SmBiosStructHeader hdr = get_header(buf);
        ASSERT_NO_THROW(info.validate_and_build(hdr, &buf[0], buf.size()));
        ASSERT_EQ(socket_designation_str, info.get_socket_designation());
        ASSERT_EQ(processor_manufacturer_str, info.get_manufacturer());
        ASSERT_EQ(processor_version_str, info.get_processor_version());
        ASSERT_EQ(serial_number_str, info.get_serial_number());
        ASSERT_EQ(asset_tag_str, info.get_asset_tag());
        ASSERT_EQ(part_number_str, info.get_part_number());
    }

    TEST(ProcessorInfoStructureTest, TC_validate_throw_001)
    {
        auto buf = init_struct();
        SmBiosStructHeader *phdr = reinterpret_cast<SmBiosStructHeader*>(&buf[0]);
        //invalid length for this struct
        phdr->length = 8;
        ProcessorInfoStructure info;
        ASSERT_THROW(info.validate_and_build(*phdr, &buf[0], buf.size()), SystoolsdException);
        //reset buf
        buf = init_struct();
        phdr = reinterpret_cast<SmBiosStructHeader*>(&buf[0]);
        ASSERT_THROW(info.validate_and_build(*phdr, &buf[0], sizeof(s_MemoryDeviceStructure) - 2),
                std::invalid_argument);
    }
}

//////////////////////////////////////////
////  SystemInfoStructure UTs
///////////////////////////////////////////
namespace SystemInfoStructureTest
{
    const std::string manufacturer_str = "manufacturer";
    const std::string product_name_str = "product_name";
    const std::string version_str = "version";
    const std::string serial_number_str = "serial_number";
    const std::string sku_number_str = "sku_number";
    const std::string family_str = "family";

    const byte type = 1;
    const byte length = sizeof(s_SystemInfoStructure);
    const byte manufacturer = 1;
    const byte product_name = 2;
    const byte version = 3;
    const byte serial_number = 4;
    const byte sku_number = 5;
    const byte family = 6;

    std::vector<byte> init_struct()
    {
        s_SystemInfoStructure data;
        bzero(&data, sizeof(data));
        data.hdr.type = type;
        data.hdr.length = length;
        data.manufacturer = manufacturer;
        data.product_name = product_name;
        data.version = version;
        data.serial_number = serial_number;
        data.sku_number = sku_number;
        data.family = family;

        std::vector<byte> buffer;
        pack_struct<s_SystemInfoStructure>(buffer, &data);
        pack_string(buffer, manufacturer_str);
        buffer.push_back('\0');
        pack_string(buffer, product_name_str);
        buffer.push_back('\0');
        pack_string(buffer, version_str);
        buffer.push_back('\0');
        pack_string(buffer, serial_number_str);
        buffer.push_back('\0');
        pack_string(buffer, sku_number_str);
        buffer.push_back('\0');
        pack_string(buffer, family_str);
        buffer.push_back('\0');
        buffer.push_back('\0');
        return buffer;
    }

    TEST(SystemInfoStructureTest, TC_validate_001)
    {
        auto buf = init_struct();
        SystemInfoStructure info;
        SmBiosStructHeader hdr = get_header(buf);
        ASSERT_NO_THROW(info.validate_and_build(hdr, &buf[0], buf.size()));
        ASSERT_EQ(manufacturer_str, info.get_manufacturer());
        ASSERT_EQ(product_name_str, info.get_product_name());
        ASSERT_EQ(version_str, info.get_version());
        ASSERT_EQ(serial_number_str, info.get_serial_number());
        ASSERT_EQ(sku_number_str, info.get_sku_number());
        ASSERT_EQ(family_str, info.get_family());
    }

    TEST(SystemInfoStructureTest, TC_validate_throw_001)
    {
        auto buf = init_struct();
        SmBiosStructHeader *phdr = reinterpret_cast<SmBiosStructHeader*>(&buf[0]);
        //invalid length for this struct
        phdr->length = 8;
        SystemInfoStructure info;
        ASSERT_THROW(info.validate_and_build(*phdr, &buf[0], buf.size()), SystoolsdException);
        //reset buf
        buf = init_struct();
        phdr = reinterpret_cast<SmBiosStructHeader*>(&buf[0]);
        ASSERT_THROW(info.validate_and_build(*phdr, &buf[0], sizeof(s_SystemInfoStructure) - 2),
                std::invalid_argument);
    }
}

//////////////////////////////////////////
//  SmBiosStructureBase UTs
/////////////////////////////////////////
namespace SmBiosStructureBaseTest
{
    std::vector<byte> init_struct()
    {
        s_TestStructure teststruct;
        bzero(&teststruct, sizeof(teststruct));
        teststruct.str1 = 1;
        teststruct.str2 = 2;
        std::vector<byte> buffer;
        pack_struct<s_TestStructure>(buffer, &teststruct);
        return buffer;
    }

    TEST(SmBiosStructureBaseTest, TC_createstructure_001)
    {
        auto structure = SmBiosStructureBase::create_structure(SmBiosStructureType::BIOS);
        EXPECT_TRUE(typeid(*structure.get()) == typeid(BiosInfoStructure));
        structure = SmBiosStructureBase::create_structure(SmBiosStructureType::SYSTEM_INFO);
        EXPECT_TRUE(typeid(*structure.get()) == typeid(SystemInfoStructure));
        structure = SmBiosStructureBase::create_structure(SmBiosStructureType::PROCESSOR_INFO);
        EXPECT_TRUE(typeid(*structure.get()) == typeid(ProcessorInfoStructure));
        structure = SmBiosStructureBase::create_structure(SmBiosStructureType::MEMORY_DEVICE);
        EXPECT_TRUE(typeid(*structure.get()) == typeid(MemoryDeviceStructure));
    }

    TEST(SmBiosStructureBaseTest, TC_setstring_001)
    {
        std::string str_to_set;
        std::vector<std::string> strings = {"some string"};
        //valid
        ASSERT_NO_THROW(SmBiosStructureBase::set_string(str_to_set, strings, 1));
        ASSERT_EQ(str_to_set, strings[0]);
        //invalid, no string 2
        ASSERT_NO_THROW(SmBiosStructureBase::set_string(str_to_set, strings, 2));
        ASSERT_EQ(std::string("Not specified"), str_to_set);
    }

    TEST(SmBiosStructureBaseTest, TC_getstrings_001)
    {
        std::vector<byte> buffer;
        s_TestStructure teststruct;
        bzero(&teststruct, sizeof(teststruct));
        auto buf = init_struct();
        auto hdr = get_header(buf);

        //throw, null buffer
        ASSERT_THROW(SmBiosStructureBase::get_strings(hdr, NULL, 10), std::invalid_argument);
        //throw, size 0
        ASSERT_THROW(SmBiosStructureBase::get_strings(hdr, &buf[0], 0), std::invalid_argument);
        //throw, smaller buffer than required by the SMBIOS header
        hdr.length = sizeof(s_TestStructure);
        ASSERT_THROW(SmBiosStructureBase::get_strings(hdr, &buf[0], sizeof(s_TestStructure) - 1),
                std::invalid_argument);

        //set the end of the structure right after formatted area, no strings section
        buf.push_back('\0');
        buf.push_back('\0');
        std::vector<std::string> strings;
        std::vector<std::string> expected;
        ASSERT_NO_THROW(strings = SmBiosStructureBase::get_strings(hdr, &buf[0], buf.size()));
        ASSERT_TRUE(strings.empty());

        //reset buffer, pack strings
        expected = {"string 1", "string 2"};
        buf = init_struct();
        pack_string(buf, expected[0]);
        buf.push_back('\0');
        pack_string(buf, expected[1]);
        buf.push_back('\0');
        buf.push_back('\0');
        ASSERT_NO_THROW(strings = SmBiosStructureBase::get_strings(hdr, &buf[0], buf.size()));
        ASSERT_EQ(expected[0], strings[0]);
        ASSERT_EQ(expected[1], strings[1]);
    }
}
