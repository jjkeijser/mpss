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
#include    "MicSpeed.hpp"

// NAMESPACES
//
using namespace  micmgmt;
using namespace  std;


//============================================================================
/** @class    micmgmt::MicSpeed      MicSpeed.hpp
 *  @ingroup  sdk
 *  @brief    The class represents a speed sample.
 *
 *  The \b %MicSpeed class encapsulates a speed sample and is mainly used
 *  to express memory transaction speed.
 */


//============================================================================
//  P U B L I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     MicSpeed::MicSpeed()
 *
 *  Construct an empty and invalid speed sample object.
 */

MicSpeed::MicSpeed()
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicSpeed::MicSpeed( uint32_t sample, int exponent )
 *  @param  sample    Speed sample
 *  @param  exponent  Unit exponent (optional)
 *
 *  Construct a speed sample object based on given \a sample value and
 *  \a exponent.
 */

MicSpeed::MicSpeed( uint32_t sample, int exponent ) :
    MicSampleBase( sample, exponent )
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicSpeed::MicSpeed( const std::string& name, uint32_t sample, int exponent )
 *  @param  name      Sample name
 *  @param  sample    Speed sample
 *  @param  exponent  Unit exponent (optional)
 *
 *  Construct a speed sample object with given \a name and specified
 *  \a sample value.
 */

MicSpeed::MicSpeed( const std::string& name, uint32_t sample, int exponent ) :
    MicSampleBase( name, sample, exponent )
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicSpeed::MicSpeed( const MicSpeed& that )
 *  @param  that    Other speed sample object
 *
 *  Construct a speed sample object as deep copy of \a that object.
 */

MicSpeed::MicSpeed( const MicSpeed& that ) :
    MicSampleBase( that )
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicSpeed::~MicSpeed()
 *
 *  Cleanup.
 */

MicSpeed::~MicSpeed()
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicSpeed&  MicSpeed::operator = ( uint32_t sample )
 *  @param  sample  Data sample
 *  @return reference to this object
 *
 *  Assign specified data \a sample to this object and return a reference to
 *  this updated object.
 */

MicSpeed&  MicSpeed::operator = ( uint32_t sample )
{
    MicSampleBase::operator = ( sample );

    return  *this;
}


//----------------------------------------------------------------------------
/** @fn     MicSpeed&  MicSpeed::operator = ( const MicSpeed& that )
 *  @param  that    Other speed object
 *  @return reference to this object
 *
 *  Assign \a that speed to this object and return a reference to this updated
 *  object.
 */

MicSpeed&  MicSpeed::operator = ( const MicSpeed& that )
{
    if (&that != this)
        MicSampleBase::operator = ( that );

    return  *this;
}


//============================================================================
//  P R I V A T E   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     int  MicSpeed::impValue() const
 *  @return speed
 *
 *  Returns the speed data sample in transactions per second.
 */

int  MicSpeed::impValue() const
{
    return  static_cast<int>( rawSample() & dataMask() );
}


//----------------------------------------------------------------------------
/** @fn     std::string  MicSpeed::impUnit() const
 *  @return unit string
 *
 *  Returns the speed unit string depending on exponent.
 */

std::string  MicSpeed::impUnit() const
{
    switch (exponent())
    {
      case  0:
        return  "T/s";

      case  3:
        return  "kT/s";

      case  6:
        return  "MT/s";

      case  9:
        return  "GT/s";

      default:
        break;
    }

    return  "?";
}
