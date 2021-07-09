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
#include "MicSpeed.hpp"

namespace micmgmt
{
    using namespace std;

    TEST(sdk, TC_KNL_mpsstools_MicSpeed_001)
    {
        const int VALUE = 750;
        const int EXPONENT = MicSampleBase::eGiga;
        const double SCALAR = 1000;
        const std::string NAME = "speed";
        const size_t NAME_LENGTH = 5;
        const uint32_t NOT_AVAILABLE = 0xc0000000;
        const uint32_t LOWER_THRESHOLD_VIOLATION = 0x40000000;
        const uint32_t UPPER_THRESHOLD_VIOLATION = 0x80000000;

        {   // Empty tests
            MicSpeed ms;
            EXPECT_FALSE( ms.isValid() );
            EXPECT_TRUE( ms.isAvailable() );
            EXPECT_FALSE( ms.isLowerThresholdViolation() );
            EXPECT_FALSE( ms.isUpperThresholdViolation() );
            EXPECT_EQ( "", ms.name() );
            EXPECT_EQ( size_t(0), ms.nameLength() );
            EXPECT_EQ( 0, ms.value() );
            EXPECT_EQ( "", ms.unit() );
        }

        {   // Value constructor tests
            MicSpeed ms( VALUE, EXPONENT );
            EXPECT_TRUE( ms.isValid() );
            EXPECT_EQ( "", ms.name() );
            EXPECT_EQ( size_t(0), ms.nameLength() );
            EXPECT_EQ( VALUE, ms.value() );
            EXPECT_EQ( EXPONENT, ms.exponent() );

            MicSpeed msdefaultone( VALUE );
            EXPECT_TRUE( msdefaultone.isValid() );
            EXPECT_EQ( "", msdefaultone.name() );
            EXPECT_EQ( size_t(0), msdefaultone.nameLength() );
            EXPECT_EQ( VALUE, msdefaultone.value() );
            EXPECT_EQ( 0, msdefaultone.exponent() );

            MicSpeed msnamed( NAME, VALUE, EXPONENT );
            EXPECT_TRUE( msnamed.isValid() );
            EXPECT_EQ( NAME, msnamed.name() );
            EXPECT_EQ( NAME_LENGTH, msnamed.nameLength() );
            EXPECT_EQ( VALUE, msnamed.value() );
            EXPECT_EQ( EXPONENT, msnamed.exponent() );

            MicSpeed msnameddefaultone( NAME, VALUE );
            EXPECT_TRUE( msnameddefaultone.isValid() );
            EXPECT_EQ( NAME, msnameddefaultone.name() );
            EXPECT_EQ( NAME_LENGTH, msnameddefaultone.nameLength() );
            EXPECT_EQ( VALUE, msnameddefaultone.value() );
            EXPECT_EQ( 0, msnameddefaultone.exponent() );
        }

        {   // Copy constructor tests
            MicSpeed msthat( NAME, VALUE, EXPONENT );
            MicSpeed msthis( msthat );
            EXPECT_TRUE( msthis.isValid() );
            EXPECT_EQ( NAME, msthis.name() );
            EXPECT_EQ( NAME_LENGTH, msthis.nameLength() );
            EXPECT_EQ( VALUE, msthis.value() );
            EXPECT_EQ( EXPONENT, msthis.exponent() );
        }

        {   // Sample assignment tests
            MicSpeed ms( NAME, VALUE-2, EXPONENT );
            ms = VALUE;
            EXPECT_TRUE( ms.isValid() );
            EXPECT_EQ( NAME, ms.name() );
            EXPECT_EQ( NAME_LENGTH, ms.nameLength() );
            EXPECT_EQ( VALUE, ms.value() );
            EXPECT_EQ( EXPONENT, ms.exponent() );
        }

        {   // Copy assignment tests
            MicSpeed msthat( NAME, VALUE, EXPONENT );
            MicSpeed msthis;
            msthis = msthat;
            EXPECT_TRUE( msthis.isValid() );
            EXPECT_EQ( NAME, msthis.name() );
            EXPECT_EQ( NAME_LENGTH, msthis.nameLength() );
            EXPECT_EQ( VALUE, msthis.value() );
            EXPECT_EQ( EXPONENT, msthis.exponent() );
        }

        {   // is Available tests
            MicSpeed ms( NAME, (VALUE | NOT_AVAILABLE), EXPONENT );
            EXPECT_FALSE( ms.isAvailable() );
            EXPECT_TRUE( ms.isLowerThresholdViolation() );
            EXPECT_TRUE( ms.isUpperThresholdViolation() );
        }

        {   // Lower limit violation tests
            MicSpeed ms( NAME, (VALUE | LOWER_THRESHOLD_VIOLATION), EXPONENT );
            EXPECT_TRUE( ms.isAvailable() );
            EXPECT_TRUE( ms.isLowerThresholdViolation() );
            EXPECT_FALSE( ms.isUpperThresholdViolation() );
        }

        {   // Upper limit violation tests
            MicSpeed ms( NAME, (VALUE | UPPER_THRESHOLD_VIOLATION), EXPONENT );
            EXPECT_TRUE( ms.isAvailable() );
            EXPECT_FALSE( ms.isLowerThresholdViolation() );
            EXPECT_TRUE( ms.isUpperThresholdViolation() );
        }

        {   // Compare tests
            MicSpeed mslow( "low", VALUE, MicSampleBase::eBase );
            MicSpeed msmid( "mid", VALUE, MicSampleBase::eKilo );
            MicSpeed mssame( "same", VALUE, MicSampleBase::eKilo );
            MicSpeed mshigh( "high", VALUE, MicSampleBase::eMega );
            EXPECT_EQ( 1, msmid.compare( mslow ) );
            EXPECT_EQ( 0, msmid.compare( mssame ) );
            EXPECT_EQ( -1, msmid.compare( mshigh ) );
        }

        {   // Unit tests
            MicSpeed mst( "mst", VALUE, MicSampleBase::eBase );
            MicSpeed msk( "msk", VALUE, MicSampleBase::eKilo );
            MicSpeed msm( "msm", VALUE, MicSampleBase::eMega );
            MicSpeed msg( "msg", VALUE, MicSampleBase::eGiga );
            MicSpeed mswhat( "mswhat", VALUE, 50);
            EXPECT_EQ( "T/s", mst.unit() );
            EXPECT_EQ( "kT/s", msk.unit() );
            EXPECT_EQ( "MT/s", msm.unit() );
            EXPECT_EQ( "GT/s", msg.unit() );
            EXPECT_EQ( "?", mswhat.unit() );
        }

        {   // Scaled value tests
            MicSpeed ms( NAME, VALUE, EXPONENT );
            EXPECT_EQ( (VALUE*SCALAR), ms.scaledValue( MicSampleBase::eMega ) );
            EXPECT_EQ( (VALUE*SCALAR*SCALAR), ms.scaledValue( MicSampleBase::eKilo ) );
            EXPECT_EQ( (VALUE*SCALAR*SCALAR*SCALAR), ms.scaledValue( MicSampleBase::eBase ) );
        }

        {   // Clear value tests
            MicSpeed ms( NAME, VALUE, EXPONENT );
            ms.clearValue();
            EXPECT_FALSE( ms.isValid() );
            EXPECT_EQ( NAME, ms.name() );
            EXPECT_EQ( 0, ms.value() );
            EXPECT_EQ( EXPONENT, ms.exponent() );
        }

        {   // Set exponent tests
            MicSpeed ms( NAME, VALUE, MicSampleBase::eBase );
            ms.setExponent( EXPONENT );
            EXPECT_EQ( EXPONENT, ms.exponent() );
        }

        {   // Set name tests
            MicSpeed ms( NAME, VALUE, EXPONENT );
            ms.setName( "Way Cooler Name" );
            EXPECT_EQ( "Way Cooler Name", ms.name() );
        }

    } // sdk.TC_KNL_mpsstools_MicSpeed_001

}   // namespace micmgmt
