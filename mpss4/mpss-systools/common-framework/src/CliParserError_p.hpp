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

#ifndef MICMGMT_CLIPARSERMICERROR_HPP
#define MICMGMT_CLIPARSERMICERROR_HPP

#include "MicErrorBase.hpp"
#ifdef UNIT_TESTS
#define PROTECTED public
#else
#define PROTECTED protected
#endif

namespace micmgmt
{
    enum ParserErrorCode : uint32_t
    {
        eParserSuccess = 0,
        eParserParseError,
        eParserApiError,
        eParserHelpError,
        eParserVersionError,
        eParserToolPosShortError,
        eParserToolPosLongError,
        eParserSubPosShortError,
        eParserSubPosLongError,
        eParserParseSpaceError,
        eParserDeviceRangeListError,
        eParserListParsePrefixError,
        eParserListParseRangedPrefixError,
        eParserInvalidArgumentSubError,
        eParserInvalidArgumentToolError,
        eParserNotSubcommandError,
        eParserUnknownShortOptionError,
        eParserUnknownLongOptionError,
        eParserUnexpectedOptionArgError,
        eParserShortOptionGroupError,
        eParserMissingOptionArgError,
        eParserInvalidOptionArgError,
        eParserMissingSubcommandError,
        eParserDuplicatedOptionsError,
    };

    class CliParserError : public MicErrorBase
    {
    private: // hide copy constructor and assignment operator so that this object cannot be copied.
        CliParserError(const CliParserError&);
        CliParserError& operator=(const CliParserError&);

    public:
        CliParserError(const std::string& toolName = "");
        virtual ~CliParserError();

    PROTECTED: // derived class virtual implementation
        virtual uint32_t signature() const;
        virtual std::string lookupError(uint32_t code) const;
    };
}; // namespace micmgmt
#endif // MICMGMT_CLIPARSERMICERROR_HPP
