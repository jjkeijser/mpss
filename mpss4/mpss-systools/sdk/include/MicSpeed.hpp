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

#ifndef MICMGMT_MICSPEED_HPP
#define MICMGMT_MICSPEED_HPP

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
//  CLASS:  MicSpeed

class  MicSpeed : public MicSampleBase
{

public:

    MicSpeed();
    explicit MicSpeed( uint32_t sample, int exponent=eBase );
    MicSpeed( const std::string& name, uint32_t sample, int exponent=eBase );
    MicSpeed( const MicSpeed& that );
   ~MicSpeed();

    MicSpeed&    operator = ( uint32_t sample );
    MicSpeed&    operator = ( const MicSpeed& that );


private:

    int          impValue() const;
    std::string  impUnit() const;

};

//----------------------------------------------------------------------------

}   // namespace micmgmt

//----------------------------------------------------------------------------

#endif // MICMGMT_MICSPEED_HPP
