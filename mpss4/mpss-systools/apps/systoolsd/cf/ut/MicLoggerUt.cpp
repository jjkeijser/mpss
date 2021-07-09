/*
 * Copyright (c) 2017, Intel Corporation.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
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
    const std::string& sLocalFileName = "logger-ut";

    bool RAN_TEST = false;

    bool TestLogExists(const string& filename)
    {
        ifstream file(filename, ios_base::binary);
        if (file.bad() || file.fail())
        {
            return false;
        }
        else
        {
            file.close();
            return true;
        }
    }
    string ReadTestLog(const string& filename)
    {
        ifstream file(filename, ios_base::binary);
        if (file.bad() || file.fail())
        {
            throw runtime_error("Fatal Error: failed to open test file '" + filename + "'!");
        }
        file.seekg(0,file.end);
        std::streamoff length = file.tellg();
        file.seekg(0, file.beg);
        char* buffer = new char[length + 1];
        file.read(buffer, length);
        string rv("");
        if (file)
        {
            buffer[length] = '\0';
            rv = buffer;
            string::size_type pos = rv.find("\r\n");
            while (pos != string::npos)
            {
                rv.erase(pos, 1);
                pos = rv.find("\r\n");
            }
        }
        delete[] buffer;
        return rv;
    }

    bool regexCompare(const string& text, const string& expression)
    {
#ifdef HAVE_REGEX
        return regex_match(text, regex(expression));
#else // HAVE_REGEX
        // BEWARE: This is NOT a regex substitute and depends on a specfic pattern!
        string localText = text;
        string localExpr = expression;
        string::size_type start = localText.find("201");
        string::size_type end = localText.find("Z: ");
        while(start != string::npos)
        {
            localText.erase(start, end - start + 1);
            start = localText.find("201");
            end = localText.find("Z: ");
        }
        start = localExpr.find(".*Z: ");
        while(start != string::npos)
        {
            localExpr.erase(start, 3);
            start = localExpr.find(".*Z: ");
        }
        return (localText == localExpr);
#endif // HAVE_REGEX
    }
}; // Empty

namespace micmgmt
{
    TEST(common_framework, TC_KNL_mpsstools_MicLogger_001)
    {
        if (RAN_TEST == false)
        {
            EXPECT_NO_THROW(LOG(ERROR_LVL, "Test of blank logger."));
            EXPECT_EQ(WARNING_LVL, micmgmt::MicLogger::getThis().getFilterLevel());
            RAN_TEST = true;
        }

        MicLogger log(sLocalFileName);

        EXPECT_EQ(WARNING_LVL, log.getFilterLevel());

        SET_LOGGER_LEVEL(DEBUG_LVL);
        EXPECT_TRUE(log.isLoggingEnabled());
        EXPECT_EQ(DEBUG_LVL, log.getFilterLevel());

        SET_LOGGER_LEVEL(INFO_LVL);
        EXPECT_EQ(INFO_LVL, log.getFilterLevel());

        SET_LOGGER_LEVEL(WARNING_LVL);
        EXPECT_EQ(WARNING_LVL, log.getFilterLevel());

        SET_LOGGER_LEVEL(ERROR_LVL);
        EXPECT_EQ(ERROR_LVL, log.getFilterLevel());

        SET_LOGGER_LEVEL(CRITICAL_LVL);
        EXPECT_EQ(CRITICAL_LVL, log.getFilterLevel());

        SET_LOGGER_LEVEL(FATAL_LVL);
        EXPECT_EQ(FATAL_LVL, log.getFilterLevel());

        string fn = log.filename_;
        std::remove(fn.c_str());
    }

    const string g1("\
.*Z: Debug   : The message.\n\
.*Z: Info    : The message.\n\
.*Z: Warning : The message.\n\
.*Z: Error   : The message.\n\
.*Z: Critical: The message.\n\
.*Z: Fatal   : The message.\n\
");

    const string g2("\
.*Z: Info    : The message.\n\
.*Z: Warning : The message.\n\
.*Z: Error   : The message.\n\
.*Z: Critical: The message.\n\
.*Z: Fatal   : The message.\n\
");

    const string g3("\
.*Z: Warning : The message.\n\
.*Z: Error   : The message.\n\
.*Z: Critical: The message.\n\
.*Z: Fatal   : The message.\n\
");

    const string g4("\
.*Z: Error   : The message.\n\
.*Z: Critical: The message.\n\
.*Z: Fatal   : The message.\n\
");

    const string g5("\
.*Z: Critical: The message.\n\
.*Z: Fatal   : The message.\n\
");

    const string g6("\
.*Z: Fatal   : The message.\n\
");

    TEST(common_framework, TC_KNL_mpsstools_MicLogger_002)
    {
        if (RAN_TEST == false)
        {
            EXPECT_NO_THROW(LOG(ERROR_LVL, "Test of blank logger."));
            EXPECT_EQ(WARNING_LVL, micmgmt::MicLogger::getThis().getFilterLevel());
            RAN_TEST = true;
        }

        MicLogger* log = new MicLogger(sLocalFileName);

        // ALL Messages
        SET_LOGGER_LEVEL(DEBUG_LVL);
        LOG(DEBUG_LVL, "The message.");
        LOG(INFO_LVL, "The message.");
        LOG(WARNING_LVL, "The message.");
        LOG(ERROR_LVL, "The message.");
        LOG(CRITICAL_LVL, "The message.");
        LOG(FATAL_LVL, "The message.");

        string data = ReadTestLog(log->filename_);
        EXPECT_TRUE(regexCompare(data, g1)) << "Actual: " << data;
        std::remove(log->filename_.c_str());

        // eInfo+ Messages
        SET_LOGGER_LEVEL(INFO_LVL);
        LOG(DEBUG_LVL, "The message.");
        LOG(INFO_LVL, "The message.");
        LOG(WARNING_LVL, "The message.");
        LOG(ERROR_LVL, "The message.");
        LOG(CRITICAL_LVL, "The message.");
        LOG(FATAL_LVL, "The message.");

        data = ReadTestLog(log->filename_);
        EXPECT_TRUE(regexCompare(data, g2)) << "Actual: " << data;
        std::remove(log->filename_.c_str());

        // eWarn+ Messages
        SET_LOGGER_LEVEL(WARNING_LVL);
        LOG(DEBUG_LVL, "The message.");
        LOG(INFO_LVL, "The message.");
        LOG(WARNING_LVL, "The message.");
        LOG(ERROR_LVL, "The message.");
        LOG(CRITICAL_LVL, "The message.");
        LOG(FATAL_LVL, "The message.");

        data = ReadTestLog(log->filename_);
        EXPECT_TRUE(regexCompare(data, g3)) << "Actual: " << data;
        std::remove(log->filename_.c_str());

        // Error+ Messages
        SET_LOGGER_LEVEL(ERROR_LVL);
        LOG(DEBUG_LVL, "The message.");
        LOG(INFO_LVL, "The message.");
        LOG(WARNING_LVL, "The message.");
        LOG(ERROR_LVL, "The message.");
        LOG(CRITICAL_LVL, "The message.");
        LOG(FATAL_LVL, "The message.");

        data = ReadTestLog(log->filename_);
        EXPECT_TRUE(regexCompare(data, g4)) << "Actual: " << data;
        std::remove(log->filename_.c_str());

        // eCritical+ Messages
        SET_LOGGER_LEVEL(CRITICAL_LVL);
        LOG(DEBUG_LVL, "The message.");
        LOG(INFO_LVL, "The message.");
        LOG(WARNING_LVL, "The message.");
        LOG(ERROR_LVL, "The message.");
        LOG(CRITICAL_LVL, "The message.");
        LOG(FATAL_LVL, "The message.");

        data = ReadTestLog(log->filename_);
        EXPECT_TRUE(regexCompare(data, g5)) << "Actual: " << data;
        std::remove(log->filename_.c_str());

        // eFatal+ Messages
        SET_LOGGER_LEVEL(FATAL_LVL);
        LOG(DEBUG_LVL, "The message.");
        LOG(INFO_LVL, "The message.");
        LOG(WARNING_LVL, "The message.");
        LOG(ERROR_LVL, "The message.");
        LOG(CRITICAL_LVL, "The message.");
        LOG(FATAL_LVL, "The message.");

        data = ReadTestLog(log->filename_);
        EXPECT_TRUE(regexCompare(data, g6)) << "Actual: " << data;
        std::remove(log->filename_.c_str());

        // Disabled Messages
        SET_LOGGER_LEVEL(DISABLED_LVL);
        LOG(DEBUG_LVL, "The message.");
        LOG(INFO_LVL, "The message.");
        LOG(WARNING_LVL, "The message.");
        LOG(ERROR_LVL, "The message.");
        LOG(CRITICAL_LVL, "The message.");
        LOG(FATAL_LVL, "The message.");

        EXPECT_FALSE(TestLogExists(log->filename_)) << std::remove(log->filename_.c_str());

        string fn = log->filename_;
        delete log;
        std::remove(fn.c_str());
    }

    const string a1("\
.*Z: Debug   : The message.\n\
.*Z: Info    : The message.\n\
.*Z: Warning : The message.\n\
.*Z: Error   : The message.\n\
.*Z: Critical: The message.\n\
.*Z: Fatal   : The message.\n\
");

    const string a2("\
.*Z: Debug   : The message.\n\
.*Z: Info    : The message.\n\
.*Z: Warning : The message.\n\
.*Z: Error   : The message.\n\
.*Z: Critical: The message.\n\
.*Z: Fatal   : The message.\n\
.*Z: Debug   : The message.\n\
.*Z: Info    : The message.\n\
.*Z: Warning : The message.\n\
.*Z: Error   : The message.\n\
.*Z: Critical: The message.\n\
.*Z: Fatal   : The message.\n\
");

    const string a3("\
.*Z: Debug   : The message.\n\
.*Z: Info    : The message.\n\
.*Z: Warning : The message.\n\
.*Z: Error   : The message.\n\
.*Z: Critical: The message.\n\
.*Z: Fatal   : The message.\n\
.*Z: Debug   : The message.\n\
.*Z: Info    : The message.\n\
.*Z: Warning : The message.\n\
.*Z: Error   : The message.\n\
.*Z: Critical: The message.\n\
.*Z: Fatal   : The message.\n\
.*Z: Warning : The message.\n\
.*Z: Error   : The message.\n\
.*Z: Critical: The message.\n\
.*Z: Fatal   : The message.\n\
");

    TEST(common_framework, TC_KNL_mpsstools_MicLogger_003)
    {
        if (RAN_TEST == false)
        {
            EXPECT_NO_THROW(LOG(ERROR_LVL, "Test of blank logger."));
            EXPECT_EQ(WARNING_LVL, micmgmt::MicLogger::getThis().getFilterLevel());
            RAN_TEST = true;
        }

        MicLogger* log = new MicLogger(sLocalFileName);

        // ALL Messages
        SET_LOGGER_LEVEL(DEBUG_LVL);
        LOG(DEBUG_LVL, "The message.");
        LOG(INFO_LVL, "The message.");
        LOG(WARNING_LVL, "The message.");
        LOG(ERROR_LVL, "The message.");
        LOG(CRITICAL_LVL, "The message.");
        LOG(FATAL_LVL, "The message.");

        string data = ReadTestLog(log->filename_);
        EXPECT_TRUE(regexCompare(data, a1)) << "Actual: " << data;

        // ALL Messages + append
        SET_LOGGER_LEVEL(DEBUG_LVL);
        LOG(DEBUG_LVL, "The message.");
        LOG(INFO_LVL, "The message.");
        LOG(WARNING_LVL, "The message.");
        LOG(ERROR_LVL, "The message.");
        LOG(CRITICAL_LVL, "The message.");
        LOG(FATAL_LVL, "The message.");

        data = ReadTestLog(log->filename_);
        EXPECT_TRUE(regexCompare(data, a2)) << "Actual: " << data;

        // Warning+ Messages + append
        SET_LOGGER_LEVEL(WARNING_LVL);
        LOG(DEBUG_LVL, "The message.");
        LOG(INFO_LVL, "The message.");
        LOG(WARNING_LVL, "The message.");
        LOG(ERROR_LVL, "The message.");
        LOG(CRITICAL_LVL, "The message.");
        LOG(FATAL_LVL, "The message.");

        data = ReadTestLog(log->filename_);
        EXPECT_TRUE(regexCompare(data, a3)) << "Actual: " << data;

        string fn = log->filename_;
        delete log;
        std::remove(fn.c_str());
    }

    TEST(common_framework, TC_KNL_mpsstools_MicLogger_004)
    {
        // Positive Tests
        unsigned int level = DEBUG_LVL;
        string levelStr = LEVEL_TO_STRING(level);
        EXPECT_EQ(level, STRING_TO_LEVEL(levelStr)) << "String Value=\"" << levelStr << "\"";

        level = INFO_LVL;
        levelStr = LEVEL_TO_STRING(level);
        EXPECT_EQ(level, STRING_TO_LEVEL(levelStr)) << "String Value=\"" << levelStr << "\"";

        level = WARNING_LVL;
        levelStr = LEVEL_TO_STRING(level);
        EXPECT_EQ(level, STRING_TO_LEVEL(levelStr)) << "String Value=\"" << levelStr << "\"";

        level = ERROR_LVL;
        levelStr = LEVEL_TO_STRING(level);
        EXPECT_EQ(level, STRING_TO_LEVEL(levelStr)) << "String Value=\"" << levelStr << "\"";

        level = CRITICAL_LVL;
        levelStr = LEVEL_TO_STRING(level);
        EXPECT_EQ(level, STRING_TO_LEVEL(levelStr)) << "String Value=\"" << levelStr << "\"";

        level = FATAL_LVL;
        levelStr = LEVEL_TO_STRING(level);
        EXPECT_EQ(level, STRING_TO_LEVEL(levelStr)) << "String Value=\"" << levelStr << "\"";

        level = DISABLED_LVL;
        levelStr = LEVEL_TO_STRING(level);
        EXPECT_EQ(level, STRING_TO_LEVEL(levelStr)) << "String Value=\"" << levelStr << "\"";

        // Negative Tests
        level = 20;
        levelStr = LEVEL_TO_STRING(level);
        EXPECT_STREQ("", levelStr.c_str()) << "String Value=\"" << levelStr << "\"";

        levelStr = "greg";
        level = STRING_TO_LEVEL(levelStr);
        EXPECT_EQ(numeric_limits<unsigned int>::max(), level) << "Unsigned Int Value=\"" << level << "\"";
    }

    TEST(common_framework, TC_KNL_mpsstools_MicLogger_005)
    {
        if (RAN_TEST == false)
        {
            EXPECT_NO_THROW(LOG(ERROR_LVL, "Test of blank logger."));
            EXPECT_EQ(WARNING_LVL, micmgmt::MicLogger::getThis().getFilterLevel());
            RAN_TEST = true;
        }

        // logger file backup tests

        MicLogger* log = new MicLogger(sLocalFileName, DebugLevel);

        for (int i = 0; i < 109; ++i) // UT max lines = 18
        {
            LOG(DEBUG_LVL, "The message.");
        }

        string fn = log->filename_;
        EXPECT_TRUE(TestLogExists(fn)) << "File '" << fn << "' missing!'";
        std::remove(fn.c_str());
        string fnb = fn + ".1";
        EXPECT_TRUE(TestLogExists(fnb)) << "File '" << fnb << "' missing!'";
        std::remove(fnb.c_str());
        fnb = fn + ".2";
        EXPECT_TRUE(TestLogExists(fnb)) << "File '" << fnb << "' missing!'";
        std::remove(fnb.c_str());
        fnb = fn + ".3";
        EXPECT_TRUE(TestLogExists(fnb)) << "File '" << fnb << "' missing!'";
        std::remove(fnb.c_str());
        fnb = fn + ".4";
        EXPECT_TRUE(TestLogExists(fnb)) << "File '" << fnb << "' missing!'";
        std::remove(fnb.c_str());
        fnb = fn + ".5";
        EXPECT_TRUE(TestLogExists(fnb)) << "File '" << fnb << "' missing!'";
        std::remove(fnb.c_str());
        fnb = fn + ".6";
        EXPECT_FALSE(TestLogExists(fnb)) << "File '" << fnb << "' exists!'";

        delete log;
    }
}; // namespace micmgmt
