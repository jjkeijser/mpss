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

#ifndef MICMGMT_MICOUTPUTFORMATTERBASE_HPP
#define MICMGMT_MICOUTPUTFORMATTERBASE_HPP

#include "MicErrorBase.hpp"
#include <string>

namespace micmgmt
{
    class MicOutputFormatterBase
    {
    private:
        std::string::size_type       indentLevel_;

    protected:
        MicOutputFormatterBase();

    public:
        virtual ~MicOutputFormatterBase();

        virtual void outputLine(const std::string& line, bool suppressInternalWSReduction, bool flush) = 0;
        virtual void outputNameValuePair(const std::string& name, const std::string& valueStr, const std::string& units) = 0;
        virtual void outputError(const std::string& fullMessage, uint32_t code, bool flush) = 0;
        virtual void startSection(const std::string& sectionTitle, bool suppressLeadingNewline) = 0;
        virtual void endSection();

        void flushNestedSections();
        std::string::size_type indentLevel() const;
    }; // class MicOutputFormatterBase

}; // namespace micmgmt

#endif // MICMGMT_MICOUTPUTFORMATTERBASE_HPP
