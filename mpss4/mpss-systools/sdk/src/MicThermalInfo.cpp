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
#include    "MicThermalInfo.hpp"
#include    "MicTemperature.hpp"
#include    "ThrottleInfo.hpp"
//
#include    "ThermalInfoData_p.hpp"  // Private

// NAMESPACE
//
using namespace  micmgmt;


//============================================================================
/** @class    micmgmt::MicThermalInfo   MicThermalInfo.hpp
 *  @ingroup  sdk
 *  @brief    This class encapsulates thermal information
 *
 *  The \b %MicThermalInfo class encapsulates thermal information of a MIC
 *  device and provides accessors to the relevant information.
 *
 *  Please \b note that all accessors only return valid data if isValid()
 *  returns \c true.
 */


//============================================================================
//  P U B L I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     MicThermalInfo::MicThermalInfo()
 *
 *  Construct an empty thermal info object.
 */

MicThermalInfo::MicThermalInfo() :
    m_pData( new ThermalInfoData )
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicThermalInfo::MicThermalInfo( const ThermalInfoData& data )
 *  @param  data    Mic thermal info data
 *
 *  Construct a mic thermal info object based on specified info \a data.
 */

MicThermalInfo::MicThermalInfo( const ThermalInfoData& data ) :
    m_pData( new ThermalInfoData )
{
    m_pData->set( data );
}


//----------------------------------------------------------------------------
/** @fn     MicThermalInfo::MicThermalInfo( const MicThermalInfo& that )
 *  @param  that    Other mic thermal info object
 *
 *  Construct a mic thermal object as deep copy of \a that object.
 */

MicThermalInfo::MicThermalInfo( const MicThermalInfo& that ) :
    m_pData( new ThermalInfoData )
{
    m_pData->set( *(that.m_pData) );
}


//----------------------------------------------------------------------------
/** @fn     MicThermalInfo::~MicThermalInfo()
 *
 *  Cleanup.
 */

MicThermalInfo::~MicThermalInfo()
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicThermalInfo&  MicThermalInfo::operator = ( const MicThermalInfo& that )
 *  @param  that    Other mic thermal object
 *  @return reference to this object
 *
 *  Assign \a that object to this object and return the updated object.
 */

MicThermalInfo&  MicThermalInfo::operator = ( const MicThermalInfo& that )
{
    if (&that != this)
        m_pData->set( *(that.m_pData) );

    return  *this;
}


//----------------------------------------------------------------------------
/** @fn     bool  MicThermalInfo::isValid() const
 *  @return valid state
 *
 *  Returns \c true if the thermal info is valid.
 */

bool  MicThermalInfo::isValid() const
{
    return m_pData->mCritical.isValid() && m_pData->mControl.isValid() &&
        m_pData->mFanRpm.isValid() && m_pData->mFanPwm.isValid() &&
        m_pData->mFanAdder.isValid();

}


//----------------------------------------------------------------------------
/** @fn     size_t  MicThermalInfo::sensorCount() const
 *  @return the number of sensors
 *
 *  Returns the number of temperatures sensors in this thermal info object.
 */

size_t  MicThermalInfo::sensorCount() const
{
    return m_pData->mSensors.size();
}


//----------------------------------------------------------------------------
/** @fn     size_t  MicThermalInfo::maximumSensorNameWidth() const
 *  @return maximum width
 *
 *  Returns the width of the longest sensor name in our thermal info object.
 *
 *  This is a convenience routine that allows horizontal alignment of
 *  temperature lists.
 */

size_t  MicThermalInfo::maximumSensorNameWidth() const
{
    if (sensorCount() == 0)
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
/** @fn     MicTemperature  MicThermalInfo::maximumSensorValue() const
 *  @return maximum temperature
 *
 *  Returns the highest temperature in our current info object.
 */

MicTemperature  MicThermalInfo::maximumSensorValue() const
{
    if (sensorCount() == 0)
        return  MicTemperature();

    MicTemperature  max = m_pData->mSensors.at( 0 );
    for (size_t i=1; i<m_pData->mSensors.size(); i++)
    {
        if (m_pData->mSensors.at( i ).compare( max ) > 0)
            max = m_pData->mSensors.at( i );
    }

    return  max;
}


//----------------------------------------------------------------------------
/** @fn     MicTemperature  MicThermalInfo::sensorValueAt( size_t index ) const
 *  @param  index   Sensor index
 *  @return temperature
 *
 *  Lookup and return temperature data for the sensor with given \a index.
 */

MicTemperature  MicThermalInfo::sensorValueAt( size_t index ) const
{
    if (index < sensorCount())
        return  m_pData->mSensors.at( index );

    return  MicTemperature();
}


//----------------------------------------------------------------------------
/** @fn     MicTemperature  MicThermalInfo::sensorValueByName( const std::string& name ) const
 *  @param  name Sensor name
 *  @return temperature
 *
 *  Lookup and return temperature data for the sensor with given \a name.
 */

MicTemperature  MicThermalInfo::sensorValueByName( const std::string& name ) const
{
    for (size_t i=0; i < sensorCount(); i++) {
        if (name == m_pData->mSensors.at(i).name())
            return m_pData->mSensors.at( i );
    }

    return  MicTemperature();
}


//----------------------------------------------------------------------------
/** @fn     MicValueUInt32  MicThermalInfo::fanRpm() const
 *  @return fan speed
 *
 *  Returns the cooling fan speed as rotations per minute.
 */

MicValueUInt32 MicThermalInfo::fanRpm() const
{
    return m_pData->mFanRpm;
}


//----------------------------------------------------------------------------
/** @fn     MicValueUInt32  MicThermalInfo::fanPwm() const
 *  @return duty cycle
 *
 *  Returns the cooling fan power duty cycle. Hence, the fan is controlled by
 *  a PWM circuitry.
 */

MicValueUInt32  MicThermalInfo::fanPwm() const
{
    return m_pData->mFanPwm;
}


//----------------------------------------------------------------------------
/** @fn     MicValueUInt32  MicThermalInfo::fanAdder() const
 *  @return fan adder
 *
 *  Returns the cooling fan adder value, which is a percentage added to the
 *  PWM algorithm.
 */

MicValueUInt32  MicThermalInfo::fanAdder() const
{
    return m_pData->mFanAdder;
}


//----------------------------------------------------------------------------
/** @fn     MicTemperature  MicThermalInfo::fanBoostThreshold() const
 *  @return temperature
 *
 *  Returns the temperature threshold at which the fan cooling will be boosted
 *  automatically.
 */

MicTemperature  MicThermalInfo::fanBoostThreshold() const
{
    return m_pData->mControl;
}


//----------------------------------------------------------------------------
/** @fn     MicTemperature  MicThermalInfo::throttleThreshold() const
 *  @return temperature
 *
 *  Returns the temperature threshold at which the thermal throttle will be
 *  activated automatically.
 */

MicTemperature  MicThermalInfo::throttleThreshold() const
{
    return m_pData->mCritical;
}


//----------------------------------------------------------------------------
/** @fn     ThrottleInfo  MicThermalInfo::throttleInfo() const
 *  @return throttle information
 *
 *  Returns thermal throttle information.
 */

ThrottleInfo  MicThermalInfo::throttleInfo() const
{
    return m_pData->mThrottleInfo;
}

//----------------------------------------------------------------------------
/** @fn     std::vector<MicTemperature>  MicThermalInfo::sensors() const
 *  @return vector of sensors
 *
 *  Returns all sensor information in a std::vector<MicTemperature>.
 */

std::vector<MicTemperature> MicThermalInfo::sensors() const
{
    return m_pData->mSensors;
}

