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
#include "MicMemory.hpp"

namespace micmgmt
{
    using namespace std;

    TEST(sdk, TC_KNL_mpsstools_MicMemory_001)
    {
        const int VALUE = 950;
        const int EXPONENT = MicSampleBase::eKilo;
        const double SCALAR = 1024;
        const std::string NAME = "memory";
        const size_t NAME_LENGTH = 6;
        const uint32_t NOT_AVAILABLE = 0xc0000000;
        const uint32_t LOWER_THRESHOLD_VIOLATION = 0x40000000;
        const uint32_t UPPER_THRESHOLD_VIOLATION = 0x80000000;

        {   // Empty tests
            MicMemory mm;
            EXPECT_FALSE( mm.isValid() );
            EXPECT_TRUE( mm.isAvailable() );
            EXPECT_FALSE( mm.isLowerThresholdViolation() );
            EXPECT_FALSE( mm.isUpperThresholdViolation() );
            EXPECT_EQ( 0, mm.value() );
            EXPECT_EQ( "", mm.unit() );
            EXPECT_EQ( "", mm.name() );
            EXPECT_EQ( size_t(0), mm.nameLength() );
        }

        {   // Value constructor tests
            MicMemory mm( VALUE, EXPONENT );
            EXPECT_TRUE( mm.isValid() );
            EXPECT_EQ( VALUE, mm.value() );
            EXPECT_EQ( EXPONENT, mm.exponent() );
            EXPECT_EQ( "", mm.name() );
            EXPECT_EQ( size_t(0), mm.nameLength() );

            MicMemory mmdefaultone( VALUE );
            EXPECT_TRUE( mmdefaultone.isValid() );
            EXPECT_EQ( VALUE, mmdefaultone.value() );
            EXPECT_EQ( MicSampleBase::eBase, mmdefaultone.exponent() );
            EXPECT_EQ( "", mmdefaultone.name() );
            EXPECT_EQ( size_t(0), mmdefaultone.nameLength() );

            MicMemory mmnamed( NAME, VALUE, EXPONENT );
            EXPECT_TRUE( mmnamed.isValid() );
            EXPECT_EQ( VALUE, mmnamed.value() );
            EXPECT_EQ( EXPONENT, mmnamed.exponent() );
            EXPECT_EQ( NAME, mmnamed.name() );
            EXPECT_EQ( NAME_LENGTH, mmnamed.nameLength() );

            MicMemory mmnameddefaultone( NAME, VALUE );
            EXPECT_TRUE( mmnameddefaultone.isValid() );
            EXPECT_EQ( VALUE, mmnameddefaultone.value() );
            EXPECT_EQ( MicSampleBase::eBase, mmnameddefaultone.exponent() );
            EXPECT_EQ( NAME, mmnameddefaultone.name() );
            EXPECT_EQ( NAME_LENGTH, mmnameddefaultone.nameLength() );
        }

        {   // Copy constructor tests
            MicMemory mmthat( NAME, VALUE, EXPONENT );
            MicMemory mmthis( mmthat );
            EXPECT_TRUE( mmthis.isValid() );
            EXPECT_EQ( VALUE, mmthis.value() );
            EXPECT_EQ( EXPONENT, mmthis.exponent() );
            EXPECT_EQ( NAME, mmthis.name() );
            EXPECT_EQ( NAME_LENGTH, mmthis.nameLength() );
        }

        {   // Sample assignement tests
            MicMemory mm;
            mm = VALUE;
            EXPECT_TRUE( mm.isValid() );
            EXPECT_EQ( VALUE, mm.value() );
            EXPECT_EQ( "Byte", mm.unit() );
            EXPECT_EQ( "", mm.name() );
            EXPECT_EQ( size_t(0), mm.nameLength() );
        }

        {   // Copy assigment tests
            MicMemory mmthat( NAME, VALUE, EXPONENT );
            MicMemory mmthis;
            mmthis = mmthat;
            EXPECT_TRUE( mmthis.isValid() );
            EXPECT_EQ( VALUE, mmthis.value() );
            EXPECT_EQ( EXPONENT, mmthis.exponent() );
            EXPECT_EQ( NAME, mmthis.name() );
            EXPECT_EQ( NAME_LENGTH, mmthis.nameLength() );
        }

        {   // Is available tests
            MicMemory mm( NAME, (VALUE | NOT_AVAILABLE), EXPONENT );
            EXPECT_FALSE( mm.isAvailable() );
            EXPECT_TRUE( mm.isLowerThresholdViolation() );
            EXPECT_TRUE( mm.isUpperThresholdViolation() );
        }

        {   // Lower limit violation tests
            MicMemory mm( NAME, (VALUE | LOWER_THRESHOLD_VIOLATION), EXPONENT );
            EXPECT_TRUE( mm.isAvailable() );
            EXPECT_TRUE( mm.isLowerThresholdViolation() );
            EXPECT_FALSE( mm.isUpperThresholdViolation() );
        }

        {   // Upper limit violation tests
            MicMemory mm( NAME, (VALUE | UPPER_THRESHOLD_VIOLATION), EXPONENT );
            EXPECT_TRUE( mm.isAvailable() );
            EXPECT_FALSE( mm.isLowerThresholdViolation() );
            EXPECT_TRUE( mm.isUpperThresholdViolation() );
        }

        {   // Compare tests
            MicMemory mmlow( "low", VALUE, MicSampleBase::eBase );
            MicMemory mmmid( "mid", VALUE, MicSampleBase::eKilo );
            MicMemory mmsame( "same", VALUE, MicSampleBase::eKilo );
            MicMemory mmhigh( "high", VALUE, MicSampleBase::eMega );
            EXPECT_EQ( 1, mmmid.compare( mmlow ) );
            EXPECT_EQ( 0, mmmid.compare( mmsame ) );
            EXPECT_EQ( -1, mmmid.compare( mmhigh ) );
        }

        {   // Unit tests
            MicMemory mmb( "mmb", VALUE, MicSampleBase::eBase );
            MicMemory mmk( "mmk", VALUE, MicSampleBase::eKilo );
            MicMemory mmm( "mmm", VALUE, MicSampleBase::eMega );
            MicMemory mmg( "mmg", VALUE, MicSampleBase::eGiga );
            MicMemory mmwhat( "what", VALUE, 1 );
            EXPECT_EQ( "Byte", mmb.unit() );
            EXPECT_EQ( "kB", mmk.unit() );
            EXPECT_EQ( "MB", mmm.unit() );
            EXPECT_EQ( "GB", mmg.unit() );
            EXPECT_EQ( "?", mmwhat.unit() );
        }

        {   // Scaled value tests
            MicMemory mm( NAME, VALUE, MicSampleBase::eMega );
            EXPECT_EQ( (VALUE/SCALAR), mm.scaledValue( MicSampleBase::eGiga ) );
            EXPECT_EQ( (VALUE*SCALAR), mm.scaledValue( MicSampleBase::eKilo ) );
            EXPECT_EQ( (VALUE*SCALAR*SCALAR), mm.scaledValue( MicSampleBase::eBase ) );
        }

        {   // Clear value tests
            MicMemory mm( NAME, VALUE, EXPONENT );
            mm.clearValue();
            EXPECT_FALSE( mm.isValid() );
            EXPECT_EQ( NAME, mm.name() );
            EXPECT_EQ( 0, mm.value() );
            EXPECT_EQ( EXPONENT, mm.exponent() );
        }

        {   // Set exponent tests
            MicMemory mm( NAME, VALUE, MicSampleBase::eGiga );
            mm.setExponent( EXPONENT );
            EXPECT_EQ( EXPONENT, mm.exponent() );
        }

        {   // Set name tests
            MicMemory mm( NAME, VALUE, EXPONENT );
            mm.setName( "Way Cooler Name" );
            EXPECT_EQ( "Way Cooler Name", mm.name() );
        }

    } // sdk.TC_KNL_mpsstools_MicMemory_001

}   // namespace micmgmt
