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

#ifndef MICMGMT_CLIPARSER_HPP
#define MICMGMT_CLIPARSER_HPP

#include <vector>
#include <memory>
#include <string>

#ifdef UNIT_TESTS
#define PRIVATE public
#else
#define PRIVATE private
#endif

namespace micmgmt
{
    // Forward references
    class MicOutputFormatterBase;
    class EvaluateArgumentCallbackInterface;
    class CliParserImpl;

    class CliParser
    {
    PRIVATE: // PIPML
        std::unique_ptr<CliParserImpl> impl_;

    private: // Hide some special methods and operators we do NOT wish to support!
        CliParser();
        CliParser(const CliParser&);
        CliParser& operator=(const CliParser&);

    public:
        CliParser(int argc, char* argv[], const std::string& toolVersion, const std::string& publishYear,
                  const std::string& programName = "", const std::string& shortDescription = "");

        ~CliParser();

        // Setup parser for parsing...
        void addOption(const std::string& name, const std::vector<std::string>& help,
                       const char& shortLetter, const bool& requiredArgument,
                       const std::string& argumentName, const std::vector<std::string>& argumentHelp,
                       const std::string& defVal, EvaluateArgumentCallbackInterface* callback = NULL);

        void addToolPositionalArg(const std::string& name, const std::vector<std::string>& help,
                                  EvaluateArgumentCallbackInterface* callback = NULL);

        void addSubcommand(const std::string& subcommand, const std::vector<std::string>& help);

        void addSubcommandPositionalArg(const std::string& subcommandName, const std::string& argName,
                                        const std::vector<std::string>& help,
                                        EvaluateArgumentCallbackInterface* callback = NULL);
        void addHelpSection(const std::string& sectionName, const std::vector<std::string>& paragraphs);

        // Parse Method
        bool parse(bool allowGeneralArguments = false);

        // Parsed command line data access methods...
        bool parseError() const;
        bool parsed() const;
        const std::string& parsedSubcommand() const;
        std::tuple<bool, bool, std::string> parsedOption(const std::string& optionName) const;
        std::vector<std::string> parsedOptionArgumentAsVector(const std::string& optionName, const std::string& prefix = "") const;
        std::vector<std::string> parsedPositionalArgumentAsVector(std::vector<std::string>::size_type index, const std::string& prefix = "") const;
        std::vector<std::string> parsedOptionArgumentAsDeviceList(const std::string& optionName) const;
        std::vector<std::string>::iterator parsedPositionalArgumentBegin();
        std::vector<std::string>::iterator parsedPositionalArgumentEnd();
        const std::string& parsedPositionalArgumentAt(std::vector<std::string>::size_type index) const;
        std::vector<std::string> parsedPositionalArgumentAsDeviceList(std::vector<std::string>::size_type index) const;
        std::vector<std::string>::size_type parsedPositionalArgumentSize();
        const std::string& toolName() const;

        // Parser output methods...
        void setOutputFormatters(MicOutputFormatterBase* stdOut = NULL,
                                 MicOutputFormatterBase* stdErr = NULL,
                                 MicOutputFormatterBase* fileOut = NULL);
        void outputVersion() const;
        void outputHelp(const std::string& subcommandName = "");
    };

}; // namespace micmgmt

#endif // MICMGMT_CLIPARSER_HPP
