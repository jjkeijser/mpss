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
#include "MicDeviceInfo.hpp"
#include "MicPciConfigInfo.hpp"
#include "MicProcessorInfo.hpp"
#include "MicVersionInfo.hpp"
#include "KnlMockDevice.hpp"

#include "MicDeviceError.hpp"

namespace micmgmt
{
    using namespace std;

    TEST(sdk, TC_KNL_mpsstools_MicDeviceInfo_001)
    {
        const uint32_t ERR_SUCCESS = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

        const int DEVICE_NUMBER = 0;
        const std::string DEVICE_NAME( "mic0" );
        const std::string DEVICE_TYPE( "x200" );
        const std::string SKU( "B0PO-SKU1" ); // from KncMockDevice

        {   // Empty tests
            MicDeviceInfo mdi;
            EXPECT_EQ( -1, mdi.deviceNumber() );
            EXPECT_EQ( "", mdi.deviceName() );
            EXPECT_EQ( "", mdi.deviceType() );
            EXPECT_EQ( "", mdi.deviceSku() );
            // This will be invalid: no MicDevice* in them
            EXPECT_FALSE( mdi.pciConfigInfo().isValid() );
            EXPECT_FALSE( mdi.processorInfo().isValid() );
            EXPECT_FALSE( mdi.versionInfo().isValid() );
        }

        {   // Refresh and clear positive tests
            MicDevice* device = new MicDevice( new KnlMockDevice( DEVICE_NUMBER ) );
            EXPECT_EQ( ERR_SUCCESS, device->open() );
            EXPECT_TRUE( device->isOpen() );

            MicDeviceInfo mdi( device );
            EXPECT_EQ( DEVICE_NUMBER, mdi.deviceNumber() );
            EXPECT_EQ( DEVICE_NAME, mdi.deviceName() );
            EXPECT_EQ( DEVICE_TYPE, mdi.deviceType() );
            EXPECT_EQ( SKU, mdi.deviceSku() );
            EXPECT_TRUE( mdi.pciConfigInfo().isValid() );
            EXPECT_TRUE( mdi.processorInfo().isValid() );

            mdi.clear();
            EXPECT_EQ( DEVICE_NUMBER, mdi.deviceNumber() );
            EXPECT_EQ( DEVICE_NAME, mdi.deviceName() );
            EXPECT_EQ( DEVICE_TYPE, mdi.deviceType() );
            // Test lazy initialization
            EXPECT_EQ( SKU, mdi.deviceSku() );
            EXPECT_TRUE( mdi.pciConfigInfo().isValid() );
            EXPECT_TRUE( mdi.processorInfo().isValid() );

            device->close();
            delete device;
        }

        {   // Refresh negative (device not open) tests
            MicDevice* device = new MicDevice( new KnlMockDevice( DEVICE_NUMBER ) );

            MicDeviceInfo mdi( device );
            EXPECT_EQ( DEVICE_NUMBER, mdi.deviceNumber() );
            EXPECT_EQ( DEVICE_NAME, mdi.deviceName() );
            EXPECT_EQ( DEVICE_TYPE, mdi.deviceType() );
            EXPECT_EQ( "", mdi.deviceSku() );
            EXPECT_FALSE( mdi.pciConfigInfo().isValid() );
            EXPECT_FALSE( mdi.processorInfo().isValid() );
            EXPECT_FALSE( mdi.versionInfo().isValid() );

            delete device;
        }

    } // sdk.TC_KNL_mpsstools_MicDeviceInfo_001

}   // namespace micmgmt
