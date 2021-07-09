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

#include "I2cBase.hpp"
#include "SystoolsdException.hpp"

#include "systoolsd_api.h"

using std::chrono::high_resolution_clock;
using std::chrono::duration_cast;

namespace
{
    const uint8_t SMBA_RESTART_REG = 0x17;
}

I2cBase::I2cBase() : busy(false), smba_wait(SMBA_RESTART_WAIT_MS), valid(false)
{

}


void I2cBase::read_bytes(uint8_t smc_cmd, char *buf, size_t length)
{
    if(!buf)
        throw std::invalid_argument("NULL buf pointer");

    is_valid_or_die();

    if(is_device_busy().is_busy)
        throw SystoolsdException(SYSTOOLSD_DEVICE_BUSY, "device busy");


    std::lock_guard<std::mutex> l(i2cbus_mutex);
    read_bytes_(smc_cmd, buf, length);
}

void I2cBase::write_bytes(uint8_t smc_cmd, char *buf, size_t length)
{
    if(!buf)
        throw std::invalid_argument("NULL buf pointer");

    is_valid_or_die();

    if(is_device_busy().is_busy)
        throw SystoolsdException(SYSTOOLSD_DEVICE_BUSY, "device busy");

    std::lock_guard<std::mutex> l(i2cbus_mutex);
    write_bytes_(smc_cmd, buf, length);
}

uint32_t I2cBase::read_u32(uint8_t smc_cmd)
{
    is_valid_or_die();

    if(is_device_busy().is_busy)
        throw SystoolsdException(SYSTOOLSD_DEVICE_BUSY, "device busy");

    std::lock_guard<std::mutex> l(i2cbus_mutex);
    return read_u32_(smc_cmd);
}

void I2cBase::write_u32(uint8_t smc_cmd, uint32_t val)
{
    is_valid_or_die();

    if(is_device_busy().is_busy)
        throw SystoolsdException(SYSTOOLSD_DEVICE_BUSY, "device busy");

    std::lock_guard<std::mutex> l(i2cbus_mutex);
    write_u32_(smc_cmd, val);
}

void I2cBase::restart_device(uint8_t addr)
{
    is_valid_or_die();
    set_device_busy(addr);
}

uint32_t I2cBase::time_busy()
{
    is_valid_or_die();
    high_resolution_clock::time_point t = high_resolution_clock::now();
    int32_t elapsed = duration_cast<std::chrono::milliseconds>(t - device_busy_timer).count();
    return static_cast<uint32_t>(elapsed);
}

bool I2cBase::is_valid() const
{
    return valid;
}

void I2cBase::is_valid_or_die() const
{
    if(!is_valid())
    {
        throw SystoolsdException(SYSTOOLSD_IO_ERROR, "no i2c access");
    }
}

void I2cBase::set_valid(bool valid_)
{
    valid = valid_;
}

void I2cBase::set_device_busy(uint8_t addr)
{
    is_valid_or_die();
    std::lock_guard<std::mutex> l(device_busy_mutex);
    if(busy)
        throw SystoolsdException(SYSTOOLSD_DEVICE_BUSY, "restart in progress");
    write_u32_(SMBA_RESTART_REG, addr);
    busy = true;
    device_busy_timer = high_resolution_clock::now();
}

BusyInfo I2cBase::is_device_busy()
{
    std::lock_guard<std::mutex> l(device_busy_mutex);
    if(busy)
    {
        //SMBA_RESTART_WAIT_MS in systoolsd_api, 5000 ms
        auto elapsed = time_busy();
        if(elapsed < smba_wait)
            return BusyInfo(true, elapsed);
        //device no longer busy!
        busy = false;
        return BusyInfo(busy, 0);
    }
    else
        return BusyInfo(busy, 0);
}

void I2cBase::set_wait_time(uint32_t ms)
{
    smba_wait = ms;
}

