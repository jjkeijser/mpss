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

#include <gtest/gtest.h>
#include "CliParserError_p.hpp"
#include <unordered_map>

namespace micmgmt
{
    using namespace std;

    namespace
    {
        const unordered_map<uint32_t, string> rawErrorStrings = {
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
        };

        const CliParserError errorBuilder("toolName");

        std::pair<uint32_t, std::string> goldenAlgorithm(uint32_t code, const std::string& specialMessage)
        {
            string msg = "Error: ";
            code &= 0xff;
            uint32_t fullCode = (code != 0) ? (code | 0x20020000) : (0);
            msg += "toolName: ";
            if (code > (uint32_t)0)
            {
                msg += rawErrorStrings.at(code);
            }
            if (specialMessage.empty() == false)
            {
                msg += specialMessage;
            }
            return pair<uint32_t, string>(fullCode, msg);
        }
    }

    TEST(common_framework, TC_KNL_mpsstools_CliParserError_001)
    {
        auto golden = goldenAlgorithm(ParserErrorCode::eParserSuccess, "");
        pair<uint32_t, string> result;

        EXPECT_STREQ("toolName", errorBuilder.toolName().c_str());

        EXPECT_NO_THROW(result = errorBuilder.buildError(ParserErrorCode::eParserSuccess, ""));
        EXPECT_EQ(golden.first, result.first);
        EXPECT_STREQ(golden.second.c_str(), result.second.c_str());

        for (uint32_t code = ParserErrorCode::eParserApiError; code <= ParserErrorCode::eParserInvalidOptionArgError; ++code)
        {
            golden = goldenAlgorithm(code, "");
            EXPECT_NO_THROW(result = errorBuilder.buildError(code, ""));
            EXPECT_EQ(golden.first, result.first);
            EXPECT_STREQ(golden.second.c_str(), result.second.c_str());
        }
    }

    TEST(common_framework, TC_KNL_mpsstools_CliParserError_002)
    {
        auto golden = goldenAlgorithm(ParserErrorCode::eParserSuccess, "There is more to life");
        pair<uint32_t, string> result;

        EXPECT_STREQ("toolName", errorBuilder.toolName().c_str());

        EXPECT_NO_THROW(result = errorBuilder.buildError(ParserErrorCode::eParserSuccess, "There is more to life"));
        EXPECT_EQ(golden.first, result.first);
        EXPECT_STREQ(golden.second.c_str(), result.second.c_str());

        for (uint32_t code = ParserErrorCode::eParserApiError; code <= ParserErrorCode::eParserInvalidOptionArgError; ++code)
        {
            golden = goldenAlgorithm(code, "There is more to life");
            EXPECT_NO_THROW(result = errorBuilder.buildError(code, "There is more to life"));
            EXPECT_EQ(golden.first, result.first);
            EXPECT_STREQ(golden.second.c_str(), result.second.c_str());
        }
    }

    TEST(common_framework, TC_KNL_mpsstools_CliParserError_003)
    {
        EXPECT_THROW(errorBuilder.lookupError(0xff), std::out_of_range);
    }
};
