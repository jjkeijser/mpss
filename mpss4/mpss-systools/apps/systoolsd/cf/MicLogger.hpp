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

#ifndef MICMGMT_MICLOGGER_HPP
#define MICMGMT_MICLOGGER_HPP

#include <ostream>
#include <mutex>
#include <memory>

#ifdef UNIT_TESTS
#define PRIVATE public
#else
#define PRIVATE private
#endif // UNIT_TESTS

namespace micmgmt
{
    const unsigned int DebugLevel    = 0;
    const unsigned int InfoLevel     = 1;
    const unsigned int WarningLevel  = 2;
    const unsigned int ErrorLevel    = 3;
    const unsigned int CriticalLevel = 4;
    const unsigned int FatalLevel    = 5;
    const unsigned int DisabledLevel = 6;

    typedef unsigned int LoggerLevel;

    class MicLogger;
    class InternalLogger;

    class MicLogger
    {
    PRIVATE:
        std::string        filename_;
        InternalLogger*    internalLogger_;
        LoggerLevel        filterLevel_;
        LoggerLevel        currentLevel_;
        std::mutex         lock_;

    private: // no default, copy, or copy operator
        MicLogger(const MicLogger&);
        MicLogger& operator=(const MicLogger&);

    PRIVATE: // construction
        explicit MicLogger(LoggerLevel filterLevel = WarningLevel);
        MicLogger(const std::string& file, LoggerLevel filterLevel = WarningLevel);

    public: // destruction
        ~MicLogger();

    public: // API
        LoggerLevel getFilterLevel() const;
        void setFilterLevel(LoggerLevel filterLevel);
        bool isLoggingEnabled() const;
        void log(LoggerLevel level, const std::string& line);

        // Static
        static MicLogger& getThis();
        static void Initialize(const std::string& filename);
        static std::string LevelToString(unsigned int level);
        static unsigned int StringToLevel(const std::string& levelStr);
#ifdef UNIT_TESTS
        static void resetInstance();
#endif

    private:
        std::string getTimeStamp() const;
        bool isInstantiated() const;
    }; // class MicLogger
}; // namespace micmgmt

// Global Macros equivilents to avoid passing the logger around.
#define DEBUG_LVL     micmgmt::DebugLevel
#define INFO_LVL      micmgmt::InfoLevel
#define WARNING_LVL   micmgmt::WarningLevel
#define ERROR_LVL     micmgmt::ErrorLevel
#define CRITICAL_LVL  micmgmt::CriticalLevel
#define FATAL_LVL     micmgmt::FatalLevel
#define DISABLED_LVL  micmgmt::DisabledLevel

// *** INTENDED GLOBAL API FOR USE IN TOOLS AND LIBRARIES (USE WITH LEVELS ABOVE)
#define INIT_LOGGER(filename) micmgmt::MicLogger::Initialize(filename)
#define SET_LOGGER_LEVEL(level) micmgmt::MicLogger::getThis().setFilterLevel(level)
#define LOG(level,line) micmgmt::MicLogger::getThis().log(level,line)
#define LEVEL_TO_STRING(level) micmgmt::MicLogger::LevelToString(level)
#define STRING_TO_LEVEL(levelString) micmgmt::MicLogger::StringToLevel(levelString)
#endif // MICMGMT_MICLOGGER_HPP
