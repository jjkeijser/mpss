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
#include "info/PowerUsageInfoGroup.hpp"
#include "smbios/SmBiosInfo.hpp"

using std::string;
using std::ifstream;

namespace
{
    const uint8_t pwr_pcie = 0x28;
    const uint8_t pwr_2x3 = 0x29;
    const uint8_t pwr_2x4 = 0x2A;
    const uint8_t force_throttle = 0x2B;
    const uint8_t avg_power_0 = 0x35;
    const uint8_t inst_power = 0x3A;
    const uint8_t inst_power_max = 0x3B;
    const uint8_t power_vccp = 0x70;
    const uint8_t power_vccu = 0x71;
    const uint8_t power_vccclr = 0x72;
    const uint8_t power_vccmlb = 0x73;
    const uint8_t power_vccmp = 0x76;
    const uint8_t power_ntb1 = 0x77;
}


PowerUsageInfoGroup::PowerUsageInfoGroup(Services::ptr &services) :
    CachedDataGroupBase<PowerUsageInfo>(300), i2c(NULL)
{
    if(!(i2c = services->get_i2c_srv()))
        throw std::invalid_argument("NULL DaemonInfoSources->i2c");
}

void PowerUsageInfoGroup::refresh_data()
{
    data.pwr_pcie = i2c->read_u32(pwr_pcie);
    data.pwr_2x3 = i2c->read_u32(pwr_2x3);
    data.pwr_2x4 = i2c->read_u32(pwr_2x4);
    data.force_throttle = i2c->read_u32(force_throttle);
    data.avg_power_0 = i2c->read_u32(avg_power_0);
    data.inst_power = i2c->read_u32(inst_power);
    data.inst_power_max = i2c->read_u32(inst_power_max);
    data.power_vccp = i2c->read_u32(power_vccp);
    data.power_vccu = i2c->read_u32(power_vccu);
    data.power_vccclr = i2c->read_u32(power_vccclr);
    data.power_vccmlb = i2c->read_u32(power_vccmlb);
    data.power_vccd012 = 0; // Deprecated
    data.power_vccd345 = 0; // Deprecated
    data.power_vccmp = i2c->read_u32(power_vccmp);
    data.power_ntb1 = i2c->read_u32(power_ntb1);
}

