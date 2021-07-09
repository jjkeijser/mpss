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

#ifndef _SYSTOOLS_SYSTOOLSD_SMCACCESS_HPP_
#define _SYSTOOLS_SYSTOOLSD_SMCACCESS_HPP_

#include <chrono>
#include <mutex>
#include <thread>
#include <utility>

#include "I2cBase.hpp"

#ifdef UNIT_TESTS
#define PRIVATE public
#else
#define PRIVATE private
#endif

class I2cIo;


class I2cAccess : public I2cBase
{
public:
    I2cAccess(I2cIo *impl, uint8_t adapter);
    ~I2cAccess();

protected:
    void read_bytes_(uint8_t smc_cmd, char *buf, size_t length);
    void write_bytes_(uint8_t smc_cmd, char *buf, size_t length);
    uint32_t read_u32_(uint8_t smc_cmd);
    void write_u32_(uint8_t smc_cmd, uint32_t val);

PRIVATE:
    uint64_t buf_to_uint(char *buf, size_t length);
    //for unit testing purposes

private:
    I2cAccess(const I2cAccess&);
    I2cAccess &operator=(I2cAccess);
    void send_cmd(uint8_t cmd);
    void set_slave_addr(int fd, long which_slave);

    uint8_t i2c_adapter;
    std::shared_ptr<I2cIo> i2c_impl;
};
#endif //_SYSTOOLS_SYSTOOLSD_SMCACCESS_HPP_
