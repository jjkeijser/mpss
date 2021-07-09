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

#include <cstring>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "PThresh.hpp"
#include "FileInterface.hpp"
#include "SystoolsdException.hpp"

#include "daemonlog.h"
#include "systoolsd_api.h"

namespace
{
    const std::string sysfs_path = "/sys/devices/virtual/powercap/intel-rapl/intel-rapl:0";
    const std::string max_power_path = sysfs_path + "/max_power_range_uw";
    const std::string low_pthresh_path = sysfs_path + "/constraint_0_power_limit_uw";
    const std::string hi_pthresh_path = sysfs_path + "/constraint_1_power_limit_uw";
}

//This class will not open the sysfs files in an RAII manner.
//Instead, the files will be opened and closed in the corresponding
//calls. The reason for this is the lifetime of the PThresh
//class.
PThresh::PThresh() : sysfs(new File)
{
    //nothing to do
}

PThresh::~PThresh()
{
    //nothing to do
}

uint32_t PThresh::get_max_phys_pwr() const
{
    //unsupported value
    return 0;
}

uint32_t PThresh::get_low_pthresh() const
{
    uint32_t low_pthresh = 0;
    const size_t size = 80;
    char buf[size];
    bzero(buf, size);
    int fd;
    if((fd = sysfs->open(low_pthresh_path, O_RDONLY)) == -1)
    {
        std::string msg = "open: " + low_pthresh_path;
        throw SystoolsdException(SYSTOOLSD_IO_ERROR, msg.c_str());
    }

    if(sysfs->read(fd, buf, size) == -1)
    {
        sysfs->close(fd);
        std::string msg = "read: " + low_pthresh_path;
        throw SystoolsdException(SYSTOOLSD_IO_ERROR, msg.c_str());
    }
    sysfs->close(fd);
    std::stringstream(buf) >> low_pthresh;
    return low_pthresh;
}

uint32_t PThresh::get_high_pthresh() const
{
    uint32_t hi_pthresh = 0;
    const size_t size = 80;
    char buf[size];
    bzero(buf, size);
    int fd;
    if((fd = sysfs->open(hi_pthresh_path, O_RDONLY)) == -1)
    {
        std::string msg = "open: " + hi_pthresh_path;
        throw SystoolsdException(SYSTOOLSD_IO_ERROR, msg.c_str());
    }

    if(sysfs->read(fd, buf, size) == -1)
    {
        sysfs->close(fd);
        std::string msg = "read: " + hi_pthresh_path;
        throw SystoolsdException(SYSTOOLSD_IO_ERROR, msg.c_str());
    }

    sysfs->close(fd);
    std::stringstream(buf) >> hi_pthresh;
    return hi_pthresh;
}

void PThresh::set_file_interface(FileInterface *file)
{
    if(!file)
    {
        throw std::invalid_argument("NULL FileInterface");
    }

    sysfs.reset(file);
}

std::shared_ptr<PWindowInterface> PThresh::window_factory(uint8_t window) const
{
    switch(window)
    {
        case 0:
        case 1:
            return std::static_pointer_cast<PWindowInterface>(std::make_shared<PWindow>(sysfs_path, window));
        default:
            throw std::invalid_argument("invalid window");
    }
}

