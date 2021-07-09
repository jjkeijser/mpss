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
#include    "MicPlatformInfo.hpp"
#include    "PlatformInfoData_p.hpp"  // Private

// SYSTEM INCLUDES
//
#include    <iomanip>
#include    <sstream>
#include    <string>
#include    <algorithm>

// NAMESPACE
//
using namespace  micmgmt;
using namespace  std;


//============================================================================
/** @class    micmgmt::MicPlatformInfo   MicPlatformInfo.hpp
 *  @ingroup  sdk
 *  @brief    This class encapsulates MIC platform information data
 *
 *  The \b %MicPlatformInfo class encapsulates MIC platform information data
 *  and provides accessors to the relevant information.
 *
 *  Please \b note that all accessors only return valid data if isValid()
 *  returns \c true.
 */


//============================================================================
//  P U B L I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     MicPlatformInfo::MicPlatformInfo()
 *
 *  Construct an empty MIC platform info object.
 */

MicPlatformInfo::MicPlatformInfo() :
    m_pData( new PlatformInfoData )
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicPlatformInfo::MicPlatformInfo( const PlatformInfoData& data )
 *  @param  data    Mic platform info data
 *
 *  Construct a MIC platform info object based on specified \a data.
 */

MicPlatformInfo::MicPlatformInfo( const PlatformInfoData& data ) :
    m_pData( new PlatformInfoData )
{
    m_pData->set( data );
}


//----------------------------------------------------------------------------
/** @fn     MicPlatformInfo::MicPlatformInfo( const MicPlatformInfo& that )
 *  @param  that    Other mic platform info object
 *
 *  Construct a MIC platform info object as deep copy of \a that object.
 */

MicPlatformInfo::MicPlatformInfo( const MicPlatformInfo& that ) :
    m_pData( new PlatformInfoData )
{
    m_pData->set( *(that.m_pData) );
}


//----------------------------------------------------------------------------
/** @fn     MicPlatformInfo::~MicPlatformInfo()
 *
 *  Cleanup.
 */

MicPlatformInfo::~MicPlatformInfo()
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicPlatformInfo&  MicPlatformInfo::operator = ( const MicPlatformInfo& that )
 *  @param  that    Other mic platform info object
 *  @return reference to this object
 *
 *  Assign \a that object to this object and return the updated object.
 */

MicPlatformInfo&  MicPlatformInfo::operator = ( const MicPlatformInfo& that )
{
    if (&that != this)
        m_pData->set( *(that.m_pData) );

    return  *this;
}

//----------------------------------------------------------------------------
/** @fn     bool  MicPlatformInfo::isValid() const
 *  @return valid state
 *
 *  Returns \c true if the MIC platform info is valid.
 */
bool  MicPlatformInfo::isValid() const
{
    if( !m_pData ) return false;

    return m_pData->mPartNumber.isValid() &&
            m_pData->mSerialNo.isValid() &&
            m_pData->mFeatureSet.isValid() &&
            m_pData->mCoprocessorOs.isValid() &&
            m_pData->mCoprocessorBrand.isValid() &&
            m_pData->mBoardSku.isValid() &&
            m_pData->mBoardType.isValid() &&
            m_pData->mStrapInfo.isValid() &&
            m_pData->mManufactDate.isValid() &&
            m_pData->mManufactTime.isValid() &&
            m_pData->mSmBusAddress.isValid() &&
            m_pData->mUuid.isValid() &&
            m_pData->mMaxPower.isValid() &&
            m_pData->mCoolActive.isValid();
}

//----------------------------------------------------------------------------
/** @fn     bool  MicPlatformInfo::isCoolingActive() const
 *  @return cooling state
 *
 *  Returns \c true if the platform uses active cooling.
 */

MicValueBool  MicPlatformInfo::isCoolingActive() const
{
    return  m_pData->mCoolActive;
}


//----------------------------------------------------------------------------
/** @fn     std::string  MicPlatformInfo::partNumber() const
 *  @return part number
 *
 *  Returns the part number of the MIC device.
 */

MicValueString  MicPlatformInfo::partNumber() const
{
    return  m_pData->mPartNumber;
}


//----------------------------------------------------------------------------
/** @fn     std::string  MicPlatformInfo::manufactureDate() const
 *  @return the manufacture date
 *
 *  Returns the date on which the device was manufactured.
 *
 *  The date is formatted as YYYY-MM-DD.
 */

MicValueString  MicPlatformInfo::manufactureDate() const
{
    return m_pData->mManufactDate;
}


//----------------------------------------------------------------------------
/** @fn     std::string  MicPlatformInfo::manufactureTime() const
 *  @return manufature time
 *
 *  Returns the time of day when the device was manufactured.
 *
 *  The format is HH:MM:SS.
 */

MicValueString  MicPlatformInfo::manufactureTime() const
{
    return  m_pData->mManufactTime;
}


//----------------------------------------------------------------------------
/** @fn     std::string  MicPlatformInfo::serialNo() const
 *  @return serial number
 *
 *  Returns the device serial number.
 */

MicValueString  MicPlatformInfo::serialNo() const
{
    return  m_pData->mSerialNo;
}


//----------------------------------------------------------------------------
/** @fn     std::string  MicPlatformInfo::uuid() const
 *  @return UUID
 *
 *  Returns the device UUID.
 */

MicValueString  MicPlatformInfo::uuid() const
{
    return  m_pData->mUuid;
}


//----------------------------------------------------------------------------
/** @fn     std::string  MicPlatformInfo::featureSet() const
 *  @return SMC hardware revision
 *
 *  Returns  the device feature set (internally called hardware revision).
 */

MicValueString  MicPlatformInfo::featureSet() const
{
    return  m_pData->mFeatureSet;
}

#if 0
//----------------------------------------------------------------------------
/** @fn     std::string  MicPlatformInfo::biosReleaseDate() const
 *  @return BIOS release date
 *
 *  Returns the BIOS release date.
 *
 *  The date is formatted as YYYY-MM-DD.
 */

std::string  MicPlatformInfo::biosReleaseDate() const
{
    if (isValid())
    {
        if (m_pData->mBiosRelDate != 0)
        {
            stringstream  strm;
            strm << std::setw( 4 ) << std::setfill( '0') << ((m_pData->mBiosRelDate >> 16) & 0xffff);
            strm << "-";
            strm << std::setw( 2 ) << std::setfill( '0') << ((m_pData->mBiosRelDate >>  8) & 0x00ff);
            strm << "-";
            strm << std::setw( 2 ) << std::setfill( '0') << ((m_pData->mBiosRelDate >>  0) & 0x00ff);

            return  strm.str();
        }
    }

    return  "";
}
#endif

//----------------------------------------------------------------------------
/** @fn     std::string  MicPlatformInfo::coprocessorOs() const
 *  @return Coprocessor OS and version
 *
 *  Returns the coprocessor OS and version.
 */

MicValueString  MicPlatformInfo::coprocessorOs() const
{
    return  m_pData->mCoprocessorOs;
}


//----------------------------------------------------------------------------
/** @fn     std::string  MicPlatformInfo::coprocessorOsVersion() const
 *  @return Coprocessor version
 *
 *  Returns the coprocessor version.
 */

MicValueString  MicPlatformInfo::coprocessorOsVersion() const
{
    return  m_pData->mCoprocessorOs;
}


//----------------------------------------------------------------------------
/** @fn     std::string  MicPlatformInfo::coprocessorBrand() const
 *  @return Coprocessor brand
 *
 *  Returns the coprocessor brand.
 */

MicValueString  MicPlatformInfo::coprocessorBrand() const
{
    return  m_pData->mCoprocessorBrand;
}


//----------------------------------------------------------------------------
/** @fn     std::string  MicPlatformInfo::boardSku() const
 *  @return board SKU
 *
 *  Returns the board SKU.
 */

MicValueString  MicPlatformInfo::boardSku() const
{
    return  m_pData->mBoardSku;
}


//----------------------------------------------------------------------------
/** @fn     std::string  MicPlatformInfo::boardType() const
 *  @return board type
 *
 *  Returns the board type.
 */

MicValueString  MicPlatformInfo::boardType() const
{
    return  m_pData->mBoardType;
}


//----------------------------------------------------------------------------
/** @fn     std::string  MicPlatformInfo::strapInfo() const
 *  @return strap info
 *
 *  Returns the strap info as string.
 */

MicValueString  MicPlatformInfo::strapInfo() const
{
    return  m_pData->mStrapInfo;
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicPlatformInfo::strapInfoRaw() const
 *  @return raw strap info
 *
 *  Returns the raw strap info.
 */

MicValueUInt32  MicPlatformInfo::strapInfoRaw() const
{
    return  m_pData->mStrapInfoRaw;
}


//----------------------------------------------------------------------------
/** @fn     uint8_t  MicPlatformInfo::smBusBaseAddress() const
 *  @return SMBus base address
 *
 *  Returns the SMBus base address.
 */

MicValueUInt8  MicPlatformInfo::smBusBaseAddress() const
{
    return  m_pData->mSmBusAddress;
}


//----------------------------------------------------------------------------
/** @fn     MicPower  MicPlatformInfo::maxPower() const
 *  @return max power
 *
 *  Returns the device maximum power (TDP).
 */

MicPower  MicPlatformInfo::maxPower() const
{
    return  m_pData->mMaxPower;
}


//----------------------------------------------------------------------------
/** @fn     void  MicPlatformInfo::clear()
 *
 *  Clear (invalidate) MIC platform info data.
 */

void  MicPlatformInfo::clear()
{
    m_pData->clear();
}

