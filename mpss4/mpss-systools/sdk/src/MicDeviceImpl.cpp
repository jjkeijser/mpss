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
#include    "MicDeviceImpl.hpp"

// NAMESPACES
//
using namespace  micmgmt;
using namespace  std;


//============================================================================
/** @class    micmgmt::MicDeviceImpl      MicDeviceImpl.hpp
 *  @ingroup  sdk
 *  @brief    The class represents a mic device implementation
 *
 */


//============================================================================
//  P U B L I C   C O N S T A N T S
//============================================================================

//----------------------------------------------------------------------------
/** @enum   micmgmt::MicDevice::AccessMode
 *
 *  Enumerations of property access modes.
 */

/** @var    MicDevice::AccessMode  MicDevice::eNoAccess
 *
 *  No access possible.
 */

/** @var    MicDevice::AccessMode  MicDevice::eReadOnly
 *
 *  Read-only access.
 */

/** @var    MicDevice::AccessMode  MicDevice::eWriteOnly
 *
 *  Write-only access.
 */

/** @var    MicDevice::AccessMode  MicDevice::eReadWrite
 *
 *  Both read and write access possible.
 */


//============================================================================
//  P U B L I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     MicDeviceImpl::~MicDeviceImpl()
 *
 *  Cleanup.
 */

MicDeviceImpl::~MicDeviceImpl()
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     bool  MicDeviceImpl::isDeviceOpen() const
 *  @return device open state
 *
 *  Returns \c true if the physical device is open.
 *
 *  This pure virtual function must be implemented by deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     std::string  MicDeviceImpl::deviceType() const
 *  @return device type
 *
 *  Returns the device type string. (e.g. "x200")
 *
 *  This pure virtual function must be implemented by deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     int  MicDeviceImpl::deviceNum() const
 *  @return device number
 *
 *  Returns the device number.
 */

int  MicDeviceImpl::deviceNum() const
{
    return  m_Number;
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDeviceImpl::getDeviceMode( MicDevice::DeviceMode* mode ) const
 *  @param  mode    Device mode return
 *  @return error code
 *
 *  Retrieves and returns the current device \a mode.
 *
 *  This pure virtual function must be implemented by deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDeviceImpl::getDeviceState( MicDevice::DeviceState* state ) const
 *  @param  state   Device state return
 *  @return error code
 *
 *  Retrieve current device \a state.
 *
 *  This pure virtual function must be implemented by deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDeviceImpl::getDevicePciConfigInfo( MicPciConfigInfo* info ) const
 *  @param  info    Pointer to PCI config info return
 *  @return error code
 *
 *  Retrieve PCI config info data into specified \a info object.
 *
 *  This pure virtual function must be implemented by deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDeviceImpl::getDeviceProcessorInfo( MicProcessorInfo* info ) const
 *  @param  info  Pointer to processor info return
 *  @return error code
 *
 *  Retrieve processor info data into specified \a info object.
 *
 *  MicProcessorInfo is available in any device state.
 *
 *  This pure virtual function must be implemented by deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDeviceImpl::getDeviceVersionInfo( MicVersionInfo* info ) const
 *  @param  info    Pointer to version info return
 *  @return error code
 *
 *  Retrieve and return various version \a info for this device.
 *
 *  This pure virtual function must be implemented by deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDeviceImpl::getDeviceMemoryInfo( MicMemoryInfo* info ) const
 *  @param  info  Pointer to memory info return
 *  @return error code
 *
 *  Retrieve memory info data into specified \a info object.
 *
 *  This pure virtual function must be implemented by deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDeviceImpl::getDeviceCoreInfo( MicCoreInfo* info ) const
 *  @param  info  Pointer to core info return
 *  @return error code
 *
 *  Retrieve core info data into specified \a info object.
 *
 *  This pure virtual function must be implemented by deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDeviceImpl::getDevicePlatformInfo( MicPlatformInfo* info ) const
 *  @param  info    Pointer to platform info return
 *  @return error code
 *
 *  Retrieve MIC platform info into specified \a info object.
 *
 *  This pure virtual function must be implemented by deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDeviceImpl::getDeviceThermalInfo( MicThermalInfo* info ) const
 *  @param  info  Pointer to thermal info return
 *  @return error code
 *
 *  Retrieve thermal info data into specified \a info object.
 *
 *  This pure virtual function must be implemented by deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDeviceImpl::getDeviceVoltageInfo( MicVoltageInfo* info ) const
 *  @param  info  Pointer to voltage info return
 *  @return error code
 *
 *  Retrieve voltage info data into specified \a info object.
 *
 *  This pure virtual function must be implemented by deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDeviceImpl::getDeviceCoreUsageInfo( MicCoreUsageInfo* info ) const
 *  @param  info  Pointer to core usage info return
 *  @return error code
 *
 *  Retrieve core usage info data into specified \a info object.
 *
 *  This pure virtual function must be implemented by deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDeviceImpl::getDevicePowerUsageInfo( MicPowerUsageInfo* info ) const
 *  @param  info  Pointer to power usage info return
 *  @return error code
 *
 *  Retrieve power usage info data into specified \a info object.
 *
 *  This pure virtual function must be implemented by deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDeviceImpl::getDevicePowerThresholdInfo( MicPowerThresholdInfo* info ) const
 *  @param  info  Pointer to power threshold info return
 *  @return error code
 *
 *  Retrieve power threshold info data into specified \a info object.
 *
 *  This pure virtual function must be implemented by deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDeviceImpl::getDeviceMemoryUsageInfo( MicMemoryUsageInfo* info ) const
 *  @param  info  Pointer to memory usage info return
 *  @return error code
 *
 *  Retrieve memory usage info data into specified \a info object.
 *
 *  This pure virtual function must be implemented by deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDeviceImpl::getDevicePostCode( std::string* code ) const
 *  @param  code    Pointer to post code return
 *  @return error code
 *
 *  Retrieve and return the post \a code string. The post code is often
 *  expressed as hexdecimal number, but it can also contain other character
 *  combinations. Therefore, the best practice if to leave the post code as
 *  string value.
 *
 *  This pure virtual function must be implemented by deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDeviceImpl::getDeviceLedMode( uint32_t* mode ) const
 *  @param  mode    Pointer to LED mode return
 *  @return error code
 *
 *  Retrieve and return the current LED \a mode.
 *
 *  This pure virtual function must be implemented by deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDeviceImpl::getDeviceEccMode( bool* enabled, bool* available ) const
 *  @param  enabled    Enabled state return
 *  @param  available  Available state return (optional)
 *  @return error code
 *
 *  Retrieve and return the current ECC mode enabled state.
 *
 *  This pure virtual function must be implemented by deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDeviceImpl::getDeviceTurboMode( bool* enabled, bool* available, bool* active ) const
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
 *  This pure virtual function must be implemented by deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDeviceImpl::getDeviceMaintenanceMode( bool* enabled, bool* available ) const
 *  @param  enabled    Enabled state
 *  @param  available  Available state (optional)
 *  @return error code
 *
 *  Returns device maintenance mode enabled and availability state.
 *
 *  This pure virtual function must be implemented by deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDeviceImpl::getSmBusAddressTrainingStatus( bool* busy, int* eta ) const
 *  @param  busy    Pointer to busy return variable
 *  @param  eta     Pointer to estimated time remaining return variable
 *  @return error code
 *
 *  Retrieve and return the SMBus training state and remaining time before
 *  completion.
 *
 *  This pure virtual function must be implemented by deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDeviceImpl::getDeviceSmcPersistenceState( bool* enabled, bool* available ) const
 *  @param  enabled    Pointer to enabled flag return
 *  @param  available  Pointer to available flag return (optional)
 *  @return error code
 *
 *  Check and return the SMC persistence state.
 *
 *  This pure virtual function must be implemented by deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDeviceImpl::getDeviceSmcRegisterData( uint8_t offset, uint8_t* data, size_t* size ) const
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
 *  This pure virtual function must be implemented by deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDeviceImpl::getDevicePowerStates( std::list<MicPowerState>* states ) const
 *  @param  states  Pointer to power states return
 *  @return error code
 *
 *  Retrieve and return power states.
 *
 *  This pure virtual function must be implemented by deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDeviceImpl::getDeviceFlashDeviceInfo( FlashDeviceInfo* info ) const
 *  @param  info    Pointer to info return object
 *  @return error code
 *
 *  Retrieves and returns flash device \a info.
 *
 *  This pure virtual function must be implemented by deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDeviceImpl::getDeviceFlashUpdateStatus( FlashStatus* status ) const
 *  @param  status  Pointer to status return object
 *  @return error code
 *
 *  Retrieves and returns flash update operation \a status.
 *
 *  This pure virtual function must be implemented by deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDeviceImpl::deviceOpen()
 *  @return error code
 *
 *  Open device and return success state.
 *
 *  This pure virtual function must be implemented by deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     void  MicDeviceImpl::deviceClose()
 *
 *  Close device.
 *
 *  This pure virtual function must be implemented by deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDeviceImpl::deviceBoot( const MicBootConfigInfo& info )
 *  @param  info    Boot configuration
 *  @return error code
 *
 *  Boots the device using specified boot configuration \a info.
 *
 *  This pure virtual function must be implemented by deriving classes.
 */



//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDeviceImpl::deviceReset( bool force )
 *  @param  force   Force reset flag
 *  @return error code
 *
 *  Resets the device.
 *
 *  This pure virtual function must be implemented by deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDeviceImpl::setDeviceLedMode( uint32_t mode )
 *  @param  mode    LED mode
 *  @return error code
 *
 *  Activate specified LED \a mode.
 *
 *  This pure virtual function must be implemented by deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDeviceImpl::setDeviceTurboModeEnabled( bool state )
 *  @param  state   Enabled state
 *  @return error code
 *
 *  Enable or disable the device turbo mode.
 *
 *  This pure virtual function must be implemented by deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDeviceImpl::setDeviceEccModeEnabled( bool state, int timeout, int* stage )
 *  @param  state    Enabled state
 *  @param  timeout  Wait timeout in milliseconds (optional)
 *  @param  stage    Pointer to stage return (optional)
 *  @return error code
 *
 *  Enable or disable ECC mode.
 *
 *  This pure virtual function must be implemented by deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDeviceImpl::setDeviceMaintenanceModeEnabled( bool state, int timeout )
 *  @param  state    Enabled state
 *  @param  timeout  Wait timeout in milliseconds (optional)
 *  @return error code
 *
 *  Enable or disable maintenance mode.
 *
 *  This pure virtual function must be implemented by deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDeviceImpl::setDeviceSmcPersistenceEnabled( bool state )
 *  @param  state   Enabled state
 *  @return error code
 *
 *  Enable or disable SMC persistence. When enabled, the SMC settings will
 *  survive over a boot sequence.
 *
 *  This pure virtual function must be implemented by deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDeviceImpl::setDeviceSmcRegisterData( uint8_t offset, const uint8_t* data, size_t size )
 *  @param  offset  Register offset
 *  @param  data    Pointer to data buffer
 *  @param  size    Size of data buffer
 *  @return error code
 *
 *  Write specified \a data of specified \a size to the  SMC register with
 *  given \a offset.
 *
 *  This pure virtual function must be implemented by deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDeviceImpl::setDevicePowerThreshold( PowerWindow window, uint16_t power, uint16_t time )
 *  @param  window  Power window
 *  @param  power   Power threshold in Watt
 *  @param  time    Window duration in milliseconds
 *  @return error code
 *
 *  Set the \a power threshold for specified power \a window and set the
 *  window duration to \a time milliseconds. Used in KNC devices.
 *
 *  This pure virtual function must be implemented by deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDeviceImpl::setDevicePowerThreshold( PowerWindow window, uint32_t power, uint32_t time )
 *  @param  window  Power window
 *  @param  power   Power threshold in microWatts
 *  @param  time    Window duration in microseconds
 *  @return error code
 *
 *  Set the \a power threshold for specified power \a window and set the
 *  window duration to \a time microseconds. Used in KNL devices.
 *
 *  This pure virtual function must be implemented by deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDeviceImpl::setDevicePowerStates( const std::list<MicPowerState>& states )
 *  @param  states  List of power states
 *  @return error code
 *
 *  Set specified power states.
 *
 *  This pure virtual function must be implemented by deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDeviceImpl::startDeviceSmBusAddressTraining( int hint, int timeout )
 *  @param  hint     SMBus base address hint
 *  @param  timeout  Timeout in milliseconds optional)
 *  @return error code
 *
 *  Start the SMBus address training procedure using specified base address
 *  \a hint.
 *
 *  This pure virtual function must be implemented by deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDeviceImpl::startDeviceFlashUpdate( FlashImage* image, bool force )
 *  @param  image   Pointer to flash image
 *  @param  force   Force flash flag
 *  @return error code
 *
 *  Initiate a flash update with specified flash \a image.
 *
 *  This pure virtual function must be implemented by deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDeviceImpl::finalizeDeviceFlashUpdate( bool abort )
 *  @param  abort   Abort flash operation flag (optional)
 *  @return error code
 *
 *  Finialize the flash update sequence.
 *
 *  If the optional \a abort flag is set \c true, the flash operation will be
 *  aborted when still in progress. The default behavior is to wait for flash
 *  operation completion.
 *
 *  This pure virtual function must be implemented by deriving classes.
 */


//============================================================================
//  P R O T E C T E D   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     MicDeviceImpl::MicDeviceImpl( int number )
 *  @param  number  Device number (optional)
 *
 *  Construct a MIC device implementation object for device with specified
 *  device \a number.
 */

MicDeviceImpl::MicDeviceImpl( int number ) :
    m_Number( number )
{
    // Nothing to do
}


