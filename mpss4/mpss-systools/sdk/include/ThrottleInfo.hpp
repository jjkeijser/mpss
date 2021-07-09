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

#ifndef MICMGMT_THROTTLEINFO_HPP
#define MICMGMT_THROTTLEINFO_HPP

// SYSTEM INCLUDES
//
#include    <cstdint>

// NAMESPACE
//
namespace  micmgmt
{


//============================================================================
//  CLASS:  ThrottleInfo

class  ThrottleInfo
{

public:

    ThrottleInfo();
    ThrottleInfo( uint32_t dur, bool active, uint32_t count=0, uint32_t time=0 );
    ThrottleInfo( const ThrottleInfo& that );
   ~ThrottleInfo();

    ThrottleInfo&  operator = ( const ThrottleInfo& that );

    bool           isNull() const;
    bool           isActive() const;
    uint32_t       duration() const;
    uint32_t       totalTime() const;
    uint32_t       eventCount() const;


private:

    uint32_t  m_duration;
    uint32_t  m_totalTime;
    uint32_t  m_eventCount;
    bool      m_active;

};

//----------------------------------------------------------------------------

}   // namespace micmgmt

//----------------------------------------------------------------------------

#endif // MICMGMT_THROTTLEINFO_HPP
