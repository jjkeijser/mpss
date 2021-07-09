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

#include "MicOutputError_p.hpp"
#include "micmgmtCommon.hpp"
#include <unordered_map>
#include <stdexcept>

namespace micmgmt
{
    using namespace std;

    namespace {
        const unordered_map<uint32_t, std::string> errorMap_ = {
            std::pair<uint32_t, std::string>(MicOutputErrorCode::eMicOutputOutputError, "A MicOutput error occured writing to the std::cout formatter"),
            std::pair<uint32_t, std::string>(MicOutputErrorCode::eMicOutputErrorError, "A MicOutput error occured writing to the std::cerr formatter"),
            std::pair<uint32_t, std::string>(MicOutputErrorCode::eMicOutputStartSectionError, "A MicOutput error occured starting a new section"),
            std::pair<uint32_t, std::string>(MicOutputErrorCode::eMicOutputEndSectionError, "A MicOutput error occured ending a new section"),
        };
    };

    /// @enum MicOutputErrorCode
    /// @brief Used in the local components for a quick name for error codes.
    ///
    /// Used in the local components for a quick name for error codes.

    /// @class MicOutputError
    /// @brief Derived from MicErrorBase and provided error handling for the MicOutput class.
    ///
    /// Derived from MicErrorBase and provided error handling for the MicOutput class.

    /// @brief Constructor to create a class to format output strings.
    /// @param [in] toolName Optional tool name that can be used when building the error string.
    ///
    /// Constructor to create a class to format output strings for MicOutputErrorCode errors.
    MicOutputError::MicOutputError(const std::string& toolName) : MicErrorBase(toolName), E_SIGNATURE_(0x20030000)
    {
    }

    /// @brief This forces the derived class to use a virtual destructor.
    ///
    /// This forces the derived class to use a virtual destructor.  No other actions are performed in this destructor.
    MicOutputError::~MicOutputError()
    {
    }

    /// @brief This method returns the signature of the instaniated class.
    /// @returns The unique top most 3 bytes of the MicException error code that this class generates and uses.
    ///
    /// This method returns the signature of the instaniated class and must be implemented in each derived class.
    uint32_t MicOutputError::signature() const
    {
        return E_SIGNATURE_; // MicOutput Signature
    }

    /// @brief This method returns the local error string based on the local error code (top 3 bytes cleared).
    /// @returns The string of the local error code without any specifics.
    /// @exception std::out_of_range Thrown when the error code is beyond the defined errors.
    ///
    /// This method returns the local error string based on the local error code (top 3 bytes cleared).
    std::string MicOutputError::lookupError(uint32_t code) const
    {
        try
        {
            return errorMap_.at(code & ~E_MASK_);
        }
        catch (out_of_range&)
        {
            throw out_of_range("MicOutputError: The error code given was not found in the error list");
        }
    }
}; // namespace micmgmt
