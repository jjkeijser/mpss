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

#include "MicErrorBase.hpp"
#include "micmgmtCommon.hpp"
#include <unordered_map>
#include <stdexcept>

namespace micmgmt
{
    using namespace std;

    namespace {
        const unordered_map<uint32_t, std::string> errorMap_ = {
            std::pair<uint32_t, std::string>(MicErrorCode::eGeneralError, "General error"),
            std::pair<uint32_t, std::string>(MicErrorCode::eUnknownError, "Unknown error"),
            std::pair<uint32_t, std::string>(MicErrorCode::eOutOfMemoryError, "Out of memory")
            //std::pair<uint32_t, std::string>(MicErrorCode::, ""),
        };
    };

    /// @enum MicErrorCode Used in the local components for a quick name for error codes.

    /// @class MicErrorBase
    /// @ingroup  common
    /// @brief This is a base class from which all component implementations are to derive.
    ///
    /// This is a base class for all component implementations to derive from that need to resolve local
    /// error codes to global error default codes and error strings.
    /// @section Usage Deriving from this class
    /// This base class contains minimal implementation and should be used to create derived classes for the
    /// individual components or tools.  Each component or tool would have its own enum for internal use.
    /// The MicErrorBase::signature and MicErrorBase::lookupError methods MUST be overridden in the derived
    /// class and return the correct signature and error string respectively.
    ///
    /// <pre>
    /// class MyError : public MicErrorBase
    /// {
    /// protected:
    ///     virtual uint32_t signature() {
    ///         return \<your signature here; MSB 3 bytes with LSB =0xff\>
    ///     }
    ///
    ///     virtual std::string lookupError(uint32_t fullErrorCodeWithSignature) {
    ///         return \<your error string here from your lookup table\>
    ///     }
    /// }
    ///
    /// // Passing to other entities via method:
    /// void method(const MicErrorBase& errorInst, uint32_t localCodeEnum, std::string msg)
    /// {
    ///     // Blah-blah
    ///     errorInst.buildError(localCodeEnum, message); // Calls derived versions of signature() and lookupError()
    ///     // Blah-blah
    /// }
    /// </pre>

    /// @brief Constructor to create a class to format output strings.
    /// @param [in] toolName Optional tool name that can be used when building the error string.
    ///
    /// Constructor to create a class to format output strings for micmgmt::MicErrorCode errors.
    MicErrorBase::MicErrorBase(const std::string& toolName) : E_MASK_(0xffffff00), toolName_(toolName)
    {
        feTrim(toolName_);
    }

    /// @brief This forces the derived class to use a virtual destructor.
    ///
    /// This forces the derived class to use a virtual destructor.  No other actions are performed in this destructor.
    MicErrorBase::~MicErrorBase()
    {
    }

    /// @param code This is the error code in the range 0-255 for the base class set of common errors
    /// (micmgmt::MicErrorCode).
    /// @param specialMessage String appended to built string (no extra spacing or other delimiter characters
    /// are inserted).
    /// @return Returns a std::pair<uint32_t,std::string> of <b>full error code</b> and <b>complete error message</b>.
    /// @exception std::out_of_range Thrown when the given local error code is out of range of the defined errors.
    ///
    /// Returns a fully composed error code and error message from the inputs and tool name passed in the constructor.
    std::pair<uint32_t, std::string> MicErrorBase::buildError(uint32_t code, const std::string& specialMessage) const
    {
        string msg = "Error: ";
        code &= ~E_MASK_;
        uint32_t fullCode = (code != 0) ? (code | signature()) : 0;
        if (toolName_.empty() == false)
        {
            msg += toolName_ + ": ";
        }
        if (fullCode != 0)
        {
            msg += lookupError(code);
        }
        if (specialMessage.empty() == false)
        {
            msg += specialMessage;
        }
        return pair<uint32_t,string>(fullCode, msg);
    }

    /// @brief Returns the local error string based on a full MicException error code.
    /// @param [in] fullErrorCode The full error code from a a MicException object.
    /// @return The stored string for the error code (minus any specifics).  Empty means a string was not found in the
    /// list or the full error code did not match the correct signature for this class.
    /// @exception std::out_of_range Thrown when the error code exceeds the errors allowed by this class.
    /// @exception std::invalid_argument Thrown when the code passed does not match the signature of tha class.
    ///
    /// Returns the local error string based on a full MicException error code.
    std::string MicErrorBase::localErrorStringFromGlobalCode(uint32_t fullErrorCode) const
    {
        if ((fullErrorCode & E_MASK_) != signature())
        {
            throw std::invalid_argument("The MicException error code is not valid for this instance");
        }
        else
        {
            return lookupError(fullErrorCode & ~E_MASK_);
        }
    }

    /// @brief This method returns the signature of the instaniated class.
    /// @returns The unique top most 3 bytes of the MicException error code that this class generates and uses.
    ///
    /// This method returns the signature of the instantiated class and must be implemented in each derived class.
    uint32_t MicErrorBase::signature() const
    {
        return 0x20000000;
    }

    /// @brief This method returns the local error string based on the local error code (top 3 bytes cleared).
    /// @param [in] code The error code from MicException that is looked up to return the error string.
    /// @returns The string of the local error code without any specifics.
    /// @exception std::out_of_range Thrown when the error code is beyond the defined errors.
    ///
    /// This method returns the local error string based on the local error code (top 3 bytes cleared).
    std::string MicErrorBase::lookupError(uint32_t code) const
    {
        try
        {
            return errorMap_.at(code & ~E_MASK_);
        }
        catch (out_of_range&)
        {
            throw out_of_range("MicErrorBase: The error code given was not found in the error list");
        }
    }

    /// @brief Returns the tool name and is accessable by derived classes.
    /// @return The name of the tool or empty string if not set.
    ///
    /// Returns the tool name and is accessible by derived classes.
    const std::string& MicErrorBase::toolName() const
    {
        return toolName_;
    }
}; // namespace micmgmt
