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

#ifndef MICMGMT_SCIFREQUEST_HPP
#define MICMGMT_SCIFREQUEST_HPP

// SYSTEM INCLUDES
//
#include    <cstddef>
#include    <cstdint>

// PROJECT INCLUDES
//
#include "ScifRequestInterface.hpp"

// NAMESPACE
//
namespace  micmgmt
{


//============================================================================
//  CLASS:  ScifRequest

class  ScifRequest : public ScifRequestInterface
{

public:

    explicit ScifRequest( int command, uint32_t param=0 );
    ScifRequest( int command, char* buffer, size_t bytes );
    ScifRequest( int command, const char* buffer, size_t bytes );
    ScifRequest( int command, uint32_t param, char* buffer, size_t bytes );
   ~ScifRequest();

    bool        isValid() const;
    bool        isError() const;
    bool        isSendRequest() const;
    int         command() const;
    uint32_t    parameter() const;
    char*       buffer() const;
    size_t      byteCount() const;
    int         errorCode() const;

    void        clearError();
    void        setError( int error );
    void        setSendRequest( bool state );


private:

    int       m_command;
    uint32_t  m_param;
    char*     m_pBuffer;
    size_t    m_bytes;
    int       m_error;
    bool      m_write;


private: // DISABLE

    ScifRequest();
    ScifRequest( const ScifRequest& );
    ScifRequest&  operator = ( const ScifRequest& );

};

//----------------------------------------------------------------------------

}   // namespace micmgmt

//----------------------------------------------------------------------------

#endif // MICMGMT_SCIFREQUEST_HPP
