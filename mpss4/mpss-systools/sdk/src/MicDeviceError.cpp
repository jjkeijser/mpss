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

//  PROJECT INCLUDES
//
#include "MicDeviceError.hpp"

//  LOCAL CONSTANTS
//
namespace  {
struct  ErrorMap
{
    micsdkerrno_t  number;
    const char*    text;
};

const ErrorMap  ERROR_MAP[] = {
    { MICSDKERR_SUCCESS,                "Success"                        },
    { MICSDKERR_INVALID_ARG,            "Invalid argument"               },
    { MICSDKERR_NO_MPSS_STACK,          "No MPSS stack found"            },
    { MICSDKERR_DRIVER_NOT_LOADED,      "Driver not loaded"              },
    { MICSDKERR_DRIVER_NOT_INITIALIZED, "Driver not initialized"         },
    { MICSDKERR_SYSTEM_NOT_INITIALIZED, "System not initialized"         },
    { MICSDKERR_NO_DEVICES,             "No devices available"           },
    { MICSDKERR_UNKNOWN_DEVICE_TYPE,    "Unknown device type"            },
    { MICSDKERR_DEVICE_OPEN_FAILED,     "Failed to open device"          },
    { MICSDKERR_INVALID_DEVICE_NUMBER,  "Invalid device number"          },
    { MICSDKERR_DEVICE_NOT_OPEN,        "Device not open"                },
    { MICSDKERR_DEVICE_NOT_ONLINE,      "Device not online"              },
    { MICSDKERR_DEVICE_IO_ERROR,        "Device I/O error"               },
    { MICSDKERR_PROPERTY_NOT_FOUND,     "Property not found"             },
    { MICSDKERR_INTERNAL_ERROR,         "Internal error"                 },
    { MICSDKERR_NOT_SUPPORTED,          "Operation not supported"        },
    { MICSDKERR_NO_ACCESS,              "No access rights"               },
    { MICSDKERR_FILE_IO_ERROR,          "File I/O error"                 },
    { MICSDKERR_DEVICE_BUSY,            "Device busy"                    },
    { MICSDKERR_NO_SUCH_DEVICE,         "No such device"                 },
    { MICSDKERR_DEVICE_NOT_READY,       "Device not ready"               },
    { MICSDKERR_INVALID_BOOT_IMAGE,     "Invalid boot image"             },
    { MICSDKERR_INVALID_FLASH_IMAGE,    "Invalid flash image"            },
    { MICSDKERR_SHARED_LIBRARY_ERROR,   "Shared library error"           },
    { MICSDKERR_BUFFER_TOO_SMALL,       "Buffer too small"               },
    { MICSDKERR_INVALID_SMC_REG_OFFSET, "Invalid SMC register offset"    },
    { MICSDKERR_SMC_OP_NOT_PERMITTED,   "SMC operation not permitted"    },
    { MICSDKERR_TIMEOUT,                "Operation timed out"            },
    { MICSDKERR_INVALID_CONFIGURATION,  "Invalid configuration detected" },
    { MICSDKERR_NO_MEMORY,              "Out of memory"                  },
    { MICSDKERR_DEVICE_ALREADY_OPEN,    "Device already open"            },
    { MICSDKERR_VERSION_MISMATCH,       "Version mismatch"               }
};

const int  ERROR_MAP_SIZE = sizeof(ERROR_MAP) / sizeof(*ERROR_MAP);

}

//  NAMESPACES
//
using namespace  micmgmt;
using namespace  std;


//============================================================================
/** @class    micmgmt::MicDeviceError      MicDeviceError.hpp
 *  @ingroup  sdk
 *  @brief    The class encapsulates error codes for MIC device classes.
 *
 *  The \b %MicDeviceError class encapsulates error codes for MIC device
 *  classes.
 *
 *  The %MicDeviceError class provides a set of convenience functions to
 *  convert and test error codes.
 *
 *  The error codes used here are 100% compatible with the error codes
 *  defined for the C programming interface.
 */


//============================================================================
//  P U B L I C   S T A T I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     int  MicDeviceError::error( uint32_t code )
 *  @param  code    Error code
 *  @return error number
 *
 *  Returns the error number corresponding to specified error \a code.
 */

int  MicDeviceError::error( uint32_t code )
{
    /// \todo Discuss this conversion
    return  (code & 0x000000ff);
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicDeviceError::errorCode( int error )
 *  @param  error   Error number
 *  @return error code
 *
 *  Returns the error code associated with specified \a error number.
 */

uint32_t  MicDeviceError::errorCode( int error )
{
    /// \todo Discuss this conversion
    return  static_cast<uint32_t>( error );
}


//----------------------------------------------------------------------------
/** @fn     const char*  MicDeviceError::errorText( int error )
 *  @param  error   Error number
 *  @return error string
 *
 *  Returns the error character string defining specified \a error number.
 */

const char*  MicDeviceError::errorText( int error )
{
    static const char* UNKNOWN_ERROR = "Unknown error";

    for (int i=0; i<ERROR_MAP_SIZE; i++)
    {
        if (ERROR_MAP[i].number == error)
            return  ERROR_MAP[i].text;
    }

    return  UNKNOWN_ERROR;
}


//----------------------------------------------------------------------------
/** @fn     const char*  MicDeviceError::errorText( uint32_t code )
 *  @param  code    Error code
 *  @return error string
 *
 *  Returns the error character string for specified error \a code.
 */

const char*  MicDeviceError::errorText( uint32_t code )
{
    return  errorText( error( code ) );
}


//----------------------------------------------------------------------------
/** @fn     bool  MicDeviceError::isSuccess( uint32_t code )
 *  @param  code    Error code
 *  @return success state
 *
 *  Returns \c true if specified error \a code describes a successful
 *  operation.
 */

bool  MicDeviceError::isSuccess( uint32_t code )
{
    return  ((code & 0xff) == 0);
}


//----------------------------------------------------------------------------
/** @fn     bool  MicDeviceError::isError( uint32_t code )
 *  @param  code    Error code
 *  @return error state
 *
 *  Returns \c true if specified error \a code describes an error condition.
 */

bool  MicDeviceError::isError( uint32_t code )
{
    return  ((code & 0xff) != 0);
}


//----------------------------------------------------------------------------
/** @fn     int  MicDeviceError::errorCodeCount()
 *  @return error code count
 *
 *  Returns the number of known and supported error codes.
 */

int  MicDeviceError::errorCodeCount()
{
    return  ERROR_MAP_SIZE;
}

