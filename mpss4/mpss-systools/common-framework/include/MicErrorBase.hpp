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

#ifndef MICMGMT_MICERROR_HPP
#define MICMGMT_MICERROR_HPP

#include <cstdint>
#include <string>
#include <utility>

namespace micmgmt
{
    enum MicErrorCode : uint32_t
    {
        eSuccess = 0,     ///< No error occured.
        eGeneralError,    ///< General program error has occured
        eUnknownError,    ///< Some unknown condition has occured
        eOutOfMemoryError ///< Implementation failed to allocate memory
    };

    class MicErrorBase
    {
    protected: // Constant for this and derived classes
        const uint32_t E_MASK_; ///< This protected field stores the correct mask for the error code for the derived class.

    private:
        std::string toolName_;

    private: // hide copy constructor and assignment operator so that this object cannot be copied.
        MicErrorBase(const MicErrorBase&);
        MicErrorBase& operator=(const MicErrorBase&);

    public: // Interface
        MicErrorBase(const std::string& toolName = "");
        virtual ~MicErrorBase();

        std::string localErrorStringFromGlobalCode(uint32_t fullErrorCode) const;
        const std::string& toolName() const;

        virtual std::pair<uint32_t, std::string> buildError(uint32_t code, const std::string& specialMessage = "") const;

    protected: // derived classes must implement these method in order to correctly build strings.
        virtual uint32_t signature() const;
        virtual std::string lookupError(uint32_t code) const;
    }; // class MicErrorBase
}; // namespace micmgmt

#endif // MICMGMT_MICERROR_HPP
