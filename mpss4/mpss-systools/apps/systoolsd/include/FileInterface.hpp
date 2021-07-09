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

#ifndef SYSTOOLS_SYSTOOLSD_FILEINTERFACE_HPP
#define SYSTOOLS_SYSTOOLSD_FILEINTERFACE_HPP

#include <string>

class FileInterface
{
public:
    virtual ~FileInterface() { };
    virtual int open(const std::string &path, int flags) = 0;
    virtual void close(int fd) = 0;
    virtual ssize_t read(int fd, char *const buf, size_t size) = 0;
    virtual ssize_t write(int fd, const char *buf, size_t size) = 0;
    virtual ssize_t write(int fd, int data) = 0;
    virtual off_t lseek(int fd, off_t offset, int whence) = 0;
    virtual void rewind(int fd) = 0;
};

class File : public FileInterface
{
public:
    virtual int open(const std::string &path, int flags);
    virtual void close(int fd);
    virtual ssize_t read(int fd, char *const buf, size_t size);
    virtual ssize_t write(int fd, const char *buf, size_t size);
    virtual ssize_t write(int fd, int data);
    virtual off_t lseek(int fd, off_t offset, int whence);
    virtual void rewind(int fd);
};

#endif //SYSTOOLS_SYSTOOLSD_FILEINTERFACE_HPP
