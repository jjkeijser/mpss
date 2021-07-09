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
#include    "ScifRequest.hpp"

// NAMESPACES
//
using namespace  micmgmt;
using namespace  std;


//============================================================================
/** @class    micmgmt::ScifRequest  ScifRequest.hpp
 *  @ingroup  sdk
 *  @brief    The class encapsulates a SCIF request
 *
 *  The \b %ScifRequest class encapsulates a SCIF request in a platform
 *  independent way.
 *
 *  There are two basic forms of SCIF requests. The first form only sends a
 *  command to the receiver to instruct it to do or set something. The only
 *  response from the receiver is a success or an error report. The isValid()
 *  function will return \c true if the request was successful. The isError()
 *  will return \c true if an error occured. An error code may be available
 *  using errorCode().
 *
 *  The second request form represents a information query and will return
 *  data into a user specified data buffer. Also here, the operation can be
 *  successful or can fail.
 */


//============================================================================
//  P U B L I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     ScifRequest::ScifRequest( int command, uint32_t param )
 *  @param  command  Request command
 *  @param  param   Request parameter (optional)
 *
 *  Construct a command SCIF request for specified \a command and optional
 *  parameter \a param.
 *
 *  This represents a command request that does not return any data.
 */

ScifRequest::ScifRequest( int command, uint32_t param ) :
    m_command( command ),
    m_param( param ),
    m_pBuffer( 0 ),
    m_bytes( 0 ),
    m_error( -1 ),
    m_write( false )
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     ScifRequest::ScifRequest( int command, char* buffer, size_t bytes )
 *  @param  command  Request command
 *  @param  buffer   Pointer to return buffer
 *  @param  bytes    Expected data return byte count
 *
 *  Construct a query SCIF request for specified \a command. The specified
 *  number of \a bytes is expected to be returned in the given user \a buffer.
 */

ScifRequest::ScifRequest( int command, char* buffer, size_t bytes ) :
    m_command( command ),
    m_param( 0 ),
    m_pBuffer( buffer ),
    m_bytes( bytes ),
    m_error( -1 ),
    m_write( false )
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     ScifRequest::ScifRequest( int command, const char* buffer, size_t bytes )
 *  @param  command  Request command
 *  @param  buffer   Pointer to send data buffer
 *  @param  bytes    Send data byte count
 *
 *  Construct a send data SCIF request for specified \a command.
 *  The \a buffer parameter specifies a pointer to the buffer containing the
 *  data to be sent. Only the specified first \a bytes number of bytes from
 *  the data buffer is sent.
 */

ScifRequest::ScifRequest( int command, const char* buffer, size_t bytes ) :
    m_command( command ),
    m_param( 0 ),
    m_pBuffer( (char*) buffer ),
    m_bytes( bytes ),
    m_error( -1 ),
    m_write( true )
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     ScifRequest::ScifRequest( int command, uint32_t param, char* buffer, size_t bytes )
 *  @param  command  Request command
 *  @param  param    Command parameter
 *  @param  buffer   Pointer to return buffer
 *  @param  bytes    Expected data return byte count
 *
 *  Construct a query SCIF request for specified \a command. The specified
 *  number of \a bytes is expected to be returned in the given user \a buffer.
 */

ScifRequest::ScifRequest( int command, uint32_t param, char* buffer, size_t bytes ) :
    m_command( command ),
    m_param( param ),
    m_pBuffer( buffer ),
    m_bytes( bytes ),
    m_error( -1 ),
    m_write( false )
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     ScifRequest::~ScifRequest()
 *
 *  Cleanup.
 */

ScifRequest::~ScifRequest()
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     bool  ScifRequest::isValid() const
 *  @return valid state
 *
 *  Returns \c true if the request was valid.
 */

bool  ScifRequest::isValid() const
{
    return  (m_error == 0);
}


//----------------------------------------------------------------------------
/** @fn     bool  ScifRequest::isError() const
 *  @return error state
 *
 *  Returns \c true if the request failed.
 */

bool  ScifRequest::isError() const
{
    return  (m_error != 0);
}


//----------------------------------------------------------------------------
/** @fn     bool  ScifRequest::isSendRequest() const
 *  @return send request state
 *
 *  Returns \c true if the request represents a send data request.
 */

bool  ScifRequest::isSendRequest() const
{
    return  m_write;
}


//----------------------------------------------------------------------------
/** @fn     int  ScifRequest::command() const
 *  @return request command
 *
 *  Returns the request command.
 */

int  ScifRequest::command() const
{
    return  m_command;
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  ScifRequest::parameter() const
 *  @return packet parameter
 *
 *  Returns the request command parameter.
 */

uint32_t  ScifRequest::parameter() const
{
    return  m_param;
}


//----------------------------------------------------------------------------
/** @fn     char*  ScifRequest::buffer() const
 *  @return pointer to buffer
 *
 *  Returns a pointer to the response buffer.
 */

char*  ScifRequest::buffer() const
{
    return  m_pBuffer;
}


//----------------------------------------------------------------------------
/** @fn     size_t  ScifRequest::byteCount() const
 *  @return bytes count
 *
 *  Returns the number of bytes expected to receive in the response buffer.
 */

size_t  ScifRequest::byteCount() const
{
    return  m_bytes;
}


//----------------------------------------------------------------------------
/** @fn     int  ScifRequest::errorCode() const
 *  @return error code
 *
 *  Returns the received error code.
 */

int  ScifRequest::errorCode() const
{
    return  m_error;
}


//----------------------------------------------------------------------------
/** @fn     void  ScifRequest::clearError()
 *
 *  Clear error.
 */

void  ScifRequest::clearError()
{
    m_error = 0;
}


//----------------------------------------------------------------------------
/** @fn     void  ScifRequest::setError( int error )
 *  @param  error   Error code
 *
 *  Set the request error code.
 */

void  ScifRequest::setError( int error )
{
    m_error = error;
}


//----------------------------------------------------------------------------
/** @fn     void  ScifRequest::setSendRequest( bool state )
 *  @param  state   Send flag
 *
 *  Flag this request as send or receive data request.
 *
 *  You would normally not need to call this function, as the compiler would
 *  pick the correct constructor for send and receive requests, assuming that
 *  the definition of the data buffer provided to the constructor identifies
 *  a receiving data buffer or a sending data buffer (e.g. const versus non
 *  const).
 *
 *  In situations where the compiler would not be able to figure it out, use
 *  this function to flag the request as receive or send request.
 */

void  ScifRequest::setSendRequest( bool state )
{
    m_write = state;
}

