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

#include <gtest/gtest.h>
#include "MicDeviceError.hpp"

namespace micmgmt
{
    using namespace std;

    void errorText_expect_eq( const char* literal, size_t error){
        EXPECT_EQ( literal, string( MicDeviceError::errorText( static_cast<int>(error) ) ) );
        EXPECT_EQ( literal, string( MicDeviceError::errorText( static_cast<uint32_t>(error) ) ) );
    }

    TEST(sdk, TC_KNL_mpsstools_MicDeviceError_001)
    {
        const size_t TOTAL_ERRORS = MicDeviceError::errorCodeCount();

        {   // Error code and bool tests
            for( uint32_t code = 0 ; code < (TOTAL_ERRORS +1) ; code++ ){
                EXPECT_EQ( static_cast<int>(code), MicDeviceError::error( code ) );
                EXPECT_EQ( code, MicDeviceError::errorCode( code ) );

                if( code == 0 ){
                    EXPECT_TRUE( MicDeviceError::isSuccess( code ) );
                    EXPECT_FALSE( MicDeviceError::isError( code ) );
                }
                else{
                    EXPECT_FALSE( MicDeviceError::isSuccess( code ) );
                    EXPECT_TRUE( MicDeviceError::isError( code ) );
                }
            }
        }

        {   // Error text tests
            errorText_expect_eq( "Success", MICSDKERR_SUCCESS );
            errorText_expect_eq( "Invalid argument", MICSDKERR_INVALID_ARG );
            errorText_expect_eq( "No MPSS stack found", MICSDKERR_NO_MPSS_STACK );
            errorText_expect_eq( "Driver not loaded", MICSDKERR_DRIVER_NOT_LOADED );
            errorText_expect_eq( "Driver not initialized", MICSDKERR_DRIVER_NOT_INITIALIZED );
            errorText_expect_eq( "System not initialized", MICSDKERR_SYSTEM_NOT_INITIALIZED );
            errorText_expect_eq( "No devices available", MICSDKERR_NO_DEVICES );
            errorText_expect_eq( "Unknown device type", MICSDKERR_UNKNOWN_DEVICE_TYPE );
            errorText_expect_eq( "Failed to open device", MICSDKERR_DEVICE_OPEN_FAILED );
            errorText_expect_eq( "Invalid device number", MICSDKERR_INVALID_DEVICE_NUMBER );
            errorText_expect_eq( "Device not open", MICSDKERR_DEVICE_NOT_OPEN );
            errorText_expect_eq( "Device not online", MICSDKERR_DEVICE_NOT_ONLINE );
            errorText_expect_eq( "Device I/O error", MICSDKERR_DEVICE_IO_ERROR );
            errorText_expect_eq( "Property not found", MICSDKERR_PROPERTY_NOT_FOUND );
            errorText_expect_eq( "Internal error", MICSDKERR_INTERNAL_ERROR );
            errorText_expect_eq( "Operation not supported", MICSDKERR_NOT_SUPPORTED );
            errorText_expect_eq( "No access rights", MICSDKERR_NO_ACCESS );
            errorText_expect_eq( "File I/O error", MICSDKERR_FILE_IO_ERROR );
            errorText_expect_eq( "Device busy", MICSDKERR_DEVICE_BUSY );
            errorText_expect_eq( "No such device", MICSDKERR_NO_SUCH_DEVICE );
            errorText_expect_eq( "Device not ready", MICSDKERR_DEVICE_NOT_READY );
            errorText_expect_eq( "Shared library error", MICSDKERR_SHARED_LIBRARY_ERROR );
            errorText_expect_eq( "Buffer too small", MICSDKERR_BUFFER_TOO_SMALL );
            errorText_expect_eq( "Invalid SMC register offset", MICSDKERR_INVALID_SMC_REG_OFFSET );
            errorText_expect_eq( "SMC operation not permitted", MICSDKERR_SMC_OP_NOT_PERMITTED );
            errorText_expect_eq( "Operation timed out", MICSDKERR_TIMEOUT );
            errorText_expect_eq( "Invalid configuration detected", MICSDKERR_INVALID_CONFIGURATION );
            errorText_expect_eq( "Out of memory", MICSDKERR_NO_MEMORY );
            errorText_expect_eq( "Device already open", MICSDKERR_DEVICE_ALREADY_OPEN );
            errorText_expect_eq( "Version mismatch", MICSDKERR_VERSION_MISMATCH );
        }

        {   // Error text negative tests
            errorText_expect_eq( "Unknown error", (TOTAL_ERRORS + 1));
        }

    } // sdk.TC_KNL_mpsstools_MicDeviceError_001

}   // namespace micmgmt
