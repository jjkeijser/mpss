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
#include "MicProcessorInfo.hpp"
#include "ProcessorInfoData_p.hpp"   // Private

namespace micmgmt
{
    using namespace std;

    TEST(sdk, TC_KNL_mpsstools_MicProcessorInfo_001)
    {
        const uint16_t TYPE = 0x1;
        const uint16_t MODEL = 0x2;
        const uint16_t FAMILY = 0x3;
        const uint16_t EXTENDED_MODEL = 0x4;
        const uint16_t EXTENDED_FAMILY = 0x5;
        const uint16_t STEPPING_ID = 0x6;
        const uint16_t SUBSTEPPING_ID = 0x7;
        const std::string STEPPING( "stepping" );
        const std::string SKU( "sku" );

        ProcessorInfoData pidata;
        pidata.mType = TYPE;
        pidata.mModel = MODEL;
        pidata.mFamily = FAMILY;
        pidata.mExtendedModel = EXTENDED_MODEL;
        pidata.mExtendedFamily = EXTENDED_FAMILY;
        pidata.mSteppingData = STEPPING_ID;
        pidata.mSubsteppingData = SUBSTEPPING_ID;
        pidata.mStepping = STEPPING;
        pidata.mSku = SKU;

        {   // Empty tests
            MicProcessorInfo mpi;
            EXPECT_FALSE( mpi.isValid() );
            EXPECT_EQ( 0x0, mpi.type().value() );
            EXPECT_EQ( 0x0, mpi.model().value() );
            EXPECT_EQ( 0x0, mpi.family().value() );
            EXPECT_EQ( 0x0, mpi.extendedModel().value() );
            EXPECT_EQ( 0x0, mpi.extendedFamily().value() );
            EXPECT_EQ( 0U, mpi.steppingId().value() );
            EXPECT_EQ( 0U, mpi.substeppingId().value() );
            EXPECT_EQ( "", mpi.stepping().value() );
            EXPECT_EQ( "", mpi.sku().value() );
            EXPECT_FALSE( mpi.type().isValid() );
            EXPECT_FALSE( mpi.model().isValid() );
            EXPECT_FALSE( mpi.family().isValid() );
            EXPECT_FALSE( mpi.extendedModel().isValid() );
            EXPECT_FALSE( mpi.extendedFamily().isValid() );
            EXPECT_FALSE( mpi.steppingId().isValid() );
            EXPECT_FALSE( mpi.substeppingId().isValid() );
            EXPECT_FALSE( mpi.stepping().isValid() );
            EXPECT_FALSE( mpi.sku().isValid() );
        }

        {   // Data constructor tests
            MicProcessorInfo mpi( pidata );
            EXPECT_TRUE( mpi.isValid() );
            EXPECT_EQ( TYPE, mpi.type().value() );
            EXPECT_EQ( MODEL, mpi.model().value() );
            EXPECT_EQ( FAMILY, mpi.family().value() );
            EXPECT_EQ( EXTENDED_MODEL, mpi.extendedModel().value() );
            EXPECT_EQ( EXTENDED_FAMILY, mpi.extendedFamily().value() );
            EXPECT_EQ( STEPPING_ID, mpi.steppingId().value() );
            EXPECT_EQ( SUBSTEPPING_ID, mpi.substeppingId().value() );
            EXPECT_EQ( STEPPING, mpi.stepping().value() );
            EXPECT_EQ( SKU, mpi.sku().value() );
            EXPECT_TRUE( mpi.type().isValid() );
            EXPECT_TRUE( mpi.model().isValid() );
            EXPECT_TRUE( mpi.family().isValid() );
            EXPECT_TRUE( mpi.extendedModel().isValid() );
            EXPECT_TRUE( mpi.extendedFamily().isValid() );
            EXPECT_TRUE( mpi.steppingId().isValid() );
            EXPECT_TRUE( mpi.substeppingId().isValid() );
            EXPECT_TRUE( mpi.stepping().isValid() );
            EXPECT_TRUE( mpi.sku().isValid() );
        }

        {   // Copy constructor tests
            MicProcessorInfo mpithat( pidata );
            MicProcessorInfo mpithis( mpithat );
            EXPECT_TRUE( mpithis.isValid() );
            EXPECT_EQ( TYPE, mpithis.type().value() );
            EXPECT_EQ( MODEL, mpithis.model().value() );
            EXPECT_EQ( FAMILY, mpithis.family().value() );
            EXPECT_EQ( EXTENDED_MODEL, mpithis.extendedModel().value() );
            EXPECT_EQ( EXTENDED_FAMILY, mpithis.extendedFamily().value() );
            EXPECT_EQ( STEPPING_ID, mpithis.steppingId().value() );
            EXPECT_EQ( SUBSTEPPING_ID, mpithis.substeppingId().value() );
            EXPECT_EQ( STEPPING, mpithis.stepping().value() );
            EXPECT_EQ( SKU, mpithis.sku().value() );
            EXPECT_TRUE( mpithis.type().isValid() );
            EXPECT_TRUE( mpithis.model().isValid() );
            EXPECT_TRUE( mpithis.family().isValid() );
            EXPECT_TRUE( mpithis.extendedModel().isValid() );
            EXPECT_TRUE( mpithis.extendedFamily().isValid() );
            EXPECT_TRUE( mpithis.steppingId().isValid() );
            EXPECT_TRUE( mpithis.substeppingId().isValid() );
            EXPECT_TRUE( mpithis.stepping().isValid() );
            EXPECT_TRUE( mpithis.sku().isValid() );
        }

        {   // Copy assignment tests
            MicProcessorInfo mpithat( pidata );
            MicProcessorInfo mpithis;
            mpithis = mpithat;
            EXPECT_TRUE( mpithis.isValid() );
            EXPECT_EQ( TYPE, mpithis.type().value() );
            EXPECT_EQ( MODEL, mpithis.model().value() );
            EXPECT_EQ( FAMILY, mpithis.family().value() );
            EXPECT_EQ( EXTENDED_MODEL, mpithis.extendedModel().value() );
            EXPECT_EQ( EXTENDED_FAMILY, mpithis.extendedFamily().value() );
            EXPECT_EQ( STEPPING_ID, mpithis.steppingId().value() );
            EXPECT_EQ( SUBSTEPPING_ID, mpithis.substeppingId().value() );
            EXPECT_EQ( STEPPING, mpithis.stepping().value() );
            EXPECT_EQ( SKU, mpithis.sku().value() );
            EXPECT_TRUE( mpithis.type().isValid() );
            EXPECT_TRUE( mpithis.model().isValid() );
            EXPECT_TRUE( mpithis.family().isValid() );
            EXPECT_TRUE( mpithis.extendedModel().isValid() );
            EXPECT_TRUE( mpithis.extendedFamily().isValid() );
            EXPECT_TRUE( mpithis.steppingId().isValid() );
            EXPECT_TRUE( mpithis.substeppingId().isValid() );
            EXPECT_TRUE( mpithis.stepping().isValid() );
            EXPECT_TRUE( mpithis.sku().isValid() );

            mpithis = mpithis; //code coverage..
            EXPECT_TRUE( mpithis.isValid() );
            EXPECT_EQ( TYPE, mpithis.type().value() );
            EXPECT_EQ( MODEL, mpithis.model().value() );
            EXPECT_EQ( FAMILY, mpithis.family().value() );
            EXPECT_EQ( EXTENDED_MODEL, mpithis.extendedModel().value() );
            EXPECT_EQ( EXTENDED_FAMILY, mpithis.extendedFamily().value() );
            EXPECT_EQ( STEPPING_ID, mpithis.steppingId().value() );
            EXPECT_EQ( SUBSTEPPING_ID, mpithis.substeppingId().value() );
            EXPECT_EQ( STEPPING, mpithis.stepping().value() );
            EXPECT_EQ( SKU, mpithis.sku().value() );
            EXPECT_TRUE( mpithis.type().isValid() );
            EXPECT_TRUE( mpithis.model().isValid() );
            EXPECT_TRUE( mpithis.family().isValid() );
            EXPECT_TRUE( mpithis.extendedModel().isValid() );
            EXPECT_TRUE( mpithis.extendedFamily().isValid() );
            EXPECT_TRUE( mpithis.steppingId().isValid() );
            EXPECT_TRUE( mpithis.substeppingId().isValid() );
            EXPECT_TRUE( mpithis.stepping().isValid() );
            EXPECT_TRUE( mpithis.sku().isValid() );
        }

        {   // Clear tests
            MicProcessorInfo mpi( pidata );
            mpi.clear();
            EXPECT_FALSE( mpi.type().isValid() );
            EXPECT_FALSE( mpi.model().isValid() );
            EXPECT_FALSE( mpi.family().isValid() );
            EXPECT_FALSE( mpi.extendedModel().isValid() );
            EXPECT_FALSE( mpi.extendedFamily().isValid() );
            EXPECT_FALSE( mpi.steppingId().isValid() );
            EXPECT_FALSE( mpi.substeppingId().isValid() );
            EXPECT_FALSE( mpi.stepping().isValid() );
            EXPECT_FALSE( mpi.sku().isValid() );
            EXPECT_FALSE( mpi.isValid() );
        }


    } // sdk.TC_KNL_mpsstools_MicProcessorInfoUt_001

}   // namespace micmgmt
