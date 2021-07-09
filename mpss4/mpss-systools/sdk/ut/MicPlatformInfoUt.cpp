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
#include "MicPlatformInfo.hpp"
#include "PlatformInfoData_p.hpp"  // Private

namespace micmgmt
{
    using namespace std;

    TEST(sdk, TC_KNL_mpsstools_MicPlatformInfo_001)
    {

        const std::string PART_NUMBER( "partnumber" );
        const std::string MANUFACTURE_DATE( "2014-12-20" );
        const std::string MANUFACTURE_TIME( "12:45:17" );
        const std::string SERIAL_NO( "serialno" );
        const std::string UUID( "01020304-0506-0708-090A-0B0C0D0E0F10" );
        const std::string FEATURE_SET( "featureset" );
        const std::string COPROCESSOR_OS_VERSION( "osversion" );
        const std::string COPROCESSOR_BRAND( "coprocessorbrand" );
        const std::string BOARD_SKU( "boardsku" );
        const std::string BOARD_TYPE( "boardtype" );
        const std::string STRAP_INFO( "strapinfo" );
        const uint32_t STRAP_INFO_RAW = 1;
        const uint32_t SM_BUS_ADDRESS = 2;
        const std::string MAX_POWER_NAME( "maxpower" );

        PlatformInfoData data;
        data.mPartNumber = PART_NUMBER;
        data.mSerialNo = SERIAL_NO;
        data.mUuid = UUID;
        data.mFeatureSet = FEATURE_SET;
        data.mCoprocessorOs = COPROCESSOR_OS_VERSION;
        data.mCoprocessorBrand = COPROCESSOR_BRAND;
        data.mBoardSku = BOARD_SKU;
        data.mBoardType = BOARD_TYPE;
        data.mStrapInfo = STRAP_INFO;
        data.mStrapInfoRaw = STRAP_INFO_RAW;
        data.mSmBusAddress = SM_BUS_ADDRESS;
        data.mMaxPower = MicPower( MAX_POWER_NAME, 850 );
        data.mCoolActive = true;
        data.mManufactDate = MANUFACTURE_DATE;
        data.mManufactTime = MANUFACTURE_TIME;

        {   // Empty tests
            MicPlatformInfo mpi;
            EXPECT_FALSE( mpi.isValid() );
            EXPECT_FALSE( mpi.isCoolingActive().value() );
            EXPECT_EQ( "", mpi.partNumber().value() );
            EXPECT_EQ( "", mpi.manufactureDate().value() );
            EXPECT_EQ( "", mpi.manufactureTime().value() );
            EXPECT_EQ( "", mpi.serialNo().value() );
            EXPECT_EQ( "", mpi.uuid().value() );
            EXPECT_EQ( "", mpi.featureSet().value() );
            EXPECT_EQ( "", mpi.coprocessorOs().value() );
            EXPECT_EQ( "", mpi.coprocessorOsVersion().value() );
            EXPECT_EQ( "", mpi.coprocessorBrand().value() );
            EXPECT_EQ( "", mpi.boardSku().value() );
            EXPECT_EQ( "", mpi.boardType().value() );
            EXPECT_EQ( "", mpi.strapInfo().value() );
            EXPECT_EQ( 0U, mpi.strapInfoRaw().value() );
            EXPECT_EQ( 0U, mpi.smBusBaseAddress().value() );
            EXPECT_FALSE( mpi.maxPower().isValid() );
        }

        {   // Data constructor tests
            MicPlatformInfo mpi( data );
            EXPECT_TRUE( mpi.isValid() );
            EXPECT_TRUE( mpi.isCoolingActive().value() );
            EXPECT_EQ( PART_NUMBER, mpi.partNumber().value() );
            EXPECT_EQ( MANUFACTURE_DATE, mpi.manufactureDate().value() );
            EXPECT_EQ( MANUFACTURE_TIME, mpi.manufactureTime().value() );
            EXPECT_EQ( SERIAL_NO, mpi.serialNo().value() );
            EXPECT_EQ( UUID, mpi.uuid().value() );
            EXPECT_EQ( FEATURE_SET, mpi.featureSet().value() );
            EXPECT_EQ( COPROCESSOR_OS_VERSION, mpi.coprocessorOs().value() );
            EXPECT_EQ( COPROCESSOR_OS_VERSION, mpi.coprocessorOsVersion().value() );
            EXPECT_EQ( COPROCESSOR_BRAND, mpi.coprocessorBrand().value() );
            EXPECT_EQ( BOARD_SKU, mpi.boardSku().value() );
            EXPECT_EQ( BOARD_TYPE, mpi.boardType().value() );
            EXPECT_EQ( STRAP_INFO, mpi.strapInfo().value() );
            EXPECT_EQ( STRAP_INFO_RAW, mpi.strapInfoRaw().value() );
            EXPECT_EQ( SM_BUS_ADDRESS, mpi.smBusBaseAddress().value() );
            EXPECT_EQ( MAX_POWER_NAME, mpi.maxPower().name() );
        }

        {   // Copy constructor tests
            MicPlatformInfo mpithat( data );
            MicPlatformInfo mpithis( mpithat );
            EXPECT_TRUE( mpithis.isValid() );
            EXPECT_TRUE( mpithis.isCoolingActive().value() );
            EXPECT_EQ( PART_NUMBER, mpithis.partNumber().value() );
            EXPECT_EQ( MANUFACTURE_DATE, mpithis.manufactureDate().value() );
            EXPECT_EQ( MANUFACTURE_TIME, mpithis.manufactureTime().value() );
            EXPECT_EQ( SERIAL_NO, mpithis.serialNo().value() );
            EXPECT_EQ( UUID, mpithis.uuid().value() );
            EXPECT_EQ( FEATURE_SET, mpithis.featureSet().value() );
            EXPECT_EQ( COPROCESSOR_OS_VERSION, mpithis.coprocessorOs().value() );
            EXPECT_EQ( COPROCESSOR_OS_VERSION, mpithis.coprocessorOsVersion().value() );
            EXPECT_EQ( COPROCESSOR_BRAND, mpithis.coprocessorBrand().value() );
            EXPECT_EQ( BOARD_SKU, mpithis.boardSku().value() );
            EXPECT_EQ( BOARD_TYPE, mpithis.boardType().value() );
            EXPECT_EQ( STRAP_INFO, mpithis.strapInfo().value() );
            EXPECT_EQ( STRAP_INFO_RAW, mpithis.strapInfoRaw().value() );
            EXPECT_EQ( SM_BUS_ADDRESS, mpithis.smBusBaseAddress().value() );
            EXPECT_EQ( MAX_POWER_NAME, mpithis.maxPower().name() );
        }

        {   // Copy assignment tests
            MicPlatformInfo mpithat( data );
            MicPlatformInfo mpithis;
            mpithis = mpithat;
            EXPECT_TRUE( mpithis.isValid() );
            EXPECT_TRUE( mpithis.isCoolingActive().value() );
            EXPECT_EQ( PART_NUMBER, mpithis.partNumber().value() );
            EXPECT_EQ( MANUFACTURE_DATE, mpithis.manufactureDate().value() );
            EXPECT_EQ( MANUFACTURE_TIME, mpithis.manufactureTime().value() );
            EXPECT_EQ( SERIAL_NO, mpithis.serialNo().value() );
            EXPECT_EQ( UUID, mpithis.uuid().value() );
            EXPECT_EQ( FEATURE_SET, mpithis.featureSet().value() );
            EXPECT_EQ( COPROCESSOR_OS_VERSION, mpithis.coprocessorOs().value() );
            EXPECT_EQ( COPROCESSOR_OS_VERSION, mpithis.coprocessorOsVersion().value() );
            EXPECT_EQ( COPROCESSOR_BRAND, mpithis.coprocessorBrand().value() );
            EXPECT_EQ( BOARD_SKU, mpithis.boardSku().value() );
            EXPECT_EQ( BOARD_TYPE, mpithis.boardType().value() );
            EXPECT_EQ( STRAP_INFO, mpithis.strapInfo().value() );
            EXPECT_EQ( STRAP_INFO_RAW, mpithis.strapInfoRaw().value() );
            EXPECT_EQ( SM_BUS_ADDRESS, mpithis.smBusBaseAddress().value() );
            EXPECT_EQ( MAX_POWER_NAME, mpithis.maxPower().name() );

            mpithis = mpithis;
            EXPECT_TRUE( mpithis.isValid() );
            EXPECT_TRUE( mpithis.isCoolingActive().value() );
            EXPECT_EQ( PART_NUMBER, mpithis.partNumber().value() );
            EXPECT_EQ( MANUFACTURE_DATE, mpithis.manufactureDate().value() );
            EXPECT_EQ( MANUFACTURE_TIME, mpithis.manufactureTime().value() );
            EXPECT_EQ( SERIAL_NO, mpithis.serialNo().value() );
            EXPECT_EQ( UUID, mpithis.uuid().value() );
            EXPECT_EQ( FEATURE_SET, mpithis.featureSet().value() );
            EXPECT_EQ( COPROCESSOR_OS_VERSION, mpithis.coprocessorOs().value() );
            EXPECT_EQ( COPROCESSOR_OS_VERSION, mpithis.coprocessorOsVersion().value() );
            EXPECT_EQ( COPROCESSOR_BRAND, mpithis.coprocessorBrand().value() );
            EXPECT_EQ( BOARD_SKU, mpithis.boardSku().value() );
            EXPECT_EQ( BOARD_TYPE, mpithis.boardType().value() );
            EXPECT_EQ( STRAP_INFO, mpithis.strapInfo().value() );
            EXPECT_EQ( STRAP_INFO_RAW, mpithis.strapInfoRaw().value() );
            EXPECT_EQ( SM_BUS_ADDRESS, mpithis.smBusBaseAddress().value() );
            EXPECT_EQ( MAX_POWER_NAME, mpithis.maxPower().name() );
        }

        {   // Date/time conversion coverage tests
            PlatformInfoData emptydata;

            MicPlatformInfo mpi( emptydata );
            EXPECT_EQ( "", mpi.manufactureDate().value() );
            EXPECT_EQ( "", mpi.manufactureTime().value() );
        }

        {   // Clear tests
            MicPlatformInfo mpi( data );
            mpi.clear();

            EXPECT_FALSE( mpi.isValid() );

            EXPECT_FALSE( mpi.isCoolingActive().value() );
            EXPECT_EQ( "", mpi.partNumber().value() );
            EXPECT_EQ( "", mpi.manufactureDate().value() );
            EXPECT_EQ( "", mpi.manufactureTime().value() );
            EXPECT_EQ( "", mpi.serialNo().value() );
            EXPECT_EQ( "", mpi.uuid().value() );
            EXPECT_EQ( "", mpi.featureSet().value() );
            EXPECT_EQ( "", mpi.coprocessorOs().value() );
            EXPECT_EQ( "", mpi.coprocessorOsVersion().value() );
            EXPECT_EQ( "", mpi.coprocessorBrand().value() );
            EXPECT_EQ( "", mpi.boardSku().value() );
            EXPECT_EQ( "", mpi.boardType().value() );
            EXPECT_EQ( "", mpi.strapInfo().value() );
            EXPECT_EQ( 0U, mpi.strapInfoRaw().value() );
            EXPECT_EQ( 0U, mpi.smBusBaseAddress().value() );

            EXPECT_FALSE( mpi.isCoolingActive().isValid() );
            EXPECT_FALSE( mpi.partNumber().isValid() );
            EXPECT_FALSE( mpi.manufactureDate().isValid() );
            EXPECT_FALSE( mpi.manufactureTime().isValid() );
            EXPECT_FALSE( mpi.serialNo().isValid() );
            EXPECT_FALSE( mpi.uuid().isValid() );
            EXPECT_FALSE( mpi.featureSet().isValid() );
            EXPECT_FALSE( mpi.coprocessorOs().isValid() );
            EXPECT_FALSE( mpi.coprocessorOsVersion().isValid() );
            EXPECT_FALSE( mpi.coprocessorBrand().isValid() );
            EXPECT_FALSE( mpi.boardSku().isValid() );
            EXPECT_FALSE( mpi.boardType().isValid() );
            EXPECT_FALSE( mpi.strapInfo().isValid() );
            EXPECT_FALSE( mpi.strapInfoRaw().isValid() );
            EXPECT_FALSE( mpi.smBusBaseAddress().isValid() );

            EXPECT_FALSE( mpi.maxPower().isValid() );
        }

    } // sdk.TC_KNL_mpsstools_MicPlatformInfo_001

}   // namespace micmgmt
