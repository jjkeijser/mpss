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

#ifndef MICMGMT_CONSOLEOUTPUTFORMATTER_HPP
#define MICMGMT_CONSOLEOUTPUTFORMATTER_HPP

#ifdef UNIT_TESTS
#define PRIVATE public
#else
#define PRIVATE private
#endif // UNIT_TESTS

#include "MicOutputFormatterBase.hpp"
#include "MicErrorBase.hpp"
#include <iostream>

namespace micmgmt
{
    class ConsoleOutputFormatter : public MicOutputFormatterBase
    {
    PRIVATE:
        const std::string::size_type INDENT;
        const std::string::size_type NAME_LENGTH;
        const std::string::size_type COLUMNS;
        const std::string            NV_SEPERATOR;

        std::ostream*                stream_;

    private: // Disabled "special" members
        ConsoleOutputFormatter& operator=(const ConsoleOutputFormatter&);
        ConsoleOutputFormatter();
        ConsoleOutputFormatter(const ConsoleOutputFormatter&);

    public: // Public interface
        ConsoleOutputFormatter(std::ostream* outputStream, std::string::size_type nameLength = 24,
                               std::string::size_type indentSize = 4);
        virtual ~ConsoleOutputFormatter() {};

        virtual void outputLine(const std::string& line = "", bool suppressInternalWSReduction = false, bool flush = false);
        virtual void outputNameValuePair(const std::string& name,
                                         const std::string& valueStr, const std::string& units = "");
        virtual void outputError(const std::string& fullmessage, uint32_t code, bool flush = false);

        virtual void startSection(const std::string& sectionTitle, bool suppressLeadingNewline = false);

    PRIVATE: // Implementation
        void outputRawLine(const std::string& line = "", bool useIndent = true, bool flush = false);
    }; // class ConsoleOutputFormatter

}; // namespace micmgmt

#endif // MICMGMT_CONSOLEOUTPUTFORMATTER_HPP
