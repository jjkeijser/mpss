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

#ifndef MICMGMT_MICVALUE_HPP
#define MICMGMT_MICVALUE_HPP

// SYSTEM INCLUDES
//
#include    <cstdint>
#include    <ostream>
#include    <string>

// NAMESPACE
//
namespace  micmgmt
{
//============================================================================
//  CLASS:  MicValue
/** @class    micmgmt::MicValue   MicValue.hpp
 *  @ingroup  sdk
 *  @brief    This class template abstracts a value with its respective validity flag
 *
 */
template <class T>
class  MicValue
{
public:


    MicValue() :
        mValue(),
        mValid( false )
    {
        // Nothing to do
    }

    MicValue( const T& value ) :
        mValue( value ),
        mValid( true )
    {
        // Nothing to do
    }

    ~MicValue()
    {
        // Nothing to do
    }

    MicValue&  operator = ( const T& value )
    {
        mValue = value;
        mValid  = true;

        return  *this;
    }

    void reset()
    {
        mValue = T();
        mValid  = false;
    }

    const T& value() const {
        return mValue;
    }

    bool isValid() const {
        return mValid;
    }

    friend std::ostream& operator << ( std::ostream& out, const MicValue<T>& m)
    {
        return out << m.value();
    }

private:
    T mValue;
    bool mValid;

};

typedef MicValue<std::string>    MicValueString;
typedef MicValue<uint64_t>       MicValueUInt64;
typedef MicValue<uint32_t>       MicValueUInt32;
typedef MicValue<uint16_t>       MicValueUInt16;
typedef MicValue<uint8_t>        MicValueUInt8;
typedef MicValue<bool>           MicValueBool;

}   // namespace micmgmt

//----------------------------------------------------------------------------

#endif // MICMGMT_MICDATA_HPP
