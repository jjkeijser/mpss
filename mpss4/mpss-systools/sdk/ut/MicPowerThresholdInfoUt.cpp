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
#include "MicPowerThresholdInfo.hpp"
#include "PowerThresholdData_p.hpp"  // Private

namespace micmgmt
{
    using namespace std;

    TEST(sdk, TC_KNL_mpsstools_MicPowerThresholdInfo_001)
    {
        const std::string LO_NAME( "lo" );
        const std::string HI_NAME( "hi" );
        const std::string MAXIMUM_NAME( "maximum" );
        const std::string WINDOW_0_NAME( "window_0" );
        const std::string WINDOW_1_NAME( "window_1" );
        const int WINDOW_0_TIME = 1;
        const int WINDOW_1_TIME = 2;

        PowerThresholdData data;
        data.mLoThreshold = MicPower( LO_NAME, 0 );
        data.mHiThreshold = MicPower( HI_NAME, 0 );
        data.mMaximum = MicPower( MAXIMUM_NAME, 0 );
        data.mWindow0Threshold = MicPower( WINDOW_0_NAME, 0 );
        data.mWindow1Threshold = MicPower( WINDOW_1_NAME, 0 );
        data.mWindow0Time = WINDOW_0_TIME;
        data.mWindow1Time = WINDOW_1_TIME;
        data.mPersistence = true;
        data.mValid = true;

        {   // Empty tests
            MicPowerThresholdInfo mpti;
            EXPECT_FALSE( mpti.isValid() );
            EXPECT_FALSE( mpti.isPersistent() );
            EXPECT_FALSE( mpti.loThreshold().isValid() );
            EXPECT_FALSE( mpti.hiThreshold().isValid() );
            EXPECT_FALSE( mpti.window0Threshold().isValid() );
            EXPECT_FALSE( mpti.window1Threshold().isValid() );
            EXPECT_EQ( 0, mpti.window0Time() );
            EXPECT_EQ( 0, mpti.window1Time() );
        }

        {   // Data constructor tests
            MicPowerThresholdInfo mpti( data );
            EXPECT_TRUE( mpti.isValid() );
            EXPECT_TRUE( mpti.isPersistent() );
            EXPECT_EQ( LO_NAME, mpti.loThreshold().name() );
            EXPECT_EQ( HI_NAME, mpti.hiThreshold().name() );
            EXPECT_EQ( WINDOW_0_NAME, mpti.window0Threshold().name() );
            EXPECT_EQ( WINDOW_1_NAME, mpti.window1Threshold().name() );
            EXPECT_EQ( WINDOW_0_TIME, mpti.window0Time() );
            EXPECT_EQ( WINDOW_1_TIME, mpti.window1Time() );
        }

        {   // Copy constructor tests
            MicPowerThresholdInfo mptithat( data );
            MicPowerThresholdInfo mptithis( mptithat );
            EXPECT_TRUE( mptithis.isValid() );
            EXPECT_TRUE( mptithis.isPersistent() );
            EXPECT_EQ( LO_NAME, mptithis.loThreshold().name() );
            EXPECT_EQ( HI_NAME, mptithis.hiThreshold().name() );
            EXPECT_EQ( WINDOW_0_NAME, mptithis.window0Threshold().name() );
            EXPECT_EQ( WINDOW_1_NAME, mptithis.window1Threshold().name() );
            EXPECT_EQ( WINDOW_0_TIME, mptithis.window0Time() );
            EXPECT_EQ( WINDOW_1_TIME, mptithis.window1Time() );
        }

        {   // Copy assignment tests
            MicPowerThresholdInfo mptithat( data );
            MicPowerThresholdInfo mptithis;
            mptithis = mptithat;
            EXPECT_TRUE( mptithis.isValid() );
            EXPECT_TRUE( mptithis.isPersistent() );
            EXPECT_EQ( LO_NAME, mptithis.loThreshold().name() );
            EXPECT_EQ( HI_NAME, mptithis.hiThreshold().name() );
            EXPECT_EQ( WINDOW_0_NAME, mptithis.window0Threshold().name() );
            EXPECT_EQ( WINDOW_1_NAME, mptithis.window1Threshold().name() );
            EXPECT_EQ( WINDOW_0_TIME, mptithis.window0Time() );
            EXPECT_EQ( WINDOW_1_TIME, mptithis.window1Time() );

            mptithis = mptithis; //code coverage..
            EXPECT_TRUE( mptithis.isValid() );
            EXPECT_TRUE( mptithis.isPersistent() );
            EXPECT_EQ( LO_NAME, mptithis.loThreshold().name() );
            EXPECT_EQ( HI_NAME, mptithis.hiThreshold().name() );
            EXPECT_EQ( WINDOW_0_NAME, mptithis.window0Threshold().name() );
            EXPECT_EQ( WINDOW_1_NAME, mptithis.window1Threshold().name() );
            EXPECT_EQ( WINDOW_0_TIME, mptithis.window0Time() );
            EXPECT_EQ( WINDOW_1_TIME, mptithis.window1Time() );
        }

    } // sdk.TC_KNL_mpsstools_MicPowerThresholdInfo_001

}   // namespace micmgmt
