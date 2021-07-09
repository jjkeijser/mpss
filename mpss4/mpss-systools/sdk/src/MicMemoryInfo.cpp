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
#include    "MicMemoryInfo.hpp"
#include    "MemoryInfoData_p.hpp"  // Private

// NAMESPACE
//
using namespace  micmgmt;
using namespace  std;


//============================================================================
/** @class    micmgmt::MicMemoryInfo   MicMemoryInfo.hpp
 *  @ingroup  sdk
 *  @brief    This class encapsulates RAM memory information
 *
 *  The \b %MicMemoryInfo class encapsulates RAM memory information of a MIC
 *  device and provides accessors to the relevant information.
 *
 *  Please \b note that all accessors only return valid data if isValid()
 *  returns \c true.
 */


//============================================================================
//  P U B L I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     MicMemoryInfo::MicMemoryInfo()
 *
 *  Construct an empty memory inf object.
 */

MicMemoryInfo::MicMemoryInfo() :
    m_pData( new MemoryInfoData )
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicMemoryInfo::MicMemoryInfo( const MemoryInfoData& data )
 *  @param  data    Mic memory info data
 *
 *  Construct a mic memory info object based on specified memory \a data.
 */

MicMemoryInfo::MicMemoryInfo( const MemoryInfoData& data ) :
    m_pData( new MemoryInfoData )
{
    m_pData->set( data );
}


//----------------------------------------------------------------------------
/** @fn     MicMemoryInfo::MicMemoryInfo( const MicMemoryInfo& that )
 *  @param  that    Other mic memory info object
 *
 *  Construct a mic memory object as deep copy of \a that object.
 */

MicMemoryInfo::MicMemoryInfo( const MicMemoryInfo& that ) :
    m_pData( new MemoryInfoData )
{
    m_pData->set( *(that.m_pData) );
}


//----------------------------------------------------------------------------
/** @fn     MicMemoryInfo::~MicMemoryInfo()
 *
 *  Cleanup.
 */

MicMemoryInfo::~MicMemoryInfo()
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicMemoryInfo&  MicMemoryInfo::operator = ( const MicMemoryInfo& that )
 *  @param  that    Other mic memory object
 *  @return reference to this object
 *
 *  Assign \a that object to this object and return the updated object.
 */

MicMemoryInfo&  MicMemoryInfo::operator = ( const MicMemoryInfo& that )
{
    if (&that != this)
        m_pData->set( *(that.m_pData) );

    return  *this;
}


//----------------------------------------------------------------------------
/** @fn     bool  MicMemoryInfo::isValid() const
 *  @return valid state
 *
 *  Returns \c true if the memory info is valid.
 */

bool  MicMemoryInfo::isValid() const
{
    return  (m_pData ? m_pData->mValid : false);
}


//----------------------------------------------------------------------------
/** @fn     std::string  MicMemoryInfo::vendorName() const
 *  @return Vendor name string
 *
 *  Returns the vendor name. (e.g. Elpida)
 */

std::string  MicMemoryInfo::vendorName() const
{
    return  (isValid() ? m_pData->mVendorName : "");
}


//----------------------------------------------------------------------------
/** @fn     std::string  MicMemoryInfo::memoryType() const
 *  @return memory type
 *
 *  Return the memory type. (e.g. GDDR)
 */

std::string  MicMemoryInfo::memoryType() const
{
    return  (isValid() ? m_pData->mMemoryType : "");
}


//----------------------------------------------------------------------------
/** @fn     std::string  MicMemoryInfo::technology() const
 *  @return memory type
 *
 *  Return the memory type. (e.g. GDDR5)
 */

std::string  MicMemoryInfo::technology() const
{
    return  (isValid() ? m_pData->mTechnology : "");
}


//----------------------------------------------------------------------------
/** @fn     uint16_t  MicMemoryInfo::revision() const
 *  @return memory revision
 *
 *  Returns the memory revision.
 */

uint16_t  MicMemoryInfo::revision() const
{
    return  (isValid() ? m_pData->mRevision : 0);
}


//----------------------------------------------------------------------------
/** @fn     MicMemory  MicMemoryInfo::density() const
 *  @return memory density
 *
 *  Returns the memory density.
 */

MicMemory  MicMemoryInfo::density() const
{
    return  (isValid() ? m_pData->mDensity : MicMemory());
}


//----------------------------------------------------------------------------
/** @fn     MicMemory  MicMemoryInfo::size() const
 *  @return size
 *
 *  Returns the total size of the memory. Unit is kilobyte.
 */

MicMemory  MicMemoryInfo::size() const
{
    return  (isValid() ? m_pData->mSize : MicMemory());
}


//----------------------------------------------------------------------------
/** @fn     MicSpeed  MicMemoryInfo::speed() const
 *  @return speed
 *
 *  Returns the speed of the memory.
 */

MicSpeed  MicMemoryInfo::speed() const
{
    return  (isValid() ? m_pData->mSpeed : MicSpeed());
}


//----------------------------------------------------------------------------
/** @fn     MicFrequency  MicMemoryInfo::frequency() const
 *  @return frequency
 *
 *  Returns the frequency of the memory.
 */

MicFrequency  MicMemoryInfo::frequency() const
{
    return  (isValid() ? m_pData->mFrequency : MicFrequency());
}


//----------------------------------------------------------------------------
/** @fn     bool  MicMemoryInfo::isEccEnabled() const
 *  @return ecc enabled state
 *
 *  Returns \c true if ECC is enabled.
 */

bool  MicMemoryInfo::isEccEnabled() const
{
    return  (isValid() ? m_pData->mEcc : false);
}


//----------------------------------------------------------------------------
/** @fn     void  MicMemoryInfo::clear()
 *
 *  Clear (invalidate) memory info.
 */

void  MicMemoryInfo::clear()
{
    m_pData->clear();
}


