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
#include "MicPower.hpp"

namespace micmgmt
{
    using namespace std;

    TEST(sdk, TC_KNL_mpsstools_MicPower_001)
    {
        const int VALUE = 250;
        const int EXPONENT = MicSampleBase::eMilli;
        const double SCALAR = 1000;
        const std::string NAME = "power";
        const size_t NAME_LENGTH = 5;
        const uint32_t NOT_AVAILABLE = 0xc0000000;
        const uint32_t LOWER_THRESHOLD_VIOLATION = 0x40000000;
        const uint32_t UPPER_THRESHOLD_VIOLATION = 0x80000000;

        {   // Empty tests
            MicPower mp;
            EXPECT_FALSE( mp.isValid() );
            EXPECT_TRUE( mp.isAvailable() );
            EXPECT_FALSE( mp.isLowerThresholdViolation() );
            EXPECT_FALSE( mp.isUpperThresholdViolation() );
            EXPECT_EQ( "", mp.name() );
            EXPECT_EQ( size_t(0), mp.nameLength() );
            EXPECT_EQ( 0, mp.value() );
            EXPECT_EQ( "", mp.unit() );
        }

        {   // Value constructor tests
            MicPower mp( VALUE, EXPONENT );
            EXPECT_TRUE( mp.isValid() );
            EXPECT_EQ( "", mp.name() );
            EXPECT_EQ( size_t(0), mp.nameLength() );
            EXPECT_EQ( VALUE, mp.value() );
            EXPECT_EQ( EXPONENT, mp.exponent() );

            MicPower mpdefaultone( VALUE );
            EXPECT_TRUE( mpdefaultone.isValid() );
            EXPECT_EQ( "", mpdefaultone.name() );
            EXPECT_EQ( size_t(0), mpdefaultone.nameLength() );
            EXPECT_EQ( VALUE, mpdefaultone.value() );
            EXPECT_EQ( 0, mpdefaultone.exponent() );

            MicPower mpnamed( NAME, VALUE, EXPONENT );
            EXPECT_TRUE( mpnamed.isValid() );
            EXPECT_EQ( NAME, mpnamed.name() );
            EXPECT_EQ( NAME_LENGTH, mpnamed.nameLength() );
            EXPECT_EQ( VALUE, mpnamed.value() );
            EXPECT_EQ( EXPONENT, mpnamed.exponent() );

            MicPower mpnameddefaultone( NAME, VALUE );
            EXPECT_TRUE( mpnameddefaultone.isValid() );
            EXPECT_EQ( NAME, mpnameddefaultone.name() );
            EXPECT_EQ( NAME_LENGTH, mpnameddefaultone.nameLength() );
            EXPECT_EQ( VALUE, mpnameddefaultone.value() );
            EXPECT_EQ( 0, mpnameddefaultone.exponent() );
        }

        {   // Copy constructor tests
            MicPower mpthat( NAME,VALUE,EXPONENT );
            MicPower mpthis( mpthat );
            EXPECT_TRUE( mpthis.isValid() );
            EXPECT_EQ( NAME, mpthis.name() );
            EXPECT_EQ( NAME_LENGTH, mpthis.nameLength() );
            EXPECT_EQ( VALUE, mpthis.value() );
            EXPECT_EQ( EXPONENT, mpthis.exponent() );
        }

        {   // Sample assignment tests
            MicPower mp;
            mp = VALUE;
            EXPECT_TRUE( mp.isValid() );
            EXPECT_EQ( "", mp.name() );
            EXPECT_EQ( size_t(0), mp.nameLength() );
            EXPECT_EQ( VALUE, mp.value() );
            EXPECT_EQ( "W", mp.unit() );
        }

        {   // Is available tests
            MicPower mp( NAME, (VALUE | NOT_AVAILABLE), EXPONENT );
            EXPECT_FALSE( mp.isAvailable() );
            EXPECT_TRUE( mp.isLowerThresholdViolation() );
            EXPECT_TRUE( mp.isUpperThresholdViolation() );
        }

        {   // Lower limit violation tests
            MicPower mp( NAME, (VALUE | LOWER_THRESHOLD_VIOLATION), EXPONENT );
            EXPECT_TRUE( mp.isAvailable() );
            EXPECT_TRUE( mp.isLowerThresholdViolation() );
            EXPECT_FALSE( mp.isUpperThresholdViolation() );
        }

        {   // Upper limit violation tests
            MicPower mp( NAME, (VALUE | UPPER_THRESHOLD_VIOLATION), EXPONENT );
            EXPECT_TRUE( mp.isAvailable() );
            EXPECT_FALSE( mp.isLowerThresholdViolation() );
            EXPECT_TRUE( mp.isUpperThresholdViolation() );
        }

        {   // Copy assignment tests
            MicPower mpthat( NAME, VALUE, EXPONENT );
            MicPower mpthis;
            mpthis = mpthat;
            EXPECT_TRUE( mpthis.isValid() );
            EXPECT_EQ( NAME, mpthis.name() );
            EXPECT_EQ( NAME_LENGTH, mpthis.nameLength() );
            EXPECT_EQ( VALUE, mpthis.value() );
            EXPECT_EQ( EXPONENT, mpthis.exponent() );
        }

        {   // Compare tests
            MicPower mplow( "low", 200, EXPONENT );
            MicPower mpmid( "mid", 500, EXPONENT );
            MicPower mpsame( "same", 500, EXPONENT );
            MicPower mphigh( "high", 750, EXPONENT );
            EXPECT_EQ( 1, mpmid.compare( mplow ) );
            EXPECT_EQ( 0, mpmid.compare( mpsame ) );
            EXPECT_EQ( -1, mpmid.compare( mphigh ) );
        }

        {   // Unit tests
            MicPower mpu( "mpu", VALUE, -6 );
            MicPower mpm( "mpm", VALUE, -3 );
            MicPower mpw( "mpw", VALUE, 0 );
            MicPower mpk( "mpk", VALUE, 3 );
            MicPower mpwhat( "what", VALUE, 1 );
            EXPECT_EQ( "uW", mpu.unit() );
            EXPECT_EQ( "mW", mpm.unit() );
            EXPECT_EQ( "W", mpw.unit() );
            EXPECT_EQ( "kW", mpk.unit() );
            EXPECT_EQ( "?", mpwhat.unit() );
        }

        {   // Scaled value tests
            MicPower mp( NAME, VALUE, EXPONENT );
            EXPECT_EQ( (VALUE*SCALAR), mp.scaledValue( MicSampleBase::eMicro ) );
            EXPECT_EQ( (VALUE/SCALAR), mp.scaledValue( MicSampleBase::eBase ) );
        }

        {   // Clear value tests
            MicPower mp( NAME, VALUE, EXPONENT );
            mp.clearValue();
            EXPECT_FALSE( mp.isValid() );
            EXPECT_EQ( NAME, mp.name() );
            EXPECT_EQ( 0, mp.value() );
            EXPECT_EQ( EXPONENT, mp.exponent() );
        }

        {   // Set exponent tests
            MicPower mp( NAME, VALUE, EXPONENT );
            mp.setExponent( MicSampleBase::eKilo );
            EXPECT_EQ( MicSampleBase::eKilo, mp.exponent() );
        }

        {   // Set name tests
            MicPower mp( NAME, VALUE, EXPONENT );
            mp.setName( "Way Cooler Name" );
            EXPECT_EQ( "Way Cooler Name", mp.name() );
        }

    } // sdk.TC_KNL_mpsstools_MicPower_001

}   // namespace micmgmt
