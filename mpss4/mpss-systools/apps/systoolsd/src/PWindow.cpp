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

#include <cstdlib>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <string>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "PWindow.hpp"
#include "FileInterface.hpp"
#include "SystoolsdException.hpp"

#include "systoolsd_api.h"

PWindow::PWindow(std::string rapl_zone, uint8_t window, FileInterface *impl) :
    sysfs(0), pthresh_fd(0), window_fd(0), window_nr(window), sysfs_path(rapl_zone),
    pthresh_path(sysfs_path + "/constraint_" +
                 std::to_string((long long int)window_nr) +
                 "_power_limit_uw"),
    time_window_path(sysfs_path + "/constraint_" +
                std::to_string((long long int)window_nr) +
                "_time_window_us")
{
    init(impl);
}

PWindow::PWindow(std::string rapl_zone, uint8_t window) :
    sysfs(0), pthresh_fd(0), window_fd(0), window_nr(window), sysfs_path(rapl_zone),
    pthresh_path(sysfs_path + "/constraint_" +
                 std::to_string((long long int)window_nr) +
                 "_power_limit_uw"),
    time_window_path(sysfs_path + "/constraint_" +
            std::to_string((long long int)window_nr) +
            "_time_window_us")
{
    init(new File);
}

void PWindow::init(FileInterface *impl)
{
    sysfs = impl;
    pthresh_fd = sysfs->open(pthresh_path, O_RDWR);
    if(pthresh_fd == -1)
    {
        delete sysfs;
        sysfs = NULL;
        throw SystoolsdException(SYSTOOLSD_IO_ERROR, pthresh_path.c_str());
    }
    window_fd = sysfs->open(time_window_path, O_RDWR);
    if(window_fd == -1)
    {
        sysfs->close(pthresh_fd);
        delete sysfs;
        sysfs = NULL;
        throw SystoolsdException(SYSTOOLSD_IO_ERROR, time_window_path.c_str());
    }
}

PWindow::~PWindow()
{
    sysfs->close(pthresh_fd);
    sysfs->close(window_fd);
    delete sysfs;
    sysfs = NULL;
}

PowerWindowInfo PWindow::get_window_info() const
{
    PowerWindowInfo info;
    info.threshold = get_pthresh();
    info.time_window = get_time_window();
    return info;
}

uint32_t PWindow::get_pthresh() const
{
    uint32_t pthresh = 0;
    const size_t size = 80;
    char buf[size];
    bzero(buf, size);
    sysfs->rewind(pthresh_fd);
    if(sysfs->read(pthresh_fd, buf, size) == -1)
    {
        throw SystoolsdException(SYSTOOLSD_IO_ERROR, "failed reading pthresh");
    }
    std::stringstream(buf) >> pthresh;
    return pthresh;
}

uint32_t PWindow::get_time_window() const
{
    uint32_t window = 0;
    const size_t size = 80;
    char buf[size];
    bzero(buf, size);
    sysfs->rewind(window_fd);
    if(sysfs->read(window_fd, buf, size) == -1)
    {
        throw SystoolsdException(SYSTOOLSD_IO_ERROR, "failed reading time window");
    }
    std::stringstream(buf) >> window;
    return window;
}

void PWindow::set_pthresh(uint32_t pthresh_uw)
{
    if(sysfs->write(pthresh_fd, pthresh_uw) <= 0)
    {
        throw SystoolsdException(SYSTOOLSD_IO_ERROR, "failed setting pthresh");
    }
}

void PWindow::set_time_window(uint32_t window_us)
{
    if(sysfs->write(window_fd, window_us) <= 0)
    {
        throw SystoolsdException(SYSTOOLSD_IO_ERROR, "failed setting time windows");
    }
}
