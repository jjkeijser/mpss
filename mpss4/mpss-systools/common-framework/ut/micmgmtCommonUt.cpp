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
#include "micmgmtCommon.hpp"

namespace micmgmt
{
    using namespace std;

    TEST(common_framework, TC_KNL_mpsstools_common_function_trim_001)
    {
        string inStr = "\n\r    \t\f  \v";
        string outStr = "";
        EXPECT_STREQ(outStr.c_str(), trim(inStr).c_str());

        inStr = "\n\r  real string  \t\f  \v";
        outStr = "real string";
        EXPECT_STREQ(outStr.c_str(), trim(inStr).c_str());

        inStr = "";
        outStr = "";
        EXPECT_STREQ(outStr.c_str(), trim(inStr).c_str());

        inStr = "real string 2  \t\f  \v";
        outStr = "real string 2";
        EXPECT_STREQ(outStr.c_str(), trim(inStr).c_str());

        inStr = "\n\r  real string 3";
        outStr = "real string 3";
        EXPECT_STREQ(outStr.c_str(), trim(inStr).c_str());
    } // common_framework.TC_KNL_mpsstools_common_function_Trim_001

    TEST(common_framework, TC_KNL_mpsstools_common_function_normalize_001)
    {
        string inStr = "  This  is a    sentence.  ";
        string outStr = "This is a sentence.";
        EXPECT_STREQ(outStr.c_str(), normalize(inStr).c_str());

        inStr = "  This  is a  \n \t  sentence.  ";
        outStr = "This is a sentence.";
        EXPECT_STREQ(outStr.c_str(), normalize(inStr).c_str());

        inStr = "\nThis  is a  \n \t  sentence.  \r";
        outStr = "This is a sentence.";
        EXPECT_STREQ(outStr.c_str(), normalize(inStr).c_str());

        inStr = "\nThis  is a  \n \t  sentence.  \r";
        outStr = "This is a sentence.";
        EXPECT_STREQ(outStr.c_str(), normalize(inStr).c_str());

        inStr = "";
        outStr = "";
        EXPECT_STREQ(outStr.c_str(), normalize(inStr).c_str());

        inStr = "red";
        outStr = "red";
        EXPECT_STREQ(outStr.c_str(), normalize(inStr).c_str());

        inStr = "red";
        outStr = "red";
        EXPECT_STREQ(outStr.c_str(), normalize(inStr).c_str());
    } // common_framework.TC_KNL_mpsstools_common_function_Normalize_001

    TEST(common_framework, TC_KNL_mpsstools_common_function_copyright_001)
    {
        EXPECT_STREQ("Copyright (C) 2017, Intel Corporation.", copyright(NULL).c_str());
        EXPECT_STREQ("Copyright (C) 2017, Intel Corporation.", copyright("").c_str());
        EXPECT_STREQ("Copyright (C) 2017, Intel Corporation.", copyright("21500").c_str());
        EXPECT_STREQ("Copyright (C) 2017, Intel Corporation.", copyright("214").c_str());

        EXPECT_STREQ("Copyright (C) 2014, Intel Corporation.", copyright("2014").c_str());
        EXPECT_STREQ("Copyright (C) 0000, Intel Corporation.", copyright("0000").c_str());
        EXPECT_STREQ("Copyright (C) 9999, Intel Corporation.", copyright("9999").c_str());

        string tmp;
        tmp = copyrightUtf8("2014");
        EXPECT_STREQ("Copyright \xC2\xA9 2014, Intel Corporation.", tmp.c_str()) << tmp;

        tmp = copyrightUtf8(NULL);
        EXPECT_STREQ("Copyright \xC2\xA9 2017, Intel Corporation.", tmp.c_str()) << tmp;

        tmp = copyrightUtf8("134535");
        EXPECT_STREQ("Copyright \xC2\xA9 2017, Intel Corporation.", tmp.c_str()) << tmp;
    } // common_framework.TC_KNL_mpsstools_common_function_copyright_001

    TEST(common_framework, TC_KNL_mpsstools_common_function_wrap_001)
    {
        string longText = "";
        vector<string> wrappedLines;
        EXPECT_THROW(wrappedLines = wrap(longText, 79), invalid_argument);

        longText = "11     chars   \n \n very      \t         surprised";
        EXPECT_THROW(wrappedLines = wrap(longText, 10), out_of_range);
        EXPECT_NO_THROW(wrappedLines = wrap(longText, 12));
        EXPECT_EQ((vector<string>::size_type)3, wrappedLines.size());
        EXPECT_STREQ("11 chars", wrappedLines[0].c_str());
        EXPECT_STREQ("very", wrappedLines[1].c_str());
        EXPECT_STREQ("surprised", wrappedLines[2].c_str());

        longText = "Even though Google Test has a rich set of assertions, they can never be complete, as it's impossible (nor a good idea) to anticipate all the scenarios a user might run into. Therefore, sometimes a user has to use EXPECT_TRUE() to check a complex expression, for lack of a better macro.";
        EXPECT_NO_THROW(wrappedLines = wrap(longText, 256));
        EXPECT_EQ((vector<string>::size_type)2, wrappedLines.size());
        EXPECT_NO_THROW(wrappedLines = wrap(longText, 79));
        EXPECT_EQ((vector<string>::size_type)4, wrappedLines.size());

        longText = "EventhoughGoogleTesthasarichsetofassertions,theycanneverbecomplete,asit'simpossible(noragoodidea)toanticipateallthescenariosausermightruninto.Therefore,sometimesauserhastouseEXPECT_TRUE()tocheckacomplexexpression,forlackofabettermacro.";
        EXPECT_NO_THROW(wrappedLines = wrap(longText, 79));
        EXPECT_EQ((vector<string>::size_type)3, wrappedLines.size());
        EXPECT_STREQ("EventhoughGoogleTesthasarichsetofassertions,theycanneverbecomplete,asit'simposs", wrappedLines[0].c_str());
        EXPECT_STREQ("ible(noragoodidea)toanticipateallthescenariosausermightruninto.Therefore,someti", wrappedLines[1].c_str());
        EXPECT_STREQ("mesauserhastouseEXPECT_TRUE()tocheckacomplexexpression,forlackofabettermacro.", wrappedLines[2].c_str());
    } // common_framework.TC_KNL_mpsstools_common_function_WrapText_001

    TEST(common_framework, TC_KNL_mpsstools_common_function_wrap_002)
    {
        vector<string> wrappedLines;
        string longText = "This is to test word wrapping capabilities with dashes involved.  The option --word should not be broken up.";
        EXPECT_NO_THROW(wrappedLines = wrap(longText, 79));
        EXPECT_STREQ("This is to test word wrapping capabilities with dashes involved. The option", wrappedLines[0].c_str());

        longText = "This tests word wrapping capabilities with dashes involved. The verbage dead-word should be broken up after the dash.";
        EXPECT_NO_THROW(wrappedLines = wrap(longText, 79));
        EXPECT_STREQ("This tests word wrapping capabilities with dashes involved. The verbage dead-", wrappedLines[0].c_str()) << wrappedLines[0].substr(0, 79);
        EXPECT_STREQ("word should be broken up after the dash.", wrappedLines[1].c_str());

        longText = "This tests word wrapping capabilities with dashes involved. The verbage dead--word should be broken up before the double dash (assunming option).";
        EXPECT_NO_THROW(wrappedLines = wrap(longText, 79));
        EXPECT_STREQ("This tests word wrapping capabilities with dashes involved. The verbage", wrappedLines[0].c_str()) << wrappedLines[0].substr(0, 79);
        EXPECT_STREQ("dead--word should be broken up before the double dash (assunming option).", wrappedLines[1].c_str());

        longText = "EventhoughGoogleTesthasarichsetofassertions,theycanneverbecomplete,asit'simp-ossible(noragoodidea)toanticipateallthescenariosausermightruninto.Therefore,s-ometimesauserhastouseEXPECT_TRUE()tocheckacomplexexpression,for-lackofabettermacro.";
        EXPECT_NO_THROW(wrappedLines = wrap(longText, 79));
        EXPECT_EQ((vector<string>::size_type)4, wrappedLines.size());
        EXPECT_STREQ("EventhoughGoogleTesthasarichsetofassertions,theycanneverbecomplete,asit'simp-", wrappedLines[0].c_str());
        EXPECT_STREQ("ossible(noragoodidea)toanticipateallthescenariosausermightruninto.Therefore,s-", wrappedLines[1].c_str());
        EXPECT_STREQ("ometimesauserhastouseEXPECT_TRUE()tocheckacomplexexpression,for-", wrappedLines[2].c_str());
        EXPECT_STREQ("lackofabettermacro.", wrappedLines[3].c_str());
    } // common_framework.TC_KNL_mpsstools_common_function_WrapText_002

    TEST(common_framework, TC_KNL_mpsstools_common_function_verifyCannonicalName_001)
    {
        vector<string> good_names = {
            "_name",
            "name",
            "name_2",
            "name-fred_2",
            "N"
        };
        vector<string> bad_names = {
            "-name",
            "9names",
            " name",
            "name ",
            "name 2",
            "name#3",
            "name*",
            "",
            "  \n \t\r "
        };

        for (auto it = good_names.begin(); it != good_names.end(); ++it)
        {
            EXPECT_TRUE(verifyCanonicalName(*it)) << "Test Text='" << *it << "'";
        }

        for (auto it = bad_names.begin(); it != bad_names.end(); ++it)
        {
            EXPECT_FALSE(verifyCanonicalName(*it)) << "Test Text='" << *it << "'";
        }
    } // common_framework, TC_KNL_mpsstools_common_function_verifyCannonicalName_001

    TEST(common_framework, TC_KNL_mpsstools_common_function_findMatchingFilesInFolder_001)
    {
        auto result = findMatchingFilesInFolder("", "");
        EXPECT_EQ(0, result.size()) << "vector[0]=" << result[0];
#ifdef _WIN32
        result = findMatchingFilesInFolder("\\Users\\Public\\", "*.something");
#else
        result = findMatchingFilesInFolder("/tmp/", "*.something");
#endif
        EXPECT_EQ(0, result.size()) << "vector[0]=" << result[0];
    } // common_framework, TC_KNL_mpsstools_common_function_findMatchingFilesInFolder_001

    TEST(common_framework, TC_KNL_mpsstools_common_function_deviceNamesFromListRange_001)
    {
        std::vector<std::string> golden = {"mic0", "mic1", "mic2", "mic3", "mic4", "mic5"};
        std::vector<std::string> emptyVector;

        //Valid case
        EXPECT_EQ(golden, deviceNamesFromListRange("mic0,mic1,mic2,mic3,mic4,mic5"));

        //Valid cases, expect fail==false
        bool fail = true;;
        EXPECT_EQ(golden, deviceNamesFromListRange("mic0,mic1-mic5", &fail));
        EXPECT_FALSE(fail);
        fail = true;

        EXPECT_EQ(golden, deviceNamesFromListRange("mic0-mic5", &fail));
        EXPECT_FALSE(fail);
        fail = true;

        EXPECT_EQ(golden, deviceNamesFromListRange("mic5-mic0", &fail));
        EXPECT_FALSE(fail);
        fail = true;

        EXPECT_EQ(golden, deviceNamesFromListRange("mic0,mic1-mic4,mic5", &fail));
        EXPECT_FALSE(fail);
        fail = true;

        EXPECT_EQ(golden, deviceNamesFromListRange("mic0,mic4-mic1,mic5", &fail));
        EXPECT_FALSE(fail);
        fail = true;

        //Valid case, overlapped range
        EXPECT_EQ(golden, deviceNamesFromListRange("mic0,mic0-mic5,mic1,mic2,mic3-mic5", &fail));
        EXPECT_FALSE(fail);
        fail = true;

        EXPECT_EQ(golden, deviceNamesFromListRange("mic5,mic4,mic3-mic0", &fail));
        EXPECT_FALSE(fail);
        fail = false;

        // Expect mic0-mic5 in returned vector, but expect fail to be set
        EXPECT_EQ(golden, deviceNamesFromListRange("mic0,mic1-mic5,mica1234", &fail));
        EXPECT_TRUE(fail);
        fail = false;

        // Expect empty vector to be returned, fail to be set
        EXPECT_EQ(emptyVector, deviceNamesFromListRange("mic0-SomeString", &fail));
        EXPECT_TRUE(fail);
        fail = false;

        EXPECT_EQ(emptyVector, deviceNamesFromListRange("Somestring-mic5", &fail));
        EXPECT_TRUE(fail);
        fail = false;

        deviceNamesFromListRange("mica0", &fail);
        EXPECT_TRUE(fail);
        fail = false;

        // Expect all valid devices to be returned, fail to be set
        EXPECT_EQ(std::vector<std::string>({"mic0", "mic1", "mic2", "mic3"}),
                deviceNamesFromListRange("mic0,mic1,mic2-mic3,mic0-nomic,mica42", &fail)
                );
        EXPECT_TRUE(fail);
    }
}; // namespace micmgmt
