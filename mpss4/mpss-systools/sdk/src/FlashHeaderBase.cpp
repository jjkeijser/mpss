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

#include    "FlashHeaderBase.hpp"
#include    "FlashImage.hpp"
#include    "MicDeviceImpl.hpp"

// NAMESPACE
//
using namespace  micmgmt;
using namespace  std;


//============================================================================
/** @class    micmgmt::FlashHeaderBase   FlashHeaderBase.hpp
 *  @ingroup  sdk
 *  @brief    This class encapsulates an abstract flash header
 *
 *  The \b %FlashHeaderBase class encapsulates an abstract flash image header
 *  and provides accessors to the relevant information.
 */


//============================================================================
//  P U B L I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     FlashHeaderBase::~FlashHeaderBase()
 *
 *  Cleanup.
 */

FlashHeaderBase::~FlashHeaderBase()
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     bool  FlashHeaderBase::isValid() const
 *  @return valid state
 *
 *  Returns \c true if the flash header is valid.
 */

bool  FlashHeaderBase::isValid() const
{
    return  m_valid;
}


//----------------------------------------------------------------------------
/** @fn     bool  FlashHeaderBase::isSigned() const
 *  @return signed state
 *
 *  Returns \c true if the image is signed.
 */

bool  FlashHeaderBase::isSigned() const
{
    return  m_signed;
}


//----------------------------------------------------------------------------
/** @fn     size_t  FlashHeaderBase::itemCount() const
 *  @return item count
 *
 *  Returns the number of flashable items found in the flash header.
 */

size_t  FlashHeaderBase::itemCount() const
{
    return  m_versions.size();
}


//----------------------------------------------------------------------------
/** @fn     FlashHeaderBase::ItemVersionMap  FlashHeaderBase::itemVersions() const
 *  @return flash item versions
 *
 *  Returns the flash item version map.
 */

FlashHeaderBase::ItemVersionMap  FlashHeaderBase::itemVersions() const
{
    return  m_versions;
}


//----------------------------------------------------------------------------
/** @fn     FlashHeaderBase::Platform  FlashHeaderBase::targetPlatform() const
 *  @return target
 *
 *  Returns the target.
 */

FlashHeaderBase::Platform  FlashHeaderBase::targetPlatform() const
{
    return  m_platform;
}


//----------------------------------------------------------------------------
/** @fn     bool  FlashHeaderBase::isCompatible( const MicDevice* device ) const
 *  @param  device  Pointer to a Mic device
 *  @return compatibility state
 *
 *  Verifies that the current flash imageis potentially compatible with
 *  specified Mic \a device. Returns \c true if the flash image seems
 *  compatible.
 *
 *  This virtual function may be reimplemented by deriving classes.
 *  The default implementation only checks for target platform compatibility.
 */

bool  FlashHeaderBase::isCompatible( const MicDevice* device ) const
{
    if (!device || !isValid())
        return  false;

    if (device->deviceType() == "x100")
        return  (targetPlatform() == eKncPlatform);

    if (device->deviceType() == "x200")
        return  (targetPlatform() == eKnlPlatform);

    return  false;
}

//----------------------------------------------------------------------------
/** @fn     std::vector<Files> FlashHeaderBase::getFiles() const
 *  @return a list of files found
 */

std::vector<DataFile> FlashHeaderBase::getFiles() const
{
    return m_pImage->getFiles();
}

//----------------------------------------------------------------------------
/** @fn     int FlashHeaderBase::getFileID(const std::string & line) const
 *  @return file id
 */

int FlashHeaderBase::getFileID(const std::string & line) const
{
    return m_pImage->getFileID(line);
}

//----------------------------------------------------------------------------
/** @fn     bool  FlashHeaderBase::processData()
 *  @return success state
 *
 *  Process header data and update internal data.
 *
 *  This pure virtual function must be implemented by deriving classes.
 */


//============================================================================
//  P R O T E C T E D   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     FlashHeaderBase::FlashHeaderBase( const FlashImage* image )
 *  @param  image   Pointer to parent flash image
 *
 *  Construct a flash header object for specified flash \a image.
 */

FlashHeaderBase::FlashHeaderBase( FATFileReaderBase & ptr, FlashImage* image ) :
    m_FatContents(ptr),
    m_pImage( image ),
    m_platform( eUnknownPlatform ),
    m_signed( false ),
    m_valid( false )
{
}


//----------------------------------------------------------------------------
/** @fn     const FlashImage*  FlashHeaderBase::image() const
 *  @return pointer to flash image
 *
 *  Returns a pointer to the parent flash image.
 */

FlashImage*  FlashHeaderBase::image()
{
    return  m_pImage;
}


//----------------------------------------------------------------------------
/** @fn     void  FlashHeaderBase::addItem( const string& name, const string& version )
 *  @param  name    Item name
 *  @param  version Item version
 *
 *  Add a flash item with given \a name and \a version.
 */

void  FlashHeaderBase::addItem( const string& name, const string& version )
{
    m_versions[name] = version;
}


//----------------------------------------------------------------------------
/** @fn     void  FlashHeaderBase::setTargetPlatform( Platform platform )
 *  @param  platform  Platform
 *
 *  Set the target \a platform for the flash image.
 */

void  FlashHeaderBase::setTargetPlatform( Platform platform )
{
    m_platform = platform;
}


//----------------------------------------------------------------------------
/** @fn     void  FlashHeaderBase::setSigned( bool state )
 *  @param  state   Signed state
 *
 *  Flag the image as signed or unsigned.
 */

void  FlashHeaderBase::setSigned( bool state )
{
    m_signed = state;
}


//----------------------------------------------------------------------------
/** @fn     void  FlashHeaderBase::setValid( bool state )
 *  @param  state   Valid state
 *
 *  Set the data valid \a state.
 */

void  FlashHeaderBase::setValid( bool state )
{
    m_valid = state;
}