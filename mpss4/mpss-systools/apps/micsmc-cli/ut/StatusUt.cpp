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

// UT Includes
#include "CommonUt.hpp"
#include "KnxMockUtDevice.hpp"

// Tool Includes
#include "CliApplication.hpp"

// SDK Includes
#include "MicDeviceManager.hpp"
#include "KnlMockDevice.hpp"
#include "micsdkerrno.h"

// C++ Includes
#include <sstream>

using namespace std;
using namespace micmgmt;
using namespace micsmccli;

namespace
{
    typedef MicDeviceManager::DeviceType DT;

    vector<string> sArgs_1 = { "show-settings", "all", "--device=mic2" };
    string sConsole_1  = "*Display Current Settings:*mic2:*ecc*:*abled*led*:*abled*turbo*:*abled*";
    string sConsole_1C = "*Display Current Settings:*mic2:*corec6*:*abled*cpufreq*:*abled*ecc*:*abled*led*:*abled*pc3*:*bled*pc6*:*bled*turbo*:*abled*";

    vector<string> sArgs_2 = { "show-settings", "turbo,led", "--device=mic2" };
    string sConsole_2  = "*Display Current Settings:*mic2:*led*:*abled*turbo*:*abled*";

    vector<string> sArgs_3 = { "show-settings", "turbo,corec6,led", "--device=mic2" };
    string sConsole_3  = "*micsmc-cli: The argument was not validated for the given subcommand:*argument='turbo,corec6,led'*\
subcommand='show-settings'*Subcommand:*show-settings <setting-read-list>*<setting-read-list>:*'all'*";
    string sConsole_3C = "*Display Current Settings:*mic2:*corec6*:*abled*led*:*abled*turbo*:*abled*";

    string sConsole_4  = "*One or more of the coprocessor(s) are not online: mic2*";

    vector<string> sArgs_5 = { "show-settings", "all", "--device=mic1000" };
    string sConsole_5  = "*Error: No such device:*mic1000*";

    vector<string> sArgs_6 = { "show-settings", "all", "--device=mic4" };
    string sConsole_6  = "*Error: No such device:*mic4*";

    vector<string> sArgs_7 = { "show-settings", "turbo,ecc", "--device=mic1" };
    string sConsole_7  = "*Display Current Settings:*mic1:*ecc*:*abled*turbo*:*abled*";

    vector<string> sArgs_8 = { "show-settings", "turbo,ecc,led" };
    string sConsole_8 = "*Display Current Settings:*mic0:*ecc*: Failed:*Device I/O error*led*: Failed:*Device I/O error*turbo*: Failed:*Device I/O error*";
}; // empty namespace

namespace micsmccli
{
    // KNL
    TEST(micsmccli, TC_KNL_mpsstools_micsmccli_knl_status_001) // positive; all
    {
        stringstream console;
        auto app = createApplicationInstance(DT::eDeviceTypeKnl, 4, console, sArgs_1);
        auto devices = createDeviceImplVector(4, DT::eDeviceTypeKnl, true, false);
        EXPECT_EQ(0, app->run(0, devices)) << console.str();
        EXPECT_TRUE(compareWithMask(console.str(), sConsole_1)) << console.str();
    }

    TEST(micsmccli, TC_KNL_mpsstools_micsmccli_knl_status_002) // positive; turbo,led
    {
        stringstream console;
        auto app = createApplicationInstance(DT::eDeviceTypeKnl, 4, console, sArgs_2);
        auto devices = createDeviceImplVector(4, DT::eDeviceTypeKnl, true, false);
        EXPECT_EQ(0, app->run(0, devices)) << console.str();
        EXPECT_TRUE(compareWithMask(console.str(), sConsole_2)) << console.str();
    }

    TEST(micsmccli, TC_KNL_mpsstools_micsmccli_knl_status_003) // negative; turbo,led,-corec6
    {
        stringstream console;
        auto app = createApplicationInstance(DT::eDeviceTypeKnl, 4, console, sArgs_3);
        auto devices = createDeviceImplVector(4, DT::eDeviceTypeKnl, true, false);
        EXPECT_NE(0, app->run(0, devices)) << console.str();
        EXPECT_TRUE(compareWithMask(console.str(), sConsole_3)) << console.str();
    }

    TEST(micsmccli, TC_KNL_mpsstools_micsmccli_knl_status_004) // negative; not online
    {
        stringstream console;
        auto app = createApplicationInstance(DT::eDeviceTypeKnl, 4, console, sArgs_2);
        auto devices = createDeviceImplVector(4, DT::eDeviceTypeKnl, false, false);
        EXPECT_NE(0, app->run(0, devices)) << console.str();
        EXPECT_TRUE(compareWithMask(console.str(), sConsole_4)) << console.str();
    }

    TEST(micsmccli, TC_KNL_mpsstools_micsmccli_knl_status_005) // negative; mic# to large
    {
        stringstream console;
        auto app = createApplicationInstance(DT::eDeviceTypeKnl, 4, console, sArgs_5);
        auto devices = createDeviceImplVector(4, DT::eDeviceTypeKnl, true, false);
        EXPECT_NE(0, app->run(0, devices)) << console.str();
        EXPECT_TRUE(compareWithMask(console.str(), sConsole_5)) << console.str();
    }

    TEST(micsmccli, TC_KNL_mpsstools_micsmccli_knl_status_006) // negative; mic# greater than coprocessor count
    {
        stringstream console;
        auto app = createApplicationInstance(DT::eDeviceTypeKnl, 4, console, sArgs_6);
        auto devices = createDeviceImplVector(4, DT::eDeviceTypeKnl, true, false);
        EXPECT_NE(0, app->run(0, devices)) << console.str();
        EXPECT_TRUE(compareWithMask(console.str(), sConsole_6)) << console.str();
    }

    TEST(micsmccli, TC_KNL_mpsstools_micsmccli_knl_status_007) // positive; turbo,ecc
    {
        stringstream console;
        auto app = createApplicationInstance(DT::eDeviceTypeKnl, 4, console, sArgs_7);
        auto devices = createDeviceImplVector(4, DT::eDeviceTypeKnl, true, false);
        EXPECT_EQ(0, app->run(0, devices)) << console.str();
        EXPECT_TRUE(compareWithMask(console.str(), sConsole_7)) << console.str();
    }

    TEST(micsmccli, TC_KNL_mpsstools_micsmccli_knl_status_008) // negative; all
    {
        stringstream console;
        auto app = createApplicationInstance(DT::eDeviceTypeKnl, 1, console, sArgs_8);
        auto devices = createDeviceImplVector(1, DT::eDeviceTypeKnl, true, false);
        for (auto it = devices.begin(); it != devices.end(); ++it)
        {
            dynamic_cast<KnxMockUtDevice*>(*it)->fakeErrorGetECC_ = MICSDKERR_DEVICE_IO_ERROR;
            dynamic_cast<KnxMockUtDevice*>(*it)->fakeErrorGetLED_ = MICSDKERR_DEVICE_IO_ERROR;
            dynamic_cast<KnxMockUtDevice*>(*it)->fakeErrorGetTurbo_ = MICSDKERR_DEVICE_IO_ERROR;
        }
        EXPECT_NE(0, app->run(0, devices)) << console.str();
        EXPECT_TRUE(compareWithMask(console.str(), sConsole_8)) << console.str();
    }

}; // namespace micsmccli
