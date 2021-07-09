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
#include    "MicBootConfigInfo.hpp"

// PRIVATE DATA
//
namespace  micmgmt {
struct  MicBootConfigInfo::PrivData
{
    std::string  mImagePath;
    std::string  mInitRamFsPath;
    std::string  mSystemMapPath;
    bool         mCustom;
};
}

// NAMESPACE
//
using namespace  micmgmt;
using namespace  std;


//============================================================================
/** @class    micmgmt::MicBootConfigInfo   MicBootConfigInfo.hpp
 *  @ingroup  sdk
 *  @brief    This class encapsulates a boot configuration
 *
 *  The \b %MicBootConfigInfo class encapsulates a boot configuration.
 *
 *  The %MicBootConfigInfo is able to describe boot configurations in a
 *  device independent way and therefore can be used to describe a KNC boot
 *  configuration as well as a KNL boot configuration.
 */


//============================================================================
//  P U B L I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     MicBootConfigInfo::MicBootConfigInfo()
 *
 *  Construct an empty boot configuration object.
 */

MicBootConfigInfo::MicBootConfigInfo() :
    m_pData( new PrivData )
{
    m_pData->mCustom = false;
}


//----------------------------------------------------------------------------
/** @fn     MicBootConfigInfo::MicBootConfigInfo( const std::string& image, bool custom )
 *  @param  image   Bootable image path
 *  @param  custom  Custom image flag (optional)
 *
 *  Construct a boot configuration object for given bootable \a image path.
 */

MicBootConfigInfo::MicBootConfigInfo( const std::string& image, bool custom ) :
    m_pData( new PrivData )
{
    m_pData->mImagePath = image;
    m_pData->mCustom    = custom;
}


//----------------------------------------------------------------------------
/** @fn     MicBootConfigInfo::MicBootConfigInfo( const std::string& image, const std::string& initramfs, bool custom )
 *  @param  image      Bootable image path
 *  @param  initramfs  Initramfs path
 *  @param  custom     Custom image flag (optional)
 *
 *  Construct a boot configuration object using given bootable \a image path
 *  and \a initramfs.
 */

MicBootConfigInfo::MicBootConfigInfo( const std::string& image, const std::string& initramfs, bool custom ) :
    m_pData( new PrivData )
{
    m_pData->mImagePath     = image;
    m_pData->mInitRamFsPath = initramfs;
    m_pData->mCustom        = custom;
}


//----------------------------------------------------------------------------
/** @fn     MicBootConfigInfo::MicBootConfigInfo( const MicBootConfigInfo& that )
 *  @param  that    Other boot config object
 *
 *  Construct a boot config object as deep copy of \a that object.
 */

MicBootConfigInfo::MicBootConfigInfo( const MicBootConfigInfo& that ) :
    m_pData( new PrivData )
{
    m_pData->mImagePath     = that.m_pData->mImagePath;
    m_pData->mInitRamFsPath = that.m_pData->mInitRamFsPath;
    m_pData->mSystemMapPath = that.m_pData->mSystemMapPath;
    m_pData->mCustom        = that.m_pData->mCustom;
}


//----------------------------------------------------------------------------
/** @fn     MicBootConfigInfo::~MicBootConfigInfo()
 *
 *  Cleanup.
 */

MicBootConfigInfo::~MicBootConfigInfo()
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicBootConfigInfo&  MicBootConfigInfo::operator = ( const MicBootConfigInfo& that )
 *  @param  that    Other boot config object
 *  @return reference to this object
 *
 *  Assign \a that object to this object and return the updated object.
 */

MicBootConfigInfo&  MicBootConfigInfo::operator = ( const MicBootConfigInfo& that )
{
    if (&that != this)
    {
        m_pData->mImagePath     = that.m_pData->mImagePath;
        m_pData->mInitRamFsPath = that.m_pData->mInitRamFsPath;
        m_pData->mSystemMapPath = that.m_pData->mSystemMapPath;
        m_pData->mCustom        = that.m_pData->mCustom;
    }

    return  *this;
}


//----------------------------------------------------------------------------
/** @fn     bool  MicBootConfigInfo::isValid() const
 *  @return valid state
 *
 *  Returns \c true if the boot configuration seems is valid.
 */

bool  MicBootConfigInfo::isValid() const
{
    return  !m_pData->mImagePath.empty();
}


//----------------------------------------------------------------------------
/** @fn     bool  MicBootConfigInfo::isCustom() const
 *  @return custom image state
 *
 *  Returns \c true if the boot configuration contains a custom image.
 */

bool  MicBootConfigInfo::isCustom() const
{
    return  m_pData->mCustom;
}


//----------------------------------------------------------------------------
/** @fn     std::string  MicBootConfigInfo::imagePath() const
 *  @return bootable image path
 *
 *  Returns the full path of the bootable image.
 */

std::string  MicBootConfigInfo::imagePath() const
{
    return  m_pData->mImagePath;
}


//----------------------------------------------------------------------------
/** @fn     std::string  MicBootConfigInfo::initRamFsPath() const
 *  @return initramfs path
 *
 *  Returns the full path of the initramfs file.
 */

std::string  MicBootConfigInfo::initRamFsPath() const
{
    return  m_pData->mInitRamFsPath;
}


//----------------------------------------------------------------------------
/** @fn     std::string  MicBootConfigInfo::systemMapPath() const
 *  @return system map path
 *
 *  Returns the full path of the system map file.
 */

std::string  MicBootConfigInfo::systemMapPath() const
{
    return  m_pData->mSystemMapPath;
}


//----------------------------------------------------------------------------
/** @fn     void  MicBootConfigInfo::clear()
 *
 *  Clear boot configuration.
 */

void  MicBootConfigInfo::clear()
{
    m_pData->mImagePath.clear();
    m_pData->mInitRamFsPath.clear();
    m_pData->mSystemMapPath.clear();

    m_pData->mCustom = false;
}


//----------------------------------------------------------------------------
/** @fn     void  MicBootConfigInfo::setCustomState( bool custom )
 *  @param  custom  Custom image flag
 *
 *  Flag or unflag the boot image as \a custom image.
 */

void  MicBootConfigInfo::setCustomState( bool custom )
{
    m_pData->mCustom = custom;
}


//----------------------------------------------------------------------------
/** @fn     void  MicBootConfigInfo::setImagePath( const string& path )
 *  @param  path    Bootable image path
 *
 *  Set the \a path of the bootable image.
 */

void  MicBootConfigInfo::setImagePath( const string& path )
{
    m_pData->mImagePath = path;
}


//----------------------------------------------------------------------------
/** @fn     void  MicBootConfigInfo::setInitRamFsPath( const string& path )
 *  @param  path    Initramfs path
 *
 *  Set the \a path of the initramfs archive.
 */

void  MicBootConfigInfo::setInitRamFsPath( const string& path )
{
    m_pData->mInitRamFsPath = path;
}


//----------------------------------------------------------------------------
/** @fn     void  MicBootConfigInfo::setSystemMapPath( const string& path )
 *  @param  path    System map path
 *
 *  Set the \a path of the system map file.
 */

void  MicBootConfigInfo::setSystemMapPath( const string& path )
{
    m_pData->mSystemMapPath = path;
}


