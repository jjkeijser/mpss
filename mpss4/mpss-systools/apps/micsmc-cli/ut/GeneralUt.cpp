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

// CF Includes
#include "MicLogger.hpp"

// C++ Includes
#include <sstream>

using namespace std;
using namespace micmgmt;
using namespace micsmccli;

namespace
{
    typedef MicDeviceManager::DeviceType DT;

    const vector<string> sEmptyArgs = {};
    const vector<MicDeviceImpl*> sEmptydeviceList = {};

    const string sConsole_1 = "*There are no coprocessors detected, please run this*tool*on*a*system*with*coprocessors*present*and coprocessor software loaded and running*";
    const string sConsole_2 = "*The coprocessor driver is not currently loaded*";
    const string sConsole_3 = "*The coprocessor software is not installed on the*current*system*";

    const vector<string> sArgs_4 = {"show-data"};
    const string sConsole_4 = "*mic2: Failed to open device*";

    const vector<string> sArgs_5 = {"show-data", "--cores"};

//    const vector<string> sArgs_6 = { "show-data", "--online", "--silent" };
    const string sConsole_6 = "";

//    const vector<string> sArgs_7 = { "show-data", "--online", "--xml" };

//    const vector<string> sArgs_8 = { "show-data", "--online", "--xml=test.xml" };
    const string sConsole_9 = "*Failed to open the XML output file: file already exists: test.xml*";

#ifdef _WIN32
//    const vector<string> sArgs_10 = { "show-data", "--online", "--xml=C:\\mywindows\\system32\\test.xml" };
    const string sConsole_10 = "*Failed to open the XML output file:*could*not*open*file:*C:\\mywindows\\system32\\test.xml*";
#else
//    const vector<string> sArgs_10 = { "show-data", "--online", "--xml=/smybin/test.xml" };
    const string sConsole_10 = "*Failed to open the XML output file:*could*not*open*file:*/smybin/test.xml*";
#endif

    const vector<string> sArgs_11 = { "show-data", "--online", "--log=debug" };

    const vector<string> sArgs_12 = { "show-data", "--online", "--log=debug", "--logfile=tmp.log" };

    const vector<string> sArgs_13 = { "enable", "ecc", "mic0", "--timeout=1" };
    const string sConsole_13 = "*Enable Setting Results:*mic0:*ecc*: Failed: Operation timed out*";
}; // empty namepsace

namespace micsmccli
{
    TEST(micsmccli, TC_KNL_mpsstools_micsmccli_knl_general_001) // negative; No devices
    {
        stringstream console;
        auto app = createApplicationInstance(DT::eDeviceTypeKnl, 0, console, sEmptyArgs);
        EXPECT_NE(0, app->run(0, sEmptydeviceList)) << console.str();
        EXPECT_TRUE(compareWithMask(console.str(), sConsole_1)) << console.str();
        clearTestEnvironment();
    }

    TEST(micsmccli, TC_KNL_mpsstools_micsmccli_knl_general_002) // negative; No driver
    {
        stringstream console;
        auto app = createApplicationInstance(DT::eDeviceTypeKnl, 0, console, sEmptyArgs);
        app->utFailDriver_ = true;
        EXPECT_NE(0, app->run(0, sEmptydeviceList)) << console.str();
        EXPECT_TRUE(compareWithMask(console.str(), sConsole_2)) << console.str();
        clearTestEnvironment();
    }

    TEST(micsmccli, TC_KNL_mpsstools_micsmccli_knl_general_003) // negative; No MPSS
    {
        stringstream console;
        auto app = createApplicationInstance(DT::eDeviceTypeKnl, 0, console, sEmptyArgs);
        app->utFailMPSS_ = true;
        EXPECT_NE(0, app->run(0, sEmptydeviceList)) << console.str();
        EXPECT_TRUE(compareWithMask(console.str(), sConsole_3)) << console.str();
        clearTestEnvironment();
    }

    TEST(micsmccli, TC_KNL_mpsstools_micsmccli_knl_general_004) // negative; Device Factory Failed on mic2
    {
        stringstream console;
        auto app = createApplicationInstance(DT::eDeviceTypeKnl, 4, console, sArgs_4);
        auto devices = createDeviceImplVector(4, DT::eDeviceTypeKnl, true, false);
        KnxMockUtDevice* mic2 = dynamic_cast<KnxMockUtDevice*>(devices[2]);
        mic2->deviceOpenError_ = MICSDKERR_DEVICE_OPEN_FAILED;
        EXPECT_NE(0, app->run(0, devices)) << console.str();
        EXPECT_TRUE(compareWithMask(console.str(), sConsole_4)) << console.str();
        clearTestEnvironment();
    }

    TEST(micsmccli, TC_KNL_mpsstools_micsmccli_knl_general_005) // positive; display_data:cores used as default action
    {
        stringstream console;
        auto app = createApplicationInstance(DT::eDeviceTypeKnl, 4, console, sArgs_5);
        auto devices = createDeviceImplVector(4, DT::eDeviceTypeKnl, true, false);
        EXPECT_EQ(0, app->run(0, devices)) << console.str();
        clearTestEnvironment();
    }

}; // namespace micsmccli
