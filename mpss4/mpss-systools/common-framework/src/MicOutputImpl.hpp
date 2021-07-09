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

#ifndef MICMGMT_MICOUTPUTIMPL_HPP
#define MICMGMT_MICOUTPUTIMPL_HPP

#ifdef UNIT_TESTS
#define PRIVATE public
#else
#define PRIVATE private
#endif // UNIT_TESTS

#include "MicOutputError_p.hpp"
#include <memory>
#include <string>
#include <mutex>

namespace micmgmt
{
    // Forward references
    class MicOutputFormatterBase;
    class ConsoleOutputFormatter;

    class MicOutputImpl
    {
    PRIVATE:
        std::unique_ptr<ConsoleOutputFormatter> stdOut_;
        std::unique_ptr<ConsoleOutputFormatter> stdErr_;
        bool                                    silentMode_;
        MicOutputFormatterBase*                 standardOut_;
        MicOutputFormatterBase*                 standardErr_;
        MicOutputFormatterBase*                 fileOut_;
        MicOutputError                          errorFormatter_;
        std::mutex                              outputMutex_;

    public:
        MicOutputImpl(const std::string& toolName, MicOutputFormatterBase* stdOut = NULL, MicOutputFormatterBase* stdErr = NULL,
                      MicOutputFormatterBase* fileOut = NULL);

        ~MicOutputImpl();

        void outputLine(const std::string& line, bool suppressInternalWSReduction, bool flush);
        void outputNameValuePair(const std::string& name, const std::string& valueStr, const std::string& units = "");
        void outputError(const std::string& fullmessage, uint32_t code, bool flush);
        void startSection(const std::string& sectionTitle, bool suppressLeadingNewline = false);
        void endSection();

        bool isSilent();
        void setSilent(bool silent);
    }; // class MicOutput
}; // namespace micmgmt

#endif // MICMGMT_MICOUTPUTIMPL_HPP
