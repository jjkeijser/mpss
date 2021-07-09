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
#include    "FlashImage.hpp"
#include    "KnlFlashHeader.hpp"
#include    "MicDeviceError.hpp"
#include    "micmgmtCommon.hpp"
#include    "FATFileReader.hpp"
#include    "FATMockFileReader.hpp"

// SYSTEM INCLUDES
//
#include    <cstring>
#include    <fstream>
#include    <sstream>

// LOCAL CONSTANTS
//
namespace {
const char* const  NUMERIC_CHARS = "0123456789";
const char* const  VERSION_SEP   = ".";
#ifdef  MICMGMT_MOCK_ENABLE
const char* const  KNC_MOCK_DEVICE_ENV = "KNC_MOCK_CNT";
const char* const  KNL_MOCK_DEVICE_ENV = "KNL_MOCK_CNT";
#endif

}

// PRIVATE DATA
//
namespace  micmgmt {
struct FlashImage::PrivData
{
    FlashHeaderBase*  mpHeader;
    char*             mpData;
    size_t            mSize;
    std::string       mPath;
    unsigned long     mPosA; // offset to file startup.nsh for SMC Fab A
    unsigned long     mPosB; // offset to file startup.nsh for SMC Fab B
    unsigned long     mPosX; // offset to file startup.nsh without SMC
    unsigned long     mPosT; // offset to file startup.nsh for first boot test
    FabImagesPaths    mPaths;
    FwToUpdate        mFwToUpdate;

    PrivData() : mpHeader(0), mpData(0), mSize(0), mPosA(0), mPosB(0),
                 mPosX(0), mPosT(0), mFwToUpdate() {}
   ~PrivData() { if (mpHeader) delete mpHeader; if (mpData) delete [] mpData; }
};

const std::string FlashImage::BIOS="BIOS";
const std::string FlashImage::ME="ME";
const std::string FlashImage::SMCFABA="SMC Fab A";
const std::string FlashImage::SMCFABB="SMC Fab B";

}
// NAMESPACE
//
using namespace  micmgmt;
using namespace  std;


//============================================================================
/** @class    micmgmt::FlashImage   FlashImage.hpp
 *  @ingroup  sdk
 *  @brief    This class encapsulates flash image
 *
 *  The \b %FlashImage class encapsulates flash image.
 */


//============================================================================
//  P U B L I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     FlashImage::FlashImage()
 *
 *  Construct an empty flash image object.
 *
 */

FlashImage::FlashImage() :
    m_pData( new PrivData )
{
    m_pFat.reset( new FATFileReader() );
    #ifdef  MICMGMT_MOCK_ENABLE
    if(!getEnvVar( KNL_MOCK_DEVICE_ENV ).second.empty())
        m_pFat.reset(new FATMockFileReader());
    #endif
    m_pData->mpHeader = new KnlFlashHeader( *m_pFat, this );
}


//----------------------------------------------------------------------------
/** @fn     FlashImage::~FlashImage()
 *
 *  Cleanup.
 */

FlashImage::~FlashImage()
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     bool  FlashImage::isValid() const
 *  @return valid state
 *
 *  Returns \c true if the flash image is valid.
 */

bool  FlashImage::isValid() const
{
    return  (flashHeader() ? flashHeader()->isValid() : false);
}


//----------------------------------------------------------------------------
/** @fn     bool  FlashImage::isSigned() const
 *  @return signed state
 *
 *  Returns \c true if the flash image is valid and signed.
 */

bool  FlashImage::isSigned() const
{
    return  (flashHeader() ? flashHeader()->isSigned() : false);
}


//----------------------------------------------------------------------------
/** @fn     size_t  FlashImage::itemCount() const
 *  @return flash item count
 *
 *  Returns the number of flashable items within the image.
 */

size_t  FlashImage::itemCount() const
{
    return  (flashHeader() ? flashHeader()->itemCount() : 0);
}


//----------------------------------------------------------------------------
/** @fn     FlashImage::ItemVersionMap  FlashImage::itemVersions() const
 *  @return flash item version map
 *
 *  Returns a map of item names and the corresponding version of all known
 *  flashable components in the flash image.
 */

FlashImage::ItemVersionMap  FlashImage::itemVersions() const
{
    return  (flashHeader() ? flashHeader()->itemVersions() : ItemVersionMap());
}


//----------------------------------------------------------------------------
/** @fn     FlashImage::Platform  FlashImage::targetPlatform() const
 *  @return target platform
 *
 *  Returns the target platform for this image.
 */

FlashImage::Platform  FlashImage::targetPlatform() const
{
    return  (flashHeader() ? flashHeader()->targetPlatform() : eUnknownPlatform);
}


//----------------------------------------------------------------------------
/** @fn     bool  FlashImage::isCompatible( const MicDevice* device ) const
 *  @param  device  Pointer to a Mic device
 *  @return compatibility state
 *
 *  Verifies that the current flash imageis potentially compatible with
 *  specified Mic \a device. Returns \c true if the flash image seems
 *  compatible.
 */

bool  FlashImage::isCompatible( const MicDevice* device ) const
{
    return  (flashHeader() ? flashHeader()->isCompatible( device ) : false);
}


//----------------------------------------------------------------------------
/** @fn     std::string  FlashImage::filePath() const
 *  @return flash file path
 *
 *  Returns the file path for this flash image.
 */

std::string  FlashImage::filePath() const
{
    return  m_pData->mPath;
}


//----------------------------------------------------------------------------
/** @fn     size_t  FlashImage::size() const
 *  @return flash image size
 *
 *  Returns the flash image size in bytes.
 */

size_t  FlashImage::size() const
{
    return  m_pData->mSize;
}


//----------------------------------------------------------------------------
/** @fn     const char*  FlashImage::data() const
 *  @return pointer to image data
 *
 *  Returns a pointer to the flash image data.
 */

const char*  FlashImage::data() const
{
    return  m_pData->mpData;
}


//----------------------------------------------------------------------------
/** @fn     void  FlashImage::clear()
 *
 *  Clear the flash image and free resources.
 */

void  FlashImage::clear()
{
    clearImage();
    //clearHeader();

    m_pData->mPath.clear();
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  FlashImage::setFilePath( const string& path )
 *  @param  path    File path
 *  @return error code
 *
 *  Set flash image file \a path.
 *
 *  This function will read the file header and determine the flash image
 *  type. Depending on type, it may load the complete file data as well.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_FILE_IO_ERROR
 *  - MICSDKERR_INTERNAL_ERROR
 *  - MICSDKERR_NOT_SUPPORTED
 */

uint32_t  FlashImage::setFilePath( const string& path )
{
    ifstream  strm( path, ios_base::in | ios_base::binary );
    if (strm.rdstate() & ifstream::failbit)
    {
        clear();        // Leave empty flash image behind
        return  MicDeviceError::errorCode( MICSDKERR_FILE_IO_ERROR );
    }
    // Determine file size
    strm.seekg( 0, ios_base::end );
    size_t  strmsize = static_cast<size_t>( strm.tellg() );
    strm.seekg( 0, ios_base::beg );
    if (strmsize == 0)
    {
        strm.close();
        clear();
        return  MicDeviceError::errorCode( MICSDKERR_FILE_IO_ERROR );
    }

    /// \todo Here we have to differentiate between KNC and KNL images and
    ///       analyze the file contents for type and version info and
    ///       populate the item version map.
    // Prepare data area if needed
    if (strmsize != size())
    {
        clear();
        m_pData->mpData = new char[strmsize];
        m_pData->mSize  = strmsize;
    }
    strm.read( m_pData->mpData, strmsize );
    bool  ok = strm.good();
    strm.close();

    if (!ok)
    {
        clear();
        return  MicDeviceError::errorCode( MICSDKERR_FILE_IO_ERROR );
    }
    m_pData->mPath = path;
    return  updateInfo();
}

void FlashImage::setFabPath(const string& path)
{
    m_pData->mPath = path;
}

std::vector<DataFile> FlashImage::getFiles() const
{
    return m_pData->mpHeader->getFiles();
}

int FlashImage::getFileID(const std::string & line) const
{
    return m_pData->mpHeader->getFileID(line);
}

FwToUpdate FlashImage::getFwToUpdate() const
{
    return m_pData->mFwToUpdate;
}

void FlashImage::setPosNameFabA(unsigned long pos)
{
    m_pData->mPosA = pos;
}

void FlashImage::setPosNameFabB(unsigned long pos)
{
    m_pData->mPosB = pos;
}

void FlashImage::setPosNameFabX(unsigned long pos)
{
    m_pData->mPosX = pos;
}

void FlashImage::setPosNameFabT(unsigned long pos)
{
    m_pData->mPosT = pos;
}

unsigned long FlashImage::getPosNameFabA() const
{
    return m_pData->mPosA;
}

unsigned long FlashImage::getPosNameFabB() const
{
    return m_pData->mPosB;
}

unsigned long FlashImage::getPosNameFabX() const
{
    return m_pData->mPosX;
}

unsigned long FlashImage::getPosNameFabT() const
{
    return m_pData->mPosT;
}

FabImagesPaths FlashImage::getFabImagesPaths() const
{
    return m_pData->mPaths;
}

void FlashImage::setBios()
{
    m_pData->mFwToUpdate.mBios = true;
}
void FlashImage::setSMC()
{
    m_pData->mFwToUpdate.mSMC = true;
}
void FlashImage::setME()
{
    m_pData->mFwToUpdate.mME = true;
}
void FlashImage::setBootTest()
{
    m_pData->mFwToUpdate.mBT = true;
}


//============================================================================
//  P R O T E C T E D   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     FlashHeaderBase*  FlashImage::flashHeader() const
 *  @return pointer to internal flash header
 *
 *  Returns a pointer to the internal flash header. Depending on the state
 *  of the flash image object, a header may not exist yet, in which case 0
 *  is returned.
 */

FlashHeaderBase*  FlashImage::flashHeader() const
{
    return  m_pData->mpHeader;
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  FlashImage::updateInfo()
 *  @return success state
 *
 *  Update the image information.
 *
 *  This virtual function may be reimplemented by deriving classes.
 *  The default implementation looks for a KNC compatible header first.
 *  If the header is not KNC compatible, it will check for KNL compatibility.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INTERNAL_ERROR
 *  - MICSDKERR_NOT_SUPPORTED
 */

uint32_t  FlashImage::updateInfo()
{
    if (!size() || !data())
        return  MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );

    if (m_pData->mpHeader->processData())
        return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    // Unsupported image

    return  MicDeviceError::errorCode( MICSDKERR_NOT_SUPPORTED );
}

//----------------------------------------------------------------------------
/** @fn     void  FlashImage::clearImage()
 *
 *  Clear image data.
 */

void  FlashImage::clearImage()
{
    if (m_pData->mpData)
        delete [] m_pData->mpData;

    m_pData->mpData = 0;
    m_pData->mSize  = 0;
}
