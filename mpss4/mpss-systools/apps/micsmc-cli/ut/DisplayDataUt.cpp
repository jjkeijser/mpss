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

    vector<string> sArgs_1 = { "show-data", "--info"};
    string sConsole_1 = "*mic0:*Coprocessor Information:*Device Series*:*\
Device ID*:*Number Of Cores*:*Coprocessor OS Version*:*BIOS Version*:*Stepping*:*\
Substepping*:*mic1:*";
    string sConsole_1C = "*mic0:*Coprocessor Information:*Device Series*:*\
Device ID*:*Number Of Cores*:*Coprocessor OS Version*:*Flash Version*:*Stepping*:*\
Substepping*:*mic1:*";

    vector<string> sArgs_2 = { "show-data", "--mem"};
    string sConsole_2 = "*mic0:*Memory Usage:*Total Memory*:*Free Memory*:*Used Memory*:*mic1:*";

    vector<string> sArgs_3 = { "show-data", "--cores"};
    string sConsole_3 = "*mic0:*Core Utilization:*Total Utilization:*User*:*%*System*:*%*Idle*:*%*Utilization Per Logical Core:*Core #*:*User*:*%*mic1:*";

    vector<string> sArgs_4 = { "show-data", "--freq"};
    string sConsole_4 = "*mic0:*Frequency and Power Usage:*Core Frequency*:*GHz*PCIe*:*Watts*2x3**:*Watts*2x4*:*Watts*mic1:*";
    string sConsole_4C = "*mic0:*Frequency and Power Usage:*Core Frequency*:*GHz*Secret*:*Watts*mic1:*";

    vector<string> sArgs_5 = { "show-data", "--temp"};
    string sConsole_5 = "*mic0:*Thermal Information:*Die*:*C*Fan Exhaust*:*C*VCCP*:*C*VCCCLR*:*C*VCCMP*:*C*West*:*C*East*:*C*mic1:*";
    string sConsole_5C = "*mic0:*Thermal Information:*Die*:*C*mic1:*";

    vector<string> sArgs_6 = { "show-data", "--online"};
    string sConsole_6 = "*Online Cards*:*mic0,mic1*";
    string sConsole_8 = "*Online Cards*: mic1\n*";

    vector<string> sArgs_7 = { "show-data", "--offline" };
    string sConsole_7 = "*Offline Cards*:*mic0,mic1*";
    string sConsole_9 = "*Offline Cards*: mic1\n*";

    vector<string> sArgs_10 = { "show-data", "--offline", "--online" };
    string sConsole_10 = "*Online Cards*: mic0,mic1\n*Offline Cards*: None\n*";
    string sConsole_11 = "*Online Cards*: mic0\n*Offline Cards*: mic1\n*";

    vector<string> sArgs_14 = { "show-data", "--all" };
    string sConsole_14 = "*mic0:*Core Utilization:*Frequency and Power Usage:*Coprocessor Information:*Memory Usage:*Thermal Information:*mic1:*";

    vector<string> sArgs_15 = { "show-data", "--info", "--cores" , "--mem"};
    string sConsole_15  = "*mic0:*Core Utilization:*Coprocessor Information:*Memory Usage:*mic1:*";

    vector<string> sArgs_16 = { "show-data" };
    string sConsole_17 = "*mic0:*Device I/O error:*mic0*Frequency and Power Usage:*Device I/O error:*mic0*Device I/O error:*mic0*\
Coprocessor Information:*Device Series*:*x200*Device ID*:*0x*Number Of Cores*:*Coprocessor OS Version*:*BIOS Version*:*\
Stepping*:*Substepping*:*0x*Device I/O error:*mic0*Thermal Information:*Device I/O error:*mic0*";
}; // empty namepsace

namespace micsmccli
{
    // KNL
    TEST(micsmccli, TC_KNL_mpsstools_micsmccli_knl_showdata_001) // positive; --info
    {
        stringstream console;
        auto app = createApplicationInstance(DT::eDeviceTypeKnl, 2, console, sArgs_1);
        auto devices = createDeviceImplVector(2, DT::eDeviceTypeKnl, true, false);
        EXPECT_EQ(0, app->run(0, devices)) << console.str();
        EXPECT_TRUE(compareWithMask(console.str(), sConsole_1)) << console.str();
    }

    TEST(micsmccli, TC_KNL_mpsstools_micsmccli_knl_showdata_002) // positive; --mem
    {
        stringstream console;
        auto app = createApplicationInstance(DT::eDeviceTypeKnl, 2, console, sArgs_2);
        auto devices = createDeviceImplVector(2, DT::eDeviceTypeKnl, true, false);
        EXPECT_EQ(0, app->run(0, devices)) << console.str();
        EXPECT_TRUE(compareWithMask(console.str(), sConsole_2)) << console.str();
    }

    TEST(micsmccli, TC_KNL_mpsstools_micsmccli_knl_showdata_003) // positive; --cores
    {
        stringstream console;
        auto app = createApplicationInstance(DT::eDeviceTypeKnl, 2, console, sArgs_3);
        auto devices = createDeviceImplVector(2, DT::eDeviceTypeKnl, true, false);
        EXPECT_EQ(0, app->run(0, devices)) << console.str();
        EXPECT_TRUE(compareWithMask(console.str(), sConsole_3)) << console.str();
    }

    TEST(micsmccli, TC_KNL_mpsstools_micsmccli_knl_showdata_004) // positive; --freq
    {
        stringstream console;
        auto app = createApplicationInstance(DT::eDeviceTypeKnl, 2, console, sArgs_4);
        auto devices = createDeviceImplVector(2, DT::eDeviceTypeKnl, true, false);
        EXPECT_EQ(0, app->run(0, devices)) << console.str();
        EXPECT_TRUE(compareWithMask(console.str(), sConsole_4)) << console.str();
    }

    TEST(micsmccli, TC_KNL_mpsstools_micsmccli_knl_showdata_005) // positive; --temp
    {
        stringstream console;
        auto app = createApplicationInstance(DT::eDeviceTypeKnl, 2, console, sArgs_5);
        auto devices = createDeviceImplVector(2, DT::eDeviceTypeKnl, true, false);
        EXPECT_EQ(0, app->run(0, devices)) << console.str();
        EXPECT_TRUE(compareWithMask(console.str(), sConsole_5)) << console.str();
    }

    TEST(micsmccli, TC_KNL_mpsstools_micsmccli_knl_showdata_006) // positive; --online
    {
        stringstream console;
        auto app = createApplicationInstance(DT::eDeviceTypeKnl, 2, console, sArgs_6);
        auto devices = createDeviceImplVector(2, DT::eDeviceTypeKnl, true, false);
        EXPECT_EQ(0, app->run(0, devices)) << console.str();
        EXPECT_TRUE(compareWithMask(console.str(), sConsole_6)) << console.str();
    }

    TEST(micsmccli, TC_KNL_mpsstools_micsmccli_knl_showdata_007) // positive; --offline
    {
        stringstream console;
        auto app = createApplicationInstance(DT::eDeviceTypeKnl, 2, console, sArgs_7);
        auto devices = createDeviceImplVector(2, DT::eDeviceTypeKnl, false, false);
        EXPECT_EQ(0, app->run(0, devices)) << console.str();
        EXPECT_TRUE(compareWithMask(console.str(), sConsole_7)) << console.str();
    }

    TEST(micsmccli, TC_KNL_mpsstools_micsmccli_knl_showdata_008) // positive; --online (mixed)
    {
        stringstream console;
        auto app = createApplicationInstance(DT::eDeviceTypeKnl, 2, console, sArgs_6);
        auto devices = createDeviceImplVector(2, DT::eDeviceTypeKnl, true, false);
        auto knx0 = dynamic_cast<KnxMockUtDevice*>(devices[0]);
        knx0->initialOnlineMode_ = false;
        EXPECT_EQ(0, app->run(0, devices)) << console.str();
        EXPECT_TRUE(compareWithMask(console.str(), sConsole_8)) << console.str();
    }

    TEST(micsmccli, TC_KNL_mpsstools_micsmccli_knl_showdata_009) // positive; --offline (mixed)
    {
        stringstream console;
        auto app = createApplicationInstance(DT::eDeviceTypeKnl, 2, console, sArgs_7);
        auto devices = createDeviceImplVector(2, DT::eDeviceTypeKnl, false, false);
        auto knx0 = dynamic_cast<KnxMockUtDevice*>(devices[0]);
        knx0->initialOnlineMode_ = true;
        EXPECT_EQ(0, app->run(0, devices)) << console.str();
        EXPECT_TRUE(compareWithMask(console.str(), sConsole_9)) << console.str();
    }

    TEST(micsmccli, TC_KNL_mpsstools_micsmccli_knl_showdata_010) // positive; --offline,--online (online)
    {
        stringstream console;
        auto app = createApplicationInstance(DT::eDeviceTypeKnl, 2, console, sArgs_10);
        auto devices = createDeviceImplVector(2, DT::eDeviceTypeKnl, true, false);
        EXPECT_EQ(0, app->run(0, devices)) << console.str();
        EXPECT_TRUE(compareWithMask(console.str(), sConsole_10)) << console.str();
    }

    TEST(micsmccli, TC_KNL_mpsstools_micsmccli_knl_showdata_011) // positive; --offline,--online (online)
    {
        stringstream console;
        auto app = createApplicationInstance(DT::eDeviceTypeKnl, 2, console, sArgs_10);
        auto devices = createDeviceImplVector(2, DT::eDeviceTypeKnl, true, false);
        auto knx1 = dynamic_cast<KnxMockUtDevice*>(devices[1]);
        knx1->initialOnlineMode_ = false;
        EXPECT_EQ(0, app->run(0, devices)) << console.str();
        EXPECT_TRUE(compareWithMask(console.str(), sConsole_11)) << console.str();
    }

    TEST(micsmccli, TC_KNL_mpsstools_micsmccli_knl_showdata_014) // positive; --all
    {
        stringstream console;
        auto app = createApplicationInstance(DT::eDeviceTypeKnl, 2, console, sArgs_14);
        auto devices = createDeviceImplVector(2, DT::eDeviceTypeKnl, true, false);
        EXPECT_EQ(0, app->run(0, devices)) << console.str();
        EXPECT_TRUE(compareWithMask(console.str(), sConsole_14)) << console.str();
    }

    TEST(micsmccli, TC_KNL_mpsstools_micsmccli_knl_showdata_015) // positive; --mem, --cores, --info
    {
        stringstream console;
        auto app = createApplicationInstance(DT::eDeviceTypeKnl, 2, console, sArgs_15);
        auto devices = createDeviceImplVector(2, DT::eDeviceTypeKnl, true, false);
        EXPECT_EQ(0, app->run(0, devices)) << console.str();
        EXPECT_TRUE(compareWithMask(console.str(), sConsole_15)) << console.str();
        EXPECT_FALSE(compareWithMask(console.str(), sConsole_14)) << console.str();
    }

    TEST(micsmccli, TC_KNL_mpsstools_micsmccli_knl_showdata_016) // positive; <no options>
    {
        stringstream console;
        auto app = createApplicationInstance(DT::eDeviceTypeKnl, 2, console, sArgs_16);
        auto devices = createDeviceImplVector(2, DT::eDeviceTypeKnl, true, false);
        EXPECT_EQ(0, app->run(0, devices)) << console.str();
        EXPECT_TRUE(compareWithMask(console.str(), sConsole_14)) << console.str();
    }

    TEST(micsmccli, TC_KNL_mpsstools_micsmccli_knl_showdata_017) // negative; all
    {
        stringstream console;
        auto app = createApplicationInstance(DT::eDeviceTypeKnl, 1, console, sArgs_16);
        auto devices = createDeviceImplVector(1, DT::eDeviceTypeKnl, true, false);
        for (auto it = devices.begin(); it != devices.end(); ++it)
        {
            dynamic_cast<KnxMockUtDevice*>(*it)->fakeErrorGetThermal_   = MICSDKERR_DEVICE_IO_ERROR;
            dynamic_cast<KnxMockUtDevice*>(*it)->fakeErrorGetPwrUsage_  = MICSDKERR_DEVICE_IO_ERROR;
            dynamic_cast<KnxMockUtDevice*>(*it)->fakeErrorGetMemUsage_  = MICSDKERR_DEVICE_IO_ERROR;
            dynamic_cast<KnxMockUtDevice*>(*it)->fakeErrorGetCoreUsage_ = MICSDKERR_DEVICE_IO_ERROR;
        }
        EXPECT_NE(0, app->run(0, devices)) << console.str();
        EXPECT_TRUE(compareWithMask(console.str(), sConsole_17)) << console.str();
    }

}; // namespace micsmccli
