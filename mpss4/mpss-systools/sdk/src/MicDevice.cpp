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
#include    "MicDevice.hpp"
#include    "MicDeviceImpl.hpp"
#include    "MicDeviceInfo.hpp"
#include    "MicDeviceError.hpp"
#include    "MicDeviceDetails.hpp"
#include    "MicProcessorInfo.hpp"
#include    "MicPciConfigInfo.hpp"
#include    "MicVersionInfo.hpp"
#include    "MicMemoryInfo.hpp"
#include    "MicCoreInfo.hpp"
#include    "MicPlatformInfo.hpp"
#include    "MicThermalInfo.hpp"
#include    "MicVoltageInfo.hpp"
#include    "MicCoreUsageInfo.hpp"
#include    "MicPowerUsageInfo.hpp"
#include    "MicPowerThresholdInfo.hpp"
#include    "MicMemoryUsageInfo.hpp"
#include    "MicBootConfigInfo.hpp"
#include    "MicPowerState.hpp"
#include    "micmgmtCommon.hpp"
#include    "FlashImage.hpp"

// PRIVATE DATA
//
namespace {
    const char* const  DEVICE_TYPE_STR_KNL = "x200";
    const char* const  DEVICE_TYPE_STR_KNC = "x100";
}

namespace  micmgmt {
struct MicDevice::PrivData
{
    std::unique_ptr<MicDeviceImpl>     mpDeviceImpl;
    std::unique_ptr<MicDeviceInfo>     mpDeviceInfo;
    std::unique_ptr<MicDeviceDetails>  mpDeviceDetails;
    bool                               mOpen;
};
}

// NAMESPACES
//
using namespace  micmgmt;
using namespace  std;


//============================================================================
/** @class    micmgmt::MicDevice      MicDevice.hpp
 *  @ingroup  sdk
 *  @brief    The class represents a MIC device
 *
 *  The \b %MicDevice class represents a MIC device.
 *
 *  Basic operation:
 *
 *  An instance of a %MicDevice is created by calling the createDevice()
 *  function in MicDeviceFactory. %MicDeviceFactory determines what
 *  specialized implementation of %MicDevice has to be created. On successful
 *  creation, createDevice() returns a pointer to a %MicDevice object
 *  supporting the desired hardware device.
 *
 *  Basic usage:
 *
 *  The %MicDevice should be opened using the open() function before use. If
 *  the device is not used anymore, it should be closed. At destruction, the
 *  device is closed automatically. The open() function will try to get as
 *  much information from the device as possible. What information will be
 *  available at the time of the open() call depends on the state of the
 *  physical device. If the physical device is in ready state, only basic
 *  information can be collected. This information is static information and
 *  does not change as long as the host system runs.
 *
 *  If the device is in online state, the device was booted and runs it
 *  onboard operating system. In this state, all information is available.
 *  Therefore, when the open() function is called when the device is online,
 *  all information and usage data is accessible. This information contains
 *  semi-static and dynamic data.
 *
 *  When the device is reset, for instance by calling reset(), all semi-static
 *  and dynamic data that the device has cached will be invalidated. At that
 *  point, only the static device data is available.
 *
 *  When a device is booted, for instance by calling boot(), the device will
 *  cycle through its boot stages until its final online state is reached.
 *  The device state can be queried to see if the device came back online.
 *  It is strongly recommended to call the updateDeviceDetails() function
 *  after every successful reboot cycle.
 */


//============================================================================
//  P U B L I C   C O N S T A N T S
//============================================================================

//----------------------------------------------------------------------------
/** @enum   micmgmt::MicDevice::DeviceState
 *
 *  Enumerations of device states.
 */

/** @var    MicDevice::DeviceState  MicDevice::eOffline
 *
 *  Offline state.
 */

/** @var    MicDevice::DeviceState  MicDevice::eOnline
 *
 *  Online state.
 */

/** @var    MicDevice::DeviceState  MicDevice::eReady
 *
 *  Ready state.
 */

/** @var    MicDevice::DeviceState  MicDevice::eReset
 *
 *  Reset state. The card is resetting.
 */

/** @var    MicDevice::DeviceState  MicDevice::eReboot
 *
 *  Reboot state. The card is rebooting.
 */

/** @var    MicDevice::DeviceState  MicDevice::eLost
 *
 *  Lost state. The device went offline without our doing.
 */

/** @var    MicDevice::DeviceState  MicDevice::eError
 *
 *  Error state. The device has come into an unrecoverable error.
 */


//----------------------------------------------------------------------------
/** @enum   micmgmt::MicDevice::DeviceMode
 *
 *  Enumerations of device modes.
 */

/** @var    MicDevice::DeviceMode  MicDevice::eUnknown
 *
 *  Unknown mode. When the device is offline, its mode cannot be determined.
 */

/** @var    MicDevice::DeviceMode  MicDevice::eNormal
 *
 *  Normal mode. The device is capable of executing all standard operations.
 */

/** @var    MicDevice::DeviceMode  MicDevice::eCustom
 *
 *  Custom mode.
 */

/** @var    MicDevice::DeviceMode  MicDevice::eFlash
 *
 *  Flash mode. The device is capable of executing flash and other
 *  configuration change operations.
 */


//----------------------------------------------------------------------------
/** @enum   micmgmt::MicDevice::PowerWindow
 *
 *  Enumerations of property power windows.
 */

/** @var    MicDevice::PowerWindow  MicDevice::eWindow0
 *
 *  Power window 0.
 */

/** @var    MicDevice::PowerWindow  MicDevice::eWindow1
 *
 *  Power window 1.
 */


//============================================================================
//  P U B L I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     MicDevice::~MicDevice()
 *
 *  Cleanup.
 */

MicDevice::~MicDevice()
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     bool  MicDevice::isOpen() const
 *  @return device open state
 *
 *  Returns \c true if the device connection is open.
 */

bool  MicDevice::isOpen() const
{
    return  m_pData->mOpen;
}


//----------------------------------------------------------------------------
/** @fn     bool  MicDevice::isOnline() const
 *  @return online state
 *
 *  Returns \c true if the device is online.
 */

bool  MicDevice::isOnline() const
{
    if (!isOpen())
        return  false;

    return  (deviceState() == eOnline);
}


//----------------------------------------------------------------------------
/** @fn     bool  MicDevice::isReady() const
 *  @return ready state
 *
 *  Returns \c true if the device is in ready state.
 */

bool  MicDevice::isReady() const
{
    if (!isOpen())
        return  false;

    return  (deviceState() == eReady);
}


//----------------------------------------------------------------------------
/** @fn     bool  MicDevice::isCompatibleFlashImage( const FlashImage& image ) const
 *  @param  image   Flash image
 *  @return compatibility state
 *
 *  Returns \c true if specified flash \a image is compatible with this
 *  device and its current configuration.
 */

bool  MicDevice::isCompatibleFlashImage( const FlashImage& image ) const
{
    if (!isOpen())
        return  false;

    if (!image.isValid())
        return  false;

    return  image.isCompatible( this );
}


//----------------------------------------------------------------------------
/** @fn     int  MicDevice::deviceNum() const
 *  @return device number
 *
 *  Returns the device number.
 */

int  MicDevice::deviceNum() const
{
    if (m_pData->mpDeviceImpl)
        return  m_pData->mpDeviceImpl->deviceNum();

    return  -1;
}


//----------------------------------------------------------------------------
/** @fn     std::string  MicDevice::deviceName() const
 *  @return device name
 *
 *  Returns the device name string. (e.g. "mic5")
 */

std::string  MicDevice::deviceName() const
{
    if (m_pData->mpDeviceImpl)
       return  micmgmt::deviceName( m_pData->mpDeviceImpl->deviceNum() );

    return  string();
}


//----------------------------------------------------------------------------
/** @fn     std::string  MicDevice::deviceType() const
 *  @return device type
 *
 *  Returns the device type string. (e.g. "x200")
 */

std::string  MicDevice::deviceType() const
{
    if (m_pData->mpDeviceImpl)
       return  m_pData->mpDeviceImpl->deviceType();

    return  string();
}


//----------------------------------------------------------------------------
/** @fn     const MicDeviceInfo&  MicDevice::deviceInfo() const
 *  @return MIC device info
 *
 *  Returns the static device information. This information does not change
 *  during operation, nor does it change over a reboot sequence.
 *
 *  This information is available independent of device state.
 */

const MicDeviceInfo&  MicDevice::deviceInfo() const
{
    return  *m_pData->mpDeviceInfo;
}


//----------------------------------------------------------------------------
/** @fn     const MicDeviceDetails&  MicDevice::deviceDetails() const
 *  @return MIC device details
 *
 *  Returns the semi-static device details. This information does not change
 *  during operation, nor does it change over a reboot sequence. However, this
 *  information is only available when the device is in the online state.
 *
 *  This data can be refreshed by calling updateDeviceDetails() while the
 *  device is in online state.
 */

const MicDeviceDetails&  MicDevice::deviceDetails() const
{
    return  *m_pData->mpDeviceDetails;
}


//----------------------------------------------------------------------------
/** @fn     MicDevice::DeviceState  MicDevice::deviceState() const
 *  @return device state
 *
 *  Returns the current device state.
 *
 *  In case the device state cannot be determined, MicDevice::eError is
 *  returned.
 */

MicDevice::DeviceState  MicDevice::deviceState() const
{
    DeviceState  state = MicDevice::eError;

    if (MicDeviceError::isError( m_pData->mpDeviceImpl->getDeviceState( &state ) ))
        return  MicDevice::eError;

    return  state;
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDevice::getProcessorInfo( MicProcessorInfo* info ) const
 *  @param  info    Pointer to processor info return
 *  @return error code
 *
 *  Returns the processor \a info for this device.
 *
 *  This information is available independent of device state.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_INTERNAL_ERROR
 *  - MICSDKERR_DEVICE_NOT_OPEN
 */

uint32_t  MicDevice::getProcessorInfo( MicProcessorInfo* info ) const
{
    if (!info)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    info->clear();

    if (!isOpen())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_OPEN );

    return  m_pData->mpDeviceImpl->getDeviceProcessorInfo( info );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDevice::getPciConfigInfo( MicPciConfigInfo* info ) const
 *  @param  info    Pointer to PCI config info return
 *  @return error code
 *
 *  Returns the PCI config \a info for this device.
 *
 *  This information is available independent of device state.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_INTERNAL_ERROR
 *  - MICSDKERR_DEVICE_NOT_OPEN
 */

uint32_t  MicDevice::getPciConfigInfo( MicPciConfigInfo* info ) const
{
    if (!info)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    info->clear();

    if (!isOpen())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_OPEN );

    return  m_pData->mpDeviceImpl->getDevicePciConfigInfo( info );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDevice::getVersionInfo( MicVersionInfo* info ) const
 *  @param  info    Pointer to version info return
 *  @return error code
 *
 *  Retrieve various version infos for this device.
 *
 *  For KNL, version information is available independent of device state.
 *
 *  For KNC, version information is only available when the device is online.
 *  When a KNC device is not online, this function will return empty version
 *  strings and the MicVersionInfo::isAvailable() function will return
 *  \c false to indicate that.
 *
 *  The returned error code is independent of the availability of version
 *  information, in case of KNC. A non-success error code is only returned
 *  in case a real device I/O error occurs or \a info is not specified.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_INTERNAL_ERROR
 *  - MICSDKERR_DEVICE_IO_ERROR
 *  - MICSDKERR_DEVICE_NOT_OPEN
 */

uint32_t  MicDevice::getVersionInfo( MicVersionInfo* info ) const
{
    if (!info)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    info->clear();

    if (!isOpen())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_OPEN );

    return  m_pData->mpDeviceImpl->getDeviceVersionInfo( info );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDevice::getMemoryInfo( MicMemoryInfo* info ) const
 *  @param  info    Pointer to memory info return
 *  @return error code
 *
 *  Returns the memory \a info for the card associated with this device.
 *  It is recommended to check the validity of the returned memory info. See
 *  MicMemoryInfo::isValid() for further information.
 *
 *  The memory information is only available when the device is online.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_INTERNAL_ERROR
 *  - MICSDKERR_DEVICE_IO_ERROR
 *  - MICSDKERR_DEVICE_NOT_OPEN
 *  - MICSDKERR_DEVICE_NOT_ONLINE
 */

uint32_t  MicDevice::getMemoryInfo( MicMemoryInfo* info ) const
{
    if (!info)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    info->clear();

    if (!isOpen())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_OPEN );

    if (!isOnline())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_ONLINE );

    return  m_pData->mpDeviceImpl->getDeviceMemoryInfo( info );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDevice::getCoreInfo( MicCoreInfo* info ) const
 *  @param  info    Pointer to core info return
 *  @return error code
 *
 *  Returns the core \a info for this device.
 *
 *  The core information is only available when the device is online.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_DEVICE_IO_ERROR
 *  - MICSDKERR_DEVICE_NOT_OPEN
 *  - MICSDKERR_DEVICE_NOT_ONLINE
 */

uint32_t  MicDevice::getCoreInfo( MicCoreInfo* info ) const
{
    if (!info)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    info->clear();

    if (!isOpen())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_OPEN );

    if (!isOnline())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_ONLINE );

    return  m_pData->mpDeviceImpl->getDeviceCoreInfo( info );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDevice::getPlatformInfo( MicPlatformInfo* info ) const
 *  @param  info    Pointer to MIC platform info return
 *  @return error code
 *
 *  Returns the MIC platform \a info data for this device.
 *
 *  The platform information is only available when the device is online.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_INTERNAL_ERROR
 *  - MICSDKERR_DEVICE_IO_ERROR
 *  - MICSDKERR_DEVICE_NOT_OPEN
 *  - MICSDKERR_DEVICE_NOT_ONLINE
 */

uint32_t  MicDevice::getPlatformInfo( MicPlatformInfo* info ) const
{
    if (!info)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    info->clear();

    if (!isOpen())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_OPEN );

    return  m_pData->mpDeviceImpl->getDevicePlatformInfo( info );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDevice::getThermalInfo( MicThermalInfo* info ) const
 *  @param  info    Pointer to thermal info return
 *  @return error code
 *
 *  Returns the thermal \a info for this device.
 *
 *  The thermal information is only available when the device is online.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_DEVICE_IO_ERROR
 *  - MICSDKERR_DEVICE_NOT_OPEN
 *  - MICSDKERR_DEVICE_NOT_ONLINE
 */

uint32_t  MicDevice::getThermalInfo( MicThermalInfo* info ) const
{
    if (!info)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    if (!isOpen())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_OPEN );

    if (!isOnline())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_ONLINE );

    return  m_pData->mpDeviceImpl->getDeviceThermalInfo( info );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDevice::getVoltageInfo( MicVoltageInfo* info ) const
 *  @param  info    Pointer to voltage info return
 *  @return error code
 *
 *  Returns the voltage \a info for this device.
 *
 *  The voltage information is only available when the device is online.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_DEVICE_IO_ERROR
 *  - MICSDKERR_DEVICE_NOT_OPEN
 *  - MICSDKERR_DEVICE_NOT_ONLINE
 */

uint32_t  MicDevice::getVoltageInfo( MicVoltageInfo* info ) const
{
    if (!info)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    if (!isOpen())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_OPEN );

    if (!isOnline())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_ONLINE );

    return  m_pData->mpDeviceImpl->getDeviceVoltageInfo( info );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDevice::getCoreUsageInfo( MicCoreUsageInfo* info ) const
 *  @param  info    Pointer to core usage info return
 *  @return error code
 *
 *  Returns the core usage \a info for this device.
 *
 *  The core usage information is only available when the device is online.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_DEVICE_IO_ERROR
 *  - MICSDKERR_DEVICE_NOT_OPEN
 *  - MICSDKERR_DEVICE_NOT_ONLINE
 */

uint32_t  MicDevice::getCoreUsageInfo( MicCoreUsageInfo* info ) const
{
    if (!info)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    if (!isOpen())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_OPEN );

    if (!isOnline())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_ONLINE );

    return  m_pData->mpDeviceImpl->getDeviceCoreUsageInfo( info );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDevice::getPowerUsageInfo( MicPowerUsageInfo* info ) const
 *  @param  info    Pointer to power usage info return
 *  @return error code
 *
 *  Returns the power usage \a info for this device.
 *
 *  The power usage information is only available when the device is online.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_DEVICE_IO_ERROR
 *  - MICSDKERR_DEVICE_NOT_OPEN
 *  - MICSDKERR_DEVICE_NOT_ONLINE
 */

uint32_t  MicDevice::getPowerUsageInfo( MicPowerUsageInfo* info ) const
{
    if (!info)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    if (!isOpen())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_OPEN );

    if (!isOnline())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_ONLINE );

    return  m_pData->mpDeviceImpl->getDevicePowerUsageInfo( info );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDevice::getPowerThresholdInfo( MicPowerThresholdInfo* info ) const
 *  @param  info    Pointer to power threshold info return
 *  @return error code
 *
 *  Returns the power threshold \a info for this device.
 *
 *  The power threshold information is only available when the device is
 *  online.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_DEVICE_IO_ERROR
 *  - MICSDKERR_DEVICE_NOT_OPEN
 *  - MICSDKERR_DEVICE_NOT_ONLINE
 */

uint32_t  MicDevice::getPowerThresholdInfo( MicPowerThresholdInfo* info ) const
{
    if (!info)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    if (!isOpen())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_OPEN );

    if (!isOnline())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_ONLINE );

    return  m_pData->mpDeviceImpl->getDevicePowerThresholdInfo( info );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDevice::getMemoryUsageInfo( MicMemoryUsageInfo* info ) const
 *  @param  info    Pointer to memory usage info return
 *  @return error code
 *
 *  Returns the memory usage \a info for this device.
 *
 *  The memory usage information is only available when the device is online.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_DEVICE_IO_ERROR
 *  - MICSDKERR_DEVICE_NOT_OPEN
 *  - MICSDKERR_DEVICE_NOT_ONLINE
 */

uint32_t  MicDevice::getMemoryUsageInfo( MicMemoryUsageInfo* info ) const
{
    if (!info)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    if (!isOpen())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_OPEN );

    if (!isOnline())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_ONLINE );

    return  m_pData->mpDeviceImpl->getDeviceMemoryUsageInfo( info );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDevice::getFlashDeviceInfo( FlashDeviceInfo* info ) const
 *  @param  info    Pointer to info return object
 *  @return error code
 *
 *  Retrieves and returns flash device \a info.
 *
 *  The flash device information is available independent of device state.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_DEVICE_IO_ERROR
 *  - MICSDKERR_DEVICE_NOT_OPEN
 *  - MICSDKERR_NOT_SUPPORTED
 */

uint32_t  MicDevice::getFlashDeviceInfo( FlashDeviceInfo* info ) const
{
    if (!info)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    if (!isOpen())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_OPEN );

    return  m_pData->mpDeviceImpl->getDeviceFlashDeviceInfo( info );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDevice::getFlashUpdateStatus( FlashStatus* status ) const
 *  @param  status  Pointer to status return object
 *  @return error code
 *
 *  Retrieves and returns flash update operation \a status.
 *
 *  The flash status information is available independent of device state.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_NO_ACCESS
 *  - MICSDKERR_INTERNAL_ERROR
 *  - MICSDKERR_DEVICE_IO_ERROR
 *  - MICSDKERR_DEVICE_NOT_OPEN
 */

uint32_t  MicDevice::getFlashUpdateStatus( FlashStatus* status ) const
{
    if (!status)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    if (!isOpen())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_OPEN );

    return  m_pData->mpDeviceImpl->getDeviceFlashUpdateStatus( status );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDevice::getPostCode( std::string* code ) const
 *  @param  code    Pointer to post code return
 *  @return error code
 *
 *  Retrieve and return the post \a code string. The post code is often
 *  expressed as hexdecimal number, but it can also contain other character
 *  combinations. Therefore, the best practice if to leave the post code as
 *  string value.
 *
 *  The post code is available at all times, independent of the current
 *  device state.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_INTERNAL_ERROR
 *  - MICSDKERR_DEVICE_NOT_OPEN
 */

uint32_t  MicDevice::getPostCode( std::string* code ) const
{
    if (!code)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    if (!isOpen())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_OPEN );

    return  m_pData->mpDeviceImpl->getDevicePostCode( code );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDevice::getLedMode( uint32_t* mode ) const
 *  @param  mode    Pointer to LED mode return
 *  @return error code
 *
 *  Retrieve and return the current LED \a mode.
 *
 *  The LED mode is only available when the device is online.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_DEVICE_IO_ERROR
 *  - MICSDKERR_DEVICE_NOT_OPEN
 *  - MICSDKERR_DEVICE_NOT_ONLINE
 */

uint32_t  MicDevice::getLedMode( uint32_t* mode ) const
{
    if (!mode)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    if (!isOpen())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_OPEN );

    if (!isOnline())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_ONLINE );

    return  m_pData->mpDeviceImpl->getDeviceLedMode( mode );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDevice::getEccMode( bool* enabled, bool* available ) const
 *  @param  enabled    Enabled state return
 *  @param  available  Available state return (optional)
 *  @return error code
 *
 *  Get ECC mode. This function can be used to determine if ECC mode is
 *  enabled or disabled.
 *
 *  The ECC mode can only be queried when the device is in online state.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_DEVICE_IO_ERROR
 *  - MICSDKERR_DEVICE_NOT_OPEN
 *  - MICSDKERR_DEVICE_NOT_ONLINE
 */

uint32_t  MicDevice::getEccMode( bool* enabled, bool* available ) const
{
    if (!enabled)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    if (!isOpen())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_OPEN );

    if (!isOnline())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_ONLINE );

    return  m_pData->mpDeviceImpl->getDeviceEccMode( enabled, available );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDevice::getTurboMode( bool* enabled, bool* available, bool* active ) const
 *  @param  enabled    Pointer to enabled state return
 *  @param  available  Pointer to available state return (optional)
 *  @param  active     Pointer to active state return (optional)
 *  @return error code
 *
 *  Determine the current turbo mode \a enabled state, \a available state,
 *  and \a active state.
 *
 *  The turbo mode information is only available when the device is online.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_DEVICE_IO_ERROR
 *  - MICSDKERR_DEVICE_NOT_OPEN
 *  - MICSDKERR_DEVICE_NOT_ONLINE
 */

uint32_t  MicDevice::getTurboMode( bool* enabled, bool* available, bool* active ) const
{
    if (!enabled)       // available and active are optional
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    if (!isOpen())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_OPEN );

    if (!isOnline())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_ONLINE );

    return  m_pData->mpDeviceImpl->getDeviceTurboMode( enabled, available, active );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDevice::getMaintenanceMode( bool* enabled, bool* available ) const
 *  @param  enabled    Pointer to enabled state return
 *  @param  available  Pointer to available state return (optional)
 *  @return error code
 *
 *  Determine the current maintenance mode and availablility.
 *
 *  The maintenance mode information is only available when the device is
 *  online.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_INTERNAL_ERROR
 *  - MICSDKERR_DEVICE_IO_ERROR
 *  - MICSDKERR_DEVICE_NOT_OPEN
 *  - MICSDKERR_NOT_SUPPORTED
 */

uint32_t  MicDevice::getMaintenanceMode( bool* enabled, bool* available ) const
{
    if (!enabled)       // available is optional
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    if (!isOpen())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_OPEN );

    return  m_pData->mpDeviceImpl->getDeviceMaintenanceMode( enabled, available );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDevice::getSmBusAddressTrainingStatus( bool* busy, int* eta ) const
 *  @param  busy    Pointer to busy return variable
 *  @param  eta     Pointer to estimated time remaining return variable
 *  @return error code
 *
 *  Retrieve and return the SMBus training state and remaining time before
 *  completion.
 *
 *  The caller must provide a pointer to a bool in which the current SMBus
 *  address training state is returned. Optionally, a pointer to an int can
 *  be provided to receive the estimated time remaining until finished. The
 *  returned time unit is milliseconds.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_DEVICE_NOT_OPEN
 *  - MICSDKERR_DEVICE_NOT_ONLINE
 */

uint32_t  MicDevice::getSmBusAddressTrainingStatus( bool* busy, int* eta ) const
{
    if (!busy)      // eta is optional
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    if (!isOpen())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_OPEN );

    if (!isOnline())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_ONLINE );

    return  m_pData->mpDeviceImpl->getDeviceSmBusAddressTrainingStatus( busy, eta );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDevice::getSmcPersistenceState( bool* enabled, bool* available ) const
 *  @param  enabled    Pointer to enabled flag return
 *  @param  available  Pointer to available flag return (optional)
 *  @return error code
 *
 *  Check and return the SMC persistence state.
 *
 *  The SMC persistence state is only available when the device is online.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_INTERNAL_ERROR
 *  - MICSDKERR_DEVICE_NOT_OPEN
 *  - MICSDKERR_DEVICE_NOT_ONLINE
 *  - MICSDKERR_NOT_SUPPORTED
 */

uint32_t  MicDevice::getSmcPersistenceState( bool* enabled, bool* available ) const
{
    if (!enabled)       // available is optional
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    if (!isOpen())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_OPEN );

    if (!isOnline())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_ONLINE );

    return  m_pData->mpDeviceImpl->getDeviceSmcPersistenceState( enabled, available )    ;
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDevice::getSmcRegisterData( uint8_t offset, uint8_t* data, size_t* size ) const
 *  @param  offset  Register offset
 *  @param  data    Pointer to data buffer
 *  @param  size    Pointer to data size
 *  @return error code
 *
 *  Read SMC register content from register with given \a offset and return
 *  a maximum of \a size bytes in specified \a data buffer. The actual
 *  number of data bytes is returned in \a size.
 *
 *  It is possible to only retrieve the expected data \a size by specifying
 *  a NULL \a data pointer.
 *
 *  The SMC register data is only available when the device is online.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_INVALID_SMC_REG_OFFSET
 *  - MICSDKERR_SMC_OP_NOT_PERMITTED
 *  - MICSDKERR_BUFFER_TOO_SMALL
 *  - MICSDKERR_INTERNAL_ERROR
 *  - MICSDKERR_DEVICE_NOT_OPEN
 *  - MICSDKERR_DEVICE_NOT_ONLINE
 */

uint32_t  MicDevice::getSmcRegisterData( uint8_t offset, uint8_t* data, size_t* size ) const
{
    if (!data && !size)     // We need at least one of them
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    if (!isOpen())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_OPEN );

    if (!isOnline())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_ONLINE );

    return  m_pData->mpDeviceImpl->getDeviceSmcRegisterData( offset, data, size )    ;
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDevice::getPowerStates( std::list<MicPowerState>* states ) const
 *  @param  states  Pointer to power states return
 *  @return error code
 *
 *  Retrieve and return power states.
 *
 *  The power states are only available when the device is online.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_INTERNAL_ERROR
 *  - MICSDKERR_DEVICE_NOT_OPEN
 *  - MICSDKERR_DEVICE_NOT_ONLINE
 */

uint32_t  MicDevice::getPowerStates( std::list<MicPowerState>* states ) const
{
    if (!states)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    if (!isOpen())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_OPEN );

    if (!isOnline())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_ONLINE );

    return  m_pData->mpDeviceImpl->getDevicePowerStates( states );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDevice::open()
 *  @return error code
 *
 *  Open device and return success state.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INTERNAL_ERROR
 *  - MICSDKERR_DEVICE_IO_ERROR
 *  - MICSDKERR_DEVICE_NOT_OPEN
 *  - MICSDKERR_VERSION_MISMATCH
 */

uint32_t  MicDevice::open()
{
    if (isOpen())
        return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    if (!m_pData->mpDeviceImpl)
        return  MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );

    m_pData->mOpen = true;

    uint32_t  result = 0;

    // If we can open the SCIF device, we should try it here
    if (deviceState() == eOnline)
    {
        result = m_pData->mpDeviceImpl->deviceOpen();
        if (MicDeviceError::isError( result ))
        {
            m_pData->mOpen = false;
            return  result;
        }
    }

    if (deviceState() != eOnline)
        return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    if (!m_pData->mpDeviceImpl->isDeviceOpen())
    {
        m_pData->mOpen = false;
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_OPEN );
    }

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     void  MicDevice::close()
 *
 *  Close device. All cached data is invalidated.
 */

void  MicDevice::close()
{
    if (m_pData->mpDeviceImpl)
    {
        if (m_pData->mpDeviceImpl->isDeviceOpen())
            m_pData->mpDeviceImpl->deviceClose();
    }

    m_pData->mpDeviceInfo->clear();
    m_pData->mpDeviceDetails->clear();
    m_pData->mOpen = false;
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDevice::boot( const MicBootConfigInfo& info )
 *  @param  info    Boot configuration
 *  @return error code
 *
 *  Initiate a device boot sequence using given boot configuration \a info.
 *
 *  This function will not block nor wait for the device to actually boot.
 *  It only initiates the boot process and it is recommended to retrieve
 *  the device state in regular intervals to observe the final device state
 *  to turn \c eOnline.
 *
 *  \b Note: It is strongly recommended to call updateDeviceDetails() once
 *  the device has reached the \c eOnline state.
 *
 *  The device can only be booted when it is in ready state.
 *  This function requires root or administrator privileges.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_INTERNAL_ERROR
 *  - MICSDKERR_INVALID_DEVICE_NUMBER
 *  - MICSDKERR_SHARED_LIBRARY_ERROR
 *  - MICSDKERR_NO_ACCESS
 *  - MICSDKERR_DEVICE_NOT_OPEN
 *  - MICSDKERR_DEVICE_NOT_READY
 */

uint32_t  MicDevice::boot( const MicBootConfigInfo& info )
{
    if (!isOpen())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_OPEN );

    if (!isReady())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_READY );

    if (!info.isValid())
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    return  m_pData->mpDeviceImpl->deviceBoot( info );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDevice::reset( bool force )
 *  @param  force   Force reset flag (optional)
 *  @return error code
 *
 *  Initiate a device reset.
 *
 *  Under certain conditions it is possible that the device waits for a
 *  particular resource to be freed before it can reset. The optional
 *  \a force flag can be specified as \c true force a reset.
 *
 *  This function will block and  wait for the device to actually reset.
 *  Once the reset process is started, the underlying library retrieve
 *  the device state in regular intervals to observe the final device state
 *  to turn \c eReady.
 *
 *
 *  This function requires root or administrator privileges.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INTERNAL_ERROR
 *  - MICSDKERR_SHARED_LIBRARY_ERROR
 */

uint32_t  MicDevice::reset( bool force )
{
    uint32_t  result = m_pData->mpDeviceImpl->deviceReset( force );
    if (MicDeviceError::isError( result ))
        return  result;

    m_pData->mpDeviceDetails->clear();  // Invalidate dynamic data

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDevice::setLedMode( uint32_t mode )
 *  @param  mode    LED mode
 *  @return error code
 *
 *  Activate specified LED \a mode.
 *
 *  The device has to be online to be able to set the LED \a mode.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_DEVICE_IO_ERROR
 *  - MICSDKERR_DEVICE_NOT_OPEN
 *  - MICSDKERR_DEVICE_NOT_ONLINE
 */

uint32_t  MicDevice::setLedMode( uint32_t mode )
{
   if (!isOpen())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_OPEN );

    if (!isOnline())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_ONLINE );

    return  m_pData->mpDeviceImpl->setDeviceLedMode( mode );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDevice::setTurboModeEnabled( bool state )
 *  @param  state   Enabled state
 *  @return error code
 *
 *  Enable or disable the turbo mode.
 *
 *  The device has to be online to be able to set the turbo mode \a state.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_DEVICE_IO_ERROR
 *  - MICSDKERR_DEVICE_NOT_OPEN
 *  - MICSDKERR_DEVICE_NOT_ONLINE
 */

uint32_t  MicDevice::setTurboModeEnabled( bool state )
{
    if (!isOpen())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_OPEN );

    if (!isOnline())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_ONLINE );

    return  m_pData->mpDeviceImpl->setDeviceTurboModeEnabled( state );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDevice::setEccModeEnabled( bool state, int timeout, int* stage )
 *  @param  state    Enabled state
 *  @param  timeout  Wait timeout in milliseconds (optional)
 *  @param  stage    Pointer to stage return (optional)
 *  @return error code
 *
 *  Enable or disable device's ECC mode.
 *
 *  Enabling or disabling ECC mode may take some time. Unless the optional
 *  \a timeout parameter is specified as > 0, this function will return
 *  immediately. In this case, the user is adviced to observe the ECC mode
 *  state change using getEccMode() for successful completion of mode change.
 *
 *  When \a timeout is specified with a value > 0, this function will block
 *  and return only when the requested ECC mode state has been established,
 *  an error was detected, or a timeout condition occurred. The timeout
 *  mechanism is enabled, as soon as a \a timeout parameter greater than 0
 *  is specified. The timeout is to be specified in milliseconds.
 *
 *  The procedure to change ECC mode consists of a sequence of stages.
 *  Some of those stages can take a significant amount of time and they
 *  map also fail. In case of a unsuccessful change of ECC mode, the caller
 *  may be interested in which stage the error occurred. For this purpose,
 *  the caller may specify a pointer to a \a stage return variable. This
 *  variable will contain the failing stage number in case of a failure.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_DEVICE_NOT_OPEN
 *  - MICSDKERR_DEVICE_NOT_READY
 *  - MICSDKERR_DEVICE_IO_ERROR
 *  - MICSDKERR_INTERNAL_ERROR
 *  - MICSDKERR_TIMEOUT
 */

uint32_t  MicDevice::setEccModeEnabled( bool state, int timeout, int* stage )
{
    if (!isOpen())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_OPEN );

    return  m_pData->mpDeviceImpl->setDeviceEccModeEnabled( state, timeout, stage );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDevice::setMaintenanceModeEnabled( bool state, int timeout )
 *  @param  state    Enabled state
 *  @param  timeout  Wait timeout in milliseconds (optional)
 *  @return error code
 *
 *  Enable or disable device's maintenance mode.
 *
 *  Enabling or disabling maintenance mode may take some time. Unless the
 *  optional \a timeout parameter is specified as > 0, this function will
 *  return immediately. In this case, the user is adviced to observe the
 *  maintenance mode state change using getMaintenanceMode() for successful
 *  completion of mode change.
 *
 *  When \a timeout is specified with a value > 0, this function will block
 *  and return only when the requested maintenance mode has been established,
 *  an error was detected, or a timeout condition occurred. The timeout
 *  mechanism is enabled, as soon as a \a timeout parameter greater than 0
 *  is specified. The timeout is to be specified in milliseconds.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_DEVICE_NOT_OPEN
 *  - MICSDKERR_INTERNAL_ERROR
 *  - MICSDKERR_TIMEOUT
 *  - MICSDKERR_NOT_SUPPORTED
 */

uint32_t  MicDevice::setMaintenanceModeEnabled( bool state, int timeout )
{
    if (!isOpen())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_OPEN );

    return  m_pData->mpDeviceImpl->setDeviceMaintenanceModeEnabled( state, timeout );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDevice::setSmcPersistenceEnabled( bool state )
 *  @param  state   Enabled state
 *  @return error code
 *
 *  Enable or disable SMC persistence. When enabled, the SMC settings will
 *  survive over a boot sequence.
 *
 *  The device has to be online to be able to set the persistence \a state.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INTERNAL_ERROR
 *  - MICSDKERR_DEVICE_NOT_OPEN
 *  - MICSDKERR_DEVICE_NOT_ONLINE
 *  - MICSDKERR_NOT_SUPPORTED
 */

uint32_t  MicDevice::setSmcPersistenceEnabled( bool state )
{
    if (!isOpen())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_OPEN );

    if (!isOnline())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_ONLINE );

    return  m_pData->mpDeviceImpl->setDeviceSmcPersistenceEnabled( state );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDevice::setSmcRegisterData( uint8_t offset, const uint8_t* data, size_t size )
 *  @param  offset  Register offset
 *  @param  data    Pointer to data buffer
 *  @param  size    Size of data buffer
 *  @return error code
 *
 *  Write specified \a data of specified \a size to the  SMC register with
 *  given \a offset.
 *
 *  The device has to be online to be able to set SMC register \a data.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_INVALID_SMC_REG_OFFSET
 *  - MICSDKERR_SMC_OP_NOT_PERMITTED
 *  - MICSDKERR_INTERNAL_ERROR
 *  - MICSDKERR_DEVICE_NOT_OPEN
 *  - MICSDKERR_DEVICE_NOT_ONLINE
 */

uint32_t  MicDevice::setSmcRegisterData( uint8_t offset, const uint8_t* data, size_t size )
{
    if (!data)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    if (!isOpen())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_OPEN );

    if (!isOnline())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_ONLINE );

    return  m_pData->mpDeviceImpl->setDeviceSmcRegisterData( offset, data, size );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDevice::setPowerThreshold( PowerWindow window, uint32_t power, uint32_t time )
 *  @param  window  Power window
 *  @param  power   Power threshold
 *  @param  time    Window duration
 *  @return error code
 *
 *  Set the \a power threshold for specified power \a window and set the
 *  window duration to \a time.
 *
 *  The device has to be online to be able to set power thresholds.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_INTERNAL_ERROR
 *  - MICSDKERR_DEVICE_NOT_OPEN
 *  - MICSDKERR_DEVICE_NOT_ONLINE
 */

uint32_t  MicDevice::setPowerThreshold( PowerWindow window, uint32_t power, uint32_t time )
{
    if (!isOpen())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_OPEN );

    if (!isOnline())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_ONLINE );

    std::string type = deviceType();

    if (type == DEVICE_TYPE_STR_KNC)
        return  m_pData->mpDeviceImpl->setDevicePowerThreshold( window, static_cast<uint16_t>(power), static_cast<uint16_t>(time) );
    else if (type == DEVICE_TYPE_STR_KNL)
        return  m_pData->mpDeviceImpl->setDevicePowerThreshold( window, power, time );
    else
        return MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDevice::setPowerStates(const std::list<MicPowerState>& states )
 *  @param  states  Power states
 *  @return error code
 *
 *  Set specified power states configuration.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_INTERNAL_ERROR
 *  - MICSDKERR_SHARED_LIBRARY_ERROR
 *  - MICSDKERR_DEVICE_IO_ERROR
 *  - MICSDKERR_DEVICE_NOT_OPEN
 *  - MICSDKERR_NOT_SUPPORTED
 */

uint32_t  MicDevice::setPowerStates(const std::list<MicPowerState>& states )
{
    if (!isOpen())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_OPEN );

    return  m_pData->mpDeviceImpl->setDevicePowerStates( states );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDevice::startSmBusAddressTraining( int hint, int timeout )
 *  @param  hint     SMBus address hint
 *  @param  timeout  Timeout in milliseconds (optional)
 *  @return error code
 *
 *  Start SMBus address training using specified base address \a hint.
 *
 *  The SMBus training procedure will take a substantial amount of time.
 *  To give the caller the option to either wait here or do some other things
 *  in the meantime, the optional \a timeout parameter is provided. The
 *  default behavior is to block the calling thread until the SMBus training
 *  procedure has completed, but will wait no longer than 5 seconds.
 *
 *  A different \a timeout value can be provided, if desired. Specifying a
 *  timeout of 0 will cause this function to return immediately while the
 *  SMBus training operates in the background.
 *
 *  \b Note: Some functions may return the \c MICSDKERR_DEVICE_BUSY error
 *  code as long as the SMBus training is commencing.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_DEVICE_BUSY
 *  - MICSDKERR_DEVICE_IO_ERROR
 *  - MICSDKERR_DEVICE_NOT_OPEN
 */

uint32_t  MicDevice::startSmBusAddressTraining( int hint, int timeout )
{
    if (!isOpen())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_OPEN );

    /// \todo Can we rely on permission checks from lower level code?

    return  m_pData->mpDeviceImpl->startDeviceSmBusAddressTraining( hint, timeout );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDevice::startFlashUpdate( FlashImage* image, bool force )
 *  @param  image   Pointer to flash image
 *  @param  force   Force update flag (optional)
 *  @return error code
 *
 *  Initiate a flash update with specified flash \a image.
 *
 *  @note   When this function is called, it is absolutely vital to call the
 *          complement of this function finalizeFlashUpdate() at the end of
 *          the flash procedure. To determine the end of the flash operation,
 *          call getFlashStatus() until the returned status signals done. The
 *          preferred function to use for this is FlashStatus::isDone().
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_DEVICE_BUSY
 *  - MICSDKERR_DEVICE_IO_ERROR
 *  - MICSDKERR_INTERNAL_ERROR
 *  - MICSDKERR_NO_ACCESS
 *  - MICSDKERR_DEVICE_NOT_OPEN
 */

uint32_t  MicDevice::startFlashUpdate( FlashImage* image, bool force )
{
    if (!image)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    if (!isOpen())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_OPEN );

    return  m_pData->mpDeviceImpl->startDeviceFlashUpdate( image, force );
}

uint32_t  MicDevice::startFlashUpdate( std::string & path, std::unique_ptr<micmgmt::FwToUpdate> & fwToUpdate )
{
    if (path.empty())
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    if (!isOpen())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_OPEN );

    return  m_pData->mpDeviceImpl->startDeviceFlashUpdate( path, fwToUpdate );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDevice::finalizeFlashUpdate( bool abort )
 *  @param  abort   Abort flash operation (optional)
 *  @return error code
 *
 *  Finialize the flash update sequence.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_DEVICE_BUSY
 *  - MICSDKERR_INTERNAL_ERROR
 *  - MICSDKERR_NO_ACCESS
 *  - MICSDKERR_DEVICE_NOT_OPEN
 */

uint32_t  MicDevice::finalizeFlashUpdate( bool abort )
{
    if (!isOpen())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_OPEN );

    return  m_pData->mpDeviceImpl->finalizeDeviceFlashUpdate( abort );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDevice::updateDeviceDetails()
 *  @return error code
 *
 *  Update semi-static device details and return success state.
 *
 *  This function should be called after a successful device (re)boot
 *  operation.
 *
 *  The device has to be online for this function to succeed.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_SHARED_LIBRARY_ERROR
 *  - MICSDKERR_DEVICE_OPEN_FAILED
 *  - MICSDKERR_DEVICE_IO_ERROR
 *  - MICSDKERR_DEVICE_NOT_OPEN
 *  - MICSDKERR_DEVICE_NOT_ONLINE
 *  - MICSDKERR_VERSION_MISMATCH
 */

uint32_t  MicDevice::updateDeviceDetails()
{
    if (!isOpen())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_OPEN );

    if (!isOnline())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_ONLINE );

    if (!m_pData->mpDeviceImpl->isDeviceOpen())
    {
        // Online but not connected to hardware. This can be after a reboot.
        uint32_t  result = m_pData->mpDeviceImpl->deviceOpen();
        if (MicDeviceError::isError( result ))
            return  result;
    }
    // Invalidate device details. Data will be refreshed when requested again by callers.
    m_pData->mpDeviceDetails->clear();
    return MicDeviceError::isError ( MICSDKERR_SUCCESS );

}


//============================================================================
//  P U B L I C   S T A T I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     std::string  MicDevice::deviceStateAsString( DeviceState state )
 *  @param  state   Device state
 *  @return state string
 *
 *  Returns the string version of specified \a state.
 */

std::string  MicDevice::deviceStateAsString( DeviceState state )
{
    switch (state)
    {
      case  eOffline:
        return  "offline";

      case  eOnline:
        return  "online";

      case  eReady:
        return  "ready";

      case  eReset:
        return  "reset";

      case  eReboot:
        return  "reboot";

      case  eLost:
        return  "lost";

      case  eError:
        return  "error";

      default:
        break;
    }

    return  "Not yet implemented";
}


//============================================================================
//  P R I V A T E   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     MicDevice::MicDevice( MicDeviceImpl* device )
 *  @param  device  Pointer to device implementation
 *
 *  Construct a MIC device usign specified \a device implementation. The
 *  %MicDevice object takes ownership of the specified \a device and it will
 *  destroy the device implementation when %MicDevice is deleted.
 */

MicDevice::MicDevice( MicDeviceImpl* device ) :
    m_pData( new PrivData )
{
    m_pData->mpDeviceImpl.reset( device );
    m_pData->mpDeviceInfo.reset( new MicDeviceInfo( this ) );
    m_pData->mpDeviceDetails.reset( new MicDeviceDetails( this ) );

    m_pData->mOpen = false;
}


//----------------------------------------------------------------------------
/** @fn     MicDeviceImpl*  MicDevice::injector() const
 *  @return pointer to device implementation
 *
 *  Returns a pointer to the internal device implementation object.
 */

MicDeviceImpl*  MicDevice::injector() const
{
    return  m_pData->mpDeviceImpl.get();
}

