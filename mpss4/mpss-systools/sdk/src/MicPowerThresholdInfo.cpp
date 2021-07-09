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
#include    "MicPowerThresholdInfo.hpp"
#include    "MicPower.hpp"
//
#include    "PowerThresholdData_p.hpp"  // Private

// NAMESPACE
//
using namespace  micmgmt;


//============================================================================
/** @class    micmgmt::MicPowerThresholdInfo   MicPowerThresholdInfo.hpp
 *  @ingroup  sdk
 *  @brief    This class encapsulates power threshold information
 *
 *  The \b %MicPowerThresholdInfo class encapsulates power thresholds
 *  information of a MIC device and provides accessors to the relevant
 *  information.
 *
 *  Please \b note that all accessors only return valid data if isValid()
 *  returns \c true.
 */


//============================================================================
//  P U B L I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     MicPowerThresholdInfo::MicPowerThresholdInfo()
 *
 *  Construct an empty power thresholds info object.
 */

MicPowerThresholdInfo::MicPowerThresholdInfo() :
    m_pData( new PowerThresholdData )
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicPowerThresholdInfo::MicPowerThresholdInfo( const PowerThresholdData& data )
 *  @param  data    Mic power threshold info data
 *
 *  Construct a mic power threshold info object based on given info \a data.
 */

MicPowerThresholdInfo::MicPowerThresholdInfo( const PowerThresholdData& data ) :
    m_pData( new PowerThresholdData )
{
    m_pData->set( data );
}


//----------------------------------------------------------------------------
/** @fn     MicPowerThresholdInfo::MicPowerThresholdInfo( const MicPowerThresholdInfo& that )
 *  @param  that    Other mic power threshold info object
 *
 *  Construct a mic power threshold info object as deep copy of \a that
 *  object.
 */

MicPowerThresholdInfo::MicPowerThresholdInfo( const MicPowerThresholdInfo& that ) :
    m_pData( new PowerThresholdData )
{
    m_pData->set( *(that.m_pData) );
}


//----------------------------------------------------------------------------
/** @fn     MicPowerThresholdInfo::~MicPowerThresholdInfo()
 *
 *  Cleanup.
 */

MicPowerThresholdInfo::~MicPowerThresholdInfo()
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicPowerThresholdInfo&  MicPowerThresholdInfo::operator = ( const MicPowerThresholdInfo& that )
 *  @param  that    Other mic power threshold info object
 *  @return reference to this object
 *
 *  Assign \a that object to this object and return the updated object.
 */

MicPowerThresholdInfo&  MicPowerThresholdInfo::operator = ( const MicPowerThresholdInfo& that )
{
    if (&that != this)
        m_pData->set( *(that.m_pData) );

    return  *this;
}


//----------------------------------------------------------------------------
/** @fn     bool  MicPowerThresholdInfo::isValid() const
 *  @return valid state
 *
 *  Returns \c true if the power threshold info is valid.
 */

bool  MicPowerThresholdInfo::isValid() const
{
    return  (m_pData ? m_pData->mValid : false);
}


//----------------------------------------------------------------------------
/** @fn     bool  MicPowerThresholdInfo::isPersistent() const
 *  @return persistence state
 *
 *  Returns \c true if the power threshold settings persist over a reboot.
 */

bool  MicPowerThresholdInfo::isPersistent() const
{
    return  (isValid() ? m_pData->mPersistence : false);
}


//----------------------------------------------------------------------------
/** @fn     MicPower  MicPowerThresholdInfo::loThreshold() const
 *  @return low threshold setting
 *
 *  Returns the low power threshold setting.
 */

MicPower  MicPowerThresholdInfo::loThreshold() const
{
    return  (isValid() ? m_pData->mLoThreshold : MicPower());
}


//----------------------------------------------------------------------------
/** @fn     MicPower  MicPowerThresholdInfo::hiThreshold() const
 *  @return high threshold setting
 *
 *  Returns the high power threshold setting.
 */

MicPower  MicPowerThresholdInfo::hiThreshold() const
{
    return  (isValid() ? m_pData->mHiThreshold : MicPower());
}


//----------------------------------------------------------------------------
/** @fn     MicPower  MicPowerThresholdInfo::maximum() const
 *  @return maximum power
 *
 *  Returns the maximum power possible physically.
 */

MicPower  MicPowerThresholdInfo::maximum() const
{
    return  (isValid() ? m_pData->mMaximum : MicPower());
}


//----------------------------------------------------------------------------
/** @fn     MicPower  MicPowerThresholdInfo::window0Threshold() const
 *  @return window 0 threshold
 *
 *  Returns the power threshold for window 0.
 */

MicPower  MicPowerThresholdInfo::window0Threshold() const
{
    return  (isValid() ? m_pData->mWindow0Threshold : MicPower());
}


//----------------------------------------------------------------------------
/** @fn     MicPower  MicPowerThresholdInfo::window1Threshold() const
 *  @return window 1 threshold
 *
 *  Returns the power threshold for window 1.
 */

MicPower  MicPowerThresholdInfo::window1Threshold() const
{
    return  (isValid() ? m_pData->mWindow1Threshold : MicPower());
}


//----------------------------------------------------------------------------
/** @fn     int  MicPowerThresholdInfo::window0Time() const
 *  @return window 0 time
 *
 *  Returns the windows 0 time in microseconds.
 */

int  MicPowerThresholdInfo::window0Time() const
{
    return  (isValid() ? m_pData->mWindow0Time : 0);
}


//----------------------------------------------------------------------------
/** @fn     int  MicPowerThresholdInfo::window1Time() const
 *  @return window 1 time
 *
 *  Returns the windows 1 time in microseconds.
 */

int  MicPowerThresholdInfo::window1Time() const
{
    return  (isValid() ? m_pData->mWindow1Time : 0);
}

