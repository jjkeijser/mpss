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

#ifndef MICMGMT_MICTEMPERATURE_HPP
#define MICMGMT_MICTEMPERATURE_HPP

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
//  CLASS:  MicTemperature

class  MicTemperature : public MicSampleBase
{

public:

    MicTemperature();
    explicit MicTemperature( uint32_t sample, int exponent=eBase );
    MicTemperature( const std::string& name, uint32_t sample, int exponent=eBase );
    MicTemperature( const MicTemperature& that );
   ~MicTemperature();

    MicTemperature&  operator = ( uint32_t sample );
    MicTemperature&  operator = ( const MicTemperature& that );

    int              celsius() const;
    int              fahrenheit() const;


private:

    uint32_t         impDataMask() const;
    uint32_t         impStatusMask() const;
    int              impValue() const;
    std::string      impUnit() const;

};

//----------------------------------------------------------------------------

}   // namespace micmgmt

//----------------------------------------------------------------------------

#endif // MICMGMT_MICTEMPERATURE_HPP
