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
#include "info/VoltageInfoGroup.hpp"
#include "I2cAccess.hpp"

using std::string;
using std::ifstream;

namespace
{
    const uint8_t voltage_vccp = 0x50;
    const uint8_t voltage_vccu = 0x51;
    const uint8_t voltage_vccclr = 0x52;
    const uint8_t voltage_vccmlb = 0x53;
    const uint8_t voltage_vccmp = 0x56;
    const uint8_t voltage_ntb1 = 0x57;
    const uint8_t voltage_vccpio = 0x58;
    const uint8_t voltage_vccsfr = 0x59;
    const uint8_t voltage_pch = 0x5A;
    const uint8_t voltage_vccmfuse = 0x5B;
    const uint8_t voltage_ntb2 = 0x5C;
    const uint8_t voltage_vpp = 0x5D;
}


VoltageInfoGroup::VoltageInfoGroup(Services::ptr &services) :
    CachedDataGroupBase<VoltageInfo>(300), i2c(NULL)
{
    if(!(i2c = services->get_i2c_srv()))
        throw std::invalid_argument("NULL DaemonInfoSources->i2c");
}

void VoltageInfoGroup::refresh_data()
{
    data.voltage_vccp = i2c->read_u32(voltage_vccp);
    data.voltage_vccu = i2c->read_u32(voltage_vccu);
    data.voltage_vccclr = i2c->read_u32(voltage_vccclr);
    data.voltage_vccmlb = i2c->read_u32(voltage_vccmlb);
    data.voltage_vccp012 = 0; // Deprecated
    data.voltage_vccp345 = 0; // Deprecated
    data.voltage_vccmp = i2c->read_u32(voltage_vccmp);
    data.voltage_ntb1 = i2c->read_u32(voltage_ntb1);
    data.voltage_vccpio = i2c->read_u32(voltage_vccpio);
    data.voltage_vccsfr = i2c->read_u32(voltage_vccsfr);
    data.voltage_pch = i2c->read_u32(voltage_pch);
    data.voltage_vccmfuse = i2c->read_u32(voltage_vccmfuse);
    data.voltage_ntb2 = i2c->read_u32(voltage_ntb2);
    data.voltage_vpp = i2c->read_u32(voltage_vpp);
}

