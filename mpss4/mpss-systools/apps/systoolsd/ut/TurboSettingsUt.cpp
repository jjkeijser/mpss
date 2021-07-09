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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "TurboSettings.hpp"
#include "mocks.hpp"

using ::testing::_;
using ::testing::Gt;
using ::testing::HasSubstr;
using ::testing::InSequence;
using ::testing::NotNull;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::ReturnArg;
using ::testing::SetArrayArgument;

class TurboSettingsTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        sysfs = new ::testing::NiceMock<MockFile>; //will be owned by TurboSettings instance

        //return a dummy file descriptor by default
        ON_CALL(*sysfs, open(_, _))
            .WillByDefault(Return(7));
    }

    void set_valid_open(int times, int with_flags)
    {
        int fd = 7;
        EXPECT_CALL(*sysfs, open(_, with_flags))
            .Times(times)
            .WillRepeatedly(Return(fd++));
    }

    //NiceMock so we don't see messages about "uninteresting calls"
    ::testing::NiceMock<MockFile> *sysfs;
};

TEST_F(TurboSettingsTest, TC_ctor_001)
{
    ASSERT_NO_THROW(TurboSettings t);
    delete sysfs;
}

TEST_F(TurboSettingsTest, TC_isturboenabled_001)
{
    //two reads will be made
    set_valid_open(2, O_RDONLY);
    //reading no_turbo, logic is inverted
    char enabled = '0';
    char disabled = '1';
    //first read for enabled, second one for disabled
    EXPECT_CALL(*sysfs, read(Gt(0), NotNull(), Gt(0)))
        .WillOnce(DoAll(SetArrayArgument<1>(&enabled, &enabled+1), ReturnArg<2>()))
        .WillOnce(DoAll(SetArrayArgument<1>(&disabled, &disabled+1), ReturnArg<2>()));
    //files must be closed
    EXPECT_CALL(*sysfs, close(Gt(0)))
        .Times(2);
    TurboSettings info;
    info.set_file_interface(sysfs);
    bool is_enabled;
    ASSERT_NO_THROW(is_enabled = info.is_turbo_enabled());
    ASSERT_TRUE(is_enabled);
    ASSERT_NO_THROW(is_enabled = info.is_turbo_enabled());
    ASSERT_FALSE(is_enabled);
}

TEST_F(TurboSettingsTest, TC_isturboenabled_throw_001)
{
    EXPECT_CALL(*sysfs, read(_, NotNull(), Gt(0)))
        .WillOnce(Return(-1));
    EXPECT_CALL(*sysfs, close(Gt(0)))
        .Times(1);
    TurboSettings info;
    info.set_file_interface(sysfs);
    ASSERT_THROW(info.is_turbo_enabled(), SystoolsdException);
}

TEST_F(TurboSettingsTest, TC_isturboenabled_throw_002)
{
    EXPECT_CALL(*sysfs, open(_, _))
        .WillOnce(Return(-1));
    EXPECT_CALL(*sysfs, close(_))
        .Times(0);
    TurboSettings info;
    info.set_file_interface(sysfs);
    ASSERT_THROW(info.is_turbo_enabled(), SystoolsdException);
}

TEST_F(TurboSettingsTest, TC_turbopct_001)
{
    set_valid_open(1, O_RDONLY);
    const uint8_t expected_pct = 70;
    char buf[2] = {'7', '0'};
    EXPECT_CALL(*sysfs, read(Gt(0), NotNull(), Gt(0)))
        .WillOnce(DoAll(SetArrayArgument<1>(buf, buf+2), ReturnArg<2>()));
    EXPECT_CALL(*sysfs, close(Gt(0)))
        .Times(1);
    TurboSettings info;
    info.set_file_interface(sysfs);
    uint8_t pct = 0;
    ASSERT_NO_THROW(pct = info.get_turbo_pct());
    ASSERT_EQ(expected_pct, pct);
}

TEST_F(TurboSettingsTest, TC_turbopct_throw_001)
{
    EXPECT_CALL(*sysfs, read(_, NotNull(), Gt(0)))
        .WillOnce(Return(-1));
    EXPECT_CALL(*sysfs, close(Gt(0)))
        .Times(1);
    TurboSettings info;
    info.set_file_interface(sysfs);
    ASSERT_THROW(info.get_turbo_pct(), SystoolsdException);
}

TEST_F(TurboSettingsTest, TC_turbopct_throw_002)
{
    EXPECT_CALL(*sysfs, open(_, _))
        .WillOnce(Return(-1));
    EXPECT_CALL(*sysfs, close(_))
        .Times(0);
    TurboSettings info;
    info.set_file_interface(sysfs);
    ASSERT_THROW(info.get_turbo_pct(), SystoolsdException);
}

TEST_F(TurboSettingsTest, TC_setturbo_001)
{
    set_valid_open(2, O_WRONLY);
    InSequence seq;
    //first call must write 0, second 1
    EXPECT_CALL(*sysfs, write(Gt(0), Pointee('0'), Gt(0)))
        .WillOnce(ReturnArg<2>());
    EXPECT_CALL(*sysfs, write(Gt(0), Pointee('1'), Gt(0)))
        .WillOnce(ReturnArg<2>());
    TurboSettings info;
    info.set_file_interface(sysfs);
    ASSERT_NO_THROW(info.set_turbo_enabled(true));
    ASSERT_NO_THROW(info.set_turbo_enabled(false));
}

TEST_F(TurboSettingsTest, TC_setturbo_throw_001)
{
    EXPECT_CALL(*sysfs, write(Gt(0), NotNull(), Gt(0)))
        .WillOnce(Return(-1));
    TurboSettings info;
    info.set_file_interface(sysfs);
    ASSERT_THROW(info.set_turbo_enabled(true), SystoolsdException);
}
