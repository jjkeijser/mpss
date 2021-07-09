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
#include "MicVoltage.hpp"

namespace micmgmt
{
    using namespace std;

    TEST(sdk, TC_KNL_mpsstools_MicVoltage_001)
    {
        const int VALUE = 500;
        const int EXPONENT = MicSampleBase::eMilli;
        const double SCALAR = 1000;
        const std::string NAME = "voltage";
        const size_t NAME_LENGTH = 7;
        const uint32_t NOT_AVAILABLE = 0xc0000000;
        const uint32_t LOWER_THRESHOLD_VIOLATION = 0x40000000;
        const uint32_t UPPER_THRESHOLD_VIOLATION = 0x80000000;

        {   // Empty tests
            MicVoltage mv;
            EXPECT_FALSE( mv.isValid() );
            EXPECT_TRUE( mv.isAvailable() );
            EXPECT_FALSE( mv.isLowerThresholdViolation() );
            EXPECT_FALSE( mv.isUpperThresholdViolation() );
            EXPECT_EQ( 0, mv.value() );
            EXPECT_EQ( "", mv.unit() );
            EXPECT_EQ( "", mv.name() );
            EXPECT_EQ( size_t(0), mv.nameLength() );
        }

        {   // Value constructor tests
            MicVoltage mv( VALUE, EXPONENT );
            EXPECT_TRUE( mv.isValid() );
            EXPECT_EQ( VALUE, mv.value() );
            EXPECT_EQ( EXPONENT, mv.exponent() );
            EXPECT_EQ( "", mv.name() );
            EXPECT_EQ( size_t(0), mv.nameLength() );

            MicVoltage mvdefaultone( VALUE );
            EXPECT_TRUE( mvdefaultone.isValid() );
            EXPECT_EQ( VALUE, mvdefaultone.value() );
            EXPECT_EQ( 0, mvdefaultone.exponent() );
            EXPECT_EQ( "", mvdefaultone.name() );
            EXPECT_EQ( size_t(0), mvdefaultone.nameLength() );

            MicVoltage mvnamed( NAME, VALUE, EXPONENT );
            EXPECT_TRUE( mvnamed.isValid() );
            EXPECT_EQ( VALUE, mvnamed.value() );
            EXPECT_EQ( EXPONENT, mvnamed.exponent() );
            EXPECT_EQ( NAME, mvnamed.name() );
            EXPECT_EQ( NAME_LENGTH, mvnamed.nameLength() );

            MicVoltage mvnameddefaultone( NAME, VALUE );
            EXPECT_TRUE( mvnameddefaultone.isValid() );
            EXPECT_EQ( VALUE, mvnameddefaultone.value() );
            EXPECT_EQ( 0, mvnameddefaultone.exponent() );
            EXPECT_EQ( NAME, mvnameddefaultone.name() );
            EXPECT_EQ( NAME_LENGTH, mvnameddefaultone.nameLength() );
        }

        {   // Copy constructor tests
            MicVoltage mvthat( NAME, VALUE, EXPONENT );
            MicVoltage mvthis( mvthat );
            EXPECT_TRUE( mvthis.isValid() );
            EXPECT_EQ( VALUE, mvthis.value() );
            EXPECT_EQ( EXPONENT, mvthis.exponent() );
            EXPECT_EQ( NAME, mvthis.name() );
            EXPECT_EQ( NAME_LENGTH, mvthis.nameLength() );
        }

        {   // Sample assignement tests
            MicVoltage mv;
            mv = VALUE;
            EXPECT_TRUE( mv.isValid() );
            EXPECT_EQ( VALUE, mv.value() );
            EXPECT_EQ( "V", mv.unit() );
            EXPECT_EQ( "", mv.name() );
            EXPECT_EQ( size_t(0), mv.nameLength() );
        }

        {   // Copy assigment tests
            MicVoltage mvthat( NAME, VALUE, EXPONENT );
            MicVoltage mvthis;
            mvthis = mvthat;
            EXPECT_TRUE( mvthis.isValid() );
            EXPECT_EQ( VALUE, mvthis.value() );
            EXPECT_EQ( EXPONENT, mvthis.exponent() );
            EXPECT_EQ( NAME, mvthis.name() );
            EXPECT_EQ( NAME_LENGTH, mvthis.nameLength() );
        }

        {   // Is available tests
            MicVoltage mv( NAME, (VALUE | NOT_AVAILABLE), EXPONENT );
            EXPECT_FALSE( mv.isAvailable() );
            EXPECT_TRUE( mv.isLowerThresholdViolation() );
            EXPECT_TRUE( mv.isUpperThresholdViolation() );
        }

        {   // Lower limit violation tests
            MicVoltage mv( NAME, (VALUE | LOWER_THRESHOLD_VIOLATION), EXPONENT );
            EXPECT_TRUE( mv.isAvailable() );
            EXPECT_TRUE( mv.isLowerThresholdViolation() );
            EXPECT_FALSE( mv.isUpperThresholdViolation() );
        }

        {   // Upper limit violation tests
            MicVoltage mv( NAME, (VALUE | UPPER_THRESHOLD_VIOLATION), EXPONENT );
            EXPECT_TRUE( mv.isAvailable() );
            EXPECT_FALSE( mv.isLowerThresholdViolation() );
            EXPECT_TRUE( mv.isUpperThresholdViolation() );
        }

        {   // Compare tests
            MicVoltage mvlow( "low", 500, MicSampleBase::eMilli );
            MicVoltage mvmid( "mid", 250, MicSampleBase::eBase );
            MicVoltage mvsame( "same", 250, MicSampleBase::eBase );
            MicVoltage mvhigh( "high", 1000, MicSampleBase::eKilo );
            EXPECT_EQ( 1, mvmid.compare( mvlow ) );
            EXPECT_EQ( 0, mvmid.compare( mvsame ) );
            EXPECT_EQ( -1, mvmid.compare( mvhigh ) );
        }

        {   // Unit tests
            MicVoltage mvu( "mvu", VALUE, MicSampleBase::eMicro );
            MicVoltage mvm( "mvm", VALUE, MicSampleBase::eMilli );
            MicVoltage mvv( "mvv", VALUE, MicSampleBase::eBase );
            MicVoltage mvk( "mvk", VALUE, MicSampleBase::eKilo );
            MicVoltage mvwhat( "what", VALUE, 1);
            EXPECT_EQ( "uV", mvu.unit() );
            EXPECT_EQ( "mV", mvm.unit() );
            EXPECT_EQ( "V", mvv.unit() );
            EXPECT_EQ( "kV", mvk.unit() );
            EXPECT_EQ( "?", mvwhat.unit() );
        }

        {   // Scaled value tests
            MicVoltage mv( NAME, VALUE, EXPONENT );
            EXPECT_EQ( (VALUE*SCALAR), mv.scaledValue( MicSampleBase::eMicro ) );
            EXPECT_EQ( (VALUE/SCALAR/SCALAR), mv.scaledValue( MicSampleBase::eKilo ) );
        }

        {   // Clear value tests
            MicVoltage mv( NAME, VALUE, EXPONENT );
            mv.clearValue();
            EXPECT_FALSE( mv.isValid() );
            EXPECT_EQ( NAME, mv.name() );
            EXPECT_EQ( 0, mv.value() );
            EXPECT_EQ( EXPONENT, mv.exponent() );
        }

        {   // Set exponent tests
            MicVoltage mv( NAME, VALUE, MicSampleBase::eKilo );
            mv.setExponent( EXPONENT );
            EXPECT_EQ( EXPONENT, mv.exponent() );
        }

        {   // Set name tests
            MicVoltage mv( NAME, VALUE, EXPONENT );
            mv.setName( "Way Cooler Name" );
            EXPECT_EQ( "Way Cooler Name", mv.name() );
        }

    } // sdk.TC_KNL_mpsstools_MicVoltage_001

}   // namespace micmgmt
