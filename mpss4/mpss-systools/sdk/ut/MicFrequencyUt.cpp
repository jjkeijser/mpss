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
#include "MicFrequency.hpp"

namespace micmgmt
{
    using namespace std;

    TEST(sdk, TC_KNL_mpsstools_MicFrequency_001)
    {
        const int VALUE = 900;
        const int EXPONENT = MicSampleBase::eMega;
        const double SCALAR = 1000;
        const std::string NAME = "frequency";
        const size_t NAME_LENGTH = 9;
        const uint32_t NOT_AVAILABLE = 0xc0000000;
        const uint32_t LOWER_THRESHOLD_VIOLATION = 0x40000000;
        const uint32_t UPPER_THRESHOLD_VIOLATION = 0x80000000;

        {   // Empty tests
            MicFrequency mf;
            EXPECT_FALSE( mf.isValid() );
            EXPECT_TRUE( mf.isAvailable() );
            EXPECT_FALSE( mf.isLowerThresholdViolation() );
            EXPECT_FALSE( mf.isUpperThresholdViolation() );
            EXPECT_EQ( 0, mf.value() );
            EXPECT_EQ( "", mf.unit() );
            EXPECT_EQ( "", mf.name() );
            EXPECT_EQ( size_t(0), mf.nameLength() );
        }

        {   // Value constructor tests
            MicFrequency mf( VALUE, EXPONENT );
            EXPECT_TRUE( mf.isValid() );
            EXPECT_EQ( VALUE, mf.value() );
            EXPECT_EQ( EXPONENT, mf.exponent() );
            EXPECT_EQ( "", mf.name() );
            EXPECT_EQ( size_t(0), mf.nameLength() );

            MicFrequency mfdefaultone( VALUE );
            EXPECT_TRUE( mfdefaultone.isValid() );
            EXPECT_EQ( VALUE, mfdefaultone.value() );
            EXPECT_EQ( 0, mfdefaultone.exponent() );
            EXPECT_EQ( "", mfdefaultone.name() );
            EXPECT_EQ( size_t(0), mfdefaultone.nameLength() );

            MicFrequency mfnamed( NAME, VALUE, EXPONENT );
            EXPECT_TRUE( mfnamed.isValid() );
            EXPECT_EQ( VALUE, mfnamed.value() );
            EXPECT_EQ( EXPONENT, mfnamed.exponent() );
            EXPECT_EQ( NAME, mfnamed.name() );
            EXPECT_EQ( NAME_LENGTH, mfnamed.nameLength() );

            MicFrequency mfnameddefaultone( NAME, VALUE );
            EXPECT_TRUE( mfnameddefaultone.isValid() );
            EXPECT_EQ( VALUE, mfnameddefaultone.value() );
            EXPECT_EQ( 0, mfnameddefaultone.exponent() );
            EXPECT_EQ( NAME, mfnameddefaultone.name() );
            EXPECT_EQ( NAME_LENGTH, mfnameddefaultone.nameLength() );
        }

        {   // Copy constructor tests
            MicFrequency mfthat( NAME, VALUE, EXPONENT );
            MicFrequency mfthis( mfthat );
            EXPECT_TRUE( mfthis.isValid() );
            EXPECT_EQ( VALUE, mfthis.value() );
            EXPECT_EQ( EXPONENT, mfthis.exponent() );
            EXPECT_EQ( NAME, mfthis.name() );
            EXPECT_EQ( NAME_LENGTH, mfthis.nameLength() );
        }

        {   // Sample assignement tests
            MicFrequency mf;
            mf = VALUE;
            EXPECT_TRUE( mf.isValid() );
            EXPECT_EQ( VALUE, mf.value() );
            EXPECT_EQ( "Hz", mf.unit() );
            EXPECT_EQ( "", mf.name() );
            EXPECT_EQ( size_t(0), mf.nameLength() );
        }

        {   // Copy assigment tests
            MicFrequency mfthat( NAME, VALUE, EXPONENT );
            MicFrequency mfthis;
            mfthis = mfthat;
            EXPECT_TRUE( mfthis.isValid() );
            EXPECT_EQ( VALUE, mfthis.value() );
            EXPECT_EQ( EXPONENT, mfthis.exponent() );
            EXPECT_EQ( NAME, mfthis.name() );
            EXPECT_EQ( NAME_LENGTH, mfthis.nameLength() );
        }

        {   // Is available tests
            MicFrequency mf( NAME, (VALUE | NOT_AVAILABLE), EXPONENT );
            EXPECT_FALSE( mf.isAvailable() );
            EXPECT_TRUE( mf.isLowerThresholdViolation() );
            EXPECT_TRUE( mf.isUpperThresholdViolation() );
        }

        {   // Lower limit violation tests
            MicFrequency mf( NAME, (VALUE | LOWER_THRESHOLD_VIOLATION), EXPONENT );
            EXPECT_TRUE( mf.isAvailable() );
            EXPECT_TRUE( mf.isLowerThresholdViolation() );
            EXPECT_FALSE( mf.isUpperThresholdViolation() );
        }

        {   // Upper limit violation tests
            MicFrequency mf( NAME, (VALUE | UPPER_THRESHOLD_VIOLATION), EXPONENT );
            EXPECT_TRUE( mf.isAvailable() );
            EXPECT_FALSE( mf.isLowerThresholdViolation() );
            EXPECT_TRUE( mf.isUpperThresholdViolation() );
        }

        {   // Compare tests
            MicFrequency mflow( "low", 900, MicSampleBase::eMilli );
            MicFrequency mfmid( "mid", 250, MicSampleBase::eBase );
            MicFrequency mfsame( "same", 250, MicSampleBase::eBase );
            MicFrequency mfhigh( "high", 1000, MicSampleBase::eKilo );
            EXPECT_EQ( 1, mfmid.compare( mflow ) );
            EXPECT_EQ( 0, mfmid.compare( mfsame ) );
            EXPECT_EQ( -1, mfmid.compare( mfhigh ) );
        }

        {   // Unit tests
            MicFrequency mfu( "mfu", VALUE, MicSampleBase::eMicro );
            MicFrequency mfmi( "mfmi", VALUE, MicSampleBase::eMilli );
            MicFrequency mfh( "mfh", VALUE, MicSampleBase::eBase );
            MicFrequency mfk( "mfk", VALUE, MicSampleBase::eKilo );
            MicFrequency mfm( "mfm", VALUE, MicSampleBase::eMega );
            MicFrequency mfg( "mfg", VALUE, MicSampleBase::eGiga );
            MicFrequency mfwhat( "what", VALUE, 1);
            EXPECT_EQ( "uHz", mfu.unit() );
            EXPECT_EQ( "mHz", mfmi.unit() );
            EXPECT_EQ( "Hz", mfh.unit() );
            EXPECT_EQ( "kHz", mfk.unit() );
            EXPECT_EQ( "MHz", mfm.unit() );
            EXPECT_EQ( "GHz", mfg.unit() );
            EXPECT_EQ( "?", mfwhat.unit() );
        }

        {   // Scaled value tests
            MicFrequency mf( NAME, VALUE, MicSampleBase::eMega );
            EXPECT_EQ( (VALUE*SCALAR), mf.scaledValue( MicSampleBase::eKilo ) );
            EXPECT_EQ( (VALUE/SCALAR), mf.scaledValue( MicSampleBase::eGiga ) );
        }

        {   // Clear value tests
            MicFrequency mf( NAME, VALUE, EXPONENT );
            mf.clearValue();
            EXPECT_FALSE( mf.isValid() );
            EXPECT_EQ( NAME, mf.name() );
            EXPECT_EQ( 0, mf.value() );
            EXPECT_EQ( EXPONENT, mf.exponent() );
        }

        {   // Set exponent tests
            MicFrequency mf( NAME, VALUE, -EXPONENT );
            mf.setExponent( EXPONENT );
            EXPECT_EQ( EXPONENT, mf.exponent() );
        }

        {   // Set name tests
            MicFrequency mf( NAME, VALUE, EXPONENT );
            mf.setName( "Way Cooler Name" );
            EXPECT_EQ( "Way Cooler Name", mf.name() );
        }

    } // sdk.TC_KNL_mpsstools_MicFrequency_001

}   // namespace micmgmt
