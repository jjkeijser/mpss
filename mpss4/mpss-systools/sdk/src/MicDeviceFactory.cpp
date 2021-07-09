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
#include    "MicDeviceFactory.hpp"
#include    "MicDeviceManager.hpp"
#include    "MicDeviceError.hpp"
#include    "KnlMockDevice.hpp"
#ifndef MICMGMT_MOCK_ENABLE
#  include  "KnlDevice.hpp"
#endif

// SYSTEM INCLUDES
//
#include    <string>
#include    <sstream>
#include    <iosfwd>

// NAMESPACES
//
using namespace  micmgmt;
using namespace  std;


//============================================================================
/** @class  micmgmt::MicDeviceFactory   MicDeviceFactory.hpp
 *  @brief  This class encapsulates a Mic device factory
 *
 *  The \b %MicDeviceFactory class creates specialized MicDevice objects on
 *  request. The %MicDeviceFactory has no knowledge of the underlaying
 *  platform nor about the available devices. It uses the MicDeviceManager to
 *  provide this information.
 */


//============================================================================
//  P U B L I C   S T A T I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     MicDeviceFactory::MicDeviceFactory( MicDeviceManager* manager )
 *  @param  manager  Pointer to device manager
 *
 *  Construct a Mic device factory object using specified device \a manager.
 */

MicDeviceFactory::MicDeviceFactory( MicDeviceManager* manager ) :
    m_pManager( manager ),
    m_errorCode( 0 )
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicDeviceFactory::~MicDeviceFactory()
 *
 *  Cleanup.
 */

MicDeviceFactory::~MicDeviceFactory()
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicDevice*  MicDeviceFactory::createDevice( size_t number ) const
 *  @param  number  Device number
 *  @return pointer to created device
 *
 *  Create a specialized Mic device instance supporting the physical device
 *  with specified device \a number. A pointer to the freshly created Mic
 *  device object is returned. If no device could be created or an invalid
 *  device number was specified, no Mic device is created and 0 is returned
 *  and the associated error code can be retrieved through errorCode().
 *
 *  On success, the errorCode() function will return MICSDKERR_SUCCESS.
 *  On failure, errorCode() will return one of the following error codes:
 *  - MICSDKERR_SYSTEM_NOT_INITIALIZED
 *  - MICSDKERR_INVALID_DEVICE_NUMBER
 *  - MICSDKERR_INTERNAL_ERROR
 *  - MICSDKERR_UNKNOWN_DEVICE_TYPE
 */

MicDevice*  MicDeviceFactory::createDevice( size_t number ) const
{
    if (!m_pManager || !m_pManager->isInitialized())
    {
        m_errorCode = MicDeviceError::errorCode( MICSDKERR_SYSTEM_NOT_INITIALIZED );
        return  0;
    }

    if (number >= m_pManager->deviceCount())
    {
        m_errorCode = MicDeviceError::errorCode( MICSDKERR_INVALID_DEVICE_NUMBER );
        return  0;
    }

    if (m_pManager->isDeviceType( number, MicDeviceManager::eDeviceTypeKnc ))
    {
        m_errorCode = MICSDKERR_NOT_SUPPORTED;
        return nullptr;
    }

     if (m_pManager->isDeviceType( number, MicDeviceManager::eDeviceTypeKnl ))
     {
#ifdef  MICMGMT_MOCK_ENABLE
        MicDevice*  device = new MicDevice( new KnlMockDevice( static_cast<int>( number ) ) );
#else
        MicDevice*  device = new MicDevice( new KnlDevice( static_cast<int>( number ) ) );
#endif
        if (device)
            m_errorCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );
        else
            m_errorCode = MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );

        return  device;
     }

     m_errorCode = MicDeviceError::errorCode( MICSDKERR_UNKNOWN_DEVICE_TYPE );

    return  0;
}


//----------------------------------------------------------------------------
/** @fn     MicDevice*  MicDeviceFactory::createDevice( const std::string& name ) const
 *  @param  name    Device name
 *  @return pointer to created device
 *
 *  Creates a specialized Mic device instance supporting the physical device
 *  with specified device \a name. A pointer to the freshly created Mic
 *  device object is returned. If no device could be created or an invalid
 *  device name was specified, no Mic device is created and 0 is returned.
 *  The associated error code is available through errorCode().
 *
 *  On success, the errorCode() function will return MICSDKERR_SUCCESS.
 *  On failure, errorCode() will return one of the following error codes:
 *  - MICSDKERR_SYSTEM_NOT_INITIALIZED
 *  - MICSDKERR_NO_SUCH_DEVICE
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_INTERNAL_ERROR
 *  - MICSDKERR_UNKNOWN_DEVICE_TYPE
 */

MicDevice*  MicDeviceFactory::createDevice( const std::string& name ) const
{
    if (name.empty())
    {
        m_errorCode = MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );
        return  0;
    }

    if (!m_pManager || !m_pManager->isInitialized())
    {
        m_errorCode = MicDeviceError::errorCode( MICSDKERR_SYSTEM_NOT_INITIALIZED );
        return  0;
    }

    if (!m_pManager->isDeviceAvailable( name ))
    {
        m_errorCode = MicDeviceError::errorCode( MICSDKERR_NO_SUCH_DEVICE );
        return  0;
    }

    // Here, we know that we have micXXX, so we can extract the number directly
    stringstream  strm( name.substr( m_pManager->deviceBaseName().length() ) );
    int  number;
    strm >> number;

    if (strm.fail())
    {
        m_errorCode = MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );
        return  0;
    }

    return  createDevice( number );
}


//----------------------------------------------------------------------------
/** @fn     MicDevice*  MicDeviceFactory::createDevice( MicDeviceImpl* device ) const
 *  @param  device  Pointer to device implementation
 *  @return pointer to created device
 *
 *  Creates a specialized MIC device object using specified implementation
 *  \a device.
 *
 *  If no device could be created or an invalid implementation device was
 *  specified, no MIC device is created and 0 is returned.
 *
 *  The associated error code is available through errorCode().
 *
 *  On success, the errorCode() function will return MICSDKERR_SUCCESS.
 *  On failure, errorCode() will return one of the following error codes:
 *  - MICSDKERR_SYSTEM_NOT_INITIALIZED
 *  - MICSDKERR_INVALID_DEVICE_NUMBER
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_INTERNAL_ERROR
 *  - MICSDKERR_UNKNOWN_DEVICE_TYPE
 *  - MICSDKERR_INVALID_CONFIGURATION
 *
 *  Please note that the injected \a device has to be compatible with the
 *  underlaying stack. So, a MPSS 3 stack now only supports KNC devices and
 *  MPSS 4 only KNL devices.
 */

MicDevice*  MicDeviceFactory::createDevice( MicDeviceImpl* device ) const
{
    if (!device)
    {
        m_errorCode = MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );
        return  0;
    }

    if ((device->deviceType() == "x100") && (!m_pManager->isDeviceType( device->deviceNum(), MicDeviceManager::eDeviceTypeKnc )))
    {
        m_errorCode = MicDeviceError::errorCode( MICSDKERR_INVALID_CONFIGURATION );
        return  0;
    }

    if ((device->deviceType() == "x200") && (!m_pManager->isDeviceType( device->deviceNum(), MicDeviceManager::eDeviceTypeKnl )))
    {
        m_errorCode = MicDeviceError::errorCode( MICSDKERR_INVALID_CONFIGURATION );
        return  0;
    }

    if (!m_pManager || !m_pManager->isInitialized())
    {
        m_errorCode = MicDeviceError::errorCode( MICSDKERR_SYSTEM_NOT_INITIALIZED );
        return  0;
    }

    if ((device->deviceNum() < 0) || (device->deviceNum() >= static_cast<int>( m_pManager->deviceCount() )))
    {
        m_errorCode = MicDeviceError::errorCode( MICSDKERR_INVALID_DEVICE_NUMBER );
        return  0;
    }

    return  new MicDevice( device );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDeviceFactory::errorCode() const
 *  @return error code
 *
 *  Returns the error code describing the last occurred error condition.
 */

uint32_t  MicDeviceFactory::errorCode() const
{
    return  m_errorCode;
}


