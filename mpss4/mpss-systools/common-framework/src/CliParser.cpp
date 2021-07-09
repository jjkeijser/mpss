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

#include "CliParser.hpp"
#include "CliParserImpl.hpp"

namespace micmgmt
{
    using namespace std;

    /// @class CliParser
    /// @ingroup  common
    /// @brief Common parser used for tools.
    ///
    /// Common parser used for tool's development.

    /// @brief This constructor will initialize the object with the raw command line using the exact values
    /// passed to the <tt>main()</tt> function of the calling tool.
    /// @param [in] argc [Required] The count of raw arguments from <tt>main()</tt> of the tool.
    /// @param [in] argv [Required] The list of strings that compose the command line from the <tt>main()</tt>
    /// entry point of the tool.
    /// @param [in] toolVersion [Required] This is the current version string of the tool (i.e. "1.0" or
    /// "1.0.1", etc...).
    /// @param [in] publishYear [Required] This is the 4 digit (YYYY) year to be used in the copyright line
    /// output of this parser.
    /// @param [in] programName [Optional; Default=<tt>""</tt>] This is the tool's name used in all output from the
    /// parser. If an empty string is passed then the executable minus path or file extention is used.
    /// @param [in] shortDescription [Optional; Default=<tt>""</tt>] This is a short single paragraph description of
	/// the tool's purpose.
    /// @exception MicException This constructor will throw an exception if argc and argv do not conform to
    /// what is given in the <tt>int main(int argc, char* argv[])</tt> entry point.
    ///
    /// This constructor will initialize the object with the raw command line using the exact values passed to
    /// the <tt>main()</tt> function entry point of the calling tool. This constructor will automatcially add
    /// <tt>\--help</tt> and <tt>\--version</tt> options to the parser.  You must derive from this class to change
    /// this behaior.
    CliParser::CliParser(int argc, char* argv[], const std::string& toolVersion, const std::string& publishYear,
                         const std::string& programName, const std::string& shortDescription)
        : impl_(new CliParserImpl(argc, argv, toolVersion, publishYear, programName, shortDescription))
    {
    }

    /// @brief Unused destructor.
    ///
    /// Unused destructor.
    CliParser::~CliParser()
    {
    }

    /// @brief This method adds an option with full help and format description (short letter, optional argument, etc...)
    /// @param [in] name [Required] The long name of the option. This must be unique to the instance of tha parser, 2 or more
    /// characters with dashes allowed and not including the leading dashes.
    /// @param [in] help [Required] The list of help paragraphs for this option.
    /// @param [in] shortLetter [Optional; Default=\c 0] The single character of the short letter. This must be unique to the
    /// instance of the parser.
    /// @param [in] requiredArgument [Optional; Default=\c false] Flag denoting whether an agrument is required (\c true) by this
    /// option or not required but optional (\c false).
    /// @param [in] argumentName [Conditionally Required; Default=\c ""] If \a requiredArgument is \c true then this must be
    /// a non-empty string,
    /// if \a requiredArgument is \c false then this is optionally the name of an optional argument of the option.
    /// @param [in] argumentHelp [Optional; Default=\c {}] The list of help paragraphs for this option's argument.
    /// @param [in] defVal [Optional; Default=\c ""] This is the default value of the argument used if the argument is not
    /// required for this option.
    /// @param [in] callback [Optional; Default=\c NULL] This is a callback interface derived from
    /// EvaluateArgumentCallbackInterface that can be called to evaluate the content of this option's argument during parsing.
    /// @exception MicException This method will throw this exception when \a name or \a help are empty.
    /// @exception MicException This method will throw this exception  when \a requiredArgument is \c true but \a argumentName or \a argumentHelp is empty.
    /// @exception MicException This method will throw this exception when this method is called after the CliParser:parse method is called.
    ///
    /// This method adds an option to the parser with full help, argument, argument help and even a callback for evaluating
    /// the content of the parsed argument on the command line.
    /// @note If an option specific to a subcommand is added to the parser, this exclusivity will not be checked, you must
    /// check that logic in your tool's code.
    /// @note If you end a help paragraph with a '@' character the following blank line will be suppressed.
    void CliParser::addOption(const std::string& name, const std::vector<std::string>& help,
                              const char& shortLetter, const bool& requiredArgument,
                              const std::string& argumentName, const std::vector<std::string>& argumentHelp,
                              const std::string& defVal, EvaluateArgumentCallbackInterface* callback)
    {
        impl_->addOption(name, help, shortLetter, requiredArgument, argumentName, argumentHelp, defVal, callback);
    }

    /// @brief This adds a tool positional argument for the tool.
    /// @param [in] name [Required] The name used to identify the positional argument in the \a callback and in the help output.
    /// @param [in] help [Required] The list of paragraphs used to describe this positional argument.
    /// @param [in] callback [Optional; Default=\c NULL] This is a callback interface derived from
    /// EvaluateArgumentCallbackInterface that can be called to evaluate the content of this positional argument during parsing.
    /// @exception MicException This method will throw this exception when subcommands are being used, the \a name is empty or invalid,
    /// or the \a help is empty.
    /// @exception MicException This method will throw this exception when an attempt is made to insert the same argument name twice.
    /// @exception MicException This method will throw this exception when called after the CliParser:parse method is called.
    ///
    /// This adds a tool positional argument for the tool.  Tool positional arguments are required to be on the command line
    /// being parsed.  Use options with arguments for "optional positional arguments".
    /// @note If you end a help paragraph with a '@' character the following blank line will be suppressed.
    void CliParser::addToolPositionalArg(const std::string& name, const std::vector<std::string>& help,
                                         EvaluateArgumentCallbackInterface* callback)
    {
        impl_->addToolPositionalArg(name, help, callback);
    }

    /// @brief This method will add one subcommand to the parser.
    /// @param [in] subcommand [Required] This is the name of the subcommand as it will appear on the command line as the first
    /// non-option argument. This will be used in the \a callback of the subcommand arguments and in the help output. This name must
    /// be unique for this instance of the parser.
    /// @param [in] help [Required] This is the list of paragraphs used to describe the subcommand.
    /// @exception MicException This method will throw this exception when any tool positional arguments have already been inserted.
    /// @exception MicException This method will throw this exception when the \a name is empty or invalid, the \a help is empty, or the
    /// subcommand name already exists.
    /// @exception MicException This method will throw this exception when called after the CliParser:parse method is called.
    ///
    /// This method will add one subcommand to the parser. More than one can be added but using subcommands is mutually
    /// exclusive to tool positional arguments.  If you want subcommand positional arguments use the
    /// CliParser::addSubcommandPositionalArg method to add them.
    /// @note If you end a help paragraph with a '@' character the following blank line will be suppressed.
    void CliParser::addSubcommand(const std::string& subcommand, const std::vector<std::string>& help)
    {
        impl_->addSubcommand(subcommand, help);
    }

    /// @brief This method adds position arguments to a subcommand.
    /// @param [in] subcommandName [Required] Name of the subcommand that the positional argument will be appended to in order (order matters).
    /// @param [in] argName [Required] The name of the argument to add.  This will be used in \a callback and in the help output.
    /// This must only be unique for this \a subcommandName.
    /// @param [in] help [Required] The list of help paragraphs for the argument name for this subcommand.
    /// @param [in] callback [Optional; Default=\c NULL] This is a callback interface derived from
    /// EvaluateArgumentCallbackInterface that can be called to evaluate the content of this positional argument during parsing.
    /// @exception MicException This method will throw this exception when adding a subcommand positional argument after a tool positional argument has been added.
    /// @exception MicException This method will throw this exception when adding the same subcommand positional argument name twice.
    /// @exception MicException This method will throw this exception when any of the arguments to this method are empty (except \a callback).
    /// @exception MicException This method will throw this exception when this method is called after the CliParser:parse method.
    ///
    /// Like the tool positional argument method CliParser::addToolPositionalArg, this method adds position arguments to a
    /// specified \a subcommandName.
    /// Subcommand positional arguments are required to be on the command line being parsed.  Use options with arguments for
    /// "optional positional arguments".
    /// @note If an option specific to a subcommand is added to the parser, this exclusivity will not be checked, you must
    /// check that logic in your tool's code.
    /// @note If you end a help paragraph with a '@' character the following blank line will be suppressed.
    void CliParser::addSubcommandPositionalArg(const std::string& subcommandName, const std::string& argName,
                                               const std::vector<std::string>& help,
                                               EvaluateArgumentCallbackInterface* callback)
    {
        impl_->addSubcommandPositionalArg(subcommandName, argName, help, callback);
    }

    /// @brief This method will parse the command line using the exact values passed to the constructor of this parser.
    /// @param allowGeneralArguments [Default=<tt>false</tt>] Specifies to allow general arguments (\c true) or only
    /// required positional arguments or subcommands and optionsl arguments (\c false).
    /// @return This method returns \c true is the parsing was successful or \c false if the parsing failed.  If \c false
    /// is returned you should cleanup and exit immediatly.
    /// @exception MicException This method will throw this exception when called out of order.
    /// @exception MicException This method will throw this exception when \a allowGeneralArguments is \c false and no tool
    /// positional arguments or subcommand positional arguments were specified.
    /// @exception MicException This method will throw this exception when called more than once or there is an output error.
    ///
    /// This method will parse the command line using the exact values passed to the <tt>main()</tt> function of the tool.
    /// If noexceptions are thrown then then the return value is valid.
    ///
    /// @note The CliParser::parse method can only be used once per instantiation.
    bool CliParser::parse(bool allowGeneralArguments)
    {
        return impl_->parse(allowGeneralArguments);
    }

    /// @brief This is to check why the CliParser::parse method returned false.
    /// @return This will return \c true if the parser encountered a parsing error, otherwise it will return \c false.
    /// @exception MicException This method will throw this exception when called before CliParser::parse method.
    ///
    /// This is to check why the CliParser::parse method returned false.  The CliParser::parse methoid will return \c false
    /// for 2 reasons; 1) There was a parse error, this will return \c true for that case. 2) Either <tt>--help</tt> or
    /// <tt>--version</tt> was requested and the parser handled that output with no error.  This will return \c false in this case.
    bool CliParser::parseError() const
    {
        return impl_->parseError();
    }

    /// @brief This method return \c true if this instance of the parser was used tp parse the commandline, or \c false otherwise.
    /// @return \c true if the CliParser::parse method was called, otherwise \c false.
    ///
    /// This method return \c true if this instance of the parser was used to parse the commandline, or \c false otherwise.
    ///
    /// @note The CliParser::parse method can only be used once per instantiation.
    bool CliParser::parsed() const
    {
        return impl_->parsed();
    }

    /// @brief The method returns the parsed subcommand if the tool is setup to use subcommands and CliParser::parse has been run.
    /// @exception MicException This method will throw this exception if either of the 2 conditions are not met
    /// (CliParser::parse called and subcommands configured).
    /// @return The name of the parsed subcommand from the commandline.
    ///
    /// The method returns the parsed subcommand if the tool is setup to use subcommands and CliParser::parse has been run.
    const std::string& CliParser::parsedSubcommand() const
    {
        return impl_->parsedSubcommand();
    }

    /// @brief This retrieves a parsed option by option name.
    /// @param [in] optionName [Required] This is the name of the option to return parsed information about.
    /// @exception MicException This method will throw this exception when an option specified doesn't exist in the options added prior to
    /// parsing.
    /// @exception MicException This method will throw this exception when called before CliParser::parse method.
    /// @return A <tt>std::tuple<bool,bool,string></tt> where the <tt>get<0></tt> element represents \c true if the option was present on the
    /// command line, \c false if the option was not present on the command line. The <tt>get<1></tt> \c bool element
    /// represents \c true when an optional option argument exists on the command or \c false otherwise.  The <tt>get<2></tt> element is
    /// either an the argument found on the command line or the default value for the argument specified in the CliParser::addOption
    /// method.
    ///
    /// This retrieves a parsed option by option name or the default value if the option was not in the parsed command line.  The
    /// \a optionName must be non-empty and must exist in the internal list of added options added from the CliParser::addOption
    /// method.
    std::tuple<bool, bool, std::string> CliParser::parsedOption(const std::string& optionName) const
    {
        return impl_->parsedOption(optionName);
    }

    /// @brief This method returns the starting iterator position for the tool or subcommand positional arguments.
    /// @return The starting iterator position for the tool or subcommand positional arguments or <tt>iterator::end()</tt>
    /// if the list is empty.
    /// @exception MicException This method will throw this exception when called before CliParser::parse method.
    ///
    /// This method returns the starting iterator position for the tool or subcommand positional arguments.
    std::vector<std::string>::iterator CliParser::parsedPositionalArgumentBegin()
    {
        return impl_->parsedPositionalArgumentBegin();
    }

    /// @brief This method returns the ending iterator position for the tool or subcommand positional arguments.
    /// @return The ending iterator position for the tool or subcommand positional arguments.
    /// @exception MicException This method will throw this exception when called before CliParser::parse method.
    ///
    /// This method returns the ending iterator position for the tool or subcommand positional arguments.
    std::vector<std::string>::iterator CliParser::parsedPositionalArgumentEnd()
    {
        return impl_->parsedPositionalArgumentEnd();
    }

    /// @brief This method returns the positional argument for the tool or subcommand depending on parser configuration.
    /// @param [in] index This is the zero based index of the positional argument to obtain.
    /// @return The parsed positional argument string for the tool or subcommand.
    /// @exception MicException This method will throw this exception when called before CliParser::parse method.
    /// @exception MicException This method will throw this exception when the index is greater than or equal to the
    /// length of the internal data collection.
    ///
    /// This method returns the positional argument for the tool or subcommand depending on parser configuration.
    const std::string& CliParser::parsedPositionalArgumentAt(std::vector<std::string>::size_type index) const
    {
        return impl_->parsedPositionalArgumentAt(index);
    }

    /// @brief This method will attempt to parse the positional argument at \a index as a device list/range.
    /// @param index The index of the parsed argument. The argument is expected to be a comma-separated
    //         list of individual device names or device ranges, e.g. "mic0,mic1-mic3,mic4".
    /// @return Returns a vector of strings containing the device names expanded from parsed argument.
    /// @exception MicException This method will throw an exception when the argument can't be correctly
    //             parsed as a device list/range.
    std::vector<std::string> CliParser::parsedPositionalArgumentAsDeviceList(std::vector<std::string>::size_type index) const
    {
        return impl_->parsedPositionalArgumentAsDeviceList(index);
    }

    /// @brief This method returns the size of the positional argument list for the tool or subcommand depending on the
    /// parser configuration.
    /// @return The parsed positional argument count for the tool or subcommand.
    /// @exception MicException This method will throw this exception when called before CliParser::parse method.
    ///
    /// This method returns the size of the positional argument list for the tool or subcommand depending on the parser
    /// configuration.
    std::vector<std::string>::size_type CliParser::parsedPositionalArgumentSize()
    {
        return impl_->parsedPositionalArgumentSize();
    }

    /// @brief This method is used to set formatters other than the ConsoleOutputFormatter to be used by the parser.
    /// @param [in] stdOut [Optional; Default=\c NULL] The new MicOutputFormatterBase derived class to use for all
    /// parser output to \c cout. \c NULL indicates to use \c std::cout.
    /// @param [in] stdErr [Optional; Default=\c NULL] The new MicOutputFormatterBase derived class to use for all
    /// parser output to \c cerr.
    /// @param [in] fileOut [Optional; Default=\c NULL] The new MicOutputFormatterBase derived class to use for all
    /// parser output to user specified \c std::ostream. \c NULL indicates to not use a file \c std::ostream.
    ///
    /// This method is used to set a formatter other than the ConsoleOutputFormatter to be used by the parser.
    ///
    /// @note This method takes full ownership of the formatters passed into it.  The pointers are stored in unique
    /// pointers <tt>std::unique_ptr<MicOutputFormatterBase></tt> so when the CliParser instance goes out of scope
    /// or new formatters are set the <tt>std::unique_ptr<MicOutputFormatterBase></tt> derived instances will be
    /// deleted.  Do not use the address of memory you own, please use new to create the
    /// <tt>std::share_ptr<MicOutputFormatterBase></tt> derived instances.  This does not mean the underlying
    /// \c std::ostream instances will be deleted, the caller owns the \c std::ostream objects.
    /// @note If formatters are added that throw exceptions not in the std::exception derived tree or exceptions
    /// from overridden methods then ANY method in the CliParser that may display an error or otherwise output
    /// using the formatters can throw the said exception.  The caller of the parser should expect and handle
    /// these exceptions.
    void CliParser::setOutputFormatters(MicOutputFormatterBase* stdOut, MicOutputFormatterBase* stdErr,
                                        MicOutputFormatterBase* fileOut)
    {
        impl_->setOutputFormatters(stdOut, stdErr, fileOut);
    }

    /// @brief This method will output the tool name and version information with the copyright line.
    /// @exception MicException This method will throw this exception when called before CliParser::parse method.
    ///
    /// This method will output the tool name and version information with the copyright line.  This will be called internally
    /// when <tt>\--version</tt> is specified or <tt>\--help</tt> is specified on the command line.
    void CliParser::outputVersion() const
    {
        impl_->outputVersion();
    }

    /// @brief This method will output the help information for the configuration of the parser.
    /// @param subcommandName [Optional; Default=\c ""] The subcommand to show usage for or if empty all usages will be shown.
    /// @exception MicException This method will throw this exception when called before CliParser::parse method.
    ///
    /// This method will output the full help information for the configuration of the parser when <tt>\--help</tt> is
    /// specified on the command line.  This method will be filtered when the option is non-empty (see Note below).
    /// When \a subcommandName is used to filter then the help will display only the help for that subcommand (include all options).
    void CliParser::outputHelp(const std::string& subcommandName)
    {
        impl_->outputHelp(subcommandName);
    }

    /// @brief This adds optional help sections to the bottom of the full help output.
    /// @param sectionName This is the displayed section header for the help section. This cannot be empty.
    /// @param paragraphs These are the paragraphs for the help section specfied by \a sectionName. This
    /// <tt>std::vector<std::string></tt> must contain at least one paragraph.
    /// @exception MicException This method will throw this exception when the 2 conditions for the parameters are not met.
    /// The conditions are non-empty \a sectionName and non-empty \a paragraphs vector.
    /// @exception MicException This method will throw this exception when called after CliParser::parse method.
    ///
    /// This adds optional help sections to the bottom of the full help output.
    void CliParser::addHelpSection(const std::string& sectionName, const std::vector<std::string>& paragraphs)
    {
        impl_->addHelpSection(sectionName, paragraphs);
    }

    /// @brief This method will return the argument to the named option as a list.
    /// @param optionName The named option to get the argument as a list.
    /// @param prefix If this is empty <tt>""</tt> then this simple list is returned in the order specified on the
    /// command line. If this argument is non-empty without numeric characters then this function will return an acending
    /// ordered list, parsing ranges, to the caller.
    /// @return Returns the option argument to the option parsed as a list (std::vector<std::string>)
    /// @exception MicException This method will throw this exception when the rules are not followed when
    /// using this method.  See the note below for the rules.
    /// @exception MicException This method will throw this exception when the passed option was not found.
    /// @exception MicException This method will throw this exception when called before CliParser::parse method.
    ///
    /// This method will return the argument to the named option as a list.
    /// @note When using the \a prefix paramater, the following rules must be used:
    /// @li \b Every item in the list must contain the prefix or none should contain it (extra alpha charaters anywhere will cause an exception).
    /// @li Ranges are specfied with <tt>prefixN-prefixM</tt> where N < M.
    /// @li All ranges or individual items in the list must be seperated by a comma.
    /// @li Whitespace is not allowed in the list on the command line.
    std::vector<std::string> CliParser::parsedOptionArgumentAsVector(const std::string& optionName, const std::string& prefix) const
    {
        return impl_->parsedOptionArgumentAsVector(optionName, prefix);
    }

    /// @brief This method will return the argument to the named positional argument as a list.
    /// @param index The index of the positional tool argument or subcommand positional argument to retrieve
    /// as a list.
    /// @param prefix If this is empty <tt>""</tt> then this simple list is returned in the order specified
    /// on the command line. If this argument is non-empty without numeric characters then this function will
    /// return an acending ordered list, parsing ranges, to the caller.
    /// @return Returns the positional argument to the option parsed as a list (std::vector<std::string>)
    /// @exception MicException This method will throw this exception when the rules are not followed when
    /// using this method.  See the note below for the rules.
    /// @exception MicException This method will throw this exception when called before CliParser::parse method.
    ///
    /// This method will return the argument to the named positional argument as a list.
    /// @note When using the \a prefix paramater, the following rules must be used:
    /// @li \b Every item in the list must contain the prefix or none should contain it.  Extra alpha
    /// charaters anywhere will cause an exception.
    /// @li Ranges are specfied with <tt>prefixN-prefixM</tt> where N < M.
    /// @li All ranges or individual items in the list must be seperated by a comma.
    /// @li Whitespace is not allowed in the list on the command line.
    std::vector<std::string> CliParser::parsedPositionalArgumentAsVector(std::vector<std::string>::size_type index, const std::string& prefix) const
    {
        return impl_->parsedPositionalArgumentAsVector(index, prefix);
    }

    /// @brief This method will attempt to parse the argument \a optionName as a device list/range.
    /// @param optionName The name of the optional argument. The argument is expected to be a comma-separated
    //         list of individual device names or device ranges, e.g. "mic0,mic1-mic3,mic4".
    /// @return Returns a vector of strings containing the device names expanded from parsed option.
    /// @exception MicException This method will throw an exception when the argument can't be correctly
    //             parsed as a device list/range.
    std::vector<std::string> CliParser::parsedOptionArgumentAsDeviceList(const std::string& optionName) const
    {
        return impl_->parsedOptionArgumentAsDeviceList(optionName);
    }


    /// @brief This method will return the current program name used in the parser instance.
    /// @return The name of the program or tool as a std::string.
    ///
    /// This method will return the current program name used in the parser instance. This is either the name passed to the
    /// constructor or derived from <tt>argv[0]</tt> from <tt>main()</tt>.
    const std::string& CliParser::toolName() const
    {
        return impl_->toolName();
    }
}; // namespace micmgmt
