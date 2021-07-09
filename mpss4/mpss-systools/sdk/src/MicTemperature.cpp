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
#include    "MicTemperature.hpp"

// SYSTEM INCLUDES
//
#include    <sstream>

// LOCAL CONSTANTS
//
namespace  {
const uint32_t  DATA_MASK = 0x0000ffff;     // Bits 0..15, signed
const uint32_t  STATUS_MASK = 0xC0000000;   // Two msb
}

// NAMESPACES
//
using namespace  micmgmt;
using namespace  std;


//============================================================================
/** @class    micmgmt::MicTemperature      MicTemperature.hpp
 *  @ingroup  sdk
 *  @brief    The class represents a temperature sample.
 *
 *  The \b %MicTemperature class encapsulates a temperature sample.
 */


//============================================================================
//  P U B L I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     MicTemperature::MicTemperature()
 *
 *  Construct an empty and invalid temperature sample object.
 */

MicTemperature::MicTemperature()
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicTemperature::MicTemperature( uint32_t sample, int exponent )
 *  @param  sample  Temperature sample
 *  @param  exponent  Unit exponent (optional)
 *
 *  Construct a temperature sample object based on given \a sample value and
 *  \a exponent.
 */

MicTemperature::MicTemperature( uint32_t sample, int exponent ) :
    MicSampleBase( sample, exponent )
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicTemperature::MicTemperature( const std::string& name, uint32_t sample, int exponent )
 *  @param  name    Sample name
 *  @param  sample  Temperature sample
 *  @param  exponent  Unit exponent (optional)
 *
 *  Construct a temperature sample object with given \a name and specified
 *  \a sample value.
 */

MicTemperature::MicTemperature( const std::string& name, uint32_t sample, int exponent ) :
    MicSampleBase( name, sample, exponent )
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicTemperature::MicTemperature( const MicTemperature& that )
 *  @param  that    Other temperature sample object
 *
 *  Construct a temperature sample object as deep copy of \a that object.
 */

MicTemperature::MicTemperature( const MicTemperature& that ) :
    MicSampleBase( that )
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicTemperature::~MicTemperature()
 *
 *  Cleanup.
 */

MicTemperature::~MicTemperature()
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicTemperature&  MicTemperature::operator = ( uint32_t sample )
 *  @param  sample  Data sample
 *  @return reference to this object
 *
 *  Assign specified data \a sample to this object and return a reference to
 *  this updated object.
 */

MicTemperature&  MicTemperature::operator = ( uint32_t sample )
{
    MicSampleBase::operator = ( sample );

    return  *this;
}


//----------------------------------------------------------------------------
/** @fn     MicTemperature&  MicTemperature::operator = ( const MicTemperature& that )
 *  @param  that    Other temperature object
 *  @return reference to this object
 *
 *  Assign \a that temperature to this object and return a reference to this
 *  update object.
 */

MicTemperature&  MicTemperature::operator = ( const MicTemperature& that )
{
    if (&that != this)
        MicSampleBase::operator = ( that );

    return  *this;
}


//----------------------------------------------------------------------------
/** @fn     int  MicTemperature::celsius() const
 *  @return temperature
 *
 *  Returns the temperature data sample in degrees Celsius.
 */

int  MicTemperature::celsius() const
{
    return  value();
}


//----------------------------------------------------------------------------
/** @fn     int  MicTemperature::fahrenheit() const
 *  @return temperature
 *
 *  Returns the temperature data sample in degrees Fahrenheit.
 */

int  MicTemperature::fahrenheit() const
{
    return  (isValid() ? static_cast<int>( (value() * 9.0) / 5 ) + 32 : 0);
}


//============================================================================
//  P R I V A T E   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     uint32_t  MicTemperature::impDataMask() const
 *  @return data mask
 *
 *  Returns the specialized data mask, representing all 16 temperature bits.
 */

uint32_t  MicTemperature::impDataMask() const
{
    return  DATA_MASK;
}

//----------------------------------------------------------------------------
/** @fn     uint32_t  MicTemperature::impStatusMask() const
 *  @return data mask
 *
 *  Returns the specialized status mask, representing the two MSB.
 */

uint32_t  MicTemperature::impStatusMask() const
{
    return  STATUS_MASK;
}


//----------------------------------------------------------------------------
/** @fn     int  MicTemperature::impValue() const
 *  @return temperature value
 *
 *  Returns the temperature data sample.
 */

int  MicTemperature::impValue() const
{
    return static_cast<int>( rawSample() & dataMask() );
}


//----------------------------------------------------------------------------
/** @fn     std::string  MicTemperature::impUnit() const
 *  @return unit string
 *
 *  Returns the temperature unit string depending on exponent.
 *  Well, this is not really true, because it is not common to do that.
 */

std::string  MicTemperature::impUnit() const
{
    if (exponent() != 0)
        return  "?";
    else
        return  "";
}

