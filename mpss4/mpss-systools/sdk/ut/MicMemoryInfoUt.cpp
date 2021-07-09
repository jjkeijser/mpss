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
#include "MicMemoryInfo.hpp"
#include "MemoryInfoData_p.hpp"  // Private
#include "MicSampleBase.hpp"

namespace micmgmt
{
    using namespace std;

    TEST(sdk, TC_KNL_mpsstools_MicMemoryInfo_001)
    {
        const std::string VENDOR_NAME( "vendor" );
        const std::string MEMORY_TYPE( "type" );
        const std::string SPEED_NAME( "speed" );
        const std::string DENSITY_NAME( "density" );
        const std::string SIZE_NAME( "size" );
        const std::string FREQUENCY_NAME( "frequency" );
        const uint16_t REVISION = 0x2;

        MemoryInfoData data;
        data.mVendorName = VENDOR_NAME;
        data.mMemoryType = MEMORY_TYPE;
        data.mSpeed = MicSpeed( SPEED_NAME, 500 );
        data.mDensity = MicMemory( DENSITY_NAME, 800 );
        data.mSize = MicMemory( SIZE_NAME, 20 );
        data.mFrequency = MicFrequency( FREQUENCY_NAME, 600 );
        data.mRevision = REVISION;
        data.mEcc = true;
        data.mValid = true;

        {   // Empty tests
            MicMemoryInfo mmi;
            EXPECT_FALSE( mmi.isValid() );
            EXPECT_EQ( "", mmi.vendorName() );
            EXPECT_EQ( "", mmi.memoryType() );
            EXPECT_EQ( 0x0, mmi.revision() );
            EXPECT_FALSE( mmi.density().isValid() );
            EXPECT_FALSE( mmi.size().isValid() );
            EXPECT_FALSE( mmi.speed().isValid() );
            EXPECT_FALSE( mmi.frequency().isValid() );
            EXPECT_FALSE( mmi.isEccEnabled() );
        }

        {   // Data constructor tests
            MicMemoryInfo mmi( data );
            EXPECT_TRUE( mmi.isValid() );
            EXPECT_EQ( VENDOR_NAME, mmi.vendorName() );
            EXPECT_EQ( MEMORY_TYPE, mmi.memoryType() );
            EXPECT_EQ( REVISION, mmi.revision() );
            EXPECT_EQ( DENSITY_NAME, mmi.density().name() );
            EXPECT_EQ( SIZE_NAME, mmi.size().name() );
            EXPECT_EQ( SPEED_NAME, mmi.speed().name() );
            EXPECT_EQ( FREQUENCY_NAME, mmi.frequency().name() );
            EXPECT_TRUE( mmi.isEccEnabled() );
        }

        {   // Copy constructor tests
            MicMemoryInfo mmithat( data );
            MicMemoryInfo mmithis( mmithat );
            EXPECT_TRUE( mmithis.isValid() );
            EXPECT_EQ( VENDOR_NAME, mmithis.vendorName() );
            EXPECT_EQ( MEMORY_TYPE, mmithis.memoryType() );
            EXPECT_EQ( REVISION, mmithis.revision() );
            EXPECT_EQ( DENSITY_NAME, mmithis.density().name() );
            EXPECT_EQ( SIZE_NAME, mmithis.size().name() );
            EXPECT_EQ( SPEED_NAME, mmithis.speed().name() );
            EXPECT_EQ( FREQUENCY_NAME, mmithis.frequency().name() );
            EXPECT_TRUE( mmithis.isEccEnabled() );
        }

        {   // Copy assignment tests
            MicMemoryInfo mmithat( data );
            MicMemoryInfo mmithis;
            mmithis = mmithat;
            EXPECT_TRUE( mmithis.isValid() );
            EXPECT_EQ( VENDOR_NAME, mmithis.vendorName() );
            EXPECT_EQ( MEMORY_TYPE, mmithis.memoryType() );
            EXPECT_EQ( REVISION, mmithis.revision() );
            EXPECT_EQ( DENSITY_NAME, mmithis.density().name() );
            EXPECT_EQ( SIZE_NAME, mmithis.size().name() );
            EXPECT_EQ( SPEED_NAME, mmithis.speed().name() );
            EXPECT_EQ( FREQUENCY_NAME, mmithis.frequency().name() );
            EXPECT_TRUE( mmithis.isEccEnabled() );

            mmithis = mmithis; //code coverage..
            EXPECT_TRUE( mmithis.isValid() );
            EXPECT_EQ( VENDOR_NAME, mmithis.vendorName() );
            EXPECT_EQ( MEMORY_TYPE, mmithis.memoryType() );
            EXPECT_EQ( REVISION, mmithis.revision() );
            EXPECT_EQ( DENSITY_NAME, mmithis.density().name() );
            EXPECT_EQ( SIZE_NAME, mmithis.size().name() );
            EXPECT_EQ( SPEED_NAME, mmithis.speed().name() );
            EXPECT_EQ( FREQUENCY_NAME, mmithis.frequency().name() );
            EXPECT_TRUE( mmithis.isEccEnabled() );
        }

        {   // Clear tests
            MicMemoryInfo mmi( data );
            mmi.clear();
            EXPECT_FALSE( mmi.isValid() );
        }

    } // sdk.TC_KNL_mpsstools_MicMemoryInfo_001

}   // namespace micmgmt
