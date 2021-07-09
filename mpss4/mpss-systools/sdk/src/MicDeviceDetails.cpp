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
#include    "MicDeviceDetails.hpp"
#include    "MicDevice.hpp"
#include    "MicCoreInfo.hpp"
#include    "MicMemoryInfo.hpp"
#include    "MicPlatformInfo.hpp"
#include    "MicDeviceError.hpp"

// PRIVATE DATA
//
namespace  micmgmt {
struct MicDeviceDetails::PrivData
{
    MicDevice*       mpDevice;
    MicCoreInfo      mCoreInfo;
    MicMemoryInfo    mMemoryInfo;
    MicPlatformInfo  mPlatformInfo;
};
}

// NAMESPACES
//
using namespace  micmgmt;
using namespace  std;


//============================================================================
/** @class    micmgmt::MicDeviceDetails      MicDeviceDetails.hpp
 *  @ingroup  sdk
 *  @brief    The class encapsulates the semi-static MIC device information.
 *
 *  The \b %MicDeviceDetails class encapsulates all semi-static device info.
 *  The information is semi-static and does not change for the device during
 *  operation nor does it change over a reboot or reset cycle. However, the
 *  information can and will only be captured when the device is online.
 *
 *  Once captured, the information will remain available until the device is
 *  closed.
 *
 *  Only after a power cycle, it cannot be guaranteed that this information
 *  is still valid, as system components could have been exchanged. This,
 *  however, should not pose any issues, as long as the software using this
 *  class is not aware of any system power cycles. This could potentially
 *  happen in a cluster environment.
 *
 */


//============================================================================
//  P U B L I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     MicDeviceDetails::MicDeviceDetails( MicDevice* device )
 *  @param  device  Pointer to parent device
 *
 *  Construct a device details object for specified \a device.
 *
 *  The device details class provides detailed information of the \a device
 *  which can only be collected when the device is online. Detailed data is
 *  only collected on request using the refresh() function. Before refresh()
 *  has executed successfully, the detailed data is invalid.
 */

MicDeviceDetails::MicDeviceDetails( MicDevice* device ) :
    m_pData( new PrivData )
{
    m_pData->mpDevice = device;
}


//----------------------------------------------------------------------------
/** @fn     MicDeviceDetails::~MicDeviceDetails()
 *
 *  Cleanup.
 */

MicDeviceDetails::~MicDeviceDetails()
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     int  MicDeviceDetails::deviceNumber() const
 *  @return device number
 *
 *  Returns the device number. If no device is available, -1 is returned.
 */

int  MicDeviceDetails::deviceNumber() const
{
    return  (m_pData->mpDevice ? m_pData->mpDevice->deviceNum() : -1);
}


//----------------------------------------------------------------------------
/** @fn     std::string  MicDeviceDetails::deviceName() const
 *  @return device name
 *
 *  Returns the device name string (e.g. "mic5"). If no device is available,
 *  an empty string is returned.
 */

std::string  MicDeviceDetails::deviceName() const
{
    return  (m_pData->mpDevice ? m_pData->mpDevice->deviceName() : "");
}


//----------------------------------------------------------------------------
/** @fn     std::string  MicDeviceDetails::deviceType() const
 *  @return device type
 *
 *  Returns the device type string (e.g. "x200"). If no device is available,
 *  an empty string is returned.
 */

std::string  MicDeviceDetails::deviceType() const
{
    return  (m_pData->mpDevice ? m_pData->mpDevice->deviceType() : "");
}


//----------------------------------------------------------------------------
/** @fn     const MicCoreInfo&  MicDeviceDetails::coreInfo() const
 *  @return core info
 *
 *  Returns the core information. The returned information is only valid if
 *  isValid() returns \c true.
 */

const MicCoreInfo&  MicDeviceDetails::coreInfo() const
{
    if(!m_pData->mpDevice || !m_pData->mpDevice->isOpen() || !m_pData->mpDevice->isOnline())
    {
        m_pData->mCoreInfo.clear();
        return m_pData->mCoreInfo;
    }

    if(!m_pData->mCoreInfo.isValid())
    {
        uint32_t status = m_pData->mpDevice->getCoreInfo( &m_pData->mCoreInfo );
        if (MicDeviceError::isError( status ))
        {
            m_pData->mCoreInfo.clear();
        }
    }
    return  m_pData->mCoreInfo;
}


//----------------------------------------------------------------------------
/** @fn     const MicMemoryInfo&  MicDeviceDetails::memoryInfo() const
 *  @return memory info
 *
 *  Returns the memory information. The returned information is only valid if
 *  isValid() returns \c true.
 */

const MicMemoryInfo&  MicDeviceDetails::memoryInfo() const
{
    if(!m_pData->mpDevice || !m_pData->mpDevice->isOpen() || !m_pData->mpDevice->isOnline())
    {
        m_pData->mMemoryInfo.clear();
        return m_pData->mMemoryInfo;
    }

    if(!m_pData->mMemoryInfo.isValid())
    {
        uint32_t status = m_pData->mpDevice->getMemoryInfo( &m_pData->mMemoryInfo );
        if (MicDeviceError::isError( status ))
        {
            m_pData->mMemoryInfo.clear();
        }
    }
    return  m_pData->mMemoryInfo;
}


//----------------------------------------------------------------------------
/** @fn     const MicPlatformInfo&  MicDeviceDetails::micPlatformInfo() const
 *  @return MIC platform info
 *
 *  Returns the MIC platform information. The returned information is only
 *  valid if isValid() returns \c true.
 */

const MicPlatformInfo&  MicDeviceDetails::micPlatformInfo() const
{
    if(!m_pData->mpDevice || !m_pData->mpDevice->isOpen())
    {
        m_pData->mPlatformInfo.clear();
        return m_pData->mPlatformInfo;
    }

    if(!m_pData->mPlatformInfo.isValid())
    {
        uint32_t status = m_pData->mpDevice->getPlatformInfo( &m_pData->mPlatformInfo );
        if (MicDeviceError::isError( status ))
        {
            m_pData->mPlatformInfo.clear();
        }
    }
    return  m_pData->mPlatformInfo;
}


//----------------------------------------------------------------------------
/** @fn     void  MicDeviceDetails::clear()
 *
 *  Clear (invalidate) all data.
 */

void  MicDeviceDetails::clear()
{
    m_pData->mCoreInfo.clear();
    m_pData->mMemoryInfo.clear();
    m_pData->mPlatformInfo.clear();
}

