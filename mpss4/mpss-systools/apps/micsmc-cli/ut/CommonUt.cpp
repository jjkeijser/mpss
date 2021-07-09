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

// UT Includes
#include "CommonUt.hpp"
#include "KnxMockUtDevice.hpp"

// App Includes
#include "CliApplication.hpp"

// SDK Includes
#include "MicDeviceManager.hpp"

// Commonf Framework Includes
#include "micmgmtCommon.hpp"
#include "CliParser.hpp"
#include "MicOutput.hpp"
#include "ConsoleOutputFormatter.hpp"

// C++ Includes
#include <sstream>

#ifdef _WIN32
    #include <Windows.h>
#else
    #include <cstdlib>
#endif

using namespace std;
using namespace micmgmt;
using namespace micsmccli;

#define TOOLNAME "micsmc-cli"
#define MICSMCCLI_DESCRIPTION "This tool is designed to provide the user the capability to monitor card \
    status and settings and, with sufficient permissions, to change specific settings on the Intel(r) Xeon \
    Phi(tm) coprocessors line of products."

typedef micmgmt::MicDeviceManager::DeviceType DT;

namespace
{
    size_t sNameWidth = 25;
    size_t sIndentSize = 3;

    // Set Environmental Variable (for KN*_MOCK_CNT)
    bool setEnvVar(const string& name, const string& value)
    {
        if (name.empty() == true)
        {
            return false;
        }
#ifdef _WIN32
        BOOL rv = FALSE;
        if (value.empty() == true)
        {
            rv = SetEnvironmentVariableA((LPCSTR)name.c_str(), NULL);
        }
        else
        {
            rv = SetEnvironmentVariableA((LPCSTR)name.c_str(), (LPCSTR)value.c_str());
        }
        return (rv == TRUE);
#else
        int rv = -1;
        if (value.empty() == true)
        {
            rv = unsetenv(name.c_str());
        }
        else
        {
            rv = setenv(name.c_str(), value.c_str(), 1);
        }
        return (rv == 0);
#endif // _WIN32
    }
}; // empty namespace

namespace micsmccli
{
    // class ArgV
    ArgV::ArgV(vector<string> commandLine)
    {
        argc_ = static_cast<int>(commandLine.size());
        argv_ = new char*[argc_ + 1];
        argv_[0] = (char*)TOOLNAME;
        for (int i = 0; i < argc_; ++i)
        {
            string cliItem = commandLine[i];
            int size = static_cast<int>(cliItem.size());
            argv_[i + 1] = new char[size + 1];
#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable: 4996)
#endif
            strncpy(argv_[i + 1], cliItem.c_str(), size);
#ifdef _WIN32
#pragma warning(pop)
#endif
            argv_[i + 1][size] = 0;
        }
    }

    ArgV::~ArgV()
    {
        for (int i = 0; i < argc_; ++i)
        {
            delete[] argv_[i + 1];
        }
        delete[] argv_;
    }

    int ArgV::argc() const
    {
        return (argc_ + 1);
    }

    char** ArgV::argv() const
    {
        return argv_;
    }

    shared_ptr<CliApplication> createApplicationInstance(int type, int cardCount, ostream& stream, vector<string> args)
    {
        ArgV argV(args);
        stringstream ss;
        ss << cardCount;

        if (type == DT::eDeviceTypeKnl)
        {
            setEnvVar("KNL_MOCK_CNT", ss.str());
            setEnvVar("MICMGMT_MPSS_MOCK_VERSION", "4.0");
        }

        CliApplication* app = new CliApplication(argV.argc(), argV.argv(), "1.2.3", TOOLNAME, MICSMCCLI_DESCRIPTION);

        app->stdOut_ = unique_ptr<MicOutputFormatterBase>(new ConsoleOutputFormatter(&stream, sNameWidth, sIndentSize));
        app->stdErr_ = unique_ptr<MicOutputFormatterBase>(new ConsoleOutputFormatter(&stream, sNameWidth, sIndentSize));
        app->output_ = make_shared<MicOutput>(app->parser_->toolName(), app->stdOut_.get(), app->stdErr_.get(), app->fileOut_.get());

        app->parser_->setOutputFormatters(app->stdOut_.get(), app->stdErr_.get(), app->fileOut_.get());

        return shared_ptr<CliApplication>(app);
    }

    void clearTestEnvironment()
    {
        setEnvVar("KNL_MOCK_CNT", "0");
        setEnvVar("MICMGMT_MPSS_MOCK_VERSION", "");
    }

    bool compareWithMask(const string& output, const string& mask)
    {
        const char* ptr = output.c_str(); (void)ptr;
        if (mask.empty() == true)
        {
            return (output == mask);
        }
        // split on "*" then check each part.  Match if all marts match.
        vector<string> parts;
        size_t star = mask.find('*');
        size_t last = 0;
        if (star > 0)
        {
            parts.push_back(mask.substr(0, star));
        }
        while (star != string::npos)
        {
            size_t start = star + 1;
            size_t end = mask.find('*', start);
            if (end != string::npos)
            {
                --end;
            }
            string p = mask.substr(start, (end - start + 1));
            last = start;
            if (p.empty() == false)
            {
                parts.push_back(p);
            }
            star = mask.find('*', last);
        }
        if (parts.size() == 0)
        {
            parts.push_back(mask);
        }
        size_t count = 0;
        star = 0;
        for (auto it = parts.begin(); it != parts.end(); ++it)
        {
            string f = *it;
            star = output.find(f, star);
            if (star != string::npos)
            {
                ++count;
                star += f.length();
            }
            else
            {
                break;
            }
        }
        return (count == parts.size());
    }

    vector<MicDeviceImpl*> createDeviceImplVector(size_t count, int type, bool online, bool admin)
    {
        vector<MicDeviceImpl*> result;
        for (size_t i = 0; i < count; ++i)
        {
            result.push_back(new KnxMockUtDevice(type, (int)i, online, admin));
        }
        return result;
    }
}; // namespace micsmccli
