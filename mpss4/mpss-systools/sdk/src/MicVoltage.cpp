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
#include    "MicVoltage.hpp"

// NAMESPACES
//
using namespace  micmgmt;
using namespace  std;


//============================================================================
/** @class    micmgmt::MicVoltage      MicVoltage.hpp
 *  @ingroup  sdk
 *  @brief    The class represents a voltage sample.
 *
 *  The \b %MicVoltage class encapsulates a voltage sample.
 */


//============================================================================
//  P U B L I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     MicVoltage::MicVoltage()
 *
 *  Construct an empty and invalid voltage sample object.
 */

MicVoltage::MicVoltage()
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicVoltage::MicVoltage( uint32_t sample, int exponent )
 *  @param  sample    Voltage sample
 *  @param  exponent  Unit exponent (optional)
 *
 *  Construct a voltage sample object based on given \a sample value and
 *  \a exponent.
 */

MicVoltage::MicVoltage( uint32_t sample, int exponent ) :
    MicSampleBase( sample, exponent )
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicVoltage::MicVoltage( const std::string& name, uint32_t sample, int exponent )
 *  @param  name      Sample name
 *  @param  sample    Voltage sample
 *  @param  exponent  Unit exponent (optional)
 *
 *  Construct a voltage sample object with given \a name and specified
 *  \a sample value.
 */

MicVoltage::MicVoltage( const std::string& name, uint32_t sample, int exponent ) :
    MicSampleBase( name, sample, exponent )
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicVoltage::MicVoltage( const MicVoltage& that )
 *  @param  that    Other voltage sample object
 *
 *  Construct a voltage sample object as deep copy of \a that object.
 */

MicVoltage::MicVoltage( const MicVoltage& that ) :
    MicSampleBase( that )
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicVoltage::~MicVoltage()
 *
 *  Cleanup.
 */

MicVoltage::~MicVoltage()
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicVoltage&  MicVoltage::operator = ( uint32_t sample )
 *  @param  sample  Data sample
 *  @return reference to this object
 *
 *  Assign specified \a sample to this object and return a reference to this
 *  updated object.
 */

MicVoltage&  MicVoltage::operator = ( uint32_t sample )
{
    MicSampleBase::operator = ( sample );

    return  *this;
}


//----------------------------------------------------------------------------
/** @fn     MicVoltage&  MicVoltage::operator = ( const MicVoltage& that )
 *  @param  that    Other voltage sample object
 *  @return reference to this object
 *
 *  Assign \a that object to this object and return a reference to this
 *  updated object.
 */

MicVoltage&  MicVoltage::operator = ( const MicVoltage& that )
{
    if (&that != this)
        MicSampleBase::operator = ( that );

    return  *this;
}


//============================================================================
//  P R I V A T E   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     int  MicVoltage::impValue() const
 *  @return voltage
 *
 *  Returns the voltage data sample.
 */

int  MicVoltage::impValue() const
{
    return  static_cast<int>( rawSample() & dataMask() );
}


//----------------------------------------------------------------------------
/** @fn     std::string  MicVoltage::impUnit() const
 *  @return unit string
 *
 *  Returns the voltage unit string depending on exponent.
 */

std::string  MicVoltage::impUnit() const
{
    switch (exponent())
    {
      case  -6:
        return  "uV";

      case  -3:
        return  "mV";

      case  0:
        return  "V";

      case  3:
        return  "kV";

      default:
        break;
    }

    return  "?";
}

