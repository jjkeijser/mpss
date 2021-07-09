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
#include    "MicVoltageInfo.hpp"
#include    "MicVoltage.hpp"
//
#include    "VoltageInfoData_p.hpp"     // Private

// NAMESPACE
//
using namespace  micmgmt;


//============================================================================
/** @class    micmgmt::MicVoltageInfo   MicVoltageInfo.hpp
 *  @ingroup  sdk
 *  @brief    This class encapsulates voltage information
 *
 *  The \b %MicVoltageInfo class encapsulates voltage information of a MIC
 *  device and provides accessors to the relevant information.
 *
 *  Please \b note that all accessors only return valid data if isValid()
 *  returns \c true.
 */


//============================================================================
//  P U B L I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     MicVoltageInfo::MicVoltageInfo()
 *
 *  Construct an empty voltage info object.
 */

MicVoltageInfo::MicVoltageInfo() :
    m_pData( new VoltageInfoData )
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicVoltageInfo::MicVoltageInfo( const VoltageInfoData& data )
 *  @param  data    Mic voltage info data
 *
 *  Construct a mic voltage info object based on specified info \a data.
 */

MicVoltageInfo::MicVoltageInfo( const VoltageInfoData& data ) :
    m_pData( new VoltageInfoData )
{
    m_pData->set( data );
}


//----------------------------------------------------------------------------
/** @fn     MicVoltageInfo::MicVoltageInfo( const MicVoltageInfo& that )
 *  @param  that    Other mic voltage info object
 *
 *  Construct a mic voltage object as deep copy of \a that object.
 */

MicVoltageInfo::MicVoltageInfo( const MicVoltageInfo& that ) :
    m_pData( new VoltageInfoData )
{
    m_pData->set( *(that.m_pData) );
}


//----------------------------------------------------------------------------
/** @fn     MicVoltageInfo::~MicVoltageInfo()
 *
 *  Cleanup.
 */

MicVoltageInfo::~MicVoltageInfo()
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicVoltageInfo&  MicVoltageInfo::operator = ( const MicVoltageInfo& that )
 *  @param  that    Other mic voltage object
 *  @return reference to this object
 *
 *  Assign \a that object to this object and return the updated object.
 */

MicVoltageInfo&  MicVoltageInfo::operator = ( const MicVoltageInfo& that )
{
    if (&that != this)
        m_pData->set( *(that.m_pData) );

    return  *this;
}


//----------------------------------------------------------------------------
/** @fn     bool  MicVoltageInfo::isValid() const
 *  @return valid state
 *
 *  Returns \c true if the voltage info is valid.
 */

bool  MicVoltageInfo::isValid() const
{
    return  (m_pData ? m_pData->mValid : false);
}


//----------------------------------------------------------------------------
/** @fn     size_t  MicVoltageInfo::sensorCount() const
 *  @return the number of sensors
 *
 *  Returns the number of voltage sensors in this info object.
 */

size_t  MicVoltageInfo::sensorCount() const
{
    return  (isValid() ? m_pData->mSensors.size() : 0);
}


//----------------------------------------------------------------------------
/** @fn     size_t  MicVoltageInfo::maximumSensorNameWidth() const
 *  @return maximum width
 *
 *  Returns the width of the longest sensor name in our sensor list.
 *
 *  This is a convenience routine that allows horizontal alignment of values.
 */

size_t  MicVoltageInfo::maximumSensorNameWidth() const
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
/** @fn     MicVoltage  MicVoltageInfo::maximumSensorValue() const
 *  @return maximum sensor value
 *
 *  Returns the highest sensor value in our sensor list.
 */

MicVoltage  MicVoltageInfo::maximumSensorValue() const
{
    if (!isValid() || (sensorCount() == 0))
        return  MicVoltage();

    MicVoltage  max = m_pData->mSensors.at( 0 );
    for (size_t i=1; i<m_pData->mSensors.size(); i++)
    {
        if (m_pData->mSensors.at( i ).compare( max ) > 0)
            max = m_pData->mSensors.at( i );
    }

    return  max;
}


//----------------------------------------------------------------------------
/** @fn     MicVoltage  MicVoltageInfo::sensorValueAt( size_t index ) const
 *  @param  index   Sensor index
 *  @return power value
 *
 *  Lookup and return voltage sensor data for the sensor with given \a index.
 */

MicVoltage  MicVoltageInfo::sensorValueAt( size_t index ) const
{
    if (isValid() && (index < sensorCount()))
        return  m_pData->mSensors.at( index );

    return  MicVoltage();
}

