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
#include "FlashImage.hpp"
#include "MicDevice.hpp"
#include "KnlMockDevice.hpp"
#include "MicDeviceError.hpp"

namespace micmgmt
{
    using namespace std;

    TEST(sdk, TC_KNL_mpsstools_FlashImage_001)
    {
        const uint32_t ERR_FILE_IO = MicDeviceError::errorCode( MICSDKERR_FILE_IO_ERROR );

        const std::string PATH( "path" );

        MicDevice *KNL_DEVICE = new MicDevice( new KnlMockDevice( 0 ) );

        {   // Empty tests
            FlashImage fi;
            EXPECT_FALSE( fi.isValid() );
            EXPECT_FALSE( fi.isSigned() );
            EXPECT_EQ( 0U, fi.itemCount() );
            EXPECT_TRUE( fi.itemVersions().empty() );
            EXPECT_EQ( FlashImage::eUnknownPlatform, fi.targetPlatform() );
            EXPECT_FALSE( fi.isCompatible( KNL_DEVICE ) );
            EXPECT_FALSE( fi.isCompatible( NULL ) );
            EXPECT_EQ( "", fi.filePath() );
            EXPECT_EQ( 0U, fi.size() );
            EXPECT_EQ( NULL, fi.data() );

            fi.clear(); //code coverage
        }

        {   // Set file path negative () tests
            FlashImage fi;
            EXPECT_EQ( ERR_FILE_IO, fi.setFilePath( PATH ) );
            EXPECT_EQ( "", fi.filePath() );
        }

        delete KNL_DEVICE;

    } // sdk.TC_KNL_mpsstools_FlashImage_001

}   // namespace micmgmt
