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
#include "CoreInfoData_p.hpp"  // Private

namespace micmgmt
{
    using namespace std;

    TEST(sdk, TC_KNL_mpsstools_MicCoreInfo_001)
    {
        const size_t CORE_COUNT = 64;
        const size_t CORE_THREAD_COUNT = 2;
        const std::string FREQUENCY_NAME( "frequency" );
        const std::string VOLTAGE_NAME( "voltage" );

        CoreInfoData data;
        data.mCoreCount = CORE_COUNT;
        data.mCoreThreadCount = CORE_THREAD_COUNT;
        data.mFrequency = MicFrequency( FREQUENCY_NAME, 527);
        data.mVoltage = MicVoltage( VOLTAGE_NAME, 750);

        {   // Empty tests
            MicCoreInfo mci;
            EXPECT_FALSE( mci.isValid() );
            EXPECT_EQ( size_t(0), mci.coreCount().value() );
            EXPECT_EQ( size_t(0), mci.coreThreadCount().value() );
            EXPECT_FALSE( mci.frequency().isValid() );
            EXPECT_FALSE( mci.voltage().isValid() );
        }

        {   // Data constructor tests
            MicCoreInfo mci( data );
            EXPECT_TRUE( mci.isValid() );
            EXPECT_EQ( CORE_COUNT, mci.coreCount().value() );
            EXPECT_EQ( CORE_THREAD_COUNT, mci.coreThreadCount().value() );
            EXPECT_EQ( FREQUENCY_NAME, mci.frequency().name() );
            EXPECT_EQ( VOLTAGE_NAME, mci.voltage().name() );
        }

        {   // Copy constructor tests
            MicCoreInfo mcithat( data );
            MicCoreInfo mcithis( mcithat );
            EXPECT_TRUE( mcithis.isValid() );
            EXPECT_EQ( CORE_COUNT, mcithis.coreCount().value() );
            EXPECT_EQ( CORE_THREAD_COUNT, mcithis.coreThreadCount().value() );
            EXPECT_EQ( FREQUENCY_NAME, mcithis.frequency().name() );
            EXPECT_EQ( VOLTAGE_NAME, mcithis.voltage().name() );
        }

        {   // Copy Assignment tests
            MicCoreInfo mcithat( data );
            MicCoreInfo mcithis;
            mcithis = mcithat;
            EXPECT_TRUE( mcithis.isValid() );
            EXPECT_EQ( CORE_COUNT, mcithis.coreCount().value() );
            EXPECT_EQ( CORE_THREAD_COUNT, mcithis.coreThreadCount().value() );
            EXPECT_EQ( FREQUENCY_NAME, mcithis.frequency().name() );
            EXPECT_EQ( VOLTAGE_NAME, mcithis.voltage().name() );

            mcithis = mcithis; //code coverage..
            EXPECT_TRUE( mcithis.isValid() );
            EXPECT_EQ( CORE_COUNT, mcithis.coreCount().value() );
            EXPECT_EQ( CORE_THREAD_COUNT, mcithis.coreThreadCount().value() );
            EXPECT_EQ( FREQUENCY_NAME, mcithis.frequency().name() );
            EXPECT_EQ( VOLTAGE_NAME, mcithis.voltage().name() );
        }

        {   // Clear tests
            MicCoreInfo mci( data );
            mci.clear();
            EXPECT_FALSE( mci.isValid() );
        }

    } // sdk.TC_KNL_mpsstools_MicCoreInfo_001

}   // namespace micmgmt
