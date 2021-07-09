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
#include    "ScifConnectionBase.hpp"
#include    "MicDeviceError.hpp"
#include    "micmgmtCommon.hpp"
#include    "SharedLibraryAdapter.hpp"

#ifdef UNIT_TESTS
#include "ScifFunctions.hpp"
#endif // UNIT_TESTS

// SCIF INCLUDES
//
#include    "scif.h"

// SYSTEM INCLUDES
//
#include    <mutex>
#include    <sstream>

// LOCAL CONSTANTS
//
namespace  {
#if defined( __linux__ )
const char* const  SCIF_SHLIB_NAME    = "libscif";
const int          SCIF_SHLIB_MAJOR   = 0;
#elif defined( _WIN32 )
const char* const  SCIF_SHLIB_NAME    = "uSCIF";
const char* const  SCIF_FNAME_ERRNO   = "_dll_errno";
#endif
const char* const  SCIF_FNAME_OPEN    = "scif_open";
const char* const  SCIF_FNAME_CLOSE   = "scif_close";
const char* const  SCIF_FNAME_BIND    = "scif_bind";
const char* const  SCIF_FNAME_CONNECT = "scif_connect";
const char* const  SCIF_FNAME_SEND    = "scif_send";
const char* const  SCIF_FNAME_RECV    = "scif_recv";
const int          SCIF_CONNECT_PORT  = 100;
}

// PRIVATE DATA
//
namespace  micmgmt {
struct  ScifConnectionBase::PrivData
{
    typedef scif_epd_t (*scif_open) (void);
    typedef int        (*scif_close) (scif_epd_t epd);
    typedef int        (*scif_bind) (scif_epd_t epd, uint16_t pn);
    typedef int        (*scif_connect) (scif_epd_t epd, struct scif_portID *dst);
    typedef int        (*scif_send) (scif_epd_t epd, void *msg, int len, int flags);
    typedef int        (*scif_recv) (scif_epd_t epd, void *msg, int len, int flags);
#if defined( _WIN32 )
    typedef int        (*_dll_errno) (void);

    _dll_errno     mpDllErrno;
#endif
    scif_open      mpScifOpen;
    scif_close     mpScifClose;
    scif_bind      mpScifBind;
    scif_connect   mpScifConnect;
    scif_send      mpScifSend;
    scif_recv      mpScifRecv;
    int            mDevNum;
    int            mPortNum;
    scif_epd_t     mHandle;
    bool           mOnline;
    std::mutex     mMutex;
    std::string    mErrorText;
};
}

// NAMESPACES
//
using namespace  micmgmt;
using namespace  std;


//============================================================================
/** @class    micmgmt::ScifConnectionBase  ScifConnectionBase.hpp
 *  @ingroup  sdk
 *  @brief    The class represents an abstract SCIF connection
 *
 */


//============================================================================
//  P U B L I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     ScifConnectionBase::~ScifConnectionBase()
 *
 *  Cleanup.
 */

ScifConnectionBase::~ScifConnectionBase()
{
    close();
}


//----------------------------------------------------------------------------
/** @fn     bool  ScifConnectionBase::isOpen() const
 *  @return connection open state
 *
 *  Returns \c true if a connection was opened.
 *
 */

bool  ScifConnectionBase::isOpen() const
{
    return  ((m_pData->mHandle == 0) ? false : true);
}


//----------------------------------------------------------------------------
/** @fn     int  ScifConnectionBase::deviceNum() const
 *  @return device number
 *
 *  Returns the number of the device using this SCIF connection.
 */

int  ScifConnectionBase::deviceNum() const
{
    return  m_pData->mDevNum;
}


//----------------------------------------------------------------------------
/** @fn     std::string  ScifConnectionBase::errorText() const
 *  @return error text
 *
 *  Returns the text of the last error condition.
 */

std::string  ScifConnectionBase::errorText() const
{
    return  m_pData->mErrorText;
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  ScifConnectionBase::open()
 *  @return error code
 *
 *  Open SCIF connection and return success state.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_SHARED_LIBRARY_ERROR
 *  - MICSDKERR_DEVICE_IO_ERROR
 */

uint32_t  ScifConnectionBase::open()
{
    if (isOpen())
        return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    if (!loadLib())
        return  MicDeviceError::errorCode( MICSDKERR_SHARED_LIBRARY_ERROR );

    scif_epd_t  handle = m_pData->mpScifOpen();
    if (handle < 0)
    {
        setErrorText( "SCIF open failed.", systemErrno() );
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_IO_ERROR );
    }

    int  port = -1;
    if (micmgmt::isAdministrator())
    {
        for ( uint16_t p=SCIF_ADMIN_PORT_END-1; p>0; p--)
        {
            port = m_pData->mpScifBind( handle, p );
            if (port == p)
                break;
        }

        if (port < 0)
        {
            setErrorText( "SCIF bind failed.", systemErrno() );
            m_pData->mpScifClose( handle );
            return  MicDeviceError::errorCode( MICSDKERR_DEVICE_IO_ERROR );
        }
    }
    else    // Non-admin user
    {
        port = m_pData->mpScifBind( handle, 0 );
        if (port < 0)
        {
            setErrorText( "SCIF bind failed.", systemErrno() );
            m_pData->mpScifClose( handle );
            return  MicDeviceError::errorCode( MICSDKERR_DEVICE_IO_ERROR );
        }
    }

    struct scif_portID  peer = { 0, 0 };
    peer.node = static_cast<uint16_t>( deviceNum() + 1 );
    peer.port = static_cast<uint16_t>( portNum() );

    if (m_pData->mpScifConnect( handle, &peer ) < 0)
    {
        int  errnum = systemErrno();
        if (errnum == ECONNREFUSED)
            setErrorText( "SCIF connection refused.", errnum );
        else
            setErrorText( "SCIF connection failed.", errnum );

        m_pData->mpScifClose( handle );

        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_IO_ERROR );
    }

    m_pData->mHandle = handle;
    m_pData->mOnline = true;

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     void  ScifConnectionBase::close()
 *
 *  Close SCIF connection.
 *
 *  This pure virtual function must be implemented in deriving classes.
 */

void  ScifConnectionBase::close()
{
    if (isOpen() && m_pData->mpScifClose)
    {
        m_pData->mpScifClose( m_pData->mHandle );
        m_pData->mHandle = 0;
        m_pData->mOnline = false;
    }
}


//----------------------------------------------------------------------------
/** @fn     bool  ScifConnectionBase::request( ScifRequest* request )
 *  @param  request  Pointer to request
 *  @return success state
 *
 *  Initiate specified SCIF \a request and return success state.
 *
 *  This pure virtual function must be implemented in deriving classes.
 */


//============================================================================
//  P R O T E C T E D   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     ScifConnectionBase::ScifConnectionBase( int devnum )
 *  @param  devnum  Device number
 *
 *  Construct a SCIF connection object for specified device number \a devnum.
 */

ScifConnectionBase::ScifConnectionBase( int devnum ) :
    m_pData( new PrivData ), m_loader( new SharedLibraryAdapter() )
{
#if defined( _WIN32 )
    m_pData->mpDllErrno    = 0;
#endif
    m_pData->mpScifOpen    = 0;
    m_pData->mpScifClose   = 0;
    m_pData->mpScifBind    = 0;
    m_pData->mpScifConnect = 0;
    m_pData->mpScifSend    = 0;
    m_pData->mpScifRecv    = 0;

    m_pData->mDevNum  = devnum;
    m_pData->mPortNum = SCIF_CONNECT_PORT;
    m_pData->mHandle  = 0;
    m_pData->mOnline  = false;
}

#ifdef UNIT_TESTS
ScifConnectionBase::ScifConnectionBase( int devnum, int portnum, int handle, bool online,
        std::string errorText, ScifFunctions& functions ) : m_pData( new PrivData ),
        m_loader( new SharedLibraryAdapter() )
{
    m_pData->mpScifOpen    = functions.open;
    m_pData->mpScifClose   = functions.close;
    m_pData->mpScifBind    = functions.bind;
    m_pData->mpScifConnect = functions.connect;
    m_pData->mpScifSend    = functions.send;
    m_pData->mpScifRecv    = functions.recv;

    m_pData->mDevNum = devnum;
    m_pData->mPortNum = portnum;
    m_pData->mHandle = handle;
    m_pData->mOnline = online;
    m_pData->mErrorText = errorText;
}
#endif // UNIT_TESTS


//----------------------------------------------------------------------------
/** @fn     int  ScifConnectionBase::portNum() const
 *  @return port number
 *
 *  Returns the port to connect to.
 */

int  ScifConnectionBase::portNum() const
{
    return  m_pData->mPortNum;
}


//----------------------------------------------------------------------------
/** @fn     bool  ScifConnectionBase::lock()
 *  @return success state
 *
 *  Lock the connection for exclusive undisturbed communication.
 */

bool  ScifConnectionBase::lock()
{
    m_pData->mMutex.lock();
    return  true;
}


//----------------------------------------------------------------------------
/** @fn     bool  ScifConnectionBase::unlock()
 *  @return success state
 *
 *  Unlock the connection from exclusive access.
 */

bool  ScifConnectionBase::unlock()
{
    m_pData->mMutex.unlock();
    return  true;
}


//----------------------------------------------------------------------------
/** @fn     int  ScifConnectionBase::send( const char* data, int len )
 *  @param  data    Pointer to data buffer
 *  @param  len     Data byte count
 *  @return success code
 *
 *  Send \a len bytes of data in specified \a data buffer to the receiving
 *  end of the connection and return success code.
 *
 *  On success, 0 is returned. On failure, -1 is returned.
 *
 *  This function blocks until all data was sent or an error occurred.
 */

int  ScifConnectionBase::send( const char* data, int len )
{
    char*  p = (char*) data;
    int    n = len;

    while (n > 0)
    {
        int  ret = m_pData->mpScifSend( m_pData->mHandle, (void*) p, n, SCIF_SEND_BLOCK );
        if ((ret < 0) && (systemErrno() == ECONNRESET))  // Lost connection?
        {
            close();
            if (open() == MicDeviceError::errorCode( MICSDKERR_SUCCESS ))
                ret = m_pData->mpScifSend( m_pData->mHandle, (void*) p, n, SCIF_SEND_BLOCK );
            else
                ret = -1;
        }

        if (ret < 0)
            return  -1;

        p += ret;     // Point to remaining data
        n -= ret;     // Remaining byte count
    }

    return  n;
}


//----------------------------------------------------------------------------
/** @fn     int  ScifConnectionBase::receive( char* data, int len )
 *  @param  data    Pointer to data return buffer
 *  @param  len     Expected byte count
 *  @return success code
 *
 *  Wait for \a len bytes of data to receive in specified \a data buffer.
 *
 *  On success, 0 is returned. On failure, -1 is returned.
 *
 *  This function blocks until all data was received or an error occurred.
 */

int  ScifConnectionBase::receive( char* data, int len )
{
    char*  p = data;
    int    n = len;

    while (n > 0)
    {
        int  ret = m_pData->mpScifRecv( m_pData->mHandle, (void*) p, n, SCIF_RECV_BLOCK );
        if (ret < 0)
            return  -1;

        p += ret;     // Point to remaining data
        n -= ret;     // Remaining byte count
    }

    return  n;
}


//----------------------------------------------------------------------------
/** @fn     void  ScifConnectionBase::setPortNum( int port )
 *  @param  port    Port number
 *
 *  Set the port number to connect to. A port number change should be made
 *  prior to an open() call.
 */

void  ScifConnectionBase::setPortNum( int port )
{
    m_pData->mPortNum = port;
}


//----------------------------------------------------------------------------
/** @fn     void  ScifConnectionBase::setErrorText( const string& text, int error )
 *  @param  text    Error text
 *  @param  error   Error number (optional)
 *
 *  Set the error text based on give \a text and optional system \a error
 *  code.
 */

void  ScifConnectionBase::setErrorText( const string& text, int error )
{
    stringstream  strm;     // C++11x not supported yet, so no to_string()
    strm << text;

    if (error)
        strm << " Errno=" << error;

    m_pData->mErrorText = strm.str();
}

//----------------------------------------------------------------------------
/** @fn     int  ScifConnectionBase::setLoader( LoaderInterface* loader)
 *  @param  loader  The loader object to set
 *
 *  Set the loader object to be used to load the SCIF library.
 */

int  ScifConnectionBase::setLoader( LoaderInterface* loader )
{
    if( !loader )
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );
    m_loader.reset( loader );
    return MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}

//============================================================================
//  P R I V A T E   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     int  ScifConnectionBase::systemErrno() const
 *  @return system errno
 *
 *  Returns the system error code errno for the last system call.
 */

int  ScifConnectionBase::systemErrno() const
{
#ifdef _WIN32
    if (m_pData->mpDllErrno)
        return  m_pData->mpDllErrno();
#endif

    return  errno;
}


//----------------------------------------------------------------------------
/** @fn     bool  ScifConnectionBase::loadLib()
 *  @return success state
 *
 *  Load SCIF shared library and try to resolve all function names.
 */

bool  ScifConnectionBase::loadLib()
{
    if (!m_loader->isLoaded())
    {
        m_loader->setFileName( SCIF_SHLIB_NAME );
#if defined( __linux__ )
        m_loader->setVersion( SCIF_SHLIB_MAJOR );
#endif
        if (!m_loader->load())
        {
            setErrorText( m_loader->errorText() );
            return  false;
        }
    }

    /// \todo In case the SCIF team also comes to the conclusion that having a library
    ///       versioning scheme is a good thing, we will have to add code here to check
    ///       the compile library version against the runtime version.

#if defined( _WIN32 )
    // In case this SCIF library function is available (prior MPSS 3.5), we use it, otherwise
    // we use the system errno.
    m_pData->mpDllErrno    = (PrivData::_dll_errno) m_loader->lookup( SCIF_FNAME_ERRNO );
#endif
    m_pData->mpScifOpen    = (PrivData::scif_open) m_loader->lookup( SCIF_FNAME_OPEN );
    m_pData->mpScifClose   = (PrivData::scif_close) m_loader->lookup( SCIF_FNAME_CLOSE );
    m_pData->mpScifBind    = (PrivData::scif_bind) m_loader->lookup( SCIF_FNAME_BIND );
    m_pData->mpScifConnect = (PrivData::scif_connect) m_loader->lookup( SCIF_FNAME_CONNECT );
    m_pData->mpScifSend    = (PrivData::scif_send) m_loader->lookup( SCIF_FNAME_SEND );
    m_pData->mpScifRecv    = (PrivData::scif_recv) m_loader->lookup( SCIF_FNAME_RECV );

    bool  complete = false;

    for (;;)
    {
        if (m_pData->mpScifOpen == 0)
            break;

        if (m_pData->mpScifClose == 0)
            break;

        if (m_pData->mpScifBind == 0)
            break;

        if (m_pData->mpScifConnect == 0)
            break;

        if (m_pData->mpScifSend == 0)
            break;

        if (m_pData->mpScifRecv == 0)
            break;

        complete = true;
        break;
    }

    if (!complete)
    {
        setErrorText( "Could not resolve all SCIF library symbols" );
        return  false;
    }

    return  true;
}


//----------------------------------------------------------------------------
/** @fn     void  ScifConnectionBase::unloadLib()
 *
 *  Unload the shared library.
 *
 *  This function will most likely not really unload the shared library, but
 *  only decrease the shared library's reference count.
 */

void  ScifConnectionBase::unloadLib()
{
    if (m_loader->isLoaded())
        m_loader->unload();
}
