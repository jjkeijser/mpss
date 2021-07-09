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
#include "SystoolsdConnection.hpp"

// SCIF PROTOCOL INCLUDES
//
#include "systoolsd_api.h"

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
    const int SUCCESS = micmgmt::MicDeviceError::errorCode( MICSDKERR_SUCCESS );
    const int INVALID_ARG = micmgmt::MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );
    const int DEVNUM = 2;
    const std::string SCIF_ERROR_STR("this is a scif error");
    const uint32_t SCIF_FD = 7;
    const int COMMAND = 0x1F;
    const uint32_t PARAMETER = 0xabad1dea;
    const uint32_t SMC_REG = 0x60;
    const size_t BYTE_COUNT = 64;
    char BUFFER[BYTE_COUNT];
}

namespace micmgmt
{
    //////////////////////////////////////////////////////////////////////////////////////
    //                          Base testing fixture class
    //////////////////////////////////////////////////////////////////////////////////////
    class SystoolsdConnectionTestBase : public ::testing::Test
    {
    public:
        // Ctor
        SystoolsdConnectionTestBase() : systoolsd(DEVNUM) { }

    protected:
        // Members representing the systoolsd connection and the SCIF request
        MockSystoolsdConnection systoolsd;
        MockScifRequest request;
        // Expectation members. Use the in .After() clauses to dictate the order
        // in which calls must happen
        Expectation sendHdr, recvResp, sendPayload;
        // SystoolsdReq header member
        SystoolsdReq header;

    protected:
        virtual void SetUp()
        {
            bzero(&header, sizeof(header));
            setConnectionDefaults( toMockScifConnection(systoolsd) );
            setRequestDefaults(request);
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

        // Helper function to return a pointer to MockScifConnection
        MockScifConnection& toMockScifConnection( MockSystoolsdConnection& systoolsdConn )
        {
            // Will throw bad_cast if fails
            return dynamic_cast<MockScifConnection&>(systoolsdConn);
        }

    }; // SystoolsdConnectionTestPrelude

    class SystoolsdConnectionSendRequestTest : public SystoolsdConnectionTestBase
    {
    protected:
        virtual void SetUp()
        {
            SystoolsdConnectionTestBase::SetUp();
            EXPECT_CALL( request, isSendRequest() )
                .WillRepeatedly( Return(true) );

            // Set expectation for protocol: send header, recv response
            // Tests using this fixture are responsible for controlling
            // subsequent send/recv

            // send header
            sendHdr = EXPECT_CALL( systoolsd, send( NotNull(), Gt(0) ) )
                .WillOnce( ReturnArg<1>() );
            // receive response
            recvResp = EXPECT_CALL( systoolsd, receive( NotNull(), Gt(0) ) )
                .After(sendHdr) // must happen after sending the request header
                .WillOnce( ReturnArg<1>() );
        }

    };

    class SystoolsdConnectionRecvRequestTest : public SystoolsdConnectionTestBase
    {
    protected:
        virtual void SetUp()
        {
            SystoolsdConnectionTestBase::SetUp();
            // This is not a send request
            EXPECT_CALL( request, isSendRequest() )
                .WillRepeatedly( Return(false) );

            // Set expectation for protocol: only send header
            // Tests using this fixture are responsible for controlling
            // the receiving of the response

            // send header
            sendHdr = EXPECT_CALL( systoolsd, send( NotNull(), Gt(0) ) )
                .WillOnce( ReturnArg<1>() );
        }
    };

    //SystoolsdConnectionTestPrelude abstracts the first portion that happens in a request
    // i.e. sending the request header and receiving the response from systoolsd
    typedef SystoolsdConnectionTestBase SystoolsdConnectionTestPrelude;

    // Fixture class for SystoolsdConnectionAbstract::smcRequest()
    class SystoolsdConnectionSmcRequestTest : public SystoolsdConnectionTestBase
    {
        protected:
            MockScifRequest smcRequest;
            static const size_t bufSize = 4;
            char buf[bufSize];

        virtual void SetUp()
        {
            SystoolsdConnectionTestBase::SetUp();
            bzero(buf, bufSize);

            ON_CALL(smcRequest, isValid())
                .WillByDefault(Return(true));
            ON_CALL(smcRequest, isError())
                .WillByDefault(Return(false));
            ON_CALL(smcRequest, isSendRequest())
                .WillByDefault(Return(false));

            // By default, return READ_SMC_REG-type request
            ON_CALL(smcRequest, command())
                .WillByDefault(Return(READ_SMC_REG));
            ON_CALL(smcRequest, parameter())
                .WillByDefault(Return(SMC_REG));
            ON_CALL(smcRequest, buffer())
                .WillByDefault(Return(buf));
            ON_CALL(smcRequest, byteCount())
                .WillByDefault(Return(bufSize));
            ON_CALL(smcRequest, errorCode())
                .WillByDefault(Return(0));
        }
    };


    //////////////////////////////////////////////////////////////////////////////////////
    //                              TESTS BEGIN HERE
    //////////////////////////////////////////////////////////////////////////////////////
    TEST_F(SystoolsdConnectionTestPrelude, TC_ctor_001)
    {
        ASSERT_EQ( DEVNUM, systoolsd.deviceNum() );
    }

    TEST_F(SystoolsdConnectionTestPrelude, TC_request_scif_open_fail_001)
    {
        EXPECT_CALL( systoolsd, isOpen() )
            .WillOnce( Return(false) );

        ASSERT_EQ( MicDeviceError::errorCode(MICSDKERR_DEVICE_NOT_OPEN), systoolsd.request(NULL) );
    }

    TEST_F(SystoolsdConnectionTestPrelude, TC_request_nullptr_fail_001)
    {
        ASSERT_EQ( MicDeviceError::errorCode(MICSDKERR_INVALID_ARG), systoolsd.request(NULL) );
    }

    TEST_F(SystoolsdConnectionTestPrelude, TC_request_nullbuf_fail_001)
    {
        EXPECT_CALL( request, isSendRequest() ).WillOnce( Return(true) );
        EXPECT_CALL( request, buffer() ).WillOnce( Return((char*)NULL) );
        ASSERT_EQ( MicDeviceError::errorCode(MICSDKERR_INVALID_ARG), systoolsd.request(&request) );
    }

    TEST_F(SystoolsdConnectionTestPrelude, TC_request_bytecount_0_fail_001)
    {
        EXPECT_CALL( request, isSendRequest() ).WillOnce( Return(true) );
        EXPECT_CALL( request, byteCount() ).WillOnce( Return(0) );
        ASSERT_EQ( MicDeviceError::errorCode(MICSDKERR_INVALID_ARG), systoolsd.request(&request) );
    }

    TEST_F(SystoolsdConnectionTestPrelude, TC_request_lock_fail_001)
    {
        EXPECT_CALL( systoolsd, lock() ).WillOnce( Return(false) );
        ASSERT_EQ( MicDeviceError::errorCode(MICSDKERR_INTERNAL_ERROR), systoolsd.request(&request) );
    }

    TEST_F(SystoolsdConnectionTestPrelude, TC_request_sendreq_fail_001)
    {
        // Test request(), expect failure because the first send() fails
        EXPECT_CALL( systoolsd, send( NotNull(), Gt(0) ) ).WillOnce( Return(-1) );
        // Expect unlock() to be called
        EXPECT_CALL( systoolsd, unlock() ).Times(1);
        ASSERT_EQ( MicDeviceError::errorCode(MICSDKERR_INTERNAL_ERROR), systoolsd.request(&request) );
    }

    TEST_F(SystoolsdConnectionTestPrelude, TC_request_receive_resp_fail_001)
    {
        // Test request(), expect failure while receiving the header
        EXPECT_CALL( systoolsd, receive( NotNull(), Gt(0) ) ).WillOnce( Return(-1) );
        // Expect unlock() to be called
        EXPECT_CALL( systoolsd, unlock() ).Times(1);
        ASSERT_EQ( MicDeviceError::errorCode(MICSDKERR_INTERNAL_ERROR), systoolsd.request(&request) );
    }

    TEST_F(SystoolsdConnectionTestPrelude, TC_request_receive_resperror_fail_001)
    {
        // Test request(), expect failure in response from systoolsd, card_errno field set

        // Create the request header that will be "read" by receive
        header.card_errno = 1; // Set error
        // The call to receive must:
        EXPECT_CALL( systoolsd, receive( NotNull(), Gt(0) ) )
            .WillOnce(
                DoAll(
                    // Write our request object to buffer
                    SetArrayArgument<0>( (char*)&header, (char*)&header + sizeof(header) ),
                    // Return the length of data "read"
                    ReturnArg<1>()
                ));
        // Expect unlock() to be called
        EXPECT_CALL( systoolsd, unlock() ).Times(1);
        ASSERT_EQ( MicDeviceError::errorCode(MICSDKERR_INTERNAL_ERROR), systoolsd.request(&request) );
    }

    TEST_F(SystoolsdConnectionTestPrelude, TC_request_receive_resperror_flush_fail_001)
    {
        // Test request(), expect failure in response from systoolsd, card_errno field set
        // Ensure that unsolicited data sent by systoolsd gets flushed

        // Create the request header that will be "read" by receive
        const uint16_t length = 16;
        header.card_errno = 1; // Set error
        header.length = length; // Set the length
        // The first call to receive must:
        recvResp = EXPECT_CALL( systoolsd, receive( NotNull(), Gt(0) ) )
            .WillOnce(
                DoAll(
                    // Write our request object to buffer
                    SetArrayArgument<0>( (char*)&header, (char*)&header + sizeof(header) ),
                    // Return the length of data "read"
                    ReturnArg<1>()
                ));

        // Expect a second call to receive to flush unsolicited data
        EXPECT_CALL( systoolsd, receive( NotNull(), length ) )
            .After(recvResp);

        // Expect unlock() to be called
        EXPECT_CALL( systoolsd, unlock() ).Times(1);
        ASSERT_EQ( MicDeviceError::errorCode(MICSDKERR_INTERNAL_ERROR), systoolsd.request(&request) );
    }

    TEST_F(SystoolsdConnectionSendRequestTest, TC_request_send_payload_fail_001)
    {
        EXPECT_CALL( systoolsd, send( NotNull(), Gt(0) ) )
            .After(recvResp) // Must happen after receiving the response
            .WillOnce( Return(-1) );
        EXPECT_CALL( systoolsd, unlock() ).Times(1);
        ASSERT_EQ( MicDeviceError::errorCode(MICSDKERR_INTERNAL_ERROR), systoolsd.request(&request) );
    }

    TEST_F(SystoolsdConnectionSendRequestTest, TC_request_receive_respfinal_fail_001)
    {
        sendPayload = EXPECT_CALL( systoolsd, send( NotNull(), Gt(0) ) )
            .After(recvResp)
            .WillOnce( ReturnArg<1>() );
        EXPECT_CALL( systoolsd, receive( NotNull(), Gt(0) ) )
            .After(sendPayload)
            .WillOnce( Return(-1) );
        // Expect unlock() to be called
        EXPECT_CALL( systoolsd, unlock() ).Times(1);
        ASSERT_EQ( MicDeviceError::errorCode(MICSDKERR_INTERNAL_ERROR), systoolsd.request(&request) );
    }

    TEST_F(SystoolsdConnectionSendRequestTest, TC_request_respfinal_error_fail_001)
    {
        sendPayload = EXPECT_CALL( systoolsd, send( NotNull(), Gt(0) ) )
            .After(recvResp)
            .WillOnce( ReturnArg<1>() );
        header.card_errno = 1;
        EXPECT_CALL( systoolsd, receive( NotNull(), Gt(0) ) )
            .After(sendPayload)
            .WillOnce( DoAll(
                SetArrayArgument<0>( (char*) &header, (char*) &header + sizeof(header) ),
                ReturnArg<1>()
            ));
        // Expect unlock() to be called
        EXPECT_CALL( systoolsd, unlock() ).Times(1);
        ASSERT_EQ( MicDeviceError::errorCode(MICSDKERR_INTERNAL_ERROR), systoolsd.request(&request) );
    }

    TEST_F(SystoolsdConnectionSendRequestTest, TC_request_respfinal_error_flush_fail_001)
    {
        sendPayload = EXPECT_CALL( systoolsd, send( NotNull(), Gt(0) ) )
            .After(recvResp)
            .WillOnce( ReturnArg<1>() );
        const uint16_t length = 60;
        header.card_errno = 1;
        header.length = length;
        Expectation recvFinal = EXPECT_CALL( systoolsd, receive( NotNull(), Gt(0) ) )
            .After(sendPayload)
            .WillOnce( DoAll(
                SetArrayArgument<0>( (char*) &header, (char*) &header + sizeof(header) ),
                ReturnArg<1>()
            ));
        EXPECT_CALL( systoolsd, receive( NotNull(), Gt(0) ) )
            .After(recvFinal);
        // Expect unlock() to be called
        EXPECT_CALL( systoolsd, unlock() ).Times(1);
        ASSERT_EQ( MicDeviceError::errorCode(MICSDKERR_INTERNAL_ERROR), systoolsd.request(&request) );
    }

    TEST_F(SystoolsdConnectionSendRequestTest, TC_request_success_001)
    {
        sendPayload = EXPECT_CALL( systoolsd, send( NotNull(), Gt(0) ) )
            .After(recvResp)
            .WillOnce( ReturnArg<1>() );
        header.card_errno = 0; // Success
        EXPECT_CALL( systoolsd, receive( NotNull(), Gt(0) ) )
            .After(sendPayload)
            .WillOnce( DoAll(
                SetArrayArgument<0>( (char*) &header, (char*) &header + sizeof(header) ),
                ReturnArg<1>()
            ));
        // Expect unlock() to be called
        EXPECT_CALL( systoolsd, unlock() ).Times(1);
        ASSERT_EQ( MicDeviceError::errorCode(MICSDKERR_SUCCESS), systoolsd.request(&request) );
    }

    TEST_F(SystoolsdConnectionRecvRequestTest, TC_request_length_mismatch_fail_001)
    {
        // Expect byteCount() to return something different from header.length
        EXPECT_CALL( request, byteCount() )
            .WillRepeatedly( Return(5) );
        header.length = 1;

        recvResp = EXPECT_CALL( systoolsd, receive( NotNull(), Gt(0) ) )
            .After(sendHdr)
            .WillOnce(
                DoAll(
                    SetArrayArgument<0>((char*) &header, (char*) &header + sizeof(header) ),
                    ReturnArg<1>()
            ));
        // Expect unlock() to be called
        EXPECT_CALL( systoolsd, unlock() ).Times(1);
        ASSERT_EQ( MicDeviceError::errorCode(MICSDKERR_INTERNAL_ERROR), systoolsd.request(&request) );
    }

    TEST_F(SystoolsdConnectionRecvRequestTest, TC_request_finalresp_fail_001)
    {
        // Expect byteCount() to be just like header.length
        const size_t length = 64;
        EXPECT_CALL( request, byteCount() )
            .WillRepeatedly( Return(length) );
        header.length = length;
        recvResp = EXPECT_CALL( systoolsd, receive( NotNull(), Gt(0) ) )
            .After(sendHdr)
            .WillOnce(
                DoAll(
                    SetArrayArgument<0>((char*) &header, (char*) &header + sizeof(header) ),
                    ReturnArg<1>()
            ));

        EXPECT_CALL( systoolsd, receive( NotNull(), Gt(0) ) )
            .After(recvResp)
            .WillOnce( Return(-1) );
        // Expect unlock() to be called
        EXPECT_CALL( systoolsd, unlock() ).Times(1);
        ASSERT_EQ( MicDeviceError::errorCode(MICSDKERR_INTERNAL_ERROR), systoolsd.request(&request) );
    }

    TEST_F(SystoolsdConnectionRecvRequestTest, TC_request_success_001)
    {
        // Expect byteCount() to be just like header.length
        const size_t length = 64;
        EXPECT_CALL( request, byteCount() )
            .WillRepeatedly( Return(length) );
        header.length = length;
        recvResp = EXPECT_CALL( systoolsd, receive( NotNull(), Gt(0) ) )
            .After(sendHdr)
            .WillOnce(
                DoAll(
                    SetArrayArgument<0>((char*) &header, (char*) &header + sizeof(header) ),
                    ReturnArg<1>()
            ));

        EXPECT_CALL( systoolsd, receive( NotNull(), Gt(0) ) )
            .After(recvResp)
            .WillOnce( ReturnArg<1>() );
        // Expect unlock() to be called
        EXPECT_CALL( systoolsd, unlock() ).Times(1);
        ASSERT_EQ( MicDeviceError::errorCode(MICSDKERR_SUCCESS), systoolsd.request(&request) );
    }

    TEST_F(SystoolsdConnectionSmcRequestTest, TC_smcrequest_success_001)
    {
        EXPECT_CALL( systoolsd, send( NotNull(), Gt(0) ) )
                .Times(1);
        EXPECT_CALL( systoolsd, receive( NotNull(), Gt(0) ) )
                .Times(1);
        ASSERT_EQ( SUCCESS, systoolsd.smcRequest(&smcRequest) );
    }

    TEST_F(SystoolsdConnectionSmcRequestTest, TC_smcrequest_scif_notopen_fail_001)
    {
        EXPECT_CALL( systoolsd, isOpen() )
            .WillOnce( Return(false) );
        ASSERT_EQ( MicDeviceError::errorCode(MICSDKERR_DEVICE_NOT_OPEN),
                systoolsd.smcRequest(&smcRequest) );
    }

    TEST_F(SystoolsdConnectionSmcRequestTest, TC_smcrequest_nullreq_fail_001)
    {
        ASSERT_EQ( INVALID_ARG, systoolsd.smcRequest(NULL) );
    }

    TEST_F(SystoolsdConnectionSmcRequestTest, TC_smcrequest_nullbuf_fail_001)
    {
        EXPECT_CALL( smcRequest, buffer() )
            .WillOnce( Return((char*)NULL) );
        ASSERT_EQ( INVALID_ARG, systoolsd.smcRequest(&smcRequest) );
    }

    TEST_F(SystoolsdConnectionSmcRequestTest, TC_smcrequest_0bytes_fail_001)
    {
        EXPECT_CALL( smcRequest, byteCount() )
            .WillOnce( Return(0) );
        ASSERT_EQ( INVALID_ARG, systoolsd.smcRequest(&smcRequest) );
    }

    TEST_F(SystoolsdConnectionSmcRequestTest, TC_smcrequest_too_much_data_fail_001)
    {
        // Expect failure when attempting a WRITE_SMC_REG request with byteCount()
        // greater than SYSTOOLSDREQ_MAX_DATA_LENGTH
        EXPECT_CALL( smcRequest, command() )
            .WillRepeatedly( Return(WRITE_SMC_REG) );
        EXPECT_CALL( smcRequest, byteCount() )
            .WillRepeatedly( Return(SYSTOOLSDREQ_MAX_DATA_LENGTH + 1) );
        ASSERT_EQ( INVALID_ARG, systoolsd.smcRequest(&smcRequest) );
    }

    TEST_F(SystoolsdConnectionSmcRequestTest, TC_smcrequest_not_smc_req_fail_001)
    {
        // Expect failure when passing a non-smc-related ScifRequest to smcRequest()
        EXPECT_CALL( smcRequest, command() )
            .WillRepeatedly( Return(GET_SYSTOOLSD_INFO) );
        ASSERT_EQ( INVALID_ARG, systoolsd.smcRequest(&smcRequest) );
    }

} //namespace micmgmt

