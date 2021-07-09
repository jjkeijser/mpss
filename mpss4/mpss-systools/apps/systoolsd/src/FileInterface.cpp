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

#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "FileInterface.hpp"
#include "SystoolsdException.hpp"

int File::open(const std::string &path, int flags)
{
    return ::open(path.c_str(), flags);
}

void File::close(int fd)
{
    ::close(fd);
}

ssize_t File::read(int fd, char *const buf, size_t size)
{
    if(!buf || size == 0)
        throw std::invalid_argument("NULL buf pointer/0-size buffer");

    return ::read(fd, reinterpret_cast<void*>(buf), size);
}

ssize_t File::write(int fd, const char * const buf, size_t size)
{
    if(!buf || size == 0)
        throw std::invalid_argument("NULL buf pointer/0-size buffer");

    return ::write(fd, reinterpret_cast<const void*>(buf), size);
}

ssize_t File::write(int fd, int data)
{
    const size_t size = 80;
    char buf[size];
    bzero(buf, size);
    snprintf(buf, size, "%d", data);
    return ::write(fd, reinterpret_cast<void*>(buf), strlen(buf));
}

off_t File::lseek(int fd, off_t offset, int whence)
{
    return ::lseek(fd, offset, whence);
}

void File::rewind(int fd)
{
    if(lseek(fd, 0, SEEK_SET) == -1)
        throw SystoolsdException(SYSTOOLSD_IO_ERROR, "rewind");
}
