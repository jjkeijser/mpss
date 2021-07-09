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

#ifndef MICMGMT_MICEXCEPTION_HPP
#define MICMGMT_MICEXCEPTION_HPP

#include <cinttypes>
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
    class MicException : public std::exception
    {
    private:
        uint32_t     micErrNo_;
        std::string  rawMessage_;

    private: // Disable default constructor.
        MicException();

    public:
        MicException(uint32_t micErrNo, const std::string& message = "");
        MicException(const std::string& message);
        MicException(const std::pair<uint32_t, std::string>& builtError);
        virtual ~MicException() NOEXCEPT;
        const char* what() const NOEXCEPT;
        uint32_t getMicErrNo() const;
    }; // class MicException

}; // namespace micmgmt

// DON'T PROPAGATE NOEXCEPT WIDELY
#undef NOEXCEPT

#endif // MICMGMT_MICEXCEPTION_HPP
