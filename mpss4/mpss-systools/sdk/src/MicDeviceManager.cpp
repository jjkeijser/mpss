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
#include    "MicDeviceManager.hpp"
#include    "MicDeviceManager_p.hpp"
#include    "MicDeviceError.hpp"
#include    "MpssStackBase.hpp"

// SYSTEM INCLUDES
//
#include    <list>
#include    <string>
#include    <fstream>
#include    <sstream>
#include    <iostream>
#include    <algorithm>
#include    <iosfwd>

// NAMESPACES
//
using namespace  micmgmt;
using namespace  std;


//============================================================================
/** @class  micmgmt::MicDeviceManager   MicDeviceManager.hpp
 *  @brief  This class encapsulates a Mic device manager
 *
 *  The \b %MicDeviceManager manages all detected MIC devices in a system.
 *  It is responsible to determines the number of MIC devices as well as
 *  their model and type.
 *
 *  In additon, the %MicDeviceManager provides some convenience functions to
 *  determine the availability of the MPSS stack and version information of
 *  the the MPSS stack and device driver.
 *
 *  Please \b note that only after a successful call of initialize() devices
 *  may be reported. Therefore, we recommend to call initialize() as soon as
 *  the %MicDeviceManager instance is created.
 */


//============================================================================
//  P U B L I C   C O N S T A N T S
//============================================================================

//----------------------------------------------------------------------------
/** @enum   micmgmt::MicDeviceManager::DeviceType
 *
 *  Enumerations of device types.
 */

/** @var    MicDeviceManager::DeviceType  MicDeviceManager::eDeviceTypeUndefined
 *
 *  Undefined device type.
 */

/** @var    MicDeviceManager::DeviceType  MicDeviceManager::eDeviceTypeAny
 *
 *  Any device type. This can be used to retrieve device type independent
 *  information.
 */

/** @var    MicDeviceManager::DeviceType  MicDeviceManager::eDeviceTypeKnc
 *
 *  KNC device type.
 */

/** @var    MicDeviceManager::DeviceType  MicDeviceManager::eDeviceTypeKnl
 *
 *  KNL device type.
 */


//============================================================================
//  P U B L I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     MicDeviceManager::MicDeviceManager()
 *
 *  Construct a mic device manager.
 */

MicDeviceManager::MicDeviceManager() :
    m_pImpl( new PrivImpl )
{
}


//----------------------------------------------------------------------------
/** @fn     MicDeviceManager::~MicDeviceManager()
 *
 *  Cleanup.
 */

MicDeviceManager::~MicDeviceManager()
{
    delete  m_pImpl;
}


//----------------------------------------------------------------------------
/** @fn     bool  MicDeviceManager::isInitialized() const
 *  @return initialized state
 *
 *  Returns \c true if the device manager was successfully initialized.
 */

bool  MicDeviceManager::isInitialized() const
{
    return  m_pImpl->isInitialized();
}


//----------------------------------------------------------------------------
/** @fn     bool  MicDeviceManager::isMpssAvailable() const
 *  @return MPSS stack availability
 *
 *  Returns \c true if the MPSS stack is available on the current system.
 */

bool  MicDeviceManager::isMpssAvailable() const
{
    return  (m_pImpl->m_mpssVersion.empty() ? false : true);
}


//----------------------------------------------------------------------------
/** @fn     std::string  MicDeviceManager::mpssVersion() const
 *  @return MPSS stack version
 *
 *  Returns the version of the MPSS stack.
 */

std::string  MicDeviceManager::mpssVersion() const
{
    return  m_pImpl->m_mpssVersion;
}


//----------------------------------------------------------------------------
/** @fn     std::string  MicDeviceManager::driverVersion() const
 *  @return Mic driver version
 *
 *  Returns the version of the mic driver.
 *  If versioning is not supported by the driver, an empty string is returned.
 */

std::string  MicDeviceManager::driverVersion() const
{
    return  m_pImpl->m_driverVersion;
}


//----------------------------------------------------------------------------
/** @fn     std::string  MicDeviceManager::deviceBaseName() const
 *  @return the device base name
 *
 *  Returns the device base name (e.g. "mic").
 */

std::string  MicDeviceManager::deviceBaseName() const
{
    return  m_pImpl->deviceBaseName();
}


//----------------------------------------------------------------------------
/** @fn     size_t  MicDeviceManager::deviceCount( int type ) const
 *  @param  type    Device type (optional)
 *  @return device count
 *
 *  Returns the number of mic devices detected in the system and match the
 *  specified \a type.
 */

size_t  MicDeviceManager::deviceCount( int type ) const
{
    return  m_pImpl->deviceCount( type );
}


//----------------------------------------------------------------------------
/** @fn     bool  MicDeviceManager::isDeviceType( size_t number, int type ) const
 *  @param  number  Device number
 *  @param  type    Device type
 *  @return match state
 *
 *  Determine if MIC device with specified \a number is of given \a type.
 *
 *  Please note, that in case an invalid device number or device type is
 *  specified, \c false will be returned.
 */

bool  MicDeviceManager::isDeviceType( size_t number, int type ) const
{
    if (number >= m_pImpl->deviceCount())
        return  false;

    return  (m_pImpl->deviceType( number ) == type);
}


//----------------------------------------------------------------------------
/** @fn     bool  MicDeviceManager::isDeviceAvailable( size_t number ) const
 *  @param  number  Device number
 *  @return availability state
 *
 *  Returns \c true if a device with specified device \a number exists.
 */

bool  MicDeviceManager::isDeviceAvailable( size_t number ) const
{
    return  (number < deviceCount());
}


//----------------------------------------------------------------------------
/** @fn     bool  MicDeviceManager::isDeviceAvailable( const string& name ) const
 *  @param  name    Device name
 *  @return availability state
 *
 *  Returns \c true if a device with specified \a name exists in the system.
 *
 *  This function is case sensitive. The name is expected to be something
 *  like "mic6" or "mic123".
 */

bool  MicDeviceManager::isDeviceAvailable( const string& name ) const
{
    const string  basename = deviceBaseName();

    // Is the name part correct?
    if (name.compare( 0, basename.length(), basename ) != 0)
        return  false;

    string  numstr = name.substr( basename.length() );

    // Any chance for a valid device number?
    if (numstr.find_first_not_of( "0123456789") != string::npos)
        return  false;

    stringstream  strm( numstr );
    int  number;
    strm >> number;
    if (strm.fail())
        return  false;

    return  isDeviceAvailable( number );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDeviceManager::initializationError() const
 *  @return error code
 *
 *  Returns the initialization error code. As long as the device manager is
 *  not initialized, \c MICSDKERR_SYSTEM_NOT_INITIALIZED is returned. After
 *  the device manager was initialized, this function returns the error code
 *  of the initialize() function. See the initialize() function for possible
 *  error codes.
 */

uint32_t  MicDeviceManager::initializationError() const
{
    return  MicDeviceError::errorCode( m_pImpl->m_initError );
}

//----------------------------------------------------------------------------
/** @fn     bool MicDeviceManager::PrivImpl::isValidConfigCard( size_t cardNumber) const
 *  @return valid state
 *
 *  Returns true if the card has a valid configuration.
 */
bool MicDeviceManager::isValidConfigCard( size_t number ) const
{
    return m_pImpl->isValidConfigCard(number);
}

//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDeviceManager::initialize()
 *  @return error code
 *
 *  Initialize mic device manager and return success state.
 *
 *  After a successful completion of this function, the updated MPSS stack
 *  version information, the device driver version information and the table
 *  maintaining the detected devices will be available.
 *
 *  This funtion may return any of the following error codes:
 *
 *  - MICSDKERR_NO_MPSS_STACK when no MPSS device stack is installed.
 *  - MICSDKERR_DRIVER_NOT_LOADED when the Mic device driver is not loaded.
 *  - MICSDKERR_DRIVER_NOT_INITIALIZED when the Mic device driver was not
 *  initialized.
 */

uint32_t  MicDeviceManager::initialize()
{
    if (isInitialized())
        return  MicDeviceError::errorCode( m_pImpl->m_initError );

    return  m_pImpl->initialize();
}



//============================================================================
//============================================================================
//============================================================================
//  P R I V A T E   I M P L E M E N T A T I O N
//============================================================================
//============================================================================
//============================================================================

#ifndef DOXYGEN_SUPPRESS_SECTION

//============================================================================
//  P U B L I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     MicDeviceManager::PrivImpl::PrivImpl()
 *
 *  Construct a MIC device manager implementation object.
 */

MicDeviceManager::PrivImpl::PrivImpl() :
    m_pMpssStack( 0 ),
    m_initError( MICSDKERR_SYSTEM_NOT_INITIALIZED )
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicDeviceManager::PrivImpl::~PrivImpl()
 *
 *  Cleanup.
 */

MicDeviceManager::PrivImpl::~PrivImpl()
{
    if (m_pMpssStack)
        delete  m_pMpssStack;
}


//----------------------------------------------------------------------------
/** @fn     bool  MicDeviceManager::PrivImpl::isInitialized() const
 *  @return initialized state
 *
 *  Returns \c true if the device manager was initialized.
 */

bool  MicDeviceManager::PrivImpl::isInitialized() const
{
    return  (m_pMpssStack ? true : false);
}


//----------------------------------------------------------------------------
/** @fn     std::string  MicDeviceManager::PrivImpl::deviceBaseName() const
 *  @return the device base name
 *
 *  Returns the device base name (e.g. "mic").
 */

std::string  MicDeviceManager::PrivImpl::deviceBaseName() const
{
    return  (m_pMpssStack ? m_pMpssStack->deviceBaseName() : "mic");
}

//----------------------------------------------------------------------------
/** @fn     bool MicDeviceManager::PrivImpl::isValidConfigCard( size_t cardNumber) const
 *  @return valid state
 *
 *  Returns true if the card has a valid configuration.
 */
bool MicDeviceManager::PrivImpl::isValidConfigCard( size_t number ) const
{
    for (auto device : m_invalidDevices)
    {
        if (device.first == number)
            return false;
    }
    return true;
}


//----------------------------------------------------------------------------
/** @fn     size_t  MicDeviceManager::PrivImpl::deviceCount( int type ) const
 *  @param  type    Device type
 *  @return device count
 *
 *  Returns the number of detected devices of specified \a type.
 */

size_t  MicDeviceManager::PrivImpl::deviceCount( int type ) const
{
    if (type == eDeviceTypeAny)
        return  m_deviceItems.size();

    size_t  count = 0;

    for (auto iter=m_deviceItems.begin(); iter!=m_deviceItems.end(); ++iter)
    {
        if (iter->second == type)
            count++;
    }

    return  count;
}


//----------------------------------------------------------------------------
/** @fn     int  MicDeviceManager::PrivImpl::deviceNumberAt( size_t index ) const
 *  @param  index   Device table index
 *  @return device number
 *
 *  Returns the device number at given device table entry \a index.
 */

int  MicDeviceManager::PrivImpl::deviceNumberAt( size_t index ) const
{
    if (index < m_deviceItems.size())
        return  static_cast<int>( m_deviceItems.at( index ).first );

    return  -1;
}


//----------------------------------------------------------------------------
/** @fn     int  MicDeviceManager::PrivImpl::deviceItemIndex( size_t number ) const
 *  @param  number  Device number
 *  @return device item index
 *
 *  Returns the deive item list index of device with specified \a number. If
 *  no such device number exists, -1 is returned.
 */

int  MicDeviceManager::PrivImpl::deviceItemIndex( size_t number ) const
{
    int  index = 0;

    for (auto iter=m_deviceItems.begin(); iter!=m_deviceItems.end(); ++iter)
    {
        if (iter->first == number)
            return  index;

        ++index;
    }

    return  -1;
}


//----------------------------------------------------------------------------
/** @fn     int  MicDeviceManager::PrivImpl::deviceType( size_t number ) const
 *  @param  number  Device number
 *  @return device type
 *
 *  Returns the type of device woth specified \a number.
 */

int  MicDeviceManager::PrivImpl::deviceType( size_t number ) const
{
    int  index = deviceItemIndex( number );

    if (index < 0)
        return  eDeviceTypeUndefined;

    switch (m_deviceItems.at( index ).second)
    {
      case  MpssStackBase::eTypeKnc:
        return  eDeviceTypeKnc;

      case  MpssStackBase::eTypeKnl:
        return  eDeviceTypeKnl;

      default:
        break;
    }

    return  eDeviceTypeUndefined;
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDeviceManager::PrivImpl::initialize()
 *  @return error code
 *
 *  Initialize device manager and return success state.
 *
 *  This internal function attempts to update the MPSS stack information, the
 *  device driver version information, and the detected device table.
 *
 *  This funtions may return any of the following error codes:
 *
 *  - MICSDKERR_NO_MPSS_STACK when no MPSS device stack is installed.
 *  - MICSDKERR_DRIVER_NOT_LOADED when the Mic device driver is not loaded.
 *  - MICSDKERR_DRIVER_NOT_INITIALIZED when the Mic device driver was not
 *  initialized.
 */

uint32_t  MicDeviceManager::PrivImpl::initialize()
{
    if (isInitialized())
        return  MicDeviceError::errorCode( m_initError );   // Return last init error

    m_initError = MICSDKERR_SUCCESS;    // Have a positive attitude

    for (;;)    // Fake loop
    {
        // Create a MPSS stack compatible with our environment
        m_pMpssStack = MpssStackBase::createMpssStack();
        if (!m_pMpssStack)
        {
            m_initError = MICSDKERR_NO_MPSS_STACK;
            break;
        }

        // Determine MPSS stack version
        if (MicDeviceError::isError( updateMpssVersion() ))
        {
            m_initError = MICSDKERR_NO_MPSS_STACK;
            break;
        }

        // Determine driver version
        uint32_t result = updateDriverVersion();
        if (MicDeviceError::isError( result ))
        {
            /* If driver version is not supported, we don't fail, it is out of our scope,
             * in this case, driver version will be an empty string and this will show
             * to the clients that driver version is not supported
             */
            if( result != MicDeviceError::errorCode( MICSDKERR_NOT_SUPPORTED ))
            {
                m_initError = MICSDKERR_DRIVER_NOT_LOADED;
                break;
            }
        }

        // Scan our devices
        result = updateDeviceCount();
        if (MicDeviceError::isError( result ))
        {
            if (result == MicDeviceError::errorCode( MICSDKERR_INVALID_CONFIGURATION ))
                m_initError = MICSDKERR_INVALID_CONFIGURATION;
            else if (result != MicDeviceError::errorCode( MICSDKERR_NO_DEVICES ))
                m_initError = MICSDKERR_DRIVER_NOT_INITIALIZED;

            break;
        }

        m_initError = MICSDKERR_SUCCESS;
        break;
    }

    return  MicDeviceError::errorCode( m_initError );
}


//============================================================================
//  P R I V A T E   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDeviceManager::PrivImpl::updateMpssVersion()
 *  @return error code
 *
 *  Update the MPSS stack version information and return error code.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_NO_MPSS_STACK
 */

uint32_t  MicDeviceManager::PrivImpl::updateMpssVersion()
{
    if (!m_pMpssStack)
        return  MicDeviceError::errorCode( MICSDKERR_NO_MPSS_STACK );

    return  m_pMpssStack->getSystemMpssVersion( &m_mpssVersion );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDeviceManager::PrivImpl::updateDriverVersion()
 *  @return error code
 *
 *  Update the MIC device driver version information and return error code.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_NO_MPSS_STACK
 */

uint32_t  MicDeviceManager::PrivImpl::updateDriverVersion()
{
    if (!m_pMpssStack)
        return  MicDeviceError::errorCode( MICSDKERR_NO_MPSS_STACK );

    return  m_pMpssStack->getSystemDriverVersion( &m_driverVersion );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDeviceManager::PrivImpl::updateDeviceCount()
 *  @return error code
 *
 *  Update the internal device table based on detected MIC devices and return
 *  error code.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_NO_MPSS_STACK
 *  - MICSDKERR_NO_DEVICES
 *  - MICSDKERR_INVALID_CONFIGURATION
 */

uint32_t  MicDeviceManager::PrivImpl::updateDeviceCount()
{
    m_deviceItems.clear();

    if (!m_pMpssStack)
        return  MicDeviceError::errorCode( MICSDKERR_NO_MPSS_STACK );

    std::vector<size_t>  numbers;
    uint32_t             result = 0;
    uint32_t             invalidCount = 0;

    result = m_pMpssStack->getSystemDeviceNumbers( &numbers );
    if (MicDeviceError::isError( result ))
        return  result;

    if (numbers.empty())
        return  MicDeviceError::errorCode( MICSDKERR_NO_DEVICES );


    for (auto iter=numbers.begin(); iter!=numbers.end(); ++iter)
    {
        size_t  number = *(iter);
        int     type   = MpssStackBase::eTypeUnknown;

        result = m_pMpssStack->getSystemDeviceType( &type, number );
        if (type == MpssStackBase::eTypeUnknown)
        {
            continue;   // Ignore and skip this device
        }

        if (MicDeviceError::isError( result ) && type == MpssStackBase::eTypeKnl)
        {
            m_invalidDevices.push_back( std::make_pair( number, type ));
            invalidCount++;
        }

        m_deviceItems.push_back( std::make_pair( number, type ) );
    }

    if ((m_deviceItems.size() - invalidCount) == 0) //We don't have any valid card
    {
        m_deviceItems.clear();
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_CONFIGURATION );
    }

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}

#endif  // DOXYGEN_SUPPRESS_SECTION
