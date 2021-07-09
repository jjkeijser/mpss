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

#include <chrono>
#include <cstring>
#include <mutex>
#include <sstream>
#include <stdexcept>

#include "I2cAccess.hpp"
#include "I2cBase.hpp"
#include "I2cIo.hpp"
#include "SystoolsdException.hpp"

#include "systoolsd_api.h"

using std::chrono::high_resolution_clock;
using std::chrono::duration_cast;

namespace
{
    //TODO: double check these values
    const uint8_t write_slave = 0x28;
    const uint8_t read_slave = 0x28;
}

I2cAccess::I2cAccess(I2cIo *impl, uint8_t adapter) :
    I2cBase(), i2c_adapter(adapter), i2c_impl(impl)
{
    set_valid(true);
}

I2cAccess::~I2cAccess()
{
}

void I2cAccess::read_bytes_(uint8_t smc_cmd, char *buf, size_t length)
{
    if(!buf)
        throw std::invalid_argument("NULL buf pointer");

    const size_t size = 32;
    char contents[size]; //smbus protocol allows at most 32 bytes
    memset(contents, 0, size);

    if(length > size)
        length = size;

    int i2c_fd = i2c_impl->open_adapter(i2c_adapter);
    if(i2c_fd < 0)
    {
        std::stringstream ss;
        ss << "failed opening adapter " << (uint32_t)i2c_adapter;
        throw SystoolsdException(SYSTOOLSD_SMC_ERROR, ss.str().c_str());
    }

    try
    {
        set_slave_addr(i2c_fd, read_slave);

        if(i2c_impl->read(i2c_fd, smc_cmd, contents, length) < 0)
        {
            std::stringstream errmsg;
            errmsg << "error reading SMC response for command " << smc_cmd;
            throw SystoolsdException(SYSTOOLSD_SMC_ERROR, errmsg.str().c_str());
        }
    }
    catch(...)
    {
        i2c_impl->close_adapter(i2c_fd);
        throw;
    }

    i2c_impl->close_adapter(i2c_fd);
    memcpy(buf, contents, length);
}

void I2cAccess::write_bytes_(uint8_t smc_cmd, char *buf, size_t length)
{
    if(!buf)
        throw std::invalid_argument("NULL buf pointer");

    int i2c_fd = i2c_impl->open_adapter(i2c_adapter);
    if(i2c_fd < 0)
    {
        std::stringstream ss;
        ss << "failed opening adapter " << (uint32_t)i2c_adapter;
        throw SystoolsdException(SYSTOOLSD_SMC_ERROR, ss.str().c_str());
    }

    try
    {
        set_slave_addr(i2c_fd, write_slave);

        //Expect all bytes to be written
        if(i2c_impl->write(i2c_fd, smc_cmd, buf, length) == -1)
        {
            std::stringstream errmsg;
            errmsg << "error reading SMC response for command " << smc_cmd;
            throw SystoolsdException(SYSTOOLSD_SMC_ERROR, errmsg.str().c_str());
        }
        i2c_impl->close_adapter(i2c_fd);
    }
    catch(...)
    {
        i2c_impl->close_adapter(i2c_fd);
        throw;
    }
}

uint32_t I2cAccess::read_u32_(uint8_t smc_cmd)
{
    const size_t size = 4;
    char buf[size];
    memset(buf, 0, size);

    read_bytes_(smc_cmd, buf, (size_t)size);
    return (uint32_t)buf_to_uint(buf, (size_t)size);
}

void I2cAccess::write_u32_(uint8_t smc_cmd, uint32_t val)
{
    write_bytes_(smc_cmd, (char*)&val, sizeof(val));
}

uint64_t I2cAccess::buf_to_uint(char *buf, size_t length)
{
    //Assuming little endian
    if(length > 8)
    {
        throw std::invalid_argument("buffer length > 8 will overflow");
    }

    if(!buf)
    {
        throw std::invalid_argument("NULL buf pointer");
    }

    const size_t bufsize = 8;
    //assuming byte == 8 bits
    union{
        uint64_t sum;
        char buf[bufsize];
    } u;

    bzero(u.buf, bufsize);
    memcpy(u.buf, buf, length);

    return u.sum;
}

void I2cAccess::send_cmd(uint8_t cmd)
{
    // TODO: remove function
    (void)cmd;
    return;
}

void I2cAccess::set_slave_addr(int i2c_fd, long which_slave)
{
    if(i2c_impl->set_slave_addr(i2c_fd, which_slave) < 0)
    {
        std::stringstream errmsg;
        errmsg << "failed setting slave address " << which_slave << " for adapater " << i2c_adapter;
        throw SystoolsdException(SYSTOOLSD_SMC_ERROR, errmsg.str().c_str());
    }
}

