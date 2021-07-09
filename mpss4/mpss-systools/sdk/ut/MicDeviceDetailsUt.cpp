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
#include "MicCoreInfo.hpp"
#include "MicDeviceDetails.hpp"
#include "MicMemoryInfo.hpp"
#include "MicPlatformInfo.hpp"
#include "KnlMockDevice.hpp"
#include "MicBootConfigInfo.hpp"
#include "MicDeviceError.hpp"
#include "MicDevice.hpp"

namespace micmgmt
{
    using namespace std;

    TEST(sdk, TC_KNL_mpsstools_MicDeviceDetails_001)
    {
        const uint32_t ERR_SUCCESS = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

        const int DEVICE_NUMBER = 0;
        const std::string DEVICE_NAME( "mic0" );
        const std::string DEVICE_TYPE( "x200" );
        const MicBootConfigInfo  BOOT_CONFIG( "bzImage" );

        {   // Empty tests
            MicDeviceDetails mdd;
            EXPECT_EQ( -1, mdd.deviceNumber() );
            EXPECT_EQ( "", mdd.deviceName() );
            EXPECT_EQ( "", mdd.deviceType() );
            // No MicDevice*, expect invalid data
            EXPECT_FALSE( mdd.coreInfo().isValid() );
            EXPECT_FALSE( mdd.memoryInfo().isValid() );
            EXPECT_FALSE( mdd.micPlatformInfo().isValid() );
        }

        {   // Refresh and clear positive tests
            MicDevice* device = new MicDevice( new KnlMockDevice( DEVICE_NUMBER ) );

            // Open device
            EXPECT_EQ( ERR_SUCCESS, device->open() );
            EXPECT_TRUE( device->isOpen() );
            EXPECT_EQ( MicDevice::DeviceState::eOnline, device->deviceState() );
            EXPECT_TRUE( device->isOnline() );

            MicDeviceDetails mdd( device );
            EXPECT_EQ( DEVICE_NUMBER, mdd.deviceNumber() );
            EXPECT_EQ( DEVICE_NAME, mdd.deviceName() );
            EXPECT_EQ( DEVICE_TYPE, mdd.deviceType() );
            EXPECT_TRUE( mdd.coreInfo().isValid() );
            EXPECT_TRUE( mdd.memoryInfo().isValid() );
            EXPECT_TRUE( mdd.micPlatformInfo().isValid() );

            mdd.clear();
            EXPECT_EQ( DEVICE_NUMBER, mdd.deviceNumber() );
            EXPECT_EQ( DEVICE_NAME, mdd.deviceName() );
            EXPECT_EQ( DEVICE_TYPE, mdd.deviceType() );
            // Test lazy initialization
            EXPECT_TRUE( mdd.coreInfo().isValid() );
            EXPECT_TRUE( mdd.memoryInfo().isValid() );
            EXPECT_TRUE( mdd.micPlatformInfo().isValid() );

            device->close();
            delete device;
        }

        {   // Refresh negative (device not open) tests
            MicDevice* device = new MicDevice( new KnlMockDevice( DEVICE_NUMBER ) );

            MicDeviceDetails mdd( device );
            EXPECT_EQ( DEVICE_NUMBER, mdd.deviceNumber() );
            EXPECT_EQ( DEVICE_NAME, mdd.deviceName() );
            EXPECT_EQ( DEVICE_TYPE, mdd.deviceType() );
            EXPECT_FALSE( mdd.coreInfo().isValid() );
            EXPECT_FALSE( mdd.memoryInfo().isValid() );
            EXPECT_FALSE( mdd.micPlatformInfo().isValid() );

            device->close();
            delete device;
        }

        {   // Refresh negative (device not online) tests
            KnlMockDevice*  knlmock = new KnlMockDevice( DEVICE_NUMBER );
            MicDevice* device = new MicDevice( knlmock );  // Takes ownership of knlmock

            // Open, but reset the device
            EXPECT_EQ( ERR_SUCCESS, device->open() );
            EXPECT_TRUE( device->isOpen() );
            knlmock->setAdmin( true );
            EXPECT_EQ( ERR_SUCCESS, device->reset() );
            knlmock->setAdmin( false );
            EXPECT_FALSE( device->isOnline() );

            MicDeviceDetails mdd( device );
            EXPECT_EQ( DEVICE_NUMBER, mdd.deviceNumber() );
            EXPECT_EQ( DEVICE_NAME, mdd.deviceName() );
            EXPECT_EQ( DEVICE_TYPE, mdd.deviceType() );
            EXPECT_FALSE( mdd.coreInfo().isValid() );
            EXPECT_FALSE( mdd.memoryInfo().isValid() );
            EXPECT_TRUE( mdd.micPlatformInfo().isValid() );

            device->close();
            delete device;
        }

    } // sdk.TC_KNL_mpsstools_MicDeviceDetails_001

}   // namespace micmgmt
