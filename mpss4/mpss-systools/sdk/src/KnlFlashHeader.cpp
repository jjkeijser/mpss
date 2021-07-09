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
#include    "KnlFlashHeader.hpp"
#include    "FlashImage.hpp"
#include    "FATFileReaderBase.hpp"
#include    "micmgmtCommon.hpp"
#include    "MicLogger.hpp"

//  SYSTEM INCLUDES
//
#include    <cstdint>
#include    <sstream>
#include    <vector>
#include    <algorithm>

// NAMESPACE
//
using namespace  micmgmt;
using namespace  std;

#include <iostream>

//============================================================================
/** @class    micmgmt::KnlFlashHeader   KnlFlashHeader.hpp
 *  @ingroup  sdk
 *  @brief    This class encapsulates a KNL flash header
 *
 *  The \b %KnlFlashHeader class encapsulates a KNL flash header and provides
 *  accessors to the relevant information.
 */


//============================================================================
//  P U B L I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     KnlFlashHeader::KnlFlashHeader( const FlashImage* image )
 *  @param  image   Pointer to parent image
 *
 *  Construct a flash image header objected using specified flash \a image.
 */

KnlFlashHeader::KnlFlashHeader( FATFileReaderBase & ptr, FlashImage* image ) :
    FlashHeaderBase( ptr, image )
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     KnlFlashHeader::~KnlFlashHeader()
 *
 *  Cleanup.
 */

KnlFlashHeader::~KnlFlashHeader()
{
   // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     bool  KnlFlashHeader::processData()
 *  @return success state
 *
 *  Process header data and update internal data and returns success state.
 *  If this function returns \c false, the header is corrupted or invalid.
 */

bool  KnlFlashHeader::processData()
{
    if (!image())
        return  false;
    if (!m_FatContents.process(image()->filePath()))
        return false;
    vector<DataFile> files = m_FatContents.getFiles();
    for (const auto &file : files)
    {
        size_t posLB = file.fileName.find("_LB");
        size_t offset;
        string tmpName;
        switch(file.fileID)
        {
            case eBios:
                addItem(FlashImage::BIOS, file.fileName.substr(0, posLB));
                image()->setBios();
                LOG(INFO_MSG, "Bios capsule found: " + file.fileName);
                break;
            case eME:
                offset = string("ME_SPS_PHI_").size();
                tmpName = file.fileName.substr(offset, posLB - offset);
                cleanVersionName(tmpName);
                addItem(FlashImage::ME, tmpName);
                image()->setME();
                LOG(INFO_MSG, "ME capsule found: " + file.fileName);
                break;
            case eSMCFabA:
                offset = string("uBMC_OP_KNL_").size();
                tmpName = file.fileName.substr(offset, posLB - offset);
                cleanVersionName(tmpName);
                addItem(FlashImage::SMCFABA, tmpName);
                image()->setSMC();
                LOG(INFO_MSG, "SMC capsule for FabA found: " + file.fileName);
                break;
            case eSMCFabB:
                offset = string("uBMC_OP_KNL_").size();
                tmpName = file.fileName.substr(offset, posLB - offset);
                cleanVersionName(tmpName);
                addItem(FlashImage::SMCFABB, tmpName);
                image()->setSMC();
                LOG(INFO_MSG, "SMC capsule for FabB found: " + file.fileName);
                break;
            case eIflash:
                LOG(INFO_MSG, "iflash32 found");
                break;
            case eStartupA:
            case eStartupB:
            case eStartupX:
                // nothing to do
                break;
            case eStartupT:
                image()->setBootTest();
                LOG(INFO_MSG, "Boot test enable");
                break;
            case eIPMI:
                LOG(INFO_MSG, "IPMI capsule found");
                break;
            case eIftemp:
                LOG(INFO_MSG, "iflash found");
                break;
            case eCapsule:
                LOG(INFO_MSG, "Capsule.cfg found");
                break;
            default:
                LOG(INFO_MSG, "Unknown file found: " + file.fileName);
                break;
        }
    }
    setTargetPlatform( eKnlPlatform );
    setSigned( true );
    setValid( true );
    return  true;
}

void KnlFlashHeader::cleanVersionName(string & str, bool removeLast) const
{
    size_t dot;
    if (removeLast)
    {
        reverse(str.begin(), str.end());
        dot = str.find(".");
        if ((dot != string::npos) && (str.find(".", dot) != string::npos))
        {
            str = str.substr(dot + 1, str.size());
        }
        reverse(str.begin(), str.end());
    }
    if ((dot = str.compare(0, 1, "0")) == 0)
    {
        str.erase(0, 1);
        cleanVersionName(str, false);
    }
    if ((dot = str.find(".0")) != string::npos)
    {
        str.erase(dot + 1, 1);
        cleanVersionName(str, false);
    }
}

std::vector<DataFile> KnlFlashHeader::getFiles() const {
   return m_FatContents.getFiles();
}

int KnlFlashHeader::getFileID(const std::string & line) const
{
    return m_FatContents.getFileID(line);
}