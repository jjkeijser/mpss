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
#include "MicMemoryUsageInfo.hpp"
#include "MemoryUsageData_p.hpp"  // Private

namespace micmgmt
{
    using namespace std;

    TEST(sdk, TC_KNL_mpsstools_MicMemoryUsageInfo_001)
    {
        const std::string TOTAL_NAME( "total" );
        const std::string USED_NAME( "used" );
        const std::string FREE_NAME( "free" );
        const std::string CACHED_NAME( "cached" );
        const std::string BUFFERS_NAME( "buffers" );

        MemoryUsageData data;
        data.mTotal = MicMemory( TOTAL_NAME, 1 );
        data.mUsed = MicMemory( USED_NAME, 2 );
        data.mFree = MicMemory( FREE_NAME, 3 );
        data.mCached = MicMemory( CACHED_NAME, 5 );
        data.mBuffers = MicMemory( BUFFERS_NAME, 6 );
        data.mValid = true;

        {   // Empty tests
            MicMemoryUsageInfo mmui;
            EXPECT_FALSE( mmui.isValid() );
            EXPECT_FALSE( mmui.total().isValid() );
            EXPECT_FALSE( mmui.used().isValid() );
            EXPECT_FALSE( mmui.free().isValid() );
            EXPECT_FALSE( mmui.cached().isValid() );
            EXPECT_FALSE( mmui.buffers().isValid() );
        }

        {   // Data constructor tests
            MicMemoryUsageInfo mmui( data );
            EXPECT_TRUE( mmui.isValid() );
            EXPECT_EQ( TOTAL_NAME, mmui.total().name() );
            EXPECT_EQ( USED_NAME, mmui.used().name() );
            EXPECT_EQ( FREE_NAME, mmui.free().name() );
            EXPECT_EQ( CACHED_NAME, mmui.cached().name() );
            EXPECT_EQ( BUFFERS_NAME, mmui.buffers().name() );
        }

        {   // Copy constructor tests
            MicMemoryUsageInfo mmuithat( data );
            MicMemoryUsageInfo mmuithis( mmuithat );
            EXPECT_TRUE( mmuithis.isValid() );
            EXPECT_EQ( TOTAL_NAME, mmuithis.total().name() );
            EXPECT_EQ( USED_NAME, mmuithis.used().name() );
            EXPECT_EQ( FREE_NAME, mmuithis.free().name() );
            EXPECT_EQ( CACHED_NAME, mmuithis.cached().name() );
            EXPECT_EQ( BUFFERS_NAME, mmuithis.buffers().name() );
        }

        {   // Copy assignment tests
            MicMemoryUsageInfo mmuithat( data );
            MicMemoryUsageInfo mmuithis;
            mmuithis = mmuithat;
            EXPECT_TRUE( mmuithis.isValid() );
            EXPECT_EQ( TOTAL_NAME, mmuithis.total().name() );
            EXPECT_EQ( USED_NAME, mmuithis.used().name() );
            EXPECT_EQ( FREE_NAME, mmuithis.free().name() );
            EXPECT_EQ( CACHED_NAME, mmuithis.cached().name() );
            EXPECT_EQ( BUFFERS_NAME, mmuithis.buffers().name() );

            mmuithis = mmuithis; //code coverage..
            EXPECT_TRUE( mmuithis.isValid() );
            EXPECT_EQ( TOTAL_NAME, mmuithis.total().name() );
            EXPECT_EQ( USED_NAME, mmuithis.used().name() );
            EXPECT_EQ( FREE_NAME, mmuithis.free().name() );
            EXPECT_EQ( CACHED_NAME, mmuithis.cached().name() );
            EXPECT_EQ( BUFFERS_NAME, mmuithis.buffers().name() );
        }

        {   // Clear tests
            MicMemoryUsageInfo mmui( data );
            mmui.clear();
            EXPECT_FALSE( mmui.isValid() );
        }

    } // sdk.TC_KNL_mpsstools_MicMemoryUsageInfo_001

}   // namespace micmgmt
