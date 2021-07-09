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

// PROJECT INCLUDES
//
#include    "PciAddress.hpp"

// SYSTEM INCLUDES
//
#include    <sstream>

// NAMESPACES
//
using namespace  micmgmt;
using namespace  std;


//============================================================================
/** @class    micmgmt::PciAddress      PciAddress.hpp
 *  @ingroup  sdk
 *  @brief    The class represents a PCI address
 *
 *  The \b %PciAddress class encapsulates a PCI address.
 */


//============================================================================
//  P U B L I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     PciAddress::PciAddress()
 *
 *  Construct an empty invalid PCI address object.
 */

PciAddress::PciAddress() :
    m_valid( false )
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     PciAddress::PciAddress( uint32_t address )
 *  @param  address  Encoded address
 *
 *  Construct a PCI address object based on specified encoded \a address value.
 *
 *  Any integer value type that can implicitly converted to uint32_t is
 *  accepted as \a address.
 */

PciAddress::PciAddress( uint32_t address ) :
    m_address( address ),
    m_valid( true )
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     PciAddress::PciAddress( int funct, int device, int bus, int domain )
 *  @param  funct   PCI function number
 *  @param  device  PCI device number
 *  @param  bus     PCI bus number (optional)
 *  @param  domain  PCI domain number (optional)
 */

PciAddress::PciAddress( int funct, int device, int bus, int domain )
{
    m_address  = (domain << 16) & 0xffff0000;
    m_address |= (bus    <<  8) & 0x0000ff00;
    m_address |= (device <<  3) & 0x000000f8;
    m_address |= (funct  <<  0) & 0x00000007;

    m_valid = true;
}


//----------------------------------------------------------------------------
/** @fn     PciAddress::PciAddress( const std::string& address )
 *  @param  address  PCI address string
 *
 *  Construct a PCI address object based on specified PCI \a address string.
 *
 *  The \a address string is also accepted as const char* string.
 */

PciAddress::PciAddress( const std::string& address )
{
    *this = address;    // Call assignment operator
}


//----------------------------------------------------------------------------
/** @fn     PciAddress::PciAddress( const PciAddress& that )
 *  @param  that    Other PCI address object
 *
 *  Construct a PCI address object as deep copy of \a that object.
 */

PciAddress::PciAddress( const PciAddress& that )
{
    m_address = that.m_address;
    m_valid   = that.m_valid;
}


//----------------------------------------------------------------------------
/** @fn     PciAddress::~PciAddress()
 *
 *  Cleanup.
 */

PciAddress::~PciAddress()
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     PciAddress&  PciAddress::operator = ( uint32_t address )
 *  @param  address  Raw PCI address
 *  @return reference to this object
 *
 *  Assign specified \a address to this PCI address.
 */

PciAddress&  PciAddress::operator = ( uint32_t address )
{
    m_address = address;
    m_valid   = true;

    return  *this;
}


//----------------------------------------------------------------------------
/** @fn     PciAddress&  PciAddress::operator = ( const std::string& address )
 *  @param  address  String address
 *  @return reference to this object
 *
 *  Assign specified PCI string \a address to this PCI address.
 */

PciAddress&  PciAddress::operator = ( const std::string& address )
{
    m_address = 0;
    m_valid   = false;

    stringstream  strm( address );

    uint32_t  domain = 0;   // Didn't want to expose as union
    uint32_t  bus    = 0;
    uint32_t  device = 0;
    uint32_t  funct  = 0;
    char      sep    = 0;

    for (;;)    // fake loop
    {
        strm >> hex >> domain;
        if (strm.fail())
            break;

        strm >> sep >> hex >> bus;
        if (strm.fail())
            break;

        strm >> sep >> hex >> device;
        if (strm.fail())
            break;

        strm >> sep >> funct;
        break;  // out of here
    }

    if (!strm.fail())
    {
        m_address |= (domain << 16) & 0xffff0000;
        m_address |= (bus    <<  8) & 0x0000ff00;
        m_address |= (device <<  3) & 0x000000f8;
        m_address |= (funct  <<  0) & 0x00000007;
        m_valid = true;
    }

    return  *this;
}


//----------------------------------------------------------------------------
/** @fn     PciAddress&  PciAddress::operator = ( const PciAddress& that )
 *  @param  that    Other PCI address object
 *  @return reference to this object
 *
 *  Assign \a that object to this object.
 */

PciAddress&  PciAddress::operator = ( const PciAddress& that )
{
    if (&that != this)
    {
        m_address = that.m_address;
        m_valid   = that.m_valid;
    }

    return  *this;
}


//----------------------------------------------------------------------------
/** @fn     bool  PciAddress::isValid() const
 *  @return valid state
 *
 *  Returns \c true if the PCI address is valid.
 */

bool  PciAddress::isValid() const
{
    return  m_valid;
}


//----------------------------------------------------------------------------
/** @fn     int  PciAddress::domain() const
 *  @return domain component
 *
 *  Returns the PCI address domain component or -1 in case the PCI address is
 *  invalid.
 */

int  PciAddress::domain() const
{
    if (!isValid())
        return  -1;

    return  ((m_address >> 16) & 0xffff);
}


//----------------------------------------------------------------------------
/** @fn     int  PciAddress::bus() const
 *  @return bus component
 *
 *  Returns the PCI address bus component or -1 in case the PCI address is
 *  invalid.
 */

int  PciAddress::bus() const
{
    if (!isValid())
        return  -1;

    return  ((m_address >> 8) & 0xff);
}


//----------------------------------------------------------------------------
/** @fn     int  PciAddress::device() const
 *  @return device component
 *
 *  Returns the PCI address device component or -1 in case the PCI address is
 *  invalid.
 */

int  PciAddress::device() const
{
    if (!isValid())
        return  -1;

    return  ((m_address >> 3) & 0x1f);
}


//----------------------------------------------------------------------------
/** @fn     int  PciAddress::function() const
 *  @return function component
 *
 *  Returns the PCI address function component or -1 in case the PCI address
 *  is invalid.
 */

int  PciAddress::function() const
{
    if (!isValid())
        return  -1;

    return  ((m_address >> 0) & 0x7);       // Silly but clear, right?
}


//----------------------------------------------------------------------------
/** @fn     std::string  PciAddress::asString() const
 *  @return PCIe address string
 *
 *  Returns the PCIe address string as "0000:00:00.0", where the individual
 *  components are from left to right:
 *  - Domain   [0..ffff]
 *  - Bus      [0..ff]
 *  - Device   [0..1f]
 *  - Function [0..7]
 *
 *  If the PCI address is invalid, "????:??:??.?" is returned.
 */

std::string  PciAddress::asString() const
{
    if (!isValid())
#ifdef __linux__
        return  "????:??:??.?";
#else
        return  "????:?:?:?";
#endif

    stringstream  strm;

    // Thanks to the people that doesn't adhere to the standards and force us
    // do checks like this one ...
#ifdef __linux__
    strm << std::hex;
#endif

    strm.fill( '0' );
    strm.width( 4 );
    strm << domain() << ':';

#ifdef __linux__
    strm.fill( '0' );
    strm.width( 2 );
    strm << bus() << ':';

    strm.fill( '0' );
    strm.width( 2 );
    strm << device() << '.';

    strm.width( 1 );
    strm << function();
#else
    strm << bus() << ':' << device() << ':' << function();
#endif

    return  strm.str();
}


//----------------------------------------------------------------------------
/** @fn     void  PciAddress::clear()
 *
 *  Clear PCI address. This makes the PCI address invalid.
 */

void  PciAddress::clear()
{
    m_address = 0;
    m_valid   = false;
}

