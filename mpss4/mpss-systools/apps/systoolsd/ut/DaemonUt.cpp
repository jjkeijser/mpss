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

#include <memory>
#include <stdexcept>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "Daemon.hpp"
#include "mocks.hpp"
#include "SystoolsdException.hpp"
#include "SystoolsdServices.hpp"

#include "systoolsd_api.h"

using std::unique_ptr;

using ::testing::_;
using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::Gt;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::ReturnArg;
using ::testing::SetArgumentPointee;
using ::testing::Throw;

namespace
{
    const uint16_t OPEN = 1 << 0;
    const uint16_t CLOSE = 1 << 1;
    const uint16_t GET_NODE_IDS = 1 << 2;
    const uint16_t BIND = 1 << 3;
    const uint16_t LISTEN = 1 << 4;

    const int EPD = 8;
    const int THIS_NODE = 2;
    const int FD = 777;

    std::unique_ptr<Services> get_mocks()
    {
        return std::unique_ptr<Services>(new MockServices);
    }

} //anon namespace

//TODO: Change these UTs so that they use a mock implementation of ScifEpInterface,
//rather than using a concrete ScifEp object with an injected mock implementation
//of ScifSocket.

class DaemonTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        fake = new FakeScifImpl();
        set_defaults(*fake);
    }

    virtual void set_defaults(FakeScifImpl &impl)
    {
        ON_CALL(impl, open())
            .WillByDefault(Return(EPD));
        //make get_node_ids() return 1 by default
        ON_CALL(impl, get_node_ids(_, _, _))
            .WillByDefault(DoAll(SetArgumentPointee<2>(THIS_NODE),
                           Return(1)));
        ON_CALL(impl, bind(Gt(0), Gt(0)))
            .WillByDefault(ReturnArg<1>());
        ON_CALL(impl, close(Gt(0)))
            .WillByDefault(Return(0));
        ON_CALL(impl, send(Gt(0), NotNull(), Gt(0), _))
            .WillByDefault(ReturnArg<2>());
        ON_CALL(impl, recv(Gt(0), NotNull(), Gt(0), _))
            .WillByDefault(ReturnArg<2>());
        ON_CALL(impl, get_fd(Gt(0)))
            .WillByDefault(Return(FD));
    }

    virtual void install_scif_expectations(FakeScifImpl &impl, uint32_t flags=OPEN|CLOSE|GET_NODE_IDS|
                                                          BIND|LISTEN)
    {
        if(flags & OPEN)
            EXPECT_CALL(impl, open())
                .Times(1);

        if(flags & CLOSE)
            EXPECT_CALL(impl, close(Gt(0)))
                .Times(1);

        if(flags & GET_NODE_IDS)
            EXPECT_CALL(impl, get_node_ids(_, _, _))
                .Times(1);

        if(flags & BIND)
            EXPECT_CALL(impl, bind(Gt(0), Gt(0)))
                .Times(1);

        if(flags & LISTEN)
            EXPECT_CALL(impl, listen(Gt(0), Gt(0)))
                .Times(1);
    }

    virtual void install_scif_expectations(uint32_t flags=OPEN|CLOSE|GET_NODE_IDS|
                                                          BIND|LISTEN)
    {
        install_scif_expectations(*fake, flags);
    }

    std::shared_ptr<ScifEpInterface> get_ep_from_accept()
    {
        //scif implementation for the "accepted" scif endpoint
        //Use a NiceMock for this mock implementation to avoid "uninteresting calls"
        std::shared_ptr<FakeScifImpl> spimpl(new NiceMock<FakeScifImpl>);
        //Use the protected constructor for ScifEp objects created from
        // a call to accept()
        set_defaults(*(FakeScifImpl*)(spimpl.get()));
        return std::shared_ptr<ScifEp>(new ScifEp(spimpl, EPD+1, THIS_NODE+1, 4444));

    }

    ScifEp::ptr get_scif(ScifSocket *impl)
    {
        return ScifEp::ptr(new ScifEp(impl));
    }

    FakeScifImpl *fake;
    ScifEp::ptr scif;
};

TEST_F(DaemonTest, TC_ctor_001)
{
    install_scif_expectations(OPEN|CLOSE|GET_NODE_IDS);
    scif = get_scif(fake);
    ASSERT_NO_THROW(Daemon daemon(scif));
}

TEST_F(DaemonTest, TC_ctor_002)
{
    scif = get_scif(new NiceMock<FakeScifImpl>());
    ASSERT_NO_THROW(Daemon daemon(scif, get_mocks()));
    delete fake;
}

TEST_F(DaemonTest, TC_ctor_throw_noscif_001)
{
    {
        ASSERT_THROW(Daemon daemon(NULL, get_mocks()), std::invalid_argument);
    }

    {
        ASSERT_THROW(Daemon daemon(NULL), std::invalid_argument);
    }

    {
        ASSERT_THROW(Daemon daemon(NULL, get_mocks(), std::map<uint16_t, DataGroupInterface*>()),
                     std::invalid_argument);
    }
    delete fake;
}

TEST_F(DaemonTest, TC_start_001)
{
    install_scif_expectations();
    scif = get_scif(fake);
    Daemon daemon(scif);
    ASSERT_NO_THROW(daemon.start());
}

TEST_F(DaemonTest, TC_start_throw_by_bind_001)
{
    install_scif_expectations(OPEN|CLOSE|GET_NODE_IDS);
    EXPECT_CALL(*fake, bind(Gt(0), Gt(0)))
        .WillOnce(Return(-1));
    scif = get_scif(fake);
    Daemon daemon(scif);
    ASSERT_THROW(daemon.start(), SystoolsdException);
}

TEST_F(DaemonTest, TC_stop_001)
{
    install_scif_expectations(OPEN|CLOSE|GET_NODE_IDS|BIND|LISTEN);
    scif = get_scif(fake);
    Daemon daemon(scif);
    ASSERT_NO_THROW(daemon.start());
    ASSERT_NO_THROW(daemon.stop());
}

TEST_F(DaemonTest, TC_stop_throw_by_double_stop_001)
{
    install_scif_expectations(OPEN|CLOSE|GET_NODE_IDS|BIND|LISTEN);
    scif = get_scif(fake);
    Daemon daemon(scif);
    ASSERT_NO_THROW(daemon.start());
    ASSERT_NO_THROW(daemon.stop());
    ASSERT_NO_THROW(daemon.stop());
}

TEST_F(DaemonTest, TC_stop_throw_by_shutdown_true_001)
{
    install_scif_expectations(OPEN|CLOSE|GET_NODE_IDS|BIND|LISTEN);
    scif = get_scif(fake);
    Daemon daemon(scif);
    ASSERT_NO_THROW(daemon.start());
    daemon.m_shutdown = true;
    ASSERT_NO_THROW(daemon.stop());
}

TEST_F(DaemonTest, TC_acquire_request_001)
{
    install_scif_expectations(OPEN|CLOSE|GET_NODE_IDS);
    scif = get_scif(fake);
    Daemon daemon(scif);
    ASSERT_NO_THROW(daemon.acquire_request());
    ASSERT_GT(daemon.m_request_count, 0);
}

TEST_F(DaemonTest, TC_acquire_request_002)
{
    install_scif_expectations(OPEN|CLOSE|GET_NODE_IDS);
    scif = get_scif(fake);
    Daemon daemon(scif);
    ASSERT_NO_THROW(daemon.acquire_request());
    ASSERT_GT(daemon.m_total_requests, 0);
}

TEST_F(DaemonTest, TC_acquire_request_throw_too_busy_001)
{
    install_scif_expectations(OPEN|CLOSE|GET_NODE_IDS);
    scif = get_scif(fake);
    Daemon daemon(scif);
    daemon.m_request_count = 32;
    ASSERT_THROW(daemon.acquire_request(), SystoolsdException);
}

TEST_F(DaemonTest, TC_release_request_001)
{
    install_scif_expectations(OPEN|CLOSE|GET_NODE_IDS);
    scif = get_scif(fake);
    Daemon daemon(scif);
    ASSERT_NO_THROW(daemon.acquire_request()); // should be 1
    ASSERT_EQ(1, daemon.m_request_count);
    ASSERT_NO_THROW(daemon.release_request()); //should be back to 0
    ASSERT_EQ(0, daemon.m_request_count);
}

TEST_F(DaemonTest, TC_notify_error_001)
{
    //expectations for fixture-provided mock object
    install_scif_expectations(OPEN|CLOSE|GET_NODE_IDS);
    auto peer = get_ep_from_accept();
    FakeScifImpl &impl = *(FakeScifImpl*)(std::static_pointer_cast<ScifEp>(peer)->scif_impl.get());
    set_defaults(impl);
    EXPECT_CALL(impl, send(_, _, _, _))
        .Times(1);
    //expectations for the test-local impl object
    install_scif_expectations(impl, CLOSE);


    SystoolsdReq req = {0, 0, 0, 0, {0}};
    scif = get_scif(fake);
    Daemon daemon(scif);
    ASSERT_NO_THROW(daemon.notify_error(peer, req, SYSTOOLSD_INVAL_STRUCT));
}

TEST_F(DaemonTest, TC_notify_error_throw_by_send_001)
{
    //expectations for fixture-provided mock object
    install_scif_expectations(OPEN|CLOSE|GET_NODE_IDS);
    auto peer = get_ep_from_accept();
    FakeScifImpl &impl = *(FakeScifImpl*)(std::static_pointer_cast<ScifEp>(peer)->scif_impl.get());
    set_defaults(impl);
    //expectations for the test-local impl object
    install_scif_expectations(impl, CLOSE);

    //throw exception from impl::send
    EXPECT_CALL(impl, send(Gt(0), NotNull(), Gt(0), _))
        .WillOnce(Throw(SystoolsdException(SYSTOOLSD_SCIF_ERROR, "mock excp")));
    SystoolsdReq req = {0, 0, 0, 0, {0}};
    scif = get_scif(fake);
    Daemon daemon(scif);
    ASSERT_NO_THROW(daemon.notify_error(peer, req, SYSTOOLSD_INVAL_STRUCT));
}

TEST_F(DaemonTest, TC_flush_client_001)
{
    install_scif_expectations(OPEN|CLOSE|GET_NODE_IDS);
    auto peer = get_ep_from_accept();
    FakeScifImpl &impl = *(FakeScifImpl*)(std::static_pointer_cast<ScifEp>(peer)->scif_impl.get());
    install_scif_expectations(impl, CLOSE);
    EXPECT_CALL(impl, recv(Gt(0), NotNull(), Gt(0), _))
        .WillOnce(Return(0));
    scif = get_scif(fake);
    Daemon daemon(scif);
    daemon.flush_client(peer);
    auto id = peer->get_port_id();
    //client must not have been closed, making port_id = 0:0
    ASSERT_NE(0, id.first);
    ASSERT_NE(0, id.second);
}

TEST_F(DaemonTest, TC_flush_client_expect_close_001)
{
    install_scif_expectations(OPEN|CLOSE|GET_NODE_IDS);
    auto peer = get_ep_from_accept();
    FakeScifImpl &impl = *(FakeScifImpl*)(std::static_pointer_cast<ScifEp>(peer)->scif_impl.get());
    EXPECT_CALL(impl, close(Gt(0)))
        .Times(AtLeast(1));
    EXPECT_CALL(impl, recv(Gt(0), NotNull(), Gt(0), _))
        .WillOnce(Return(123));
    scif = get_scif(fake);
    Daemon daemon(scif);
    daemon.flush_client(peer);
    auto id = peer->get_port_id();
    //client must not have been closed, making port_id = 0:0
    ASSERT_EQ(0, id.first);
    ASSERT_EQ(0, id.second);
}

TEST_F(DaemonTest, TC_add_session_001)
{
    install_scif_expectations(OPEN|CLOSE|GET_NODE_IDS);
    auto peer = get_ep_from_accept();
    scif = get_scif(fake);
    Daemon daemon(scif);
    ASSERT_EQ(0, daemon.m_sessions.size());
    ASSERT_NO_THROW(daemon.add_session(std::make_shared<DaemonSession>(peer)));
    ASSERT_EQ(1, daemon.m_sessions.size());
}
