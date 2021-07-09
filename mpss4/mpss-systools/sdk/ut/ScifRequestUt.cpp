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
#include "ScifRequest.hpp"

namespace micmgmt
{
    using namespace std;

    TEST(sdk, TC_KNL_mpsstools_ScifRequest_001)
    {
        const char* const  DATABUF   = "cdefgabc";
        const size_t       DATALEN   = sizeof( DATABUF );
        const int          COMMAND1  = 1;
        const int          ERRORCODE = 13;
        const uint32_t     PARAMETER = 0xdeadbeef;

        {   // Command query constructor test
            ScifRequest  request( COMMAND1 );
            EXPECT_FALSE( request.isValid() );
            EXPECT_TRUE( request.isError() );           // Not significant
            EXPECT_FALSE( request.isSendRequest() );
            EXPECT_EQ( COMMAND1, request.command() );
            EXPECT_EQ( 0u, request.parameter() );
            EXPECT_EQ( 0u, request.byteCount() );
            EXPECT_EQ( -1, request.errorCode() );
        }

        {   // Command query with parameter constructor test
            ScifRequest  request( COMMAND1, PARAMETER );
            EXPECT_FALSE( request.isValid() );
            EXPECT_TRUE( request.isError() );           // Not significant
            EXPECT_FALSE( request.isSendRequest() );
            EXPECT_EQ( COMMAND1, request.command() );
            EXPECT_EQ( PARAMETER, request.parameter() );
            EXPECT_EQ( 0u, request.byteCount() );
            EXPECT_EQ( -1, request.errorCode() );
        }

        {   // Command query with return buffer constructor test
            size_t  datalen = 10;
            char    databuf[ datalen ];

            ScifRequest  request( COMMAND1, databuf, datalen );
            EXPECT_FALSE( request.isValid() );
            EXPECT_TRUE( request.isError() );
            EXPECT_FALSE( request.isSendRequest() );
            EXPECT_EQ( COMMAND1, request.command() );
            EXPECT_EQ( 0u, request.parameter() );
            EXPECT_EQ( &databuf[0], request.buffer() );
            EXPECT_EQ( datalen, request.byteCount() );
            EXPECT_EQ( -1, request.errorCode() );
        }

        {   // Command query with parameter and return buffer constructor test
            size_t  datalen = 10;
            char    databuf[ datalen ];

            ScifRequest  request( COMMAND1, PARAMETER, databuf, datalen );
            EXPECT_FALSE( request.isValid() );
            EXPECT_TRUE( request.isError() );
            EXPECT_FALSE( request.isSendRequest() );
            EXPECT_EQ( COMMAND1, request.command() );
            EXPECT_EQ( PARAMETER, request.parameter() );
            EXPECT_STREQ( &databuf[0], request.buffer() );
            EXPECT_EQ( datalen, request.byteCount() );
            EXPECT_EQ( -1, request.errorCode() );
        }

        {   // Command send with data constructor test
            ScifRequest  request( COMMAND1, DATABUF, DATALEN );
            EXPECT_FALSE( request.isValid() );
            EXPECT_TRUE( request.isError() );
            EXPECT_TRUE( request.isSendRequest() );
            EXPECT_EQ( COMMAND1, request.command() );
            EXPECT_EQ( 0u, request.parameter() );
            EXPECT_STREQ( DATABUF, request.buffer() );
            EXPECT_EQ( DATALEN, request.byteCount() );
            EXPECT_EQ( -1, request.errorCode() );
        }

        {   // Clear error test
            ScifRequest  request( COMMAND1 );
            EXPECT_FALSE( request.isValid() );
            EXPECT_TRUE( request.isError() );
            EXPECT_EQ( -1, request.errorCode() );
            request.clearError();
            EXPECT_TRUE( request.isValid() );
            EXPECT_FALSE( request.isError() );
            EXPECT_EQ( 0, request.errorCode() );
        }

        {   // Set error code test
            ScifRequest  request( COMMAND1 );
            EXPECT_FALSE( request.isValid() );
            EXPECT_TRUE( request.isError() );
            EXPECT_EQ( -1, request.errorCode() );
            request.setError( ERRORCODE );
            EXPECT_FALSE( request.isValid() );
            EXPECT_TRUE( request.isError() );
            EXPECT_EQ( ERRORCODE, request.errorCode() );
        }

        {   // Set send request test
            ScifRequest  request( COMMAND1 );
            EXPECT_FALSE( request.isSendRequest() );
            request.setSendRequest( true );
            EXPECT_TRUE( request.isSendRequest() );
        }

    } // sdk.TC_KNL_mpsstools_ScifRequest_001

}   // namespace micmgmt
