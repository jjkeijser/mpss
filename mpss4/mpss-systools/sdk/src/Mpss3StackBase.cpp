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
#include    "Mpss3StackBase.hpp"
#include    "MicDeviceError.hpp"

// SYSTEM INCLUDES
//
#include    <string>
#if defined( __linux__ )
#  include  <cstring>
#  include  <climits>
#elif defined( _WIN32 )
#  include  <Windows.h>
#  include  <winreg.h>
#  ifndef  NAME_MAX
#    define  NAME_MAX  FILENAME_MAX
#  endif
#endif

// LOCAL CONSTANTS
//
namespace  {
const char* const  PROPKEY_DEVICE_MODE           = "mode";
const char* const  PROPVAL_DEVICE_STATE_OFFLINE  = "no response";
const char* const  PROPVAL_DEVICE_STATE_ONLINE   = "online";
const char* const  PROPVAL_DEVICE_STATE_READY    = "ready";
const char* const  PROPVAL_DEVICE_STATE_RESET    = "resetting";
const char* const  PROPVAL_DEVICE_STATE_BOOTING  = "booting";
const char* const  PROPVAL_DEVICE_STATE_LOST     = "lost";
const char* const  PROPVAL_DEVICE_STATE_SHUTDOWN = "shutdown";
const char* const  PROPVAL_DEVICE_STATE_NO_BOOT  = "boot failed";
const char* const  PROPVAL_DEVICE_STATE_NO_RESET = "reset failed";
const char* const  PROPVAL_DEVICE_STATE_INVALID  = "invalid";
const char* const  PROPKEY_DEVICE_STATE          = "state";
const char* const  PROPVAL_DEVICE_MODE_NA        = "N/A";
const char* const  PROPVAL_DEVICE_MODE_LINUX     = "linux";
const char* const  PROPVAL_DEVICE_MODE_ELF       = "elf";
const char* const  PROPVAL_DEVICE_MODE_FLASH     = "flash";
#if defined( __linux__ )
const char* const  MPSS_VERSION_RPM_COMMAND      = "rpm -qi mpss-daemon | grep -i version";
#elif defined( _WIN32 )
const char* const  MPSS_VERSION_PATH             = "SOFTWARE\\Intel\\Intel(R) Xeon Phi(TM) coprocessor\\tic";
const char* const  MPSS_VERSION_KEY              = "mpss_version";
#endif
}

// NAMESPACES
//
using namespace  micmgmt;
using namespace  std;


//============================================================================
/** @class    micmgmt::Mpss3StackBase  Mpss3StackBase.hpp
 *  @ingroup  sdk
 *  @brief    The class represents a platform independent MPSS 3.x stack
 *
 *  The \b %Mpss3StackBase class represents a platform independent MPSS 3.x
 *  stack.
 */


//============================================================================
//  P U B L I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     Mpss3StackBase::~Mpss3StackBase()
 *
 *  Cleanup.
 */

Mpss3StackBase::~Mpss3StackBase()
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  Mpss3StackBase::getDeviceMode( int* mode ) const
 *  @param  mode    Pointer to mode return variable
 *  @return error code
 *
 *  Retrieve and return the current device \a mode.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_INTERNAL_ERROR
 */

uint32_t  Mpss3StackBase::getDeviceMode( int* mode ) const
{
    if (!mode)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    *mode = eModeUnknown;

    string    statestr;
    uint32_t  result = getDeviceProperty( &statestr, PROPKEY_DEVICE_MODE );
    if (MicDeviceError::isError( result ))
        return  result;

    if (statestr == PROPVAL_DEVICE_MODE_NA)
        *mode = eModeUnknown;
    else if (statestr == PROPVAL_DEVICE_MODE_LINUX)
        *mode = eModeLinux;
    else if (statestr == PROPVAL_DEVICE_MODE_ELF)
        *mode = eModeElf;
    else if (statestr == PROPVAL_DEVICE_MODE_FLASH)
        *mode = eModeFlash;
    else
        return  MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  Mpss3StackBase::getDeviceState( int* state ) const
 *  @param  state   Pointer to state return variable
 *  @return error code
 *
 *  Retrieve and return the current device \a state.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_INTERNAL_ERROR
 */

uint32_t  Mpss3StackBase::getDeviceState( int* state ) const
{
    if (!state)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    *state = eStateInvalid;

    string    statestr;
    uint32_t  result = getDeviceProperty( &statestr, PROPKEY_DEVICE_STATE );
    if (MicDeviceError::isError( result ))
        return  result;

    if (statestr == PROPVAL_DEVICE_STATE_READY)
        *state = eStateReady;
    else if (statestr == PROPVAL_DEVICE_STATE_BOOTING)
        *state = eStateBooting;
    else if (statestr == PROPVAL_DEVICE_STATE_OFFLINE)
        *state = eStateNoResponse;
    else if (statestr == PROPVAL_DEVICE_STATE_NO_BOOT)
        *state = eStateBootFailed;
    else if (statestr == PROPVAL_DEVICE_STATE_ONLINE)
        *state = eStateOnline;
    else if (statestr == PROPVAL_DEVICE_STATE_SHUTDOWN)
        *state = eStateShutdown;
    else if (statestr == PROPVAL_DEVICE_STATE_LOST)
        *state = eStateNodeLost;
    else if (statestr == PROPVAL_DEVICE_STATE_RESET)
        *state = eStateResetting;
    else if (statestr == PROPVAL_DEVICE_STATE_NO_RESET)
        *state = eStateResetFailed;
    else if (statestr == PROPVAL_DEVICE_STATE_INVALID)
        *state = eStateInvalid;
    else
        return  MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//============================================================================
//  P U B L I C   S T A T I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     std::string  Mpss3StackBase::mpssStackVersion()
 *  @return MPSS stack version
 *
 *  Returns the complete MPSS stack version.
 *
 *  If the version cannot be determined, an empty string is returned.
 */

std::string  Mpss3StackBase::mpssStackVersion()
{
    string  version;

#if defined( __linux__ )

    // Open pipe, send data data request
    FILE*  fd = popen( MPSS_VERSION_RPM_COMMAND, "r" );
    if (fd)
    {
        // Read response
        char  data[NAME_MAX] = { 0 };
        char*  rbuf = fgets( data, NAME_MAX, fd );
        pclose( fd );

        // Parse response
        if (rbuf == data)
        {
            // Remove possible newlines
            char* last = data;
            auto data_len = strlen( data );
            if (data_len > 0)
                 last += data_len - 1;

            while ((last >= data) && (*last == '\n'))
            {
                *last = '\0';
                --last;
            }

            // Extract version
            char*  vstr = strtok( data, " " );
            vstr = strtok( NULL, " " );
            vstr = strtok( NULL, " " );

            if (vstr)
                version.assign( vstr );
        }
    }

#elif defined( _WIN32 )

    HKEY  key = NULL;
    LONG  ret = RegOpenKeyExA( HKEY_LOCAL_MACHINE, MPSS_VERSION_PATH, 0, KEY_READ, &key );
    if (ret == ERROR_SUCCESS)
    {
        DWORD  type;
        DWORD  size = NAME_MAX;
        char   data[NAME_MAX] = { 0 };

        ret = RegQueryValueExA( key, MPSS_VERSION_KEY, 0, &type, (LPBYTE) data, &size );
        if ((ret == ERROR_SUCCESS) && (type == REG_SZ))
        {
            version.assign( data, size );
        }

        RegCloseKey( key );
    }

#else
#  error  "*** UNSUPPORTED OS ***"
#endif

    return  version;
}


//============================================================================
//  P R O T E C T E D   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     Mpss3StackBase::Mpss3StackBase( int device )
 *  @param  device  Device number
 *
 *  Construct a MPSS 3.x stack base class object using given \a device number.
 */

Mpss3StackBase::Mpss3StackBase( int device ) :
    MpssStackBase( device )
{
    // Nothing to do
}

