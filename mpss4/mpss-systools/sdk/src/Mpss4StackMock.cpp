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
#include    "Mpss4StackMock.hpp"
#include    "MicDeviceError.hpp"
#include    "MicBootConfigInfo.hpp"
#include    "micmgmtCommon.hpp"
//
#include    "PciConfigData_p.hpp"

// SYSTEM INCLUDES
//
#include    <string>
#include    <vector>

// LOCAL CONSTANTS
//
namespace  {
const char* const  MPSS_MIC_DEV_BASENAME = "mic";
const char* const  MPSS_DRIVER_VERSION   = "1.4.0 (mock)";
const char* const  KNL_MOCK_DEVICE_ENV   = "KNL_MOCK_CNT";
const char* const  KNC_MOCK_DEVICE_ENV   = "KNC_MOCK_CNT";
}

// PRIVATE DATA
//
namespace  micmgmt {
typedef std::pair<size_t,int>    DeviceItem;
typedef std::vector<DeviceItem>  DeviceList;

struct  Mpss4StackMock::PrivData
{
    std::string  mDriverVersion;
    DeviceList   mDevices;
};
}

// NAMESPACES
//
using namespace  micmgmt;
using namespace  std;


//============================================================================
/** @class    micmgmt::Mpss4StackMock  Mpss4StackMock.hpp
 *  @ingroup  sdk
 *  @brief    The class represents a mock MPSS 4.x stack class
 *
 *  The \b %Mpss4StackMock class encapsulates a mock MPSS 4.x stack.
 */


//============================================================================
//  P U B L I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     Mpss4StackMock::Mpss4StackMock( int device )
 *  @param  device  Device number
 *
 *  Construct a MPSS 4.x stack object using specified \a device number.
 */

Mpss4StackMock::Mpss4StackMock( int device ) :
    Mpss4StackBase( device ),
    m_pData( new PrivData )
{
    // Set versions
    m_pData->mDriverVersion.assign( MPSS_DRIVER_VERSION );
}


//----------------------------------------------------------------------------
/** @fn     Mpss4StackMock::~Mpss4StackMock()
 *
 *  Cleanup.
 */

Mpss4StackMock::~Mpss4StackMock()
{
    // Nothing to do (yet)
}


//----------------------------------------------------------------------------
/** @fn     std::string  Mpss4StackMock::mpssHomePath() const
 *  @return home path
 *
 *  Returns the MPSS home directory path.
 */

std::string  Mpss4StackMock::mpssHomePath() const
{
    return  userHomePath();
}


//----------------------------------------------------------------------------
/** @fn     std::string  Mpss4StackMock::mpssBootPath() const
 *  @return boot path
 *
 *  Returns the MPSS boot image directory path.
 */

std::string  Mpss4StackMock::mpssBootPath() const
{
	return  mpssHomePath();
}


//----------------------------------------------------------------------------
/** @fn     std::string  Mpss4StackMock::mpssFlashPath() const
 *  @return flash path
 *
 *  Returns the MPSS flash image directory path.
 */

std::string  Mpss4StackMock::mpssFlashPath() const
{
    return  mpssHomePath();
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  Mpss4StackMock::getSystemDriverVersion( std::string* version ) const
 *  @param  version  Version string return
 *  @return error code
 *
 *  Determine the device driver version and return in specified \a version
 *  parameter.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 */

uint32_t  Mpss4StackMock::getSystemDriverVersion( std::string* version ) const
{
    if (!version)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    *version = m_pData->mDriverVersion;

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  Mpss4StackMock::getSystemDeviceNumbers( std::vector<size_t>* list ) const
 *  @param  list    Pointer to device number list return
 *  @return error code
 *
 *  Create and return a \a list of detected device numbers in the system.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 */

uint32_t  Mpss4StackMock::getSystemDeviceNumbers( std::vector<size_t>* list ) const
{
    if (!list)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    list->clear();

    // Populate device list
    string  knl_env = getEnvVar( KNL_MOCK_DEVICE_ENV ).second;
    size_t  knl_cnt = (knl_env.empty() == false) ? stol(knl_env, NULL, 10) : 0;

    for (size_t i=0; i<knl_cnt; i++)
        m_pData->mDevices.push_back( std::make_pair( i, eTypeKnl ) );

    // Allow mixed configuration for testing the same
    string  knc_env = getEnvVar( KNC_MOCK_DEVICE_ENV ).second;
    size_t  knc_cnt = (knc_env.empty() == false) ? stol( knc_env, NULL, 10 ) : 0;

    for (size_t i=knl_cnt; i<knl_cnt+knc_cnt; i++)
        m_pData->mDevices.push_back( std::make_pair( i, eTypeUnknown ) );   // MPSS 4 should not detect KNC

    // Give caller what we have
    for (auto iter=m_pData->mDevices.begin(); iter!=m_pData->mDevices.end(); ++iter)
        list->push_back( iter->first );

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  Mpss4StackMock::getSystemDeviceType( int* type, size_t number ) const
 *  @param  type    Device type return
 *  @param  number  Device number
 *  @return error code
 *
 *  Determine and return the device \a type for device with given \a number.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_INVALID_DEVICE_NUMBER
 */

uint32_t  Mpss4StackMock::getSystemDeviceType( int* type, size_t number ) const
{
    if (!type)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    if (number >= m_pData->mDevices.size())
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_DEVICE_NUMBER );

    *type = m_pData->mDevices.at( number ).second;

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  Mpss4StackMock::getSystemProperty( std::string* data, const std::string& name ) const
 *  @param  data    Data return
 *  @param  name    Property name
 *  @return error code
 *
 *  Get system property with given \a name and return property \a data.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 */

uint32_t  Mpss4StackMock::getSystemProperty( std::string* data, const std::string& name ) const
{
    if (!data || name.empty())
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    data->clear();

    /// \todo Should we implement a table of mock values?

    *data = "Mocked Data";

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  Mpss4StackMock::getDeviceMode( int* mode ) const
 *  @param  mode    Pointer to mode return variable
 *  @return error code
 *
 *  Retrieve and return the current device \a mode.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_NOT_SUPPORTED
 */

uint32_t  Mpss4StackMock::getDeviceMode( int* mode ) const
{
    if (!mode)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    *mode = eModeUnknown;

    return  MicDeviceError::errorCode( MICSDKERR_NOT_SUPPORTED );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  Mpss4StackMock::getDeviceState( int* state ) const
 *  @param  state   Pointer to state return variable
 *  @return error code
 *
 *  Retrieve and return the current device \a state.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_NOT_SUPPORTED
 */

uint32_t  Mpss4StackMock::getDeviceState( int* state ) const
{
    if (!state)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    *state = eStateInvalid; // The mock device will not call this function anyway

    return  MicDeviceError::errorCode( MICSDKERR_NOT_SUPPORTED );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  Mpss4StackMock::getDevicePciConfigData( PciConfigData* data ) const
 *  @param  data    Pointer to PCI config data return
 *  @return error code
 *
 *  Determine and return PCI configuration for the associated device into
 *  specified PCI \a data structure.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_NOT_SUPPORTED
 */

uint32_t  Mpss4StackMock::getDevicePciConfigData( PciConfigData* data ) const
{
    if (!data)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    data->clear();      // The mock device will not call this function anyway

    return  MicDeviceError::errorCode( MICSDKERR_NOT_SUPPORTED );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  Mpss4StackMock::getDeviceProperty( std::string* data, const std::string& name ) const
 *  @param  data    Data return
 *  @param  name    Property name
 *  @return error code
 *
 *  Get device property with given \a name and return property \a data.
 *
 *  Under Linux, device properties are stored in the SYSFS system. In case of
 *  MIC devices, the property is mapped to /sys/class/mic/\<device name\>/\<name\>.
 *
 *  This implementation composes the path based on base path and device name
 *  and reads the content of the specified \a name into given \a data.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_NOT_SUPPORTED
 */

uint32_t  Mpss4StackMock::getDeviceProperty( std::string* data, const std::string& name ) const
{
    if (!data || name.empty())
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    data->clear();      // The mock device will not call this function anyway

    return  MicDeviceError::errorCode( MICSDKERR_NOT_SUPPORTED );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  Mpss4StackMock::deviceReset( bool force )
 *  @param  force   Force flag (optional)
 *  @return error code
 *
 *  Reset device. This brings a device from online state to ready state, if
 *  all goes well.
 *
 *  This function is non-blocking and will return immediately when the reset
 *  has been accepted by the hardware device. The caller should call the
 *  getDeviceState() function to observe the device state transitions.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_NOT_SUPPORTED
 */

uint32_t  Mpss4StackMock::deviceReset( bool force )
{
    (void) force;   // The mock device will not call this function anyway

    return  MicDeviceError::errorCode( MICSDKERR_NOT_SUPPORTED );
}


