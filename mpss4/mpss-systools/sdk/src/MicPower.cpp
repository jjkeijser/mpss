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
#include    "MicPower.hpp"

// NAMESPACES
//
using namespace  micmgmt;
using namespace  std;


//============================================================================
/** @class    micmgmt::MicPower      MicPower.hpp
 *  @ingroup  sdk
 *  @brief    The class represents a power sample.
 *
 *  The \b %MicPower class encapsulates a power sample.
 */


//============================================================================
//  P U B L I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     MicPower::MicPower()
 *
 *  Construct an empty and invalid power sample object.
 */

MicPower::MicPower()
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicPower::MicPower( uint32_t sample, int exponent )
 *  @param  sample    Power sample
 *  @param  exponent  Unit exponent (optional)
 *
 *  Construct a power sample object based on given \a sample value and
 *  \a exponent.
 */

MicPower::MicPower( uint32_t sample, int exponent ) :
    MicSampleBase( sample, exponent )
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicPower::MicPower( const std::string& name, uint32_t sample, int exponent )
 *  @param  name      Sample name
 *  @param  sample    Power sample
 *  @param  exponent  Unit exponent (optional)
 *
 *  Construct a power sample object with given \a name and specified
 *  \a sample value.
 */

MicPower::MicPower( const std::string& name, uint32_t sample, int exponent ) :
    MicSampleBase( name, sample, exponent )
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicPower::MicPower( const MicPower& that )
 *  @param  that    Other power sample object
 *
 *  Construct a power sample object as deep copy of \a that object.
 */

MicPower::MicPower( const MicPower& that ) :
    MicSampleBase( that )
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicPower::~MicPower()
 *
 *  Cleanup.
 */

MicPower::~MicPower()
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicPower&  MicPower::operator = ( uint32_t sample )
 *  @param  sample  Data sample
 *  @return reference to this object
 *
 *  Assign specified data \a sample to this object and return a reference to
 *  this updated object.
 */

MicPower&  MicPower::operator = ( uint32_t sample )
{
    MicSampleBase::operator = ( sample );

    return  *this;
}


//----------------------------------------------------------------------------
/** @fn     MicPower&  MicPower::operator = ( const MicPower& that )
 *  @param  that    Other power object
 *  @return reference to this object
 *
 *  Assign \a that power to this object and return a reference to this updated
 *  object.
 */

MicPower&  MicPower::operator = ( const MicPower& that )
{
    if (&that != this)
        MicSampleBase::operator = ( that );

    return  *this;
}


//============================================================================
//  P R I V A T E   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     int  MicPower::impValue() const
 *  @return power
 *
 *  Returns the power data sample in Watt.
 */

int  MicPower::impValue() const
{
    return  static_cast<int>( rawSample() & dataMask() );
}


//----------------------------------------------------------------------------
/** @fn     std::string  MicPower::impUnit() const
 *  @return unit string
 *
 *  Returns the power unit string depending on exponent.
 */

std::string  MicPower::impUnit() const
{
    switch (exponent())
    {
      case  -6:
        return  "uW";

      case  -3:
        return  "mW";

      case  0:
        return  "W";

      case  3:
        return  "kW";

      default:
        break;
    }

    return  "?";
}
