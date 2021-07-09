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
#include    "MpssStackBase.hpp"
#include    "MicDeviceError.hpp"
#include    "MicBootConfigInfo.hpp"
#if defined( MICMGMT_MOCK_ENABLE )
#  include  "Mpss3StackMock.hpp"
#  include  "Mpss4StackMock.hpp"
#  define   CREATE_MPSS_STACK(vnum,dnum)  new Mpss##vnum##StackMock( dnum )
#elif defined( __linux__ )
#  include  <cstring>
#  include  <dirent.h>
#  include  "Mpss3StackLinux.hpp"
#  include  "Mpss4StackLinux.hpp"
#  define   CREATE_MPSS_STACK(vnum,dnum)  new Mpss##vnum##StackLinux( dnum )
#elif defined( _WIN32 )
#  include  <Windows.h>
#  include  <winreg.h>
#  ifndef  NAME_MAX
#    define  NAME_MAX  FILENAME_MAX
#  endif
#  include  "Mpss3StackWindows.hpp"
#  include  "Mpss4StackWindows.hpp"
#  define   CREATE_MPSS_STACK(vnum,dnum)  new Mpss##vnum##StackWindows( dnum )
#else
#  error  "*** UNSUPPORTED MPSS ENVIRONMENT ***"
#endif
#include    "micmgmtCommon.hpp"

// SYSTEM INCLUDES
//
#include    <sstream>
#include    <string>

// LOCAL CONSTANTS
//
namespace  {
const char* const  MPSS_MIC_DEV_BASENAME    = "mic";
const char* const  MPSS_STACK_3X_VERSION    = "3.";
const char* const  MPSS_STACK_4X_VERSION    = "4.";
#if defined( MICMGMT_MOCK_ENABLE )
const char* const  MPSS_MOCK_VERSION_ENV    = "MICMGMT_MPSS_MOCK_VERSION";
const char* const  MPSS_MOCK_STACK_VERSION  = "4.5.0 (mock)";
#endif
}

// NAMESPACES
//
using namespace  micmgmt;
using namespace  std;


//============================================================================
/** @class    micmgmt::MpssStackBase  MpssStackBase.hpp
 *  @ingroup  sdk
 *  @brief    The class represents an abstract MPSS stack base class
 *
 *  The \b %MpssStackBase class encapsulates an abstract MPSS stack.
 */


//============================================================================
//  P U B L I C   C O N S T A N T S
//============================================================================

//----------------------------------------------------------------------------
/** @enum   micmgmt::MpssStackBase::DeviceType
 *
 *  Enumerations of device types.
 */

/** @var    MpssStackBase::DeviceType  MpssStackBase::eTypeUnknown
 *
 *  Unknown device type.
 */

/** @var    MpssStackBase::DeviceType  MpssStackBase::eTypeKnc
 *
 *  KNC device type.
 */

/** @var    MpssStackBase::DeviceType  MpssStackBase::eTypeKnl
 *
 *  KNL device type.
 */


//----------------------------------------------------------------------------
/** @enum   micmgmt::MpssStackBase::DeviceMode
 *
 *  Enumerations of device modes.
 */

/** @var    MpssStackBase::DeviceMode  MpssStackBase::eModeUnknown
 *
 *  Unknown mode.
 */

/** @var    MpssStackBase::DeviceMode  MpssStackBase::eModeLinux
 *
 *  Linux mode.
 */

/** @var    MpssStackBase::DeviceMode  MpssStackBase::eModeElf
 *
 *  ELF mode.
 */

/** @var    MpssStackBase::DeviceMode  MpssStackBase::eModeFlash
 *
 *  Flash mode.
 */


//----------------------------------------------------------------------------
/** @enum   micmgmt::MpssStackBase::DeviceState
 *
 *  Enumerations of device states.
 */

/** @var    MpssStackBase::DeviceState  MpssStackBase::eStateReady
 *
 *  Ready state.
 */

/** @var    MpssStackBase::DeviceState  MpssStackBase::eStateBooting
 *
 *  Booting state.
 */

/** @var    MpssStackBase::DeviceState  MpssStackBase::eStateNoResponse
 *
 *  No response state (aka offline state).
 */

/** @var    MpssStackBase::DeviceState  MpssStackBase::eStateBootFailed
 *
 *  Boot failed state.
 */

/** @var    MpssStackBase::DeviceState  MpssStackBase::eStateOnline
 *
 *  Online state.
 */

/** @var    MpssStackBase::DeviceState  MpssStackBase::eStateShutdown
 *
 *  Shutdown state.
 */

/** @var    MpssStackBase::DeviceState  MpssStackBase::eStateNodeLost
 *
 *  Node lost state.
 */

/** @var    MpssStackBase::DeviceState  MpssStackBase::eStateResetting
 *
 *  Resetting state.
 */

/** @var    MpssStackBase::DeviceState  MpssStackBase::eStateResetFailed
 *
 *  Reset failed state.
 */

/** @var    MpssStackBase::DeviceState  MpssStackBase::eStateInvalid
 *
 *  Invalid state.
 */


//============================================================================
//  P U B L I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     MpssStackBase::~MpssStackBase()
 *
 *  Cleanup.
 */

MpssStackBase::~MpssStackBase()
{
    // Nothing to do (yet)
}


//----------------------------------------------------------------------------
/** @fn     std::string  MpssStackBase::deviceName() const
 *  @return device name
 *
 *  Returns the complete name of the associated device (e.g. mic3).
 *  If no associated device was defined, an empty string is returned.
 */

std::string  MpssStackBase::deviceName() const
{
    if (deviceNumber() < 0)
        return  string( "" );

    stringstream  strm;
    strm << deviceBaseName();
    strm << deviceNumber();

    return  strm.str();
}


//----------------------------------------------------------------------------
/** @fn     bool  MpssStackBase::isKncDevice() const
 *  @return KNC device type state
 *
 *  Returns \c true if the associated device is a KNC device.
 */

bool  MpssStackBase::isKncDevice() const
{
    int  type;
    if (MicDeviceError::isSuccess( getDeviceType( &type ) ) && (type == eTypeKnc))
        return  true;

    return  false;
}


//----------------------------------------------------------------------------
/** @fn     bool  MpssStackBase::isKnlDevice() const
 *  @return KNL device type state
 *
 *  Returns \c true if the associated device is a KNL device.
 */

bool  MpssStackBase::isKnlDevice() const
{
    int  type;
    if (MicDeviceError::isSuccess( getDeviceType( &type ) ) && (type == eTypeKnl))
        return  true;

    return  false;
}


//----------------------------------------------------------------------------
/** @fn     int  MpssStackBase::deviceNumber() const
 *  @return device number
 *
 *  Returns the number of the associated device. If no device was associated
 *  with this MPSS stack instance, -1 is returned.
 */

int  MpssStackBase::deviceNumber() const
{
    return  m_deviceNum;
}


//----------------------------------------------------------------------------
/** @fn     std::string  MpssStackBase::deviceBaseName() const
 *  @return device base name
 *
 *  Returns the device base name.
 *
 *  This virtual function may be reimplemented by deriving classes.
 *  The default implementation returns "mic".
 */

std::string  MpssStackBase::deviceBaseName() const
{
    return  string( MPSS_MIC_DEV_BASENAME );
}


//----------------------------------------------------------------------------
/** @fn     std::string  MpssStackBase::mpssHomePath() const
 *  @return home path
 *
 *  Returns the MPSS home directory path.
 *
 *  This pure virtual function must be implemented in deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     std::string  MpssStackBase::mpssBootPath() const
 *  @return boot path
 *
 *  Returns the MPSS boot image directory path.
 *
 *  This pure virtual function must be implemented in deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     std::string  MpssStackBase::mpssFlashPath() const
 *  @return flash path
 *
 *  Returns the MPSS flash image directory path.
 *
 *  This pure virtual function must be implemented in deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     uint32_t  MpssStackBase::getSystemMpssVersion( std::string* version ) const
 *  @param  version  Version string return
 *  @return error code
 *
 *  Determine MPSS stack version and return in specified \a version parameter.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_INTERNAL_ERROR
 */

uint32_t  MpssStackBase::getSystemMpssVersion( std::string* version ) const
{
    if (!version)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    int mpssStack;
    *version = mpssVersion(&mpssStack);

    if (version->empty())
        return  MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MpssStackBase::getSystemDriverVersion( std::string* version ) const
 *  @param  version  Version string return
 *  @return error code
 *
 *  Determine the device driver version and return in specified \a version
 *  parameter.
 *
 *  This pure virtual function must be implemented in deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     uint32_t  MpssStackBase::getSystemDeviceNumbers( std::vector<size_t>* list ) const
 *  @param  list    Pointer to device number list return
 *  @return error code
 *
 *  Create and return a \a list of detected device numbers in the system.
 *
 *  This pure virtual function must be implemented in deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     uint32_t  MpssStackBase::getSystemDeviceType( int* type, size_t number ) const
 *  @param  type    Device type return
 *  @param  number  Device number
 *  @return error code
 *
 *  Determine and return the device \a type for device with given \a number.
 *
 *  This pure virtual function must be implemented in deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     uint32_t  MpssStackBase::getSystemProperty( std::string* data, const std::string& name ) const
 *  @param  data    Data return
 *  @param  name    Property name
 *  @return error code
 *
 *  Get system property with given \a name and return property \a data.
 *
 *  Under Linux, device properties are stored in the SYSFS system. In case of
 *  MIC devices, the property is mapped to /sys/class/mic/\<name\>.
 *
 *  This pure virtual function must be implemented in deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     uint32_t  MpssStackBase::getDeviceType( int* type ) const
 *  @param  type    Pointer to device type return variable
 *  @return error code
 *
 *  Determine and return device \a type for associated device.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_INTERNAL_ERROR
 */

uint32_t  MpssStackBase::getDeviceType( int* type ) const
{
    if (!type)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    if (deviceNumber() < 0)
        return  MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );

    size_t  devnum = deviceNumber();

    return  getSystemDeviceType( type, devnum );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MpssStackBase::getDeviceMode( int* mode ) const
 *  @param  mode    Pointer to mode return variable
 *  @return error code
 *
 *  Retrieve and return the current device \a mode.
 *
 *  This pure virtual function must be implemented in deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     uint32_t  MpssStackBase::getDeviceState( int* state ) const
 *  @param  state   Pointer to state return variable
 *  @return error code
 *
 *  Retrieve and return the current device \a state.
 *
 *  This pure virtual function must be implemented in deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     uint32_t  MpssStackBase::getDevicePciConfigData( PciConfigData* data ) const
 *  @param  data    Pointer to PCI config data return
 *  @return error code
 *
 *  Determine and return PCI configuration for the associated device into
 *  specified PCI \a data structure.
 *
 *  This pure virtual function must be implemented in deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     uint32_t  MpssStackBase::getDeviceProperty( std::string* data, const std::string& name ) const
 *  @param  data    Data return
 *  @param  name    Property name
 *  @return error code
 *
 *  Get device property with given \a name and return property \a data.
 *
 *  This pure virtual function must be implemented in deriving classes.
 */


//----------------------------------------------------------------------------
/** @fn     uint32_t  MpssStackBase::getDeviceProperty( uint32_t* data, const std::string& name, int base ) const
 *  @param  data    Integer data return
 *  @param  name    Property name
 *  @param  base    Conversion base (optional)
 *  @return error code
 *
 *  Get device property with given \a name and return property as integer
 *  value in \a data. Optionally, a conversion \a base can be specified. The
 *  default base is 10. Specify 16 for hexadecimal conversion.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_INTERNAL_ERROR
 */

uint32_t  MpssStackBase::getDeviceProperty( uint32_t* data, const std::string& name, int base ) const
{
    if (!data || name.empty())
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    *data = 0;

    if ((base != 8) && (base != 10) && (base != 16))
        base = 10;

    string    sdata;
    uint32_t  result = getDeviceProperty( &sdata, name );
    if (MicDeviceError::isError( result ))
        return  result;

    if (sdata.empty())
        return  MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );

    std::stringstream  strm;

    // We already defaulted to decimal if base not in {8, 10, 16}
    if (base == 8)
        strm << std::oct;
    else if (base == 10)
        strm << std::dec;
    else
        strm << std::hex;

    strm << sdata;
    strm >> *data;

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MpssStackBase::getDeviceProperty( uint16_t* data, const std::string& name, int base ) const
 *  @param  data    Integer data return
 *  @param  name    Property name
 *  @param  base    Conversion base (optional)
 *  @return error code
 *
 *  Get device property with given \a name and return property as integer
 *  value in \a data. Optionally, a conversion \a base can be specified. The
 *  default base is 10. Specify 16 for hexadecimal conversion.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_INTERNAL_ERROR
 */

uint32_t  MpssStackBase::getDeviceProperty( uint16_t* data, const std::string& name, int base ) const
{
    uint32_t  value  = 0;
    uint32_t  result = getDeviceProperty( &value, name, base );
    if (MicDeviceError::isError( result ))
        return  result;

    *data = static_cast<uint16_t>( value );

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MpssStackBase::getDeviceProperty( uint8_t* data, const std::string& name, int base ) const
 *  @param  data    Integer data return
 *  @param  name    Property name
 *  @param  base    Conversion base (optional)
 *  @return error code
 *
 *  Get device property with given \a name and return property as integer
 *  value in \a data. Optionally, a conversion \a base can be specified. The
 *  default base is 10. Specify 16 for hexadecimal conversion.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_INTERNAL_ERROR
 */

uint32_t  MpssStackBase::getDeviceProperty( uint8_t* data, const std::string& name, int base ) const
{
    uint32_t  value  = 0;
    uint32_t  result = getDeviceProperty( &value, name, base );
    if (MicDeviceError::isError( result ))
        return  result;

    *data = static_cast<uint8_t>( value );

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MpssStackBase::deviceReset( bool force )
 *  @param  force   Force flag (optional)
 *  @return error code
 *
 *  Reset device. This brings a device from online state to ready state, if
 *  all goes well.
 *
 *  This function is semi-blocking that will return as soon as the reset
 *  has been accepted by the device driver. This, however, can take up to
 *  around a second for a device. Therefore, if this is not an acceptable
 *  behavior for the caller's application, we recommend to run the device
 *  in a separate thread.
 *
 *  The caller should call the getDeviceState() function to observe the
 *  device state transitions until the final MicDevice::eReady state has
 *  been reached.
 *
 *  This virtual function may be reimplemented by deriving classes.
 *  The default implementation does nothing and always returns not supported.
 *
 *  This function always returns MICSDKERR_NOT_SUPPORTED.
 */

uint32_t  MpssStackBase::deviceReset( bool force )
{
    // The default implementation does nothing and reports not supported
    (void) force;

    return  MicDeviceError::errorCode( MICSDKERR_NOT_SUPPORTED );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MpssStackBase::deviceBoot( const MicBootConfigInfo& info )
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
 *  This virtual function may be reimplemented by deriving classes.
 *  The default implementation does nothing and always returns not supported.
 *
 *  This function always returns MICSDKERR_NOT_SUPPORTED.
 */

uint32_t  MpssStackBase::deviceBoot( const MicBootConfigInfo& info )
{
    // The default implementation does nothing and reports not supported
    (void) info;

    return  MicDeviceError::errorCode( MICSDKERR_NOT_SUPPORTED );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MpssStackBase::setDeviceProperty( const string& name, const string& data )
 *  @param  name    Property name
 *  @param  data    Property data
 *  @return error code
 *
 *  Assign specified \a data to the property with given \a name.
 *
 *  This virtual function may be reimplemented by deriving classes.
 *  The default implementation does nothing and always returns not supported.
 *
 *  This function always returns MICSDKERR_NOT_SUPPORTED.
 */

uint32_t  MpssStackBase::setDeviceProperty( const string& name, const string& data )
{
    // The default implementation does nothing and reports not supported
    (void) name;
    (void) data;

    return  MicDeviceError::errorCode( MICSDKERR_NOT_SUPPORTED );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MpssStackBase::setDevicePowerStates( const std::list<MicPowerState>& states )
 *  @param  states  Power states
 *  @return error code
 *
 *  Set the device power states according the given list of power \a states.
 *
 *  This virtual function may be reimplemented by deriving classes.
 *  The default implementation does nothing and always returns not supported.
 *
 *  This function always returns MICSDKERR_NOT_SUPPORTED.
 */

uint32_t  MpssStackBase::setDevicePowerStates( const std::list<MicPowerState>& states )
{
    // The default implementation does nothing and reports not supported
    (void) states;

    return  MicDeviceError::errorCode( MICSDKERR_NOT_SUPPORTED );
}


//============================================================================
//  P U B L I C   S T A T I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     MpssStackBase*  MpssStackBase::createMpssStack( int device )
 *  @param  device  Associated device number (optional)
 *  @return pointer to MPSS stack object
 *
 *  Construct and return a pointer to a specialized MPSS stack.
 *  At construction time, a device can be associated with the stack by
 *  specifying the \a device number.
 *
 *  The current implementation decides which stack specialization type to
 *  create at runtime. The first distinction is between a real MPSS stack
 *  and a Mock MPSS stack.
 *
 *  A Mock MPSS stack will only be created when the SDK was compiled with
 *  the definition \c MICMGMT_MOCK_ENABLE. The Mock MPSS stack has no
 *  hardware or MPSS library dependencies. In case the SDK was compiled
 *  for Mock support, this function will create a Mock MPSS stack with the
 *  highest possible supported MPSS stack version. If a user desires to have
 *  this function create a Mock MPSS stack with a different version, it is
 *  possible to define a runtime environment variable that specifies the
 *  desired MPSS stack version.
 *
 *  To overwrite the default Mock MPSS stack versionto be created, define
 *  the runtime environment variable \c MICMGMT_MPSS_MOCK_VERSION and assign
 *  the desired version as string. The string should include a major and
 *  minor version number, separated by a dot. On a Linux system and depending
 *  the the shell used, this could be like:
 *
 *  \c export \c MICMGMT_MPSS_MOCK_VERSION="3.8"
 *
 *  The more typical case is that the SDK was not compiled for Mock MPSS
 *  support. In this case, the system is expected to have a MPSS stack
 *  installed. At runtime, this function determines what the version of the
 *  installed MPSS stack is and creates the correct instance of the MPSS
 *  stack support object.
 *
 *  If no MPSS stack object could be created, 0 is returned.
 *
 *  The caller is responisble for destroying the created MPSS stack object.
 */

MpssStackBase*  MpssStackBase::createMpssStack( int device )
{
    MpssStackBase*  stack = 0;

    int mpssStack;
    mpssVersion(&mpssStack);

    if (mpssStack == 4)
        stack = CREATE_MPSS_STACK( 4, device );
    else if (mpssStack == 3)
        stack = CREATE_MPSS_STACK( 3, device );

    return  stack;
}


//----------------------------------------------------------------------------
/** @fn     std::string  MpssStackBase::mpssVersion()
 *  @return MPSS stack version
 *
 *  Returns the complete MPSS stack version.
 *
 *  If the version cannot be determined, an empty string is returned.
 */

std::string  MpssStackBase::mpssVersion(int* mpssStack)
{
    string  version;
    *mpssStack = -1;

#if defined( MICMGMT_MOCK_ENABLE )

    std::pair<bool,std::string>  mockversion = getEnvVar( MPSS_MOCK_VERSION_ENV );
    version = mockversion.first ? mockversion.second : MPSS_MOCK_STACK_VERSION;
    *mpssStack = version.find(MPSS_STACK_3X_VERSION) == 0 ? 3 : 4;

#else

    version = Mpss4StackBase::mpssStackVersion();
    if (!version.empty())
        *mpssStack = 4;

    if (version.empty()) {
        version = Mpss3StackBase::mpssStackVersion();
        if (!version.empty())
            *mpssStack = 3;
    }

#endif

    return  version;
}


//----------------------------------------------------------------------------
/** @fn     bool  MpssStackBase::isMockStack()
 *  @return mocked MPSS stack state
 *
 *  Returns \c true if the MPSS stack is mocked. Otherwise, a real MPSS stack
 *  is used.
 */

bool  MpssStackBase::isMockStack()
{
#if defined( MICMGMT_MOCK_ENABLE )
    return  true;
#else
    return  false;
#endif
}


//============================================================================
//  P R O T E C T E D   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     MpssStackBase::MpssStackBase( int device )
 *  @param  device  Device number (optional)
 *
 *  Construct a MPSS stack object using specified \a device number.
 */

MpssStackBase::MpssStackBase( int device ) :
    m_deviceNum( device )
{
    // Nothing to do
}


//----------------------------------------------------------------------------

#ifdef  CREATE_MPSS_STACK
#  undef  CREATE_MPSS_STACK
#endif
