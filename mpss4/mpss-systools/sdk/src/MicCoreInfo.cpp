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
#include    "MicCoreInfo.hpp"
#include    "CoreInfoData_p.hpp"  // Private

// NAMESPACE
//
using namespace  micmgmt;
using namespace  std;


//============================================================================
/** @class    micmgmt::MicCoreInfo   MicCoreInfo.hpp
 *  @ingroup  sdk
 *  @brief    This class encapsulates core information
 *
 *  The \b %MicCoreInfo class encapsulates core information of a MIC
 *  device and provides accessors to the relevant information.
 *
 *  Please \b note that all accessors only return valid data if isValid()
 *  returns \c true.
 */


//============================================================================
//  P U B L I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     MicCoreInfo::MicCoreInfo()
 *
 *  Construct an empty core info object.
 */

MicCoreInfo::MicCoreInfo() :
    m_pData( new CoreInfoData )
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicCoreInfo::MicCoreInfo( const CoreInfoData& data )
 *  @param  data    Mic core info data
 *
 *  Construct a mic core info object based on specified core \a data.
 */

MicCoreInfo::MicCoreInfo( const CoreInfoData& data ) :
    m_pData( new CoreInfoData )
{
    m_pData->set( data );
}


//----------------------------------------------------------------------------
/** @fn     MicCoreInfo::MicCoreInfo( const MicCoreInfo& that )
 *  @param  that    Other mic info object
 *
 *  Construct a mic core info object as deep copy of \a that info object.
 */

MicCoreInfo::MicCoreInfo( const MicCoreInfo& that ) :
    m_pData( new CoreInfoData )
{
    m_pData->set( *(that.m_pData) );
}


//----------------------------------------------------------------------------
/** @fn     MicCoreInfo::~MicCoreInfo()
 *
 *  Cleanup.
 */

MicCoreInfo::~MicCoreInfo()
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicCoreInfo&  MicCoreInfo::operator = ( const MicCoreInfo& that )
 *  @param  that    Other mic core object
 *  @return reference to this object
 *
 *  Assign \a that object to this object and return the updated object.
 */

MicCoreInfo&  MicCoreInfo::operator = ( const MicCoreInfo& that )
{
    if (&that != this)
        m_pData->set( *(that.m_pData) );

    return  *this;
}


//----------------------------------------------------------------------------
/** @fn     bool  MicCoreInfo::isValid() const
 *  @return valid state
 *
 *  Returns \c true if the core info is valid.
 */

bool  MicCoreInfo::isValid() const
{
    return m_pData->mCoreCount.isValid() && m_pData->mCoreThreadCount.isValid() &&
        m_pData->mFrequency.isValid() && m_pData->mVoltage.isValid();
}


//----------------------------------------------------------------------------
/** @fn     const MicValueUInt64&  MicCoreInfo::coreCount() const
 *  @return core count
 *
 *  Returns the number of available cores.
 */

const MicValueUInt64&  MicCoreInfo::coreCount() const
{
    return m_pData->mCoreCount;
}


//----------------------------------------------------------------------------
/** @fn     cons MicValueUInt64&  MicCoreInfo::coreThreadCount() const
 *  @return core thread count
 *
 *  Returns the number of threads per core.
 */

const MicValueUInt64&  MicCoreInfo::coreThreadCount() const
{
    return m_pData->mCoreThreadCount;
}


//----------------------------------------------------------------------------
/** @fn     MicFrequency  MicCoreInfo::frequency() const
 *  @return frequency
 *
 *  Returns the core frequency.
 */

MicFrequency  MicCoreInfo::frequency() const
{
    return  (isValid() ? m_pData->mFrequency : MicFrequency());
}


//----------------------------------------------------------------------------
/** @fn     MicVoltage  MicCoreInfo::voltage() const
 *  @return voltage
 *
 *  Returns the core voltage.
 */

MicVoltage  MicCoreInfo::voltage() const
{
    return  (isValid() ? m_pData->mVoltage : MicVoltage());
}


//----------------------------------------------------------------------------
/** @fn     void  MicCoreInfo::clear()
 *
 *  Clear (invalidate) core info data.
 */

void  MicCoreInfo::clear()
{
    m_pData->clear();
}

