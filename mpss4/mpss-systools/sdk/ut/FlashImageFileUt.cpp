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
#include "FlashImageFile.hpp"
#include "MicDeviceError.hpp"

namespace micmgmt
{
    using namespace std;

    TEST(sdk, TC_KNL_mpsstools_FlashImageFile_001)
    {
        const uint32_t ERR_FILE_IO = MicDeviceError::errorCode( MICSDKERR_FILE_IO_ERROR );

        const std::string PATH_NAME( "pathname" );

        {   // Empty tests
            FlashImageFile fif;
            EXPECT_EQ( "", fif.pathName() );
        }

        {   // Path constructor tests
            FlashImageFile fif( PATH_NAME );
            EXPECT_EQ( PATH_NAME, fif.pathName() );
            EXPECT_EQ( ERR_FILE_IO, fif.load() );
        }

        {   // Set path name tests
            FlashImageFile fif;
            fif.setPathName( PATH_NAME );
            EXPECT_EQ( PATH_NAME, fif.pathName() );
            EXPECT_EQ( ERR_FILE_IO, fif.load() );
        }

    } // sdk.TC_KNL_mpsstools_FlashImageFile_001

}   // namespace micmgmt
