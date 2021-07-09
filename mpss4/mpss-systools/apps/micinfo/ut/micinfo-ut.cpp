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

#include <string>
#include <vector>
#include <iostream>
#include <tuple>
#include <ctime>
#include <iomanip>
#include <gtest/gtest.h>


#include "CliParser.hpp"
#include "MicException.hpp"
#include "micmgmtCommon.hpp"
#include "MicOutput.hpp"
#include "ConsoleOutputFormatter.hpp"
#include "XmlOutputFormatter.hpp"
#include "MicLogger.hpp"
#include "MicDeviceFactory.hpp"
#include "MicDeviceManager.hpp"
#include "MicDevice.hpp"
#include "MicDeviceInfo.hpp"
#include "MicDeviceDetails.hpp"
#include "MicDeviceError.hpp"
#include "MicCoreInfo.hpp"
#include "MicProcessorInfo.hpp"
#include "MicThermalInfo.hpp"
#include "MicTemperature.hpp"
#include "MicMemory.hpp"
#include "MicMemoryInfo.hpp"
#include "MicPciConfigInfo.hpp"
#include "MicPlatformInfo.hpp"
#include "MicMemoryUsageInfo.hpp"
#include "MicPowerUsageInfo.hpp"
#include "MicVoltageInfo.hpp"
#include "ThrottleInfo.hpp"
#include "PciAddress.hpp"
#include "MicVoltage.hpp"
#include "MicPower.hpp"
#include "MicSpeed.hpp"
#include "MicVersionInfo.hpp"
#include "MicFrequency.hpp"
#include "MicDeviceError.hpp"
#include "utilities.hpp"
#include "ut-utilities.hpp"
#include "KnlMockDevice.hpp"
#include "MicBootConfigInfo.hpp"

using namespace std;
using namespace micmgmt;
using namespace micinfo;

// Positive Test
// Unit Test for one fixed device.
namespace micinfo_unittest
{
const string header = R"(micinfo Utility Log
Created On Wed Jan 02 02:03:55 1980
)";

const std::string basicInfoKnl = "Device No: 0, Device Name: mic0 [x200]";
const string systemInfoKnl = R"(
System Info:
    Host OS                        : Test OS
    OS Version                     : Test OS Version
    MPSS Version                   : 4.1.2.3
    Host Physical Memory           : 1000000 MB
)";
const string versionInfoKnl = R"(
Version:
    SMC Firmware Version           : 3.4.5.6
    Coprocessor OS Version         : Linux
    Device Serial Number           : 00000000001
    BIOS Version                   : 1.2.3.4
    BIOS Build date                : 2014-12-05
    ME Version                     : 6.7.8.9
)";
const string boardInfoKnl = R"(
Board:
    Vendor ID                      : 0x8086
    Device ID                      : 0x2258
    Subsystem ID                   : 0x2500
    Coprocessor Stepping ID        : 0xb0
    UUID                           : Insufficient Privileges
    PCIe Width                     : Insufficient Privileges
    PCIe Speed                     : Insufficient Privileges
    PCIe Ext Tag Field             : Insufficient Privileges
    PCIe No Snoop                  : Insufficient Privileges
    PCIe Relaxed Ordering          : Insufficient Privileges
    PCIe Max payload size          : Insufficient Privileges
    PCIe Max read request size     : Insufficient Privileges
    Coprocessor Model              : 0x01
    Coprocessor Type               : 0x00
    Coprocessor Family             : 0x0b
    Coprocessor Stepping           : B0
    Board SKU                      : B0PO-SKU1
    ECC Mode                       : Disabled
    PCIe Bus Information           : )"
#ifdef __linux__
                                  R"(0000:00:00.0)"
#else
                                  R"(0000:0:0:0)"
#endif
R"(
    Coprocessor SMBus Address      : 0x00000030
    Coprocessor Brand              : Intel
    Coprocessor Board Type         : Board type
    Coprocessor TDP                : 300.00 W
)";
const string coreInfoKnl = R"(
Core:
    Total No. of Active Cores      : 62
    Threads per Core               : 4
    Voltage                        : 997.00 mV
    Frequency                      : 1.00 GHz
)";
const string thermalInfoKnl = R"(
Thermal:
    Thermal Dissipation            : Active
    Fan RPM                        : 2700
    Fan PWM                        : 50 %
    Die Temp                       : 58 C
)";
const string memoryInfoKnl = R"(
Memory:
    Vendor                         : Elpida
    Size                           : 8192.00 MB
    Technology                     : MCDRAM
    Speed                          : 5.50 GT/s
    Frequency                      : 2.75 GHz
)";

void parse(Options& opts, int argc, const char* argv[]){
    CliParser parser(argc, const_cast<char**>(argv), "tool version", "tool year");
    vector<string> groupHelp = {"Group help"};
    vector<string> groupListHelp = {"Group list help"};
    parser.addOption("group", groupHelp, 'g', true, "group_list", groupListHelp, "all");
    parser.parse();
    parseGroups(parser, opts);
}

void assertGroups(int argc, const char* argv[]) {
    Options opts;
    parse(opts, argc, argv);
    for(int i=0; i<GRP_SIZE; i++)
        EXPECT_TRUE(opts.groups[i]);
}

void assertGroups(int argc, const char* argv[], int selected) {
    Options opts;
    parse(opts, argc, argv);
    for(int i=0; i<GRP_SIZE; i++) {
        if(i == selected) continue;
        EXPECT_FALSE(opts.groups[i]);
    }
    EXPECT_TRUE(opts.groups[selected]);
}

TEST(MicInfoUnitTestUtilities, TC_parse_group_001) {
    const char* argv[] = {"micinfo"};
    assertGroups(1, argv);
}

TEST(MicInfoUnitTestUtilities, TC_parse_group_002) {
    const char* argv[] = {"micinfo", "-g", "all"};
    assertGroups(3, argv);
}

TEST(MicInfoUnitTestUtilities, TC_parse_group_003) {
    const char* argv[] = {"micinfo", "-g", "system"};
    assertGroups(3, argv, GRP_SYS);
}

TEST(MicInfoUnitTestUtilities, TC_parse_group_004) {
    const char* argv[] = {"micinfo", "-g", "version"};
    assertGroups(3, argv, GRP_VER);
}

TEST(MicInfoUnitTestUtilities, TC_parse_group_005) {
    const char* argv[] = {"micinfo", "-g", "board"};
    assertGroups(3, argv, GRP_BOARD);
}

TEST(MicInfoUnitTestUtilities, TC_parse_group_006) {
    const char* argv[] = {"micinfo", "-g", "core"};
    assertGroups(3, argv, GRP_CORES);
}

TEST(MicInfoUnitTestUtilities, TC_parse_group_007) {
    const char* argv[] = {"micinfo", "-g", "thermal"};
    assertGroups(3, argv, GRP_THER);
}

TEST(MicInfoUnitTestUtilities, TC_parse_group_008) {
    const char* argv[] = {"micinfo", "-g", "memory"};
    assertGroups(3, argv, GRP_GDDR);
}

TEST(MicInfoUnitTestUtilities, TC_parse_group_009) {
    const char* argv[] = {"micinfo", "-g", "invalid_group"};
    Options opts;
    EXPECT_THROW(parse(opts, 3, argv), MicException);
}

TEST(MicInfoUnitTestUtilities, TC_format_hex_001) {
    EXPECT_EQ(NOT_AVAILABLE, formatHex((uint16_t)0, 0, false));
}

class MicInfoUnitTest : public::testing::Test {

    public:
    unique_ptr<MicDevice> micDevice;
    unique_ptr<MicInfoApp> app;
    unique_ptr<MicDeviceManager> manager;
    stringstream output;

    void SetUp(){
        manager.reset(new MicDeviceManager);
        uint32_t res = manager->initialize();
        EXPECT_EQ( uint32_t(0), res);
        MicDeviceFactory deviceFactory( manager.get() );
        micDevice.reset(deviceFactory.createDevice("mic0"));
        res = micDevice->open();
        EXPECT_EQ( uint32_t(0), res);
        std::unique_ptr<std::ostream> none;
        app.reset( new MicInfoApp(*manager.get(), &output, none, false));
    }
    void TearDown(){
        micDevice->close();
    }
};

class MicInfoUnitTestKNL : public MicInfoUnitTest {

    void SetUp(){
        createKnlMockDevice("1");
        MicInfoUnitTest::SetUp();
    }
};

    TEST_F(MicInfoUnitTestKNL, TC_show_select_info_knl_001)
    {
        app->show_select_info( micDevice, -1 );
        EXPECT_EQ("", output.str());
    }

    TEST_F(MicInfoUnitTestKNL, TC_show_basic_info_knl_001)
    {
        string res = app->basicInfo( micDevice );
        EXPECT_EQ(basicInfoKnl, res);
    }

    TEST_F(MicInfoUnitTestKNL, TC_show_version_info_knl_001)
    {
        app->show_select_info( micDevice, GRP_VER );
        EXPECT_EQ(versionInfoKnl, output.str());
    }

    TEST_F(MicInfoUnitTestKNL, TC_show_memory_info_knl_001)
    {
        app->show_select_info( micDevice, GRP_GDDR );
        EXPECT_EQ(memoryInfoKnl, output.str());
    }

    TEST_F(MicInfoUnitTestKNL, TC_show_board_info_knl_001)
    {
        app->show_select_info( micDevice, GRP_BOARD );
        EXPECT_EQ(boardInfoKnl, output.str());
    }

    TEST_F(MicInfoUnitTestKNL, TC_show_cores_info_knl_001)
    {
        app->show_select_info( micDevice, GRP_CORES );
        EXPECT_EQ(coreInfoKnl, output.str());
    }

    TEST_F(MicInfoUnitTestKNL, TC_show_thermal_info_knl_001)
    {
        app->show_select_info( micDevice, GRP_THER );
        EXPECT_EQ(thermalInfoKnl, output.str());
    }

    TEST_F(MicInfoUnitTestKNL, test_dispatch_001)
    {
        Options opts;
        opts.devices.push_back(0);
        for ( int i = 0; i < GRP_SIZE; i++) {
            opts.groups[i] = true;
        }
        app->dispatch( opts);
        stringstream allInfo;
        allInfo << header << systemInfoKnl << endl << basicInfoKnl << endl << versionInfoKnl
                << boardInfoKnl << coreInfoKnl << thermalInfoKnl << memoryInfoKnl << endl;
        EXPECT_EQ(allInfo.str(), output.str());
    }

    TEST_F(MicInfoUnitTestKNL, test_dispatch_002)
    {
        Options opts;
        opts.devices.push_back(0);
        for ( int i = 0; i < GRP_SIZE; i++) {
            opts.groups[i] = false;
        }
        app->dispatch( opts);
        EXPECT_EQ(header + "\n", output.str());
    }

    TEST_F(MicInfoUnitTestKNL, test_dispatch_003)
    {
        Options opts;
        opts.devices.push_back(0);
        for ( int i = 0; i < GRP_SIZE; i++) {
            opts.groups[i] = true;
        }
        opts.groups[1] = false;
        app->dispatch( opts);
        stringstream allInfo;
        allInfo << header << systemInfoKnl << endl << basicInfoKnl << endl
                << boardInfoKnl << coreInfoKnl << thermalInfoKnl << memoryInfoKnl << endl;
        EXPECT_EQ(allInfo.str(), output.str());
    }

    TEST(MicInfoUnitTest, test_micinfoapp_001)
    {
        MicDeviceManager manager;
        unique_ptr<ostream> sstr(new stringstream);
        MicInfoApp app(manager, NULL, sstr, false);
        EXPECT_TRUE(NULL != app.stdOut.get());
    }
}
