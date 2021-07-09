/*
 * Copyright (c) 2017, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
*/

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "Daemon.hpp"
#include "SafeBool.hpp"

#include "handler/PThreshRequestHandler.hpp"
#include "handler/RequestHandler.hpp"
#include "handler/RestartSmba.hpp"
#include "handler/SetRequestHandler.hpp"
#include "handler/SmcRwHandler.hpp"
#include "handler/TurboRequestHandler.hpp"
#include "mocks.hpp"

using ::testing::_;
using ::testing::Gt;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::Throw;

//Request handler to test the abstract base class RequestHandlerBase
//Define a MOCK_METHOD; no need to define it in mocks.hpp.
class TestRequestHandler : public RequestHandlerBase
{
public:
    TestRequestHandler(SystoolsdReq &req, DaemonSession::ptr sess, Daemon &owner) :
        RequestHandlerBase(req, sess, owner){ }
    MOCK_METHOD0(handle_request, void());
};

//DataGroupInterface mock implementation
class FakeDataGroup : public DataGroupInterface
{
public:
    MOCK_METHOD0(get_size, size_t());
    MOCK_METHOD2(copy_data_to, void(char *, size_t*));
    MOCK_METHOD1(get_raw_data, void*(bool));
    MOCK_METHOD0(force_refresh, void());

protected:
    void refresh_data() { return; };
};

//Test fixture for RequestHandlerBase tests
//Create all dependencies
class RequestHandlerBaseTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        scif = ScifEp::ptr(new NiceMock<MockScifEp>()); //will be owned by Daemon instance
        peer = std::make_shared<NiceMock<MockScifEp>>();
        client = peer.get();
        sess = std::make_shared<DaemonSession>(peer);
        bzero(&req, sizeof(req));
        data = new NiceMock<FakeDataGroup>(); //will be owned by Daemon instance
        pthresh_data = new FakeDataGroup; //will be owned by Daemon instance
        turbo_data = new FakeDataGroup;

        //return a default size
        ON_CALL(*data, get_size())
            .WillByDefault(Return(size));

        //return dummy buffer
        ON_CALL(*data, get_raw_data(_))
            .WillByDefault(Return((void*)buf));

        //return root port by default
        ON_CALL(*client, get_port_id())
            .WillByDefault(Return(std::make_pair<uint16_t, uint16_t>(0, 1)));

        //return device is not busy by default
        ON_CALL(i2c, is_device_busy())
            .WillByDefault(Return(BusyInfo(false, 0)));

        ON_CALL(pthresh, window_factory(_))
            .WillByDefault(Return(std::make_shared<NiceMock<MockPWindow>>()));

        data_groups.insert(std::make_pair<uint16_t, DataGroupInterface*>(0, data));
        data_groups.insert(std::make_pair<uint16_t, DataGroupInterface*>(GET_PTHRESH_INFO, pthresh_data));
        data_groups.insert(std::make_pair<uint16_t, DataGroupInterface*>(GET_TURBO_INFO, turbo_data));
        auto services = std::unique_ptr<Services>(new MockServices);
        daemon = new Daemon(scif, std::move(services), data_groups);
    }

    virtual void TearDown()
    {
        delete daemon;
    }

    void client_no_root()
    {
        srand(time(NULL));
        EXPECT_CALL(*client, get_port_id())
            .WillOnce(Return(
                        std::make_pair<uint16_t, uint16_t>(0,
                            (rand() % ((uint16_t)(~0) - 1024) + 1024))));
    }

    ScifEp::ptr scif;
    std::shared_ptr<MockScifEp> peer;
    DaemonSession::ptr sess;
    Daemon *daemon;
    FakeDataGroup *data;
    FakeDataGroup *pthresh_data;
    FakeDataGroup *turbo_data;
    std::map<uint16_t, DataGroupInterface *> data_groups;
    SystoolsdReq req;
    micmgmt::SafeBool stop;
    MockScifEp *client;
    NiceMock<MockI2cAccess> i2c;
    NiceMock<MockPThresh> pthresh;
    NiceMock<MockTurbo> turbo;
    char buf[100]; //dummy buffer to return when calling FakeDataGroup::get_raw_data

    static const uint16_t reqtype = 0;
    static const size_t size = 17;
};

typedef RequestHandlerBaseTest RequestHandlerTest;
typedef RequestHandlerBaseTest SetRequestHandlerTest;
typedef RequestHandlerBaseTest RestartSmbaHandlerTest;
typedef RequestHandlerBaseTest PThreshRequestHandlerTest;
typedef RequestHandlerBaseTest TurboRequestHandlerTest;
typedef RequestHandlerBaseTest SmcRwHandlerTest;

TEST_F(RequestHandlerBaseTest, TC_Run_001)
{
    TestRequestHandler test(req, sess, *daemon);
    EXPECT_CALL(test, handle_request())
        .Times(1);
    ASSERT_NO_THROW(test.Run(stop));
}

TEST_F(RequestHandlerBaseTest, TC_Run_throwbybusy_001)
{
    //Prepare Daemon instance to throw when calling acquire_request()
    try
    {
        while(1)
            daemon->acquire_request();
    }
    catch(SystoolsdException &excp)
    {
        //nothing to do
    }
    TestRequestHandler test(req, sess, *daemon);
    //no call to handle_request() should happen
    EXPECT_CALL(test, handle_request())
        .Times(0);

    //expect that scif client is notified properly
    EXPECT_CALL(*client, send(NotNull(), sizeof(SystoolsdReq)))
        .WillOnce(Return(1));
    ASSERT_NO_THROW(test.Run(stop));
}

TEST_F(RequestHandlerBaseTest, TC_Run_throwbyhandle_001)
{
    //have handle_request() throw an exception
    TestRequestHandler test(req, sess, *daemon);
    EXPECT_CALL(test, handle_request())
        .WillOnce(Throw(SystoolsdException(SYSTOOLSD_UNKOWN_ERROR, "")));
    EXPECT_CALL(*client, send(NotNull(), sizeof(SystoolsdReq)))
        .WillOnce(Return(1));
    ASSERT_NO_THROW(test.Run(stop));

    EXPECT_CALL(test, handle_request())
        .WillOnce(Throw(std::invalid_argument("")));
    EXPECT_CALL(*client, send(NotNull(), sizeof(SystoolsdReq)))
            .WillOnce(Return(1));
    ASSERT_NO_THROW(test.Run(stop));
}

TEST_F(RequestHandlerBaseTest, TC_replyerror_001)
{
    TestRequestHandler test(req, sess, *daemon);
    //expect client gets notified
    EXPECT_CALL(*client, send(NotNull(), sizeof(SystoolsdReq)))
        .WillOnce(Return(1));
    ASSERT_NO_THROW(test.reply_error(SYSTOOLSD_INTERNAL_ERROR));

    EXPECT_CALL(*client, send(NotNull(), sizeof(SystoolsdReq)))
        .WillOnce(Return(1));
    SystoolsdException excp(SYSTOOLSD_INTERNAL_ERROR, "");
    ASSERT_NO_THROW(test.reply_error(excp));
}

TEST_F(RequestHandlerBaseTest, TC_replyerror_throw_001)
{
    TestRequestHandler test(req, sess, *daemon);
    //when it is not possible to reach client, make sure the connection is closed
    EXPECT_CALL(*client, send(NotNull(), sizeof(SystoolsdReq)))
        .WillOnce(Throw(SystoolsdException(SYSTOOLSD_SCIF_ERROR, "")));
    EXPECT_CALL(*client, close()).
        Times(1);
    ASSERT_NO_THROW(test.reply_error(SYSTOOLSD_UNKOWN_ERROR));

    EXPECT_CALL(*client, send(NotNull(), sizeof(SystoolsdReq)))
        .WillOnce(Throw(std::invalid_argument("")));
    EXPECT_CALL(*client, close()).
        Times(1);
    ASSERT_NO_THROW(test.reply_error(SYSTOOLSD_UNKOWN_ERROR));

    SystoolsdException excp(SYSTOOLSD_SCIF_ERROR, "");
    EXPECT_CALL(*client, send(NotNull(), sizeof(SystoolsdReq)))
        .WillOnce(Throw(SystoolsdException(SYSTOOLSD_SCIF_ERROR, "")));
    EXPECT_CALL(*client, close()).
        Times(1);
    ASSERT_NO_THROW(test.reply_error(excp));

    EXPECT_CALL(*client, send(NotNull(), sizeof(SystoolsdReq)))
        .WillOnce(Throw(std::invalid_argument("")));
    EXPECT_CALL(*client, close()).
        Times(1);
    ASSERT_NO_THROW(test.reply_error(excp));
}

TEST_F(RequestHandlerBaseTest, TC_isfromroot_001)
{
    srand(time(NULL));
    std::pair<uint16_t, uint16_t> noroot(0, rand() % ((uint16_t)~0 - 1024) + 1024); //above or equal SCIF_ADMIN_PORT_END
    std::pair<uint16_t, uint16_t> root(0, rand() % 1023 + 1); //below SCIF_ADMIN_PORT_END
    std::cout << "noroot port: " << noroot.second << std::endl;
    std::cout << "root port: " << root.second << std::endl;
    EXPECT_CALL(*client, get_port_id())
        .WillOnce(Return(noroot));
    TestRequestHandler test(req, sess, *daemon);
    EXPECT_FALSE(test.is_from_root());

    EXPECT_CALL(*client, get_port_id())
        .WillOnce(Return(root));
    EXPECT_TRUE(test.is_from_root());
}

TEST_F(RequestHandlerBaseTest, TC_createrequest_001)
{
    req.req_type = SET_PTHRESH_W0;
    SystoolsdServices::ptr services = MockServices::get_services();
    //expect a PThreshRequestHandler
    std::shared_ptr<RequestHandlerBase> handler;
    handler = RequestHandlerBase::create_request(req, sess, *daemon, services);
    EXPECT_TRUE(typeid(*handler.get()) == typeid(PThreshRequestHandler));

    req.req_type = SET_PTHRESH_W1;
    handler = RequestHandlerBase::create_request(req, sess, *daemon, services);
    EXPECT_TRUE(typeid(*handler.get()) == typeid(PThreshRequestHandler));

    req.req_type = RESTART_SMBA;
    handler = RequestHandlerBase::create_request(req, sess, *daemon, services);
    EXPECT_TRUE(typeid(*handler.get()) == typeid(RestartSmba));

    req.req_type = SET_TURBO;
    handler = RequestHandlerBase::create_request(req, sess, *daemon, services);
    EXPECT_TRUE(typeid(*handler.get()) == typeid(TurboRequestHandler));

    std::vector<uint8_t> set_requests = {SET_PWM_ADDER, SET_LED_BLINK};
    for(auto request = set_requests.begin(); request != set_requests.end(); ++request)
    {
        req.req_type = *request;
        handler = RequestHandlerBase::create_request(req, sess, *daemon, services);
        EXPECT_TRUE(typeid(*handler.get()) == typeid(SetRequestHandler));
    }

    std::vector<uint8_t> get_requests = {
        GET_MEMORY_UTILIZATION,
        GET_DEVICE_INFO,
        GET_POWER_USAGE,
        GET_THERMAL_INFO,
        GET_VOLTAGE_INFO,
        GET_DIAGNOSTICS_INFO,
        GET_FWUPDATE_INFO,
        GET_MEMORY_INFO,
        GET_PROCESSOR_INFO,
        GET_CORES_INFO,
        GET_CORE_USAGE,
        GET_PTHRESH_INFO,
        GET_SMBA_INFO
    };

    for(auto request = get_requests.begin(); request != get_requests.end(); ++request)
    {
        req.req_type = *request;
        handler = RequestHandlerBase::create_request(req, sess, *daemon, services);
        EXPECT_TRUE(typeid(*handler.get()) == typeid(RequestHandler));
    }
}

TEST_F(RequestHandlerTest, TC_handlerequest_001)
{
    req.req_type = reqtype;
    RequestHandler handler(req, sess, *daemon);
    //expect a SystoolsdReq object to be sent
    EXPECT_CALL(*client, send(NotNull(), sizeof(SystoolsdReq)));
    //expect real data to be sent
    EXPECT_CALL(*client, send(NotNull(), size));
    ASSERT_NO_THROW(handler.handle_request());
}

TEST_F(RequestHandlerTest, TC_handlerequest_unsupportedreq_001)
{
    //invalid request type
    req.req_type = reqtype + 1;
    //expect client to be notified about error
    EXPECT_CALL(*client, send(NotNull(), sizeof(SystoolsdReq)));
    RequestHandler handler(req, sess, *daemon);
    ASSERT_NO_THROW(handler.handle_request());
}

TEST_F(RequestHandlerTest, TC_servicerequest_001)
{
    req.req_type = reqtype + 1;
    EXPECT_CALL(*client, send(NotNull(), sizeof(SystoolsdReq)));
    RequestHandler handler(req, sess, *daemon);
    ASSERT_NO_THROW(handler.service_request());
}

TEST_F(RequestHandlerTest, TC_servicerequest_throw_001)
{
    EXPECT_CALL(*data, get_raw_data(_))
        .WillOnce(Throw(SystoolsdException(SYSTOOLSD_UNKOWN_ERROR, "")));
    EXPECT_CALL(*client, send(NotNull(), sizeof(SystoolsdReq)));
    RequestHandler handler(req, sess, *daemon);
    ASSERT_NO_THROW(handler.service_request());

    EXPECT_CALL(*data, get_raw_data(_))
        .WillOnce(Throw(std::invalid_argument("")));
    EXPECT_CALL(*client, send(NotNull(), sizeof(SystoolsdReq)));
    ASSERT_NO_THROW(handler.service_request());
}

TEST_F(SetRequestHandlerTest, TC_ctor_001)
{
    ASSERT_NO_THROW(SetRequestHandler handler(req, sess, *daemon, &i2c));
}

TEST_F(SetRequestHandlerTest, TC_ctor_throw_001)
{
    ASSERT_THROW(SetRequestHandler handler(req, sess, *daemon, NULL), std::invalid_argument);
}

TEST_F(SetRequestHandlerTest, TC_handlerequest_001)
{
    req.req_type = SET_LED_BLINK;
    SetRequestHandler handler(req, sess, *daemon, &i2c);
    //expect i2c write to be performed and client to be notified about success
    EXPECT_CALL(i2c, write_u32(_, _));
    EXPECT_CALL(*client, send(NotNull(), sizeof(SystoolsdReq)));
    ASSERT_NO_THROW(handler.handle_request());
}

TEST_F(SetRequestHandlerTest, TC_handlerequest_unsupportedreq_001)
{
    req.req_type = 0; //unsupported "set" request type
    //make sure no i2c operation is performed
    EXPECT_CALL(i2c, write_u32(_, _))
        .Times(0);
    //expect client is notified about error
    EXPECT_CALL(*client, send(NotNull(), sizeof(SystoolsdReq)))
        .Times(1);
    SetRequestHandler handler(req, sess, *daemon, &i2c);
    ASSERT_NO_THROW(handler.handle_request());
}

TEST_F(SetRequestHandlerTest, TC_handlerequest_noroot_001)
{
    req.req_type = SET_LED_BLINK;
    client_no_root();
    EXPECT_CALL(*client, send(NotNull(), sizeof(SystoolsdReq)));
    EXPECT_CALL(i2c, write_u32(_, _))
        .Times(0);
    SetRequestHandler handler(req, sess, *daemon, &i2c);
    ASSERT_NO_THROW(handler.handle_request());
}

TEST_F(SetRequestHandlerTest, TC_handlerequest_writefail_001)
{
    req.req_type = SET_LED_BLINK;
    EXPECT_CALL(i2c, write_u32(_, _))
        .WillOnce(Throw(SystoolsdException(SYSTOOLSD_UNKOWN_ERROR, "")));
    EXPECT_CALL(*client, send(NotNull(), sizeof(SystoolsdReq)));
    SetRequestHandler handler(req, sess, *daemon, &i2c);
    ASSERT_NO_THROW(handler.handle_request());

}

TEST_F(SetRequestHandlerTest, TC_handlerequest_noack_001)
{
    req.req_type = SET_LED_BLINK;
    EXPECT_CALL(*client, send(NotNull(), sizeof(SystoolsdReq)))
        .WillOnce(Throw(SystoolsdException(SYSTOOLSD_SCIF_ERROR, "")));
    EXPECT_CALL(*client, close());
    SetRequestHandler handler(req, sess, *daemon, &i2c);
    ASSERT_NO_THROW(handler.handle_request());

}

TEST_F(RestartSmbaHandlerTest, TC_ctor_001)
{
    req.req_type = RESTART_SMBA;
    EXPECT_CALL(i2c, is_device_busy())
        .WillOnce(Return(BusyInfo(false, 0)));
    RestartSmba handler(req, sess, *daemon, &i2c);
    ASSERT_EQ(0, handler.error);
}

TEST_F(RestartSmbaHandlerTest, TC_ctor_throw_nulli2c_001)
{
    req.req_type = RESTART_SMBA;
    ASSERT_THROW(RestartSmba(req, sess, *daemon, NULL), std::invalid_argument);
}

TEST_F(RestartSmbaHandlerTest, TC_handlerequest_noroot_001)
{
    client_no_root();
    EXPECT_CALL(*client, send(NotNull(), sizeof(SystoolsdReq)));
    RestartSmba handler(req, sess, *daemon, &i2c);
    ASSERT_NO_THROW(handler.handle_request());
    ASSERT_EQ(SYSTOOLSD_INSUFFICIENT_PRIVILEGES, handler.error);
}

TEST_F(RestartSmbaHandlerTest, TC_handlerequest_throw_001)
{
    EXPECT_CALL(i2c, restart_device(_))
        .WillOnce(Throw(SystoolsdException(SYSTOOLSD_UNKOWN_ERROR, "")));
    ASSERT_NO_THROW(RestartSmba handler(req, sess, *daemon, &i2c));
    EXPECT_CALL(i2c, restart_device(_))
        .WillOnce(Throw(std::invalid_argument("")));
    ASSERT_NO_THROW(RestartSmba handler(req, sess, *daemon, &i2c));
}

TEST_F(RestartSmbaHandlerTest, TC_handlerequest_busy_001)
{
    EXPECT_CALL(i2c, is_device_busy())
        .WillOnce(Return(BusyInfo(true, 434)));
    ASSERT_NO_THROW(RestartSmba handler(req, sess, *daemon, &i2c));
}

TEST_F(PThreshRequestHandlerTest, TC_ctor_001)
{
    ASSERT_NO_THROW(PThreshRequestHandler(req, sess, *daemon, &pthresh));
}

TEST_F(PThreshRequestHandlerTest, TC_ctor_throw_001)
{
    ASSERT_THROW(PThreshRequestHandler(req, sess, *daemon, NULL), std::invalid_argument);
}

TEST_F(PThreshRequestHandlerTest, TC_handlerequest_001)
{
    req.req_type = SET_PTHRESH_W0;
    //for a successful request, two different SystoolsdReq headers must be sent
    //one is the ACK, the second one is the notification of success/failure
    EXPECT_CALL(*client, send(NotNull(), sizeof(SystoolsdReq)))
        .Times(2);
    //expect a PowerWindowInfo to be received
    EXPECT_CALL(*client, recv(NotNull(), sizeof(PowerWindowInfo), _))
        .WillOnce(Return(int(sizeof(PowerWindowInfo))));

    //Create a new daemon instance with a FakeDataGroup to play as a pthresh info group
    //expect force_refresh() to be called on the data group for GET_PTHRESH_INFO
    EXPECT_CALL(*pthresh_data, force_refresh());
    PThreshRequestHandler handler(req, sess, *daemon, &pthresh);
    ASSERT_NO_THROW(handler.handle_request());
}

TEST_F(PThreshRequestHandlerTest, TC_handlerequest_noroot_001)
{
    client_no_root();
    //expect a SystoolsdReq header to be sent signaling insufficient privileges
    EXPECT_CALL(*client, send(NotNull(), sizeof(SystoolsdReq)));
    PThreshRequestHandler handler(req, sess, *daemon, &pthresh);
    ASSERT_NO_THROW(handler.handle_request());
}

TEST_F(PThreshRequestHandlerTest, TC_handlerequest_noack_001)
{
    EXPECT_CALL(pthresh, window_factory(_))
        .WillOnce(Throw(SystoolsdException(SYSTOOLSD_INTERNAL_ERROR, "inval")));
    PThreshRequestHandler handler(req, sess, *daemon, &pthresh);
    ASSERT_NO_THROW(handler.handle_request());
}

TEST_F(PThreshRequestHandlerTest, TC_handlerequest_protmismatch_001)
{
    EXPECT_CALL(*client, recv(NotNull(), sizeof(PowerWindowInfo), _))
        .WillOnce(Return(sizeof(PowerWindowInfo) - 1));
    //expect client to be closed, protocol mismatch
    EXPECT_CALL(*client, close());
    PThreshRequestHandler handler(req, sess, *daemon, &pthresh);
    ASSERT_NO_THROW(handler.handle_request());
}

TEST_F(PThreshRequestHandlerTest, TC_handlerequest_errorsetting_001)
{
    EXPECT_CALL(*client, send(NotNull(), sizeof(SystoolsdReq)))
        .Times(2);
    //expect a PowerWindowInfo to be received
    EXPECT_CALL(*client, recv(NotNull(), sizeof(PowerWindowInfo), _))
        .WillOnce(Return(int(sizeof(PowerWindowInfo))));
    auto pwindow = std::make_shared<NiceMock<MockPWindow>>();
    EXPECT_CALL(pthresh, window_factory(_))
        .WillOnce(Return(pwindow));
    EXPECT_CALL(*pwindow.get(), set_time_window(_))
        .WillOnce(Throw(SystoolsdException(SYSTOOLSD_UNKOWN_ERROR, "")));
    PThreshRequestHandler handler(req, sess, *daemon, &pthresh);
    ASSERT_NO_THROW(handler.handle_request());
}

TEST_F(TurboRequestHandlerTest, TC_ctor_001)
{
    ASSERT_NO_THROW(TurboRequestHandler h(req, sess, *daemon, &turbo));
}

TEST_F(TurboRequestHandlerTest, TC_ctor_throw_001)
{
    ASSERT_THROW(TurboRequestHandler h(req, sess, *daemon, NULL), std::invalid_argument);
}

TEST_F(TurboRequestHandlerTest, TC_handlerequest_001)
{
    req.data[0] = '1';
    EXPECT_CALL(*client, send(NotNull(), sizeof(SystoolsdReq)))
        .Times(2);
    EXPECT_CALL(turbo, set_turbo_enabled(true));
    //expect turbo info group to be refreshed for each call to handle_request
    EXPECT_CALL(*turbo_data, force_refresh())
        .Times(2);
    TurboRequestHandler handler(req, sess, *daemon, &turbo);
    ASSERT_NO_THROW(handler.handle_request());
    req.data[0] = 0;
    EXPECT_CALL(turbo, set_turbo_enabled(false));
    TurboRequestHandler handler2(req, sess, *daemon, &turbo);
    ASSERT_NO_THROW(handler2.handle_request());
}

TEST_F(TurboRequestHandlerTest, TC_handlerequest_noroot_001)
{
    client_no_root();
    //set_turbo_enabled() musn't be called
    EXPECT_CALL(turbo, set_turbo_enabled(_))
        .Times(0);
    EXPECT_CALL(*client, send(NotNull(), sizeof(SystoolsdReq)));
    TurboRequestHandler handler(req, sess, *daemon, &turbo);
    ASSERT_NO_THROW(handler.handle_request());
}

TEST_F(SmcRwHandlerTest, TC_handlerequest_001)
{
    req.length = 1;
    req.extra = 0x60; // Any register..
    {
        // read operation
        req.req_type = READ_SMC_REG;
        EXPECT_CALL(i2c, read_bytes(_, _, _))
            .Times(1);
        SmcRwHandler handler(req, sess, *daemon, &i2c);
        ASSERT_NO_THROW(handler.handle_request());
    }

    {
        // write operation
        req.req_type = WRITE_SMC_REG;
        EXPECT_CALL(i2c, write_bytes(_, _, _))
            .Times(1);
        SmcRwHandler handler(req, sess, *daemon, &i2c);
        ASSERT_NO_THROW(handler.handle_request());
    }
}

TEST_F(SmcRwHandlerTest, TC_handlerequest_noroot_001)
{
    client_no_root();
    EXPECT_CALL(i2c, read_bytes(_, _, _))
        .Times(0);
    EXPECT_CALL(*client, send(NotNull(), sizeof(SystoolsdReq)));
    SmcRwHandler handler(req, sess, *daemon, &i2c);
    ASSERT_NO_THROW(handler.handle_request());
}

TEST_F(SmcRwHandlerTest, TC_handlerequest_ctor_throw_001)
{
    ASSERT_THROW(SmcRwHandler handler(req, sess, *daemon, NULL),
            std::invalid_argument);
}

