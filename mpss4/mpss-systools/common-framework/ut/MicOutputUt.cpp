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
#include "MicOutput.hpp"
#include "MicOutputError_p.hpp"
#include "ConsoleOutputFormatter.hpp"
#include <ostream>
#include <sstream>
#include <memory>

namespace micmgmt
{
    using namespace std;

    TEST(common_framework, TC_KNL_mpsstools_MicOutput_001)
    {
        ostringstream* stdOut = new ostringstream();
        ostringstream* stdErr = new ostringstream();
        ostringstream* fileOut = new ostringstream();

        {
            MicOutputFormatterBase* out = new ConsoleOutputFormatter(stdOut, 20, 3);
            MicOutputFormatterBase* err = new ConsoleOutputFormatter(stdErr);
            MicOutputFormatterBase* fil = new ConsoleOutputFormatter(fileOut, 16, 4);
            MicOutput output("MicOutput", out, err, fil);
            output.outputError("Message.", MicOutputErrorCode::eMicOutputOutputError);
            EXPECT_STREQ("Message.\n", stdErr->str().c_str());
            EXPECT_STREQ("Message.\n", fileOut->str().c_str());
            EXPECT_STREQ("", stdOut->str().c_str());

            stdOut->str(""); stdErr->str(""); fileOut->str(""); // Clear
            stdOut->clear(); stdErr->clear(); fileOut->clear(); // Clear

            output.startSection("Variables");
            output.outputNameValuePair("Name", "Fred");
            output.outputNameValuePair("Height", "180", "cm");
            output.endSection();
            EXPECT_STREQ("\nVariables:\n   Name                 : Fred\n   Height               : 180 cm\n", stdOut->str().c_str());
            EXPECT_STREQ("", stdErr->str().c_str());
            EXPECT_STREQ("\nVariables:\n    Name             : Fred\n    Height           : 180 cm\n", fileOut->str().c_str());

            stdOut->str(""); stdErr->str(""); fileOut->str(""); // Clear
            stdOut->clear(); stdErr->clear(); fileOut->clear(); // Clear

            output.startSection("Variables");
            output.outputNameValuePair("Name", "Fred");
            output.outputError("Oops!", MicOutputErrorCode::eMicOutputOutputError);
            output.outputNameValuePair("Height", "180", "cm");
            output.endSection();
            output.outputLine("This is the end.");
            EXPECT_STREQ("\nVariables:\n   Name                 : Fred\n   Height               : 180 cm\nThis is the end.\n", stdOut->str().c_str());
            EXPECT_STREQ("Oops!\n", stdErr->str().c_str());
            EXPECT_STREQ("\nVariables:\n    Name             : Fred\nOops!\n    Height           : 180 cm\nThis is the end.\n", fileOut->str().c_str());

            stdOut->str(""); stdErr->str(""); fileOut->str(""); // Clear
            stdOut->clear(); stdErr->clear(); fileOut->clear(); // Clear

            output.startSection("Nested and Indented");
            output.outputError("Raw error string!", MicOutputErrorCode::eMicOutputSuccess);
            output.endSection();
            EXPECT_STREQ("\nNested and Indented:\n", stdOut->str().c_str());
            EXPECT_STREQ("Raw error string!\n", stdErr->str().c_str());
            EXPECT_STREQ("\nNested and Indented:\nRaw error string!\n", fileOut->str().c_str());

            delete fil;
            delete err;
            delete out;
        }

        delete fileOut;
        delete stdErr;
        delete stdOut;
    } // common_framework.TC_KNL_mpsstools_MicOutput_001

    TEST(common_framework, TC_KNL_mpsstools_MicOutput_002)
    {
        ostringstream* stdOut = new ostringstream();
        ostringstream* stdErr = new ostringstream();

        {
            MicOutputFormatterBase* out = new ConsoleOutputFormatter(stdOut, 20, 3);
            MicOutputFormatterBase* err = new ConsoleOutputFormatter(stdErr);
            MicOutput output("MicOutput", out, err);
            output.outputError("Message.", MicOutputErrorCode::eMicOutputOutputError);
            EXPECT_STREQ("Message.\n", stdErr->str().c_str());
            EXPECT_STREQ("", stdOut->str().c_str());

            stdOut->str(""); stdErr->str(""); // Clear
            stdOut->clear(); stdErr->clear(); // Clear

            output.startSection("Variables");
            output.outputNameValuePair("Name", "Fred");
            output.outputNameValuePair("Height", "180", "cm");
            output.endSection();
            EXPECT_STREQ("\nVariables:\n   Name                 : Fred\n   Height               : 180 cm\n", stdOut->str().c_str());
            EXPECT_STREQ("", stdErr->str().c_str());

            stdOut->str(""); stdErr->str(""); // Clear
            stdOut->clear(); stdErr->clear(); // Clear

            output.startSection("Variables");
            output.outputNameValuePair("Name", "Fred");
            output.outputError("Oops!", MicOutputErrorCode::eMicOutputOutputError);
            output.outputNameValuePair("Height", "180", "cm");
            output.endSection();
            output.outputLine("This is the end.");
            EXPECT_STREQ("\nVariables:\n   Name                 : Fred\n   Height               : 180 cm\nThis is the end.\n", stdOut->str().c_str());
            EXPECT_STREQ("Oops!\n", stdErr->str().c_str());

            delete err;
            delete out;
        }

        delete stdErr;
        delete stdOut;
    } // common_framework.TC_KNL_mpsstools_MicOutput_002

    TEST(common_framework, TC_KNL_mpsstools_MicOutput_003)
    {
        ostringstream* stdOut = new ostringstream();
        ostringstream* stdErr = new ostringstream();
        ostringstream* fileOut = new ostringstream();
        {
            MicOutputFormatterBase* out = new ConsoleOutputFormatter(stdOut, 20, 3);
            MicOutputFormatterBase* err = new ConsoleOutputFormatter(stdErr);
            MicOutputFormatterBase* fil = new ConsoleOutputFormatter(fileOut, 20, 3);
            MicOutput output("MicOutput", out, err, fil);

            EXPECT_FALSE(output.isSilent());
            output.setSilent(true);
            EXPECT_TRUE(output.isSilent());
            output.outputError("Message.", MicOutputErrorCode::eMicOutputErrorError);
            EXPECT_STREQ("Message.\n", stdErr->str().c_str());
            EXPECT_STREQ("", stdOut->str().c_str());
            EXPECT_STREQ("Message.\n", fileOut->str().c_str());

            fileOut->str(""); fileOut->clear(); // clear
            stdErr->str("");  stdErr->clear();  // clear

            output.startSection("Variables");
            output.outputNameValuePair("Name", "Fred");
            output.outputNameValuePair("Height", "180", "cm");
            output.endSection();
            EXPECT_STREQ("", stdOut->str().c_str());
            EXPECT_STREQ("", stdErr->str().c_str());
            EXPECT_STREQ("\nVariables:\n   Name                 : Fred\n   Height               : 180 cm\n", fileOut->str().c_str());

            fileOut->str(""); fileOut->clear(); // clear

            output.setSilent(false);
            EXPECT_FALSE(output.isSilent());

            output.startSection("Variables");
            output.outputNameValuePair("Name", "Fred");
            output.outputError("Oops!", MicOutputErrorCode::eMicOutputErrorError);
            output.outputNameValuePair("Height", "180", "cm");
            output.endSection();
            output.outputLine("This is the end.");
            EXPECT_STREQ("\nVariables:\n   Name                 : Fred\n   Height               : 180 cm\nThis is the end.\n", stdOut->str().c_str());
            EXPECT_STREQ("Oops!\n", stdErr->str().c_str());
            EXPECT_STREQ("\nVariables:\n   Name                 : Fred\nOops!\n   Height               : 180 cm\nThis is the end.\n", fileOut->str().c_str());

            delete fil;
            delete err;
            delete out;
        }
        delete fileOut;
        delete stdErr;
        delete stdOut;
    } // common_framework.TC_KNL_mpsstools_MicOutput_003

}; // namespace micmgmt