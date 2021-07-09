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

#ifndef MICMGMT_SYSTOOLSDCONNECTION_HPP
#define MICMGMT_SYSTOOLSDCONNECTION_HPP

// SYSTEM INCLUDES
//
#include <cstring>
#include <sstream>

// PROJECT INCLUDES
//
#include    "MicDeviceError.hpp"
#include    "ScifConnectionBase.hpp"
#include    "ScifRequestInterface.hpp"

// SCIF PROTOCOL INCLUDES
//
#include    "systoolsd_api.h"

#ifdef UNIT_TESTS
#define PRIVATE public
#else
#define PRIVATE private
#endif // UNIT_TESTS

// NAMESPACE
//
namespace  micmgmt
{

const int SYSTOOLSD_CONNECT_PORT = 130;

template <class BaseClass>
class SystoolsdConnectionAbstract : public BaseClass
{
public:
    explicit SystoolsdConnectionAbstract( int devnum ) :
        BaseClass( devnum )
    {
        BaseClass::setPortNum( SYSTOOLSD_CONNECT_PORT );
    }

    ~SystoolsdConnectionAbstract() { };

    // Definition provided below
    uint32_t request ( ScifRequestInterface *request );
    uint32_t smcRequest ( ScifRequestInterface *request );

private: // DISABLE

    SystoolsdConnectionAbstract();
    SystoolsdConnectionAbstract( const SystoolsdConnectionAbstract& );
    SystoolsdConnectionAbstract&  operator = ( const SystoolsdConnectionAbstract& );
};

typedef SystoolsdConnectionAbstract<ScifConnectionBase> SystoolsdConnection;



//============================================================================
//  BEGIN IMPLEMENTATION OF TEMPLATE METHODS
//============================================================================




template <class BaseClass>
uint32_t SystoolsdConnectionAbstract<BaseClass>::request ( ScifRequestInterface *request)
{
    if (!BaseClass::isOpen())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_OPEN );

    if (!request)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    if (request->isSendRequest() && (!request->buffer() || (request->byteCount() == 0)))
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    if (!BaseClass::lock())
        return  MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );

    SystoolsdReq  header;
    memset( &header, 0, sizeof( header ) );
    header.req_type = static_cast<uint16_t>( request->command() );
    if (request->parameter())
    {
        header.data[0] = (request->parameter() >>  0) & 0xff;
        header.data[1] = (request->parameter() >>  8) & 0xff;
        header.data[2] = (request->parameter() >> 16) & 0xff;
        header.data[3] = (request->parameter() >> 24) & 0xff;
    }

    if (BaseClass::send( (char*) &header, sizeof( header ) ) < 0)
    {
        BaseClass::unlock();
        std::stringstream  strm;
        strm << "scif_send: cmd 0x" << std::hex << header.req_type
             << ": Len 0x" << std::hex << sizeof( header );
        BaseClass::setErrorText( strm.str() );
        return  MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );
    }

    // Prepare receive response header
    SystoolsdReq  response;
    memset( &response, 0, sizeof( response ) );
    if (BaseClass::receive( (char*) &response, sizeof( response ) ) < 0)
    {
        BaseClass::unlock();
        std::stringstream  strm;
        strm << "scif_recv: cmd 0x" << std::hex << header.req_type
             << ": Len 0x" << std::hex << sizeof( header );
        BaseClass::setErrorText( strm.str() );
        return  MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );
    }

    if (response.card_errno)
    {
        // Was any unsolicited data sent?
        if (response.length > 0)
        {
            std::string  garbage( response.length, 0 );
            BaseClass::receive( (char*) garbage.c_str(), static_cast<int>( garbage.length() ) );
        }

        BaseClass::unlock();

        std::stringstream  strm;
        strm << "scif_recv: cmd 0x" << std::hex << header.req_type
             << ", Error 0x" << std::hex << response.card_errno;
        BaseClass::setErrorText( strm.str() );
        return  MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );
    }

    if (request->isSendRequest())
    {
        // Send request data
        if (BaseClass::send( request->buffer(), static_cast<int>( request->byteCount() ) ) < 0)
        {
            BaseClass::unlock();
            std::stringstream  strm;
            strm << "scif_send: cmd 0x" << std::hex << header.req_type
                 << ": Data Len 0x" << std::hex << request->byteCount();
            BaseClass::setErrorText( strm.str() );
            return  MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );
        }

        // Prepare receive response header
        memset( &response, 0, sizeof( response ) );
        if (BaseClass::receive( (char*) &response, sizeof( response ) ) < 0)
        {
            BaseClass::unlock();
            std::stringstream  strm;
            strm << "scif_recv: cmd 0x" << std::hex << header.req_type
                 << ": Data Len 0x" << std::hex << request->byteCount();
            BaseClass::setErrorText( strm.str() );
            return  MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );
        }

        if (response.card_errno)
        {
            // Was any unsolicited data sent?
            if (response.length > 0)
            {
                std::string  garbage( response.length, 0 );
                BaseClass::receive( (char*) garbage.c_str(), static_cast<int>( garbage.length() ) );
            }

            BaseClass::unlock();

            std::stringstream  strm;
            strm << "scif_recv: cmd 0x" << std::hex << header.req_type
                 << ", Data Error 0x" << std::hex << response.card_errno;
            BaseClass::setErrorText( strm.str() );
            return  MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );
        }
    }
    else
    {
        // Check return information
        if (response.length != request->byteCount())
        {
            BaseClass::unlock();
            std::stringstream  strm;
            strm << "scif_recv: cmd 0x" << std::hex << header.req_type
                 << ": Response payload len 0x" << std::hex << response.length
                 << ": Expected 0x" << std::hex << request->byteCount();
            BaseClass::setErrorText( strm.str() );
            return  MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );
        }

        if (request->byteCount() > 0)
        {
            // Receive response data
            if (BaseClass::receive( request->buffer(), response.length ) < 0)
            {
                BaseClass::unlock();
                std::stringstream  strm;
                strm << "scif_recv: cmd 0x" << std::hex << header.req_type << ": Response failed";
                BaseClass::setErrorText( strm.str() );
                return  MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );
            }
        }
    }

    BaseClass::unlock();

    request->clearError();

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}

template <class BaseClass>
uint32_t SystoolsdConnectionAbstract<BaseClass>::smcRequest ( ScifRequestInterface *request)
{
    if (!BaseClass::isOpen())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_OPEN );

    if (!request)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    if (!request->buffer() || request->byteCount() == 0)
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t command = request->command();
    bool isWriteOp = command == WRITE_SMC_REG;
    // Max length for write requests is SYSTOOLSDREQ_MAX_DATA_LENGTH (systoolsd_api.h)
    if (isWriteOp && request->byteCount() > SYSTOOLSDREQ_MAX_DATA_LENGTH)
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    // Only accept READ_SMC_REG or WRITE_SMC_REG commands
    if (command != READ_SMC_REG && command != WRITE_SMC_REG)
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    SystoolsdReq  header;
    memset( &header, 0, sizeof( header ) );
    header.req_type = static_cast<uint16_t>( request->command() );
    header.extra = static_cast<uint8_t>( request->parameter() );
    header.length = static_cast<uint16_t>( request->byteCount() );

    // Prepare data buffer to be sent
    if (isWriteOp)
    {
        memcpy( header.data, request->buffer(), request->byteCount() );
    }

    if (!BaseClass::lock())
        return  MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );

    if (BaseClass::send( (char*) &header, sizeof( header ) ) < 0)
    {
        BaseClass::unlock();
        std::stringstream  strm;
        strm << "scif_send: cmd 0x" << std::hex << header.req_type
             << ": Len 0x" << std::hex << sizeof( header );
        BaseClass::setErrorText( strm.str() );
        return  MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );
    }

    // Prepare receive response header
    SystoolsdReq  response;
    memset( &response, 0, sizeof( response ) );
    if (BaseClass::receive( (char*) &response, sizeof( response ) ) < 0)
    {
        BaseClass::unlock();
        std::stringstream  strm;
        strm << "scif_recv: cmd 0x" << std::hex << header.req_type
             << ": Len 0x" << std::hex << sizeof( header );
        BaseClass::setErrorText( strm.str() );
        return  MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );
    }

    if (response.card_errno)
    {
        BaseClass::unlock();

        std::stringstream  strm;
        strm << "scif_recv: cmd 0x" << std::hex << header.req_type
             << ", Error 0x" << std::hex << response.card_errno;
        BaseClass::setErrorText( strm.str() );
        return  MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );
    }

    // If this was a read request, copy the data to the user-provided buffer
    if (!isWriteOp)
    {
        // copy SYSTOOLSDREQ_MAX_DATA_LENGTH bytes at most
        size_t size = (request->byteCount() <= SYSTOOLSDREQ_MAX_DATA_LENGTH ?
                request->byteCount() : SYSTOOLSDREQ_MAX_DATA_LENGTH);
        memcpy( request->buffer(), response.data, size );
    }

    BaseClass::unlock();

    request->clearError();

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}

} // namespace micmgmt

//----------------------------------------------------------------------------

#endif // MICMGMT_SYSTOOLSDCONNECTION_HPP
