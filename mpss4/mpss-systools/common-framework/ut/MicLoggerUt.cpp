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
#include "MicLogger.hpp"
#include "micmgmtCommon.hpp"

#include <fstream>
#include <stdexcept>
#include <cstdio>

#ifndef _WIN32
    #if __GNUC__ == 4
        #if __GNUC_MINOR__ > 8
            #define HAVE_REGEX
        #endif
    #elif __GNUC__ > 4
        #define HAVE_REGEX
    #endif // GCC Version
#else // _WIN32
    #define HAVE_REGEX
#endif // _WIN32

#ifdef HAVE_REGEX
    #include <regex>
#endif

using namespace std;

namespace // Empty
{
    std::stringstream sLocalStream;

    bool RAN_TEST = false;

    bool regexCompare(const string& text, const string& expression)
    {
#ifdef HAVE_REGEX
        return regex_match(text, regex(expression));
#else // HAVE_REGEX
        // BEWARE: This is NOT a regex substitute and depends on a specific pattern!
        string localText = text;
        string localExpr = expression;
        string::size_type start = localText.find("201");
        string::size_type end = localText.find(": ");
        while(start != string::npos)
        {
            localText.erase(start, end - start + 1);
            start = localText.find("201");
            end = localText.find(": ", start);
        }
        start = localExpr.find(".*: ");
        while(start != string::npos)
        {
            localExpr.erase(start, 3);
            start = localExpr.find(".*: ");
        }
        return (localText == localExpr);
#endif // HAVE_REGEX
    }
}; // Empty

namespace micmgmt
{
    const string expected = ".*: Debug   : The 1st message.\n"
                            ".*: Info    : The 2nd message.\n"
                            ".*: Warning : The 3rd message.\n"
                            ".*: Error   : The 4th message.\n"
                            ".*: Critical: The 5th message.\n"
                            ".*: Fatal   : The 6th message.\n";

    TEST(common_framework, TC_KNL_mpsstools_MicLogger_001)
    {
        if (RAN_TEST == false)
        {
            EXPECT_NO_THROW(LOG(ERROR_MSG, "Test of blank logger."));
            RAN_TEST = true;
        }

        INIT_LOGGER(&sLocalStream);

        LOG(DEBUG_MSG, string("The 1st message."));
        LOG(INFO_MSG, "The %dnd message.", 2);
        LOG(WARNING_MSG, "The 3rd message.");
        LOG(ERROR_MSG, "%s %dth message.", "The", 4);
        LOG(CRITICAL_MSG, string("The %dth message."), 5);
        LOG(FATAL_MSG, "%s", "The 6th message.");

        string data = sLocalStream.str();
        EXPECT_TRUE(regexCompare(data, expected)) << "Actual: " << data;
    }
}; // namespace micmgmt
