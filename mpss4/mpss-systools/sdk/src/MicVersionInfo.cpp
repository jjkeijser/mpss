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
#include    "MicVersionInfo.hpp"
#include    "VersionInfoData_p.hpp"   // Private

// NAMESPACE
//
using namespace  micmgmt;
using namespace  std;

namespace
{
    const unsigned int ntbVersionSize = 8;
}

//============================================================================
/** @class    micmgmt::MicVersionInfo   MicVersionInfo.hpp
 *  @ingroup  sdk
 *  @brief    This class encapsulates device components version information
 *
 *  The \b %MicVersionInfo class encapsulates device components version
 *  information and provides accessors to the relevant information.
 *
 *  %MicVersionInfo is available in any device state, as long as the
 *  device driver is loaded.
 *
 *  Please \b note that all accessors only return valid data if isValid()
 *  returns \c true.
 */


//============================================================================
//  P U B L I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     MicVersionInfo::MicVersionInfo()
 *
 *  Construct an empty version info object.
 */

MicVersionInfo::MicVersionInfo() :
    m_pData( new VersionInfoData )
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicVersionInfo::MicVersionInfo( const VersionInfoData& data )
 *  @param  data    Mic version info data
 *
 *  Construct a mic version info object based on specified version \a data.
 */

MicVersionInfo::MicVersionInfo( const VersionInfoData& data ) :
    m_pData( new VersionInfoData )
{
    m_pData->set( data );
}


//----------------------------------------------------------------------------
/** @fn     MicVersionInfo::MicVersionInfo( const MicVersionInfo& that )
 *  @param  that    Other mic version info object
 *
 *  Construct a mic version object as deep copy of \a that object.
 */

MicVersionInfo::MicVersionInfo( const MicVersionInfo& that ) :
    m_pData( new VersionInfoData )
{
    m_pData->set( *(that.m_pData) );
}


//----------------------------------------------------------------------------
/** @fn     MicVersionInfo::~MicVersionInfo()
 *
 *  Cleanup.
 */

MicVersionInfo::~MicVersionInfo()
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicVersionInfo&  MicVersionInfo::operator = ( const MicVersionInfo& that )
 *  @param  that    Other mic version object
 *  @return reference to this object
 *
 *  Assign \a that object to this object and return the updated object.
 */

MicVersionInfo&  MicVersionInfo::operator = ( const MicVersionInfo& that )
{
    if (&that != this)
        m_pData->set( *(that.m_pData) );

    return  *this;
}


//----------------------------------------------------------------------------
/** @fn     bool  MicVersionInfo::isValid() const
 *  @return valid state
 *
 *  Returns \c true if the version info is valid.
 */

bool  MicVersionInfo::isValid() const
{
    if( !m_pData ) return false;

    return  m_pData->mFlashVersion.isValid() &&
            m_pData->mBiosVersion.isValid() &&
            m_pData->mBiosRelDate.isValid() &&
            m_pData->mOemStrings.isValid() &&
            m_pData->mPlxVersion.isValid() &&
            m_pData->mSmcVersion.isValid() &&
            m_pData->mSmcBootVersion.isValid() &&
            m_pData->mRamCtrlVersion.isValid() &&
            m_pData->mMeVersion.isValid() &&
            m_pData->mMpssVersion.isValid() &&
            m_pData->mDriverVersion.isValid();
}


//----------------------------------------------------------------------------
/** @fn     const MicValueString&  MicVersionInfo::flashVersion() const
 *  @return Flash version
 *
 *  Returns the loaded and active flash image version.
 */

const MicValueString&  MicVersionInfo::flashVersion() const
{
    return  m_pData->mFlashVersion;
}


//----------------------------------------------------------------------------
/** @fn     const MicValueString&  MicVersionInfo::biosVersion() const
 *  @return BIOS version
 *
 *  Returns the BIOS version.
 */

const MicValueString  MicVersionInfo::biosVersion() const
{
    if (string(m_pData->mBiosVersion.value()).empty())
    {
        return string("N/A");
    }
    return  m_pData->mBiosVersion.value();
}


//----------------------------------------------------------------------------
/** @fn     const MicValueString&  MicVersionInfo::biosReleaseDate() const
 *  @return BIOS release date
 *
 *  Returns the BIOS release date.
 *
 *  The date is formatted as YYYY-MM-DD.
 */

const MicValueString&  MicVersionInfo::biosReleaseDate() const
{
    return  m_pData->mBiosRelDate;
}


//----------------------------------------------------------------------------
/** @fn     const MicValueString&  MicVersionInfo::oemStrings() const
 *  @return OEM strings
 *
 *  Returns the OEM strings as one long string. The formatting of the string
 *  is OEM specific. Most likely, the individual strings are separated by a
 *  new line character.
 */

const MicValueString&  MicVersionInfo::oemStrings() const
{
    return  m_pData->mOemStrings;
}


//----------------------------------------------------------------------------
/** @fn     const MicValueString&  MicVersionInfo::ntbVersion() const
 *  @return NTB EEPROM version
 *
 *  Returns the device' SMC boot loader's NTB EEPROM version.
 */

const MicValueString  MicVersionInfo::ntbVersion() const
{
    if (string(m_pData->mPlxVersion.value()).empty())
    {
        return string("N/A");
    }
    return m_pData->mPlxVersion.value();
}


//----------------------------------------------------------------------------
/** @fn     const MicValueString&  MicVersionInfo::fabVersion() const
 *  @return Fab version
 *
 *  Returns the device Fab version.
 */

const MicValueString  MicVersionInfo::fabVersion() const
{
    if (!m_pData->mFabVersion.value().compare("0"))
    {
        return string("Fab_A");
    }
    if (!m_pData->mFabVersion.value().compare("1"))
    {
        return string("Fab_B");
    }
    return string("N/A");
}


//----------------------------------------------------------------------------
/** @fn     const MicValueString&  MicVersionInfo::smcVersion() const
 *  @return SMC version
 *
 *  Returns the device SMC version.
 */

const MicValueString  MicVersionInfo::smcVersion() const
{
    if (string(m_pData->mSmcVersion.value()).empty())
    {
        return string("N/A");
    }
    return m_pData->mSmcVersion.value();
}


//----------------------------------------------------------------------------
/** @fn     const MicValueString&  MicVersionInfo::smcBootLoaderVersion() const
 *  @return SMC boot loader version
 *
 *  Returns the device SMC boot loader version.
 */

const MicValueString&  MicVersionInfo::smcBootLoaderVersion() const
{
    return  m_pData->mSmcBootVersion;
}


//----------------------------------------------------------------------------
/** @fn     const MicValueString&  MicVersionInfo::ramControllerVersion() const
 *  @return RAM controller version
 *
 *  Returns the device's RAM controller version. For KNL it returns the MCDRAM
 *  controller version. For KNC, an empty string is returned.
 */

const MicValueString&  MicVersionInfo::ramControllerVersion() const
{
    return  m_pData->mRamCtrlVersion;
}


//----------------------------------------------------------------------------
/** @fn     const MicValueString&  MicVersionInfo::meVersion() const
 *  @return ME version
 *
 *  Returns the management engine version.
 */

const MicValueString  MicVersionInfo::meVersion() const
{
    if (string(m_pData->mMeVersion.value()).empty())
    {
        return string("N/A");
    }
    return m_pData->mMeVersion.value();
}


//----------------------------------------------------------------------------
/** @fn     const MicValueString&  MicVersionInfo::mpssStackVersion() const
 *  @return MPSS stack version
 *
 *  Returns the MPSS stack version.
 */

const MicValueString&  MicVersionInfo::mpssStackVersion() const
{
    return  m_pData->mMpssVersion;
}


//----------------------------------------------------------------------------
/** @fn     const MicValueString&  MicVersionInfo::driverVersion() const
 *  @return driver version
 *
 *  Returns the device driver version.
 */

const MicValueString&  MicVersionInfo::driverVersion() const
{
    return  m_pData->mDriverVersion;
}


//----------------------------------------------------------------------------
/** @fn     void  MicVersionInfo::clear()
 *
 *  Clear (invalidate) version info data.
 */

void  MicVersionInfo::clear()
{
    m_pData->clear();
}

