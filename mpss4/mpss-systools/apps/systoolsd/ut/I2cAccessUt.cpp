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

#include <iostream>
#include <thread>
#include <vector>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "I2cAccess.hpp"
#include "mocks.hpp"
#include "SystoolsdException.hpp"

using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::Ge;
using ::testing::Gt;
using ::testing::IsNull;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::ReturnArg;
using ::testing::SetArgPointee;
using ::testing::_;

namespace
{
    const uint8_t OPEN = 1 << 0;
    const uint8_t SET_SLAVE = 1 << 1;
    const uint8_t READ = 1 << 2;
    const uint8_t WRITE = 1 << 3;
    const uint8_t CLOSE = 1 << 4;

    const uint8_t BUS = 0;
    const uint8_t CMD = 0x60;
    const uint8_t SLAVE = 0xAB;
    const int EXPECTED_FD = 7;
    const char EXPECTED_BYTE = 'A';

    const uint32_t min_wait = 50;
}

//to be used in *restart_device* UTs.
//The function will sleep for a random amount of milliseconds, up to max_wait.
//Afterwards, i2c.is_device_busy() will be called, and the result should match
//expected_busy.
void access_device(I2cAccess &i2c, bool expected_busy, uint32_t max_wait)
{
    std::srand(std::time(NULL));
    usleep(((rand() % max_wait) + min_wait) * 1000);
    ASSERT_EQ(expected_busy, i2c.is_device_busy().is_busy);
}


//Test fixture for I2c mock tests
//It sets up the mock interface with the default behavior
class I2cAccessTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        mock = new NiceMock<MockI2cIo>();

        //default behavior for mocked functions
        ON_CALL(*mock, open_adapter(Ge(0)))
            .WillByDefault(Return(EXPECTED_FD));
        ON_CALL(*mock, set_slave_addr(Gt(0), Gt(0)))
            .WillByDefault(Return(0));
        ON_CALL(*mock, read(Gt(0), _, NotNull(), Gt(0)))
            .WillByDefault(DoAll(SetArgPointee<2>(EXPECTED_BYTE), ReturnArg<3>()));
        ON_CALL(*mock, write(Gt(0), _, NotNull(), Gt(0)))
            .WillByDefault(ReturnArg<3>());
        ON_CALL(*mock, close_adapter(Gt(0)))
            .WillByDefault(Return(0));
    }

    void install_expectations(uint8_t flags_call=0, uint8_t flags_no_call=0)
    {
        if(flags_call & OPEN)
            EXPECT_CALL(*mock, open_adapter(Ge(0)))
                .WillOnce(Return(EXPECTED_FD));

        if(flags_call & SET_SLAVE)
            EXPECT_CALL(*mock, set_slave_addr(Gt(0), Gt(0)))
                .Times(AtLeast(1));

        if(flags_call & READ)
            EXPECT_CALL(*mock, read(Gt(0), _, NotNull(), Gt(0)))
                .Times(AtLeast(1));

        if(flags_call & WRITE)
            EXPECT_CALL(*mock, write(Gt(0), _, NotNull(), Gt(0)))
                .Times(AtLeast(1));

        if(flags_call & CLOSE)
            EXPECT_CALL(*mock, close_adapter(Gt(0)))
                .WillOnce(Return(0));

        if(flags_no_call & OPEN)
            EXPECT_CALL(*mock, open_adapter(_))
                .Times(0);

        if(flags_no_call & SET_SLAVE)
            EXPECT_CALL(*mock, set_slave_addr(_, _))
                .Times(0);

        if(flags_no_call & READ)
            EXPECT_CALL(*mock, read(_, _, _, _))
                .Times(0);

        if(flags_no_call & WRITE)
            EXPECT_CALL(*mock, write(_, _, _, _))
                .Times(0);

        if(flags_no_call & CLOSE)
            EXPECT_CALL(*mock, close_adapter(_))
                .Times(0);

    }

    MockI2cIo *mock;
};

TEST_F(I2cAccessTest, TC_ctor_001)
{
    install_expectations();
    ASSERT_NO_THROW(I2cAccess i2c(mock, BUS));
}

TEST_F(I2cAccessTest, TC_read_throw_by_null_001)
{
    //expect that read() never gets called
    install_expectations(0, READ);
    I2cAccess i2c(mock, BUS);
    ASSERT_THROW(i2c.read_bytes(CMD, NULL, (size_t)1), std::invalid_argument);
}

TEST_F(I2cAccessTest, TC_read_throw_by_slave_001)
{
    const size_t size = 4;
    char buf[size] = {0};
    install_expectations(OPEN|CLOSE, READ);
    EXPECT_CALL(*mock, set_slave_addr(Gt(0), Gt(0)))
        .WillOnce(Return(-1));
    I2cAccess i2c(mock, BUS);
    ASSERT_THROW(i2c.read_bytes(CMD, buf, size), SystoolsdException);
}

TEST_F(I2cAccessTest, TC_read_throw_by_read_001)
{
    const size_t size = 4;
    char buf[size] = {0};
    install_expectations(OPEN|CLOSE|SET_SLAVE);
    EXPECT_CALL(*mock, read(Gt(0), _, NotNull(), Gt(0)))
        .WillOnce(Return(-1));
    I2cAccess i2c(mock, BUS);
    ASSERT_THROW(i2c.read_bytes(CMD, buf, size), SystoolsdException);
}

TEST_F(I2cAccessTest, TC_read_001)
{
    const size_t size = 1;
    char buf = '\0';
    install_expectations(OPEN|CLOSE|SET_SLAVE|READ);
    I2cAccess i2c(mock, BUS);
    ASSERT_NO_THROW(i2c.read_bytes(CMD, &buf, size));
    ASSERT_EQ(EXPECTED_BYTE, buf);
}

TEST_F(I2cAccessTest, TC_write_throw_by_null_001)
{
    install_expectations();
    I2cAccess i2c(mock, BUS);
    ASSERT_THROW(i2c.write_bytes(CMD, NULL, (size_t)1), std::invalid_argument);
}

TEST_F(I2cAccessTest, TC_write_throw_by_slave_001)
{
    const size_t size = 4;
    char buf[size] = {0};
    install_expectations(OPEN|CLOSE, WRITE);
    EXPECT_CALL(*mock, set_slave_addr(Gt(0), Gt(0)))
        .WillOnce(Return(-1));
    I2cAccess i2c(mock, BUS);
    ASSERT_THROW(i2c.write_bytes(CMD, buf, size), SystoolsdException);
}

TEST_F(I2cAccessTest, TC_write_throw_by_sendcmd_001)
{
    const size_t size = 4;
    char buf[size] = {0};
    install_expectations(OPEN|CLOSE|SET_SLAVE);
    //send_cmd interally uses write()
    EXPECT_CALL(*mock, write(Gt(0), _, NotNull(), Gt(0)))
        .Times(1)
        .WillOnce(Return(-1));
    I2cAccess i2c(mock, BUS);
    ASSERT_THROW(i2c.write_bytes(CMD, buf, size), SystoolsdException);
}

TEST_F(I2cAccessTest, TC_write_throw_by_write_001)
{
    const size_t size = 4;
    char buf[size] = {0};
    install_expectations(OPEN|CLOSE|SET_SLAVE);
    //First call to write() (called from send_cmd) succeeds,
    //second call (the actual call to write data) fails.
    EXPECT_CALL(*mock, write(Gt(0), _, NotNull(), Gt(0)))
        .WillOnce(Return(-1));
    I2cAccess i2c(mock, BUS);
    ASSERT_THROW(i2c.write_bytes(CMD, buf, size), SystoolsdException);
}

TEST_F(I2cAccessTest, TC_write_001)
{
    const size_t size = 4;
    char buf[size] = {0};
    install_expectations(OPEN|CLOSE|SET_SLAVE|WRITE);
    I2cAccess i2c(mock, BUS);
    ASSERT_NO_THROW(i2c.write_bytes(CMD, buf, size));
}

TEST_F(I2cAccessTest, TC_readu32_001)
{
    install_expectations(OPEN|CLOSE|SET_SLAVE|READ);
    I2cAccess i2c(mock, BUS);
    ASSERT_NO_THROW(i2c.read_u32(CMD));
}

TEST_F(I2cAccessTest, TC_writeu32_001)
{
    uint32_t buf = 1;
    install_expectations(OPEN|CLOSE|SET_SLAVE|WRITE, READ);
    I2cAccess i2c(mock, BUS);
    ASSERT_NO_THROW(i2c.write_u32(CMD, buf));
}

TEST_F(I2cAccessTest, TC_buftouint_throw_by_null_001)
{
    I2cAccess i2c(mock, BUS);
    ASSERT_THROW(i2c.buf_to_uint(NULL, 1), std::invalid_argument);
}

TEST_F(I2cAccessTest, TC_buftouint_throw_by_length_001)
{
    I2cAccess i2c(mock, BUS);
    char c = '\0';
    ASSERT_THROW(i2c.buf_to_uint(&c, 9), std::invalid_argument);
}

TEST_F(I2cAccessTest, TC_buftouint_001)
{
    I2cAccess i2c(mock, BUS);
    char c = '\0';
    ASSERT_NO_THROW(i2c.buf_to_uint(&c, 1));
}

TEST_F(I2cAccessTest, TC_restart_device_001)
{
    I2cAccess i2c(mock, BUS);
    i2c.set_wait_time(300);
    ASSERT_FALSE(i2c.is_device_busy().is_busy);
    ASSERT_NO_THROW(i2c.restart_device(0));
    ASSERT_TRUE(i2c.is_device_busy().is_busy);
    usleep(300 * 1000);
    ASSERT_FALSE(i2c.is_device_busy().is_busy);

}

TEST_F(I2cAccessTest, TC_restart_device_restart_in_progress_001)
{
    I2cAccess i2c(mock, BUS);
    ASSERT_FALSE(i2c.is_device_busy().is_busy);
    ASSERT_NO_THROW(i2c.restart_device(0xa));
    ASSERT_THROW(i2c.restart_device(0xa), SystoolsdException);
}

TEST_F(I2cAccessTest, TC_restart_device_mthread_001)
{
    //create threads
    std::vector<std::thread> threads;
    const int nthreads = 5;
    //500 ms wait
    uint32_t wait = 500;
    I2cAccess i2c(mock, BUS);
    i2c.set_wait_time(wait);
    ASSERT_FALSE(i2c.is_device_busy().is_busy);
    ASSERT_NO_THROW(i2c.restart_device(0xa));
    for(int i = 0; i < nthreads; ++i)
    {
        //launch access_device() in various threads. Expect all to be busy.
        threads.push_back(std::thread(access_device, std::ref(i2c), true, wait - 100));
    }
    for(auto t = threads.begin(); t != threads.end(); ++t)
    {
        t->join();
    }
    usleep(wait * 1000);
    ASSERT_FALSE(i2c.is_device_busy().is_busy);
}

TEST_F(I2cAccessTest, TC_restart_device_attempt_read_001)
{
    I2cAccess i2c(mock, BUS);
    ASSERT_FALSE(i2c.is_device_busy().is_busy);
    i2c.set_wait_time(500);
    ASSERT_NO_THROW(i2c.restart_device(0xa));
    ASSERT_THROW(i2c.read_u32(2), SystoolsdException);
}

TEST_F(I2cAccessTest, TC_restart_device_attempt_write_001)
{
    I2cAccess i2c(mock, BUS);
    ASSERT_FALSE(i2c.is_device_busy().is_busy);
    i2c.set_wait_time(500);
    ASSERT_NO_THROW(i2c.restart_device(0xa));
    ASSERT_THROW(i2c.write_u32(2, 3), SystoolsdException);
}
