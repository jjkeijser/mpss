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

#include "FileReader.hpp"
#include "mocks.hpp"
#include "smbios/EntryPointEfi.hpp"
#include "smbios/EntryPointMemoryScan.hpp"
#include "SystoolsdException.hpp"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Gt;
using ::testing::Matcher;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SetArrayArgument;
using ::testing::Throw;

TEST(EntryPointEfiTest, TC_ctor_001)
{
    ASSERT_NO_THROW(EntryPointEfi efi);
}

TEST(EntryPointEfiTest, TC_open_001)
{
    //First call succeeds
    MockFileStream *stream = new MockFileStream; // owned by EntryPointEfi
    EXPECT_CALL(*stream, open(Matcher<const char*>(NotNull()), _))
        .Times(1);
    EXPECT_CALL(*stream, good())
        .WillOnce(Return(true));
    EntryPointEfi efi;
    efi.set_file_reader(stream);
    ASSERT_NO_THROW(efi.open());
}

TEST(EntryPointEfiTest, TC_open_002)
{
    //First open fails, second one succeeds
    MockFileStream *stream = new MockFileStream; // owned by EntryPointEfi
    EXPECT_CALL(*stream, open(Matcher<const char*>(NotNull()), _))
        .Times(2);
    EXPECT_CALL(*stream, good())
        .WillOnce(Return(false))
        .WillOnce(Return(true));
    EntryPointEfi efi;
    efi.set_file_reader(stream);
    ASSERT_NO_THROW(efi.open());
}

TEST(EntryPointEfiTest, TC_open_throw_001)
{
    MockFileStream *stream = new MockFileStream; // owned by EntryPointEfi
    EXPECT_CALL(*stream, good())
        .WillRepeatedly(Return(false));
    EntryPointEfi efi;
    efi.set_file_reader(stream);
    ASSERT_THROW(efi.open(), SystoolsdException);
}

TEST(EntryPointEfiTest, TC_isefi_001)
{
    ASSERT_TRUE(EntryPointEfi().is_efi());
}

TEST(EntryPointEfiTest, TC_getaddr_001)
{
    std::vector<std::string> lines = {
        "ACPI20=0xbbfdf014",
        "ACPI=0xbbfdf000",
        "SMBIOS=0xabad1dea"
    };
    uint64_t expected_addr = 0xabad1dea;
    MockFileStream *stream = new MockFileStream; // owned by EntryPointEfi
    EXPECT_CALL(*stream, is_open())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*stream, good())
        .WillRepeatedly(Return(true));
    // Calls to MockFileStream::getline() will write to argument 0 (buffer)
    // and return stream. Three calls will happen, and each one will return
    // the corresponding line in vector lines.
    EXPECT_CALL(*stream, getline(NotNull(), Gt(0)))
        .WillOnce(
                DoAll(
                    SetArrayArgument<0>(lines[0].c_str(), lines[0].c_str() + lines[0].size()),
                    ReturnRef(*stream)
                    ))
        .WillOnce(
                DoAll(
                    SetArrayArgument<0>(lines[1].c_str(), lines[1].c_str() + lines[1].size()),
                    ReturnRef(*stream)
                    ))
        .WillOnce(DoAll(
                    SetArrayArgument<0>(lines[2].c_str(), lines[2].c_str() + lines[2].size()),
                    ReturnRef(*stream)
                    ));
    EntryPointEfi efi;
    efi.set_file_reader(stream);
    uint64_t addr;
    ASSERT_NO_THROW(addr = efi.get_addr());
    ASSERT_EQ(expected_addr, addr);
    //ensure addr is cached by object
    ASSERT_NO_THROW(addr = efi.get_addr());
    ASSERT_EQ(expected_addr, addr);
}

TEST(EntryPointEfiTest, TC_getaddr_notopen_001)
{
    MockFileStream *stream = new MockFileStream; // owned by EntryPointEfi
    EXPECT_CALL(*stream, is_open())
        .WillOnce(Return(false));
    EntryPointEfi efi;
    efi.set_file_reader(stream);
    ASSERT_THROW(efi.get_addr(), SystoolsdException);
}

TEST(EntryPointEfiTest, TC_getaddr_nosmbios_001)
{
    std::vector<std::string> lines = {
        "ACPI20=0xbbfdf014",
        "ACPI=0xbbfdf000",
        "UNICORNS=0xabad1dea"
    };
    MockFileStream *stream = new MockFileStream; // owned by EntryPointEfi
    EXPECT_CALL(*stream, is_open())
        .WillRepeatedly(Return(true));

    // fourth call must fail, because there are only three lines
    // until our file is "not good()"
    EXPECT_CALL(*stream, good())
        .WillOnce(Return(true))
        .WillOnce(Return(true))
        .WillOnce(Return(true))
        .WillOnce(Return(false));

    // Calls to MockFileStream::getline() will write to argument 0 (buffer)
    // and return stream. Three calls will happen, and each one will return
    // the corresponding line in vector lines.
    EXPECT_CALL(*stream, getline(NotNull(), Gt(0)))
        .WillOnce(
                DoAll(
                    SetArrayArgument<0>(lines[0].c_str(), lines[0].c_str() + lines[0].size()),
                    ReturnRef(*stream)
                    ))
        .WillOnce(
                DoAll(
                    SetArrayArgument<0>(lines[1].c_str(), lines[1].c_str() + lines[1].size()),
                    ReturnRef(*stream)
                    ))
        .WillOnce(DoAll(
                    SetArrayArgument<0>(lines[2].c_str(), lines[2].c_str() + lines[2].size()),
                    ReturnRef(*stream)
                    ));
    EntryPointEfi efi;
    efi.set_file_reader(stream);
    ASSERT_THROW(efi.get_addr(), SystoolsdException);

}

TEST(EntryPointEfiTest, TC_setfilereader_throw_001)
{
    ASSERT_THROW(EntryPointEfi().set_file_reader(NULL), std::invalid_argument);
}

TEST(EntryPointEfiTest, TC_close_001)
{
    MockFileStream *stream = new MockFileStream; // owned by EntryPointEfi
    EXPECT_CALL(*stream, close())
        .Times(AtLeast(1));
    EntryPointEfi efi;
    efi.set_file_reader(stream);
    ASSERT_NO_THROW(efi.close());
}

TEST(EntryPointMemoryScanTest, TC_ctor_001)
{
    ASSERT_NO_THROW(EntryPointMemoryScan memscan);
    ASSERT_NO_THROW(EntryPointMemoryScan memscan("somepath"));
}

TEST(EntryPointMemoryScanTest, TC_isefi_001)
{
    ASSERT_FALSE(EntryPointMemoryScan().is_efi());
}

TEST(EntryPointMemoryScanTest, TC_open_001)
{
    MockFileStream *stream = new MockFileStream; // owned by EntryPointMemoryScan
    EXPECT_CALL(*stream, open(Matcher<const std::string&>(_), _))
        .Times(1);
    EXPECT_CALL(*stream, good())
        .WillOnce(Return(true));
    EntryPointMemoryScan memscan;
    memscan.set_file_reader(stream);
    ASSERT_NO_THROW(memscan.open());
}

TEST(EntryPointMemoryScanTest, TC_open_throw_001)
{
    MockFileStream *stream = new MockFileStream; // owned by EntryPointMemoryScan
    EXPECT_CALL(*stream, open(Matcher<const std::string&>(_), _))
        .Times(1);
    EXPECT_CALL(*stream, good())
        .WillOnce(Return(false));
    EntryPointMemoryScan memscan;
    memscan.set_file_reader(stream);
    ASSERT_THROW(memscan.open(), SystoolsdException);
}

TEST(EntryPointMemoryScanTest, TC_getaddr_001)
{
    MockFileStream *stream = new MockFileStream; // owned by EntryPointMemoryScan

    EXPECT_CALL(*stream, is_open())
        .WillOnce(Return(true)); // File will be open

    EXPECT_CALL(*stream, good())
        .WillRepeatedly(Return(true)); // Stream will be "good()"

    EXPECT_CALL(*stream, tellg())
        .WillRepeatedly(Return(7)); // As long as it doesn't return -1, we don't care for this test

    EXPECT_CALL(*stream, seekg(_))
        .WillOnce(ReturnRef(*stream));

    // our dummy buffer with the anchor string in the 3rd parag
    std::vector<char> buf(64, 'A');
    buf[32] = '_';
    buf[33] = 'S';
    buf[34] = 'M';
    buf[35] = '_';

    // For each successive call:
    //   Fill the 'buffer' argument with paragraph n
    //   Return the stream
    EXPECT_CALL(*stream, read(NotNull(), _))
        .Times(3) //only three calls must happen, because the anchor string is in
                  // paragraph 3
        .WillOnce(
                DoAll(SetArrayArgument<0>(&buf[0], &buf[0] + 16),
                      ReturnRef(*stream)
                ))
        .WillOnce(
                DoAll(SetArrayArgument<0>(&buf[16], &buf[16] + 16),
                      ReturnRef(*stream)
                ))
        .WillOnce(
                DoAll(SetArrayArgument<0>(&buf[32], &buf[32] + 16),
                      ReturnRef(*stream)
                ));

    const uint64_t expected_offset = 16 * 2;
    uint64_t offset = 0;
    EntryPointMemoryScan memscan;
    memscan.set_file_reader(stream);
    ASSERT_NO_THROW(offset = memscan.get_addr());
    ASSERT_EQ(expected_offset, offset);
    //make sure the address is cached by the object
    ASSERT_NO_THROW(offset = memscan.get_addr());
    ASSERT_EQ(expected_offset, offset);
}

TEST(EntryPointMemoryScanTest, TC_getaddr_throw_001)
{
    MockFileStream *stream = new MockFileStream; // owned by EntryPointMemoryScan
    EXPECT_CALL(*stream, is_open())
        .WillOnce(Return(false)); //fail, file not open
    EntryPointMemoryScan memscan;
    memscan.set_file_reader(stream);
    ASSERT_THROW(memscan.get_addr(), SystoolsdException);
}

TEST(EntryPointMemoryScanTest, TC_getaddr_throw_002)
{
    MockFileStream *stream = new MockFileStream; // owned by EntryPointMemoryScan
    EXPECT_CALL(*stream, is_open())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*stream, seekg(_))
        .WillRepeatedly(ReturnRef(*stream));
    EXPECT_CALL(*stream, good())
        .WillOnce(Return(false)); //fail, file not good()
    EntryPointMemoryScan memscan;
    memscan.set_file_reader(stream);
    ASSERT_THROW(memscan.get_addr(), SystoolsdException);
}

TEST(EntryPointMemoryScanTest, TC_getaddr_throw_003)
{
    MockFileStream *stream = new MockFileStream; // owned by EntryPointMemoryScan
    EXPECT_CALL(*stream, is_open())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*stream, seekg(_))
        .WillRepeatedly(ReturnRef(*stream));
    EXPECT_CALL(*stream, good())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*stream, tellg())
        .WillOnce(Return(-1)); //fail, tellg() returns -1
    EntryPointMemoryScan memscan;
    memscan.set_file_reader(stream);
    ASSERT_THROW(memscan.get_addr(), SystoolsdException);
}

TEST(EntryPointMemoryScanTest, TC_getaddr_throw_004)
{
    MockFileStream *stream = new MockFileStream; // owned by EntryPointMemoryScan
    EXPECT_CALL(*stream, is_open())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*stream, seekg(_))
        .WillRepeatedly(ReturnRef(*stream));
    EXPECT_CALL(*stream, good())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*stream, tellg())
        .WillRepeatedly(Return(-1)); //fail, tellg() returns -1
    EntryPointMemoryScan memscan;
    memscan.set_file_reader(stream);
    ASSERT_THROW(memscan.get_addr(), SystoolsdException);
}

TEST(EntryPointMemoryScanTest, TC_getaddr_throw_005)
{
    MockFileStream *stream = new MockFileStream; // owned by EntryPointMemoryScan
    EXPECT_CALL(*stream, is_open())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*stream, seekg(_))
        .WillRepeatedly(ReturnRef(*stream));
    EXPECT_CALL(*stream, tellg())
        .WillRepeatedly(Return(7));
    EXPECT_CALL(*stream, good())
        .WillOnce(Return(true))
        .WillOnce(Return(false)); // fail, good() check fails in loop
    EntryPointMemoryScan memscan;
    memscan.set_file_reader(stream);
    ASSERT_THROW(memscan.get_addr(), SystoolsdException);
}

TEST(EntryPointMemoryScanTest, TC_getaddr_throw_006)
{
    const long int invalid_offset =
        0xf0000 + // initial offset at which the SMBIOS table might be located
        0x10000;  // the range in which the SMBIOS table might be located
    MockFileStream *stream = new MockFileStream; // owned by EntryPointMemoryScan
    EXPECT_CALL(*stream, is_open())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*stream, seekg(_))
        .WillRepeatedly(ReturnRef(*stream));
    EXPECT_CALL(*stream, good())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*stream, tellg())
        .WillOnce(Return(invalid_offset)); // fail, the SMBIOS table won't be past this offset
    EntryPointMemoryScan memscan;
    memscan.set_file_reader(stream);
    ASSERT_THROW(memscan.get_addr(), SystoolsdException);
}

TEST(EntryPointMemoryScanTest, TC_setfilereader_throw_001)
{
    ASSERT_THROW(EntryPointMemoryScan().set_file_reader(NULL), std::invalid_argument);
}

