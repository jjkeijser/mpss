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
#include "MicPowerUsageInfo.hpp"
#include "PowerUsageData_p.hpp"  // Private

namespace micmgmt
{
    using namespace std;

    TEST(sdk, TC_KNL_mpsstools_MicPowerUsageInfo_001)
    {
        const size_t SENSOR_COUNT = 4;
        const size_t MAXIMUM_SENSOR_NAME_WIDTH = 16;
        const std::string MAXIMUM_SENSOR_VALUE_NAME( "max" );
        const std::string WIDEST_SENSOR_NAME( "widestsensorname" );
        const int WIDEST_SENSOR_INDEX = 3;
        const std::string THROTTLE_NAME( "throttle" );

        PowerUsageData data;
        data.mSensors.push_back( MicPower( "mid", 500 ) );
        data.mSensors.push_back( MicPower( MAXIMUM_SENSOR_VALUE_NAME, 800 ) );
        data.mSensors.push_back( MicPower( "low", 25 ) );
        data.mSensors.push_back( MicPower( WIDEST_SENSOR_NAME, 300 ) );
        data.mThrottle = MicPower( THROTTLE_NAME, 900 );
        data.mThrottleInfo = ThrottleInfo( 3, true, 2, 1 );
        data.mValid = true;

        PowerUsageData emptydata;
        emptydata.mValid = true;

        {   // Empty tests
            MicPowerUsageInfo mpui;
            EXPECT_FALSE( mpui.isValid() );
            EXPECT_EQ( size_t(0), mpui.sensorCount() );
            EXPECT_EQ( size_t(0), mpui.maximumSensorNameWidth() );
            EXPECT_FALSE( mpui.maximumSensorValue().isValid() );
            EXPECT_FALSE( mpui.sensorValueAt( 0 ).isValid() );
            EXPECT_FALSE( mpui.throttleThreshold().isValid() );
            EXPECT_TRUE( mpui.throttleInfo().isNull() );
        }

        {   // Data constructor tests
            MicPowerUsageInfo mpui( data );
            EXPECT_TRUE( mpui.isValid() );
            EXPECT_EQ( SENSOR_COUNT, mpui.sensorCount() );
            EXPECT_EQ( MAXIMUM_SENSOR_NAME_WIDTH, mpui.maximumSensorNameWidth() );
            EXPECT_EQ( MAXIMUM_SENSOR_VALUE_NAME, mpui.maximumSensorValue().name() );
            EXPECT_EQ( WIDEST_SENSOR_NAME, mpui.sensorValueAt( WIDEST_SENSOR_INDEX ).name() );
            EXPECT_EQ( THROTTLE_NAME, mpui.throttleThreshold().name() );
            EXPECT_FALSE( mpui.throttleInfo().isNull() );
        }

        {   // Copy constructor tests
            MicPowerUsageInfo mpuithat( data );
            MicPowerUsageInfo mpuithis( mpuithat );
            EXPECT_TRUE( mpuithis.isValid() );
            EXPECT_EQ( SENSOR_COUNT, mpuithis.sensorCount() );
            EXPECT_EQ( MAXIMUM_SENSOR_NAME_WIDTH, mpuithis.maximumSensorNameWidth() );
            EXPECT_EQ( MAXIMUM_SENSOR_VALUE_NAME, mpuithis.maximumSensorValue().name() );
            EXPECT_EQ( WIDEST_SENSOR_NAME, mpuithis.sensorValueAt( WIDEST_SENSOR_INDEX ).name() );
            EXPECT_EQ( THROTTLE_NAME, mpuithis.throttleThreshold().name() );
            EXPECT_FALSE( mpuithis.throttleInfo().isNull() );
        }

        {   // Copy assignment tests
            MicPowerUsageInfo mpuithat( data );
            MicPowerUsageInfo mpuithis;
            mpuithis = mpuithat;
            EXPECT_TRUE( mpuithis.isValid() );
            EXPECT_EQ( SENSOR_COUNT, mpuithis.sensorCount() );
            EXPECT_EQ( MAXIMUM_SENSOR_NAME_WIDTH, mpuithis.maximumSensorNameWidth() );
            EXPECT_EQ( MAXIMUM_SENSOR_VALUE_NAME, mpuithis.maximumSensorValue().name() );
            EXPECT_EQ( WIDEST_SENSOR_NAME, mpuithis.sensorValueAt( WIDEST_SENSOR_INDEX ).name() );
            EXPECT_EQ( THROTTLE_NAME, mpuithis.throttleThreshold().name() );
            EXPECT_FALSE( mpuithis.throttleInfo().isNull() );

            mpuithis = mpuithis; //code coverage..
            EXPECT_TRUE( mpuithis.isValid() );
            EXPECT_EQ( SENSOR_COUNT, mpuithis.sensorCount() );
            EXPECT_EQ( MAXIMUM_SENSOR_NAME_WIDTH, mpuithis.maximumSensorNameWidth() );
            EXPECT_EQ( MAXIMUM_SENSOR_VALUE_NAME, mpuithis.maximumSensorValue().name() );
            EXPECT_EQ( WIDEST_SENSOR_NAME, mpuithis.sensorValueAt( WIDEST_SENSOR_INDEX ).name() );
            EXPECT_EQ( THROTTLE_NAME, mpuithis.throttleThreshold().name() );
            EXPECT_FALSE( mpuithis.throttleInfo().isNull() );
        }

        {   // Empty data coverage tests
            MicPowerUsageInfo mpui( emptydata );
            EXPECT_TRUE( mpui.isValid() );
            EXPECT_EQ( size_t(0), mpui.maximumSensorNameWidth() );
            EXPECT_FALSE( mpui.maximumSensorValue().isValid() );
            EXPECT_FALSE( mpui.sensorValueAt( 1 ).isValid() );
        }

    } // sdk.TC_KNL_mpsstools_MicPowerUsageInfo_001

}   // namespace micmgmt
