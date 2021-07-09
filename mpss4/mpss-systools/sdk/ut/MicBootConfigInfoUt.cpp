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
#include "MicBootConfigInfo.hpp"

namespace micmgmt
{
    using namespace std;

    TEST(sdk, TC_KNL_mpsstools_MicBootConfigInfo_001)
    {
        const std::string IMAGE_PATH( "/boot/image/path" );
        const std::string INITRAM_FS_PATH( "/path/to/initramfs" );
        const std::string SYS_MAP_PATH( "/path/to/sysmap" );

        {   // Empty tests
            MicBootConfigInfo mbci;
            EXPECT_FALSE( mbci.isValid() );
            EXPECT_FALSE( mbci.isCustom() );
            EXPECT_EQ( "", mbci.imagePath() );
            EXPECT_EQ( "", mbci.initRamFsPath() );
            EXPECT_EQ( "", mbci.systemMapPath() );
        }

        {   // Value constructor tests
            MicBootConfigInfo mbci( IMAGE_PATH );
            EXPECT_TRUE( mbci.isValid() );
            EXPECT_FALSE( mbci.isCustom() );
            EXPECT_EQ( IMAGE_PATH, mbci.imagePath() );
            EXPECT_EQ( "", mbci.initRamFsPath() );
            EXPECT_EQ( "", mbci.systemMapPath() );

            MicBootConfigInfo mbcit1( IMAGE_PATH, true);
            EXPECT_TRUE( mbcit1.isValid() );
            EXPECT_TRUE( mbcit1.isCustom() );
            EXPECT_EQ( IMAGE_PATH, mbcit1.imagePath() );
            EXPECT_EQ( "", mbcit1.initRamFsPath() );
            EXPECT_EQ( "", mbcit1.systemMapPath() );

            MicBootConfigInfo mbcit2( IMAGE_PATH, INITRAM_FS_PATH);
            EXPECT_TRUE( mbcit2.isValid() );
            EXPECT_FALSE( mbcit2.isCustom() );
            EXPECT_EQ( IMAGE_PATH, mbcit2.imagePath() );
            EXPECT_EQ( INITRAM_FS_PATH, mbcit2.initRamFsPath() );
            EXPECT_EQ( "", mbcit2.systemMapPath() );

            MicBootConfigInfo mbcit3( IMAGE_PATH, INITRAM_FS_PATH, true);
            EXPECT_TRUE( mbcit3.isValid() );
            EXPECT_TRUE( mbcit3.isCustom() );
            EXPECT_EQ( IMAGE_PATH, mbcit3.imagePath() );
            EXPECT_EQ( INITRAM_FS_PATH, mbcit3.initRamFsPath() );
            EXPECT_EQ( "", mbcit3.systemMapPath() );
        }

        {   // Copy constructor tests
            MicBootConfigInfo mbcithat( IMAGE_PATH, INITRAM_FS_PATH, true);
            MicBootConfigInfo mbcithis( mbcithat );
            EXPECT_TRUE( mbcithis.isValid() );
            EXPECT_TRUE( mbcithis.isCustom() );
            EXPECT_EQ( IMAGE_PATH, mbcithis.imagePath() );
            EXPECT_EQ( INITRAM_FS_PATH, mbcithis.initRamFsPath() );
            EXPECT_EQ( "", mbcithis.systemMapPath() );
        }

        {   // Copy assignment tests
            MicBootConfigInfo mbcithat( IMAGE_PATH, INITRAM_FS_PATH, true);
            MicBootConfigInfo mbcithis;
            mbcithis = mbcithat;
            EXPECT_TRUE( mbcithis.isValid() );
            EXPECT_TRUE( mbcithis.isCustom() );
            EXPECT_EQ( IMAGE_PATH, mbcithis.imagePath() );
            EXPECT_EQ( INITRAM_FS_PATH, mbcithis.initRamFsPath() );
            EXPECT_EQ( "", mbcithis.systemMapPath() );
        }

        {   // Set path tests 
            MicBootConfigInfo mbci;
            mbci.setCustomState( true );
            mbci.setImagePath( IMAGE_PATH );
            mbci.setInitRamFsPath( INITRAM_FS_PATH);
            mbci.setSystemMapPath( SYS_MAP_PATH );
            EXPECT_TRUE( mbci.isValid() );
            EXPECT_TRUE( mbci.isCustom() );
            EXPECT_EQ( IMAGE_PATH, mbci.imagePath() );
            EXPECT_EQ( INITRAM_FS_PATH, mbci.initRamFsPath() );
            EXPECT_EQ( SYS_MAP_PATH, mbci.systemMapPath() );

            // Value clear test
            mbci.clear();
            EXPECT_FALSE( mbci.isValid() );
            EXPECT_FALSE( mbci.isCustom() );
            EXPECT_EQ( "", mbci.imagePath() );
            EXPECT_EQ( "", mbci.initRamFsPath() );
            EXPECT_EQ( "", mbci.systemMapPath() );
        }

    } // sdk.TC_KNL_mpsstools_MicBootConfigInfo_001

}   // namespace micmgmt
