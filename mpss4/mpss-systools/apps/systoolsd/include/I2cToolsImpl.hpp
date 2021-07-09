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

#ifndef SYSTOOLS_SYSTOOLSD_I2CTOOLSIMPL_HPP
#define SYSTOOLS_SYSTOOLSD_I2CTOOLSIMPL_HPP

#include "I2cIo.hpp"

class I2cToolsImpl : public I2cIo
{
public:
    virtual int open_adapter(uint8_t adapter);
    virtual int set_slave_addr(int fd, long which_slave);
    virtual int read(int fd, uint8_t command, char *buf, size_t count);
    virtual int write(int fd, uint8_t command, char *buf, size_t count);
    virtual int close_adapter(int adapter);
};

#endif

