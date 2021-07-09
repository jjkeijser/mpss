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
#include "ThrottleInfo.hpp"

namespace micmgmt
{
    using namespace std;

    TEST(sdk, TC_KNL_mpsstools_ThrottleInfo_001)
    {
        const uint32_t DURATION = 3;
        const uint32_t EVENT_COUNT = 2;
        const uint32_t TOTAL_TIME = 1;

        {   // Empty tests
            ThrottleInfo ti;
            EXPECT_TRUE( ti.isNull() );
            EXPECT_FALSE( ti.isActive() );
            EXPECT_EQ( 0U, ti.duration() );
            EXPECT_EQ( 0U, ti.totalTime() );
            EXPECT_EQ( 0U, ti.eventCount() );
        }

        {   // Value constructor tests
            ThrottleInfo ti( DURATION, true, EVENT_COUNT, TOTAL_TIME );
            EXPECT_FALSE( ti.isNull() );
            EXPECT_TRUE( ti.isActive() );
            EXPECT_EQ( DURATION, ti.duration() );
            EXPECT_EQ( TOTAL_TIME, ti.totalTime() );
            EXPECT_EQ( EVENT_COUNT, ti.eventCount() );

            ThrottleInfo tidefaultone( DURATION, true, EVENT_COUNT );
            EXPECT_FALSE( tidefaultone.isNull() );
            EXPECT_TRUE( tidefaultone.isActive() );
            EXPECT_EQ( DURATION, tidefaultone.duration() );
            EXPECT_EQ( 0U, tidefaultone.totalTime() );
            EXPECT_EQ( EVENT_COUNT, tidefaultone.eventCount() );

            ThrottleInfo tidefaulttwo( DURATION, true );
            EXPECT_FALSE( tidefaulttwo.isNull() );
            EXPECT_TRUE( tidefaulttwo.isActive() );
            EXPECT_EQ( DURATION, tidefaulttwo.duration() );
            EXPECT_EQ( 0U, tidefaulttwo.totalTime() );
            EXPECT_EQ( 0U, tidefaulttwo.eventCount() );
        }

        {   // Copy constructor tests
            ThrottleInfo thatinfo( DURATION, true, EVENT_COUNT, TOTAL_TIME );
            ThrottleInfo thisinfo( thatinfo );
            EXPECT_TRUE( thisinfo.isActive() );
            EXPECT_EQ( DURATION, thisinfo.duration() );
            EXPECT_EQ( TOTAL_TIME, thisinfo.totalTime() );
            EXPECT_EQ( EVENT_COUNT, thisinfo.eventCount() );
        }

        {   // Copy assignment tests
            ThrottleInfo thatinfo( DURATION, true, EVENT_COUNT, TOTAL_TIME );
            ThrottleInfo thisinfo;
            thisinfo = thatinfo;
            EXPECT_TRUE( thisinfo.isActive() );
            EXPECT_EQ( DURATION, thisinfo.duration() );
            EXPECT_EQ( TOTAL_TIME, thisinfo.totalTime() );
            EXPECT_EQ( EVENT_COUNT, thisinfo.eventCount() );

            thisinfo = thisinfo; //code coverage..
            EXPECT_TRUE( thisinfo.isActive() );
            EXPECT_EQ( DURATION, thisinfo.duration() );
            EXPECT_EQ( TOTAL_TIME, thisinfo.totalTime() );
            EXPECT_EQ( EVENT_COUNT, thisinfo.eventCount() );
        }

        {   // isNull coverage tests
            ThrottleInfo ticount( 0, false, EVENT_COUNT, 0 );
            EXPECT_FALSE( ticount.isNull() );

            ThrottleInfo titime( 0, false, 0, TOTAL_TIME );
            EXPECT_FALSE( titime.isNull() );
        }

    } // sdk.TC_KNL_mpsstools_ThrottleInfo_001

}   // namespace micmgmt
