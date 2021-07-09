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

#include <iostream>

#include <gtest/gtest.h>
#include <TestDataGroup.hpp>

using std::cout;
using std::endl;

/* TC_getdata_001
 * Create data group
 * Request data
 * Ensure returned values match the expected ones
 * Request data once more
 * Expect the cached data to be returned
 */
TEST(CachedDataGroupTest, TC_getdata_001)
{
    uint64_t timeout = 300; //300ms
    TestDataGroup data_group(timeout);
    TestStruct test_struct = data_group.get_data();
    EXPECT_EQ(TestDataGroup::val1_initial, test_struct.val1);
    EXPECT_EQ(TestDataGroup::val2_initial, test_struct.val2);

    test_struct = data_group.get_data();
    EXPECT_EQ(TestDataGroup::val1_initial, test_struct.val1);
    EXPECT_EQ(TestDataGroup::val2_initial, test_struct.val2);
}

/* TC_wait_and_getdata_001
 * Create data group
 * Request data
 * Sleep to go over timeout
 * Request data again
 * Expect values to be refreshed
 */
TEST(CachedDataGroupTest, TC_wait_and_getdata_001)
{
    uint64_t timeout = 300;
    uint32_t expected_val1 = TestDataGroup::val1_initial * 2;
    uint32_t expected_val2 = TestDataGroup::val2_initial * 2;

    TestDataGroup data_group(timeout);
    TestStruct test_struct = data_group.get_data();
    EXPECT_EQ(TestDataGroup::val1_initial, test_struct.val1);
    EXPECT_EQ(TestDataGroup::val2_initial, test_struct.val2);

    //Sleep for 350ms to go over timeout
    usleep(350 * 1000);
    //Next call should refresh data
    test_struct = data_group.get_data();
    EXPECT_EQ(expected_val1, test_struct.val1);
    EXPECT_EQ(expected_val2, test_struct.val2);
}

TEST(CachedDataGroupTest, TC_force_refresh_001)
{
    uint64_t timeout = 300;
    uint32_t expected_val1 = TestDataGroup::val1_initial * 2;
    uint32_t expected_val2 = TestDataGroup::val2_initial * 2;

    TestDataGroup data_group(timeout);
    TestStruct test_struct = data_group.get_data();
    EXPECT_EQ(TestDataGroup::val1_initial, test_struct.val1);
    EXPECT_EQ(TestDataGroup::val2_initial, test_struct.val2);

    //Next call should refresh data
    test_struct = data_group.get_data(true);
    EXPECT_EQ(expected_val1, test_struct.val1);
    EXPECT_EQ(expected_val2, test_struct.val2);
}

TEST(CachedDataGroupTest, TC_getsize_001)
{
    TestDataGroup data_group(0);
    ASSERT_EQ(sizeof(TestStruct), data_group.get_size());
}

TEST(CachedDataGroupTest, TC_copydatato_001)
{
    const size_t csize = 100;
    size_t size = csize;
    char buf[size];
    TestDataGroup data_group(0);
    TestStruct golden = data_group.get_data();
    bzero(buf, csize);
    //ensure copy works
    data_group.copy_data_to(buf, &size);
    TestStruct *t = reinterpret_cast<TestStruct*>(buf);
    ASSERT_EQ(golden.val1, t->val1);
    ASSERT_EQ(golden.val2, t->val2);
    //check function set the pointer to the appropriate size
    ASSERT_EQ(sizeof(TestStruct), size);

    //bzero again, copy only half
    size = sizeof(TestStruct) / 2;
    bzero(buf, csize);
    data_group.copy_data_to(buf, &size);
    t = reinterpret_cast<TestStruct*>(buf);
    ASSERT_EQ(golden.val1, t->val1);
    ASSERT_NE(golden.val2, t->val2);
    //must still be zero
    ASSERT_EQ(0, t->val2);
}

TEST(CachedDataGroupTest, TC_copyrawdata_001)
{
    TestDataGroup data_group(0);
    TestStruct golden = data_group.get_data();
    void *data = data_group.get_raw_data();
    TestStruct *t = reinterpret_cast<TestStruct*>(data);
    ASSERT_EQ(golden.val1, t->val1);
    ASSERT_EQ(golden.val2, t->val2);

    //force refresh, resulting data should be different from golden struct
    data = data_group.get_raw_data(true);
    t = reinterpret_cast<TestStruct*>(data);
    ASSERT_NE(golden.val1, t->val1);
    ASSERT_NE(golden.val2, t->val2);
}
