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

#include <gtest/gtest.h>
#include "MicDevice.hpp"

namespace micmgmt
{
    using namespace std;

    TEST(sdk, TC_KNL_mpsstools_MicDeviceStatic_001)
    {

        {   // Device state as string (static) tests
            EXPECT_EQ( "offline", MicDevice::deviceStateAsString( MicDevice::DeviceState::eOffline ) );
            EXPECT_EQ( "online", MicDevice::deviceStateAsString( MicDevice::DeviceState::eOnline ) );
            EXPECT_EQ( "ready", MicDevice::deviceStateAsString( MicDevice::DeviceState::eReady ) );
            EXPECT_EQ( "reset", MicDevice::deviceStateAsString( MicDevice::DeviceState::eReset ) );
            EXPECT_EQ( "reboot", MicDevice::deviceStateAsString( MicDevice::DeviceState::eReboot ) );
            EXPECT_EQ( "lost", MicDevice::deviceStateAsString( MicDevice::DeviceState::eLost ) );
            EXPECT_EQ( "error", MicDevice::deviceStateAsString( MicDevice::DeviceState::eError ) );
        }

    } // sdk.TC_KNL_mpsstools_MicDeviceStatic_001

}   // namespace micmgmt
