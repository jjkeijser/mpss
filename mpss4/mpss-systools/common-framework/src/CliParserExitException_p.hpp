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

#ifndef MICMGMT_CLIPARSEREXITEXCEPTION_HPP
#define MICMGMT_CLIPARSEREXITEXCEPTION_HPP

#include "CliParserError_p.hpp"
#include <string>
#include <exception>

// For exception compatibility between gcc 4.4 and VS2013
#if defined(_MSC_VER) && _MSC_VER >= 1800
#   define NOEXCEPT
#else
#   define NOEXCEPT throw()
#endif
namespace micmgmt
{
    class CliParserExitException : public std::exception
    {
    private:
        std::string     message_;
        std::string     optionContext_;
        std::string     subcommandContext_;
        ParserErrorCode errorCode_;

    private: // Disable default constructor.
        CliParserExitException() {};

    public:
        CliParserExitException(ParserErrorCode errorCode, const std::string& message,
                               const std::string& optionContext = "", const std::string& subcommandContext = "");
        virtual ~CliParserExitException() NOEXCEPT;
        const char* what() const NOEXCEPT;
        const char* optionContext() const;
        const char* subcommandContext() const;
        ParserErrorCode errorCode() const;
    }; // class MicException

}; // namespace micmgmt

// DON'T PROPAGATE NOEXCEPT WIDELY
#undef NOEXCEPT

#endif // MICMGMT_CLIPARSEREXITEXCEPTION_HPP
