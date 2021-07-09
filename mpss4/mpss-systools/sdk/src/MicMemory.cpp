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
#include    "MicMemory.hpp"

// LOCAL CONSTANTS
//
namespace  {
const uint32_t  STATUS_MASK        = 0x0;
const uint32_t  BYTES_PER_KILOBYTE = 1024;
const uint32_t  BYTES_PER_MEGABYTE = 1024 * BYTES_PER_KILOBYTE;
const uint32_t  BYTES_PER_GIGABYTE = 1024 * BYTES_PER_MEGABYTE;
}

// NAMESPACES
//
using namespace  micmgmt;
using namespace  std;


//============================================================================
/** @class    micmgmt::MicMemory      MicMemory.hpp
 *  @ingroup  sdk
 *  @brief    The class represents a memory size.
 *
 *  The \b %MicMemory class encapsulates a memory size.
 */


//============================================================================
//  P U B L I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     MicMemory::MicMemory()
 *
 *  Construct an empty and invalid memory size sample object.
 */

MicMemory::MicMemory()
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicMemory::MicMemory( uint32_t sample, int exponent )
 *  @param  sample    Memory size sample
 *  @param  exponent  Unit exponent (optional)
 *
 *  Construct a memory size sample object based on given \a sample value and
 *  \a exponent.
 */

MicMemory::MicMemory( uint32_t sample, int exponent ) :
    MicSampleBase( sample, exponent )
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicMemory::MicMemory( const std::string& name, uint32_t sample, int exponent )
 *  @param  name      Sample name
 *  @param  sample    emory size sample
 *  @param  exponent  Unit exponent (optional)
 *
 *  Construct a memory size sample object with given \a name and specified
 *  \a sample value.
 */

MicMemory::MicMemory( const std::string& name, uint32_t sample, int exponent ) :
    MicSampleBase( name, sample, exponent )
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicMemory::MicMemory( const MicMemory& that )
 *  @param  that    Other memory size sample object
 *
 *  Construct a memory size sample object as deep copy of \a that object.
 */

MicMemory::MicMemory( const MicMemory& that ) :
    MicSampleBase( that )
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicMemory::~MicMemory()
 *
 *  Cleanup.
 */

MicMemory::~MicMemory()
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicMemory&  MicMemory::operator = ( uint32_t sample )
 *  @param  sample  Data sample
 *  @return reference to this object
 *
 *  Assign specified \a sample to this object and return a reference to this
 *  updated object.
 */

MicMemory&  MicMemory::operator = ( uint32_t sample )
{
    MicSampleBase::operator = ( sample );

    return  *this;
}


//----------------------------------------------------------------------------
/** @fn     MicMemory&  MicMemory::operator = ( const MicMemory& that )
 *  @param  that    Other memory size sample object
 *  @return reference to this object
 *
 *  Assign \a that object to this object and return a reference to this
 *  updated object.
 */

MicMemory&  MicMemory::operator = ( const MicMemory& that )
{
    if (&that != this)
        MicSampleBase::operator = ( that );

    return  *this;
}


//============================================================================
//  P R I V A T E   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     double  MicMemory::scaled( double value, int exponent ) const
 *  @param  value     Value
 *  @param  exponent  Desired exponent
 *  @return scaled value
 *
 *  Returns the scaled value according specified \a exponent.
 *
 *  This implementation returns calculated values for conversions to Bytes,
 *  kB, MB, and GB. The exponent is 0, 3, 6, and 9 respectively. Any other
 *  exponent is ignore and \a value is returned without conversion.
 */

double  MicMemory::scaled( double value, int exponent ) const
{
    int     exunit = (exponent < 0) ? -exponent : exponent;
    bool    divide = (exunit != exponent);

    switch (exunit)
    {
      case  eKilo:
        return  (divide ? value / BYTES_PER_KILOBYTE : value * BYTES_PER_KILOBYTE);

      case  eMega:
        return  (divide ? value / BYTES_PER_MEGABYTE : value * BYTES_PER_MEGABYTE);

      case  eGiga:
        return  (divide ? value / BYTES_PER_GIGABYTE : value * BYTES_PER_GIGABYTE);

      default:
      case  eBase:
        return  value;
    }
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicMemory::impDataMask() const
 *  @return data mask
 *
 *  Returns the specialized data mask.
 */

uint32_t  MicMemory::impDataMask() const
{
    return  ~STATUS_MASK;
}


//----------------------------------------------------------------------------
/** @fn     int  MicMemory::impValue() const
 *  @return memory size
 *
 *  Returns the memory size data sample.
 */

int  MicMemory::impValue() const
{
    return  static_cast<int>( rawSample() & dataMask() );
}


//----------------------------------------------------------------------------
/** @fn     std::string  MicMemory::impUnit() const
 *  @return unit string
 *
 *  Returns the memory size unit string depending on exponent.
 */

std::string  MicMemory::impUnit() const
{
    switch (exponent())
    {
      case  0:
        return  "Byte";

      case  3:
        return  "kB";

      case  6:
        return  "MB";

      case  9:
        return  "GB";

      default:
        break;
    }

    return  "?";
}

