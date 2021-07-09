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
#include "ConsoleOutputFormatter.hpp"
#include <algorithm>
#include <stdexcept>

namespace micmgmt
{
    using namespace std;
    /// @class ConsoleOutputFormatter ConsoleOutputFormatter.hpp
    /// @ingroup  common
    /// @brief Class for writing all text formatted output to the console used by the MicOuput class.
    ///
    /// This class is used by the common framework to output textual data to the console.

    /// @brief Constructor for text output class.
    /// @param [in] outputStream This is the target stream to write the formatted output to.
    /// @param [in] nameLength [Optional; Default=<tt>24</tt>] This represents how many characters the name portion of the name-value pair will consume of the line.
    /// @param [in] indentSize [Optional; Default=<tt>4</tt>] This is the indent size used for the nested sections, each section is indented by this much relative.
    /// to the previous section.
    ///
    /// This constructs an object that formats the output for the console.  This class does NOT take ownership of the ostream passed.
    ConsoleOutputFormatter::ConsoleOutputFormatter(std::ostream* outputStream, std::string::size_type nameLength, std::string::size_type indentSize)
        : MicOutputFormatterBase(), INDENT(indentSize), NAME_LENGTH(nameLength), COLUMNS(79), NV_SEPERATOR(" : "), stream_(outputStream)

    {
        if (stream_ == NULL)
        {
            stream_ = &cout;
        }
    }

    /// @brief Formats and writes a line to the target ostream object.
    /// @param [in] line The string to format and output with a ending \c std:endl.
    /// @param [in] suppressInternalWSReduction Skip reducing inner text whitespace in a line.
    /// @param [in] flush indicates whether a endl is applied instead of a flush
    ///
    /// This implementation will use the micmgmt::wrap to make sure that the maximum column width is not surpassed to display the line.
    void ConsoleOutputFormatter::outputLine(const string& line, bool suppressInternalWSReduction, bool flush)
    {
        string localLine;
        if(flush){
            localLine = line;
        }
        else{
            localLine = trim(line);
        }
        //Disable multiline print in case flush is active
        if (line.length() > (COLUMNS - (INDENT * indentLevel())) && flush == false)
        {
            vector<string> lines = wrap(localLine, (COLUMNS - (INDENT * indentLevel())), suppressInternalWSReduction);
            for (auto it = lines.begin(); it != lines.end(); ++it)
            {
                outputRawLine(*it,true,flush);
            }
        }
        else
        {
            outputRawLine(localLine,true,flush);
        }
    }

    /// @brief Formats and writes a line containing the name-value and optional units.
    /// @param [in] name The name portion of the name-value pair.  This string will be trimmed.
    /// @param [in] valueStr The value portion of the name-value pair.  This string will NOT be trimmed.
    /// @param [in] units The optional units for the \a valueStr. Default is empty string and this string will be trimmed.
    ///
    /// This method will output the name-value pair with the name field starting at current indent and right justified to nameLength
    /// specified in the constructor.  This will be followed by the string \c " : ", then the value will be output.  If there is a
    /// non-empty units this will be printed immediately following the value (no space).
    ///
    /// @exception invalid_argument Thrown when the trimmed \n name field is empty.
    /// @exception overflow_error Thrown when the total line length will exceed the max column width specified in the constructor.
    void ConsoleOutputFormatter::outputNameValuePair(const string& name, const string& valueStr, const string& units)
    {
        string sp_units = (units.length() == 0) ? units : " " + units;
        string localName = trim(name);
        if (localName.empty() == true)
        {
            throw invalid_argument("'name' cannot be empty or all whitespace.");
        }
        string::size_type totalLength = max<string::size_type>(localName.length(), NAME_LENGTH) + valueStr.length() + sp_units.length() + (INDENT * indentLevel()) + NV_SEPERATOR.length();
        if (totalLength > COLUMNS)
        {
            throw overflow_error("Name value pair will exceed maximum line length.");
        }
        string::size_type padding = 0;
        if (localName.length() < NAME_LENGTH)
        {
            padding = NAME_LENGTH - localName.length();
        }
        string line = localName + string(padding, ' ') + NV_SEPERATOR + valueStr + sp_units;
        outputRawLine(line);
    }


    /// @brief This method will wrap the error message as needed and will ignore indents.
    /// @param [in] fullmessage The full error string that is needing to be written to the target std::ostream.
    /// @param [in] code Unused in this class, see base class MicOutputFormatter::outputError.
    /// @param [in] flush indicates whether a endl is applied instead of a flush
    ///
    /// This method is for wrapping and writing the error string to the output stream.
    void ConsoleOutputFormatter::outputError(const std::string& fullmessage, uint32_t /*code*/, bool flush)
    {
        string localMsg;
        if(flush){
            localMsg = fullmessage;
        }
        else{
            localMsg = trim(fullmessage);
        }

        if (localMsg.length() > COLUMNS && flush == false)
        {
            vector<string> lines = wrap(localMsg, COLUMNS);
            for (auto it = lines.begin(); it != lines.end(); ++it)
            {
                outputRawLine(*it, false, flush);
            }
        }
        else
        {
            outputRawLine(localMsg, false, flush);
        }
    }

    /// @brief This overrides (but calls) the base class implementation.
    /// @param [in] sectionTitle This is the section name output at the current section before entering the nested section.
    /// @param [in] suppressLeadingNewline This will suppress the leading extra newline before starting the section.
    ///
    /// This override will print a blank line, followed by the section name,then increases the indent and section level.
    void ConsoleOutputFormatter::startSection(const std::string& sectionTitle, bool suppressLeadingNewline)
    {
        if (sectionTitle.empty() == false)
        {
            if (suppressLeadingNewline == false)
            {
                outputRawLine();
            }
            outputRawLine(trim(sectionTitle) + ":");
        }
        MicOutputFormatterBase::startSection(sectionTitle, suppressLeadingNewline);
    }

    void ConsoleOutputFormatter::outputRawLine(const std::string& line, bool useIndent, bool flush)
    {
        string::size_type indent = indentLevel() * INDENT * (useIndent?1:0);
        if ((line.length() + indent) > COLUMNS && flush == false)
        {
            throw std::out_of_range("micmgmt::ConsoleOutputFormatter::outputRawLine: Calculated line length greater than maximum width!");
        }
        if (line.length() == 0)
        {
            if(flush)
                (*stream_) << std::flush;
            else
                (*stream_) << endl;
        }
        else
        {
            if(flush)
                (*stream_) << string(indent, ' ') << line << std::flush;
            else
                (*stream_) << string(indent, ' ') << line << endl;
        }
    }

}; // namespace micmgmt
