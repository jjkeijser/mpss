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
#include    "SharedLibrary.hpp"

// SYSTEM INCLUDES
//
#if defined( __linux__ )
#  include  <dlfcn.h>
#  define  SHLIB_HANDLE   void*
#  define  SHLIB_INVALID  NULL
#elif defined( _WIN32 )
#  include  <Windows.h>
#  define  SHLIB_HANDLE   HINSTANCE
#  define  SHLIB_INVALID  NULL
#else
#  error  "*** UNSUPPORTED OS ***"
#endif
#include    <sstream>

// NAMESPACE
//
using namespace  micmgmt;
using namespace  std;

// PRIVATE DATA
//
struct  SharedLibrary::PrivData
{
    std::string   mFileName;
    std::string   mErrorStr;
    SHLIB_HANDLE  mHandle;
    int           mMajor;
    int           mMinor;
    int           mPatch;
};


//============================================================================
/** @class    micmgmt::SharedLibrary   SharedLibrary.hpp
 *  @ingroup  common
 *  @brief    This class encapsulates a shared library
 *
 *  The \b %SharedLibrary class encapsulates a shared library in a platform
 *  independent way.
 *
 *  The %SharedLibrary class allows loading of a shared library on demand in
 *  contrast with the shared library being loaded when the application is
 *  started.
 *
 *  A major advantage of accessing shared libraries through this class is that
 *  no library dependencies exist at build time. All function names are being
 *  resolved at runtime.
 *
 *  To use the %SharedLibrary class, simply create an instance of the class
 *  and specify the name of the library. Please note that the name should not
 *  include a path, nor an extension. The main reason for this is to maintain
 *  platform independency.
 */


//============================================================================
//  P U B L I C   C O N S T A N T S
//============================================================================

//----------------------------------------------------------------------------
/** @enum   micmgmt::SharedLibrary::LoadMode
 *
 *  Enumerations of load modes.
 */

/** @var    SharedLibrary::LoadMode  SharedLibrary::eLoadNow
 *
 *  Load the shared library and resolve all symbols.
 */

/** @var    SharedLibrary::LoadMode  SharedLibrary::eLoadLazy
 *
 *  Load the shared library but do not resolve the symbols as long as they
 *  are not used.
 */


//============================================================================
//  P U B L I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     SharedLibrary::SharedLibrary()
 *
 *  Construct an empty shared library object.
 */

SharedLibrary::SharedLibrary() :
    m_pData( new PrivData )
{
    m_pData->mHandle = SHLIB_INVALID;
    m_pData->mMajor  = -1;
    m_pData->mMinor  = -1;
    m_pData->mPatch  = -1;
}


//----------------------------------------------------------------------------
/** @fn     SharedLibrary::SharedLibrary( const std::string& name )
 *  @param  name    File name
 *
 *  Construct a shared library with specified file \a name.
 */

SharedLibrary::SharedLibrary( const std::string& name ) :
    m_pData( new PrivData )
{
    m_pData->mFileName = name;
    m_pData->mHandle   = SHLIB_INVALID;
    m_pData->mMajor    = -1;
    m_pData->mMinor    = -1;
    m_pData->mPatch    = -1;
}


//----------------------------------------------------------------------------
/** @fn     SharedLibrary::~SharedLibrary()
 *
 *  Cleanup.
 */

SharedLibrary::~SharedLibrary()
{
//FIXME: Currently don't unload library on windows due to it causes a hang
//Given that the unload is done in the destruction, this is done by the OS anyway.
#if defined( __linux__ )
    if (isLoaded())
        unload();
#endif
}


//----------------------------------------------------------------------------
/** @fn     bool  SharedLibrary::isLoaded() const
 *  @return loaded state
 *
 *  Returns \c true if the shared libary is loaded.
 */

bool  SharedLibrary::isLoaded() const
{
    return  (m_pData->mHandle != SHLIB_INVALID);
}


//----------------------------------------------------------------------------
/** @fn     std::string  SharedLibrary::fileName() const
 *  @return file name
 *
 *  Returns the shared library file name.
 */

std::string  SharedLibrary::fileName() const
{
    return  m_pData->mFileName;
}


//----------------------------------------------------------------------------
/** @fn     std::string  SharedLibrary::errorText() const
 *  @return error text
 *
 *  Returns the string describing the current error condition. If no error
 *  condition exists, an empty string is returned.
 */

std::string  SharedLibrary::errorText() const
{
    return  m_pData->mErrorStr;
}


//----------------------------------------------------------------------------
/** @fn     std::string  SharedLibrary::version() const
 *  @return version string
 *
 *  Returns the library version as string. If no version information was
 *  defined, an empty string is returned.
 */

std::string  SharedLibrary::version() const
{
    stringstream  strm;

    for (;;)    // Fake loop
    {
        if (m_pData->mMajor < 0)
            break;

        strm << m_pData->mMajor;

        if (m_pData->mMinor < 0)
            break;

        strm << "." << m_pData->mMinor;

        if (m_pData->mPatch < 0)
            break;

        strm << "." << m_pData->mPatch;

        break;      // Bail out
    }

    return  strm.str();
}


//----------------------------------------------------------------------------
/** @fn     bool  SharedLibrary::load( int mode )
 *  @param  mode    Load mode (optional)
 *  @return success state
 *
 *  Load the shared library using specified \a mode.
 */

bool  SharedLibrary::load( int mode )
{
    if (isLoaded())
        return  true;

    if (m_pData->mFileName.empty())
    {
        m_pData->mErrorStr = "Missing shared library name";
        return  false;
    }

#if defined( __linux__ )

    std::string  path = m_pData->mFileName;
    path.append( ".so" );

    const string  postfix = version();
    if (!postfix.empty())
    {
        path.append( "." );
        path.append( postfix );
    }

    int  flags = (mode == eLoadLazy) ? RTLD_LAZY : RTLD_NOW;
    m_pData->mHandle = dlopen( path.c_str(), flags );
    if (!m_pData->mHandle)
    {
        m_pData->mErrorStr = dlerror();
        return  false;
    }

#elif defined( _WIN32 )

    (void) mode;

    std::string  path = m_pData->mFileName;
    path.append( ".dll" );

    m_pData->mHandle = LoadLibraryA( path.c_str() );
    if (m_pData->mHandle == NULL)
    {
        m_pData->mErrorStr = "Failed to load shared library";
        return  false;
    }

#endif

    return  true;
}


//----------------------------------------------------------------------------
/** @fn     bool  SharedLibrary::unload()
 *  @return success state
 *
 *  Unload the shared library.
 */

bool  SharedLibrary::unload()
{
    if (!isLoaded())
        return  true;

#if defined( __linux__ )

    if (dlclose( m_pData->mHandle ) != 0)
    {
        m_pData->mErrorStr = "Failed to close";
        return  false;
    }

    m_pData->mHandle = SHLIB_INVALID;

#elif defined( _WIN32 )

    FreeLibrary( m_pData->mHandle );
    m_pData->mHandle = SHLIB_INVALID;

#endif

    return  true;
}


//----------------------------------------------------------------------------
/** @fn     void*  SharedLibrary::lookup( const std::string& symbol )
 *  @param  symbol  Symbol name
 *  @return pointer to function
 *
 *  Lookup function with specified \a symbol name and return a pointer to it.
 *  If the symbol cannot be resolved, 0 is returned.
 */

void*  SharedLibrary::lookup( const std::string& symbol )
{
    void*  ret = 0;

    if (!isLoaded())
    {
        m_pData->mErrorStr = "Shared library not loaded";
        return  ret;
    }

    if (symbol.empty())
    {
        m_pData->mErrorStr = "Invalid symbol name";
        return  ret;
    }

#if defined( __linux__ )

    dlerror();      // Clear last error

    ret = dlsym( m_pData->mHandle, symbol.c_str() );
    char*  err = dlerror();
    if (err)
        m_pData->mErrorStr = err;

#elif defined( _WIN32 )

    ret = (void*) GetProcAddress( m_pData->mHandle, symbol.c_str() );
    if (ret == NULL)
        m_pData->mErrorStr = "Failed to resolve symbol";

#endif

    return  ret;
}


//----------------------------------------------------------------------------
/** @fn     void  SharedLibrary::setFileName( const std::string& name )
 *  @param  name    Library file name
 *
 *  Set the library file \a name.
 */

void  SharedLibrary::setFileName( const std::string& name )
{
    if (!isLoaded())
        m_pData->mFileName = name;
}


//----------------------------------------------------------------------------
/** @fn     void  SharedLibrary::setVersion( int major, int minor, int patch )
 *  @param  major   Major version component
 *  @param  minor   Minor version component (optional)
 *  @param  patch   Patch version component (optional)
 *
 *  Define a shared library version. Specified version information will be
 *  used to identify a specific version of a shared library.
 */

void  SharedLibrary::setVersion( int major, int minor, int patch )
{
    m_pData->mMajor = major;
    m_pData->mMinor = minor;
    m_pData->mPatch = (minor < 0) ? -1 : patch; // Just to be sure
}


//----------------------------------------------------------------------------

#undef  SHLIB_HANDLE
#undef  SHLIB_INVALID
