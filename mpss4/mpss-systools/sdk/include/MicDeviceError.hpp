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

#ifndef MICMGMT_MICDEVICEERROR_HPP
#define MICMGMT_MICDEVICEERROR_HPP

//  PROJECT INCLUDES
//
#include    "micsdkerrno.h"

//  NAMESPACE
//
namespace micmgmt
{


//============================================================================
//  CLASS:  MicDeviceError

class  MicDeviceError
{

public:     // STATIC

    static int          error( uint32_t code );
    static uint32_t     errorCode( int error );
    static const char*  errorText( int error );
    static const char*  errorText( uint32_t code );
    static bool         isSuccess( uint32_t code );
    static bool         isError( uint32_t code );
    static int          errorCodeCount();


private:    // DISABLE

    MicDeviceError();
   ~MicDeviceError() {}

};

//----------------------------------------------------------------------------

} // namespace micmgmt

//----------------------------------------------------------------------------

#endif // MICMGMT_MICDEVICEERROR_HPP
