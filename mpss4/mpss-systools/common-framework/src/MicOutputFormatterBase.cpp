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

#include "MicOutputFormatterBase.hpp"
#include <stdexcept>

namespace micmgmt
{
    using namespace std;
    /// @class MicOutputFormatterBase MicOutputFormatterBase.hpp
    /// @ingroup  common
    /// @brief Base class for all output formatters using by the MicOuput class.
    ///
    /// This class is used by the common framework to build output formatters for diffeent targets.
    /// Two relavent examples of possible derived classes are a class for text output for the console and
    /// one for XML output to a file.

    /// @brief Constructor for the MicOuputFormatterBase class
    ///
    /// This class must be derived from because the constructor is protected and it had pure virtual functions in it.
    /// Make sure you call the base class implementations of overridden methods.
    MicOutputFormatterBase::MicOutputFormatterBase() : indentLevel_(0)
    {
    }

    /// @brief Destructor for the MicOuputFormatterBase class
    ///
    /// This will call MicOutputFormatterBase::destroy to cleanup the state by calling flushNestedSections to cleanup
    /// nested sections that the user did not cleanup.
    MicOutputFormatterBase::~MicOutputFormatterBase()
    {
        flushNestedSections();
    }

    /// @brief Starts a new section with a heading \a sectionTitle.
    /// @param [in] sectionTitle The section heading to be displayed at current level before nested level begins.
    /// @param [in] suppressLeadingNewline This will suppress the leading extra newline before starting the section.
    /// If the \a sectionTitle is empty before or after trimming then only a new line will be output.
    ///
    /// This is intended to start an indented section or a nexted level in a non-raw text formatter.
    /// The nesting level count is maintained by this base class and all overrides in derived classes should call this
    /// base class method unless they do not want to use the level tracking provided.  The base class doesn't output
    /// the sectionTitle, override to do the correct thing for your format.  Be sure to call this base class method in
    /// your implmentation.
    void MicOutputFormatterBase::startSection(const string& /* sectionTitle */, bool /*suppressLeadingNewline*/)
    {
        ++indentLevel_;
    }

    /// @brief Ends a new section decrementing the nested level.
    /// @exception std::logic_error This is thrown when too many MicOutputFormatterBase::endSection for the
    /// MicOutputFormatterBase::startSection methods.
    ///
    /// No output of any kind is perfomed by this method and it will not decrease the nesting level if already at zero.
    /// Override this method only if you are replacing the nested level tracking.  If you need to "add" functionality in
    /// a derived class then be sure to call this base class method in your implementation.
    void MicOutputFormatterBase::endSection()
    {
        if (indentLevel_ > 0)
        {
            --indentLevel_;
        }
        else
        {
            throw logic_error("Called more 'endSection' methods than 'startSection' calls!");
        }
    }

    /// @brief Calls endSection for each level not already closed by the user.
    ///
    /// This method is called by the destructor on close to make sure all nested sections are closed.
    /// There is no need to override this method as it will call the virtual endSection in the derived class.
    void MicOutputFormatterBase::flushNestedSections()
    {
        while (indentLevel_ > 0)
        {
            endSection();
        }
    }

    /// @brief Returns the current indent level count (zero based).
    /// @return a std::string::size_type representing the indent level.
    ///
    /// Returns the current indent level count (zero based).
    std::string::size_type MicOutputFormatterBase::indentLevel() const
    {
        return indentLevel_;
    }

    /* #####################################################################################################
       >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> Pure Virtual Method Documentation <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
       ##################################################################################################### */

    /// @fn MicOutputFormatterBase::outputError ( const std::string& fullMessage, MicErrorCode code, int sysErrNo )
    /// @pure
    /// @brief Must override! This is intended to output the error string to the target std::ostream.
    /// @param [in] fullMessage This is the prebuilt error message to format and output.
    /// @param [in] code This is the actual error code for the error.
    ///
    /// Implement this method to output the passed string to the target format and \c std::ostream.

    /// @fn MicOutputFormatterBase::outputLine ( const std::string& line )
    /// @pure
    /// @brief Must override! This is intended to output a single "line" to the target stream_.
    /// @param [in] line This is the line to be output.
    ///
    /// Implement this method to output the line in the format you are targeting in the derived class.

    /// @fn MicOutputFormatterBase::outputNameValuePair ( const std::string& name, const std::string& valueStr, const std::string& units )
    /// @pure
    /// @brief Must override! This is intended to output a name-value pair with optional units for the target format used.
    /// @param [in] name This is the name portion on the name-value pair.
    /// @param [in] valueStr This is the value of the name-value pair.
    /// @param [in] units This is the optional (default empty string) of the units for the \a valueStr.
    ///
    /// Implement this method to output a name-value pair (with optional units) to your targeted format.

}; // namespace micmgmt
