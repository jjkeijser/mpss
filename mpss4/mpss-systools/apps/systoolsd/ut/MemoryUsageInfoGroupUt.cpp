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
#include <cstring>
#include <thread>
#include <cstdlib>
#include <ctime>

#include <gtest/gtest.h>
#include <info/MemoryUsageInfoGroup.hpp>
#include <systoolsd_api.h>

using std::cout;
using std::endl;
using std::thread;

namespace
{
    const uint64_t timeout = 900; //900ms for MemoryUsageInfoGroup refresh
}

void expect_same_reading(MemoryUsageInfo data1, MemoryUsageInfo data2)
{
    EXPECT_EQ(data1.total, data2.total);
    EXPECT_EQ(data1.used, data2.used);
    EXPECT_EQ(data1.free, data2.free);
    EXPECT_EQ(data1.buffers, data2.buffers);
    EXPECT_EQ(data1.cached, data2.cached);
}

void get_data(MemoryUsageInfoGroup *mem, MemoryUsageInfo *data, bool force_refresh)
{
    *data = mem->get_data(force_refresh);
}

void print_data(MemoryUsageInfo u)
{
    cout << "u.total = " << u.total << endl;
    cout << "u.used = " << u.used << endl;
    cout << "u.free = " << u.free << endl;
    cout << "u.buffers = " << u.buffers << endl;
    cout << "u.cached = " << u.cached << endl;
}

/* TC_getdata_001
 * Create MemoryUsageInfoGroup and get its data
 * Create MemoryUsageInfoGroup and get its data again
 * Expect the cached data to be returned
 */
TEST(MemoryUsageInfoGroupTest, TC_getdata_001)
{
    MemoryUsageInfoGroup memgroup;
    MemoryUsageInfo data1;
    MemoryUsageInfo data2;
    ASSERT_NO_THROW(data1 = memgroup.get_data());
    ASSERT_NO_THROW(data2 = memgroup.get_data());
    expect_same_reading(data1, data2);
}

/* TC_wait_and_getdata_001
 * Create data group
 * Get data and save last_refresh_
 * Sleep so timeout expires
 * Get data again, expect last_refresh to be different than saved one
 */
TEST(MemoryUsageInfoGroupTest, TC_wait_and_getdata_001)
{
    uint64_t last_refresh = -1;
    MemoryUsageInfoGroup memgroup;
    ASSERT_NO_THROW((void)memgroup.get_data());
    last_refresh = memgroup.last_refresh;

    usleep((timeout + 50) * 1000);

    ASSERT_NO_THROW((void)memgroup.get_data());
    EXPECT_NE(last_refresh, memgroup.last_refresh);
}

/* TC_force_refresh_001
 * Create data group
 * Get data and save last_refresh_
 * Get data again, sleep a few ms, but forcing refresh
 * Expect last_refresh to be different than saved one
 */
TEST(MemoryUsageInfoGroupTest, TC_force_refresh_001)
{
    uint64_t last_refresh = -1;
    MemoryUsageInfoGroup memgroup;
    ASSERT_NO_THROW((void)memgroup.get_data());
    last_refresh = memgroup.last_refresh;

    usleep(10 * 1000); //Sleep for 10ms, not enough to exceed timeout

    ASSERT_NO_THROW(memgroup.get_data(true));
    EXPECT_NE(last_refresh, memgroup.last_refresh);
}

TEST(MemoryUsageInfoGroupTest, TC_mthread_001)
{
    MemoryUsageInfo us[2];
    MemoryUsageInfoGroup mem;
    thread ts[2];

    srand(time(NULL));
    for(int i = 0; i < 2; i++)
    {
        ts[i] = thread(get_data, &mem, &us[i], (i==0 ? true : false));
    }

    for(int i = 0; i < 2; i++)
    {
        ts[i].join();
    }

    for(int i = 0; i < 2; i++)
    {
        print_data(us[i]);
    }

}
