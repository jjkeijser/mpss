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

#include "micmgmtCommon.hpp"
#include "XmlOutputFormatter.hpp"
#include <stdexcept>
#include <iomanip>
#include <chrono>

namespace micmgmt
{
    using namespace std;

    /// @class XmlOutputFormatter XmlOutputFormatter.hpp
    /// @ingroup  common
    /// @brief Class for writing all text formatted output to a XML formatted stream used by the MicOuput class.
    ///
    /// This class is used by the common framework to output xml data to a \c std::ostream.

    /// @brief Constructor for XML output class.
    /// @param [in] outputStream This is the target stream to write the formatted output to.
    /// @param [in] toolName Tool name to be used in xml output document element.
    /// @exception std::invalid_argument This is thrown if the tool name is not a valid XML name.
    ///
    /// This constructs an object that formats the output in XML and writes it to a \c std::ostream.  This class does
    /// NOT take ownership of the ostream passed.
    XmlOutputFormatter::XmlOutputFormatter(std::ostream* outputStream, const std::string& toolName) : stream_(*outputStream)
    {
        string localToolName = trim(toolName);
        if (verifyCanonicalName(localToolName) == false)
        {
            throw invalid_argument("XmlOutputFormatter: constructor: toolName is not a real XML compliant name");
        }
        stream_ << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone='yes'?><tool";
        stream_ << " toolname=\"" << toolName << "\" timestamp=\"" << getXmlTime() << "\">";
    }

    XmlOutputFormatter::~XmlOutputFormatter()
    {
        flushNestedSections();
        stream_ << "</tool>";
    }

    /// @brief Formats as XML and writes to the target ostream object.
    /// @param [in] line The string to format and output with XML tagging.
    ///
    /// This implementation ignore pretty printing of the XML and will instead make the smallest possible file.
    /// The format will be: &lt;line&gt;line text&lt;/line&gt;
    void XmlOutputFormatter::outputLine(const std::string& line, bool /*suppressInternalWSReduction*/, bool /*flush*/)
    {
        string localLine = trim(line);
        if (localLine.empty() == true)
        {
            stream_ << "<line />";
        }
        else
        {
            stream_ << "<line>" << convertToXmlString(localLine) << "</line>";
        }
    }

    /// @brief Formats and writes XML containing the name-value and optional units.
    /// @param [in] name The name portion of the name-value pair.  This string will be trimmed.
    /// @param [in] valueStr The value portion of the name-value pair.  This string will NOT be trimmed.
    /// @param [in] units The optional units for the \a valueStr. Default is empty string and this string will be trimmed.
    ///
    /// This method will create a XML tag pair with the name-value pair in the form:
    /// &lt;data name="\c name" units="\c units"&gt;valueStr&lt;/data&gt;
    ///
    /// @exception invalid_argument Thrown when the trimmed \n name field is empty or \a units or \a name contain a '<' character.
    void XmlOutputFormatter::outputNameValuePair(const std::string& name, const std::string& valueStr, const std::string& units)
    {
        string localName = trim(name);
        if (localName.empty() == true || validXmlAttribute(localName) == false)
        {
            throw invalid_argument("'name' cannot be empty or all whitespace or contain a '<' character");
        }
        string localUnits = trim(units);
        if (validXmlAttribute(localUnits) == false)
        {
            throw invalid_argument("The units paramater cannot contain a '<' character");
        }
        stream_ << "<data name=\"" << localName << "\"";
        if (localUnits.empty() == false)
        {
            stream_ << " units=\"" << localUnits << "\"";
        }
        string localValueStr = trim(valueStr);
        if (localValueStr.empty() == true)
        {
            stream_ << " />";
        }
        else
        {
            stream_ << ">" << convertToXmlString(localValueStr) << "</data>";
        }
    }

    /// @brief This method will wrap the error message in an XML tag pair.
    /// @param [in] fullmessage The error message to be written to the XML stream.
    /// @param [in] code The error code to be written to the XML stream.
    ///
    /// This method will wrap the error message in an XML tag pair.  The format will be: &lt;error&gt;error message&lt;/error&gt;
    void XmlOutputFormatter::outputError(const std::string& fullmessage, uint32_t code, bool /*flush*/)
    {
        stream_ << "<error";
        if (code != MicErrorCode::eSuccess)
        {
            stream_ << " code=\"0x" << hex << setfill('0') << setw(8) << code << dec << setfill(' ') << setw(0) << "\"";
        }
        stream_ << ">" << convertToXmlString(fullmessage) << "</error>";
    }

    /// @brief This overrides (but calls) the base class implementation.
    /// @param [in] sectionTitle This is the section name output at the current section before entering the nested section.
    /// @param [in] suppressLeadingNewline This is not used by this class.
    /// @exception std::invalid_argument This is thrown when the passed section title contains a '<' character.  This is
    /// forbidden by the XML standard.
    ///
    /// This override will output and stack the name to be popped on a call to XmlOutputFormatter::endSection.
    void XmlOutputFormatter::startSection(const std::string& sectionTitle, bool /*suppressLeadingNewline*/)
    {
        MicOutputFormatterBase::startSection(sectionTitle, false);

        string localSectionName = trim(sectionTitle);
        if (validXmlAttribute(localSectionName) == false)
        {
            throw invalid_argument("The section name cannot contain a '<' character");
        }
        if (localSectionName.empty() == false)
        {
            stream_ << "<section name=\"" << localSectionName << "\">";
        }
        else
        {
            stream_ << "<section>";
        }
    }

    /// @brief This overrides (but calls) the base class implementation.
    /// @exception std::logic_error This is thrown when too many MicOutputFormatterBase::endSection for the
    /// MicOutputFormatterBase::startSection methods.
    ///
    /// This override will pop the stacked the name and send the closing XML tag pair to the std::ostream.
    void XmlOutputFormatter::endSection()
    {
        MicOutputFormatterBase::endSection();
        stream_ << "</section>";
    }

    //########################################################################
    //# Private Implementation
    //########################################################################
#ifdef _WIN32
#pragma warning(disable:4996) // Disable warning...
#endif
    std::string XmlOutputFormatter::getXmlTime() const
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

    std::string XmlOutputFormatter::convertToXmlString(const std::string& str) const
    {
        string localStr = trim(str);
        if (localStr.empty() == true)
        {
            return localStr;
        }
        string::size_type pos = localStr.find_first_of("&<>'\"");
        while (pos != string::npos)
        {
            if (localStr[pos] == '&' && localStr.compare(pos, 5, "&amp;") != 0 && localStr.compare(pos, 4, "&lt;") != 0 &&
                localStr.compare(pos, 4, "&gt;") != 0 && localStr.compare(pos, 6, "&apos;") && localStr.compare(pos, 6, "&quot;"))
            {
                localStr.erase(pos, 1);
                localStr.insert(pos, "&amp;");
            }
            else if (localStr[pos] == '<')
            {
                localStr.erase(pos, 1);
                localStr.insert(pos, "&lt;");
            }
            else if (localStr[pos] == '>')
            {
                localStr.erase(pos, 1);
                localStr.insert(pos, "&gt;");
            }
            else if (localStr[pos] == '\'')
            {
                localStr.erase(pos, 1);
                localStr.insert(pos, "&apos;");
            }
            else if (localStr[pos] == '"')
            {
                localStr.erase(pos, 1);
                localStr.insert(pos, "&quot;");
            }
            pos = localStr.find_first_of("&<>'\"", pos + 1);
        }
        return localStr;
    }

    bool XmlOutputFormatter::validXmlAttribute(const std::string& attr) const
    {
        if (attr.find("<") != string::npos)
        {
            return false;
        }
        return true;
    }
}; // namespace micmgmt
