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
#include    "utilities.hpp"
#include    "ut-utilities.hpp"
#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;

void createKnlMockDevice( string value )
{
    string zero  = "0";
    string mock  = "KNL_MOCK_CNT";
    string mpss  = "MICMGMT_MPSS_MOCK_VERSION";
    string stack = "4.1.2.3";

#ifdef _WIN32
    SetEnvironmentVariableA((LPCSTR)mock.c_str(), (LPCSTR)value.c_str());
    SetEnvironmentVariableA((LPCSTR)mpss.c_str(), (LPCSTR)stack.c_str());
#else
    setenv( mock.c_str(), value.c_str(), 1 );
    setenv( mpss.c_str(), stack.c_str(), 1 );
#endif
}

