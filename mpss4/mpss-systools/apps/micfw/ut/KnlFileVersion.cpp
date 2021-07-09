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
#include "micsdkerrno.h"

using namespace std;
using namespace micmgmt;

namespace
{
    vector<string> NoFileName = { "file-version" };
    vector<string> Normal = { "file-version", "PLACEHOLDER" };
    vector<string> NormalVerbose = { "file-version", "PLACEHOLDER", "-v" };
    vector<string> NormalSilent = { "file-version", "PLACEHOLDER", "-s" };
    vector<string> InvalidCommandFile = { "file-version", "PLACEHOLDER", "--file=KNL_FLASH_A0_V_1.2.3.4.hddimg" };
    vector<string> InvalidCommandDevice = { "file-version", "PLACEHOLDER", "--device=mic0" };

    string GoodImage = "*.hddimg:\n*BIOS*:*\n*ME*:*\n*SMC Fab A*:*\n*SMC Fab B*:*";
    string LongNameIn = "USING_A_VERY_LONG_NAME_FOR_A_FILE_IMAGE_IN_ORDER_TO_TEST_LONG_FILE_NAME_WITHOUT_A_CRASH.hddimg";
    string LongNameOut = "*:\n*BIOS*:*\n*ME*:*\n*SMC Fab A*:*\n*SMC Fab B*:*";
    string NoBios = "*.hddimg:\n*ME*:*\n*SMC Fab A*:*\n*SMC Fab B*:*";
    string NoME = "*.hddimg:\n*BIOS*:*\n*SMC Fab A*:*\n*SMC Fab B*:*";
    string NoSMC = "*.hddimg:\n*BIOS*:*\n*ME*:*";
    string BadImage = "Error: Invalid firmware image: Bad Firmware Image = '*.hddimg'";
    string InvalidFile = "*Error: Invalid firmware image: Firmware Image =*";
    string IlegalCommandCombination = "Error: The command/option combination is illegal:*";
    string ParseError = "*Parser error: There are not enough positional arguments on the\ncommand line for the subcommand 'file-version'*";
    string EmptyImage = "*Error: Invalid firmware image: Image is empty";
} // empty namespace

namespace micfw
{
    void fillPlaceHolders(vector<string> & vec, string newFile = "")
    {
        if (vec.size() > 1)
        {
            if(newFile.empty())
                vec[1] = micfw::Utilities::flashFolder() + filePathSeparator();
            else if(vec[1] == "PLACEHOLDER")
                vec[1] = micfw::Utilities::flashFolder() + filePathSeparator() + newFile;
        }
    }

    struct FileVersionTestData
    {
        vector<string> parameters;
        string imageName;
        string outputTemplate;
        FileVersionTestData(vector<string> parameters_, string imageName_, string outputTemplate_)
        : parameters(parameters_), imageName(imageName_), outputTemplate(outputTemplate_){}
    };

    void fileVersionTest(FileVersionTestData data)
    {
        vector<string> & parameters = data.parameters;
        fillPlaceHolders(parameters, data.imageName);
        auto& app = Utilities::knlTestSetup(parameters);
        EXPECT_NO_THROW(app.first->run());
        EXPECT_TRUE(Utilities::compareWithMask(Utilities::stream().str(), data.outputTemplate)) << Utilities::stream().str();
        Utilities::knlTestTearDown();
    }
    TEST(micfw, TC_KNL_File_Version_001)
    {
        fileVersionTest(FileVersionTestData(Normal, "GOLD_IMAGE.hddimg", GoodImage));
    }
    TEST(micfw, TC_KNL_File_Version_002)
    {
        fileVersionTest(FileVersionTestData(NormalVerbose, "GOLD_IMAGE.hddimg", GoodImage));
    }
    TEST(micfw, TC_KNL_File_Version_003)
    {
        fileVersionTest(FileVersionTestData(NormalSilent, "GOLD_IMAGE.hddimg", ""));
    }
    TEST(micfw, TC_KNL_File_Version_004)
    {
        fileVersionTest(FileVersionTestData(Normal, "TABLE_SWAP.hddimg", GoodImage));
    }
    TEST(micfw, TC_KNL_File_Version_005)
    {
        fileVersionTest(FileVersionTestData(Normal, "TABLE_ERROR1.hddimg", BadImage));
    }
    TEST(micfw, TC_KNL_File_Version_006)
    {
        fileVersionTest(FileVersionTestData(Normal, "TABLE_ERROR2.hddimg", BadImage));
    }
    TEST(micfw, TC_KNL_File_Version_007)
    {
        fileVersionTest(FileVersionTestData(Normal, "TABLE_ERROR3.hddimg", BadImage));
    }
    TEST(micfw, TC_KNL_File_Version_008)
    {
        fileVersionTest(FileVersionTestData(Normal, "TABLE_ERROR4.hddimg", BadImage));
    }
    TEST(micfw, TC_KNL_File_Version_009)
    {
        fileVersionTest(FileVersionTestData(Normal, "TABLE_ERROR5.hddimg", BadImage));
    }
    TEST(micfw, TC_KNL_File_Version_010)
    {
        fileVersionTest(FileVersionTestData(Normal, "TABLE_ERROR6.hddimg", BadImage));
    }
    TEST(micfw, TC_KNL_File_Version_011)
    {
        fileVersionTest(FileVersionTestData(Normal, "IFLASH_SHA_ERROR.hddimg", BadImage));
    }
    TEST(micfw, TC_KNL_File_Version_012)
    {
        fileVersionTest(FileVersionTestData(Normal, "BIOS_SHA_ERROR.hddimg", BadImage));
    }
    TEST(micfw, TC_KNL_File_Version_013)
    {
        fileVersionTest(FileVersionTestData(Normal, "SMC_SHA_ERROR.hddimg", BadImage));
    }
    TEST(micfw, TC_KNL_File_Version_014)
    {
        fileVersionTest(FileVersionTestData(Normal, "ME_SHA_ERROR.hddimg", BadImage));
    }
    TEST(micfw, TC_KNL_File_Version_015)
    {
        fileVersionTest(FileVersionTestData(Normal, "NSH_SHA_ERROR.hddimg", BadImage));
    }
    TEST(micfw, TC_KNL_File_Version_016)
    {
        fileVersionTest(FileVersionTestData(Normal, "IPMI_SHA_ERROR.hddimg", BadImage));
    }
    TEST(micfw, TC_KNL_File_Version_017)
    {
        fileVersionTest(FileVersionTestData(Normal, "TEMP_SHA_ERROR.hddimg", BadImage));
    }
    TEST(micfw, TC_KNL_File_Version_018)
    {
        fileVersionTest(FileVersionTestData(Normal, "CAPSULE_ERROR.hddimg", BadImage));
    }
    TEST(micfw, TC_KNL_File_Version_019)
    {
        fileVersionTest(FileVersionTestData(Normal, "VERSION_ERROR.hddimg", BadImage));
    }
    TEST(micfw, TC_KNL_File_Version_020)
    {
        fileVersionTest(FileVersionTestData(Normal, "CAPSULE_EMPTY.hddimg", BadImage));
    }
    TEST(micfw, TC_KNL_File_Version_021)
    {
        fileVersionTest(FileVersionTestData(Normal, "EMPTY_ERROR.hddimg", EmptyImage));
    }
    TEST(micfw, TC_KNL_File_Version_022)
    {
        fileVersionTest(FileVersionTestData(Normal, "EXTRA_FILE.hddimg", GoodImage));
    }
    TEST(micfw, TC_KNL_File_Version_023)
    {
        fileVersionTest(FileVersionTestData(Normal, LongNameIn, LongNameOut));
    }
    TEST(micfw, TC_KNL_File_Version_024)
    {
        fileVersionTest(FileVersionTestData(Normal, "NO_BIOS_CAPSULE.hddimg", NoBios));
    }
    TEST(micfw, TC_KNL_File_Version_025)
    {
        fileVersionTest(FileVersionTestData(Normal, "NO_SMC_CAPSULE.hddimg", NoSMC));
    }
    TEST(micfw, TC_KNL_File_Version_026)
    {
        fileVersionTest(FileVersionTestData(Normal, "NO_ME_CAPSULE.hddimg", NoME));
    }
    TEST(micfw, TC_KNL_File_Version_027)
    {
        fileVersionTest(FileVersionTestData(InvalidCommandFile, "GOLD_IMAGE.hddimg", IlegalCommandCombination));
    }
    TEST(micfw, TC_KNL_File_Version_028)
    {
        fileVersionTest(FileVersionTestData(InvalidCommandDevice, "GOLD_IMAGE.hddimg", IlegalCommandCombination));
    }
    TEST(micfw, TC_KNL_File_Version_029)
    {
        fileVersionTest(FileVersionTestData(Normal, "", InvalidFile));
    }
    TEST(micfw, TC_KNL_File_Version_030)
    {
        fileVersionTest(FileVersionTestData(Normal, "BAD_FILE_NAME.hddimg", InvalidFile));
    }
    TEST(micfw, TC_KNL_File_Version_031)
    {
        fileVersionTest(FileVersionTestData(NoFileName, "", ParseError));
    }
} // namespace micfw
