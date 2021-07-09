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
#include "MsTimer.hpp"
#include <chrono>

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
}

#define TOLTEST(x) (((long long)x-ACCURACY) < x && x <((long long)x+ACCURACY))

namespace micmgmt
{
    TEST(common_framework, TC_KNL_mpsstools_MsTimer_001)
    {
        const long long ACCURACY = 10;

        {
            long long start = getClockAsMs();
            MsTimer tmr(1000);
            tmr.timeout();
            long long diff = tmr.elapsed().count() - (getClockAsMs() - start);
            EXPECT_TRUE(TOLTEST(diff)) << "Tolerance exceeded: diff = " << diff;
        }
        {
            long long start = getClockAsMs();
            MsTimer tmr(100);
            tmr.timeout();
            long long diff = tmr.elapsed().count() - (getClockAsMs() - start);
            EXPECT_TRUE(TOLTEST(diff)) << "Tolerance exceeded: diff = " << diff;
        }
        {
            long long start = getClockAsMs();
            MsTimer tmr(50);
            tmr.timeout();
            long long diff = tmr.elapsed().count() - (getClockAsMs() - start);
            EXPECT_TRUE(TOLTEST(diff)) << "Tolerance exceeded: diff = " << diff;
        }
        {
            long long start = getClockAsMs();
            MsTimer tmr(25);
            tmr.timeout();
            long long diff = tmr.elapsed().count() - (getClockAsMs() - start);
            EXPECT_TRUE(TOLTEST(diff)) << "Tolerance exceeded: diff = " << diff;
        }
    }

    TEST(common_framework, TC_KNL_mpsstools_MsTimer_002)
    {
        const long long ACCURACY = 10;

        MsTimer tmr(1000);
        long long start = getClockAsMs();
        tmr.timeout();
        long long diff = tmr.elapsed().count() - (getClockAsMs() - start);
        EXPECT_TRUE(TOLTEST(diff)) << "Tolerance exceeded: diff = " << diff;

        start = getClockAsMs();
        tmr.reset(100);
        tmr.timeout();
        diff = tmr.elapsed().count() - (getClockAsMs() - start);
        EXPECT_TRUE(TOLTEST(diff)) << "Tolerance exceeded: diff = " << diff;

        start = getClockAsMs();
        tmr.reset(50);
        tmr.timeout();
        diff = tmr.elapsed().count() - (getClockAsMs() - start);
        EXPECT_TRUE(TOLTEST(diff)) << "Tolerance exceeded: diff = " << diff;

        start = getClockAsMs();
        tmr.reset(25);
        tmr.timeout();
        diff = tmr.elapsed().count() - (getClockAsMs() - start);
        EXPECT_TRUE(TOLTEST(diff)) << "Tolerance exceeded: diff = " << diff;
    }

    TEST(common_framework, TC_KNL_mpsstools_MsTimer_003)
    {
        const long long ACCURACY = 25;

        unsigned int testVal = 1000;
        long long start = getClockAsMs();
        MsTimer::sleep((unsigned int)testVal);
        long long diff = getClockAsMs() - start;
        EXPECT_TRUE(TOLTEST(testVal)) << "Tolerance exceeded: diff = " << diff;

        testVal = 100;
        start = getClockAsMs();
        MsTimer::sleep(testVal);
        diff = getClockAsMs() - start;
        EXPECT_TRUE(TOLTEST(testVal)) << "Tolerance exceeded: diff = " << diff;

        testVal = 50;
        start = getClockAsMs();
        MsTimer::sleep(testVal);
        diff = getClockAsMs() - start;
        EXPECT_TRUE(TOLTEST(testVal)) << "Tolerance exceeded: diff = " << diff;
    }
}; // namespace micmgmt

#undef msSleep
#undef TOLTEST
