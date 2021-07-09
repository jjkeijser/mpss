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

    vector<string> sArgs_1 = { "enable", "all", "mic2" };
    string sConsole_1 = "*Enable Setting Results:*mic2:*led*: Successful*turbo*: Successful*";
    string sConsole_1C = "*Enable Setting Results:*mic2:*corec6*: Successful*cpufreq*: Successful*led*: Successful*pc3*: Successful*pc6*: Successful*turbo*: Successful*";

    vector<string> sArgs_2 = { "enable", "turbo,led", "mic2" };
    vector<string> sArgs_2C = { "enable", "ecc,turbo,led,cpufreq,corec6,pc3,pc6", "mic2" };
    string sConsole_2C = "*micsmc-cli: The argument was not validated for the*subcommand:*argument='ecc,turbo,led,cpufreq,corec6,pc3,pc6',*subcommand='enable'*Subcommand:*enable*NOTE:*";

    vector<string> sArgs_3 = { "disable", "ecc", "all" };
    string sConsole_3 = "*Disable Setting Results:*mic0:*ecc*: Successful*mic1:*ecc*: Successful*mic2:*ecc*: Successful*mic3:*ecc*: Successful*";
    string sConsole_4 = "*Disable Setting Results:*mic0:*ecc*: Failed:*Device not online*mic1:*ecc*: Failed:*Device not online*mic2:*ecc*: Failed:*Device not online*mic3:*ecc*: Failed:*Device not online*";
    string sConsole_4C = "*Disable Setting Results:*mic0:*ecc*: Failed:*Device not ready*mic1:*ecc*: Failed:*Device not ready*mic2:*ecc*: Failed:*Device not ready*mic3:*ecc*: Failed:*Device not ready*";

    vector<string> sArgs_5 = { "enable", "turbo", "mic0-mic3" };
    string sConsole_5 = "*Enable Setting Results:*mic0:*turbo*: Successful*mic1:*turbo*: Successful*mic2:*turbo*: Successful*mic3:*turbo*: Successful*";
    string sConsole_7 = "*Enable Setting Results:*mic0:*turbo*: Failed:*Device not online*mic1:*turbo*: Failed:*Device not online*mic2:*turbo*: Failed:*Device not online*mic3:*turbo*: Failed:*Device not online*";

    vector<string> sArgs_6 = { "disable", "led", "mic1,mic2" };
    string sConsole_6 = "*Disable Setting Results:*mic1:*led*: Successful*mic2:*led*: Successful*";
    string sConsole_8 = "*Disable Setting Results:*mic1:*led*: Failed:*Device not online*mic2:*led*: Failed:*Device not online*";

    vector<string> sArgs_9C = { "disable", "cpufreq", "all" };
    string sConsole_9C = "*Disable Setting Results:*mic0:*cpufreq*: Successful*mic1:*cpufreq*: Successful*mic2:*cpufreq*: Successful*mic3:*cpufreq*: Successful*";
    string sConsole_13C = "*Disable Setting Results:*mic0:*cpufreq*: Failed:*Device not online*mic1:*cpufreq*: Failed:*Device not online*mic2:*cpufreq*: Failed:*Device not online*mic3:*cpufreq*: Failed:*Device not online*";

    vector<string> sArgs_10C = { "disable", "corec6", "all" };
    string sConsole_10C = "*Disable Setting Results:*mic0:*corec6*: Successful*mic1:*corec6*: Successful*mic2:*corec6*: Successful*mic3:*corec6*: Successful*";
    string sConsole_14C = "*Disable Setting Results:*mic0:*corec6*: Failed:*Device not online*mic1:*corec6*: Failed:*Device not online*mic2:*corec6*: Failed:*Device not online*mic3:*corec6*: Failed:*Device not online*";

    vector<string> sArgs_11C = { "disable", "pc3", "all" };
    string sConsole_11C = "*Disable Setting Results:*mic0:*pc3*: Successful*mic1:*pc3*: Successful*mic2:*pc3*: Successful*mic3:*pc3*: Successful*";
    string sConsole_15C = "*Disable Setting Results:*mic0:*pc3*: Failed:*Device not online*mic1:*pc3*: Failed:*Device not online*mic2:*pc3*: Failed:*Device not online*mic3:*pc3*: Failed:*Device not online*";

    vector<string> sArgs_12C = { "disable", "pc6", "all" };
    string sConsole_12C = "*Disable Setting Results:*mic0:*pc6*: Successful*mic1:*pc6*: Successful*mic2:*pc6*: Successful*mic3:*pc6*: Successful*";
    string sConsole_16C = "*Disable Setting Results:*mic0:*pc6*: Failed:*Device not online*mic1:*pc6*: Failed:*Device not online*mic2:*pc6*: Failed:*Device not online*mic3:*pc6*: Failed:*Device not online*";

    vector<string> sArgs_9 = { "enable", "turbo", "all" };
    string sConsole_9  = "*Enable Setting Results:*mic0:*turbo*: Failed:*Device busy*";
    string sConsole_10 = "*Enable Setting Results:*mic0:*turbo*: Failed:*Device I/O error*";
}; // empty namespace

namespace micsmccli
{
    // KNL
    TEST(micsmccli, TC_KNL_mpsstools_micsmccli_knl_change_001) // positive; all
    {
        stringstream console;
        auto app = createApplicationInstance(DT::eDeviceTypeKnl, 4, console, sArgs_1);
        app->utFakeAdministratorMode_ = true;
        auto devices = createDeviceImplVector(4, DT::eDeviceTypeKnl, true, true);
        EXPECT_EQ(0, app->run(0, devices)) << console.str();
        EXPECT_TRUE(compareWithMask(console.str(), sConsole_1)) << console.str();
    }

    TEST(micsmccli, TC_KNL_mpsstools_micsmccli_knl_change_002) // positive; all listed out
    {
        stringstream console;
        auto app = createApplicationInstance(DT::eDeviceTypeKnl, 4, console, sArgs_2);
        app->utFakeAdministratorMode_ = true;
        auto devices = createDeviceImplVector(4, DT::eDeviceTypeKnl, true, true);
        EXPECT_EQ(0, app->run(0, devices)) << console.str();
        EXPECT_TRUE(compareWithMask(console.str(), sConsole_1)) << console.str();
    }

#if 0 // Removed due to lack or ECC write in KNL...
    TEST(micsmccli, TC_KNL_mpsstools_micsmccli_knl_change_003) // positive; ecc
    {
        stringstream console;
        auto app = createApplicationInstance(DT::eDeviceTypeKnl, 4, console, sArgs_3);
        app->utFakeAdministratorMode_ = true;
        auto devices = createDeviceImplVector(4, DT::eDeviceTypeKnl, true, true);
        EXPECT_EQ(0, app->run(0, devices)) << console.str();
        EXPECT_TRUE(compareWithMask(console.str(), sConsole_3)) << console.str();
    }

    TEST(micsmccli, TC_KNL_mpsstools_micsmccli_knl_change_004) // negative; ecc ready state
    {
        stringstream console;
        auto app = createApplicationInstance(DT::eDeviceTypeKnl, 4, console, sArgs_3);
        app->utFakeAdministratorMode_ = true;
        auto devices = createDeviceImplVector(4, DT::eDeviceTypeKnl, false, true);
        EXPECT_EQ(0, app->run(0, devices)) << console.str();
        EXPECT_TRUE(compareWithMask(console.str(), sConsole_4)) << console.str();
    }
#endif // 0

    TEST(micsmccli, TC_KNL_mpsstools_micsmccli_knl_change_005) // positive; turbo
    {
        stringstream console;
        auto app = createApplicationInstance(DT::eDeviceTypeKnl, 4, console, sArgs_5);
        app->utFakeAdministratorMode_ = true;
        auto devices = createDeviceImplVector(4, DT::eDeviceTypeKnl, true, true);
        EXPECT_EQ(0, app->run(0, devices)) << console.str();
        EXPECT_TRUE(compareWithMask(console.str(), sConsole_5)) << console.str();
    }

    TEST(micsmccli, TC_KNL_mpsstools_micsmccli_knl_change_006) // positive; led
    {
        stringstream console;
        auto app = createApplicationInstance(DT::eDeviceTypeKnl, 4, console, sArgs_6);
        app->utFakeAdministratorMode_ = true;
        auto devices = createDeviceImplVector(4, DT::eDeviceTypeKnl, true, true);
        EXPECT_EQ(0, app->run(0, devices)) << console.str();
        EXPECT_TRUE(compareWithMask(console.str(), sConsole_6)) << console.str();
    }

    TEST(micsmccli, TC_KNL_mpsstools_micsmccli_knl_change_007) // negative; turbo
    {
        stringstream console;
        auto app = createApplicationInstance(DT::eDeviceTypeKnl, 4, console, sArgs_5);
        app->utFakeAdministratorMode_ = true;
        auto devices = createDeviceImplVector(4, DT::eDeviceTypeKnl, false, true);
        EXPECT_EQ(0, app->run(0, devices)) << console.str();
        EXPECT_TRUE(compareWithMask(console.str(), sConsole_7)) << console.str();
    }

    TEST(micsmccli, TC_KNL_mpsstools_micsmccli_knl_change_008) // negative; led
    {
        stringstream console;
        auto app = createApplicationInstance(DT::eDeviceTypeKnl, 4, console, sArgs_6);
        app->utFakeAdministratorMode_ = true;
        auto devices = createDeviceImplVector(4, DT::eDeviceTypeKnl, false, true);
        EXPECT_EQ(0, app->run(0, devices)) << console.str();
        EXPECT_TRUE(compareWithMask(console.str(), sConsole_8)) << console.str();
    }

    TEST(micsmccli, TC_KNL_mpsstools_micsmccli_knl_change_009) // negative; turbo
    {
        stringstream console;
        auto app = createApplicationInstance(DT::eDeviceTypeKnl, 1, console, sArgs_9);
        app->utFakeAdministratorMode_ = true;
        auto devices = createDeviceImplVector(1, DT::eDeviceTypeKnl, true, true);
        for (auto it = devices.begin(); it != devices.end(); ++it)
        {
            dynamic_cast<KnxMockUtDevice*>(*it)->fakeErrorSetTurbo_ = MICSDKERR_DEVICE_BUSY;
        }
        EXPECT_EQ(0, app->run(0, devices)) << console.str();
        EXPECT_TRUE(compareWithMask(console.str(), sConsole_9)) << console.str();
    }

    TEST(micsmccli, TC_KNL_mpsstools_micsmccli_knl_change_010) // negative; turbo
    {
        stringstream console;
        auto app = createApplicationInstance(DT::eDeviceTypeKnl, 1, console, sArgs_9);
        app->utFakeAdministratorMode_ = true;
        auto devices = createDeviceImplVector(1, DT::eDeviceTypeKnl, true, true);
        for (auto it = devices.begin(); it != devices.end(); ++it)
        {
            dynamic_cast<KnxMockUtDevice*>(*it)->fakeErrorSetTurbo_ = MICSDKERR_DEVICE_IO_ERROR;
        }
        EXPECT_EQ(0, app->run(0, devices)) << console.str();
        EXPECT_TRUE(compareWithMask(console.str(), sConsole_10)) << console.str();
    }

}; // namespace micsmccli
