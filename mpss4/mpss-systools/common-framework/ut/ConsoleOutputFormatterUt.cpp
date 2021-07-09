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
#include "MicOutputError_p.hpp"
#include "ConsoleOutputFormatter.hpp"
#include <sstream>

namespace micmgmt
{
    using namespace std;

    TEST(common_framework, TC_KNL_mpsstools_ConsoleOutputFormatter_001)
    {
        ostringstream* oStrStream = new ostringstream();
        {
            oStrStream->str(""); oStrStream->clear(); // clear
            ConsoleOutputFormatter formatter(oStrStream, 24, 4);
            formatter.outputLine("This is a test.");
            formatter.outputLine();
            formatter.startSection("SUBSECTION1");
            formatter.outputLine("Testing...");
            formatter.startSection("SUBSECTION2");
            formatter.outputLine("Testing...");
            formatter.endSection();
            formatter.endSection();
            formatter.outputLine("Testing...");
            string str = oStrStream->str();
            EXPECT_STREQ("This is a test.\n\n\nSUBSECTION1:\n    Testing...\n\n    SUBSECTION2:\n        Testing...\nTesting...\n", str.c_str());
        }
        {
            oStrStream->str(""); oStrStream->clear(); // clear
            ConsoleOutputFormatter formatter(oStrStream, 24, 4);
            formatter.outputLine("This is a test.",false,true);
            formatter.outputLine();
            formatter.startSection("SUBSECTION1");
            formatter.outputLine("Testing...");
            formatter.startSection("SUBSECTION2");
            formatter.outputLine("Testing...",false,true);
            formatter.endSection();
            formatter.endSection();
            formatter.outputLine("Testing...");
            string str = oStrStream->str();
            EXPECT_STREQ("This is a test.\n\nSUBSECTION1:\n    Testing...\n\n    SUBSECTION2:\n        Testing...Testing...\n", str.c_str());
        }
        {
            oStrStream->str(""); oStrStream->clear(); // clear
            ConsoleOutputFormatter formatter(oStrStream, 24, 2);
            formatter.outputLine("This is a test.");
            formatter.outputLine();
            formatter.startSection("SUBSECTION1");
            formatter.outputLine("Testing...");
            formatter.startSection("SUBSECTION2");
            formatter.outputLine("Testing...");
            formatter.endSection();
            formatter.endSection();
            formatter.outputLine("Testing...");
            string str = oStrStream->str();
            EXPECT_STREQ("This is a test.\n\n\nSUBSECTION1:\n  Testing...\n\n  SUBSECTION2:\n    Testing...\nTesting...\n", str.c_str());
        }
        {
            oStrStream->str(""); oStrStream->clear(); // clear
            ConsoleOutputFormatter formatter(oStrStream, 8, 4);
            formatter.outputLine("This is a test.");
            formatter.outputLine();
            formatter.startSection("SUBSECTION1");
            formatter.outputNameValuePair("Name1", "Value1");
            formatter.outputNameValuePair("Name2", "Value2");
            formatter.startSection("SUBSECTION2");
            formatter.outputNameValuePair("Name3", "Value3");
            formatter.outputNameValuePair("Name4", "200", "ms");
            formatter.endSection();
            formatter.endSection();
            formatter.outputLine("Testing...");
            string str = oStrStream->str();
            EXPECT_STREQ("This is a test.\n\n\nSUBSECTION1:\n    Name1    : Value1\n    Name2    : Value2\n\n    SUBSECTION2:\n        Name3    : Value3\n        Name4    : 200 ms\nTesting...\n", str.c_str());
        }
        {
            oStrStream->str(""); oStrStream->clear(); // clear
            ConsoleOutputFormatter formatter(oStrStream, 8, 2);
            formatter.outputLine("This is a test.");
            formatter.outputLine();
            formatter.startSection("SUBSECTION1");
            formatter.outputNameValuePair("Name1", "Value1");
            formatter.outputNameValuePair("Name2", "Value2");
            formatter.startSection("SUBSECTION2");
            formatter.outputNameValuePair("Name3", "Value3");
            formatter.outputNameValuePair("Name4", "Value4");
            EXPECT_THROW(formatter.outputNameValuePair("BigName4", "Value4Value4Value4Value4Value4Value4Value4Value4Value4Value4Value4Value4Value4Value4Value4Value4Value4Value4"), overflow_error);
            EXPECT_THROW(formatter.outputNameValuePair("", "Value5"), invalid_argument);
            formatter.endSection();
            formatter.endSection();
            EXPECT_THROW(formatter.endSection(),logic_error);
            formatter.outputLine("Testing...");
            string str = oStrStream->str();
            EXPECT_STREQ("This is a test.\n\n\nSUBSECTION1:\n  Name1    : Value1\n  Name2    : Value2\n\n  SUBSECTION2:\n    Name3    : Value3\n    Name4    : Value4\nTesting...\n", str.c_str());
        }
        delete oStrStream;
    } // common_framework.TC_KNL_mpsstools_DefaultOutputFormatter_001

    TEST(common_framework, TC_KNL_mpsstools_ConsoleOutputFormatter_002)
    {
        ostringstream* oStrStream = new ostringstream();
        {
            oStrStream->str(""); oStrStream->clear(); // clear
            ConsoleOutputFormatter formatter(oStrStream, 24, 4);
            formatter.outputLine("This is a test.");
            formatter.outputLine();
            formatter.startSection("SUBSECTION1");
            formatter.outputLine("Testing...");
            formatter.outputError("", MicOutputErrorCode::eMicOutputErrorError);
            formatter.endSection();
            formatter.outputLine("Testing...");
            string str = oStrStream->str();
            EXPECT_STREQ("This is a test.\n\n\nSUBSECTION1:\n    Testing...\n\nTesting...\n", str.c_str());
        }
        {
            oStrStream->str(""); oStrStream->clear(); // clear
            ConsoleOutputFormatter formatter(oStrStream, 24, 4);
            formatter.outputLine("This is a test.");
            formatter.outputLine();
            formatter.startSection("SUBSECTION1");
            formatter.outputLine("Testing...");
            formatter.outputError("This is a really, really, really, really, really, really long error message that should get wrapped.", MicOutputErrorCode::eMicOutputErrorError);
            formatter.endSection();
            formatter.outputLine("Testing...");
            string str = oStrStream->str();
            EXPECT_STREQ("This is a test.\n\n\nSUBSECTION1:\n    Testing...\nThis is a really, really, really, really, really, really long error message\nthat should get wrapped.\nTesting...\n", str.c_str());
        }
        delete oStrStream;
    } // common_framework.TC_KNL_mpsstools_DefaultOutputFormatter_002

    TEST(common_framework, TC_KNL_mpsstools_ConsoleOutputFormatter_003) // Test new suppress section newline feature
    {
        ostringstream* oStrStream = new ostringstream();
        {
            oStrStream->str(""); oStrStream->clear(); // clear
            ConsoleOutputFormatter formatter(oStrStream, 24, 4);
            formatter.outputLine("This is a test.");
            formatter.outputLine();
            formatter.startSection("SUBSECTION1", true);
            formatter.outputLine("Testing...");
            formatter.outputError("", MicOutputErrorCode::eMicOutputErrorError);
            formatter.endSection();
            formatter.outputLine("Testing   ...", true);
            string str = oStrStream->str();
            EXPECT_STREQ("This is a test.\n\nSUBSECTION1:\n    Testing...\n\nTesting   ...\n", str.c_str());
        }
        {
            oStrStream->str(""); oStrStream->clear(); // clear
            ConsoleOutputFormatter formatter(oStrStream, 24, 4);
            formatter.outputLine("This is a test.");
            formatter.outputLine();
            formatter.startSection("SUBSECTION1", true);
            formatter.outputLine("Testing     ...", true);
            formatter.outputError("This is a really, really, really, really, really, really long error message that should get wrapped.", MicOutputErrorCode::eMicOutputErrorError);
            formatter.endSection();
            formatter.outputLine("Testing...");
            string str = oStrStream->str();
            EXPECT_STREQ("This is a test.\n\nSUBSECTION1:\n    Testing     ...\nThis is a really, really, really, really, really, really long error message\nthat should get wrapped.\nTesting...\n", str.c_str());
        }
        delete oStrStream;
    } // common_framework.TC_KNL_mpsstools_DefaultOutputFormatter_002
}; // namespace micmgmt
