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

#include <stdexcept>
#include <functional>
#include <iostream>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "ScifEp.hpp"
#include "ScifSocket.hpp"
#include "SystoolsdException.hpp"
#include "mocks.hpp"

using ::testing::Assign;
using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::Gt;
using ::testing::IsNull;
using ::testing::Matcher;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::ReturnArg;
using ::testing::SetArgumentPointee;
using ::testing::SetArrayArgument;
using ::testing::_;

namespace
{
    const uint32_t OPEN = 1 << 0;
    const uint32_t CLOSE = 1 << 1;
    const uint32_t GET_NODE_IDS = 1 << 2;

    const int EPD = 7; //descriptor returned by mocked open() calls
    const uint16_t THIS_NODE = 2; //node number set by mocked get_node_ids() calls
    const uint16_t THIS_PORT = 4444; //port number to be set by mocked bind() calls
}

//BEGIN TESTS

//test fixture
class ScifEpTest : public ::testing::Test
{
protected:
    //set up the fake SCIF implementation
    //use "good" functions as defaults
    virtual void SetUp()
    {
        //this will be freed by unique_ptr in ScifEp
        fake = new NiceMock<FakeScifImpl>();
        //make open() return non-zero by default
        ON_CALL(*fake, open())
            .WillByDefault(Return(EPD));
        //make get_node_ids() return 1 by default
        ON_CALL(*fake, get_node_ids(_, _, _))
            .WillByDefault(DoAll(SetArgumentPointee<2>(THIS_NODE),
                           Return(1)));
        ON_CALL(*fake, bind(Gt(0), Gt(0)))
            .WillByDefault(ReturnArg<1>());
        ON_CALL(*fake, close(Gt(0)))
            .WillByDefault(Return(0));
    }

    virtual void install_expectations(uint32_t flags=OPEN|CLOSE|GET_NODE_IDS)
    {
        if(flags & OPEN)
            EXPECT_CALL(*fake, open())
                .Times(1);

        if(flags & CLOSE)
            EXPECT_CALL(*fake, close(Gt(0)))
                .Times(1);

        if(flags & GET_NODE_IDS)
            EXPECT_CALL(*fake, get_node_ids(_, _, _))
                .Times(1);
    }

    FakeScifImpl *fake;
};

TEST_F(ScifEpTest, TC_accept_001)
{
    install_expectations(OPEN|GET_NODE_IDS);
    EXPECT_CALL(*fake, close(Gt(0)))
            .Times(AtLeast(2));
    EXPECT_CALL(*fake, accept(Gt(0), NotNull(), NotNull(),NotNull(),  _))
        .WillOnce(DoAll(SetArgumentPointee<3>(11), Return(0)));
    ScifEp ep(fake);
    ASSERT_NO_THROW(ScifEp::ptr new_ep = ep.accept());
}

TEST_F(ScifEpTest, TC_accept_throw_001)
{
    install_expectations();
    EXPECT_CALL(*fake, accept(Gt(0), NotNull(), NotNull(), NotNull(), _))
        .WillOnce(Return(-1));
    ScifEp ep(fake);
    ASSERT_THROW(ScifEp::ptr new_ep = ep.accept(), SystoolsdException);
}

TEST_F(ScifEpTest, TC_bind_001)
{
    install_expectations();
    EXPECT_CALL(*fake, bind(Gt(0), Gt(0)))
        .Times(1);
    ScifEp ep(fake);
    ASSERT_NO_THROW(ep.bind(THIS_PORT));
    ScifEp::scif_port_id id;
    ASSERT_NO_THROW(id = ep.get_port_id());
    ASSERT_EQ(THIS_PORT, id.second);
}

TEST_F(ScifEpTest, TC_bind_throw_001)
{
    install_expectations();
    EXPECT_CALL(*fake, bind(Gt(0), Gt(0)))
        .WillOnce(Return(-1));
    ScifEp ep(fake);
    ASSERT_THROW(ep.bind(THIS_PORT), SystoolsdException);
}

TEST_F(ScifEpTest, TC_close_001)
{
    install_expectations(OPEN|GET_NODE_IDS);
    EXPECT_CALL(*fake, close(Gt(0)))
        .Times(AtLeast(1))
        .WillRepeatedly(Return(0));
    ScifEp ep(fake);
    ASSERT_NO_THROW(ep.close());
    ScifEp::scif_port_id id = ep.get_port_id();
    ASSERT_EQ(0, id.first);
    ASSERT_EQ(0, id.second);
}

TEST_F(ScifEpTest, TC_init_001)
{
    install_expectations();
    ASSERT_NO_THROW(ScifEp ep(fake));
}

TEST_F(ScifEpTest, TC_init_throw_by_open_001)
{
    //make open() return -1
    EXPECT_CALL(*fake, open())
        .Times(1)
        .WillOnce(Return(-1));
    EXPECT_CALL(*fake, close(_))
        .Times(0);
    ASSERT_THROW(ScifEp ep(fake), SystoolsdException);
}

TEST_F(ScifEpTest, TC_init_throw_by_get_node_ids_001)
{
    install_expectations(OPEN|CLOSE);
    //get_node_ids() must return 1, but set errno
    EXPECT_CALL(*fake, get_node_ids(_, _, _))
        .Times(1)
        .WillOnce(DoAll(Assign(&errno, EBADF), Return(1)));
    ASSERT_THROW(ScifEp ep(fake), SystoolsdException);;
}


TEST_F(ScifEpTest, TC_listen_001)
{
    install_expectations();
    EXPECT_CALL(*fake, listen(Gt(0), Gt(0)))
        .Times(1);
    ScifEp ep(fake);
    ASSERT_NO_THROW(ep.listen(5));
}

TEST_F(ScifEpTest, TC_listen_throw_001)
{
    install_expectations();
    EXPECT_CALL(*fake, listen(Gt(0), Gt(0)))
        .WillOnce(Return(-1));
    ScifEp ep(fake);
    ASSERT_THROW(ep.listen(), SystoolsdException);
}

TEST_F(ScifEpTest, TC_connect_001)
{
    install_expectations();
    EXPECT_CALL(*fake, connect(Gt(0), _, _))
        .WillOnce(Return(THIS_PORT));
    ScifEp ep(fake);
    ASSERT_NO_THROW(ep.connect(0, THIS_PORT));
    ScifEp::scif_port_id id = ep.get_port_id();
    ASSERT_EQ(THIS_PORT, id.second);
}

TEST_F(ScifEpTest, TC_connect_throw_001)
{
    install_expectations();
    EXPECT_CALL(*fake, connect(Gt(0), _, _))
        .WillOnce(Return(-1));
    ScifEp ep(fake);
    ASSERT_THROW(ep.connect(0, THIS_PORT), SystoolsdException);
}

TEST_F(ScifEpTest, TC_recv_001)
{
    const int len = 5;
    char buf[len] = {0};
    install_expectations();
    EXPECT_CALL(*fake, recv(Gt(0), NotNull(), Gt(0), _))
        .WillOnce(ReturnArg<2>());
    ScifEp ep(fake);
    ASSERT_EQ(len, ep.recv(buf, len));
}

TEST_F(ScifEpTest, TC_recv_throw_null_buf_001)
{
    install_expectations();
    //Make sure recv() isn't called and that exception is thrown
    //when receiving NULL buffer
    EXPECT_CALL(*fake, recv(_, _, _, _))
        .Times(0);
    ScifEp ep(fake);
    ASSERT_THROW(ep.recv(NULL, 1, false), SystoolsdException);
}

TEST_F(ScifEpTest, TC_recv_fail_001)
{
    install_expectations();
    EXPECT_CALL(*fake, recv(Gt(0), NotNull(), _, _))
        .WillOnce(Return(-1));
    ScifEp ep(fake);
    char c = 0;
    int len = 1;
    ASSERT_EQ(-1, ep.recv(&c, len));
}

TEST_F(ScifEpTest, TC_send_001)
{
    const int len = 5;
    char buf[len] = {0};
    install_expectations();
    EXPECT_CALL(*fake, send(Gt(0), NotNull(), Gt(0), _))
        .WillOnce(ReturnArg<2>());
    ScifEp ep(fake);
    ASSERT_EQ(len, ep.send(buf, len));
}

TEST_F(ScifEpTest, TC_send_throw_null_buf_001)
{
    install_expectations();
    //Make sure send() isn't called and that exception is thrown
    //when receiving NULL buffer
    EXPECT_CALL(*fake, send(_, _, _, _))
        .Times(0);
    ScifEp ep(fake);
    ASSERT_THROW(ep.send(NULL, 1), SystoolsdException);
}

TEST_F(ScifEpTest, TC_send_fail_001)
{
    install_expectations();
    EXPECT_CALL(*fake, send(Gt(0), NotNull(), _, _))
        .WillOnce(Return(-1));
    ScifEp ep(fake);
    char c = 0;
    int len = 1;
    ASSERT_EQ(-1, ep.send(&c, len));
}

TEST_F(ScifEpTest, TC_get_port_id_001)
{
    install_expectations(OPEN|CLOSE);
    EXPECT_CALL(*fake, get_node_ids(_, _, NotNull()))
        .WillOnce(DoAll(SetArgumentPointee<2>(THIS_NODE), Return(3)));
    EXPECT_CALL(*fake, bind(Gt(0), Gt(0)))
        .Times(1);
    ScifEp ep(fake);
    ep.bind(THIS_PORT);
    ScifEp::scif_port_id id = ep.get_port_id();
    ASSERT_EQ(THIS_NODE, id.first);
    ASSERT_EQ(THIS_PORT, id.second);
}

TEST_F(ScifEpTest, TC_get_epd_001)
{
    install_expectations(CLOSE|GET_NODE_IDS);
    const int expected_fd = 7;
    EXPECT_CALL(*fake, open())
        .WillOnce(Return(expected_fd));
    ScifEp ep(fake);
    ASSERT_EQ(expected_fd, ep.get_epd());
}

TEST_F(ScifEpTest, TC_reset_shutdown_001)
{
    EXPECT_CALL(*fake, open())
        .Times(2);
    EXPECT_CALL(*fake, close(Gt(0)))
        .Times(AtLeast(2));
    EXPECT_CALL(*fake, get_node_ids(_, _, _))
        .Times(AtLeast(2));
    EXPECT_CALL(*fake, bind(_, _))
        .Times(1);

    ScifEp ep(fake);
    ep.bind(THIS_PORT);
    ScifEp::scif_port_id id = ep.get_port_id();
    ASSERT_EQ(THIS_NODE, id.first);
    ASSERT_EQ(THIS_PORT, id.second);
    ASSERT_NO_THROW(ep.reset());
    id = ep.get_port_id();
    ASSERT_EQ(THIS_NODE, id.first);
    ASSERT_EQ(0, id.second);
}

TEST_F(ScifEpTest, TC_select_001)
{
    const uint8_t eps_nr = 10;
    // Have underlying function "select" 4 EPs
    const uint8_t expected_eps_nr = 4;
    std::vector<ScifEp::ptr> eps;
    const std::vector<int> expected_fds = { 2, 3, 6, 9 };
    // Populate vector of ScifEp objects to "be watched"
    for(uint8_t i = 1; i < eps_nr; ++i)
    {
        FakeScifImpl *impl = new NiceMock<FakeScifImpl>; // Will be owned by ScifEp object
        eps.push_back(std::make_shared<ScifEp>(impl));
    }

    EXPECT_CALL(*fake, poll_read(_, -1, NotNull()))
        .WillOnce(Return(expected_fds));

    ScifEp ep(fake);
    std::vector<ScifEp::ptr> returned_eps;
    ASSERT_NO_THROW(returned_eps = ep.select_read(eps, NULL));
    // Ensure only expected_eps_nr is returned
    ASSERT_EQ(expected_eps_nr, returned_eps.size());

    // Now use a timeout
    timeval tv = { 1, 0 }; // 1 sec timeout = 1000 ms timeout
    long timeout = 1000;
    EXPECT_CALL(*fake, poll_read(_, timeout, NotNull()))
        .WillOnce(Return(expected_fds));

    ASSERT_NO_THROW(returned_eps = ep.select_read(eps, &tv));
    ASSERT_EQ(expected_eps_nr, returned_eps.size());
}
