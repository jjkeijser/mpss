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
#include    "MicFrequency.hpp"

// LOCAL CONSTANTS
//
namespace  {
const uint32_t  STATUS_MASK = 0x0;
}

// NAMESPACES
//
using namespace  micmgmt;
using namespace  std;


//============================================================================
/** @class    micmgmt::MicFrequency      MicFrequency.hpp
 *  @ingroup  sdk
 *  @brief    The class represents a frequency.
 *
 *  The \b %MicFrequency class encapsulates a frequency.
 */


//============================================================================
//  P U B L I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     MicFrequency::MicFrequency()
 *
 *  Construct an empty and invalid frequency sample object.
 */

MicFrequency::MicFrequency()
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicFrequency::MicFrequency( uint32_t sample, int exponent )
 *  @param  sample    Frequency sample
 *  @param  exponent  Unit exponent (optional)
 *
 *  Construct a frequency sample object based on given \a sample value and
 *  \a exponent.
 */

MicFrequency::MicFrequency( uint32_t sample, int exponent ) :
    MicSampleBase( sample, exponent )
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicFrequency::MicFrequency( const std::string& name, uint32_t sample, int exponent )
 *  @param  name      Sample name
 *  @param  sample    Frequency sample
 *  @param  exponent  Unit exponent (optional)
 *
 *  Construct a frequency sample object with given \a name and specified
 *  \a sample value.
 */

MicFrequency::MicFrequency( const std::string& name, uint32_t sample, int exponent ) :
    MicSampleBase( name, sample, exponent )
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicFrequency::MicFrequency( const MicFrequency& that )
 *  @param  that    Other frequency sample object
 *
 *  Construct a frequency sample object as deep copy of \a that object.
 */

MicFrequency::MicFrequency( const MicFrequency& that ) :
    MicSampleBase( that )
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicFrequency::~MicFrequency()
 *
 *  Cleanup.
 */

MicFrequency::~MicFrequency()
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicFrequency&  MicFrequency::operator = ( uint32_t sample )
 *  @param  sample  Data sample
 *  @return reference to this object
 *
 *  Assign specified \a sample to this object and return a reference to this
 *  updated object.
 */

MicFrequency&  MicFrequency::operator = ( uint32_t sample )
{
    MicSampleBase::operator = ( sample );

    return  *this;
}


//----------------------------------------------------------------------------
/** @fn     MicFrequency&  MicFrequency::operator = ( const MicFrequency& that )
 *  @param  that    Other frequency sample object
 *  @return reference to this object
 *
 *  Assign \a that object to this object and return a reference to this
 *  updated object.
 */

MicFrequency&  MicFrequency::operator = ( const MicFrequency& that )
{
    if (&that != this)
        MicSampleBase::operator = ( that );

    return  *this;
}


//============================================================================
//  P R I V A T E   I N T E R F A C E
//============================================================================



//----------------------------------------------------------------------------
/** @fn     uint32_t  MicFrequency::impDataMask() const
 *  @return data mask
 *
 *  Returns the specialized data mask.
 */

uint32_t  MicFrequency::impDataMask() const
{
    return  ~STATUS_MASK;
}


//----------------------------------------------------------------------------
/** @fn     int  MicFrequency::impValue() const
 *  @return frequency
 *
 *  Returns the frequency data sample.
 */

int  MicFrequency::impValue() const
{
    return  static_cast<int>( rawSample() & dataMask() );
}


//----------------------------------------------------------------------------
/** @fn     std::string  MicFrequency::impUnit() const
 *  @return unit string
 *
 *  Returns the frequency unit string depending on exponent.
 */

std::string  MicFrequency::impUnit() const
{
    switch (exponent())
    {
      case  -6:
        return  "uHz";

      case  -3:
        return  "mHz";

      case  0:
        return  "Hz";

      case  3:
        return  "kHz";

      case  6:
        return  "MHz";

      case  9:
        return  "GHz";

      default:
        break;
    }

    return  "?";
}

