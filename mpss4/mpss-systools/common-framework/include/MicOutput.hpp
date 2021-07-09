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

#ifndef MICMGMT_MICOUTPUT_HPP
#define MICMGMT_MICOUTPUT_HPP

#ifdef UNIT_TESTS
#define PRIVATE public
#else
#define PRIVATE private
#endif // UNIT_TESTS

#include <memory>
#include <sstream>
#include <string>

namespace micmgmt
{
    // Forward References
    class MicOutputImpl;
    class MicOutputFormatterBase;

    class MicOutput
    {
    private: // Hide some special methods and operators we do NOT wish to support!
        MicOutput();
        MicOutput(const MicOutput&);
        MicOutput& operator=(const MicOutput&);

    PRIVATE: // PIMPL
        std::unique_ptr<MicOutputImpl>  impl_;

    public:
        MicOutput(const std::string& toolName, MicOutputFormatterBase* stdOut = NULL, MicOutputFormatterBase* stdErr = NULL,
                  MicOutputFormatterBase* fileOut = NULL);

        ~MicOutput();

        void outputLine(const std::string& line = "", bool suppressInternalWSReduction = false, bool flush = false);
        void outputNameValuePair(const std::string& name, const std::string& valueStr, const std::string& units = "");
        template <class Value>
        void outputNameMicValuePair(const std::string& name, const Value& micValue,
                  const std::string& units = "", const std::string& caseInvalid = "Not Available");
        void outputError(const std::string& fullmessage, uint32_t code, bool flush = false);
        void startSection(const std::string& sectionTitle, bool suppressLeadingNewline = false);
        void endSection();

        bool isSilent() const;
        void setSilent(bool silent);
    }; // class MicOutput

    /// @brief This method outputs a line containing the name-MicValue pair with optional units to the target
    ///        \c std::ostream formatted with the \c std::cout formatter. Internally, the isValid() member
    ///        of the \c micValue is called. If it returns true, the value of \c micValue is used, otherwise,
    ///        the optional argument caseInvalid is used.
    /// @param [in] name The name portion of the name-value pair.
    /// @param [in] micValue The MicValue instance of the name-value pair.
    /// @param [in] units The optional units for the \a valueStr. Default is empty string.
    /// @param [in] caseInvalid The optional string in case \a micValue is invalid. Default is "Not Available".
    ///
    template <class Value>
    void MicOutput::outputNameMicValuePair(const std::string& name, const Value& micValue,
            const std::string& units, const std::string& caseInvalid)
    {
        std::stringstream value;
        value << (micValue.isValid() ? micValue.value() : caseInvalid);
        this->outputNameValuePair(name, value.str(), units);
    }

}; // namespace micmgmt

#endif // MICMGMT_MICOUTPUT_HPP
