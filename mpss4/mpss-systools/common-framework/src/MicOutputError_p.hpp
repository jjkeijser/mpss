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

#ifndef MICMGMT_MICOUTPUTERROR_HPP
#define MICMGMT_MICOUTPUTERROR_HPP

#include "MicErrorBase.hpp"
#ifdef UNIT_TESTS
#define PROTECTED public
#else
#define PROTECTED protected
#endif

namespace micmgmt
{
    enum MicOutputErrorCode : uint32_t
    {
        eMicOutputSuccess = 0, ///< No error occured.
        eMicOutputOutputError, ///< An error occured writing to the std::cout formatter.
        eMicOutputErrorError, ///< An error occured writing to the std::cerr formatter.
        eMicOutputStartSectionError, ///< An error occured writing to the std::cerr formatter.
        eMicOutputEndSectionError, ///< An error occured writing to the std::cerr formatter.
    };

    class MicOutputError : public MicErrorBase
    {
    private: // constants
        const uint32_t E_SIGNATURE_;

    private: // hide copy constructor and assignment operator so that this object cannot be copied.
        MicOutputError(const MicOutputError&);
        MicOutputError& operator=(const MicOutputError&);

    public:
        MicOutputError(const std::string& toolName = "");
        virtual ~MicOutputError();

    PROTECTED: // Derived class virtual implementation
        virtual uint32_t signature() const;
        virtual std::string lookupError(uint32_t code) const;
    }; // class MicOutputError
}; // namespace micmgmt

#endif // MICMGMT_MICOUTPUTERROR_HPP
