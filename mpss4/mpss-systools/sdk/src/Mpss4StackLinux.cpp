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
#include    "Mpss4StackLinux.hpp"
#include    "MicDeviceError.hpp"
#include    "MicBootConfigInfo.hpp"
#include    "MicPciConfigInfo.hpp"
#include    "MicSpeed.hpp"
#include    "micmgmtCommon.hpp"
#include    "SharedLibraryAdapter.hpp"
//
#include    "PciConfigData_p.hpp"

#ifdef UNIT_TESTS
#include    "LibmpssconfigFunctions.hpp"
#endif // UNIT_TESTS

// MPSS INCLUDES
//
#ifdef  __cplusplus
extern "C" {    // Linux driver team decided to ignore C++
#endif
#include    "../../3rd-party/linux/drivers/mpss4/mic.h"  /// \todo Fix when final header path known
#ifdef  __cplusplus
}
#endif

// SYSTEM INCLUDES
//
#include    <list>
#include    <cstdio>
#include    <cstring>
#include    <fstream>
#include    <sstream>
#include    <string>
#include    <dirent.h>
#include    <sys/types.h>

// LOCAL CONSTANTS
//
namespace  {

// Return component value or -1
int  ExtractComponentValue( const std::string& strvalue, const std::string& name )
{
    size_t  cindex = strvalue.find( name );
    if (cindex == std::string::npos)
        return  -1;

    size_t  vstart = strvalue.find_first_of( micmgmt::mpss4linux_detail::NUMERIC_CHARACTERS, cindex );
    if (vstart == std::string::npos)
        return  -1;

    size_t  vend = strvalue.find_first_not_of( micmgmt::mpss4linux_detail::NUMERIC_CHARACTERS, vstart );

    std::stringstream  strm( strvalue.substr( vstart, vend - vstart ) );
    uint32_t  value = 0;
    strm >> value;

    return  (strm.fail() ? -1 : value);
}

uint32_t  ParseComponentValue( std::string* data, const std::string& valstr, const std::string& name )
{
    // Extract model component and return as hexadecimal string
    int  value = ExtractComponentValue( valstr, name );
    if (value < 0)
        return  micmgmt::MicDeviceError::errorCode( MICSDKERR_PROPERTY_NOT_FOUND );

    // Return model as hex string in data parameter
    std::stringstream  valstrm;
    valstrm << std::hex << value;
    if (valstrm.fail())
        return  micmgmt::MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );

    *data = valstrm.str();
    return  micmgmt::MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}
}

namespace  micmgmt {
}

// NAMESPACES
//
using namespace  micmgmt;
using namespace  micmgmt::mpss4linux_detail;
using namespace  std;


//============================================================================
/** @class    micmgmt::Mpss4StackLinux  Mpss4StackLinux.hpp
 *  @ingroup  sdk
 *  @brief    The class represents a MPSS 4.x stack class for Linux
 *
 *  The \b %Mpss4StackLinux class encapsulates a Linux based MPSS 4.x stack.
 */


//============================================================================
//  P U B L I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     Mpss4StackLinux::Mpss4StackLinux( int device )
 *  @param  device  Device number
 *
 *  Construct a MPSS 4.x stack object using specified \a device number.
 */

Mpss4StackLinux::Mpss4StackLinux( int device ) :
    Mpss4StackBase( device ),
    m_pData( new PrivData ),
    m_pLoader(new SharedLibraryAdapter)
{
    m_pData->mpBootLinux = 0;
    m_pData->mpBootMedia = 0;
    m_pData->mpGetState  = 0;
    m_pData->mpReset     = 0;

    loadLib();
}

#ifdef UNIT_TESTS
Mpss4StackLinux::Mpss4StackLinux( int device, LibmpssconfigFunctions& functions ) :
    Mpss4StackBase( device ), m_pData( new PrivData )
{
    m_pData->mpBootLinux = functions.boot_linux;
    m_pData->mpBootMedia = functions.boot_media;
    m_pData->mpGetState  = functions.get_state;
    m_pData->mpReset     = functions.reset_node;
}
#endif


//----------------------------------------------------------------------------
/** @fn     Mpss4StackLinux::~Mpss4StackLinux()
 *
 *  Cleanup.
 */

Mpss4StackLinux::~Mpss4StackLinux()
{
    unloadLib();
}


//----------------------------------------------------------------------------
/** @fn     std::string  Mpss4StackLinux::mpssHomePath() const
 *  @return home path
 *
 *  Returns the MPSS home directory path.
 */

std::string  Mpss4StackLinux::mpssHomePath() const
{
    return  micmgmt::mpssInstallFolder();
}


//----------------------------------------------------------------------------
/** @fn     std::string  Mpss4StackLinux::mpssBootPath() const
 *  @return boot path
 *
 *  Returns the MPSS boot image directory path.
 */

std::string  Mpss4StackLinux::mpssBootPath() const
{
    return  string( mpssHomePath() + '/' + MPSS_DEFAULT_BOOT_DIR );     /// \todo According to Don, this will be /lib/firmware
}


//----------------------------------------------------------------------------
/** @fn     std::string  Mpss4StackLinux::mpssFlashPath() const
 *  @return flash path
 *
 *  Returns the MPSS flash image directory path.
 */

std::string  Mpss4StackLinux::mpssFlashPath() const
{
    return  string( mpssHomePath() + '/' + MPSS_DEFAULT_FLASH_DIR );    /// \todo According to Don, this will be /lib/firmware
}

//----------------------------------------------------------------------------
/** @fn     uint32_t  Mpss4StackLinux::getSystemDriverVersion( std::string* version ) const
 *  @param  version  Version string return
 *  @return error code
 *
 *  Determine the device driver version and return in specified \a version
 *  parameter.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_NOT_SUPPORTED
 */

uint32_t  Mpss4StackLinux::getSystemDriverVersion( std::string* version ) const
{
    if (!version)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    version->clear();

    return  MicDeviceError::errorCode( MICSDKERR_NOT_SUPPORTED );
}

//----------------------------------------------------------------------------
/** @fn     uint32_t  Mpss4StackLinux::getSystemDeviceNumbers( std::vector<size_t>* list ) const
 *  @param  list    Pointer to device number list return
 *  @return error code
 *
 *  Create and return a \a list of detected device numbers in the system.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_DRIVER_NOT_LOADED
 *  - MICSDKERR_DRIVER_NOT_INITIALIZED
 */

uint32_t Mpss4StackLinux::getSystemDeviceNumbers( std::vector<size_t>* list ) const
{
    DirentFunctions direntFunctions;
    return getSystemDeviceNumbers( list, direntFunctions );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  Mpss4StackLinux::getSystemDeviceType( int* type, size_t number ) const
 *  @param  type    Device type return
 *  @param  number  Device number
 *  @return error code
 *
 *  Determine and return the device \a type for device with given \a number.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 */

uint32_t  Mpss4StackLinux::getSystemDeviceType( int* type, size_t number ) const
{
    return getSystemDeviceType<Mpss4StackLinux>( type, number, *this );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  Mpss4StackLinux::getSystemProperty( std::string* data, const std::string& name ) const
 *  @param  data    Data return
 *  @param  name    Property name
 *  @return error code
 *
 *  Get system property with given \a name and return property \a data.
 *
 *  Under Linux, device properties are stored in the SYSFS system. In case of
 *  MIC devices, the property is mapped to /sys/class/mic/\<name\>.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_INTERNAL_ERROR
 */

uint32_t  Mpss4StackLinux::getSystemProperty( std::string* data, const std::string& name ) const
{
    std::ifstream ifs;
    return getSystemProperty( data, name, ifs );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  Mpss4StackLinux::getDevicePciConfigData( PciConfigData* data ) const
 *  @param  data    Pointer to PCI config data return
 *  @return error code
 *
 *  Determine and return PCI configuration for the associated device into
 *  specified PCI \a data structure.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_INVALID_DEVICE_NUMBER
 *  - MICSDKERR_NO_ACCESS
 *  - MICSDKERR_PROPERTY_NOT_FOUND
 *  - MICSDKERR_INTERNAL_ERROR
 */

uint32_t  Mpss4StackLinux::getDevicePciConfigData( PciConfigData* data ) const
{
    return getDevicePciConfigData<Mpss4StackLinux>( data, *this );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  Mpss4StackLinux::getDeviceProperty( std::string* data, const std::string& name ) const
 *  @param  data    Data return
 *  @param  name    Property name
 *  @return error code
 *
 *  Get device property with given \a name and return property \a data.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_INVALID_DEVICE_NUMBER
 *  - MICSDKERR_NO_ACCESS
 *  - MICSDKERR_PROPERTY_NOT_FOUND
 *  - MICSDKERR_INTERNAL_ERROR
 */

uint32_t  Mpss4StackLinux::getDeviceProperty( std::string* data, const std::string& name ) const
{
    std::ifstream ifs;
    return getDeviceProperty<std::ifstream, ParseComponentValue>( data, name, ifs );
}

//----------------------------------------------------------------------------
/** @fn     uint32_t  Mpss4StackLinux::readPciConfigSpace( std::string* bool* hasAccess) const
 *  @param  data    Data return
 *  @param  bool    Access flag return
 *  @return error code
 *
 *  Read the PCI config space into \a data and set the \a hasAccess flag.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_INVALID_DEVICE_NUMBER
 *  - MICSDKERR_INTERNAL_ERROR
 */

uint32_t Mpss4StackLinux:: readPciConfigSpace( std::string* data, bool* hasAccess ) const
{
    std::ifstream ifs;
    return readPciConfigSpace<std::ifstream>( data, hasAccess, ifs );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  Mpss4StackLinux::getDeviceState( int* state ) const
 *  @param  state   Pointer to state return variable
 *  @return error code
 *
 *  Retrieve and return the current device \a state.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_INTERNAL_ERROR
 *  - MICSDKERR_SHARED_LIBRARY_ERROR
 */

uint32_t  Mpss4StackLinux::getDeviceState( int* state ) const
{
    if (!state)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    if(!m_pData->mpGetState)
        return MicDeviceError::errorCode( MICSDKERR_SHARED_LIBRARY_ERROR );

    *state = eStateInvalid;

    const char*  sstate = m_pData->mpGetState( deviceNumber() );
    if (!sstate)
        return  MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );

    string  statestr( sstate );

    if (statestr == PROPVAL_DEVICE_STATE_READY)
        *state = eStateReady;
    else if (statestr == PROPVAL_DEVICE_STATE_BOOTING)
        *state = eStateBooting;
    else if (statestr == PROPVAL_DEVICE_STATE_RESETTING)
        *state = eStateResetting;
    else if (statestr == PROPVAL_DEVICE_STATE_ONLINE)
        *state = eStateOnline;
    else if (statestr == PROPVAL_DEVICE_STATE_ONLINE_FW)
        *state = eStateOnlineFW;
    else if (statestr == PROPVAL_DEVICE_STATE_BOOTING_FW)
        *state = eStateBootingFW;
    else if (statestr == PROPVAL_DEVICE_STATE_UNKNOWN)
        *state = eStateInvalid;
    else
        return  MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  Mpss4StackLinux::deviceReset( bool force )
 *  @param  force   Force flag (optional)
 *  @return error code
 *
 *  Reset device. This brings a device from online state to ready state, if
 *  all goes well.
 *
 *  This function is a blocking function. If this is not an acceptable
 *  behavior for the caller's application, we recommend to run the device
 *  in a separate thread.
 *
 *  The caller should call the getDeviceState() function to observe the
 *  device state transitions until the final MicDevice::eReady state has
 *  been reached.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, the following error will be returned:
 *  - MICSDKERR_SHARED_LIBRARY_ERROR
 */

uint32_t  Mpss4StackLinux::deviceReset( bool force )
{
    (void) force;

    if(!m_pData->mpReset)
        return MicDeviceError::errorCode( MICSDKERR_SHARED_LIBRARY_ERROR );

    if(MicDeviceError::isError( m_pData->mpReset( deviceNumber() ) ))     // Blocking call
    {
        return MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );
    }

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  Mpss4StackLinux::deviceBoot( const MicBootConfigInfo& info )
 *  @param  info    Boot parameters
 *  @return error code
 *
 *  Boot device using specified boot parameter \a info. This brings the device
 *  from ready state to online state, if all goes well.
 *
 *  This function is non-blocking and will return immediately when the boot
 *  has been accepted by the hardware device. The caller should call the
 *  getDeviceState() function to observe the device state transitions.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_INVALID_DEVICE_NUMBER
 *  - MICSDKERR_SHARED_LIBRARY_ERROR
 *  - MICSDKERR_NO_ACCESS
 */

uint32_t  Mpss4StackLinux::deviceBoot( const MicBootConfigInfo& info )
{
    return deviceBoot<MicBootConfigInfo,
                      KnlCustomBootManager,
                      micmgmt::isAdministrator> (info);
}



//----------------------------------------------------------------------------
/** @fn     uint32_t  Mpss4StackLinux::setLoader( LoaderInterface* loader )
 *  @param  loader  A heap-allocated object implementing the LoaderInterface
 *  @return error code
 *
 *  Set \a loader as the new LoaderInterface object to perform library symbol lookup.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 */

uint32_t Mpss4StackLinux::setLoader( LoaderInterface *loader )
{
    if( !loader )
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );
    m_pLoader.reset( loader );
    return MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}



//============================================================================
//  P R I V A T E   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     uint32_t  Mpss4StackLinux::getDeviceProperty( std::string* data, const std::string& name, const std::string& subkey, char sep ) const
 *  @param  data    Data return
 *  @param  name    Property name
 *  @param  subkey  Subkey
 *  @param  sep     Name/value separator character (optional)
 *  @return error code
 *
 *  Get device property with given \a name for device with specified \a device
 *  number and \a subkey. The property data is returned in \a data.
 *
 *  Optionally, the character used as separator between key and value can be
 *  specified. The default character is '='.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_INVALID_DEVICE_NUMBER
 *  - MICSDKERR_NO_ACCESS
 *  - MICSDKERR_PROPERTY_NOT_FOUND
 *  - MICSDKERR_INTERNAL_ERROR
 */

uint32_t  Mpss4StackLinux::getDeviceProperty( std::string* data, const std::string& name, const std::string& subkey, char sep ) const
{
    std::ifstream ifs;
    return getDeviceProperty(data, name, subkey, ifs, sep);
}


//----------------------------------------------------------------------------
/** @fn     bool  Mpss4StackLinux::loadLib()
 *  @return success state
 *
 *  Load shared library, resolve symbols and return success state.
 */

bool  Mpss4StackLinux::loadLib()
{
    if (!m_pLoader->isLoaded())
    {
        m_pLoader->setFileName( MPSSLIB_SHLIB_NAME );
        m_pLoader->setVersion( 0 ); // TODO: Double check this in next releases
        if (!m_pLoader->load())
            return  false;
    }

    /// \todo In case the LIBMPSS team also comes to the conclusion that having a
    ///       library versioning scheme is a good thing, we will have to add code here
    ///       to check the compile library version against the runtime version.

    m_pData->mpBootLinux = (PrivData::boot_linux) m_pLoader->lookup( MPSSLIB_FNAME_BOOT_LINUX );
    m_pData->mpBootMedia = (PrivData::boot_media) m_pLoader->lookup( MPSSLIB_FNAME_BOOT_MEDIA );
    m_pData->mpGetState  = (PrivData::get_state)  m_pLoader->lookup( MPSSLIB_FNAME_GET_STATE );
    m_pData->mpReset     = (PrivData::reset_node) m_pLoader->lookup( MPSSLIB_FNAME_RESET );

    if (!m_pData->mpBootLinux || !m_pData->mpBootMedia || !m_pData->mpGetState || !m_pData->mpReset)
        return  false;

    return  true;
}


//----------------------------------------------------------------------------
/** @fn     void  Mpss4StackLinux::unloadLib()
 *
 *  Unload the shared library.
 *
 *  This function will most likely not really unload the shared library, but
 *  only decrease the shared library's reference count.
 */

void  Mpss4StackLinux::unloadLib()
{
    if (m_pLoader->isLoaded())
        m_pLoader->unload();
}
