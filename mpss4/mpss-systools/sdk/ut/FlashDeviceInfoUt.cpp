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
#include "FlashDeviceInfo.hpp"

namespace micmgmt
{
    using namespace std;

    TEST(sdk, TC_KNL_mpsstools_FlashDeviceInfo_001)
    {
        const int OFFSET_FAMILY = 8;
        const int OFFSET_MODEL = 16;

        typedef pair<int, std::string> FdiPair;

        const FdiPair BRAND_ATMEL    = { 0x1f, "Atmel" };
        const FdiPair BRAND_MACRONIX = { 0xc2, "Macronix" };
        const FdiPair BRAND_WINBOND  = { 0xef, "Winbond" };
        const FdiPair FAMILY_AT26DF  = { 0x45, "AT26DF" };
        const FdiPair FAMILY_AT25DF  = { 0x46, "AT25DF" };
        const FdiPair FAMILY_MX25    = { 0x20, "MX25" };
        const FdiPair FAMILY_W25X    = { 0x30, "W25X" };
        const FdiPair FAMILY_W25Q    = { 0x40, "W25Q" };
        const FdiPair MODEL_081      = { 0x01, "081" };
        const FdiPair MODEL_161      = { 0x02, "161" };
        const FdiPair MODEL_L8005D   = { 0x14, "L8005D" };
        const FdiPair MODEL_L1605D   = { 0x15, "L1605D" };
        const FdiPair MODEL_L3205D   = { 0x16, "L3205D" };
        const FdiPair MODEL_L6405D   = { 0x17, "L6405D" };
        const FdiPair MODEL_80       = { 0x14, "80" };
        const FdiPair MODEL_16       = { 0x15, "16" };
        const FdiPair MODEL_32       = { 0x16, "32" };
        const FdiPair MODEL_64       = { 0x17, "64" };

        const FdiPair SAMPLE_VALUE = { ( BRAND_ATMEL.first |
                                        (FAMILY_AT26DF.first << OFFSET_FAMILY) |
                                        (MODEL_161.first << OFFSET_MODEL) ),
                                       ( BRAND_ATMEL.second + " " +
                                         FAMILY_AT26DF.second +
                                         MODEL_161.second ) };
        const int INVALID_ID = -1;
        const std::string INVALID_MEMBER( "?" );
        const std::string INVALID_MODEL_ATMEL( "xxx" );
        const std::string INVALID_MODEL_MACRONIX( "xxxxxx" );
        const std::string INVALID_MODEL_WINBOND( "xx" );
        const std::string INVALID_INFO( "<unknown> <unknown>" );

        {   // Empty tests
            FlashDeviceInfo  fdi;
            EXPECT_FALSE( fdi.isValid() );
            EXPECT_EQ( INVALID_ID, fdi.manufacturerId() );
            EXPECT_EQ( INVALID_ID, fdi.familyId() );
            EXPECT_EQ( INVALID_ID, fdi.modelId() );
            EXPECT_EQ( INVALID_MEMBER, fdi.manufacturer() );
            EXPECT_EQ( INVALID_MEMBER, fdi.family() );
            EXPECT_EQ( INVALID_MEMBER, fdi.model() );
            EXPECT_EQ( INVALID_INFO, fdi.info() );
        }

        {   // Integer constructor tests
            FlashDeviceInfo fdi( SAMPLE_VALUE.first );
            EXPECT_TRUE( fdi.isValid() );
            EXPECT_EQ( BRAND_ATMEL.first, fdi.manufacturerId() );
            EXPECT_EQ( FAMILY_AT26DF.first, fdi.familyId() );
            EXPECT_EQ( MODEL_161.first, fdi.modelId() );
            EXPECT_EQ( BRAND_ATMEL.second, fdi.manufacturer() );
            EXPECT_EQ( FAMILY_AT26DF.second, fdi.family() );
            EXPECT_EQ( MODEL_161.second, fdi.model() );
            EXPECT_EQ( SAMPLE_VALUE.second, fdi.info() );
        }

        {   // Copy constructor tests
            FlashDeviceInfo fdithat( SAMPLE_VALUE.first );
            FlashDeviceInfo fdithis( fdithat );
            EXPECT_TRUE( fdithis.isValid() );
            EXPECT_EQ( BRAND_ATMEL.first, fdithis.manufacturerId() );
            EXPECT_EQ( FAMILY_AT26DF.first, fdithis.familyId() );
            EXPECT_EQ( MODEL_161.first, fdithis.modelId() );
            EXPECT_EQ( BRAND_ATMEL.second, fdithis.manufacturer() );
            EXPECT_EQ( FAMILY_AT26DF.second, fdithis.family() );
            EXPECT_EQ( MODEL_161.second, fdithis.model() );
            EXPECT_EQ( SAMPLE_VALUE.second, fdithis.info() );
        }

        {   // Copy assignment tests
            FlashDeviceInfo fdithat( SAMPLE_VALUE.first );
            FlashDeviceInfo fdithis;
            fdithis = fdithat;
            EXPECT_TRUE( fdithis.isValid() );
            EXPECT_EQ( BRAND_ATMEL.first, fdithis.manufacturerId() );
            EXPECT_EQ( FAMILY_AT26DF.first, fdithis.familyId() );
            EXPECT_EQ( MODEL_161.first, fdithis.modelId() );
            EXPECT_EQ( BRAND_ATMEL.second, fdithis.manufacturer() );
            EXPECT_EQ( FAMILY_AT26DF.second, fdithis.family() );
            EXPECT_EQ( MODEL_161.second, fdithis.model() );
            EXPECT_EQ( SAMPLE_VALUE.second, fdithis.info() );

            fdithis = fdithis;
            EXPECT_TRUE( fdithis.isValid() );
            EXPECT_EQ( BRAND_ATMEL.first, fdithis.manufacturerId() );
            EXPECT_EQ( FAMILY_AT26DF.first, fdithis.familyId() );
            EXPECT_EQ( MODEL_161.first, fdithis.modelId() );
            EXPECT_EQ( BRAND_ATMEL.second, fdithis.manufacturer() );
            EXPECT_EQ( FAMILY_AT26DF.second, fdithis.family() );
            EXPECT_EQ( MODEL_161.second, fdithis.model() );
            EXPECT_EQ( SAMPLE_VALUE.second, fdithis.info() );
        }

        {   // isValid coverage tests
            FlashDeviceInfo fdimanufact( SAMPLE_VALUE.first & 0xffffff00 );
            EXPECT_FALSE( fdimanufact.isValid() );

            FlashDeviceInfo fdifamily( SAMPLE_VALUE.first & 0xffff00ff );
            EXPECT_FALSE( fdifamily.isValid() );

            FlashDeviceInfo fdimodel( SAMPLE_VALUE.first & 0xff00ffff );
            EXPECT_FALSE( fdimodel.isValid() );
        }

        {   // manufacturer coverage tests
            FlashDeviceInfo fdiatmel( BRAND_ATMEL.first );
            EXPECT_EQ( BRAND_ATMEL.first, fdiatmel.manufacturerId() );
            EXPECT_EQ( BRAND_ATMEL.second, fdiatmel.manufacturer() );

            FlashDeviceInfo fdimacronix( BRAND_MACRONIX.first );
            EXPECT_EQ( BRAND_MACRONIX.first, fdimacronix.manufacturerId() );
            EXPECT_EQ( BRAND_MACRONIX.second, fdimacronix.manufacturer() );

            FlashDeviceInfo fdiwinbond( BRAND_WINBOND.first );
            EXPECT_EQ( BRAND_WINBOND.first, fdiwinbond.manufacturerId() );
            EXPECT_EQ( BRAND_WINBOND.second, fdiwinbond.manufacturer() );
        }

        {   // family coverage tests
            FlashDeviceInfo fdiat26df( BRAND_ATMEL.first | (FAMILY_AT26DF.first << OFFSET_FAMILY) );
            EXPECT_EQ( FAMILY_AT26DF.first, fdiat26df.familyId() );
            EXPECT_EQ( FAMILY_AT26DF.second, fdiat26df.family() );

            FlashDeviceInfo fdiat25df( BRAND_ATMEL.first | (FAMILY_AT25DF.first << OFFSET_FAMILY) );
            EXPECT_EQ( FAMILY_AT25DF.first, fdiat25df.familyId() );
            EXPECT_EQ( FAMILY_AT25DF.second, fdiat25df.family() );

            FlashDeviceInfo fdiatnone( BRAND_ATMEL.first );
            EXPECT_EQ( INVALID_MEMBER, fdiatnone.family() );

            FlashDeviceInfo fdimx25( BRAND_MACRONIX.first | (FAMILY_MX25.first << OFFSET_FAMILY) );
            EXPECT_EQ( FAMILY_MX25.first, fdimx25.familyId() );
            EXPECT_EQ( FAMILY_MX25.second, fdimx25.family() );

            FlashDeviceInfo fdimxnone( BRAND_MACRONIX.first );
            EXPECT_EQ( INVALID_MEMBER, fdimxnone.family() );

            FlashDeviceInfo fdiw25x( BRAND_WINBOND.first | (FAMILY_W25X.first << OFFSET_FAMILY) );
            EXPECT_EQ( FAMILY_W25X.first, fdiw25x.familyId() );
            EXPECT_EQ( FAMILY_W25X.second, fdiw25x.family() );

            FlashDeviceInfo fdiw25q( BRAND_WINBOND.first | (FAMILY_W25Q.first << OFFSET_FAMILY) );
            EXPECT_EQ( FAMILY_W25Q.first, fdiw25q.familyId() );
            EXPECT_EQ( FAMILY_W25Q.second, fdiw25q.family() );

            FlashDeviceInfo fdiwnone( BRAND_WINBOND.first );
            EXPECT_EQ( INVALID_MEMBER, fdiwnone.family() );
        }

        {   // model coverage tests
            FlashDeviceInfo fdi081( BRAND_ATMEL.first | (MODEL_081.first << OFFSET_MODEL) );
            EXPECT_EQ( MODEL_081.first, fdi081.modelId() );
            EXPECT_EQ( MODEL_081.second, fdi081.model() );

            FlashDeviceInfo fdi161( BRAND_ATMEL.first | (MODEL_161.first << OFFSET_MODEL) );
            EXPECT_EQ( MODEL_161.first, fdi161.modelId() );
            EXPECT_EQ( MODEL_161.second, fdi161.model() );

            FlashDeviceInfo fdixxx( BRAND_ATMEL.first );
            EXPECT_EQ( INVALID_MODEL_ATMEL, fdixxx.model() );

            FlashDeviceInfo fdiL8( BRAND_MACRONIX.first | (MODEL_L8005D.first << OFFSET_MODEL) );
            EXPECT_EQ( MODEL_L8005D.first, fdiL8.modelId() );
            EXPECT_EQ( MODEL_L8005D.second, fdiL8.model() );

            FlashDeviceInfo fdiL1( BRAND_MACRONIX.first | (MODEL_L1605D.first << OFFSET_MODEL) );
            EXPECT_EQ( MODEL_L1605D.first, fdiL1.modelId() );
            EXPECT_EQ( MODEL_L1605D.second, fdiL1.model() );

            FlashDeviceInfo fdiL3( BRAND_MACRONIX.first | (MODEL_L3205D.first << OFFSET_MODEL) );
            EXPECT_EQ( MODEL_L3205D.first, fdiL3.modelId() );
            EXPECT_EQ( MODEL_L3205D.second, fdiL3.model() );

            FlashDeviceInfo fdiL6( BRAND_MACRONIX.first | (MODEL_L6405D.first << OFFSET_MODEL) );
            EXPECT_EQ( MODEL_L6405D.first, fdiL6.modelId() );
            EXPECT_EQ( MODEL_L6405D.second, fdiL6.model() );

            FlashDeviceInfo fdixxxxxx( BRAND_MACRONIX.first );
            EXPECT_EQ( INVALID_MODEL_MACRONIX, fdixxxxxx.model() );

            FlashDeviceInfo fdi80( BRAND_WINBOND.first | (MODEL_80.first << OFFSET_MODEL) );
            EXPECT_EQ( MODEL_80.first, fdi80.modelId() );
            EXPECT_EQ( MODEL_80.second, fdi80.model() );

            FlashDeviceInfo fdi16( BRAND_WINBOND.first | (MODEL_16.first << OFFSET_MODEL) );
            EXPECT_EQ( MODEL_16.first, fdi16.modelId() );
            EXPECT_EQ( MODEL_16.second, fdi16.model() );

            FlashDeviceInfo fdi32( BRAND_WINBOND.first | (MODEL_32.first << OFFSET_MODEL) );
            EXPECT_EQ( MODEL_32.first, fdi32.modelId() );
            EXPECT_EQ( MODEL_32.second, fdi32.model() );

            FlashDeviceInfo fdi64( BRAND_WINBOND.first | (MODEL_64.first << OFFSET_MODEL) );
            EXPECT_EQ( MODEL_64.first, fdi64.modelId() );
            EXPECT_EQ( MODEL_64.second, fdi64.model() );

            FlashDeviceInfo fdixx( BRAND_WINBOND.first );
            EXPECT_EQ( INVALID_MODEL_WINBOND, fdixx.model() );
        }

        {   // Clear tests
            FlashDeviceInfo fdi( SAMPLE_VALUE.first );
            fdi.clear();
            EXPECT_FALSE( fdi.isValid() );
            EXPECT_EQ( INVALID_ID, fdi.manufacturerId() );
            EXPECT_EQ( INVALID_ID, fdi.familyId() );
            EXPECT_EQ( INVALID_ID, fdi.modelId() );
            EXPECT_EQ( INVALID_MEMBER, fdi.manufacturer() );
            EXPECT_EQ( INVALID_MEMBER, fdi.family() );
            EXPECT_EQ( INVALID_MEMBER, fdi.model() );
            EXPECT_EQ( INVALID_INFO, fdi.info() );
        }

    } // sdk.TC_KNL_mpsstools_FlashDeviceInfo_001

}   // namespace micmgmt
