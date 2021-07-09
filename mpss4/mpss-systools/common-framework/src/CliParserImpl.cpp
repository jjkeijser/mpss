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

// Private Includes
#include "micmgmtCommon.hpp"
#include "commonStrings.hpp"
#include "CliParserImpl.hpp"
#include "CliParserExitException_p.hpp"
#include "ConsoleOutputFormatter.hpp"

// Public Interface Includes
#include "MicOutput.hpp"
#include "EvaluateArgumentCallbackInterface.hpp"

// std Includes
#include <stdexcept>
#include <sstream>
#include <set>
#include <memory>
#include <limits>

namespace
{
    // This caps the upper range of a ranged argument list
    const size_t RANGE_CAP = 255;

    // This is used to suppress the blank line at the end of a paragraph in the parser help generation.
    const char sParagraphSuppressionChar = '@';
} // empty namespace

namespace micmgmt
{
    using namespace std;

    CliParserImpl::CliParserImpl(int argc, char* argv[], const std::string& toolVersion, const std::string& publishYear,
                                 const std::string& programName, const std::string& shortDescription)
        : LONG_SEP("--"), SHORT_SEP("-"), LIST_SEP(","), RANGE_SEP("-"), output_(NULL), parsed_(false), parseError_(false),
          disallowMultiOption_(true), allowGeneralArguments_(false)
    {

        if (argc < 1 || argv == NULL)
        {
            CliParserError localError; // Only created under serious conditions because this is expensive!
            auto info = localError.buildError(ParserErrorCode::eParserApiError,
                                              "Invalid tool arguments given to the constructor");
            throw MicException(info);
        }
        // Set programName_
        string localName = trim(programName);
        if (localName.empty() == true)
        {
            programName_ = argv[0];
            for (auto it = programName_.begin(); it != programName_.end(); ++it)
            {
                if (*it == '\\')
                {
                    *it = '/';
                }
            }
            std::string::size_type pos = programName_.rfind('/');
            if (pos != std::string::npos)
            {
                programName_.erase(0, pos + 1);
            }
            pos = programName_.rfind(".exe");
            if (pos != std::string::npos)
            {
                programName_.erase(pos);
            }
        }
        else
        {
            programName_ = localName;
        }
        errorFormatter_ = unique_ptr<CliParserError>(new CliParserError(programName_));

        // Set application information...
        string localVersion = trim(toolVersion);
        if (localVersion.empty())
        {
            throw buildAPIException("The 'version' arguments must be a non-empty string");
        }
        try
        {
            copyright_ = copyright(publishYear.c_str());
        }
        catch (invalid_argument& e)
        {
            throw buildAPIException(e.what());
        }
        version_ = localVersion;
        description_ = normalize(shortDescription);

        // Store raw arguments for parsing...
        for (int i = 1; i < argc; ++i)
        {
            rawArguments_.push_back(argv[i]);
        }

        // Add required default options.
        addOption(commonOptHelpName, commonOptHelpHelp, 'h', false, "", commonEmptyHelp, "", NULL);
        addOption(commonOptVersionName, commonOptVersionHelp, 0, false, "", commonEmptyHelp, "", NULL);

        // Add default output formatters (cout, cerr, no file formatter).
        setOutputFormatters();
    }

    CliParserImpl::~CliParserImpl()
    {
        if (output_ != NULL)
        {
            delete output_;
        }
    }

    void CliParserImpl::addOption(const std::string& name, const std::vector<std::string>& help,
                                  const char& shortLetter, const bool& requiredArgument,
                                  const std::string& argumentName, const std::vector<std::string>& argumentHelp,
                                  const std::string& defVal, EvaluateArgumentCallbackInterface* callback)
    {
        if (parsed_ == true)
        {
            throw buildAPIException("The 'addOption' method cannot be called after the 'parse' method");
        }
        string localName = trim(name);
        if (verifyCanonicalName(localName) == false || localName[0] == '_')
        {
            throw buildAPIException("The option name must be non-empty and a valid name and not start with an underscore");
        }
        if (localName.length() < 2)
        {
            throw buildAPIException("The option name must be at least 2 characters long");
        }
        if (shortLetter != 0 && isalnum(shortLetter) == 0)
        {
            throw buildAPIException("The short option letter can only be an alpha-numeric value");
        }
        for (auto it = optionDefinitions_.begin(); it != optionDefinitions_.end(); ++it)
        {
            if (it->first == localName)
            {
                throw buildAPIException("You cannot add the same option name twice: " + localName);
            }
            if (shortLetter != 0 && it->second.shortName() == shortLetter)
            {
                throw buildAPIException("You cannot add the same short option name twice: " + string(1, shortLetter));
            }
        }
        if (help.empty() == true || trim(help[0]).empty() == true)
        {
            throw buildAPIException("Help for the option name is required to be non-empty");
        }
        try
        {
            OptionDef def(localName, help, shortLetter, requiredArgument, argumentName, argumentHelp, defVal, callback);
            optionDefinitions_[localName] = def;
        }
        catch (std::invalid_argument& e)
        {
            throw buildAPIException(e.what());
        }
    }

    void CliParserImpl::addToolPositionalArg(const std::string& name, const std::vector<std::string>& help,
                                             EvaluateArgumentCallbackInterface* callback)
    {
        if (parsed_ == true)
        {
            throw buildAPIException("The 'addToolPositionalArg' method cannot be called after the 'parse' method");
        }
        if (subcommandDefinitions_.empty() == false)
        {
            throw buildAPIException("It is illegal to add tool positional arguments after you added a subcommand");
        }
        if (help.empty() == true || trim(help[0]).empty() == true)
        {
            throw buildAPIException("Help for the positional argument name is required to be non-empty");
        }
        for (auto it = toolPositionalDefinitions_.begin(); it != toolPositionalDefinitions_.end(); ++it)
        {
            if (trim(name) == it->name())
            {
                throw buildAPIException("It is illegal to insert multiple positional arguments with the same name");
            }
        }
        try
        {
            toolPositionalDefinitions_.push_back(ArgumentDef(name, EvaluateArgumentType::ePositionalArgument, help,
                                                             true, "", callback));
        }
        catch (std::invalid_argument&)
        {
            throw buildAPIException("The argument's 'name' and/or 'help' were illegal");
        }
    }

    void CliParserImpl::addSubcommand(const std::string& subcommand, const std::vector<std::string>& help)
    {
        if (parsed_ == true)
        {
            throw buildAPIException("The 'addSubcommand' method cannot be called after the 'parse' method");
        }
        if (toolPositionalDefinitions_.empty() == false)
        {
            throw buildAPIException("It is illegal to add subcommands after you have added tool positional arguments");
        }
        string localName = trim(subcommand);
        if (verifyCanonicalName(localName) == false || localName[0] == '_')
        {
            throw buildAPIException("The subcommand name must be non-empty and a valid name and not start with a underscore");
        }
        if (help.empty() == true || trim(help[0]).empty() == true)
        {
            throw buildAPIException("Help for the subcommand name is required to be non-empty");
        }
        for (auto it = subcommandDefinitions_.begin(); it != subcommandDefinitions_.end(); ++it)
        {
            if (localName == it->first)
            {
                throw buildAPIException("It is illegal to insert multiple subcommands with the same name");
            }
        }
        subcommandDefinitions_[localName] = SubcommandDef(localName, help);
    }

    void CliParserImpl::addSubcommandPositionalArg(const std::string& subcommandName, const std::string& argName,
                                               const std::vector<std::string>& help,
                                               EvaluateArgumentCallbackInterface* callback)
    {
        if (parsed_ == true)
        {
            throw buildAPIException("The 'addSubcommandPositionalArg' method cannot be called after the 'parse' method");
        }

        string localName = trim(subcommandName);
        auto found = subcommandDefinitions_.find(localName);
        if (found == subcommandDefinitions_.end())
        {
            throw buildAPIException("The subcommand '" + subcommandName + "' must be added to the parser before calling 'addSubcommandPositionalArg' with the subcommand name");
        }
        string localArgName = trim(argName);
        if (verifyCanonicalName(localArgName) == false || localArgName[0] == '_')
        {
            throw buildAPIException("The subcommand positional argument name must be non-empty and a valid name and not start with a underscore");
        }
        if (help.empty() == true || trim(help[0]).empty() == true)
        {
            throw buildAPIException("Help for the subcommand positional argument name is required to be non-empty");
        }
        for (auto it = found->second.positionalArgumentBegin(); it != found->second.positionalArgumentEnd(); ++it)
        {
            if (localArgName == it->name())
            {
                throw buildAPIException("It is illegal to insert multiple subcommand positional arguments with the same name");
            }
        }
        found->second.addArgument(localArgName, help, callback);
    }

    bool CliParserImpl::parse(bool allowGeneralArguments)
    {
        allowGeneralArguments_ = allowGeneralArguments;
        if (parsed_ == true)
        {
            throw buildAPIException("The 'parse' method cannot be called more than once per instantiation of the CliParser class");
        }
        parsed_ = true;

        try
        {
            for (auto it = rawArguments_.begin(); it != rawArguments_.end(); ++it)
            {
                parseArgument(it);
            }

            if (options_.find(commonOptHelpName) != options_.end())
            { // Process --help
                if (arguments_.size() == 0 && options_.size() == 1)
                {
                    outputHelp(subcommand_);
                    return false;
                }
                else
                {
                    auto info = errorFormatter_->buildError(ParserErrorCode::eParserHelpError);
                    output_->outputError(info.second, info.first);
                    outputUsageError(commonOptHelpName);
                    return false;
                }
            }
            else if (options_.find(commonOptVersionName) != options_.end())
            { // Process --version
                if (arguments_.size() == 0 && options_.size() == 1)
                {
                    outputVersion();
                    return false;
                }
                else
                {
                    auto info = errorFormatter_->buildError(ParserErrorCode::eParserVersionError);
                    output_->outputError(info.second, info.first);
                    outputUsageError(commonOptVersionName);
                    parseError_ = true;
                    return false;
                }
            }
            if (toolPositionalDefinitions_.size() != 0 && arguments_.size() != toolPositionalDefinitions_.size())
            {
                auto info = errorFormatter_->buildError(ParserErrorCode::eParserToolPosShortError);
                output_->outputError(info.second, info.first);
                outputUsageError();
                parseError_ = true;
                return false;
            }
            else if (subcommandDefinitions_.size() > 0 && subcommand_.empty() == false)
            {
                auto subDef = subcommandDefinitions_.find(subcommand_);
                if ((allowGeneralArguments == false && arguments_.size() < subDef->second.positionalArgumentSize()) ||
                    (allowGeneralArguments == true && subDef->second.positionalArgumentSize() > arguments_.size()))
                {
                    auto info = errorFormatter_->buildError(ParserErrorCode::eParserSubPosShortError, " '" + subDef->first + "'");
                    output_->outputError(info.second, info.first);
                    outputUsageError("", subcommand_);
                    parseError_ = true;
                    return false;
                }
                if (allowGeneralArguments == false && arguments_.size() > subDef->second.positionalArgumentSize())
                {
                    auto info = errorFormatter_->buildError(ParserErrorCode::eParserSubPosLongError, ": '" + subDef->first + "'");
                    output_->outputError(info.second, info.first);
                    outputUsageError("", subcommand_);
                    parseError_ = true;
                    return false;
                }
            }

            if (subcommandDefinitions_.size() > 0 && subcommand_.empty() == true)
            {
                auto info = errorFormatter_->buildError(ParserErrorCode::eParserMissingSubcommandError);
                output_->outputError(info.second, ParserErrorCode::eParserMissingSubcommandError);
                outputUsageError();
                parseError_ = true;
                return false;
            }

            return true;
        }
        catch (CliParserExitException& context)
        {
            auto info = errorFormatter_->buildError(context.errorCode(), context.what());
            output_->outputError(info.second, context.errorCode());
            outputUsageError(context.optionContext(), context.subcommandContext());
            parseError_ = true;
            return false;
        }
        catch (out_of_range& ex)
        {
            auto info = errorFormatter_->buildError(eParserInvalidArgumentSubError, ex.what());
            output_->outputError(info.second, info.first);
            parseError_ = true;
            return false;
        }
    }

    bool CliParserImpl::parseError() const
    {
        if (parsed_ == false)
        {
            throw buildAPIException("The 'parseError' method cannot be called before the 'parse' method");
        }
        else
        {
            return parseError_;
        }
    }

    bool CliParserImpl::parsed() const
    {
        return parsed_;
    }

    const std::string& CliParserImpl::parsedSubcommand() const
    {
        if (parsed_ == false)
        {
            throw buildAPIException("The 'parsedSubcommand' method cannot be called before the 'parse' method");
        }
        return subcommand_;
    }

    std::tuple<bool, bool, std::string> CliParserImpl::parsedOption(const std::string& optionName) const
    {
        if (parsed_ == false)
        {
            throw buildAPIException("The 'parsedOption' method cannot be called before the 'parse' method");
        }
        auto found = options_.find(trim(optionName));
        bool present = false;
        bool argPresent = false;
        string defVal = "";
        if (found != options_.end())
        { // Found option on command line
            present = true;
            argPresent = found->second.first;
            defVal = found->second.second;
        }
        else
        { // Option not found on command line
            auto optionDef = optionDefinitions_.find(trim(optionName));
            if (optionDef != optionDefinitions_.end())
            {
                if ((bool)optionDef->second.argumentDef() == true)
                {
                    defVal = optionDef->second.argumentDef()->defaultValue();
                }
            }
        }
        return tuple<bool, bool, string>(present, argPresent, defVal);
    }

    std::vector<std::string>::iterator CliParserImpl::parsedPositionalArgumentBegin()
    {
        if (parsed_ == false)
        {
            throw buildAPIException("The 'parsedPositionalArgumentBegin' method cannot be called before the 'parse' method");
        }
        return arguments_.begin();
    }

    std::vector<std::string>::iterator CliParserImpl::parsedPositionalArgumentEnd()
    {
        if (parsed_ == false)
        {
            throw buildAPIException("The 'parsedPositionalArgumentEnd' method cannot be called before the 'parse' method");
        }
        return arguments_.end();
    }

    const std::string& CliParserImpl::parsedPositionalArgumentAt(std::vector<std::string>::size_type index) const
    {
        if (parsed_ == false)
        {
            throw buildAPIException("The 'parsedPositionalArgumentAt' method cannot be called before the 'parse' method");
        }
        if (index >= arguments_.size())
        {
            throw buildAPIException("The 'index' for the positional argument is out of range");
        }
        return arguments_.at(index);
    }

    std::vector<std::string> CliParserImpl::parsedPositionalArgumentAsDeviceList(std::vector<std::string>::size_type index) const
    {
        bool fail = true;
        std::string deviceListRange = parsedPositionalArgumentAt(index);
        auto deviceList = micmgmt::deviceNamesFromListRange(deviceListRange, &fail);
        if (fail)
            throw buildException(ParserErrorCode::eParserDeviceRangeListError,
                    ": '" + deviceListRange + "'");
        return deviceList;
    }


    std::vector<std::string>::size_type CliParserImpl::parsedPositionalArgumentSize()
    {
        if (parsed_ == false)
        {
            throw buildAPIException("The 'parsedPositionalArgumentSize' method cannot be called before the 'parse' method");
        }
        return arguments_.size();
    }

    void CliParserImpl::setOutputFormatters(MicOutputFormatterBase* stdOut, MicOutputFormatterBase* stdErr,
                                            MicOutputFormatterBase* fileOut)
    {
        if (output_ != NULL)
        {
            delete output_;
            output_ = NULL;
        }
        output_ = new MicOutput(programName_, stdOut, stdErr, fileOut);
    }

    void CliParserImpl::outputVersion() const
    {
        if (parsed_ == false)
        {
            throw buildAPIException("The 'outputVersion' method cannot be called after the 'parse' method");
        }
        output_->outputLine(programName_ + " version " + version_);
        output_->outputLine(copyright_);
    }

    void CliParserImpl::outputHelp(const std::string& subcommandName)
    {
        if (parsed_ == false)
        {
            throw buildAPIException("The 'outputHelp' method cannot be called before the 'parse' method");
        }
        // Output tool info
        outputVersion();

        // Output usage lines
        outputFullUsage(subcommandName);
        if (subcommandName.empty() == true)
        { // output description
            output_->startSection("Description");
            output_->outputLine(description_);
            output_->endSection();
        }
        if (subcommandDefinitions_.size() > 0)
        { // Output subcommand and positional argument help
            output_->startSection("Subcommands");
            for (auto it = subcommandDefinitions_.begin(); it != subcommandDefinitions_.end(); ++it)
            {
                if (subcommandName.empty() == true || it->first == subcommandName)
                {
                    outputSubcommand(it->first);
                    auto next = it;
                    ++next;
                    if (next != subcommandDefinitions_.end() && subcommandName.empty() == true)
                    {
                        output_->outputLine("");
                    }
                }
            }
            output_->endSection();
        }
        else if (toolPositionalDefinitions_.size() > 0)
        { // Output tool positional parameters help
            output_->startSection("Positional Arguments");
            for (auto it = toolPositionalDefinitions_.begin(); it != toolPositionalDefinitions_.end(); ++it)
            {
                output_->outputLine("<" + it->name() + ">:");
                output_->startSection("");
                outputParagraphs(it->help());
                output_->endSection();
                output_->outputLine("");
            }
            output_->endSection();
        }

        // Output options help
        if (optionDefinitions_.size() > 0 && subcommandName.empty() == true)
        {
            output_->startSection("Options");
            for (auto it = optionDefinitions_.begin(); it != optionDefinitions_.end(); ++it)
            {
                outputOption(it->first);
                auto next = it;
                ++next;
                if (next != optionDefinitions_.end())
                {
                    output_->outputLine("");
                }
            }
            output_->endSection();
        }

        // Output extra help sections
        if (extraHelpSections_.size() > 0 && subcommandName.empty() == true)
        {
            for (auto it = extraHelpSections_.begin(); it != extraHelpSections_.end(); ++it)
            {
                output_->startSection(it->first);
                outputParagraphs(it->second);
                output_->endSection();
            }
        }
        output_->outputLine("");
    }

    void CliParserImpl::addHelpSection(const std::string& sectionName, const std::vector<std::string>& paragraphs)
    {
        if (parsed_ == true)
        {
            throw buildAPIException("The 'addHelpSection' method cannot be called after the 'parse' method");
        }
        string localName = trim(sectionName);
        if (localName.empty() == true)
        {
            throw buildAPIException("CliParserImpl::addHelpSection: Missing section name");
        }
        vector<string> localParagraphs;
        for (auto it = paragraphs.begin(); it != paragraphs.end(); ++it)
        {
            string para;
            if (it->size() > 0 && (*it)[it->size() - 1] == sParagraphSuppressionChar)
            {
                para = trim(*it);
            }
            else
            {
                para = normalize(*it);
            }
            localParagraphs.push_back(para);
        }
        if (localParagraphs.empty() == true)
        {
            throw buildAPIException("CliParserImpl::addHelpSection: There are no paragraphs to display");
        }
        if (localParagraphs[0].empty() == true)
        {
            throw buildAPIException("CliParserImpl::addHelpSection: The first paragraph of help text must not be empty");
        }

        extraHelpSections_.push_back(pair<string,vector<string>>(localName, localParagraphs));
    }

    std::vector<std::string> CliParserImpl::parsedOptionArgumentAsVector(const std::string& optionName, const std::string& prefix) const
    {
        if (parsed_ == false)
        {
            throw buildAPIException("The 'parsedOptionArgumentAsVector' method cannot be called before the 'parse' method");
        }
        auto optionDef = optionDefinitions_.find(optionName);
        if (optionDef == optionDefinitions_.end())
        {
            throw buildAPIException("The option '" + optionName + "' was not found");
        }
        string arg = get<2>(parsedOption(optionName));
        if (arg == optionDef->second.argumentDef()->defaultValue())
        {
            vector<string> def = { arg };
            return def;
        }
        else
        {
            return parseList(arg, prefix);
        }
    }

    std::vector<std::string> CliParserImpl::parsedPositionalArgumentAsVector(std::vector<std::string>::size_type index, const std::string& prefix) const
    {
        if (parsed_ == false)
        {
            throw buildAPIException("The 'parsedPositionalArgumentAsVector' method cannot be called before the 'parse' method");
        }
        string arg = parsedPositionalArgumentAt(index);
        return parseList(arg, prefix);
    }

    std::vector<std::string> CliParserImpl::parsedOptionArgumentAsDeviceList(const std::string& optionName)
    {
        bool fail = true;
        std::string deviceListRange = get<2>(parsedOption(optionName));
        auto deviceList = micmgmt::deviceNamesFromListRange(deviceListRange, &fail);
        if (fail)
            throw buildException(ParserErrorCode::eParserDeviceRangeListError,
                    ": '" + deviceListRange + "'");
        return deviceList;

    }

    const std::string& CliParserImpl::toolName() const
    {
        return programName_;
    }

//#######################################
//# Private Class Implementation Section
//#######################################
    void CliParserImpl::exitApplication(ParserErrorCode errorCode, const std::string& msg,
                                        const std::string& optionContext, const std::string& subcommandContext)
    {
        throw CliParserExitException(errorCode, msg, optionContext, subcommandContext);
    }

    void CliParserImpl::parseArgument(std::vector<std::string>::iterator& argIterator)
    {
        string arg = *argIterator;
        if (arg.find(SHORT_SEP) == 0)
        { // Possible Option
            if (arg.find(LONG_SEP) == 0)
            { // Long Option
                processLongOption(arg.erase(0,LONG_SEP.length()), argIterator);
            }
            else // Short Option
            {
                for (auto it = ++(arg.begin()); it != arg.end(); ++it)
                {
                    string optionName = findLongOptionFromShort(*it);
                    if (optionName.empty())
                    {
                        exitApplication(ParserErrorCode::eParserUnknownShortOptionError, " '" + string(1, *it) + "'");
                    }
                    else
                    {
                        auto itNext = next(it, 1); // peek next
                        bool lastArg = (itNext == arg.end());
                        if (lastArg == false)
                        {
                            processLongOption(optionName, argIterator, true); // name, iterator&, disallowArgument
                        }
                        else
                        {
                            processLongOption(optionName, argIterator);
                        }
                    }
                }
            }
        }
        else if (subcommandDefinitions_.size() > 0)
        { // Expect Subcommand
            if (subcommand_.empty() == true)
            { // Subcommand
                auto it = subcommandDefinitions_.find(arg);
                if (it != subcommandDefinitions_.end())
                {
                    subcommand_ = arg;
                }
                else
                {
                    exitApplication(ParserErrorCode::eParserNotSubcommandError);
                }
            }
            else
            { // Subcommand Positional Argument
                processPositionalArgument(argIterator);
            }
        }
        else
        { // Tool Positional Argument
            processPositionalArgument(argIterator);
        }

    }

    std::string CliParserImpl::findLongOptionFromShort(const char& shortOption)
    {
        if (shortOption != 0)
        {
            for (auto it = optionDefinitions_.begin(); it != optionDefinitions_.end(); ++it)
            {
                if (it->second.shortName() == shortOption)
                {
                    return it->second.optionName();
                }
            }
        }
        return string();
    }

    void CliParserImpl::processPositionalArgument(std::vector<std::string>::iterator& argIterator)
    {
        if (subcommand_.empty() == false)
        { // Subcommand Argument
            auto subcommandDef = subcommandDefinitions_.find(subcommand_);
            if (subcommandDef->second.positionalArgumentSize() > 0 &&
                arguments_.size() < subcommandDef->second.positionalArgumentSize())
            { // Specified Positional Arguments
                if (subcommandDef->second.positionalArgumentAt(arguments_.size()).checkArgumentValue(*argIterator, subcommand_) == true)
                {
                    arguments_.push_back(*argIterator);
                }
                else
                { // Not a Valid Argument!
                    exitApplication(ParserErrorCode::eParserInvalidArgumentSubError,
                                    ": argument='" + *argIterator + "', subcommand='" + subcommand_ + "'", "", subcommand_);
                }
            }
            else if (subcommandDef->second.positionalArgumentSize() == 0)
            { // Unspecified Subcommand Arguments (Not Validated)
                arguments_.push_back(*argIterator);
            }
            else
            { // Too many command line args for subcommand!!!
                exitApplication(ParserErrorCode::eParserSubPosLongError, ": '" + subcommand_ + "'", "", subcommand_);
            }
        }
        else if (toolPositionalDefinitions_.size() > 0)
        { // Tool Positional Argument
            if (arguments_.size() < toolPositionalDefinitions_.size())
            { // Specified Tool Positional Arguments
                if (toolPositionalDefinitions_.at(arguments_.size()).checkArgumentValue(*argIterator) == true)
                {
                    arguments_.push_back(*argIterator);
                }
                else
                { // Not a Valid Argument!
                    exitApplication(ParserErrorCode::eParserInvalidArgumentToolError, ": '" + *argIterator + "'", "", subcommand_);
                }
            }
            else
            { // Too many command line args!!!
                exitApplication(ParserErrorCode::eParserToolPosLongError, "", "", subcommand_);
            }
        }
        else
        { // General Argument (Not Validated)
            arguments_.push_back(*argIterator);
        }
    }

    void CliParserImpl::processLongOption(const std::string& option, std::vector<std::string>::iterator& argIterator,
                                      bool disallowArgument)
    {
        bool foundArgument = false;
        pair<string, string> optionPair;
        if (option.find("=") != string::npos)
        { // Attached Argument
            optionPair.first = option.substr(0, option.find("="));
            optionPair.second = option.substr(option.find("=") + 1);
            foundArgument = true;
        }
        else
        { // Unattached Argument or No Argument
            optionPair.first = option;
        }

        if (options_.find(optionPair.first) != options_.end()) { // Do not allow duplicated options
            exitApplication(ParserErrorCode::eParserDuplicatedOptionsError, ": '" + optionPair.first + "'");
        }

        auto optDef = optionDefinitions_.find(optionPair.first);
        if (optDef == optionDefinitions_.end())
        { // No Such Option!
            exitApplication(ParserErrorCode::eParserUnknownLongOptionError, ": '" + optionPair.first + "'");
        }
        if (foundArgument == true && optDef->second.hasArgument() == false)
        {
            exitApplication(ParserErrorCode::eParserUnexpectedOptionArgError,
                            ": option='--" + optionPair.first + "', argument='" + optionPair.second + "'", optionPair.first);
        }
        if (disallowArgument == true && optDef->second.hasArgument() == true &&
            optDef->second.argumentDef()->isRequired())
        { // Illegal Condition in Grouping of Short Options
            exitApplication(ParserErrorCode::eParserShortOptionGroupError,
                            ": '" + string(1, optDef->second.shortName()) + "'", optionPair.first);
        }
        if (optionPair.second.empty() == true)
        {
            auto nextArg = next(argIterator); // Peek next

            if (optDef->second.hasArgument() == true && optDef->second.argumentDef()->isRequired())
            { // Required Argument
                if (nextArg == rawArguments_.end())
                { // Missing Required Argument
                    exitApplication(ParserErrorCode::eParserMissingOptionArgError, ": '" + optionPair.first + "'",
                                    optionPair.first);
                }
                else
                {
                    if (nextArg->find(SHORT_SEP) == 0 || nextArg->find(LONG_SEP) == 0)
                    { // Error Required Argument Missing
                        exitApplication(ParserErrorCode::eParserMissingOptionArgError, ": '" + optionPair.first + "'",
                                        optionPair.first);
                    }
                    else
                    {
                        optionPair.second = *nextArg;
                        ++argIterator;
                        foundArgument = true;
                    }
                }
            }
            else if (optDef->second.hasArgument() == true)
            { // Optional Argument
                if (nextArg != rawArguments_.end())
                { // Possible Optional Argument
                    if (nextArg->find(SHORT_SEP) != 0 && nextArg->find(LONG_SEP) != 0)
                    { // Optional Agument Found; Consume nextArg
                        optionPair.second = *nextArg;
                        ++argIterator;
                        foundArgument = true;
                    }
                    else
                    { // Missing Option; Use Default Value
                        optionPair.second = optDef->second.argumentDef()->defaultValue();
                    }
                }
                else
                { // Missing Option; Use Default Value
                    optionPair.second = optDef->second.argumentDef()->defaultValue();
                }
            }
        }
        if (optDef->second.hasArgument() == true && foundArgument == true)
        { // Validate Argument
            if (optDef->second.checkArgumentValue(optionPair.second) == false)
            { // Invalid Argument
                exitApplication(ParserErrorCode::eParserInvalidOptionArgError, ": '" + optionPair.second + "'",
                                optionPair.first);
            }
        }

        if (options_.find(optionPair.first) == options_.end())
        {
            options_[optionPair.first] = pair<bool, string>(foundArgument, optionPair.second);
        }
        else if (disallowMultiOption_ == false)
        {
            options_[optionPair.first].second += LIST_SEP + optionPair.second;
        }
    }

    void CliParserImpl::outputFullUsage(const std::string& subcommandName)
    {
        output_->startSection("Usage");
        outputUsageLines(subcommandName);
        output_->endSection();
    }

    void CliParserImpl::outputOption(const std::string& name) const
    {
        auto optionDef = optionDefinitions_.find(name);
        if (optionDef == optionDefinitions_.end())
        {
            throw buildAPIException("CliParserImpl::outputOption: Option name '" + name + "' was not found");
        }
        ostringstream lineStream;
        if (optionDef->second.shortName() != 0)
        {
            lineStream << "-" << optionDef->second.shortName() << ", ";
        }
        lineStream << "--" << name;
        if (optionDef->second.hasArgument())
        {
            bool required = optionDef->second.argumentDef()->isRequired();
            if (required == true)
            {
                lineStream << " <" << optionDef->second.argumentDef()->name() << ">";
            }
            else
            {
                lineStream << " [" << optionDef->second.argumentDef()->name() << "]";
            }
        }
        output_->outputLine(lineStream.str());
        output_->startSection("");
        outputParagraphs(optionDef->second.optionHelp());
        if (optionDef->second.hasArgument() == true && optionDef->second.argumentDef()->help().size() > 0)
        {
            output_->startSection(optionDef->second.argumentDef()->name());
            outputParagraphs(optionDef->second.argumentDef()->help());
            output_->endSection();
        }
        output_->endSection();
    }

    bool CliParserImpl::removeBlankLineSuppressionChar(std::string& para) const
    {
        bool rv = false;
        if (para != "" && para[para.size() - 1] == sParagraphSuppressionChar)
        {
            para.erase(para.size() - 1);
            rv = true;
        }
        return rv;
    }

    void CliParserImpl::outputParagraphs(const std::vector<std::string>& paragraphs) const
    {
        for (auto it = paragraphs.begin(); it != paragraphs.end(); ++it)
        {
            string para = *it;
            bool skipBlankLine = removeBlankLineSuppressionChar(para);
            output_->outputLine(para, skipBlankLine);
            auto next = it;
            ++next;
            if (next != paragraphs.end() && skipBlankLine == false)
            {
                output_->outputLine("");
            }
        }
    }

    void CliParserImpl::outputSubcommand(const std::string& name)
    {
        auto subcommandDef = subcommandDefinitions_.find(name);
        if (subcommandDef == subcommandDefinitions_.end())
        {
            throw buildAPIException("CliParserImpl::outputSubcommand: Subcommand name '" + name + "' was not found");
        }
        ostringstream lineStream;
        lineStream << name;
        if (subcommandDef->second.positionalArgumentSize() > 0)
        {
            for (auto it = subcommandDef->second.positionalArgumentBegin();
                 it != subcommandDef->second.positionalArgumentEnd(); ++it)
            {
                lineStream << " <" << it->name() << ">";
            }
        }
        else if (allowGeneralArguments_ == true)
        {
            lineStream << " [argument [...]]";
        }
        output_->outputLine(lineStream.str());
        output_->startSection("");
        outputParagraphs(subcommandDef->second.subcommandHelp());
        if (subcommandDef->second.positionalArgumentSize() > 0)
        {
            for (auto it = subcommandDef->second.positionalArgumentBegin();
                 it != subcommandDef->second.positionalArgumentEnd(); ++it)
            {
                output_->startSection("<" + it->name() + ">");
                outputParagraphs(it->help());
                output_->endSection();
            }
        }
        output_->endSection();
    }

    void CliParserImpl::outputUsageLines(const std::string& subcommandName)
    {
        if (subcommandName.empty() == true)
        {
            if (optionDefinitions_.find(commonOptVersionName) != optionDefinitions_.end())
            {
                output_->outputLine(programName_ + " --" + commonOptVersionName);
            }
            if (optionDefinitions_.find(commonOptHelpName) != optionDefinitions_.end())
            {
                output_->outputLine(programName_ + " --" + commonOptHelpName);
            }
        }
        if (subcommandDefinitions_.size() > 0)
        {
            for (auto it = subcommandDefinitions_.begin(); it != subcommandDefinitions_.end(); ++it)
            {
                if (subcommandName.empty() == true || subcommandName == it->first)
                {
                    ostringstream line;
                    if (optionDefinitions_.find(commonOptHelpName) != optionDefinitions_.end())
                    {
                        line << programName_ << " " << it->first << " --" + commonOptHelpName;
                        output_->outputLine(line.str());
                        line.str(""); // clear
                    }
                    line << programName_ << " " << it->first << " [OPTIONS]";
                    if (it->second.positionalArgumentSize() > 0)
                    {
                        for (auto it2 = it->second.positionalArgumentBegin(); it2 != it->second.positionalArgumentEnd(); ++it2)
                        {
                            line << " <" << it2->name() << ">";
                        }
                    }
                    else
                    {
                        if (allowGeneralArguments_ == true)
                        {
                            line << " [argument [...]]";
                        }
                    }
                    output_->outputLine(line.str());
                }
            }
        }
        else if (toolPositionalDefinitions_.size() > 0)
        {
            ostringstream line;
            line << programName_ << " [OPTIONS]";
            for (auto it = toolPositionalDefinitions_.begin(); it != toolPositionalDefinitions_.end(); ++it)
            {
                line << " <" << it->name() << ">";
            }
            output_->outputLine(line.str());
        }
        else
        {
            string line = " [OPTIONS]";
            if (allowGeneralArguments_ == true)
            {
                line += " [argument [...]]";
            }
            output_->outputLine(programName_ + line);
        }
    }

    void CliParserImpl::outputUsageError(const std::string& option, const std::string& subcommand)
    {
        if (option.empty() == false && subcommand.empty() == false)
        {
            throw buildAPIException("CliParserImpl::outputUsage: you cannot specify both an option and a subcommand to filter on at the same time");
        }
        if (option.empty() == false)
        { // Option output
            output_->startSection("Option");
            outputOption(option);
            output_->endSection();
        }
        else
        { // Possible output with subcommand filter.
            if (subcommandDefinitions_.size() > 0)
            {
                if (subcommand.empty() == false)
                {
                    output_->startSection("Subcommand");
                    outputSubcommand(subcommand);
                    output_->endSection();
                }
                else
                {
                    output_->startSection("Usage");
                    outputUsageLines();
                    output_->endSection();
                }
            }
            else
            {
                output_->outputError("Please use '" + programName_ + " --help" + "' to get help.",
                                     ParserErrorCode::eParserSuccess);
                outputFullUsage();
            }
        }
    }

    std::vector<std::string> CliParserImpl::parseList(const std::string& argument, const std::string& prefix) const
    {
        string localArgument = trim(argument);
        if (localArgument.empty() == true)
        {
            return vector<string>(); // Empty vector
        }
        string wS = "\t ";
        if (localArgument.find_first_of(wS) != string::npos)
        {
            string::size_type pos = localArgument.find_first_of(wS);
            while (pos != string::npos)
            {
                string::size_type last = localArgument.find_first_not_of(wS, pos);
                localArgument.erase(pos, last - pos);
                pos = localArgument.find_first_of(wS);
            }
        }
        vector<string> rawList;
        string localArg = localArgument;
        string item;
        string::size_type pos = localArg.find(LIST_SEP);
        while (pos != string::npos)
        {
            item = localArg.substr(0, pos);
            localArg.erase(0, pos + 1);
            if (prefix.empty() == false)
            {
                if (item.length() <= prefix.length() || item.substr(0, prefix.length()) != prefix)
                {
                    throw buildException(ParserErrorCode::eParserListParsePrefixError, ": '" + localArgument + "'");
                }
            }
            rawList.push_back(item);
            pos = localArg.find(LIST_SEP);
            if (pos == string::npos && prefix.empty() == false)
            {
                if (localArg.length() <= prefix.length() || localArg.substr(0, prefix.length()) != prefix)
                {
                    throw buildException(ParserErrorCode::eParserListParsePrefixError, ": '" + localArgument + "'");
                }

            }
        }
        item = localArg;
        if (prefix.empty() == false)
        {
            if (item.length() <= prefix.length() || item.substr(0, prefix.length()) != prefix)
            {
                throw buildException(ParserErrorCode::eParserListParsePrefixError, ": '" + localArgument + "'");
            }
        }
        rawList.push_back(item);
        if (prefix.empty() == false)
        {
            if (localArg.length() <= prefix.length() || localArg.substr(0, prefix.length()) != prefix)
            {
                throw buildException(ParserErrorCode::eParserListParsePrefixError, " '" + localArgument + "'");
            }

            return parseRangedList(rawList, prefix);
        }
        else if (argument.find_first_not_of("0123456789" + LIST_SEP + RANGE_SEP) == string::npos)
        {
            return parseRangedList(rawList, prefix);
        }
        else
        {
            return rawList;
        }
    }

    bool CliParserImpl::validUnsigned32bitNumber(const std::string& sNumber, uint32_t* value) const
    {
        if (sNumber.size() > 10) // Max ten digits for 32 bit number
        {
            return false;
        }
        uint64_t number = stoull(sNumber);
        if (number > static_cast<uint64_t>(numeric_limits<int32_t>::max()))
        {
            return false;
        }
        if (number > RANGE_CAP)
        {
            return false;
        }
        if (value != NULL)
        {
            *value = static_cast<uint32_t>(number);
        }
        return true;
    }

    std::vector<std::string> CliParserImpl::parseRangedList(std::vector<std::string>& rawList, const std::string& prefix) const
    {
        set<unsigned long> uniqueSorted2;
        for (auto item = rawList.begin(); item != rawList.end(); ++item)
        {
            vector<string>::size_type pos = item->find(RANGE_SEP);
            if (pos != string::npos)
            { // Ranged
                string startItem = item->substr(0, pos);
                string endItem = item->substr(pos + RANGE_SEP.length());
                if (endItem.length() <= prefix.length() || endItem.compare(0, prefix.length(), prefix) != 0)
                {
                    throw buildException(ParserErrorCode::eParserListParseRangedPrefixError, ": '" + *item + "'");
                }
                if (startItem.erase(0, prefix.length()).find_first_not_of("0123456789") != string::npos)
                {
                    throw buildException(ParserErrorCode::eParserListParseRangedPrefixError, ": '" + *item + "'");
                }
                if (endItem.erase(0, prefix.length()).find_first_not_of("0123456789") != string::npos)
                {
                    throw buildException(ParserErrorCode::eParserListParseRangedPrefixError, ": '" + *item + "'");
                }
                uint32_t start;
                uint32_t end;
                try
                {
                    if (validUnsigned32bitNumber(startItem, &start) == false)
                    {
                        throw buildException(ParserErrorCode::eParserListParseRangedPrefixError, ": '" + *item + "'");
                    }
                    if (validUnsigned32bitNumber(endItem, &end) == false)
                    {
                        throw buildException(ParserErrorCode::eParserListParseRangedPrefixError, ": '" + *item + "'");
                    }
                    if (start > end)
                    {
                        throw buildException(ParserErrorCode::eParserListParseRangedPrefixError, ": '" + *item + "'");
                    }
                }
                catch (const invalid_argument&)
                {
                    throw buildException(ParserErrorCode::eParserListParseRangedPrefixError, ": '" + *item + "'");
                }
                catch (const out_of_range&)
                {
                    throw buildException(ParserErrorCode::eParserListParseRangedPrefixError, ": '" + *item + "'");
                }
                for (unsigned long i = start; i <= end; ++i)
                {
                    uniqueSorted2.insert(i);
                }
            }
            else
            { // Not ranged
                if (item->find_first_not_of("0123456789", prefix.length()) != string::npos)
                {
                    throw buildException(ParserErrorCode::eParserListParseRangedPrefixError, ": '" + *item + "'");
                }
                uint32_t number;
                if (validUnsigned32bitNumber((*item).substr(prefix.length()), &number) == false)
                {
                    throw buildException(ParserErrorCode::eParserListParseRangedPrefixError, ": '" + *item + "'");
                }
                uniqueSorted2.insert(number);
            }
        }
        vector<string> finalList;
        for (auto it = uniqueSorted2.begin(); it != uniqueSorted2.end(); ++it)
        {
            finalList.push_back(prefix + to_string((unsigned long long)*it));
        }
        return finalList;
    }

    MicException CliParserImpl::buildException(ParserErrorCode code, const std::string& message) const
    {
        auto info = errorFormatter_->buildError((uint32_t)code, message);
        return MicException(info.first, info.second);
    }

    MicException CliParserImpl::buildAPIException(const std::string& message) const
    {
        return buildException(ParserErrorCode::eParserApiError, message);
    }
}; // namespace micmgmt
