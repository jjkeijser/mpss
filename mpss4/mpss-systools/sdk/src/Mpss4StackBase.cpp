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
#include    "Mpss4StackBase.hpp"
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
const char* const  PROPVAL_DEVICE_MODE_NA        = "N/A";
const char* const  PROPVAL_DEVICE_MODE_LINUX     = "linux";
const char* const  PROPVAL_DEVICE_MODE_FLASH     = "flash";
#if defined( __linux__ )
const char* const  MPSS_VERSION_RPM_COMMAND      = "rpm -q --queryformat '%{version}.%{release}' mpss-release";
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
/** @class    micmgmt::Mpss4StackBase  Mpss4StackBase.hpp
 *  @ingroup  sdk
 *  @brief    The class represents a platform independent MPSS 4.x stack
 *
 *  The \b %Mpss4StackBase class represents a platform independent MPSS 4.x
 *  stack.
 */


//============================================================================
//  P U B L I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     Mpss4StackBase::~Mpss4StackBase()
 *
 *  Cleanup.
 */

Mpss4StackBase::~Mpss4StackBase()
{
    // Nothing to do
}

//============================================================================
//  P U B L I C   S T A T I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     std::string  Mpss4StackBase::mpssStackVersion()
 *  @return MPSS stack version
 *
 *  Returns the complete MPSS stack version.
 *
 *  If the version cannot be determined, an empty string is returned.
 */

std::string  Mpss4StackBase::mpssStackVersion()
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

        if (rbuf == data)
        {
            version = data;
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
/** @fn     Mpss4StackBase::Mpss4StackBase( int device )
 *  @param  device  Device number
 *
 *  Construct a MPSS 4.x stack base class object using given \a device number.
 */

Mpss4StackBase::Mpss4StackBase( int device ) :
    MpssStackBase( device )
{
    // Nothing to do
}


