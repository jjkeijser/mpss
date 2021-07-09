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
#include "MicVoltageInfo.hpp"
#include "VoltageInfoData_p.hpp"  // Private

namespace micmgmt
{
    using namespace std;

    TEST(sdk, TC_KNL_mpsstools_MicVoltageInfo_001)
    {
        const size_t SENSOR_COUNT = 3;
        const size_t MAXIMUM_SENSOR_NAME_WIDTH = 16;
        const std::string MAXIMUM_SENSOR_VALUE_NAME( "max" );
        const std::string WIDEST_SENSOR_NAME( "widestsensorname" );
        const int WIDEST_SENSOR_INDEX = 2;

        VoltageInfoData data;
        data.mSensors.push_back( MicVoltage( MAXIMUM_SENSOR_VALUE_NAME, 750 ) );
        data.mSensors.push_back( MicVoltage( "other", 2 ) );
        data.mSensors.push_back( MicVoltage( WIDEST_SENSOR_NAME, 200 ) );
        data.mValid = true;

        VoltageInfoData emptydata;
        emptydata.mValid = true;

        {   // Empty tests
            MicVoltageInfo mvi;
            EXPECT_FALSE( mvi.isValid() );
            EXPECT_EQ( size_t(0), mvi.sensorCount() );
            EXPECT_EQ( size_t(0), mvi.maximumSensorNameWidth() );
            EXPECT_FALSE( mvi.maximumSensorValue().isValid() );
            EXPECT_FALSE( mvi.sensorValueAt( 0 ).isValid() );
        }

        {   // Data constructor tests
            MicVoltageInfo mvi( data );
            EXPECT_TRUE( mvi.isValid() );
            EXPECT_EQ( SENSOR_COUNT, mvi.sensorCount() );
            EXPECT_EQ( MAXIMUM_SENSOR_NAME_WIDTH, mvi.maximumSensorNameWidth() );
            EXPECT_EQ( MAXIMUM_SENSOR_VALUE_NAME, mvi.maximumSensorValue().name() );
            EXPECT_EQ( WIDEST_SENSOR_NAME, mvi.sensorValueAt( WIDEST_SENSOR_INDEX ).name() );
        }

        {   // Copy constructor tests
            MicVoltageInfo mvithat( data );
            MicVoltageInfo mvithis( mvithat );
            EXPECT_TRUE( mvithis.isValid() );
            EXPECT_EQ( SENSOR_COUNT, mvithis.sensorCount() );
            EXPECT_EQ( MAXIMUM_SENSOR_NAME_WIDTH, mvithis.maximumSensorNameWidth() );
            EXPECT_EQ( MAXIMUM_SENSOR_VALUE_NAME, mvithis.maximumSensorValue().name() );
            EXPECT_EQ( WIDEST_SENSOR_NAME, mvithis.sensorValueAt( WIDEST_SENSOR_INDEX ).name() );
        }

        {   // Copy assignment tests
            MicVoltageInfo mvithat( data );
            MicVoltageInfo mvithis;
            mvithis = mvithat;
            EXPECT_TRUE( mvithis.isValid() );
            EXPECT_EQ( SENSOR_COUNT, mvithis.sensorCount() );
            EXPECT_EQ( MAXIMUM_SENSOR_NAME_WIDTH, mvithis.maximumSensorNameWidth() );
            EXPECT_EQ( MAXIMUM_SENSOR_VALUE_NAME, mvithis.maximumSensorValue().name() );
            EXPECT_EQ( WIDEST_SENSOR_NAME, mvithis.sensorValueAt( WIDEST_SENSOR_INDEX ).name() );

            mvithis = mvithis; //code coverage..
            EXPECT_TRUE( mvithis.isValid() );
            EXPECT_EQ( SENSOR_COUNT, mvithis.sensorCount() );
            EXPECT_EQ( MAXIMUM_SENSOR_NAME_WIDTH, mvithis.maximumSensorNameWidth() );
            EXPECT_EQ( MAXIMUM_SENSOR_VALUE_NAME, mvithis.maximumSensorValue().name() );
            EXPECT_EQ( WIDEST_SENSOR_NAME, mvithis.sensorValueAt( WIDEST_SENSOR_INDEX ).name() );
        }

        {   // Empty data coverage tests
            MicVoltageInfo mvi( emptydata );
            EXPECT_TRUE( mvi.isValid() );
            EXPECT_EQ( size_t(0), mvi.maximumSensorNameWidth() );
            EXPECT_FALSE( mvi.maximumSensorValue().isValid() );
            EXPECT_FALSE( mvi.sensorValueAt( WIDEST_SENSOR_INDEX ).isValid() );
        }

    } // sdk.TC_KNL_mpsstools_MicVoltageInfo_001

}   // namespace micmgmt
