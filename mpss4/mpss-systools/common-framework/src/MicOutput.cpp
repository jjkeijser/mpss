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

#include "MicOutput.hpp"
#include "MicOutputImpl.hpp"
#include "ConsoleOutputFormatter.hpp"
#include "MicException.hpp"
#include <iostream>

namespace micmgmt
{
    using namespace std;
    /// @class MicOutput MicOutput.hpp
    /// @ingroup  common
    /// @brief Class to control all output to \c std::cout, \c std::cerr, and a optional file using formatters derived
    /// from MicOutputFormatterBase.
    ///
    /// This class in intended to abstract out all non-logging output to console and files for a given tool.

    /// @brief Constructor that takes raw pointers to 3 formatters to control non-loggin output.
    /// @param [in] toolName Name used when building error strings. Represents the tool or application name. Can be empty.
    /// @param [in] stdOut This formatter is used for everything intended to go to \c std::cout.  Methods MicOutput::outputLine and
    /// MicOutput::outputNameValuePair primarily use this formatter (default = \c std::cout).
    /// @param [in] stdErr This formmater is used for everything intended to go to \c std::cerr.  Method MicOutput::outputError
    /// primarily uses this formatter (default = \c std::cerr).
    /// @param [in] fileOut Optional. This formatter catpture everything from methods MicOutput::outputLine, MicOutput::outputNameValuePair,
    /// and MicOutput::outputError (default == \c NULL).
    ///
    /// This class should be used to control ALL non-logging output to console and output files.
    ///
    /// @note This class will <b>NOT</b> take  ownership of the formatter pointers passed into it.  The pointers are stored raw and
    /// only used by the class.
    MicOutput::MicOutput(const std::string& toolName, MicOutputFormatterBase* stdOut, MicOutputFormatterBase* stdErr,
                         MicOutputFormatterBase* fileOut)
        : impl_(new MicOutputImpl(toolName, stdOut, stdErr, fileOut))
    {
    }

    /// @brief Unused destructor.
    ///
    /// Unused destructor.
    MicOutput::~MicOutput()
    {

    }

    /// @brief This method outputs a line of text to the target \c std::ostream foramatted with the \c std::cout formatter.
    /// @param [in] line The line to format and write to the target \c std::ostream.
    /// @param [in] suppressInternalWSReduction Skip reducing inner text whitespace in a line.
    /// @param [in] flush determine whether endl or flush is applied at the end of the string.
    ///
    /// This is the primary output method for \c std::cout.
    void MicOutput::outputLine(const string& line, bool suppressInternalWSReduction, bool flush)
    {
        impl_->outputLine(line, suppressInternalWSReduction, flush);
    }

    /// @brief This method outputs a line containing the name-value pair with optional units to the target \c std::ostream formatted with the \c std::cout formatter.
    /// @param [in] name The name portion of the name-value pair.
    /// @param [in] valueStr The value portion of the name-value pair.
    /// @param [in] units The optional units for the \a valueStr. Default is empty string.
    ///
    /// This is the primary output method for name-value pair data.
    void MicOutput::outputNameValuePair(const string& name, const string& valueStr, const string& units)
    {
        impl_->outputNameValuePair(name, valueStr, units);
    }

    /// @brief This method will output the error message to the \c std::cerr formatter.
    /// @param [in] fullMessage [Required] Full error message that will be written to the target \c std::ostream.
    /// @param [in] code [Required] The globally created unique error code from MicErrorBase::buildError or
    /// @param [in] flush determine whether endl or flush is applied at the end of the string.
    /// a derived class.
    ///
    /// This method is for wrapping and writing the error string to the output stream for \c std::cerr.
    void MicOutput::outputError(const std::string& fullMessage, uint32_t code, bool flush)
    {
        impl_->outputError(fullMessage, code, flush);
    }

    /// @brief This starts a new nested section level.
    /// @param [in] sectionTitle This is the section name for the nested section.
    /// @param [in] suppressLeadingNewline This will suppress the leading extra newline before starting the section.
    ///
    /// This override will print a blank line, followed by the section name,then increases the indent and section level.
    void MicOutput::startSection(const string& sectionTitle, bool suppressLeadingNewline)
    {
        impl_->startSection(sectionTitle, suppressLeadingNewline);
    }

    /// @brief Ends a new section decrementing the nested level.
    ///
    /// This simply calls the correct formatter MicOutputFormatterBase::endSection methods on the supplied formatters.
    void MicOutput::endSection()
    {
        impl_->endSection();
    }

    /// @brief returns the value of the "silent" flag that determines is output is sent to the \c std::cout and \c std::cerr formatters.
    /// @return Returns \c true if output is suppressed or \c false if output is turned on.
    ///
    /// The "silent" flag does not affect the file formatter.
    bool MicOutput::isSilent() const
    {
        return impl_->isSilent();
    }

    /// @brief Sets the "silent" flag that  determines is output is sent to the \c std::cout and \c std::cerr formatters.
    /// @param [in] silent This flag should be \c true for suppress output to \c std::cout and \c std::cerr or \c false to allow writing to those stream.
    ///
    /// The "silent" flag does not affect the file formatter.
    void MicOutput::setSilent(bool silent)
    {
        impl_->setSilent(silent);
    }

}; // namespace micmgmt
