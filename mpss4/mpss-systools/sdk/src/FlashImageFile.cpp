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
#include    "FlashImageFile.hpp"

// SYSTEM INCLUDES
//
#include    <fstream>

// NAMESPACE
//
using namespace  micmgmt;
using namespace  std;


//============================================================================
/** @class    micmgmt::FlashImageFile   FlashImageFile.hpp
 *  @ingroup  sdk
 *  @brief    This class encapsulates flash image file
 *
 *  The \b %FlashImageFile class encapsulates flash image file.
 *
 *  %FlashImageFile derives from FlashImage and extends the interface for
 *  file path handling and file header handling.
 */


//============================================================================
//  P U B L I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     FlashImageFile::FlashImageFile()
 *
 *  Construct an empty flash image file object.
 */

FlashImageFile::FlashImageFile()
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     FlashImageFile::FlashImageFile( const std::string& path )
 *  @param  path    Path name
 *
 *  Construct a flash image file object with given flash image file \a path.
 */

FlashImageFile::FlashImageFile( const std::string& path ) :
    m_path( path )
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     FlashImageFile::~FlashImageFile()
 *
 *  Cleanup.
 */

FlashImageFile::~FlashImageFile()
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     std::string  FlashImageFile::pathName() const
 *  @return path name
 *
 *  Returns the path name associated with the flash image file.
 */

std::string  FlashImageFile::pathName() const
{
    return  m_path;
}


//----------------------------------------------------------------------------
/** @fn     void  FlashImageFile::setPathName( const string& path )
 *  @param  path    Path name
 *
 *  Set the \a path name.
 */

void  FlashImageFile::setPathName( const string& path )
{
    m_path = path;
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  FlashImageFile::load()
 *  @return error code
 *
 *  Loads the flash image from the flash image file with defined path name.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_FILE_IO_ERROR
 *  - MICSDKERR_INTERNAL_ERROR
 *  - MICSDKERR_NOT_SUPPORTED
 */

uint32_t  FlashImageFile::load()
{
    return  setFilePath( m_path );
}


