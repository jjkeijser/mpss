/*
 * Copyright (c) 2017, Intel Corporation.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
*/

#ifndef MICINFO_UT_UTILITIES_HPP
#define MICINFO_UT_UTILITIES_HPP

#include    "MicDeviceFactory.hpp"
#include    "MicDeviceManager.hpp"
#include    "MicDevice.hpp"
#include    "MicDeviceInfo.hpp"
#include    "MicDeviceDetails.hpp"
#include    "MicDeviceError.hpp"
#include    "MicCoreInfo.hpp"
#include    "MicProcessorInfo.hpp"
#include    "MicThermalInfo.hpp"
#include    "MicTemperature.hpp"
#include    "MicMemory.hpp"
#include    "MicMemoryInfo.hpp"
#include    "MicPciConfigInfo.hpp"
#include    "MicPlatformInfo.hpp"
#include    "MicMemoryUsageInfo.hpp"
#include    "MicPowerUsageInfo.hpp"
#include    "MicVoltageInfo.hpp"
#include    "ThrottleInfo.hpp"
#include    "PciAddress.hpp"
#include    "MicVoltage.hpp"
#include    "MicPower.hpp"
#include    "MicSpeed.hpp"
#include    "MicVersionInfo.hpp"
#include    "MicFrequency.hpp"
#include    "MicDeviceError.hpp"
#include    "MicBootConfigInfo.hpp"
#include    "KnlMockDevice.hpp"
#include    "MicBootConfigInfo.hpp"

void createKnlMockDevice( std::string value );

void forceKnlCardOnline( micmgmt::MicDevice* mockDevice );

#endif //MICINFO_UT_UTILITIES_HPP
