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

#include "MicException.hpp"

// For exception compatibility between gcc 4.4 and VS2013
#if defined(_MSC_VER) && _MSC_VER >= 1800
#   define NOEXCEPT
#else
#   define NOEXCEPT throw()
#endif

namespace micmgmt
{
    using namespace std;

    /// @class MicException MicException.hpp
    /// @brief Derived from \c std::exception and used by the Common Framework.
    /// @ingroup  common
    ///
    /// This class is used by the common framework to denote all types of errors that can be thrown
    /// from public interfaces exposed by a specific component.

    /// @brief Constructor for the \c micmgmt::MicException class.
    /// @param [in] micErrNo This is the numeric error (represented by a MicErrorCode enum) being thrown.
    /// @param [in] message Optional: This is specific text that may be useful to add to the error string (default="").
    ///
    /// This constructor creates the exception object that is thrown to the catch block.  It is recommended that
    /// throws are by value and catches are by reference.
    MicException::MicException(uint32_t micErrNo, const std::string& message)
        : micErrNo_(micErrNo), rawMessage_(message)
    {
    }

    /// @brief Constructor for the \c micmgmt::MicException class.
    /// @param [in] builtError This is the return value from MicErrorBase::buildError method.
    ///
    /// This constructor creates the exception object that is thrown to the catch block.  It is recommended that
    /// throws are by value and catches are by reference.
    MicException::MicException(const std::pair<uint32_t, std::string>& builtError)
        : micErrNo_(builtError.first), rawMessage_(builtError.second)
    {
    }

    MicException::MicException(const std::string& message)
        : micErrNo_(uint32_t(-1)), rawMessage_(message)
    {
    }

    /// @brief Destructor for the \c micmgmt::MicException class.
    ///
    /// This virtual destructor destroys the exception object.
    MicException::~MicException() NOEXCEPT
    {
    }

    /// @brief Returns the pre-built error message string.
    /// @return Returns a const char* pointer to the error message string.  The pointer is not valid after object is destroyed.
    ///
    /// Returns the pre-built error message string.
    const char* MicException::what() const NOEXCEPT
    {
        return rawMessage_.c_str();
    }

    /// @brief Returns the MicException class defined error code.
    /// @return Returns an <tt>unsigned int</tt> of the error code.
    ///
    /// Returns the MicException class defined error code.
    uint32_t MicException::getMicErrNo() const
    {
        return micErrNo_;
    }
}; // namespace micmgmt

#undef NOEXCEPT
