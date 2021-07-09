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
#include "MicThermalInfo.hpp"
#include "ThermalInfoData_p.hpp"  // Private

namespace micmgmt
{
    using namespace std;

    TEST(sdk, TC_KNL_mpsstools_MicThermalInfo_001)
    {
        const size_t SENSOR_COUNT = 4;
        const size_t MAXIMUM_SENSOR_NAME_WIDTH = 16;
        const std::string MAXIMUM_SENSOR_VALUE_NAME( "max" );
        const std::string WIDEST_SENSOR_NAME( "widestsensorname" );
        const int WIDEST_SENSOR_INDEX = 2;
        const std::string CONTROL_NAME( "control" );
        const std::string CRITICAL_NAME( "critical" );
        const uint32_t FAN_RPM = 800;
        const uint32_t FAN_PWM = 250;
        const uint32_t FAN_ADDER = 3;

        ThermalInfoData data;
        data.mSensors.push_back( MicTemperature( "mid", 30 ) );
        data.mSensors.push_back( MicTemperature( MAXIMUM_SENSOR_VALUE_NAME, 45 ) );
        data.mSensors.push_back( MicTemperature( WIDEST_SENSOR_NAME, 25 ) );
        data.mSensors.push_back( MicTemperature( "low", 10 ) );
        data.mCritical = MicTemperature( CRITICAL_NAME, 40 );
        data.mControl = MicTemperature( CONTROL_NAME, 30 );
        data.mFanRpm = FAN_RPM;
        data.mFanPwm = FAN_PWM;
        data.mFanAdder = FAN_ADDER;
        data.mThrottleInfo = ThrottleInfo( 3, true, 2, 1 );
        data.mValid = true;

        ThermalInfoData emptydata;
        emptydata.mValid = true;

        {   // Empty tests
            MicThermalInfo mti;
            EXPECT_FALSE( mti.isValid() );
            EXPECT_EQ( size_t(0), mti.sensorCount() );
            EXPECT_EQ( size_t(0), mti.maximumSensorNameWidth() );
            EXPECT_FALSE( mti.maximumSensorValue().isValid() );
            EXPECT_FALSE( mti.sensorValueAt( 0 ).isValid() );
            EXPECT_EQ( static_cast<uint32_t>(0), mti.fanRpm().value() );
            EXPECT_EQ( static_cast<uint32_t>(0), mti.fanPwm().value() );
            EXPECT_EQ( static_cast<uint32_t>(0), mti.fanAdder().value() );
            EXPECT_FALSE( mti.fanBoostThreshold().isValid() );
            EXPECT_FALSE( mti.throttleThreshold().isValid() );
            EXPECT_TRUE( mti.throttleInfo().isNull() );
        }

        {   // Data constructor tests
            MicThermalInfo mti( data );
            EXPECT_TRUE( mti.isValid() );
            EXPECT_EQ( SENSOR_COUNT, mti.sensorCount() );
            EXPECT_EQ( MAXIMUM_SENSOR_NAME_WIDTH, mti.maximumSensorNameWidth() );
            EXPECT_EQ( MAXIMUM_SENSOR_VALUE_NAME, mti.maximumSensorValue().name() );
            EXPECT_EQ( WIDEST_SENSOR_NAME, mti.sensorValueAt( WIDEST_SENSOR_INDEX ).name() );
            EXPECT_EQ( FAN_RPM, mti.fanRpm().value() );
            EXPECT_EQ( FAN_PWM, mti.fanPwm().value() );
            EXPECT_EQ( FAN_ADDER, mti.fanAdder().value() );
            EXPECT_EQ( CONTROL_NAME, mti.fanBoostThreshold().name() );
            EXPECT_EQ( CRITICAL_NAME, mti.throttleThreshold().name() );
            EXPECT_FALSE( mti.throttleInfo().isNull() );
        }

        {   // Copy constructor tests
            MicThermalInfo mtithat( data );
            MicThermalInfo mtithis( mtithat );
            EXPECT_TRUE( mtithis.isValid() );
            EXPECT_EQ( SENSOR_COUNT, mtithis.sensorCount() );
            EXPECT_EQ( MAXIMUM_SENSOR_NAME_WIDTH, mtithis.maximumSensorNameWidth() );
            EXPECT_EQ( MAXIMUM_SENSOR_VALUE_NAME, mtithis.maximumSensorValue().name() );
            EXPECT_EQ( WIDEST_SENSOR_NAME, mtithis.sensorValueAt( WIDEST_SENSOR_INDEX ).name() );
            EXPECT_EQ( FAN_RPM, mtithis.fanRpm().value() );
            EXPECT_EQ( FAN_PWM, mtithis.fanPwm().value() );
            EXPECT_EQ( FAN_ADDER, mtithis.fanAdder().value() );
            EXPECT_EQ( CONTROL_NAME, mtithis.fanBoostThreshold().name() );
            EXPECT_EQ( CRITICAL_NAME, mtithis.throttleThreshold().name() );
            EXPECT_FALSE( mtithis.throttleInfo().isNull() );
        }

        {   // Copy assignment tests
            MicThermalInfo mtithat( data );
            MicThermalInfo mtithis;
            mtithis = mtithat;
            EXPECT_TRUE( mtithis.isValid() );
            EXPECT_EQ( SENSOR_COUNT, mtithis.sensorCount() );
            EXPECT_EQ( MAXIMUM_SENSOR_NAME_WIDTH, mtithis.maximumSensorNameWidth() );
            EXPECT_EQ( MAXIMUM_SENSOR_VALUE_NAME, mtithis.maximumSensorValue().name() );
            EXPECT_EQ( WIDEST_SENSOR_NAME, mtithis.sensorValueAt( WIDEST_SENSOR_INDEX ).name() );
            EXPECT_EQ( FAN_RPM, mtithis.fanRpm().value() );
            EXPECT_EQ( FAN_PWM, mtithis.fanPwm().value() );
            EXPECT_EQ( FAN_ADDER, mtithis.fanAdder().value() );
            EXPECT_EQ( CONTROL_NAME, mtithis.fanBoostThreshold().name() );
            EXPECT_EQ( CRITICAL_NAME, mtithis.throttleThreshold().name() );
            EXPECT_FALSE( mtithis.throttleInfo().isNull() );

            mtithis = mtithis; //code coverage..
            EXPECT_TRUE( mtithis.isValid() );
            EXPECT_EQ( SENSOR_COUNT, mtithis.sensorCount() );
            EXPECT_EQ( MAXIMUM_SENSOR_NAME_WIDTH, mtithis.maximumSensorNameWidth() );
            EXPECT_EQ( MAXIMUM_SENSOR_VALUE_NAME, mtithis.maximumSensorValue().name() );
            EXPECT_EQ( WIDEST_SENSOR_NAME, mtithis.sensorValueAt( WIDEST_SENSOR_INDEX ).name() );
            EXPECT_EQ( FAN_RPM, mtithis.fanRpm().value() );
            EXPECT_EQ( FAN_PWM, mtithis.fanPwm().value() );
            EXPECT_EQ( FAN_ADDER, mtithis.fanAdder().value() );
            EXPECT_EQ( CONTROL_NAME, mtithis.fanBoostThreshold().name() );
            EXPECT_EQ( CRITICAL_NAME, mtithis.throttleThreshold().name() );
            EXPECT_FALSE( mtithis.throttleInfo().isNull() );
        }

        {   // Empty data coverage tests
            MicThermalInfo mti( emptydata );
            EXPECT_FALSE( mti.isValid() );
            EXPECT_EQ( size_t(0), mti.maximumSensorNameWidth() );
            EXPECT_FALSE( mti.maximumSensorValue().isValid() );
            EXPECT_FALSE( mti.sensorValueAt( 1 ).isValid() );
        }

    } // sdk.TC_KNL_mpsstools_MicThermalInfo_001

}   // namespace micmgmt
