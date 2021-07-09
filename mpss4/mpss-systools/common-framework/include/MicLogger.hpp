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
    enum MessageType{
        DebugMsg = 0,
        InfoMsg,
        WarningMsg,
        ErrorMsg,
        CriticalMsg,
        FatalMsg,
    };

    class MicLogger;
    class InternalLogger;

    class MicLogger
    {
    PRIVATE:
        std::mutex         lock_;
        std::ostream*      out_;

    private: // no default, copy, or copy operator
        MicLogger(const MicLogger&);
        MicLogger& operator=(const MicLogger&);

    PRIVATE: // construction
        explicit MicLogger();

    public: // destruction
        ~MicLogger();

    public: // API
        void vlog(MessageType type, const char* line, va_list arg);
        void log(MessageType type, const char* line, ...);
        void log(MessageType type, std::string line, ...);

        // Static
        static MicLogger& getInstance();
        void initialize(std::ostream* ostr);
        void reset();

    private:
        std::string getTimeStamp() const;
    }; // class MicLogger
}; // namespace micmgmt

// Global Macros equivalents to avoid passing the logger around.
#define DEBUG_MSG     micmgmt::DebugMsg
#define INFO_MSG      micmgmt::InfoMsg
#define WARNING_MSG   micmgmt::WarningMsg
#define ERROR_MSG     micmgmt::ErrorMsg
#define CRITICAL_MSG  micmgmt::CriticalMsg
#define FATAL_MSG     micmgmt::FatalMsg

// *** INTENDED GLOBAL API FOR USE IN TOOLS AND LIBRARIES
#define INIT_LOGGER(out) micmgmt::MicLogger::getInstance().initialize(out)
#define LOG(type, format, ...) micmgmt::MicLogger::getInstance().log(type, format, ##__VA_ARGS__)
#define RESET_LOGGER() micmgmt::MicLogger::getInstance().reset()
#endif // MICMGMT_MICLOGGER_HPP
