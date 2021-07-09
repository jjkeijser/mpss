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

#include <gtest/gtest.h>

#include "CommonUt.hpp"
#include "MicLogger.hpp"
#include "MicOutput.hpp"
#include "MicOutputImpl.hpp"
#include "KnlUnitTestDevice.hpp"
#include "MicDeviceFactory.hpp"
#include "FlashStatus.hpp"
#include "micsdkerrno.h"

#include <cstdio>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#define UNUSED(expr) do { (void)(expr); } while ((void)0,0)

using namespace std;
using namespace micmgmt;

typedef void (*callbackFunctionError)(MicDevice* device);
typedef MicDevice* (*callbackFunctionDevice)(MicDeviceFactory& factory, const string& cardName, int /*type*/);

namespace
{
    vector<string> UpdateAll = { "update", "all" };
    vector<string> UpdateAllFileVerbose = { "update", "all", "--file", "PLACEHOLDER", "-v" };
    vector<string> UpdateAllFileInterleave = { "update", "all", "--file", "PLACEHOLDER", "--interleave" };
    vector<string> UpdateAllFile = { "update", "all", "--file", "PLACEHOLDER" };
    vector<string> UpdateAllSilentVerbose = { "update", "all", "-sv" };
    vector<string> UpdateAllVerbose = { "update", "all", "-v" };
    vector<string> UpdateAllSilent = { "update", "all", "--file", "PLACEHOLDER", "-s" };
    vector<string> UpdateAllInterleave = { "update", "all", "--file", "PLACEHOLDER", "--interleave" };
    vector<string> UpdateSomeOfMany = { "update", "mic0,mic5,mic13,mic23,mic31", "--file", "PLACEHOLDER" };
    vector<string> UpdateOutOfRange = { "update", "mic5", "--file", "PLACEHOLDER" };

    string UpdateOneSucess = "Flashing process with file:*Summary:\n*mic0*Successful\n*";
    string UpdateFourSucess = "Flashing process with file:*Summary:\n*mic0*Successful\n*mic1*:*Successful\n*mic2*:*Successful\n*mic3*:*Successful\n*";
    string UpdateFailOneCard = "*Error: Flashing new firmware failed*Summary:\n*mic0*:*Failed\n*NOTE: One or more KNL devices failed on firmware update*";
    string UpdateFailFourCard = "*Flashing process with file:*Error: Flashing new firmware failed*Summary:\n*mic0*:*Failed\n*mic1*:*Failed\n*mic2*:*Failed\n*mic3*:*Failed\n*NOTE: One or more KNL devices failed on firmware update*";
    string UpdateFailNoReady = "*Error: Devices not in 'Ready' state. Please set to 'Ready' state*";
    string UpdateNoDevice = "*Warning: micfw: There were no driver or coprocessors detected*";
    string UpdateFailEmpty = "*Error: Invalid firmware image: Image is empty";
    string UpdateFailSilentVerbose = "*Error: Cannot use both '--silent' and '--verbose' on the same command line";
    string UpdateInvalidPath = "*Error: Unable to find an appropriate flash image: Firmware Image*";
    string UpdateFailVM = "*Error: Guest OS is not allowed to perform firmware update*";
    string UpdateFailStart = "Flashing process with file:*Failed to start flashing*Summary:\n*mic0*:*Failed\n*mic1*:*Failed\n*mic2*:*Failed\n*mic3*:*Failed\n*NOTE: One or more KNL devices failed on firmware*update*";
    string UpdateManyCards = "Flashing process with file:*Summary:\n*mic0*Successful\n*mic1*:*Successful\n*mic2*:*Successful\n*mic16*:*Successful\n*mic30*:*Successful\n*mic31*:*Successful\n*";
    string UpdateSomeCards = "Flashing process with file:*Summary:\n*mic0*Successful\n*mic5*:*Successful\n*mic13*:*Successful\n*mic23*:*Successful\n*mic31*:*Successful\n*";
    string UpdateFailOutOfRange = "*Error: The devices passed on the command line exceeded the number of devices*";

    string LongNameIn = "USING_A_VERY_LONG_NAME_FOR_A_FILE_IMAGE_IN_ORDER_TO_TEST_LONG_FILE_NAME_WITHOUT_A_CRASH.hddimg";

    void fillPlaceHolders(vector<string> & vec, string newFile = "", bool path = false)
    {
        micfw::Utilities::generateFlashFolderPathAndFiles();
        string sep = filePathSeparator();
        if(vec.size() > 3)
        {
            if(newFile.empty())
                vec[3] = micfw::Utilities::flashFolder() + sep;
            else if(path)
                vec[3] = newFile;
            else
                vec[3] = micfw::Utilities::flashFolder() + sep + newFile;
        }
    }

    void trappedSignalCallback()
    {
    }

    MicDevice* createSpecialKnlMockDevice(MicDeviceFactory& factory, const string& cardName, int /*type*/)
    {
        size_t len = micmgmt::deviceName(0).size() - 1;
        int cardNum = std::atoi(cardName.substr(len).c_str());
        KnlUnitTestDevice* knl = new KnlUnitTestDevice(cardNum);
        MicDevice* device = factory.createDevice(knl);
        knl->setInitialDeviceState(MicDevice::DeviceState::eReady);
        knl->setFSM(0);
        return device;
    }

    MicDevice* createSpecialKnlMockDeviceNoReady(MicDeviceFactory& factory, const string& cardName, int /*type*/)
    {
        size_t len = micmgmt::deviceName(0).size() - 1;
        int cardNum = std::atoi(cardName.substr(len).c_str());
        KnlUnitTestDevice* knl = new KnlUnitTestDevice(cardNum);
        MicDevice* device = factory.createDevice(knl);
        knl->setInitialDeviceState(MicDevice::DeviceState::eOnline);
        knl->setFSM(0);
        return device;
    }

    void startFlashFailure_1(MicDevice* device)
    {
        KnlUnitTestDevice* mock = dynamic_cast<KnlUnitTestDevice*>(device->injector());
        if (mock != NULL)
        {
            mock->scenarioStartFlashFails(MICSDKERR_NO_ACCESS); // Not root.
        }
    }

    void startFlashFailure_2(MicDevice* device)
    {
        KnlUnitTestDevice* mock = dynamic_cast<KnlUnitTestDevice*>(device->injector());
        if (mock != NULL)
        {
            mock->scenarioStartFlashFails(MICSDKERR_DEVICE_BUSY); // device busy.
        }
    }

    void startFlashFailure_3(MicDevice* device)
    {
        KnlUnitTestDevice* mock = dynamic_cast<KnlUnitTestDevice*>(device->injector());
        if (mock != NULL)
        {
            mock->scenarioStartFlashFails(MICSDKERR_DEVICE_IO_ERROR); // Something when wrong in the driver I/O.
        }
    }

    void startFlashFailure_4(MicDevice* device)
    {
        KnlUnitTestDevice* mock = dynamic_cast<KnlUnitTestDevice*>(device->injector());
        if (mock != NULL)
        {
            mock->scenarioStartFlashFails(MICSDKERR_INTERNAL_ERROR); // Unknown reason.
        }
    }

    void startFlashFailureAmazonFeatureLock(MicDevice* device)
    {
        KnlUnitTestDevice* mock = dynamic_cast<KnlUnitTestDevice*>(device->injector());
        if (mock != NULL)
        {
            mock->setFSM(400);
        }
    }

    void startFlashFailureNoBios(MicDevice* device)
    {
        KnlUnitTestDevice* mock = dynamic_cast<KnlUnitTestDevice*>(device->injector());
        if (mock != NULL)
        {
            mock->setFSM(600);
        }
    }

    void startFlashFailureNoSMC(MicDevice* device)
    {
        KnlUnitTestDevice* mock = dynamic_cast<KnlUnitTestDevice*>(device->injector());
        if (mock != NULL)
        {
            mock->setFSM(700);
        }
    }

    void startFlashFailureNoME(MicDevice* device)
    {
        KnlUnitTestDevice* mock = dynamic_cast<KnlUnitTestDevice*>(device->injector());
        if (mock != NULL)
        {
            mock->setFSM(800);
        }
    }

    void startFlashFailureBios(MicDevice* device)
    {
        KnlUnitTestDevice* mock = dynamic_cast<KnlUnitTestDevice*>(device->injector());
        if (mock != NULL)
        {
            mock->setFSM(500);
        }
    }

    void startFlashFailureIflashError(MicDevice* device)
    {
        KnlUnitTestDevice* mock = dynamic_cast<KnlUnitTestDevice*>(device->injector());
        if (mock != NULL)
        {
            mock->setFSM(900);
        }
    }

    void startFlashSuccess(MicDevice* device)
    {
        // No error
        UNUSED(device);
    }

} // empty namespace

namespace micfw
{
    struct KnlUpdateTestData
    {
        vector<string> parameters;
        int cardsNumber;
        string imageName;
        callbackFunctionDevice device;
        callbackFunctionError errFunc;
        string outputTemplate;
        bool emptyFolder;

        KnlUpdateTestData(vector<string> parameters_, int cardsNumber_, string imageName_, callbackFunctionDevice device_,
            callbackFunctionError errFunc_, string outputTemplate_)
            : parameters(parameters_), cardsNumber(cardsNumber_), imageName(imageName_), device(device_), errFunc(errFunc_),
            outputTemplate(outputTemplate_), emptyFolder(false){}
        KnlUpdateTestData(vector<string> parameters_, int cardsNumber_, string imageName_, callbackFunctionDevice device_,
            callbackFunctionError errFunc_, string outputTemplate_, bool emptyFolder_)
            : parameters(parameters_), cardsNumber(cardsNumber_), imageName(imageName_), device(device_), errFunc(errFunc_),
            outputTemplate(outputTemplate_), emptyFolder(emptyFolder_){}
    };

    struct KnlUpdateOpenFileTestData
    {
        int fileA;
        int fileB;
        int fileC;
        int fileD;
        bool expectedValue;

        KnlUpdateOpenFileTestData(int fileA_, int fileB_, int fileC_, int fileD_, bool expectedValue_)
        : fileA(fileA_), fileB(fileB_), fileC(fileC_), fileD(fileD_), expectedValue(expectedValue_) {}
    };

    void updateTest(KnlUpdateTestData data)
    {
        vector<string> & parameters = data.parameters;
        fillPlaceHolders(parameters, data.imageName);
        auto& app = Utilities::knlTestSetup(parameters, data.cardsNumber, data.emptyFolder);
        EXPECT_NO_THROW(app.first->setTrapSignalHandle(trappedSignalCallback));
        EXPECT_NO_THROW(app.first->run(data.device, data.errFunc));
        EXPECT_TRUE(Utilities::compareWithMask(Utilities::stream().str(), data.outputTemplate)) << Utilities::stream().str();
        Utilities::knlTestTearDown();
    }
    void openFileTest(KnlUpdateOpenFileTestData data)
    {
        fillPlaceHolders(UpdateAllFile, "GOLD_IMAGE.hddimg");
        std::vector<std::string> filesToOpen = {
            UpdateAllFile[3],
            micfw::Utilities::flashFolder() + filePathSeparator() + "TEST_A",
            micfw::Utilities::flashFolder() + filePathSeparator() + "TEST_B",
            micfw::Utilities::flashFolder() + filePathSeparator() + "TEST_C",
            micfw::Utilities::flashFolder() + filePathSeparator() + "TEST_D",
            micfw::Utilities::flashFolder() + filePathSeparator() };
        auto& app = Utilities::knlTestSetup(UpdateAllFile, 4);
        if (data.expectedValue)
            EXPECT_TRUE(app.first->testImageCreator(filesToOpen[0],
                        filesToOpen[data.fileA], filesToOpen[data.fileB],
                        filesToOpen[data.fileC], filesToOpen[data.fileD])) << Utilities::stream().str();
        else
            EXPECT_FALSE(app.first->testImageCreator(filesToOpen[0], filesToOpen[data.fileA], filesToOpen[data.fileB],
            filesToOpen[data.fileC], filesToOpen[data.fileD])) << Utilities::stream().str();
        Utilities::knlTestTearDown(filesToOpen[1], filesToOpen[2], filesToOpen[3], filesToOpen[4]);
    }
    TEST(micfw, TC_KNL_Update_Test_001)
    {
        updateTest(KnlUpdateTestData(UpdateAllFile, 1, "GOLD_IMAGE.hddimg", createSpecialKnlMockDevice, startFlashSuccess, UpdateOneSucess));
    }
    TEST(micfw, TC_KNL_Update_Test_002)
    {
        updateTest(KnlUpdateTestData(UpdateAllFile, 4, "GOLD_IMAGE.hddimg", createSpecialKnlMockDevice, startFlashSuccess, UpdateFourSucess));
    }
    TEST(micfw, TC_KNL_Update_Test_003)
    {
        updateTest(KnlUpdateTestData(UpdateAllFileVerbose, 4, "GOLD_IMAGE.hddimg", createSpecialKnlMockDevice, startFlashSuccess, UpdateFourSucess));
    }
    TEST(micfw, TC_KNL_Update_Test_004)
    {
        updateTest(KnlUpdateTestData(UpdateAllInterleave, 4, "GOLD_IMAGE.hddimg", createSpecialKnlMockDevice, startFlashSuccess, UpdateFourSucess));
    }
    TEST(micfw, TC_KNL_Update_Test_005)
    {
        updateTest(KnlUpdateTestData(UpdateAllFile, 4, "GOLD_IMAGE.hddimg", createSpecialKnlMockDevice, startFlashSuccess, UpdateFourSucess));
    }
    TEST(micfw, TC_KNL_Update_Test_006)
    {
        updateTest(KnlUpdateTestData(UpdateOutOfRange, 4, "GOLD_IMAGE.hddimg", createSpecialKnlMockDevice, startFlashSuccess, UpdateFailOutOfRange));
    }
    TEST(micfw, TC_KNL_Update_Test_007)
    {
        updateTest(KnlUpdateTestData(UpdateAllFileVerbose, 1, "GOLD_IMAGE.hddimg", createSpecialKnlMockDevice, startFlashFailureBios, UpdateFailOneCard));
    }
    TEST(micfw, TC_KNL_Update_Test_008)
    {
        updateTest(KnlUpdateTestData(UpdateAllFile, 4, "GOLD_IMAGE.hddimg", createSpecialKnlMockDevice, startFlashFailureIflashError, UpdateFailFourCard));
    }
    TEST(micfw, TC_KNL_Update_Test_009)
    {
        updateTest(KnlUpdateTestData(UpdateAllFile, 1, "GOLD_IMAGE.hddimg", createSpecialKnlMockDeviceNoReady, startFlashSuccess, UpdateFailNoReady));
    }
    TEST(micfw, TC_KNL_Update_Test_010)
    {
        updateTest(KnlUpdateTestData(UpdateAllFile, 0, "GOLD_IMAGE.hddimg", createSpecialKnlMockDevice, startFlashSuccess, UpdateNoDevice));
    }
    TEST(micfw, TC_KNL_Update_Test_011)
    {
        updateTest(KnlUpdateTestData(UpdateAllFile, 4, "EMPTY_ERROR.hddimg", createSpecialKnlMockDevice, startFlashSuccess, UpdateFailEmpty));
    }
    TEST(micfw, TC_KNL_Update_Test_012)
    {
        updateTest(KnlUpdateTestData(UpdateAllFile, 4, "NO_BIOS_CAPSULE.hddimg", createSpecialKnlMockDevice, startFlashFailureNoBios, UpdateFourSucess));
    }
    TEST(micfw, TC_KNL_Update_Test_013)
    {
        updateTest(KnlUpdateTestData(UpdateAllFile, 4, "NO_SMC_CAPSULE.hddimg", createSpecialKnlMockDevice, startFlashFailureNoSMC, UpdateFourSucess));
    }
    TEST(micfw, TC_KNL_Update_Test_014)
    {
        updateTest(KnlUpdateTestData(UpdateAllFile, 4, "NO_ME_CAPSULE.hddimg", createSpecialKnlMockDevice, startFlashFailureNoME, UpdateFourSucess));
    }
    TEST(micfw, TC_KNL_Update_Test_015)
    {
        updateTest(KnlUpdateTestData(UpdateAllSilentVerbose, 4, "", createSpecialKnlMockDevice, startFlashSuccess, UpdateFailSilentVerbose));
    }
    TEST(micfw, TC_KNL_Update_Test_016)
    {
        updateTest(KnlUpdateTestData(UpdateAll, 4, "", createSpecialKnlMockDevice, startFlashSuccess, UpdateInvalidPath, true));
    }
    TEST(micfw, TC_KNL_Update_Test_017)
    {
        updateTest(KnlUpdateTestData(UpdateAllFile, 4, LongNameIn, createSpecialKnlMockDevice, startFlashSuccess, UpdateFourSucess));
    }
    TEST(micfw, TC_KNL_Update_Test_018)
    {
        updateTest(KnlUpdateTestData(UpdateAllFile, 1, "GOLD_IMAGE.hddimg", createSpecialKnlMockDevice, startFlashFailureAmazonFeatureLock, UpdateFailVM));
    }
    TEST(micfw, TC_KNL_Update_Test_019)
    {
        updateTest(KnlUpdateTestData(UpdateAllFile, 4, "SOME_FILE.hddimg", createSpecialKnlMockDevice, startFlashSuccess, UpdateInvalidPath));
    }
    TEST(micfw, TC_KNL_Update_Test_020)
    {
        updateTest(KnlUpdateTestData(UpdateAllSilent, 4, "GOLD_IMAGE.hddimg", createSpecialKnlMockDevice, startFlashSuccess, ""));
    }
    TEST(micfw, TC_KNL_Update_Test_021)
    {
        updateTest(KnlUpdateTestData(UpdateAllFile, 4, "GOLD_IMAGE.hddimg", createSpecialKnlMockDevice, startFlashFailure_1, UpdateFailStart));
    }
    TEST(micfw, TC_KNL_Update_Test_022)
    {
        updateTest(KnlUpdateTestData(UpdateAllFile, 4, "GOLD_IMAGE.hddimg", createSpecialKnlMockDevice, startFlashFailure_2, UpdateFailStart));
    }
    TEST(micfw, TC_KNL_Update_Test_023)
    {
        updateTest(KnlUpdateTestData(UpdateAllFile, 4, "GOLD_IMAGE.hddimg", createSpecialKnlMockDevice, startFlashFailure_3, UpdateFailStart));
    }
    TEST(micfw, TC_KNL_Update_Test_024)
    {
        updateTest(KnlUpdateTestData(UpdateAllFile, 4, "GOLD_IMAGE.hddimg", createSpecialKnlMockDevice, startFlashFailure_4, UpdateFailStart));
    }
    TEST(micfw, TC_KNL_Update_Test_025)
    {
        updateTest(KnlUpdateTestData(UpdateAllFile, 32, "GOLD_IMAGE.hddimg", createSpecialKnlMockDevice, startFlashSuccess, UpdateManyCards));
    }
    TEST(micfw, TC_KNL_Update_Test_026)
    {
        updateTest(KnlUpdateTestData(UpdateAllFile, 32, "GOLD_IMAGE.hddimg", createSpecialKnlMockDevice, startFlashSuccess, UpdateManyCards));
    }
    TEST(micfw, TC_KNL_Update_Test_027)
    {
        updateTest(KnlUpdateTestData(UpdateSomeOfMany, 32, "GOLD_IMAGE.hddimg", createSpecialKnlMockDevice, startFlashSuccess, UpdateSomeCards));
    }
    TEST(micfw, TC_KNL_Update_Open_File_Test_001)
    {
        openFileTest(KnlUpdateOpenFileTestData(1, 2, 3, 4, false));
    }
    TEST(micfw, TC_KNL_Update_Open_File_Test_002)
    {
        openFileTest(KnlUpdateOpenFileTestData(0, 2, 3, 4, true));
    }
    TEST(micfw, TC_KNL_Update_Open_File_Test_003)
    {
        openFileTest(KnlUpdateOpenFileTestData(1, 0, 3, 4, true));
    }
    TEST(micfw, TC_KNL_Update_Open_File_Test_004)
    {
        openFileTest(KnlUpdateOpenFileTestData(1, 2, 0, 4, true));
    }
    TEST(micfw, TC_KNL_Update_Open_File_Test_005)
    {
        openFileTest(KnlUpdateOpenFileTestData(4, 2, 3, 0, true));
    }
    TEST(micfw, TC_KNL_Update_Open_File_Test_006)
    {
        openFileTest(KnlUpdateOpenFileTestData(1, 4, 3, 0, true));
    }
    TEST(micfw, TC_KNL_Update_Open_File_Test_007)
    {
        openFileTest(KnlUpdateOpenFileTestData(1, 2, 4, 0, true));
    }
} // namespace micfw