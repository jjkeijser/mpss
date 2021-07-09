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

#include "CliParserExitException_p.hpp"

// For exception compatibility between gcc 4.4 and VS2013
#if defined(_MSC_VER) && _MSC_VER >= 1800
#   define NOEXCEPT
#else
#   define NOEXCEPT throw()
#endif

namespace micmgmt
{
    using namespace std;

    CliParserExitException::CliParserExitException(ParserErrorCode errorCode, const std::string& message,
                                                   const std::string& optionContext,
                                                   const std::string& subcommandContext)
        : message_(message), optionContext_(optionContext), subcommandContext_(subcommandContext), errorCode_(errorCode)
    {
    }

    CliParserExitException::~CliParserExitException() NOEXCEPT
    {
    }

    const char* CliParserExitException::what() const NOEXCEPT
    {
        return message_.c_str();
    }

    const char* CliParserExitException::optionContext() const
    {
        return optionContext_.c_str();
    }

    const char* CliParserExitException::subcommandContext() const
    {
        return subcommandContext_.c_str();
    }

    ParserErrorCode CliParserExitException::errorCode() const
    {
        return errorCode_;
    }
}; // namespace micmgmt

#undef NOEXCEPT
