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

#include "CommonUt.hpp"
#include "MicLogger.hpp"
#include "KnlUnitTestDevice.hpp"
#include "MicDeviceFactory.hpp"
#include "micsdkerrno.h"

using namespace std;
using namespace micmgmt;

namespace
{
    uint32_t gSimpleError = 0;

    vector<string> sKnlDeviceVersionCL_1 = { "device-version" }; // Normal
    vector<string> sKnlDeviceVersionCL_2 = { "device-version", "--interleave" }; // Normal (no change)
    vector<string> sKnlDeviceVersionCL_6 = { "device-version", "--log=noop" }; // error
    vector<string> sKnlDeviceVersionCL_7 = { "device-version", "--logfile=test.log" }; // logfile created; default level.
    vector<string> sKnlDeviceVersionCL_8 = { "device-version", "--log=info", "--logfile=test.log" }; // logfile created; info level
    vector<string> sKnlDeviceVersionCL_9 = { "device-version", "--device=" }; // error; empty list
    vector<string> sKnlDeviceVersionCL_10 = { "device-version", "--device=all" }; // error; empty list
    vector<string> sKnlDeviceVersionCL_17 = { "device-version", "--file=some_filename" }; // Bad CL
    vector<string> sKnlDeviceVersionCL_18 = { "device-version", "-v" }; // Normal

    string sOutputTemplate_1 = "*\
mic0:*BIOS Version*:*ME Version*:*SMC Version*:*NTB EEPROM Version*:*\
mic1:*BIOS Version*:*ME Version*:*SMC Version*:*NTB EEPROM Version*:*\
mic2:*BIOS Version*:*ME Version*:*SMC Version*:*NTB EEPROM Version*:*";

    string sOutputTemplate_3 = "";

    string sOutputTemplate_6 = "Error: *: *: 'noop'\n*Option:\n*--log*level:\n*";

    string sOutputTemplate_9 = "Error:*:*'device'\n*Option:\n*--device*device-list:\n*";

    string sOutputTemplate_10 = "*\
mic0:*BIOS Version*:*ME Version*:*SMC Version*:*NTB EEPROM Version*:*\
mic1:*BIOS Version*:*ME Version*:*SMC Version*:*NTB EEPROM Version*:*\
mic2:*BIOS Version*:*ME Version*:*SMC Version*:*NTB EEPROM Version*:*\
mic3:*BIOS Version*:*ME Version*:*SMC Version*:*NTB EEPROM Version*:*\
mic4:*BIOS Version*:*ME Version*:*SMC Version*:*NTB EEPROM Version*:*\
mic5:*BIOS Version*:*ME Version*:*SMC Version*:*NTB EEPROM Version*:*\
mic6:*BIOS Version*:*ME Version*:*SMC Version*:*NTB EEPROM Version*:*\
mic7:*BIOS Version*:*ME Version*:*SMC Version*:*NTB EEPROM Version*:*";

    string sOutputTemplate_11 = "*\
mic0:*BIOS Version*:*ME Version*:*SMC Version*:*NTB EEPROM Version*:*\
mic2:*BIOS Version*:*ME Version*:*SMC Version*:*NTB EEPROM Version*:*\
mic3:*BIOS Version*:*ME Version*:*SMC Version*:*NTB EEPROM Version*:*\
mic4:*BIOS Version*:*ME Version*:*SMC Version*:*NTB EEPROM Version*:*\
mic7:*BIOS Version*:*ME Version*:*SMC Version*:*NTB EEPROM Version*:*";

    string sOutputTemplate_12 = "Error: Coprocessor failed to open: mic*\n";

    string sOutputTemplate_15 = "Error: Device is in use by another application and is busy: mic*\n";

    string sOutputTemplate_16 = "Error: Unknown error: mic*\n\n";
    string sOutputTemplate_17 = "Error: The command/option combination is illegal:*'device*-*version'*and*'--file'*";

    MicDevice* createCardsOffline(MicDeviceFactory& factory, const string& cardName, int /*type*/)
    {
        size_t pos = micmgmt::deviceName(0).find_first_of("0123456789");
        int cardNum = std::atoi(cardName.substr(pos).c_str());
        KnlUnitTestDevice* knl = new KnlUnitTestDevice(cardNum);
        MicDevice* device = factory.createDevice(knl);
        knl->setInitialDeviceState(MicDevice::DeviceState::eReady);
        return device;
    }

    void setFabAVersion(MicDevice* device)
    {
        KnlUnitTestDevice* mock = dynamic_cast<KnlUnitTestDevice*>(device->injector());
        if (mock != NULL)
        {
            mock->setFabVersion("0");
        }
    }

    void setFabBVersion(MicDevice* device)
    {
        KnlUnitTestDevice* mock = dynamic_cast<KnlUnitTestDevice*>(device->injector());
        if (mock != NULL)
        {
            mock->setFabVersion("1");
        }
    }

    void testSimpleErrorCallback(MicDevice* device)
    {
        KnlUnitTestDevice* mock = dynamic_cast<KnlUnitTestDevice*>(device->injector());
        if (mock != NULL)
        {
            mock->scenarioGetDevicePlatformInfoFails(gSimpleError);
        }
    }
}; // empty namespace

namespace micfw
{
    TEST(micfw, TC_KNL_mpsstools_micfw_knl_device_version_001)
    {
        auto& app = Utilities::knlTestSetup(sKnlDeviceVersionCL_1);

        EXPECT_NO_THROW(app.first->run(createCardsOffline));

        EXPECT_TRUE(Utilities::compareWithMask(Utilities::stream().str(), sOutputTemplate_1)) << Utilities::stream().str();

        Utilities::knlTestTearDown();
    }

    TEST(micfw, TC_KNL_mpsstools_micfw_knl_device_version_002)
    {
        auto& app = Utilities::knlTestSetup(sKnlDeviceVersionCL_2);

        EXPECT_NO_THROW(app.first->run(createCardsOffline));

        EXPECT_TRUE(Utilities::compareWithMask(Utilities::stream().str(), sOutputTemplate_1)) << Utilities::stream().str();

        Utilities::knlTestTearDown();
    }

    TEST(micfw, TC_KNL_mpsstools_micfw_knl_device_version_009)
    {
        auto& app = Utilities::knlTestSetup(sKnlDeviceVersionCL_9);

        EXPECT_NE(0, app.first->run(createCardsOffline));

        EXPECT_TRUE(Utilities::compareWithMask(Utilities::stream().str(), sOutputTemplate_9)) << Utilities::stream().str();

        Utilities::knlTestTearDown();
    }

    TEST(micfw, TC_KNL_mpsstools_micfw_knl_device_version_010)
    {
        auto& app = Utilities::knlTestSetup(sKnlDeviceVersionCL_10, 8);

        EXPECT_EQ(0, app.first->run(createCardsOffline));

        EXPECT_TRUE(Utilities::compareWithMask(Utilities::stream().str(), sOutputTemplate_10)) << Utilities::stream().str();

        Utilities::knlTestTearDown();
    }

    /* Negative Test Cases for MicDevice::GetPlatformInfo()
    MICSDKERR_DEVICE_NOT_OPEN,          Device not open
    MICSDKERR_DEVICE_IO_ERROR,          Device I/O error
    MICSDKERR_INTERNAL_ERROR,           Internal error
    MICSDKERR_DEVICE_BUSY               Device busy
    */
    TEST(micfw, TC_KNL_mpsstools_micfw_knl_device_version_012)
    {
        auto& app = Utilities::knlTestSetup(sKnlDeviceVersionCL_1);

        gSimpleError = MICSDKERR_DEVICE_NOT_OPEN;
        EXPECT_EQ((int)eMicFwDeviceOpenFailed, app.first->run(createCardsOffline, testSimpleErrorCallback));

        EXPECT_TRUE(Utilities::compareWithMask(Utilities::stream().str(), sOutputTemplate_12)) << Utilities::stream().str();

        Utilities::knlTestTearDown();
    }

    TEST(micfw, TC_KNL_mpsstools_micfw_knl_device_version_013)
    {
        auto& app = Utilities::knlTestSetup(sKnlDeviceVersionCL_1);

        gSimpleError = MICSDKERR_DEVICE_IO_ERROR;
        EXPECT_EQ((int)eMicFwDeviceOpenFailed, app.first->run(createCardsOffline, testSimpleErrorCallback));

        EXPECT_TRUE(Utilities::compareWithMask(Utilities::stream().str(), sOutputTemplate_12)) << Utilities::stream().str();

        Utilities::knlTestTearDown();
    }

    TEST(micfw, TC_KNL_mpsstools_micfw_knl_device_version_014)
    {
        auto& app = Utilities::knlTestSetup(sKnlDeviceVersionCL_1);

        gSimpleError = MICSDKERR_INTERNAL_ERROR;
        EXPECT_EQ((int)eMicFwDeviceOpenFailed, app.first->run(createCardsOffline, testSimpleErrorCallback));

        EXPECT_TRUE(Utilities::compareWithMask(Utilities::stream().str(), sOutputTemplate_12)) << Utilities::stream().str();

        Utilities::knlTestTearDown();
    }

    TEST(micfw, TC_KNL_mpsstools_micfw_knl_device_version_015)
    {
        auto& app = Utilities::knlTestSetup(sKnlDeviceVersionCL_1);

        gSimpleError = MICSDKERR_DEVICE_BUSY;
        EXPECT_EQ((int)eMicFwDeviceBusy, app.first->run(createCardsOffline, testSimpleErrorCallback));

        EXPECT_TRUE(Utilities::compareWithMask(Utilities::stream().str(), sOutputTemplate_15)) << Utilities::stream().str();

        Utilities::knlTestTearDown();
    }

    TEST(micfw, TC_KNL_mpsstools_micfw_knl_device_version_016)
    {
        auto& app = Utilities::knlTestSetup(sKnlDeviceVersionCL_1);

        gSimpleError = MICSDKERR_PROPERTY_NOT_FOUND;
        EXPECT_EQ((int)eMicFwUnknownError, app.first->run(createCardsOffline, testSimpleErrorCallback));

        EXPECT_TRUE(Utilities::compareWithMask(Utilities::stream().str(), sOutputTemplate_16)) << Utilities::stream().str();

        Utilities::knlTestTearDown();
    }

    TEST(micfw, TC_KNL_mpsstools_micfw_knl_device_version_017)
    {
        auto& app = Utilities::knlTestSetup(sKnlDeviceVersionCL_17);

        gSimpleError = MICSDKERR_PROPERTY_NOT_FOUND;
        EXPECT_NE(0, app.first->run(createCardsOffline, testSimpleErrorCallback));

        EXPECT_TRUE(Utilities::compareWithMask(Utilities::stream().str(), sOutputTemplate_17)) << Utilities::stream().str();

        Utilities::knlTestTearDown();
    }

    TEST(micfw, TC_KNL_mpsstools_micfw_knl_device_version_018)
    {
        auto& app = Utilities::knlTestSetup(sKnlDeviceVersionCL_1);

        EXPECT_NO_THROW(app.first->run(createCardsOffline, setFabAVersion));

        EXPECT_TRUE(Utilities::compareWithMask(Utilities::stream().str(), sOutputTemplate_1)) << Utilities::stream().str();

        Utilities::knlTestTearDown();
    }

    TEST(micfw, TC_KNL_mpsstools_micfw_knl_device_version_019)
    {
        auto& app = Utilities::knlTestSetup(sKnlDeviceVersionCL_1);

        EXPECT_NO_THROW(app.first->run(createCardsOffline, setFabBVersion));

        EXPECT_TRUE(Utilities::compareWithMask(Utilities::stream().str(), sOutputTemplate_1)) << Utilities::stream().str();

        Utilities::knlTestTearDown();
    }

}; // namespace micfw
