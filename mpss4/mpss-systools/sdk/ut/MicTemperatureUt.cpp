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
#include "MicTemperature.hpp"

namespace micmgmt
{
    using namespace std;

    TEST(sdk, TC_KNL_mpsstools_MicTemperature_001)
    {
        const int VALUE = 80;
        const double VALUE_FAHRENHEIT = (VALUE*9.0/5) + 32;
        const int EXPONENT = MicSampleBase::eBase;
        const double SCALAR = 1000;
        const std::string NAME = "temperature";
        const size_t NAME_LENGTH = 11;
        const uint32_t NOT_AVAILABLE = 0xc0000000;
        const uint32_t LOWER_THRESHOLD_VIOLATION = 0x40000000;
        const uint32_t UPPER_THRESHOLD_VIOLATION = 0x80000000;

        {   // Empty tests
            MicTemperature mt;
            EXPECT_FALSE( mt.isValid() );
            EXPECT_TRUE( mt.isAvailable() );
            EXPECT_FALSE( mt.isLowerThresholdViolation() );
            EXPECT_FALSE( mt.isUpperThresholdViolation() );
            EXPECT_EQ( "", mt.name() );
            EXPECT_EQ( size_t(0), mt.nameLength() );
            EXPECT_EQ( EXPONENT, mt.value() );
            EXPECT_EQ( "", mt.unit() );
        }

        {   // Value constructor tests
            MicTemperature mt( VALUE, EXPONENT );
            EXPECT_TRUE( mt.isValid() );
            EXPECT_EQ( "", mt.name() );
            EXPECT_EQ( size_t(0), mt.nameLength() );
            EXPECT_EQ( VALUE, mt.value() );
            EXPECT_EQ( EXPONENT, mt.exponent() );

            MicTemperature mtdefaultone( VALUE );
            EXPECT_TRUE( mtdefaultone.isValid() );
            EXPECT_EQ( "", mtdefaultone.name() );
            EXPECT_EQ( size_t(0), mtdefaultone.nameLength() );
            EXPECT_EQ( VALUE, mtdefaultone.value() );
            EXPECT_EQ( EXPONENT, mtdefaultone.exponent() );

            MicTemperature mtnamed( NAME, VALUE, EXPONENT );
            EXPECT_TRUE( mtnamed.isValid() );
            EXPECT_EQ( NAME, mtnamed.name() );
            EXPECT_EQ( NAME_LENGTH, mtnamed.nameLength() );
            EXPECT_EQ( VALUE, mtnamed.value() );
            EXPECT_EQ( EXPONENT, mtnamed.exponent() );

            MicTemperature mtnameddefaultone( NAME, VALUE );
            EXPECT_TRUE( mtnameddefaultone.isValid() );
            EXPECT_EQ( NAME, mtnameddefaultone.name() );
            EXPECT_EQ( NAME_LENGTH, mtnameddefaultone.nameLength() );
            EXPECT_EQ( VALUE, mtnameddefaultone.value() );
            EXPECT_EQ( EXPONENT, mtnameddefaultone.exponent() );
        }

        {   // Copy constructor tests
            MicTemperature mtthat( NAME, VALUE, EXPONENT );
            MicTemperature mtthis( mtthat );
            EXPECT_TRUE( mtthis.isValid() );
            EXPECT_EQ( NAME, mtthis.name() );
            EXPECT_EQ( NAME_LENGTH, mtthis.nameLength() );
            EXPECT_EQ( VALUE, mtthis.value() );
            EXPECT_EQ( EXPONENT, mtthis.exponent() );
        }

        {   // Sample assignment tests
            MicTemperature mt;
	    mt = VALUE;
            EXPECT_TRUE( mt.isValid() );
            EXPECT_EQ( "", mt.name() );
            EXPECT_EQ( size_t(0), mt.nameLength() );
            EXPECT_EQ( VALUE, mt.value() );
            EXPECT_EQ( EXPONENT, mt.exponent() );
        }


        {   // Copy assignment tests
            MicTemperature mtthat( NAME, VALUE, EXPONENT );
            MicTemperature mtthis;
            mtthis = mtthat;
            EXPECT_TRUE( mtthis.isValid() );
            EXPECT_EQ( NAME, mtthis.name() );
            EXPECT_EQ( NAME_LENGTH, mtthis.nameLength() );
            EXPECT_EQ( VALUE, mtthis.value() );
            EXPECT_EQ( EXPONENT, mtthis.exponent() );
        }

        {   // Celsius tests
            MicTemperature mt( NAME, VALUE, EXPONENT );
            EXPECT_EQ( VALUE, mt.celsius() );
        }

        {   // Farenheit tests
            MicTemperature mt( NAME, VALUE, EXPONENT );
            EXPECT_EQ( VALUE_FAHRENHEIT, mt.fahrenheit() );
        }

        {   // Is available tests
            MicTemperature mt( NAME, (VALUE | NOT_AVAILABLE), EXPONENT );
            EXPECT_FALSE( mt.isAvailable() );
            EXPECT_TRUE( mt.isLowerThresholdViolation() );
            EXPECT_TRUE( mt.isUpperThresholdViolation() );
        }

        {   // Lower limit violation tests
            MicTemperature mt( NAME, (VALUE | LOWER_THRESHOLD_VIOLATION), EXPONENT);
            EXPECT_TRUE( mt.isAvailable() );
            EXPECT_TRUE( mt.isLowerThresholdViolation() );
            EXPECT_FALSE( mt.isUpperThresholdViolation() );
        }

        {   // Upper limit violation tests
            MicTemperature mt( NAME, (VALUE | UPPER_THRESHOLD_VIOLATION), EXPONENT);
            EXPECT_TRUE( mt.isAvailable() );
            EXPECT_FALSE( mt.isLowerThresholdViolation() );
            EXPECT_TRUE( mt.isUpperThresholdViolation() );
        }

        {   // Compare tests
            MicTemperature mtlow( "low", 2, EXPONENT );
            MicTemperature mtmid( "mid", 60, EXPONENT );
            MicTemperature mtsame( "same", 60, EXPONENT );
            MicTemperature mthigh( "high", 105, EXPONENT );
            EXPECT_EQ( 1, mtmid.compare( mtlow ) );
            EXPECT_EQ( EXPONENT, mtmid.compare( mtsame ) );
            EXPECT_EQ( -1, mtmid.compare( mthigh ) );
        }

        {   // Unit tests
            MicTemperature mt( VALUE, EXPONENT );
            MicTemperature mtwhat( "what", VALUE, 1 );
            EXPECT_EQ( "", mt.unit() );
            EXPECT_EQ( "?", mtwhat.unit() );
        }

        {   // Scaled value tests
            MicTemperature mt( NAME, VALUE, EXPONENT );
            EXPECT_EQ( (VALUE*SCALAR), mt.scaledValue( MicSampleBase::eMilli ) );
            EXPECT_EQ( (VALUE/SCALAR), mt.scaledValue( MicSampleBase::eKilo ) );
        }

        {   // Clear value tests
            MicTemperature mt( NAME, VALUE, EXPONENT );
            mt.clearValue();
            EXPECT_FALSE( mt.isValid() );
            EXPECT_EQ( NAME, mt.name() );
            EXPECT_EQ( 0, mt.value() );
            EXPECT_EQ( EXPONENT, mt.exponent() );
        }

        {   // Set exponent tests
            MicTemperature mt( NAME, VALUE, MicSampleBase::eGiga );
            mt.setExponent( EXPONENT );
            EXPECT_EQ( EXPONENT, mt.exponent() );
        }

        {   // Set name tests
            MicTemperature mt( NAME, VALUE, EXPONENT );
            mt.setName( "Way Cooler Name" );
            EXPECT_EQ( "Way Cooler Name", mt.name() );
        }

    } // sdk.TC_KNL_mpsstools_MicTemperature_001

}   // namespace micmgmt
