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

#include <sstream>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "I2cToolsImpl.hpp"

#include "../../../3rd-party/linux/i2c-dev.h"

int I2cToolsImpl::open_adapter(uint8_t adapter)
{
    std::stringstream s;
    s << "/dev/i2c-" << (uint32_t)adapter;
    return open(s.str().c_str(), O_RDWR);
}

int I2cToolsImpl::set_slave_addr(int fd, long which_slave)
{
    return ioctl(fd, I2C_SLAVE, which_slave);
}

int I2cToolsImpl::read(int fd, uint8_t command, char *buf, size_t count)
{
    return i2c_smbus_read_i2c_block_data(fd, command, count, reinterpret_cast<__u8*>(buf));
}

int I2cToolsImpl::write(int fd, uint8_t command, char *buf, size_t count)
{
    return i2c_smbus_write_i2c_block_data(fd, command, count, reinterpret_cast<__u8*>(buf));
}

int I2cToolsImpl::close_adapter(int adapter)
{
    return ::close(adapter);
}

