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
#include <stdexcept>
#include <cstdio>

#include "ToolSettings.hpp"
#include "micmgmtCommon.hpp"

using namespace std;

namespace
{
    string sGoodFile = "DefaultFlashFolder=/usr/share/mpss/flash\n";
    string sGoodWithSingle = "DefaultFlashFolder='/usr/share/mpss/flash'\n";
    string sGoodWithDouble = "DefaultFlashFolder=\"/usr/share/mpss/flash\"\n";
    string sGoodWithTrailingComment = "DefaultFlashFolder=/usr/share/mpss/flash # Trailing Comment\n";
    string sGoodWithExtraSpaces = "DefaultFlashFolder   =    /usr/share/mpss/flash # Trailing Comment\n";
    string sGoodWithSection = " # Comment line...\n [MySection] # Comment\nDefaultFlashFolder   =    '/usr/share/mpss/flash' # Trailing Comment\n";
    string sManyBadLines = "\
 # Comment line...\n\
 [BadSection\n\
     =red # Bad name/value line\n\
[BadSection2 )  #\n\
\n\
Line1=\"No end to this value...\n\
Line2='Allow \"#\" in value'\n\
Line3='Mismatched quotes\"\n\
\n\
Line4\t=\t'No end in value plus a # comment portion\n\
[MySection]\n\
\tDefaultFlashFolder\t=    '/usr/share/mpss/flash' # Trailing Comment\n\
";
    string sEmptyFile = "";
    string sGoodFileNoNewline = "DefaultFlashFolder=/usr/share/mpss/flash";

    void writeFile(string contents)
    {
        fstream strm("systools.conf", ios_base::trunc | ios_base::out);
        if (strm.good() == true)
        {
            strm << contents;
            strm.close();
        }
    }

    bool removeFile()
    {
        return (std::remove("systools.conf") == 0)?true:false;
    }
}; // empty namespace

namespace micmgmt
{
    TEST(common_framework, TC_KNL_mpsstools_ToolSettings_001)
    {
        writeFile(sGoodFile);

        ToolSettings* cfg = NULL;
        EXPECT_NO_THROW(cfg = new ToolSettings());

        EXPECT_TRUE(cfg->loaded_);
        EXPECT_EQ(true, (cfg->data_.find("DefaultFlashFolder") != cfg->data_.end()));
        EXPECT_STREQ("/usr/share/mpss/flash", cfg->data_["DefaultFlashFolder"].c_str());
        EXPECT_STREQ(cfg->getValue("DefaultFlashFolder").c_str(), cfg->data_["DefaultFlashFolder"].c_str());

        delete cfg;

        EXPECT_TRUE(removeFile());
    }

    TEST(common_framework, TC_KNL_mpsstools_ToolSettings_002)
    {
        writeFile(sGoodWithSingle);

        ToolSettings* cfg = NULL;
        EXPECT_NO_THROW(cfg = new ToolSettings());

        EXPECT_TRUE(cfg->loaded_);
        EXPECT_EQ(true, (cfg->data_.find("DefaultFlashFolder") != cfg->data_.end()));
        EXPECT_STREQ("/usr/share/mpss/flash", cfg->data_["DefaultFlashFolder"].c_str());
        EXPECT_STREQ(cfg->getValue("DefaultFlashFolder").c_str(), cfg->data_["DefaultFlashFolder"].c_str());

        delete cfg;

        EXPECT_TRUE(removeFile());
    }

    TEST(common_framework, TC_KNL_mpsstools_ToolSettings_003)
    {
        writeFile(sGoodWithDouble);

        ToolSettings* cfg = NULL;
        EXPECT_NO_THROW(cfg = new ToolSettings());

        EXPECT_TRUE(cfg->loaded_);
        EXPECT_EQ(true, (cfg->data_.find("DefaultFlashFolder") != cfg->data_.end()));
        EXPECT_STREQ("/usr/share/mpss/flash", cfg->data_["DefaultFlashFolder"].c_str());
        EXPECT_STREQ(cfg->getValue("DefaultFlashFolder").c_str(), cfg->data_["DefaultFlashFolder"].c_str());

        delete cfg;

        EXPECT_TRUE(removeFile());
    }

    TEST(common_framework, TC_KNL_mpsstools_ToolSettings_004)
    {
        writeFile(sGoodWithTrailingComment);

        ToolSettings* cfg = NULL;
        EXPECT_NO_THROW(cfg = new ToolSettings());

        EXPECT_TRUE(cfg->loaded_);
        EXPECT_EQ(true, (cfg->data_.find("DefaultFlashFolder") != cfg->data_.end()));
        EXPECT_STREQ("/usr/share/mpss/flash", cfg->data_["DefaultFlashFolder"].c_str());
        EXPECT_STREQ(cfg->getValue("DefaultFlashFolder").c_str(), cfg->data_["DefaultFlashFolder"].c_str());

        delete cfg;

        EXPECT_TRUE(removeFile());
    }

    TEST(common_framework, TC_KNL_mpsstools_ToolSettings_005)
    {
        writeFile(sGoodWithExtraSpaces);

        ToolSettings* cfg = NULL;
        EXPECT_NO_THROW(cfg = new ToolSettings());

        EXPECT_TRUE(cfg->loaded_);
        EXPECT_EQ(true, (cfg->data_.find("DefaultFlashFolder") != cfg->data_.end()));
        EXPECT_STREQ("/usr/share/mpss/flash", cfg->data_["DefaultFlashFolder"].c_str());
        EXPECT_STREQ(cfg->getValue("DefaultFlashFolder").c_str(), cfg->data_["DefaultFlashFolder"].c_str());

        delete cfg;

        EXPECT_TRUE(removeFile());
    }

    TEST(common_framework, TC_KNL_mpsstools_ToolSettings_006) // value in section
    {
        writeFile(sGoodWithSection);

        ToolSettings* cfg = NULL;
        EXPECT_NO_THROW(cfg = new ToolSettings());

        EXPECT_TRUE(cfg->loaded_);
        EXPECT_EQ(true, (cfg->data_.find("MySection.DefaultFlashFolder") != cfg->data_.end()));
        EXPECT_STREQ("/usr/share/mpss/flash", cfg->data_["MySection.DefaultFlashFolder"].c_str());
        EXPECT_STREQ(cfg->getValue("DefaultFlashFolder", "MySection").c_str(), cfg->data_["MySection.DefaultFlashFolder"].c_str());

        delete cfg;

        EXPECT_TRUE(removeFile());
    }

    TEST(common_framework, TC_KNL_mpsstools_ToolSettings_007) // No File
    {
        ToolSettings* cfg = NULL;
        EXPECT_NO_THROW(cfg = new ToolSettings());

        EXPECT_FALSE(cfg->loaded_);
        EXPECT_EQ(true, (cfg->data_.find("DefaultFlashFolder") == cfg->data_.end()));
        EXPECT_STREQ("", cfg->data_["DefaultFlashFolder"].c_str());

        EXPECT_STREQ("", cfg->data_["NoSuchEntry"].c_str());
        EXPECT_STREQ(cfg->getValue("DefaultFlashFolder").c_str(), cfg->data_["DefaultFlashFolder"].c_str());

        delete cfg;
    }

    TEST(common_framework, TC_KNL_mpsstools_ToolSettings_008) // Test many malformed lines...
    {
        writeFile(sManyBadLines);

        ToolSettings* cfg = NULL;
        EXPECT_NO_THROW(cfg = new ToolSettings());

        EXPECT_TRUE(cfg->loaded_);
        EXPECT_EQ(true, (cfg->data_.find("MySection.DefaultFlashFolder") != cfg->data_.end()));
        EXPECT_STREQ("/usr/share/mpss/flash", cfg->data_["MySection.DefaultFlashFolder"].c_str());
        EXPECT_STREQ(cfg->getValue("DefaultFlashFolder", "MySection").c_str(), cfg->data_["MySection.DefaultFlashFolder"].c_str());

        EXPECT_STREQ("\"No end to this value...", cfg->data_["Line1"].c_str());
        EXPECT_STREQ("Allow \"#\" in value", cfg->data_["Line2"].c_str());
        EXPECT_STREQ("'Mismatched quotes\"", cfg->data_["Line3"].c_str());
        EXPECT_STREQ("'No end in value plus a", cfg->data_["Line4"].c_str());
        delete cfg;

        EXPECT_TRUE(removeFile());
    }

    TEST(common_framework, TC_KNL_mpsstools_ToolSettings_009) // Empty File
    {
        writeFile(sEmptyFile);

        ToolSettings* cfg = NULL;
        EXPECT_NO_THROW(cfg = new ToolSettings());

        EXPECT_TRUE(cfg->loaded_);
        EXPECT_TRUE(cfg->data_.find("DefaultFlashFolder") == cfg->data_.end());
        EXPECT_STREQ("", cfg->data_["DefaultFlashFolder"].c_str());
        EXPECT_STREQ(cfg->getValue("DefaultFlashFolder").c_str(), cfg->data_["DefaultFlashFolder"].c_str());

        delete cfg;

        EXPECT_TRUE(removeFile());
    }

    TEST(common_framework, TC_KNL_mpsstools_ToolSettings_010) // One line no newline...
    {
        writeFile(sGoodFileNoNewline);

        ToolSettings* cfg = NULL;
        EXPECT_NO_THROW(cfg = new ToolSettings());

        EXPECT_TRUE(cfg->loaded_);
        EXPECT_TRUE(cfg->data_.find("DefaultFlashFolder") != cfg->data_.end());
        EXPECT_STREQ("/usr/share/mpss/flash", cfg->data_["DefaultFlashFolder"].c_str());
        EXPECT_STREQ(cfg->getValue("DefaultFlashFolder").c_str(), cfg->data_["DefaultFlashFolder"].c_str());

        delete cfg;

        EXPECT_TRUE(removeFile());
    }
}; // namespace micmgmt
