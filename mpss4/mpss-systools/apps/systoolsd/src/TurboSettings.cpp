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
#include <sstream>
#include <stdexcept>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "FileInterface.hpp"
#include "SystoolsdException.hpp"
#include "TurboSettings.hpp"

#include "daemonlog.h"

namespace
{
    const std::string intel_pstate = "/sys/devices/system/cpu/intel_pstate/";
    const std::string no_turbo_path = intel_pstate + "no_turbo";
    const std::string turbo_pct_path = intel_pstate + "turbo_pct";
}

//This class will not open the sysfs files in an RAII manner.
//Instead, the files will be opened and closed in the corresponding
//calls. The reason for this is the lifetime of the TurboSettings
//class.
TurboSettings::TurboSettings() : sysfs(new File)
{
}

TurboSettings::~TurboSettings()
{
    //nothing to do (anymore)
}

bool TurboSettings::is_turbo_enabled() const
{
    uint8_t mode = read(no_turbo_path);
    //if no_turbo is 1 it is disabled
    return (mode == 0 ? true : false);
}

uint8_t TurboSettings::get_turbo_pct() const
{
    return read(turbo_pct_path);
}

void TurboSettings::set_turbo_enabled(bool enabled)
{
    //writing to no_turbo: 1 means disabled
    char c = (enabled ? '0' : '1');
    int fd;
    if((fd = sysfs->open(no_turbo_path, O_WRONLY)) == -1)
    {
        std::string msg = "open: " + no_turbo_path;
        throw SystoolsdException(SYSTOOLSD_IO_ERROR, msg);
    }

    if(sysfs->write(fd, &c, 1) < 1)
    {
        sysfs->close(fd);
        std::string msg = "write: " + no_turbo_path;
        throw SystoolsdException(SYSTOOLSD_IO_ERROR, msg);
    }

    sysfs->close(fd);
}

void TurboSettings::set_file_interface(FileInterface *file)
{
    if(!file)
    {
        throw std::invalid_argument("NULL FileInterface");
    }

    sysfs.reset(file);
}

uint8_t TurboSettings::read(const std::string &path) const
{
    const size_t size = 32;
    char buf[size] = {0};
    bzero(buf, size);
    int fd;
    if((fd = sysfs->open(path, O_RDONLY)) == -1)
    {
        std::string msg = "open: " + path;
        throw SystoolsdException(SYSTOOLSD_IO_ERROR, msg);
    }

    if(sysfs->read(fd, buf, size) == -1)
    {
        sysfs->close(fd);
        std::string msg = "read: " + path;
        throw SystoolsdException(SYSTOOLSD_IO_ERROR, msg);
    }

    sysfs->close(fd);

    std::stringstream ss;
    ss << buf;
    uint16_t value;
    ss >> value;
    return static_cast<uint8_t>(value);
}
