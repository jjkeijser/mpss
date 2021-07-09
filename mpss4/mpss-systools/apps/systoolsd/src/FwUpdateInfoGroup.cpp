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

#include "Daemon.hpp"
#include "I2cAccess.hpp"
#include "info/FwUpdateInfoGroup.hpp"

using std::string;
using std::ifstream;

namespace
{
    const uint8_t fwu_sts = 0xE1;
    const uint8_t fwu_cmd = 0xE2;
}


FwUpdateInfoGroup::FwUpdateInfoGroup(Services::ptr &services) :
    CachedDataGroupBase<FwUpdateInfo>(300), i2c(NULL)
{
    if(!(i2c = services->get_i2c_srv()))
        throw std::invalid_argument("NULL DaemonInfoSources->i2c");
}

void FwUpdateInfoGroup::refresh_data()
{
    data.fwu_sts = i2c->read_u32(fwu_sts);
    data.fwu_cmd = i2c->read_u32(fwu_cmd);
}

