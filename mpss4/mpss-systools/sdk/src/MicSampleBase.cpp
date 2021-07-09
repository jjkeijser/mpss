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
#include    "MicLogger.hpp"
#include    "MicSampleBase.hpp"

// LOCAL CONSTANTS
//
namespace  {
const uint32_t  STATUS_MASK     = 0xc0000000;
const uint32_t  UNAVAILABLE     = 0xc0000000;
const uint32_t  VALID_DATA      = 0x00000000;
const uint32_t  LOWER_THRESHOLD = 0x40000000;
const uint32_t  UPPER_THRESHOLD = 0x80000000;
}   // unnamed namespace

// NAMESPACES
//
using namespace  micmgmt;
using namespace  std;


//============================================================================
/** @class    micmgmt::MicSampleBase      MicSampleBase.hpp
 *  @ingroup  sdk
 *  @brief    The class represents a data sample.
 *
 *  The \b %MicSampleBase abstract base class encapsulates a generic data
 *  sample.
 *
 *  The %MicSampleBase class stores a data sample and some general status info
 *  about the sample. A data sample can be unavailable due the nature of the
 *  underlaying hardware acquisition device. In such case, the data contained
 *  in the sample is not trustworthy. The isAvailable() function should be
 *  used to obtain this information about the sample.
 *
 *  The generic data sample is used to hold data from any common MIC sensor.
 *  The can be a voltage or a temperature for example. In case of temperature
 *  it is likely that the base unit is used. The base unit in this case would
 *  be degrees. A voltage sensor may report data in microvolt. To be able for
 *  the user of the data sample to know what the actual unit is, the
 *  %MicSampleBase class provides an exponent() function. This function returns
 *  the exponent that can be used to scale the data or simply to know what
 *  the unit of the sample is in relation to the base unit. See exponent()
 *  for more information on that.
 *
 *  The %MicSampleBase class defines a series of standard exponents for
 *  convenience, but the exponent parameter used in the API is not limited
 *  to the defined exponent factors. Any integer value can be used as legal
 *  exponent.
 *
 *  The acquisition device has knowledge about two upper thresholds, the lower
 *  threshold and the upper threshold. If the data sample exceeds these
 *  thresholds can be queried using isLowerThresholdViolation() and the
 *  isUpperThresholdViolation() functions.
 */


//============================================================================
//  P U B L I C   C O N S T A N T S
//============================================================================

//----------------------------------------------------------------------------
/** @enum   micmgmt::MicSampleBase::Exponent
 *
 *  Enumerations of exponents. These exponent definitions are provided for
 *  convenience to quickly specify a standard exponent value.
 */

/** @var    MicSampleBase::Exponent  MicSampleBase::eGiga
 *
 *  Giga unit exponent (10<sub>9</sub>).
 */

/** @var    MicSampleBase::Exponent  MicSampleBase::eMega
 *
 *  Mega unit exponent (10<sub>6</sub>).
 */

/** @var    MicSampleBase::Exponent  MicSampleBase::eKilo
 *
 *  Kilo unit exponent (10<sub>3</sub>).
 */

/** @var    MicSampleBase::Exponent  MicSampleBase::eBase
 *
 *  Base unit exponent.
 */

/** @var    MicSampleBase::Exponent  MicSampleBase::eMilli
 *
 *  Milli unit exponent (10<sub>-3</sub>).
 */

/** @var    MicSampleBase::Exponent  MicSampleBase::eMicro
 *
 *  Micro unit exponent (10<sub>-6</sub>).
 */

/** @var    MicSampleBase::Exponent  MicSampleBase::eNano
 *
 *  Nano unit exponent (10<sub>-9</sub>).
 */


//============================================================================
//  P U B L I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     MicSampleBase::~MicSampleBase()
 *
 *  Cleanup.
 */

MicSampleBase::~MicSampleBase()
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicSampleBase&  MicSampleBase::operator = ( uint32_t sample )
 *  @param  sample  Data sample
 *  @return updated sample
 *
 *  Assign the given \a sample to this object and return a reference to this
 *  updated object.
 */

MicSampleBase&  MicSampleBase::operator = ( uint32_t sample )
{
    m_sample = sample;
    m_valid  = true;

    return  *this;
}


//----------------------------------------------------------------------------
/** @fn     MicSampleBase&  MicSampleBase::operator = ( const MicSampleBase& that )
 *  @param  that    Other sample object
 *  @return updated sample
 *
 *  Assign \a that object to this object and return the updated object.
 */

MicSampleBase&  MicSampleBase::operator = ( const MicSampleBase& that )
{
    if (&that != this)
    {
        m_name      = that.m_name;
        m_sample    = that.m_sample;
        m_exponent  = that.m_exponent;
        m_valid     = that.m_valid;
    }

    return  *this;
}


//----------------------------------------------------------------------------
/** @fn     bool  MicSampleBase::isValid() const
 *  @return valid state
 *
 *  Returns \c true if the data sample is considered to be valid.
 */

bool  MicSampleBase::isValid() const
{
    return  m_valid;
}


//----------------------------------------------------------------------------
/** @fn     bool  MicSampleBase::isAvailable() const
 *  @return availability state
 *
 *  Returns \c true if the data sample is available.
 */

bool  MicSampleBase::isAvailable() const
{
    return  ((m_sample & STATUS_MASK) != UNAVAILABLE);
}


//----------------------------------------------------------------------------
/** @fn     bool  MicSampleBase::isLowerThresholdViolation() const
 *  @return a lower threshold violation state
 *
 *  Returns \c true if the sensor considers the current sample data as a
 *  violation of the lower threshold.
 */

bool  MicSampleBase::isLowerThresholdViolation() const
{
    return  ((m_sample & STATUS_MASK & LOWER_THRESHOLD) == LOWER_THRESHOLD);
}


//----------------------------------------------------------------------------
/** @fn     bool  MicSampleBase::isUpperThresholdViolation() const
 *  @return a upper threshold violation state
 *
 *  Returns \c true if the sensor considers the current sample data as a
 *  violation of the upper threshold.
 */

bool  MicSampleBase::isUpperThresholdViolation() const
{
    return  ((m_sample & STATUS_MASK & UPPER_THRESHOLD) == UPPER_THRESHOLD);
}


//----------------------------------------------------------------------------
/** @fn     int  MicSampleBase::compare( const MicSampleBase& that ) const
 *  @param  that    Other data sample
 *  @return comparison result
 *
 *  Returns -1 if this data sample is lower than \a that data sample, or +1
 *  if this data sample is greater than \a that data sample. If both data
 *  samples are equal, 0 is returned.
 */

int  MicSampleBase::compare( const MicSampleBase& that ) const
{
    double  data1 = normalizedValue();
    double  data2 = that.normalizedValue();

    if (data1 < data2)
        return  -1;
    else if (data1 > data2)
        return  1;
    else
        return  0;
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicSampleBase::rawSample() const
 *  @return raw data sample
 *
 *  Returns the raw unmasked data sample.
 */

uint32_t  MicSampleBase::rawSample() const
{
    return  m_sample;
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicSampleBase::dataMask() const
 *  @return data mask
 *
 *  Returns the data mask, which mask out all non-data bits.
 *
 *  This function calls impDataMask() and adds the status bits before
 *  returning the composite mask.
 *
 *  Perform a bitwise AND with the raw data sample to retrieve the pure data
 *  from a raw sample.
 */

uint32_t  MicSampleBase::dataMask() const
{
    return  (impDataMask() & ~STATUS_MASK);
}


//----------------------------------------------------------------------------
/** @fn     int  MicSampleBase::exponent() const
 *  @return exponent
 *
 *  Returns the exponent, that can be used to resolve the sample's unit,
 *  based on the sample's base unit.
 *
 *  For example, the base unit of a voltage is Volt. An exponent of 3
 *  would mean that the sample value of 1 means 1000 Volt or 1 kVolt.
 *  An exponent of -3 would mean that the sample is in millivolt.
 */

int  MicSampleBase::exponent() const
{
    return  m_exponent;
}


//----------------------------------------------------------------------------
/** @fn     std::string  MicSampleBase::name() const
 *  @return sample name
 *
 *  Returns the sample name. (e.g. "Vccp").
 */

std::string  MicSampleBase::name() const
{
    return  m_name;
}


//----------------------------------------------------------------------------
/** @fn     size_t  MicSampleBase::nameLength() const
 *  @return name length
 *
 *  Returns the length of the name. This returns the same as name().length().
 */

size_t  MicSampleBase::nameLength() const
{
    return  m_name.length();
}


//----------------------------------------------------------------------------
/** @fn     std::string  MicSampleBase::unit() const
 *  @return unit string
 *
 *  Returns the unit string based on sample specialization and exponent.
 */

std::string  MicSampleBase::unit() const
{
    return  (isValid() ? impUnit() : "");
}


//----------------------------------------------------------------------------
/** @fn     int  MicSampleBase::value() const
 *  @return sample value
 *
 *  Returns the sample value.
 */

int  MicSampleBase::value() const
{
    return  impValue();
}


//----------------------------------------------------------------------------
/** @fn     double  MicSampleBase::normalizedValue() const
 *  @return normalized value
 *
 *  Returns a normalized value, scaled according the exponent setting.
 *
 *  The data sample is typically stored in the unit defined by the data
 *  source. This is done to achieve maximum resolution. To be able to use
 *  the data correctly, the scaling factor in relation to the base unit is
 *  stored along with the data sample. This is called the \em exponent.
 *
 *  This function takes the internal data value and scales it according the
 *  associated exponent and returns the result as base unit value.
 */

double  MicSampleBase::normalizedValue() const
{
    return  scaled( value(), exponent() );
}


//----------------------------------------------------------------------------
/** @fn     double  MicSampleBase::scaledValue( int exponent ) const
 *  @param  exponent  Desired exponent (optional)
 *  @return scaled value
 *
 *  Returns the scaled value according specified \a exponent.
 */

double  MicSampleBase::scaledValue( int exponent ) const
{
    return  scaled( normalizedValue(), -exponent );
}


//----------------------------------------------------------------------------
/** @fn     void  MicSampleBase::setExponent( int exponent )
 *  @param  exponent  Exponent
 *
 *  Set the unit exponent. See exponent() for more information.
 */

void  MicSampleBase::setExponent( int exponent )
{
    m_exponent = exponent;
}


//----------------------------------------------------------------------------
/** @fn     void  MicSampleBase::setName( const string& name )
 *  @param  name    Sample name
 *
 *  Set the name of the sample. This name is used to identify the data sample.
 */

void  MicSampleBase::setName( const string& name )
{
    m_name = name;
}


//----------------------------------------------------------------------------
/** @fn     void  MicSampleBase::clearValue()
 *
 *  Clear and invalidate the sample value.
 *  Name and exponent are \b not touched.
 */

void  MicSampleBase::clearValue()
{
    m_sample = 0;
    m_valid  = false;
}


//============================================================================
//  P R O T E C T E D   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     MicSampleBase::MicSampleBase()
 *
 *  Construct an empty and invalid data sample object.
 */

MicSampleBase::MicSampleBase() :
    m_sample( 0 ),
    m_exponent( 0 ),
    m_valid( false )
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicSampleBase::MicSampleBase( uint32_t sample, int exponent )
 *  @param  sample    Data sample
 *  @param  exponent  Unit exponent (optional)
 *
 *  Construct a data sample object based on given \a sample and \a exponent.
 */

MicSampleBase::MicSampleBase( uint32_t sample, int exponent ) :
    m_sample( sample ),
    m_exponent( exponent ),
    m_valid( false )
{
    if ( sample & impStatusMask() )
        LOG( WARNING_MSG, "MicSampleBase: invalid sample" );
    else
        m_valid = true;
}


//----------------------------------------------------------------------------
/** @fn     MicSampleBase::MicSampleBase( const std::string& name, uint32_t sample, int exponent )
 *  @param  name      Sample name
 *  @param  sample    Data sample (optional)
 *  @param  exponent  Unit exponent (optional)
 *
 *  Construct a data sample object with given \a name, based on specified
 *  \a sample and \a exponent.
 */

MicSampleBase::MicSampleBase( const std::string& name, uint32_t sample, int exponent ) :
    m_name( name ),
    m_sample( sample ),
    m_exponent( exponent ),
    m_valid( false )
{
    if ( sample & impStatusMask() )
        LOG( WARNING_MSG, "MicSampleBase: invalid sample: %s",
             name.c_str() );
    else
        m_valid = true;
}


//----------------------------------------------------------------------------
/** @fn     MicSampleBase::MicSampleBase( const MicSampleBase& that )
 *  @param  that    Other data sample object
 *
 *  Construct a data sample object as deep copy of \a that object.
 */

MicSampleBase::MicSampleBase( const MicSampleBase& that )
{
    m_name     = that.m_name;
    m_sample   = that.m_sample;
    m_exponent = that.m_exponent;
    m_valid    = that.m_valid;
}


//----------------------------------------------------------------------------
/** @fn     double  MicSampleBase::scaled( double value, int exponent ) const
 *  @param  value     Value
 *  @param  exponent  Desired exponent
 *  @return scaled value
 *
 *  Returns the scaled value according specified \a exponent.
 *
 *  This virtual function may be reimplemented by deriving classes.
 *  The default implementation scales by 10 times the power of \a exponent.
 */

double  MicSampleBase::scaled( double value, int exponent ) const
{
    double  result = value;

    if (exponent < 0)
    {
        while (exponent++)
            result /= 10;
    }
    else if (exponent > 0)
    {
        while (exponent--)
            result *= 10;
    }

    return  result;
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicSampleBase::impDataMask() const
 *  @return data mask
 *
 *  Returns the specialized data mask.
 *
 *  This virtual function may be reimplemented by deriving classes.
 *  The default implementation returns all bits set, except for the status
 *  bits.
 */

uint32_t  MicSampleBase::impDataMask() const
{
    return  ~STATUS_MASK;
}

//----------------------------------------------------------------------------
/** @fn     uint32_t  MicSampleBase::impStatusMask() const
 *  @return data mask
 *
 *  Returns the specialized status mask.
 *
 *  This virtual function may be reimplemented by deriving classes.
 *  The default implementation returns 0.
 */

uint32_t  MicSampleBase::impStatusMask() const
{
    return  0;
}


//----------------------------------------------------------------------------
/** @fn     int  MicSampleBase::impValue() const
 *  @return value
 *
 *  Returns the converted sample value.
 *
 *  This pure virtual function must be implemented by deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     std::string  MicSampleBase::impUnit() const
 *  @return unit string
 *
 *  This pure virtual function must be implemented by deriving classes.
 */

