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
#include    "MicMemoryUsageInfo.hpp"
#include    "MemoryUsageData_p.hpp"  // Private

// NAMESPACE
//
using namespace  micmgmt;
using namespace  std;


//============================================================================
/** @class    micmgmt::MicMemoryUsageInfo   MicMemoryUsageInfo.hpp
 *  @ingroup  sdk
 *  @brief    This class encapsulates memory usage information
 *
 *  The \b %MicMemoryUsageInfo class encapsulates memory usage information
 *  of a MIC device and provides accessors to the relevant information.
 *
 *  Please \b note that all accessors only return valid data if isValid()
 *  returns \c true.
 */


//============================================================================
//  P U B L I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     MicMemoryUsageInfo::MicMemoryUsageInfo()
 *
 *  Construct an empty memory usage info object.
 */

MicMemoryUsageInfo::MicMemoryUsageInfo() :
    m_pData( new MemoryUsageData )
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicMemoryUsageInfo::MicMemoryUsageInfo( const MemoryUsageData& data )
 *  @param  data    Mic memory usage info data
 *
 *  Construct a mic memory usage info object based on specified \a data.
 */

MicMemoryUsageInfo::MicMemoryUsageInfo( const MemoryUsageData& data ) :
    m_pData( new MemoryUsageData )
{
    m_pData->set( data );
}


//----------------------------------------------------------------------------
/** @fn     MicMemoryUsageInfo::MicMemoryUsageInfo( const MicMemoryUsageInfo& that )
 *  @param  that    Other mic memory usage info object
 *
 *  Construct a mic memory usage info object as deep copy of \a that object.
 */

MicMemoryUsageInfo::MicMemoryUsageInfo( const MicMemoryUsageInfo& that ) :
    m_pData( new MemoryUsageData )
{
    m_pData->set( *(that.m_pData) );
}


//----------------------------------------------------------------------------
/** @fn     MicMemoryUsageInfo::~MicMemoryUsageInfo()
 *
 *  Cleanup.
 */

MicMemoryUsageInfo::~MicMemoryUsageInfo()
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicMemoryUsageInfo&  MicMemoryUsageInfo::operator = ( const MicMemoryUsageInfo& that )
 *  @param  that    Other mic memory usage object
 *  @return reference to this object
 *
 *  Assign \a that object to this object and return the updated object.
 */

MicMemoryUsageInfo&  MicMemoryUsageInfo::operator = ( const MicMemoryUsageInfo& that )
{
    if (&that != this)
        m_pData->set( *(that.m_pData) );

    return  *this;
}


//----------------------------------------------------------------------------
/** @fn     bool  MicMemoryUsageInfo::isValid() const
 *  @return valid state
 *
 *  Returns \c true if the memory usage info is valid.
 */

bool  MicMemoryUsageInfo::isValid() const
{
    return  (m_pData ? m_pData->mValid : false);
}

//----------------------------------------------------------------------------
/** @fn     MicMemory  MicMemoryUsageInfo::total() const
 *  @return total memory
 *
 *  Returns the total memory.
 */

MicMemory  MicMemoryUsageInfo::total() const
{
    return  (isValid() ? m_pData->mTotal : MicMemory());
}


//----------------------------------------------------------------------------
/** @fn     MicMemory  MicMemoryUsageInfo::used() const
 *  @return used memory
 *
 *  Returns the used memory. It is computed as follows:
 *  memUsed = total() - free() - cached() - buffers()
 */

MicMemory  MicMemoryUsageInfo::used() const
{
    return  (isValid() ? m_pData->mUsed : MicMemory());
}


//----------------------------------------------------------------------------
/** @fn     MicMemory  MicMemoryUsageInfo::free() const
 *  @return free memory
 *
 *  Returns the free memory.
 */

MicMemory  MicMemoryUsageInfo::free() const
{
    return  (isValid() ? m_pData->mFree : MicMemory());
}


//----------------------------------------------------------------------------
/** @fn     MicMemory  MicMemoryUsageInfo::buffers() const
 *  @return memory used for file buffers
 *
 *  Returns the amount of memory used for file buffers.
 */

MicMemory  MicMemoryUsageInfo::buffers() const
{
    return  (isValid() ? m_pData->mBuffers : MicMemory());
}


//----------------------------------------------------------------------------
/** @fn     MicMemory  MicMemoryUsageInfo::cached() const
 *  @return memory used as cache
 *
 *  Returns the amount of memory used as cache
 */

MicMemory  MicMemoryUsageInfo::cached() const
{
    return  (isValid() ? m_pData->mCached : MicMemory());
}


//----------------------------------------------------------------------------
/** @fn     void  MicMemoryUsageInfo::clear()
 *
 *  Clear (invalidate) memory info.
 */

void  MicMemoryUsageInfo::clear()
{
    m_pData->clear();
}


