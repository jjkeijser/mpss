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

#ifndef MICMGMT_CLIPARSERIMPL_HPP
#define MICMGMT_CLIPARSERIMPL_HPP

#ifdef UNIT_TESTS
#define PRIVATE public
#else
#define PRIVATE private
#endif // UNIT_TESTS

#include "CliParserError_p.hpp"
#include "MicException.hpp"
#include "OptionDef_p.hpp"
#include "SubcommandDef_p.hpp"
#include <vector>
#include <memory>
#include <utility>
#include <unordered_map>

namespace micmgmt
{
    // Forward references
    class MicOutput;
    class MicOutputFormatterBase;
    class EvaluateArgumentCallbackInterface;

    class CliParserImpl
    {
    PRIVATE: // Constants
        const std::string LONG_SEP;
        const std::string SHORT_SEP;
        const std::string LIST_SEP;
        const std::string RANGE_SEP;

    private: // Typedefs
        typedef std::unordered_map<std::string, std::pair<bool, std::string>> ParsedOptionMap;
        typedef std::vector<std::pair<std::string,std::vector<std::string>>>  ExtraSectionVector;

    PRIVATE: // Fields
        ArgumentDefVector                       toolPositionalDefinitions_;
        OptionDefMap                            optionDefinitions_;
        SubcommandDefMap                        subcommandDefinitions_;
        ExtraSectionVector                      extraHelpSections_;
        MicOutput*                              output_;
        std::unique_ptr<CliParserError>         errorFormatter_;

        std::vector<std::string>                rawArguments_;

        std::string                             version_;
        std::string                             copyright_;
        bool                                    parsed_;
        bool                                    parseError_;
        std::string                             programName_;
        std::string                             subcommand_;
        std::vector<std::string>                arguments_;
        std::string                             description_;
        ParsedOptionMap                         options_;
        bool                                    disallowMultiOption_;
        bool                                    allowGeneralArguments_;

    private: // Hide some special methods and operators we do NOT wish to support!
        CliParserImpl();
        CliParserImpl(const CliParserImpl&);
        CliParserImpl& operator=(const CliParserImpl&);

    public:
        CliParserImpl(int argc, char* argv[], const std::string& toolVersion, const std::string& copyrightCompany,
                      const std::string& programName = "", const std::string& shortDescription = "");

        ~CliParserImpl();

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
        bool parseError() const;

        // Parsed command line data access methods...
        bool parsed() const;
        const std::string& parsedSubcommand() const;
        std::tuple<bool, bool, std::string> parsedOption(const std::string& optionName) const;
        std::vector<std::string> parsedOptionArgumentAsVector(const std::string& optionName, const std::string& prefix = "") const;
        std::vector<std::string> parsedPositionalArgumentAsVector(std::vector<std::string>::size_type index, const std::string& prefix = "") const;
        std::vector<std::string> parsedOptionArgumentAsDeviceList(const std::string& optionName);
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

    PRIVATE: // Internal Implementation
        bool validUnsigned32bitNumber(const std::string& sNumber, uint32_t* value = NULL) const;

        void exitApplication(ParserErrorCode errorCode = ParserErrorCode::eParserSuccess, const std::string& extra = "", const std::string& optionContext = "",
                             const std::string& subcommandContext = "");
        void parseArgument(std::vector<std::string>::iterator& argIterator);
        void processLongOption(const std::string& option, std::vector<std::string>::iterator& argIterator,
                               bool disallowArgument = false);
        std::string findLongOptionFromShort(const char& shortOption);
        void processPositionalArgument(std::vector<std::string>::iterator& argIterator);

        bool removeBlankLineSuppressionChar(std::string& para) const;
        void outputParagraphs(const std::vector<std::string>& paragraphs) const;
        void outputOption(const std::string& name) const;
        void outputFullUsage(const std::string& subcommandName = "");
        void outputSubcommand(const std::string& name);
        void outputUsageLines(const std::string& subcommandName = "");
        void outputUsageError(const std::string& option = "", const std::string& subcommand = "");

        std::vector<std::string> parseList(const std::string& name, const std::string& prefix = "") const;
        std::vector<std::string> parseRangedList(std::vector<std::string>& rawList, const std::string& prefix) const;

        MicException buildException(ParserErrorCode code, const std::string& message) const;
        MicException buildAPIException(const std::string& message) const;
    };

}; // namespace micmgmt

#endif // MICMGMT_CLIPARSERIMPL_HPP
