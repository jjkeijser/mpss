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

#ifndef MICMGMT_XMLOUTPUTFORMATTER_HPP
#define MICMGMT_XMLOUTPUTFORMATTER_HPP

#ifdef UNIT_TESTS
#define PRIVATE public
#else
#define PRIVATE private
#endif // UNIT_TESTS

#include "MicOutputFormatterBase.hpp"
#include "MicErrorBase.hpp"
#include <iostream>
#include <stack>

namespace micmgmt
{
    class XmlOutputFormatter : public MicOutputFormatterBase
    {
    PRIVATE:
        std::ostream&                stream_;

    private: // Disabled "special" members
        XmlOutputFormatter& operator=(const XmlOutputFormatter&);
        XmlOutputFormatter();
        XmlOutputFormatter(const XmlOutputFormatter&);

    PRIVATE:
        std::string getXmlTime() const;
        std::string convertToXmlString(const std::string& str) const;
        bool validXmlAttribute(const std::string& attr) const;

    public:
        XmlOutputFormatter(std::ostream* outputStream, const std::string& toolName);

        virtual ~XmlOutputFormatter();

        virtual void outputLine(const std::string& line = "", bool suppressInternalWSReduction = false, bool flush = false);
        virtual void outputNameValuePair(const std::string& name, const std::string& valueStr, const std::string& units = "");
        virtual void outputError(const std::string& fullmessage, uint32_t code, bool flush = false);
        virtual void startSection(const std::string& sectionTitle, bool suppressLeadingNewline = false);
        virtual void endSection();
    }; // class XmlOutputFormatter

}; // namespace micmgmt

#endif // MICMGMT_XMLOUTPUTFORMATTER_HPP
