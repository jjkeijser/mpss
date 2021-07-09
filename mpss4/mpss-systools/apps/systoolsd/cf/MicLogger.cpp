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

using namespace std;

/// @dontinclude src/MicLogger.cpp

/// @var DEBUG_LVL 0
///
/// For \a SET_LOGGER_LEVEL() this will log \b ALL log entries to the file. For \a LOG() this will create a new
/// entry for the log at this logging level.

/// @var INFO_LVL 1
///
/// For \a SET_LOGGER_LEVEL() this will log entries to the file with INFO_LVL and above (not DEBUG_LVL).
/// For \a LOG() this will create a new entry for the log at this logging level.

/// @var WARNING_LVL 2
///
/// For \a SET_LOGGER_LEVEL() this will log entries to the file with WARNING_LVL and above (not INFO_LVL or below).
/// For \a LOG() this will create a new entry for the log at this logging level.

/// @var ERROR_LVL 3
///
/// For \a SET_LOGGER_LEVEL() this will log entries to the file with ERROR_LVL and above (not WARNING_LVL or below).
/// For \a LOG() this will create a new entry for the log at this logging level.

/// @var CRITICAL_LVL 4
///
/// For \a SET_LOGGER_LEVEL() this will log entries to the file with CRITICAL_LVL and above (not ERROR_LVL or below).
/// For \a LOG() this will create a new entry for the log at this logging level.

/// @var FATAL_LVL 5
///
/// For \a SET_LOGGER_LEVEL() this will log entries to the file with FATAL_LVL and above (not CRITICAL_LVL or below).
/// For \a LOG() this will create a new entry for the log at this logging level.

/// @var DISABLED_LVL 6
///
/// For \a SET_LOGGER_LEVEL() this will log \b NO log entries to the file. This is an illegal value for the \a LOG() macro.

/// @def INIT_LOGGER(filename, append)
/// @brief Intializes the global logger with a filename to use.
/// @exception std::runtime_error This is thrown when the user has no home folder and logging cannot occur.
///
/// Intializes the global logger with a filename to use.  If \a append is true this will not append to the previous file but
/// will overwrite it.
/// <dl>
/// <dt>filename</dt><dd>This is the filename to open for logging.</dd>
/// <dt>append</dt><dd>This is the mode to open the file for logging. \c append = \c true means file is appended to, otherwise
/// the file is truncated.</dd>
/// </dl>
/// @Note: This macro function can throw a \c std::runtime_error exception if the filename cannot be opened.

/// @def SET_LOGGER_LEVEL(level)
/// @brief Set the logger level filter for storing log messages into the logfile.
/// 
/// Set the logger level filter for storing log messages into the logfile.
/// <dl>
/// <dt>level</dt><dd>This is the filtering level for the logger instance, this can be changed at any time. This must be one of:
/// \c DEBUG_LVL, \c INFO_LVL, \c WARNING_LVL, \c ERROR_LVL, \c CRITICAL_LVL, \c FATAL_LVL, or \c DISABLED_LVL</dd>
/// </dl>

/// @def LOG(level, line)
/// @brief Logs a line to the log file.
///
/// Logs a line to the log file.  The newline is implied unless you wish to insert extra newlines into the log file.
/// <dl>
/// <dt>level</dt><dd>The logging level to insert the line with into the log file or stream. If a value greater than \c CRITICAL_LVL
/// then \c CRITICAL_LVL will be used. This must be one of: \c DEBUG_LVL, \c INFO_LVL, \c WARNING_LVL, \c ERROR_LVL, \c CRITICAL_LVL,
/// or \c FATAL_LVL</dd>
/// <dt>line</dt><dd>The text to be added to the log file or stream.  Empty stream will be ignored.</dd>
/// </dl>
/// @example <tt>LOG(DEBUG_LVL, "This is the text to be logged along with this integer: " + to_string(2048) + ".");</tt>

/// @def LEVEL_TO_STRING(level)
/// @brief Converts a valid level unsigned int to a std::string.
///
/// Converts a valid level unsigned int to a std::string. This will return either an empty string if the input \c level is not of of
/// the below values or the text string equivilent of the level provided.
/// <dl>
/// <dt>level</dt><dd>The logging level to convert to a string. If a value greater than \c CRITICAL_LVL
/// then \c CRITICAL_LVL will be used. This must be one of: \c DEBUG_LVL, \c INFO_LVL, \c WARNING_LVL, \c ERROR_LVL, \c CRITICAL_LVL,
/// \c FATAL_LVL, or \c DISABLED_LVL</dd>
/// </dl>

/// @def STRING_TO_LEVEL(levelString)
/// @brief Converts a valid level string to its appropriate unsigned int value.
///
/// Converts a valid level string to its appropriate unsigned int value. This will return either a valid level unsigned int or
/// <tt>std::numeric_limits<unsigned int>::max()</tt> if the string was not valid.
/// <dl>
/// <dt>levelString</dt><dd>The logging level as a string convert to the log level unsigned int value.  This must be one
/// of: \c debug, \c info, \c warning, \c error, \c critical, \c fatal, or \c disabled.</dd>
/// </dl>

namespace
{
    std::unique_ptr<micmgmt::MicLogger> sBlankLogger;

    unordered_map<unsigned int, string> sMap = {
            { micmgmt::DebugLevel,    "Debug" },
            { micmgmt::InfoLevel,     "Info" },
            { micmgmt::WarningLevel,  "Warning" },
            { micmgmt::ErrorLevel,    "Error" },
            { micmgmt::CriticalLevel, "Critical" }, // Max Name Width = 8
            { micmgmt::FatalLevel,    "Fatal" },
            { micmgmt::DisabledLevel, "Disabled" }
    };
    const size_t sMaxWidth = 8;

    unordered_map<string, unsigned int> sReverseMap = {
            { "debug", micmgmt::DebugLevel },
            { "info", micmgmt::InfoLevel },
            { "warning", micmgmt::WarningLevel },
            { "error", micmgmt::ErrorLevel },
            { "critical", micmgmt::CriticalLevel },
            { "fatal", micmgmt::FatalLevel },
            { "disabled", micmgmt::DisabledLevel }
    };

    bool sInstantiated = false;

    micmgmt::MicLogger* sThis = NULL;
    std::unique_ptr<micmgmt::MicLogger> sInitalized;
}; // Empty namespace

namespace micmgmt
{
    // For Blank MicLogger...
    MicLogger::MicLogger(LoggerLevel filterLevel)
        : internalLogger_(NULL), filterLevel_(filterLevel), currentLevel_(DisabledLevel)
    {
        if (sInstantiated == true)
        {
            throw runtime_error("Cannot create more than one instance of this class per executable");
        }
        if (filterLevel_ > DisabledLevel)
        {
            filterLevel_ = DisabledLevel;
        }
        sInstantiated = false;
        sThis = NULL;
    }

    MicLogger::MicLogger(const std::string& filename, LoggerLevel filterLevel)
        : internalLogger_(NULL), filterLevel_(filterLevel), currentLevel_(DisabledLevel)
    {
        if (sInstantiated == true)
        {
            throw runtime_error("Cannot create more than one instance of this class per executable");
        }
        if (filterLevel_ > DisabledLevel)
        {
            filterLevel_ = DisabledLevel;
        }

        string name = micmgmt::fileNameFromPath(filename);
        string::size_type pos = name.find_last_of(".");
        if (pos != string::npos)
        {
            name.erase(pos);
        }
        internalLogger_ = new InternalLogger(name);
        filename_ = internalLogger_->getBaseFilename();
#ifdef UNIT_TESTS
        std::remove(filename_.c_str());
#endif
        sInstantiated = true;
        sThis = this;
    }

    MicLogger::~MicLogger()
    {
        if (sInstantiated == true)
        {
            sInstantiated = false;
            if (internalLogger_ != NULL)
            {
                delete internalLogger_;
                internalLogger_ = NULL;
            }
            sThis = NULL;
        }
    }

    void MicLogger::log(LoggerLevel level, const std::string& line)
    {
        if (line.empty() == true)
        {
            return;
        }
        if (level >= DisabledLevel)
        {
            level = CriticalLevel;
        }

        if (internalLogger_ != NULL && level >= filterLevel_)
        {
            lock_guard<mutex> guard(lock_);

            string logLine;
            string lvl = sMap[level];
            logLine = getTimeStamp() + ": ";
            logLine += lvl;
            logLine += string(sMaxWidth - lvl.length(), ' ');
            logLine += ": " + line;

            internalLogger_->logLine(logLine);
        }
    }

// This is required because the MSVC++ has "safe" version of gmtime that are NOT standard.
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
            if (strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", pts) > 0)
            {
                return string(buf);
            }
        }
        return string();
    }
#ifdef _WIN32
#pragma warning(default:4996) // Re-enable Warning...
#endif

    LoggerLevel MicLogger::getFilterLevel() const
    {
        return filterLevel_;
    }

    void MicLogger::setFilterLevel(LoggerLevel filterLevel)
    {
        if (filterLevel > DisabledLevel)
        {
            filterLevel = DisabledLevel;
        }
        filterLevel_ = filterLevel;
    }

    bool MicLogger::isLoggingEnabled() const
    {
        return (filterLevel_ < DisabledLevel)?true:false;
    }

    // Static public accessor needed only for macros
    MicLogger& MicLogger::getThis()
    {
        if (sThis != NULL)
        {
            return *sThis;
        }
        else
        {
            if ((bool)sBlankLogger == false)
            {
                sBlankLogger = unique_ptr<MicLogger>(new MicLogger(WARNING_LVL));
            }
            return *(sBlankLogger.get());
        }
    }

    void MicLogger::Initialize(const std::string& filename)
    {
        if (micmgmt::userHomePath() == "")
        {
            throw new std::runtime_error("Logging was requested but cannot be done due to user environment restrictions: internal reference #1");
        }
        sInitalized = unique_ptr<MicLogger>(new MicLogger(filename));
    }

    std::string MicLogger::LevelToString(unsigned int level)
    {
        if (level <= DisabledLevel)
        {
            return toLower(sMap[level]);
        }
        else
        {
            return string();
        }
    }

    unsigned int MicLogger::StringToLevel(const std::string& levelStr)
    {
        string localLevelStr = toLower(levelStr);
        if (sReverseMap.count(localLevelStr) == 0)
        {
            return numeric_limits<unsigned int>::max();
        }
        else
        {
            return sReverseMap[localLevelStr];
        }
    }
#ifdef UNIT_TESTS
    void MicLogger::resetInstance()
    {
        MicLogger* current = sInitalized.release();
        if (current != NULL)
        {
            string fn = current->filename_;
            std::remove(fn.c_str());
            string fbn = fn + ".1";
            std::remove(fbn.c_str());
            fbn = fn + ".2";
            std::remove(fbn.c_str());
            fbn = fn + ".3";
            std::remove(fbn.c_str());
            fbn = fn + ".4";
            std::remove(fbn.c_str());
            fbn = fn + ".5";
            std::remove(fbn.c_str());
            delete current;
            sThis = NULL;
        }
    }
#endif
}; // namespace micmgmt
