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
#include    "MicDeviceInfo.hpp"
#include    "MicDeviceError.hpp"
#include    "MicPciConfigInfo.hpp"
#include    "MicProcessorInfo.hpp"
#include    "MicVersionInfo.hpp"
#include    "MicDevice.hpp"

// PRIVATE DATA
//
namespace  micmgmt {
struct MicDeviceInfo::PrivData
{
    MicDevice*        mpDevice;
    MicPciConfigInfo  mPciConfig;
    MicProcessorInfo  mProcessorInfo;
    MicVersionInfo    mVersionInfo;
};
}

// NAMESPACES
//
using namespace  micmgmt;
using namespace  std;


//============================================================================
/** @class    micmgmt::MicDeviceInfo      MicDeviceInfo.hpp
 *  @ingroup  sdk
 *  @brief    The class encapsulates the static MIC device information.
 *
 *  The \b %MicDeviceInfo class encapsulates all static MIC device info. The
 *  information is static and does not change for the device during operation
 *  nor does it change over a reboot or reset cycle. The static device info
 *  is available at any time and independent of the device state.
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
/** @fn     MicDeviceInfo::MicDeviceInfo( MicDevice* device )
 *  @param  device  Pointer to parent device
 *
 *  Construct a MIC device info object for specified \a device.
 *
 *  The device info class will maintain all static device information.
 */

MicDeviceInfo::MicDeviceInfo( MicDevice* device ) :
    m_pData( new PrivData )
{
    m_pData->mpDevice = device;
}


//----------------------------------------------------------------------------
/** @fn     MicDeviceInfo::~MicDeviceInfo()
 *
 *  Cleanup.
 */

MicDeviceInfo::~MicDeviceInfo()
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     int  MicDeviceInfo::deviceNumber() const
 *  @return device number
 *
 *  Returns the device number. If no device is available, -1 is returned.
 */

int  MicDeviceInfo::deviceNumber() const
{
    return  (m_pData->mpDevice ? m_pData->mpDevice->deviceNum() : -1);
}


//----------------------------------------------------------------------------
/** @fn     std::string  MicDeviceInfo::deviceName() const
 *  @return device name
 *
 *  Returns the device name string (e.g. "mic5"). If no device is available,
 *  an empty string is returned.
 */

std::string  MicDeviceInfo::deviceName() const
{
    return  (m_pData->mpDevice ? m_pData->mpDevice->deviceName() : "");
}


//----------------------------------------------------------------------------
/** @fn     std::string  MicDeviceInfo::deviceType() const
 *  @return device type
 *
 *  Returns the device type string (e.g. "x200"). If no device is available,
 *  an empty string is returned.
 */

std::string  MicDeviceInfo::deviceType() const
{
    return  (m_pData->mpDevice ? m_pData->mpDevice->deviceType() : "");
}


//----------------------------------------------------------------------------
/** @fn     std::string  MicDeviceInfo::deviceSku() const
 *  @return device SKU
 *
 *  Returns the device SKU string. If no device is available, an empty SKU
 *  string is returned.
 */

std::string  MicDeviceInfo::deviceSku() const
{
    return  processorInfo().sku().value();
}


//----------------------------------------------------------------------------
/** @fn     const MicPciConfigInfo&  MicDeviceInfo::pciConfigInfo() const
 *  @return PCI configuration info
 *
 *  Returns the PCI configuration information.
 */

const MicPciConfigInfo&  MicDeviceInfo::pciConfigInfo() const
{
    if (m_pData->mpDevice == 0 || !m_pData->mpDevice->isOpen())
    {
        m_pData->mPciConfig.clear();
        return  m_pData->mPciConfig;
    }

    if(!m_pData->mPciConfig.isValid())
    {
        uint32_t error = m_pData->mpDevice->getPciConfigInfo( &m_pData->mPciConfig );
        if(MicDeviceError::isError( error ))
        {
            m_pData->mPciConfig.clear();
        }
    }
    return  m_pData->mPciConfig;
}


//----------------------------------------------------------------------------
/** @fn     const MicProcessorInfo&  MicDeviceInfo::processorInfo() const
 *  @return procesor info
 *
 *  Returns the processor information.
 */

const MicProcessorInfo&  MicDeviceInfo::processorInfo() const
{
    if (m_pData->mpDevice == 0 || !m_pData->mpDevice->isOpen())
    {
        m_pData->mProcessorInfo.clear();
        return  m_pData->mProcessorInfo;
    }

    if(!m_pData->mProcessorInfo.isValid())
    {
        uint32_t error = m_pData->mpDevice->getProcessorInfo( &m_pData->mProcessorInfo );
        if(MicDeviceError::isError( error ))
        {
            m_pData->mProcessorInfo.clear();
        }
    }
    return  m_pData->mProcessorInfo;
}


//----------------------------------------------------------------------------
/** @fn     const MicVersionInfo&  MicDeviceInfo::versionInfo() const
 *  @return version info
 *
 *  Returns the version information.
 */

const MicVersionInfo&  MicDeviceInfo::versionInfo() const
{
    if (m_pData->mpDevice == 0 || !m_pData->mpDevice->isOpen())
    {
        m_pData->mVersionInfo.clear();
        return  m_pData->mVersionInfo;
    }

    if(!m_pData->mVersionInfo.isValid())
    {
        uint32_t error = m_pData->mpDevice->getVersionInfo( &m_pData->mVersionInfo );
        if(MicDeviceError::isError( error ))
        {
            m_pData->mVersionInfo.clear();
        }
    }
    return  m_pData->mVersionInfo;
}


//----------------------------------------------------------------------------
/** @fn     void  MicDeviceInfo::clear()
 *
 *  Clear (invalidate) all data.
 */

void  MicDeviceInfo::clear()
{
    m_pData->mPciConfig.clear();
    m_pData->mProcessorInfo.clear();
    m_pData->mVersionInfo.clear();
}

