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

#ifndef MICMGMT_PCIADDRESS_HPP
#define MICMGMT_PCIADDRESS_HPP

// SYSTEM INCLUDES
//
#include    <cstdint>
#include    <string>

// NAMESPACE
//
namespace  micmgmt
{


//============================================================================
//  CLASS:  PciAddress

class  PciAddress
{

public:

    PciAddress();
    PciAddress( uint32_t address );
    PciAddress(  int funct,  int device,  int bus=0,  int domain=0 );
    PciAddress( const std::string& address );
    PciAddress( const PciAddress& that );
   ~PciAddress();

    PciAddress&  operator = ( uint32_t address );
    PciAddress&  operator = ( const std::string& address );
    PciAddress&  operator = ( const PciAddress& that );

    bool         isValid() const;
    int          domain() const;
    int          bus() const;
    int          device() const;
    int          function() const;
    std::string  asString() const;

    void         clear();


private:

    uint32_t  m_address;
    bool      m_valid;

};

//----------------------------------------------------------------------------

}   // namespace micmgmt

//----------------------------------------------------------------------------

#endif // MICMGMT_PCIADDRESS_HPP
