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

#include "SafeBool.hpp"

namespace micmgmt
{
    using namespace std;

    /// @class SafeBool
    /// @ingroup common
    /// @brief This class replaces C++11's atomic<bool> since its missing in gcc 4.4.
    ///
    /// This class replaces C++11's atomic<bool> since its missing in gcc 4.4.  This class contains
    /// all the needed operator overloads to be used as a bool.

    /// @brief Constructs a SafeBool with a initial value.
    /// @param [in] val [Optional; Default=<tt>false</tt>] The initial value to set in the SafeBool.
    ///
    /// Constructs a SafeBool with a initial value.
    SafeBool::SafeBool(bool val) : value_(val)
    {
    };

    /// @brief Empty destruction.
    ///
    /// Empty destruction.
    SafeBool::~SafeBool()
    {
    }

    /// @brief Overloaded equal operator to assign a \c bool to a SafeBool.
    /// @param [in] val [Required] The value to set atomically into the LHS SafeBool instance.
    /// @returns a reference to the SafeBool instance.
    ///
    /// Overloaded equal operator to assign a \c bool to a SafeBool.
    SafeBool& SafeBool::operator=(bool val)
    {
        setBool(val);
        return *this;
    }

    /// @brief Overloaded equal operator to assign a SafeBool to a SafeBool (copy operator).
    /// @param [in] val [Required] The value to set atomically into the LHS SafeBool instance.
    /// @returns a reference to the SafeBool instance.
    ///
    /// Overloaded equal operator to assign a SafeBool to a SafeBool (copy operator).
    SafeBool& SafeBool::operator=(SafeBool& safe)
    {
        if (&safe != this)
        {
            setBool(safe.getBool());
        }
        return *this;
    }

    /// @brief Cast operator to cast a SafeBool to a \c bool.
    ///
    /// Cast operator to cast a SafeBool to a \c bool.
    SafeBool::operator bool()
    {
        return getBool();
    }

    /// @brief Compare 2 SafeBool objects for equality.
    /// @param [in] rhs [Required] The right handed side of the comparison.
    /// @return Returns \c true is this->value_ == rhs.value_, \c false otherise.
    ///
    /// Compare 2 SafeBool objects for equality.
    bool SafeBool::operator==(SafeBool& rhs)
    {
        return (getBool() == rhs.getBool());
    }

    /// @brief Compare 2 SafeBool objects for inequality.
    /// @param [in] rhs [Required] The right handed side of the comparison.
    /// @return Returns \c true is this->value_ != rhs.value_, \c false otherise.
    ///
    /// Compare 2 SafeBool objects for inequality.
    bool SafeBool::operator!=(SafeBool& rhs)
    {
        return (getBool() != rhs.getBool());
    }

    // PRIVATE IMPLEMENTATION
    bool SafeBool::getBool()
    {
        mtx_.lock();
        bool val = value_;
        mtx_.unlock();
        return val;
    }

    bool SafeBool::setBool(bool val)
    {
        mtx_.lock();
        value_ = val;
        mtx_.unlock();
        return val;
    }
};
