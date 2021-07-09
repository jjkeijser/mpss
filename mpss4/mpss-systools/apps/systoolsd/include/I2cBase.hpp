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

#ifndef SYSTOOLS_SYSTOOLSD_I2CBASE_HPP_
#define SYSTOOLS_SYSTOOLSD_I2CBASE_HPP_

#include <chrono>
#include <mutex>

#ifdef UNIT_TESTS
#define PRIVATE public
#else
#define PRIVATE private
#endif

struct BusyInfo
{
    bool is_busy;
    uint32_t elapsed_busy;
    BusyInfo(bool is_busy_, uint32_t elapsed_busy_) :
        is_busy(is_busy_), elapsed_busy(elapsed_busy_){ }
};

class I2cBase
{
public:
    I2cBase();
    virtual ~I2cBase() { };
    //these public methods only to be overridden by unit test classes
    //they provide the locking and restart mechanisms
    virtual void read_bytes(uint8_t smc_cmd, char *buf, size_t length);
    virtual void write_bytes(uint8_t smc_cmd, char *buf, size_t length);
    virtual uint32_t read_u32(uint8_t smc_cmd);
    virtual void write_u32(uint8_t smc_cmd, uint32_t val);
    virtual void restart_device(uint8_t addr);
    virtual BusyInfo is_device_busy();
    virtual uint32_t time_busy();

protected:
    //Deriving classes must call set_valid(true) once they have been constructed,
    //or else every call to the public interface defined by I2cBase will throw
    //and exception.
    bool is_valid() const;
    void is_valid_or_die() const;
    void set_valid(bool valid);

protected:
    virtual void read_bytes_(uint8_t smc_cmd, char *buf, size_t length) = 0;
    virtual void write_bytes_(uint8_t smc_cmd, char *buf, size_t length) = 0;
    virtual uint32_t read_u32_(uint8_t smc_cmd) = 0;
    virtual void write_u32_(uint8_t smc_cmd, uint32_t val) = 0;

PRIVATE:
    void set_wait_time(uint32_t ms);
    std::mutex i2cbus_mutex;
    std::mutex device_busy_mutex;
    std::chrono::high_resolution_clock::time_point device_busy_timer;
    void set_device_busy(uint8_t addr);
    bool busy;
    uint32_t smba_wait;
    bool valid;
};

#endif
