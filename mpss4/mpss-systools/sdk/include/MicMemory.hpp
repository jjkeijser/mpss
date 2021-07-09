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

#ifndef MICMGMT_MICMEMORY_HPP
#define MICMGMT_MICMEMORY_HPP

// PROJECT INCLUDES
//
#include    "MicSampleBase.hpp"

// SYSTEM INCLUDES
//
#include    <string>

// NAMESPACE
//
namespace  micmgmt
{


//============================================================================
//  CLASS:  MicMemory

class  MicMemory : public MicSampleBase
{

public:

    MicMemory();
    explicit MicMemory( uint32_t sample, int exponent=eBase );
    MicMemory( const std::string& name, uint32_t sample, int exponent=eBase );
    MicMemory( const MicMemory& that );
   ~MicMemory();

    MicMemory&  operator = ( uint32_t sample );
    MicMemory&  operator = ( const MicMemory& that );


private:

    double        scaled( double value, int exponent ) const;
    uint32_t      impDataMask() const;
    int           impValue() const;
    std::string   impUnit() const;

};

//----------------------------------------------------------------------------

}   // namespace micmgmt

//----------------------------------------------------------------------------

#endif // MICMGMT_MICMEMORY_HPP
