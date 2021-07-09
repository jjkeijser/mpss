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

#ifndef MICMGMT_MICPOWER_HPP
#define MICMGMT_MICPOWER_HPP

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
//  CLASS:  MicPower

class  MicPower : public MicSampleBase
{

public:

    MicPower();
    explicit MicPower( uint32_t sample, int exponent=eBase );
    MicPower( const std::string& name, uint32_t sample, int exponent=eBase );
    MicPower( const MicPower& that );
   ~MicPower();

    MicPower&    operator = ( uint32_t sample );
    MicPower&    operator = ( const MicPower& that );


private:

    int          impValue() const;
    std::string  impUnit() const;

};

//----------------------------------------------------------------------------

}   // namespace micmgmt

//----------------------------------------------------------------------------

#endif // MICMGMT_MICPOWER_HPP
