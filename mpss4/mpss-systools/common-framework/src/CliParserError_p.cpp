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

#include "CliParserError_p.hpp"
#include <unordered_map>
#include <stdexcept>

namespace micmgmt
{
    using namespace std;

    namespace {
        const unordered_map<uint32_t, std::string> errorMap_ = {
            std::pair<uint32_t, std::string>(ParserErrorCode::eParserApiError, ""),
            std::pair<uint32_t, std::string>(ParserErrorCode::eParserParseError, "Parser error: "),
            std::pair<uint32_t, std::string>(ParserErrorCode::eParserHelpError, "Parser error: The '--help' option can only be used on the command line by itself or with a subcommand"),
            std::pair<uint32_t, std::string>(ParserErrorCode::eParserVersionError, "Parser error: The '--version' option can only be used on the command line by itself"),
            std::pair<uint32_t, std::string>(ParserErrorCode::eParserToolPosShortError, "Parser error: There are not enough tool positional arguments on the command line"),
            std::pair<uint32_t, std::string>(ParserErrorCode::eParserToolPosLongError, "Parser error: There are too many tool positional arguments on the command line"),
            std::pair<uint32_t, std::string>(ParserErrorCode::eParserSubPosShortError, "Parser error: There are not enough positional arguments on the command line for the subcommand"),
            std::pair<uint32_t, std::string>(ParserErrorCode::eParserSubPosLongError, "Parser error: There are too many positional arguments on the command line for the subcommand"),
            std::pair<uint32_t, std::string>(ParserErrorCode::eParserParseSpaceError, "An argument list cannot have spaces"),
            std::pair<uint32_t, std::string>(ParserErrorCode::eParserDeviceRangeListError, "Incorrect device list/range"),
            std::pair<uint32_t, std::string>(ParserErrorCode::eParserListParsePrefixError, "The argument list is not a correctly formatted prefixed list"),
            std::pair<uint32_t, std::string>(ParserErrorCode::eParserListParseRangedPrefixError, "The argument list item is not correctly formatted as a ranged list item"),
            std::pair<uint32_t, std::string>(ParserErrorCode::eParserInvalidArgumentSubError, "The argument was not validated for the given subcommand"),
            std::pair<uint32_t, std::string>(ParserErrorCode::eParserInvalidArgumentToolError, "The tool positional argument was not validated"),
            std::pair<uint32_t, std::string>(ParserErrorCode::eParserNotSubcommandError, "The tool's command line parsing expects the first non-option argument to be a subcommand"),
            std::pair<uint32_t, std::string>(ParserErrorCode::eParserUnknownShortOptionError, "An unknown short option specified on the command line"),
            std::pair<uint32_t, std::string>(ParserErrorCode::eParserUnknownLongOptionError, "An unknown long option specified on the command line"),
            std::pair<uint32_t, std::string>(ParserErrorCode::eParserUnexpectedOptionArgError, "An option has an unexpected argument"),
            std::pair<uint32_t, std::string>(ParserErrorCode::eParserShortOptionGroupError, "A short option was not the last option in a group of short options but requires a argument, this is not allowed"),
            std::pair<uint32_t, std::string>(ParserErrorCode::eParserMissingOptionArgError, "A required option argument is missing from the command line"),
            std::pair<uint32_t, std::string>(ParserErrorCode::eParserInvalidOptionArgError, "An option argument was not valid on the command line"),
            std::pair<uint32_t, std::string>(ParserErrorCode::eParserMissingSubcommandError, "A subcommand was expected on the command line but was missing"),
            std::pair<uint32_t, std::string>(ParserErrorCode::eParserDuplicatedOptionsError, "Duplicated options are not allowed"),
        };
    };

    /// @enum ParserErrorCode
    /// @brief Used in the CliParser component for a quick name for error codes.
    ///
    /// Used in the local components for a quick name for error codes.

    /// @class CliParserError
    /// @brief Derived from MicErrorBase and provided error handling for the MicOutput class.
    ///
    /// Derived from MicErrorBase and provided error handling for the MicOutput class.

    /// @brief Constructor to create a class to format output strings.
    /// @param [in] toolName Optional tool name that can be used when building the error string.
    ///
    /// Constructor to create a class to format output strings for ParserErrorCode errors.
    CliParserError::CliParserError(const std::string& toolName) : MicErrorBase(toolName)
    {
    }

    /// @brief This forces the derived class to use a virtual destructor.
    ///
    /// This forces the derived class to use a virtual destructor.  No other actions are performed in this destructor.
    CliParserError::~CliParserError()
    {
    }

   /// @brief This method returns the signature of the instaniated class.
   /// @returns The unique top most 3 bytes of the MicException error code that this class generates and uses.
   ///
   /// This method returns the signature of the instaniated class and must be implemented in each derived class.
   uint32_t CliParserError::signature() const
   {
       return 0x20020000; // Parser Error Signature
   }

   /// @brief This method returns the local error string based on the local error code (top 3 bytes cleared).
   /// @returns The string of the local error code without any specifics.
   /// @exception std::out_of_range Thrown when the error code is beyond the defined errors.
   ///
   /// This method returns the local error string based on the local error code (top 3 bytes cleared).
   std::string CliParserError::lookupError(uint32_t code) const
   {
       try
       {
           return errorMap_.at(code & ~E_MASK_);
       }
       catch (out_of_range&)
       {
           throw out_of_range("CliParserError: The error code given was not found in the error list");
       }
   }
}; // namespace micmgmt
