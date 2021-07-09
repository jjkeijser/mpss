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
#include "info/ThermalInfoGroup.hpp"

using std::string;
using std::ifstream;

namespace
{
    const uint8_t temp_cpu = 0x40;
    const uint8_t temp_exhaust = 0x41;
    const uint8_t temp_vccp = 0x43;
    const uint8_t temp_vccclr = 0x44;
    const uint8_t temp_vccmp = 0x45;
    const uint8_t temp_west = 0x47;
    const uint8_t temp_east = 0x48;
    const uint8_t fan_tach = 0x49;
    const uint8_t fan_pwm = 0x4A;
    const uint8_t fan_pwm_adder = 0x4B;
    const uint8_t tcritical = 0x4C;
    const uint8_t tcontrol = 0x4D;
}


ThermalInfoGroup::ThermalInfoGroup(Services::ptr &services) :
    CachedDataGroupBase<ThermalInfo>(300), i2c(NULL)
{
    if(!(i2c = services->get_i2c_srv()))
        throw std::invalid_argument("NULL DaemonInfoSources->i2c");

}

void ThermalInfoGroup::refresh_data()
{
    data.temp_cpu = i2c->read_u32(temp_cpu);
    data.temp_exhaust = i2c->read_u32(temp_exhaust);
    data.temp_inlet = 0; // Deprecated
    data.temp_vccp = i2c->read_u32(temp_vccp);
    data.temp_vccclr = i2c->read_u32(temp_vccclr);
    data.temp_vccmp = i2c->read_u32(temp_vccmp);
    data.temp_mid = 0; // Deprecated
    data.temp_west = i2c->read_u32(temp_west);
    data.temp_east = i2c->read_u32(temp_east);
    data.fan_tach = i2c->read_u32(fan_tach);
    data.fan_pwm = i2c->read_u32(fan_pwm);
    data.fan_pwm_adder = i2c->read_u32(fan_pwm_adder);
    data.tcritical = i2c->read_u32(tcritical);
    data.tcontrol = i2c->read_u32(tcontrol);
    // The following values, while the register offset still exists in the SMC,
    // they have changed to be WEST_TEMP and NTB_TEMP respectively.
    data.thermal_throttle_duration = 0; // Deprecated
    data.thermal_throttle = 0; // Deprecated
}

