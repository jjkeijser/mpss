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

#ifndef MICMGMT_RASCONNECTION_HPP
#define MICMGMT_RASCONNECTION_HPP

// SYSTEM INCLUDES
//
#include    <sstream>

// SCIF PROTOCOL INCLUDES
//
#include    "mic/micras_api.h"

// PROJECT INCLUDES
//
#include    "MicDeviceError.hpp"
#include    "ScifConnectionBase.hpp"
#include    "ScifRequestInterface.hpp"

const int RAS_PORT = 100;

// NAMESPACE
//
namespace  micmgmt
{


//============================================================================
//  CLASS:  RasConnectionAbstract

template <class BaseClass>
class  RasConnectionAbstract : public BaseClass
{

public:

    explicit RasConnectionAbstract( int devnum ) : BaseClass( devnum )
    {
        BaseClass::setPortNum( RAS_PORT );
    }

   ~RasConnectionAbstract()
   {
   }

    uint32_t  request( ScifRequestInterface* request );


private: // DISABLE

    RasConnectionAbstract();
    RasConnectionAbstract( const RasConnectionAbstract& );
    RasConnectionAbstract&  operator = ( const RasConnectionAbstract& );

};

template <class BaseClass>
uint32_t RasConnectionAbstract<BaseClass>::request( ScifRequestInterface* request )
{
    if (!BaseClass::isOpen())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_OPEN );

    if (!request)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    if (!BaseClass::lock())
        return  MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );


    // Create and send RAS request
    struct mr_hdr  header;
    header.cmd   = static_cast<uint16_t>( request->command() );
    header.len   = 0;
    header.parm  = request->parameter();
    header.stamp = 0;
    header.spent = 0;

    if (BaseClass::send( (char*) &header, sizeof( header ) ) < 0)
    {
        BaseClass::unlock();
        std::stringstream  strm;
        strm << "scif_send: cmd 0x" << std::hex << header.cmd
             << ": Len 0x" << std::hex << sizeof( header );
        BaseClass::setErrorText( strm.str() );
        return  MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );
    }


    // Create and receive RAS response
    struct mr_hdr  response = { 0, 0, 0, 0, 0 };

    if (BaseClass::receive( (char*) &response, sizeof( response ) ) < 0)
    {
        BaseClass::unlock();
        std::stringstream  strm;
        strm << "scif_recv: cmd 0x" << std::hex << header.cmd
             << ": Len 0x" << std::hex << sizeof( header );
        BaseClass::setErrorText( strm.str() );
        return  MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );
    }


    // Analyze response
    if (response.cmd & MR_ERROR)
    {
        // We have a problem

        // Was request command valid?
        if ((response.cmd & MR_OP_MASK) != header.cmd)
        {
            BaseClass::unlock();
            std::stringstream  strm;
            strm << "scif_recv: Unexpected opcode 0x" << std::hex
                 << (response.cmd & MR_OP_MASK)
                 << ": Expected 0x" << std::hex << header.cmd;
            BaseClass::setErrorText( strm.str() );
            return  MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );
        }

        // Was error information sent?
        if (response.len == sizeof( struct mr_err ))
        {
            struct mr_err  merr;

            if (BaseClass::receive( (char*) &merr, sizeof( merr ) ) < 0)
            {
                BaseClass::unlock();
                std::stringstream  strm;
                strm << "scif_recv: cmd 0x" << std::hex << header.cmd
                     << ": Failed error record: Len 0x" << std::hex << sizeof( merr );
                BaseClass::setErrorText( strm.str() );
                return  MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );
            }

            // Error info available
            request->setError( merr.err );
            BaseClass::unlock();
            std::stringstream  strm;
            strm << "RAS: cmd 0x" << std::hex << header.cmd
                 << ": Error 0x" << std::hex << merr.err;
            BaseClass::setErrorText( strm.str() );
            return  MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );
        }

        // Was any unsolicited data sent?
        if (response.len > 0)
        {
            std::string  garbage( response.len, 0 );
            BaseClass::receive( (char*) garbage.c_str(), static_cast<int>( garbage.length() ) );
        }

        // Unknown error
        BaseClass::unlock();
        std::stringstream  strm;
        strm << "scif_recv: cmd 0x" << std::hex << header.cmd
             << ": Unknown error: Len 0x" << std::hex << response.len;
        BaseClass::setErrorText( strm.str() );
        return  MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );
    }

    // Did we get the corrent amount of data?
    if (response.len != request->byteCount())
    {
        BaseClass::unlock();
        std::stringstream  strm;
        strm << "scif_recv: cmd 0x" << std::hex << header.cmd
             << ": Response payload len 0x" << std::hex << response.len
             << ": Expected 0x" << std::hex << request->byteCount();
        BaseClass::setErrorText( strm.str() );
        return  MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );
    }


    // Receive response data
    if (BaseClass::receive( request->buffer(), response.len ) < 0)
    {
        BaseClass::unlock();
        std::stringstream  strm;
        strm << "scif_recv: cmd 0x" << std::hex << header.cmd << ": Response failed";
        BaseClass::setErrorText( strm.str() );
        return  MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );
    }

    BaseClass::unlock();

    request->clearError();

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}

typedef RasConnectionAbstract<ScifConnectionBase> RasConnection;

//----------------------------------------------------------------------------

}   // namespace micmgmt

//----------------------------------------------------------------------------

#endif // MICMGMT_RASCONNECTION_HPP
