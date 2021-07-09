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

#include <gtest/gtest.h>
#include "ThreadPool.hpp"
#include "ThreadPoolImpl.hpp"
#include "WorkItemInterface.hpp"
#include "MsTimer.hpp"

// Macro for the correct system sleep() call.
#ifdef _WIN32
#include <Windows.h>
#define msSleep(x) Sleep((DWORD)x)
#else
#include <unistd.h>
#define msSleep(x) usleep((useconds_t)x * (useconds_t)1000)
#endif

namespace // Empty
{
    long long getClockAsMs()
    {
        auto dur = std::chrono::high_resolution_clock::now().time_since_epoch();
        return std::chrono::duration_cast<std::chrono::milliseconds>(dur).count();
    }

    class MyWork : public micmgmt::WorkItemInterface
    {
    public:
        MyWork() {};
        virtual ~MyWork() {};
        virtual void Run(micmgmt::SafeBool& stopSignal)
        { // 0.5 seconds per task plus or minus clock error (higher on Windows).
            micmgmt::MsTimer t(0);
            for (int i = 0; i < 50; ++i)
            {
                if (stopSignal == true)
                {
                    break;
                }

                t.reset(10); // 10ms wait
                t.timeout();
            }
        }
    };
}; // Empty

#define TOLTEST(x) (((long long)x-ACCURACY) < x && x <((long long)x+ACCURACY))

namespace micmgmt
{
    using namespace std;

    TEST(common_framework, TC_KNL_mpsstools_ThreadPool_001)
    {
        const long long ACCURACY = 100; // 1% Tolerance
        shared_ptr<MyWork> task = make_shared<MyWork>();
        ThreadPool pool(10);
        long long start = getClockAsMs();
        for (int i = 0; i < 50; ++i) // Estimated work 3000ms
        {
            pool.addWorkItem(task);
        }
        pool.wait();
        long long diff = getClockAsMs() - start;
        EXPECT_TRUE(TOLTEST(3000)) << "Tolerance exceeded: diff = " << diff;
    }

    TEST(common_framework, TC_KNL_mpsstools_ThreadPool_002)
    {
        const long long ACCURACY = 10; // 1% Tolerance
        shared_ptr<MyWork> task = make_shared<MyWork>();
        ThreadPool pool(100);
        long long start = getClockAsMs();
        for (int i = 0; i < 100; ++i) // Estimated work 1200ms
        {
            pool.addWorkItem(task);
        }
        pool.wait();
        long long diff = getClockAsMs() - start;
        EXPECT_TRUE(TOLTEST(1200)) << "Tolerance exceeded: diff = " << diff;
    }

    TEST(common_framework, TC_KNL_mpsstools_ThreadPool_003)
    {
        const long long ACCURACY = 100; // 1% Tolerance
        shared_ptr<MyWork> task = make_shared<MyWork>();
        ThreadPool pool(10);
        long long start = getClockAsMs();
        for (int i = 0; i < 100; ++i) // Estimated work 12000ms
        {
            pool.addWorkItem(task);
        }
        pool.wait(2000); // Premature timeout
        pool.stopPool(); // Kill pool
        long long diff = getClockAsMs() - start;
        EXPECT_TRUE(TOLTEST(2000)) << "Tolerance exceeded: diff = " << diff;
    }
}; // namespace micmgmt

#undef msSleep
