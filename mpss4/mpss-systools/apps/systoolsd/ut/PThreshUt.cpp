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

#include <stdexcept>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>


#include "PThresh.hpp"
#include "FileInterface.hpp"
#include "SystoolsdException.hpp"
#include "mocks.hpp"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Ge;
using ::testing::Gt;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::ReturnArg;
using ::testing::SetArrayArgument;

namespace
{
    const int FD = 7;
    const std::string sysfs_path = "/sys/devices/virtual/powercap/intel-rapl/intel-rapl:0";
}

class PThreshTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        //This will be deleted by the class using it
        sysfs = new NiceMock<MockFile>();

        //default behavior for mocked functions
        ON_CALL(*sysfs, open(_, _))
            .WillByDefault(Return(FD));
        ON_CALL(*sysfs, read(Gt(0), NotNull(), Gt(0)))
            .WillByDefault(ReturnArg<2>());
        ON_CALL(*sysfs, write(Gt(0), NotNull(), Gt(0)))
            .WillByDefault(ReturnArg<2>());
        ON_CALL(*sysfs, write(Gt(0), _))
            .WillByDefault(Return(4));
    }
    MockFile *sysfs;
};

typedef PThreshTest PWindowTest;

TEST_F(PThreshTest, TC_getmaxphyspwr_001)
{
    PThresh p;
    p.set_file_interface(sysfs);
    uint32_t pwr = 0;
    ASSERT_NO_THROW(pwr = p.get_max_phys_pwr());
    ASSERT_EQ(0, pwr); //unsupported value
}

TEST_F(PThreshTest, TC_getlowpthresh_001)
{
    EXPECT_CALL(*sysfs, open(_, O_RDONLY))
        .WillOnce(Return(FD));
    EXPECT_CALL(*sysfs, close(Gt(0)))
        .Times(1);
    char res[] = {'1', '2', '3', '4'};
    uint32_t pwr = 0;
    EXPECT_CALL(*sysfs, read(Gt(0), NotNull(), Gt(0)))
        .WillOnce(DoAll(SetArrayArgument<1>(res, res+4), Return(4)));
    PThresh p;
    p.set_file_interface(sysfs);
    ASSERT_NO_THROW(pwr = p.get_low_pthresh());
    ASSERT_EQ(1234, pwr);
}

TEST_F(PThreshTest, TC_getlowpthresh_throw_by_read_001)
{
    EXPECT_CALL(*sysfs, close(Gt(0)))
        .Times(1);
    EXPECT_CALL(*sysfs, read(Gt(0), NotNull(), Gt(0)))
        .WillOnce(Return(-1));
    PThresh p;
    p.set_file_interface(sysfs);
    ASSERT_THROW(p.get_low_pthresh(), SystoolsdException);
}

TEST_F(PThreshTest, TC_getlowpthresh_throw_by_open_001)
{
    EXPECT_CALL(*sysfs, open(_, O_RDONLY))
        .WillOnce(Return(-1));
    EXPECT_CALL(*sysfs, close(_))
        .Times(0);
    PThresh p;
    p.set_file_interface(sysfs);
    ASSERT_THROW(p.get_low_pthresh(), SystoolsdException);
}


TEST_F(PThreshTest, TC_gethighpthresh_001)
{
    EXPECT_CALL(*sysfs, open(_, O_RDONLY))
        .WillOnce(Return(FD));
    EXPECT_CALL(*sysfs, close(Gt(0)))
        .Times(1);
    char res[] = {'1', '2', '3', '4'};
    uint32_t pwr = 0;
    EXPECT_CALL(*sysfs, read(Gt(0), NotNull(), Gt(0)))
        .WillOnce(DoAll(SetArrayArgument<1>(res, res+4), Return(4)));
    PThresh p;
    p.set_file_interface(sysfs);
    ASSERT_NO_THROW(pwr = p.get_high_pthresh());
    ASSERT_EQ(1234, pwr);
}

TEST_F(PThreshTest, TC_gethighpthresh_throw_by_read_001)
{
    EXPECT_CALL(*sysfs, read(Gt(0), NotNull(), Gt(0)))
        .WillOnce(Return(-1));
    EXPECT_CALL(*sysfs, close(Gt(0)))
        .Times(1);
    PThresh p;
    p.set_file_interface(sysfs);
    ASSERT_THROW(p.get_high_pthresh(), SystoolsdException);
}

TEST_F(PThreshTest, TC_gethighpthresh_throw_by_open_001)
{
    EXPECT_CALL(*sysfs, open(_, O_RDONLY))
        .WillOnce(Return(-1));
    EXPECT_CALL(*sysfs, close(_))
        .Times(0);
    PThresh p;
    p.set_file_interface(sysfs);
    ASSERT_THROW(p.get_high_pthresh(), SystoolsdException);
}

TEST_F(PWindowTest, TC_ctor_001)
{
    EXPECT_CALL(*sysfs, open(_, _))
        .WillOnce(Return(1))
        .WillOnce(Return(2));
    EXPECT_CALL(*sysfs, close(Gt(0)))
        .Times(2);
    ASSERT_NO_THROW(PWindow w(sysfs_path, 0, sysfs));
}

TEST_F(PWindowTest, TC_ctor_throw_by_open_001)
{
    EXPECT_CALL(*sysfs, open(_, _))
        .WillOnce(Return(-1));
    ASSERT_THROW(PWindow w(sysfs_path, 0, sysfs), SystoolsdException);
}

TEST_F(PWindowTest, TC_ctor_throw_by_open_002)
{
    EXPECT_CALL(*sysfs, open(_, _))
        .WillOnce(Return(1))
        .WillOnce(Return(-1));
    //expect the first file descriptor to be closed
    EXPECT_CALL(*sysfs, close(Gt(0)))
        .Times(1);
    ASSERT_THROW(PWindow w(sysfs_path, 0, sysfs), SystoolsdException);
}

TEST_F(PWindowTest, TC_getpthresh_001)
{
    EXPECT_CALL(*sysfs, open(_, _))
        .WillOnce(Return(1))
        .WillOnce(Return(2));
    EXPECT_CALL(*sysfs, close(Gt(0)))
        .Times(2);
    char res[] = {'1', '2', '3', '4'};
    uint32_t thresh = 0;
    EXPECT_CALL(*sysfs, read(1, NotNull(), Gt(0)))
        .WillOnce(DoAll(SetArrayArgument<1>(res, res+4), Return(4)));
    PWindow w(sysfs_path, 0, sysfs);
    ASSERT_NO_THROW(thresh = w.get_pthresh());
    ASSERT_EQ(1234, thresh);
}

TEST_F(PWindowTest, TC_getpthresh_throw_by_read_001)
{
    EXPECT_CALL(*sysfs, open(_, _))
        .WillOnce(Return(1))
        .WillOnce(Return(2));
    EXPECT_CALL(*sysfs, close(Gt(0)))
        .Times(2);
    EXPECT_CALL(*sysfs, read(1, NotNull(), Gt(0)))
        .WillOnce(Return(-1));
    PWindow w(sysfs_path, 0, sysfs);
    ASSERT_THROW(w.get_pthresh(), SystoolsdException);
}


TEST_F(PWindowTest, TC_gettimewindow_001)
{
    EXPECT_CALL(*sysfs, open(_, _))
        .WillOnce(Return(1))
        .WillOnce(Return(2));
    EXPECT_CALL(*sysfs, close(Gt(0)))
        .Times(2);
    char res[] = {'1', '2', '3', '4'};
    uint32_t tw = 0;
    EXPECT_CALL(*sysfs, read(2, NotNull(), Gt(0)))
        .WillOnce(DoAll(SetArrayArgument<1>(res, res+4), Return(4)));
    PWindow w(sysfs_path, 0, sysfs);
    ASSERT_NO_THROW(tw = w.get_time_window());
    ASSERT_EQ(1234, tw);
}

TEST_F(PWindowTest, TC_gettimewindow_throw_by_read_001)
{
    EXPECT_CALL(*sysfs, open(_, _))
        .WillOnce(Return(1))
        .WillOnce(Return(2));
    EXPECT_CALL(*sysfs, close(Gt(0)))
        .Times(2);
    EXPECT_CALL(*sysfs, read(2, NotNull(), Gt(0)))
        .WillOnce(Return(-1));
    PWindow w(sysfs_path, 0, sysfs);
    ASSERT_THROW(w.get_time_window(), SystoolsdException);
}

TEST_F(PWindowTest, TC_setpthresh_001)
{
    EXPECT_CALL(*sysfs, open(_, _))
        .WillOnce(Return(1))
        .WillOnce(Return(2));
    EXPECT_CALL(*sysfs, close(Gt(0)))
        .Times(2);
    uint32_t thresh = 4444;
    EXPECT_CALL(*sysfs, write(1, thresh))
        .WillOnce(Return(4));
    PWindow w(sysfs_path, 0, sysfs);
    ASSERT_NO_THROW(w.set_pthresh(thresh));
}

TEST_F(PWindowTest, TC_setpthresh_throw_by_write)
{
    EXPECT_CALL(*sysfs, open(_, _))
        .WillOnce(Return(1))
        .WillOnce(Return(2));
    EXPECT_CALL(*sysfs, close(Gt(0)))
        .Times(2);
    uint32_t thresh = 4444;
    EXPECT_CALL(*sysfs, write(1, thresh))
        .WillOnce(Return(-1));
    PWindow w(sysfs_path, 0, sysfs);
    ASSERT_THROW(w.set_pthresh(thresh), SystoolsdException);
}

TEST_F(PWindowTest, TC_settimewindow_001)
{
    EXPECT_CALL(*sysfs, open(_, _))
        .WillOnce(Return(1))
        .WillOnce(Return(2));
    EXPECT_CALL(*sysfs, close(Gt(0)))
        .Times(2);
    uint32_t tw = 9999;
    EXPECT_CALL(*sysfs, write(2, tw))
        .WillOnce(Return(4));
    PWindow w(sysfs_path, 0, sysfs);
    ASSERT_NO_THROW(w.set_time_window(tw));
}

TEST_F(PWindowTest, TC_settimewindow_throw_by_write)
{
    EXPECT_CALL(*sysfs, open(_, _))
        .WillOnce(Return(1))
        .WillOnce(Return(2));
    EXPECT_CALL(*sysfs, close(Gt(0)))
        .Times(2);
    uint32_t tw = 9999;
    EXPECT_CALL(*sysfs, write(2, tw))
        .WillOnce(Return(-1));
    PWindow w(sysfs_path, 0, sysfs);
    ASSERT_THROW(w.set_time_window(tw), SystoolsdException);
}

