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

#ifndef MICMGMT_SAFEBOOL_HPP
#define MICMGMT_SAFEBOOL_HPP

#ifdef UNIT_TESTS
#define PRIVATE public
#else
#define PRIVATE private
#endif // UNIT_TESTS

#include <mutex>

namespace micmgmt
{
    class SafeBool
    {
    public: // API
        explicit SafeBool(bool val = false);
        ~SafeBool();

        // operators
        SafeBool& operator=(bool val);
        SafeBool& operator=(SafeBool& safe);
        operator bool();
        bool operator==(SafeBool& rhs);
        bool operator!=(SafeBool& rhs);

    private: // IMPLEMENTATION
        bool getBool();
        bool setBool(bool val);

    PRIVATE: // DATA
        bool       value_;
        std::mutex mtx_;
    };
}; // namespace micmgmt

#endif // MICMGMT_SAFEBOOL_HPP
