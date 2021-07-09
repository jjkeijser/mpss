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
#include    "ThrottleInfo.hpp"

// NAMESPACES
//
using namespace  micmgmt;


//============================================================================
/** @class    micmgmt::ThrottleInfo      ThrottleInfo.hpp
 *  @ingroup  sdk
 *  @brief    The class represents throttle event info
 *
 *  The \b %ThrottleInfo class encapsulates throttle event info.
 */


//============================================================================
//  P U B L I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     ThrottleInfo::ThrottleInfo()
 *
 *  Construct an empty throttle info object.
 */

ThrottleInfo::ThrottleInfo() :
    m_duration( 0 ),
    m_totalTime( 0 ),
    m_eventCount( 0 ),
    m_active( false )
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     ThrottleInfo::ThrottleInfo( uint32_t dur, bool active, uint32_t count, uint32_t time )
 *  @param  dur     Duration (ms)
 *  @param  active  Active state
 *  @param  count   Event count (optional)
 *  @param  time    Total throttle time (optional)
 */

ThrottleInfo::ThrottleInfo( uint32_t dur, bool active, uint32_t count, uint32_t time ) :
    m_duration( dur ),
    m_totalTime( time ),
    m_eventCount( count ),
    m_active( active )
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     ThrottleInfo::ThrottleInfo( const ThrottleInfo& that )
 *  @param  that    Other throttle info object
 *
 *  Construct a throttle info object as deep copy of \a that object.
 */

ThrottleInfo::ThrottleInfo( const ThrottleInfo& that )
{
    m_duration   = that.m_duration;
    m_totalTime  = that.m_totalTime;
    m_eventCount = that.m_eventCount;
    m_active     = that.m_active;
}


//----------------------------------------------------------------------------
/** @fn     ThrottleInfo::~ThrottleInfo()
 *
 *  Cleanup.
 */

ThrottleInfo::~ThrottleInfo()
{
    // Nothing to do (yet)
}


//----------------------------------------------------------------------------
/** @fn     ThrottleInfo&  ThrottleInfo::operator = ( const ThrottleInfo& that )
 *  @param  that    Other throttle info object
 *  @return reference to this object
 *
 *  Assign \a that object to this object by making a deep copy of \a that.
 */

ThrottleInfo&  ThrottleInfo::operator = ( const ThrottleInfo& that )
{
    if (&that != this)
    {
        m_duration   = that.m_duration;
        m_totalTime  = that.m_totalTime;
        m_eventCount = that.m_eventCount;
        m_active     = that.m_active;
    }

    return  *this;
}


//----------------------------------------------------------------------------
/** @fn     bool  ThrottleInfo::isNull() const
 *  @return null state
 *
 *  Returns \c true if the throttle object is a null object. A null object
 *  does not represent valid data.
 */

bool  ThrottleInfo::isNull() const
{
    return  ((m_duration || m_totalTime || m_eventCount) ? false : true);
}


//----------------------------------------------------------------------------
/** @fn     bool  ThrottleInfo::isActive() const
 *  @return active state
 *
 *  Returns \c true if the device is currently throttling.
 */

bool  ThrottleInfo::isActive() const
{
    return  m_active;
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  ThrottleInfo::duration() const
 *  @return duration in milliseconds
 *
 *  Returns the current throttle duration in milliseconds.
 */

uint32_t  ThrottleInfo::duration() const
{
    return  m_duration;
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  ThrottleInfo::totalTime() const
 *  @return total throttle time
 *
 *  Returns the total throttle time since device boot in milliseconds.
 */

uint32_t  ThrottleInfo::totalTime() const
{
    return  m_totalTime;
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  ThrottleInfo::eventCount() const
 *  @return event count
 *
 *  Returns the number of throttle events occurred since device boot.
 */

uint32_t  ThrottleInfo::eventCount() const
{
    return  m_eventCount;
}

