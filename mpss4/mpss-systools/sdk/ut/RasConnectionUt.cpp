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

// GTEST AND GMOCK INCLUDES
//
#include <gmock/gmock.h>
#include <gtest/gtest.h>

// PROJECT INCLUDES
//
#include "MicDeviceError.hpp"
#include "RasConnection.hpp"

// SCIF PROTOCOL INCLUDES
//
#include "mic/micras_api.h"

// UNIT TEST INCLUDES
//
#include "mocks.hpp"


using ::testing::Expectation;
using ::testing::Gt;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::ReturnArg;
using ::testing::SetArrayArgument;

namespace
{
    const int DEVNUM = 2;
    const std::string SCIF_ERROR_STR("this is a scif error");
    const uint32_t SCIF_FD = 7;
    const int COMMAND = 0x1F;
    const uint32_t PARAMETER = 0xabad1dea;
    const size_t BYTE_COUNT = 64;
    char BUFFER[BYTE_COUNT];

} // namespace

namespace micmgmt
{
    //////////////////////////////////////////////////////////////////////////////////////
    //                          Base testing fixture class
    //////////////////////////////////////////////////////////////////////////////////////
    class RasConnectionTestBase : public ::testing::Test
    {
    public:
        // Ctor
        RasConnectionTestBase() : ras( DEVNUM ) { }

    protected:
        // ras header member
        struct mr_hdr header;
        // ras connection member
        MockRasConnection ras;
        // SCIF request member
        MockScifRequest request;
        // Expectation members to model protocol flow
        Expectation sendHdr, recvResp, recvData;

        virtual void SetUp()
        {
            bzero( &header, sizeof(header) );
            setConnectionDefaults( ras );
            setRequestDefaults( request );
        }

        // Helper function to set default actions on MockScifConnection objects
        void setConnectionDefaults( MockScifConnection& connection )
        {
            ON_CALL(connection, isOpen())
                .WillByDefault(Return(true));
            ON_CALL(connection, deviceNum())
                .WillByDefault(Return(DEVNUM));
            ON_CALL(connection, errorText())
                .WillByDefault(Return(SCIF_ERROR_STR));
            ON_CALL(connection, open())
                .WillByDefault(Return(SCIF_FD));
            ON_CALL(connection, lock())
                .WillByDefault(Return(true));
            ON_CALL(connection, unlock())
                .WillByDefault(Return(true));
            ON_CALL(connection, send(NotNull(), Gt(0)))
                .WillByDefault(ReturnArg<1>());
            ON_CALL(connection, receive(NotNull(), Gt(0)))
                .WillByDefault(ReturnArg<1>());
        }

        // Helper function to set default actions on MockScifRequest objects
        void setRequestDefaults( MockScifRequest &req )
        {

            ON_CALL(req, isValid())
                .WillByDefault(Return(true));
            ON_CALL(req, isError())
                .WillByDefault(Return(false));
            ON_CALL(req, isSendRequest())
                .WillByDefault(Return(false));
            ON_CALL(req, command())
                .WillByDefault(Return(COMMAND));
            ON_CALL(req, parameter())
                .WillByDefault(Return(PARAMETER));
            ON_CALL(req, buffer())
                .WillByDefault(Return(BUFFER));
            ON_CALL(req, byteCount())
                .WillByDefault(Return(BYTE_COUNT));
            ON_CALL(req, errorCode())
                .WillByDefault(Return(0));
        }

        // Helper function to set an expectation on unlock() that must
        // happen after the specified argument
        void expectUnlock( Expectation after )
        {
            EXPECT_CALL( ras, unlock() )
                .Times(1)
                .After(after);
        }

    }; // RasConnectionTestBase

    class RasConnectionTestHeaderSent : public RasConnectionTestBase
    {
    protected:
        virtual void SetUp()
        {
            RasConnectionTestBase::SetUp();
            header.len = BYTE_COUNT; // So that request.byteCount() and response header.len match
            sendHdr = EXPECT_CALL( ras, send( NotNull(), Gt(0) ) )
                .WillOnce( ReturnArg<1>() );
            recvResp = EXPECT_CALL( ras, receive( NotNull(), Gt(0) ) )
                .WillOnce(
                    DoAll(
                        SetArrayArgument<0>( (char*)&header,
                                               (char*)&header + sizeof(header) ),
                        ReturnArg<1>()
                    ));
        }
    };

    typedef RasConnectionTestBase RasConnectionTestPrelude;

    //////////////////////////////////////////////////////////////////////////////////////
    //                              TESTS BEGIN HERE
    //////////////////////////////////////////////////////////////////////////////////////
    TEST_F(RasConnectionTestPrelude, TC_ctor_001)
    {
        ASSERT_EQ( DEVNUM, ras.deviceNum() );
    }

    TEST_F(RasConnectionTestPrelude, TC_request_nullptr_fail_001)
    {
        ASSERT_EQ( MicDeviceError::errorCode(MICSDKERR_INVALID_ARG), ras.request(NULL) );
    }

    TEST_F(RasConnectionTestPrelude, TC_request_lock_fail_001)
    {
        EXPECT_CALL( ras, lock() )
            .WillOnce( Return(false) );
        ASSERT_EQ( MicDeviceError::errorCode(MICSDKERR_INTERNAL_ERROR), ras.request(&request) );
    }

    TEST_F(RasConnectionTestPrelude, TC_request_sendhdr_fail_001)
    {
        sendHdr = EXPECT_CALL( ras, send( NotNull(), Gt(0) ) )
            .WillOnce( Return(-1) );
        // Expect unlock to be called
        expectUnlock(sendHdr);
        ASSERT_EQ( MicDeviceError::errorCode(MICSDKERR_INTERNAL_ERROR), ras.request(&request) );
    }

    TEST_F(RasConnectionTestPrelude, TC_request_recvresp_fail_001)
    {
        recvResp = EXPECT_CALL(ras, receive( NotNull(), Gt(0) ) )
            .WillOnce( Return(-1) );
        EXPECT_CALL( ras, unlock() )
            .Times(1)
            .After(recvResp);
        ASSERT_EQ( MicDeviceError::errorCode(MICSDKERR_INTERNAL_ERROR), ras.request(&request) );
    }

    TEST_F(RasConnectionTestHeaderSent, TC_request_recvresp_error_opmask_fail_001)
    {
        // Note the mismatch between header.cmd and what request.command() will return:
        // The header "returned" by the call to receive
        header.cmd = (1 | MR_ERROR); // Set error bit

        // The command that will be requested to micras
        EXPECT_CALL( request, command() )
            .WillOnce( Return(0) );
        expectUnlock(recvResp);

        ASSERT_EQ( MicDeviceError::errorCode(MICSDKERR_INTERNAL_ERROR), ras.request(&request) );
    }

    TEST_F(RasConnectionTestHeaderSent, TC_request_recvresp_error_recverror_fail_001)
    {
        // The header "returned" by the call to receive
        header.cmd |= MR_ERROR; // Set error bit
        header.len = sizeof( struct mr_err );

        // The command that will be requested to micras
        EXPECT_CALL( request, command() )
            .WillOnce( Return(0) );
        Expectation recvError;
        // Expect an additional call to receive to get error information
        recvError = EXPECT_CALL( ras, receive( NotNull(), Gt(0) ) )
            .After( recvResp )
            .WillOnce( Return(-1) ); //BUT FAIL

        expectUnlock(recvError);

        ASSERT_EQ( MicDeviceError::errorCode(MICSDKERR_INTERNAL_ERROR), ras.request(&request) );
    }

    TEST_F(RasConnectionTestHeaderSent, TC_request_recvresp_error_garbage_fail_001)
    {
        // The header "returned" by the call to receive
        header.cmd |= MR_ERROR; // Set error bit
        header.len = sizeof( struct mr_err ) + 10; // Garbage will be received

        // The command that will be requested to micras
        EXPECT_CALL( request, command() )
            .WillOnce( Return(0) );
        Expectation recvGarbage;
        // Expect an additional call to receive to get error information
        recvGarbage = EXPECT_CALL( ras, receive( NotNull(), Gt(0) ) )
            .Times(1)
            .After( recvResp );

        expectUnlock(recvGarbage);

        ASSERT_EQ( MicDeviceError::errorCode(MICSDKERR_INTERNAL_ERROR), ras.request(&request) );

    }

    TEST_F(RasConnectionTestHeaderSent, TC_request_recvresp_error_fail_001)
    {
        // The header "returned" by the call to receive
        header.cmd |= MR_ERROR; // Set error bit
        header.len = sizeof( struct mr_err );

        // The micras error struct that will be "returned"
        struct mr_err err = {0, 0}; // We don't care about these values

        // The command that will be requested to micras
        EXPECT_CALL( request, command() )
            .WillOnce( Return(0) );
        Expectation recvError;
        // Expect an additional call to receive to get error information
        recvError = EXPECT_CALL( ras, receive( NotNull(), Gt(0) ) )
            .After( recvResp )
            .WillOnce(
                DoAll(
                    SetArrayArgument<0>(
                        (char*)&err,
                        (char*)&err + sizeof(err)),
                    ReturnArg<1>()
                ));

        expectUnlock(recvError);

        ASSERT_EQ( MicDeviceError::errorCode(MICSDKERR_INTERNAL_ERROR), ras.request(&request) );
    }

    TEST_F(RasConnectionTestHeaderSent, TC_request_resplen_mismatch_001)
    {
        header.len = 4;
        EXPECT_CALL( request, byteCount() ).WillRepeatedly( Return(3) );
        expectUnlock(recvResp);
        ASSERT_EQ( MicDeviceError::errorCode(MICSDKERR_INTERNAL_ERROR), ras.request(&request) );
    }

    TEST_F(RasConnectionTestHeaderSent, TC_request_recvdata_fail_001)
    {
        recvData = EXPECT_CALL( ras, receive( NotNull(), Gt(0) ) )
            .After(recvResp)
            .WillOnce( Return(-1) );
        expectUnlock(recvData);
        ASSERT_EQ( MicDeviceError::errorCode(MICSDKERR_INTERNAL_ERROR), ras.request(&request) );
    }

    TEST_F(RasConnectionTestHeaderSent, TC_request_success_001)
    {
        recvData = EXPECT_CALL( ras, receive( NotNull(), Gt(0) ) )
            .After( recvResp )
            .WillOnce( ReturnArg<1>() );
        ASSERT_EQ( MicDeviceError::errorCode(MICSDKERR_SUCCESS), ras.request(&request) );
    }

} // namespace micmgmt
