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
#include "MicVersionInfo.hpp"
#include "VersionInfoData_p.hpp"  // Private

namespace micmgmt
{
    using namespace std;

    TEST(sdk, TC_KNL_mpsstools_MicVersionInfo_001)
    {
        const std::string FLASH_VERSION( "flashversion" );
        const std::string BIOS_VERSION( "biosversion" );
        const std::string BIOS_REL_DATE( "2020-01-31" );
        const std::string OEM_STRINGS( "oemstrings" );
        const std::string NTB_VERSION( "0203000C" );
        const std::string SMC_VERSION( "smcversion" );
        const std::string SMC_BOOT( "smcboot" );
        const std::string RAM_CTRL( "ramctrl" );
        const std::string ME_VERSION( "meversion" );
        const std::string MPSS_VERSION( "4.0.0" );
        const std::string DRIVER_VERSION( "4.0.0-driver" );

        VersionInfoData data;
        data.mFlashVersion = FLASH_VERSION;
        data.mBiosVersion = BIOS_VERSION;
        data.mBiosRelDate = BIOS_REL_DATE;
        data.mOemStrings = OEM_STRINGS;
        data.mPlxVersion = "0203000C";
        data.mSmcVersion = SMC_VERSION;
        data.mSmcBootVersion = SMC_BOOT;
        data.mRamCtrlVersion = RAM_CTRL;
        data.mMeVersion = ME_VERSION;
        data.mMpssVersion = MPSS_VERSION;
        data.mDriverVersion = DRIVER_VERSION;

        {   // Empty tests
            MicVersionInfo mvi;
            EXPECT_FALSE( mvi.isValid() );
            EXPECT_EQ( "", mvi.flashVersion().value() );
            EXPECT_EQ( "", mvi.biosReleaseDate().value() );
            EXPECT_EQ( "N/A", mvi.biosVersion().value() );
            EXPECT_EQ( "", mvi.oemStrings().value() );
            EXPECT_EQ( "N/A", mvi.ntbVersion().value() );
            EXPECT_EQ( "N/A", mvi.smcVersion().value() );
            EXPECT_EQ( "", mvi.smcBootLoaderVersion().value() );
            EXPECT_EQ( "", mvi.ramControllerVersion().value() );
            EXPECT_EQ( "N/A", mvi.meVersion().value() );
            EXPECT_EQ( "", mvi.mpssStackVersion().value() );
            EXPECT_EQ( "", mvi.driverVersion().value() );
        }

        {   // Data constructor tests
            MicVersionInfo mvi( data );
            EXPECT_TRUE( mvi.isValid() );
            EXPECT_EQ( FLASH_VERSION, mvi.flashVersion().value() );
            EXPECT_EQ( BIOS_REL_DATE, mvi.biosReleaseDate().value() );
            EXPECT_EQ( BIOS_VERSION, mvi.biosVersion().value() );
            EXPECT_EQ( OEM_STRINGS, mvi.oemStrings().value() );
            EXPECT_EQ( NTB_VERSION, mvi.ntbVersion().value() );
            EXPECT_EQ( SMC_VERSION, mvi.smcVersion().value() );
            EXPECT_EQ( SMC_BOOT, mvi.smcBootLoaderVersion().value() );
            EXPECT_EQ( RAM_CTRL, mvi.ramControllerVersion().value() );
            EXPECT_EQ( ME_VERSION, mvi.meVersion().value() );
            EXPECT_EQ( MPSS_VERSION, mvi.mpssStackVersion().value() );
            EXPECT_EQ( DRIVER_VERSION, mvi.driverVersion().value() );
        }

        {   // Copy constructor tests
            MicVersionInfo mvithat( data );
            MicVersionInfo mvithis( mvithat );
            EXPECT_TRUE( mvithis.isValid() );
            EXPECT_EQ( FLASH_VERSION, mvithis.flashVersion().value() );
            EXPECT_EQ( BIOS_REL_DATE, mvithis.biosReleaseDate().value() );
            EXPECT_EQ( BIOS_VERSION, mvithis.biosVersion().value() );
            EXPECT_EQ( OEM_STRINGS, mvithis.oemStrings().value() );
            EXPECT_EQ( NTB_VERSION, mvithis.ntbVersion().value() );
            EXPECT_EQ( SMC_VERSION, mvithis.smcVersion().value() );
            EXPECT_EQ( SMC_BOOT, mvithis.smcBootLoaderVersion().value() );
            EXPECT_EQ( RAM_CTRL, mvithis.ramControllerVersion().value() );
            EXPECT_EQ( ME_VERSION, mvithis.meVersion().value() );
            EXPECT_EQ( MPSS_VERSION, mvithis.mpssStackVersion().value() );
            EXPECT_EQ( DRIVER_VERSION, mvithis.driverVersion().value() );
        }

        {   // Copy assignment tests
            MicVersionInfo mvithat( data );
            MicVersionInfo mvithis = mvithat;
            EXPECT_TRUE( mvithis.isValid() );
            EXPECT_EQ( FLASH_VERSION, mvithis.flashVersion().value() );
            EXPECT_EQ( BIOS_REL_DATE, mvithis.biosReleaseDate().value() );
            EXPECT_EQ( BIOS_VERSION, mvithis.biosVersion().value() );
            EXPECT_EQ( OEM_STRINGS, mvithis.oemStrings().value() );
            EXPECT_EQ( NTB_VERSION, mvithis.ntbVersion().value() );
            EXPECT_EQ( SMC_VERSION, mvithis.smcVersion().value() );
            EXPECT_EQ( SMC_BOOT, mvithis.smcBootLoaderVersion().value() );
            EXPECT_EQ( RAM_CTRL, mvithis.ramControllerVersion().value() );
            EXPECT_EQ( ME_VERSION, mvithis.meVersion().value() );
            EXPECT_EQ( MPSS_VERSION, mvithis.mpssStackVersion().value() );
            EXPECT_EQ( DRIVER_VERSION, mvithis.driverVersion().value() );

            mvithis = mvithis; //code coverage..
            EXPECT_TRUE( mvithis.isValid() );
            EXPECT_EQ( FLASH_VERSION, mvithis.flashVersion().value() );
            EXPECT_EQ( BIOS_REL_DATE, mvithis.biosReleaseDate().value() );
            EXPECT_EQ( BIOS_VERSION, mvithis.biosVersion().value() );
            EXPECT_EQ( OEM_STRINGS, mvithis.oemStrings().value() );
            EXPECT_EQ( NTB_VERSION, mvithis.ntbVersion().value() );
            EXPECT_EQ( SMC_VERSION, mvithis.smcVersion().value() );
            EXPECT_EQ( SMC_BOOT, mvithis.smcBootLoaderVersion().value() );
            EXPECT_EQ( RAM_CTRL, mvithis.ramControllerVersion().value() );
            EXPECT_EQ( ME_VERSION, mvithis.meVersion().value() );
            EXPECT_EQ( MPSS_VERSION, mvithis.mpssStackVersion().value() );
            EXPECT_EQ( DRIVER_VERSION, mvithis.driverVersion().value() );
        }

        {   // Clear tests
            MicVersionInfo mvi( data );
            mvi.clear();
            EXPECT_FALSE( mvi.isValid() );
        }

    } // sdk.TC_KNL_mpsstools_MicVersionInfo_001

}   // namespace micmgmt
