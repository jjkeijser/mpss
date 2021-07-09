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
#include    "MicPowerUsageInfo.hpp"
#include    "MicPower.hpp"
#include    "ThrottleInfo.hpp"
//
#include    "PowerUsageData_p.hpp"   // Private

// NAMESPACE
//
using namespace  micmgmt;


//============================================================================
/** @class    micmgmt::MicPowerUsageInfo   MicPowerUsageInfo.hpp
 *  @ingroup  sdk
 *  @brief    This class encapsulates power usage information
 *
 *  The \b %MicPowerUsageInfo class encapsulates power usage information of
 *  a MIC device and provides accessors to the relevant information.
 *
 *  Please \b note that all accessors only return valid data if isValid()
 *  returns \c true.
 */


//============================================================================
//  P U B L I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     MicPowerUsageInfo::MicPowerUsageInfo()
 *
 *  Construct an empty power usage info object.
 */

MicPowerUsageInfo::MicPowerUsageInfo() :
    m_pData( new PowerUsageData )
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicPowerUsageInfo::MicPowerUsageInfo( const PowerUsageData& data )
 *  @param  data    Mic power usage info data
 *
 *  Construct a mic power usage info object based on specified info \a data.
 */

MicPowerUsageInfo::MicPowerUsageInfo( const PowerUsageData& data ) :
    m_pData( new PowerUsageData )
{
    m_pData->set( data );
}


//----------------------------------------------------------------------------
/** @fn     MicPowerUsageInfo::MicPowerUsageInfo( const MicPowerUsageInfo& that )
 *  @param  that    Other mic power info object
 *
 *  Construct a mic power usage info object as deep copy of \a that object.
 */

MicPowerUsageInfo::MicPowerUsageInfo( const MicPowerUsageInfo& that ) :
    m_pData( new PowerUsageData )
{
    m_pData->set( *(that.m_pData) );
}


//----------------------------------------------------------------------------
/** @fn     MicPowerUsageInfo::~MicPowerUsageInfo()
 *
 *  Cleanup.
 */

MicPowerUsageInfo::~MicPowerUsageInfo()
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicPowerUsageInfo&  MicPowerUsageInfo::operator = ( const MicPowerUsageInfo& that )
 *  @param  that    Other mic power usage info object
 *  @return reference to this object
 *
 *  Assign \a that object to this object and return the updated object.
 */

MicPowerUsageInfo&  MicPowerUsageInfo::operator = ( const MicPowerUsageInfo& that )
{
    if (&that != this)
        m_pData->set( *(that.m_pData) );

    return  *this;
}


//----------------------------------------------------------------------------
/** @fn     bool  MicPowerUsageInfo::isValid() const
 *  @return valid state
 *
 *  Returns \c true if the power usage info is valid.
 */

bool  MicPowerUsageInfo::isValid() const
{
    return  (m_pData ? m_pData->mValid : false);
}


//----------------------------------------------------------------------------
/** @fn     size_t  MicPowerUsageInfo::sensorCount() const
 *  @return the number of sensors
 *
 *  Returns the number of power sensors in this info object.
 */

size_t  MicPowerUsageInfo::sensorCount() const
{
    return  (isValid() ? m_pData->mSensors.size() : 0);
}


//----------------------------------------------------------------------------
/** @fn     size_t  MicPowerUsageInfo::maximumSensorNameWidth() const
 *  @return maximum width
 *
 *  Returns the width of the longest sensor name in our sensor list.
 *
 *  This is a convenience routine that allows horizontal alignment of values.
 */

size_t  MicPowerUsageInfo::maximumSensorNameWidth() const
{
    if (!isValid() || (sensorCount() == 0))
        return  0;

    size_t  max = 0;
    for (size_t i=0; i<m_pData->mSensors.size(); i++)
    {
        if (m_pData->mSensors.at( i ).nameLength() > max)
            max = m_pData->mSensors.at( i ).nameLength();
    }

    return  max;
}


//----------------------------------------------------------------------------
/** @fn     MicPower  MicPowerUsageInfo::maximumSensorValue() const
 *  @return maximum sensor value
 *
 *  Returns the highest sensor value in our sensor list.
 */

MicPower  MicPowerUsageInfo::maximumSensorValue() const
{
    if (!isValid() || (sensorCount() == 0))
        return  MicPower();

    MicPower  max = m_pData->mSensors.at( 0 );
    for (size_t i=1; i<m_pData->mSensors.size(); i++)
    {
        if (m_pData->mSensors.at( i ).compare( max ) > 0)
            max = m_pData->mSensors.at( i );
    }

    return  max;
}


//----------------------------------------------------------------------------
/** @fn     MicPower  MicPowerUsageInfo::sensorValueAt( size_t index ) const
 *  @param  index   Sensor index
 *  @return power value
 *
 *  Lookup and return power sensor data for the sensor with given \a index.
 */

MicPower  MicPowerUsageInfo::sensorValueAt( size_t index ) const
{
    if (isValid() && (index < sensorCount()))
        return  m_pData->mSensors.at( index );

    return  MicPower();
}


//----------------------------------------------------------------------------
/** @fn     MicPower  MicPowerUsageInfo::throttleThreshold() const
 *  @return throttle threshold
 *
 *  Returns the power throttle threshold setting.
 */

MicPower  MicPowerUsageInfo::throttleThreshold() const
{
    return  (isValid() ? m_pData->mThrottle : MicPower());
}


//----------------------------------------------------------------------------
/** @fn     ThrottleInfo  MicPowerUsageInfo::throttleInfo() const
 *  @return throttle information
 *
 *  Returns power throttle information.
 */

ThrottleInfo  MicPowerUsageInfo::throttleInfo() const
{
    return  (isValid() ? m_pData->mThrottleInfo : ThrottleInfo());
}

//----------------------------------------------------------------------------
/** @fn     std::vector<MicPower>  MicPowerUsageInfo::sensors() const
 *  @return vector of sensors
 *
 *  Returns all sensor information in a std::vector<MicPower>.
 */

std::vector<MicPower> MicPowerUsageInfo::sensors() const
{
    return m_pData->mSensors;
}
