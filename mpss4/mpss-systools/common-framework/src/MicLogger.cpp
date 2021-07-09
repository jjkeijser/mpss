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

#include "MicLogger.hpp"
#include "InternalLogger.hpp"
#include "micmgmtCommon.hpp"

#include <utility>
#include <stdexcept>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <unordered_map>
#include <limits>
#include <array>
#include <unordered_set>

#include <stdarg.h>

#define LOG_LEN 512
#ifdef _WIN32
#define vsnprintf vsprintf_s
#endif

using namespace std;

/// @dontinclude src/MicLogger.cpp

/// @def INIT_LOGGER(out)
/// @brief Initializes the global logger with an output stream to use.
///
/// Initializes the global logger with an output stream to use.
/// <dl>
/// <dt>out</dt><dd>This is the pointer to an output stream for logging.  NULL
/// means log disabled.</dd>
/// </dl>

/// @def LOG(type, format, args...)
/// @brief Logs a line to the log stream.
///
/// Logs a line to the log stream.  The newline is implied unless you wish to
/// insert extra newlines into the log file.
/// <dl>
/// <dt>type</dt><dd>The type to identify the message to be logged.  This must
/// be one of: \c DEBUG_MSG, \c INFO_MSG, \c WARNING_MSG, \c ERROR_MSG,
/// \c CRITICAL_MSG or \c FATAL_MSG</dd>
/// <dt>format</dt><dd>A printf-like format-string representing the text to be
/// added to the log stream.  Empty string will be ignored.</dd>
/// <dt>args...</dt><dd>Variable arguments to complete the C-format-string.</dd>
/// </dl>
/// @example <tt>LOG("This is the text to be logged along with this integer: %i.", 2048);</tt>

namespace
{
    void strFormat(string & line)
    {
        string str;
        unordered_set<char> data = {'d','i','u','o','x','X','f','F','e','E','g','G','a','A','c','s','p','n','#','0','-',' ','+'};
        size_t dataSize = data.size();
        string::iterator it = line.begin();
        for (unsigned int current = 0; current < line.size(); ++it, ++current)
        {
            bool lastElement = false;
            if (current >= dataSize)
            {
                lastElement = true;
            }
            if (*it == '%' && (lastElement || data.find(line.at(current + 1)) == data.end()))
            {
                str.push_back('%');
            }
            str.push_back(*it);
        }
        line = str;
    }

    unordered_map<unsigned int, string> sMap = {
            { micmgmt::DebugMsg,    "Debug   " },
            { micmgmt::InfoMsg,     "Info    " },
            { micmgmt::WarningMsg,  "Warning " },
            { micmgmt::ErrorMsg,    "Error   " },
            { micmgmt::CriticalMsg, "Critical" },
            { micmgmt::FatalMsg,    "Fatal   " },
    };
}; // Empty namespace

namespace micmgmt
{
    // For Blank MicLogger...
    MicLogger::MicLogger(): out_(NULL)
    {
    }

    MicLogger::~MicLogger()
    {
    }

    void MicLogger::vlog(MessageType type, const char* line, va_list va)
    {
        if (out_ && line[0] != '\0')
        {
            array<char,LOG_LEN>buffer;
            vsnprintf(buffer.data(), buffer.max_size(), line, va);
            lock_guard<mutex> guard(lock_);
            *out_ << getTimeStamp() << ": " << sMap[type] << ": " << buffer.data()
                  << endl << flush;
        }
    }
    void MicLogger::log(MessageType type, const char* line, ...)
    {
        string str=line;
        strFormat(str);
        va_list va;
        va_start(va, line);
        vlog(type, str.c_str(), va);
        va_end(va);
    }

    void MicLogger::log(MessageType type, string line, ...)
    {
        string str=line;
        strFormat(str);
        va_list va;
        va_start(va, line);
        vlog(type, str.c_str(), va);
        va_end(va);
    }

// MSVC++ has "safe" version of gmtime that are NOT standard
#ifdef _WIN32
#pragma warning(disable:4996) // Disable warning...
#endif
    std::string MicLogger::getTimeStamp() const
    {
        time_t      now = time(0);
        struct tm*  pts = gmtime(&now);
        char        buf[64];
        buf[0] = 0;
        if (pts != NULL)
        {
            if (strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", pts) > 0)
            {
                return string(buf);
            }
        }
        return string();
    }
#ifdef _WIN32
#pragma warning(default:4996) // Re-enable Warning...
#endif

    // Static public accessor needed only for macros
    MicLogger& MicLogger::getInstance()
    {
        static MicLogger logger;
        return logger;
    }

    void MicLogger::initialize(std::ostream* ostr)
    {
        out_ = ostr;
    }

    void MicLogger::reset()
    {
        out_ = NULL;
    }

}; // namespace micmgmt