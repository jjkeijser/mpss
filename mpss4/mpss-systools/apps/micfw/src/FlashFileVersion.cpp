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
#include <iomanip>
#include <sstream>
#include <fstream>
#include <cstdio>

#include "MicFwError.hpp"

#include "FlashFileVersion.hpp"
#include "FlashImage.hpp"

#include "MicOutput.hpp"
#include "MicException.hpp"
#include "micmgmtCommon.hpp"
#include "MicLogger.hpp"
#include "Utils.hpp"

using namespace std;
using namespace micmgmt;

/*
Current hddimg file version supported. Since hddimg files are distributed together
with the MPSS we do not support backward compatibility of hddimg files.

version    feature
   0.x     original
   1.x     write postcode 0x0E aka firmware_online
   2.x     introduces checksum verification and sentinels to verify iflash32 started working
   3.x     introduces support to allow multi Fab PCB firmware. remove backward compatibility
*/

namespace
{
    const string currentImageVersion = "3.0";
    const int SHALEN = 50;
    const int MAXWIDTH = 78;
} //END PRIVATE NAMESPACE

namespace micfw
{
    FlashFileVersion::FlashFileVersion(const std::string& imageFile,
        std::shared_ptr<micmgmt::MicOutput>& output, const std::string& toolName)
        : imageFile_(imageFile), output_(output), toolName_(toolName)
    {
    }

    FlashFileVersion::~FlashFileVersion()
    {
    }

    int FlashFileVersion::runCheck()
    {
        MicFwError err(toolName_);
        if(image_.setFilePath(imageFile_))
        {
            throw MicException(err.buildError(MicFwErrorCode::eMicFwFileImageBad,
                               ": Image is empty"));
        }
        return verifyHash(image_);
    }

    bool FlashFileVersion::run()
    {
        if (runCheck())
        {
            MicFwError err(toolName_);
            throw MicException(err.buildError(MicFwErrorCode::eMicFwFileImageBad,
                               ": Bad Firmware Image = '" + fileNameFromPath(imageFile_) + "'"));
        }
        FlashHeaderInterface::ItemVersionMap versions = image_.itemVersions();
        string showName = string(fileNameFromPath(imageFile_));
        if(showName.size() > MAXWIDTH)
        {
            showName.resize(MAXWIDTH);
            LOG(WARNING_MSG, "Filename is too long to display completely");
        }
        output_->startSection(showName);
        for (auto& it : versions)
        {
            const string& name = it.first;
            const string& version = it.second;
            output_->outputNameValuePair(name, version);
        }
        output_->endSection();
        return false;
    }

    bool FlashFileVersion::createImageVersions(const micmgmt::FabImagesPaths & fip) const
    {
        LOG(INFO_MSG, "Creating images...");
        LOG(INFO_MSG, "Base file: " + image_.filePath());
        LOG(INFO_MSG, "Image for PCB A: " + fip.nameA);
        LOG(INFO_MSG, "Image for PCB B: " + fip.nameB);
        LOG(INFO_MSG, "Image for PCB X: " + fip.nameX);
        ImageCreator ic;
        return ic.createFabImages(image_, fip);
    }

    int FlashFileVersion::verifyHash(FlashImage & image)
    {
        HashTable ht;
        CalculateSHA256 sha = CalculateSHA256();
        vector<DataFile> files = image.getFiles();
        bool isHash = false;
        string errorMsg = "Checksum: FAILED";
        for (auto &file : files)
        {
            if (!file.fileName.compare("capsule.cfg"))
            {
                LOG(INFO_MSG, "Running checksum verification...");
                processHashTable(ht, file, image);
                isHash=true;
                break;
            }
        }
        if (!isHash)
        {
            errorMsg = "Checksum verification is not available";
            goto error;
        }
        if (ht.imageVersion.compare(currentImageVersion) != 0)
        {
            errorMsg = "Incompatible hddimg vesion. Found: "
                       + ht.imageVersion + " required: " + currentImageVersion;
            goto error;
        }
        LOG(INFO_MSG, "hddimg version: " + ht.imageVersion);
        for ( auto & file : files)
        {
            int fileID = file.fileID;
            if ( fileID < eCount && fileID > 0 )
            {
                LOG(INFO_MSG, "File: " + file.fileName);
                if(sha.calculateSHA256(file)) // Get SHA256 value
                    goto error;
                if (file.fileHash.compare(ht.fileHash[fileID]))
                    goto error;
                LOG(INFO_MSG, "Checksum: PASS");
                if (!file.fileName.compare(FABA_PATTERN))
                    image.setPosNameFabA(file.entryPosition);
                if (!file.fileName.compare(FABB_PATTERN))
                    image.setPosNameFabB(file.entryPosition);
                if (!file.fileName.compare(FABX_PATTERN))
                    image.setPosNameFabX(file.entryPosition);
                if (!file.fileName.compare(TEST_PATTERN))
                    image.setPosNameFabT(file.entryPosition);
                if (fileID == eBios)
                    image.setBios();
                if (fileID == eME)
                    image.setME();
                if ((fileID == eSMCFabA) || (fileID == eSMCFabB))
                    image.setSMC();
            }
        }
        LOG(INFO_MSG, "All files in capsule " + fileNameFromPath(imageFile_)
            + " were verified successfully");
        return 0;
    error:
        LOG(ERROR_MSG, errorMsg);
        MicFwError err(toolName_);
        throw MicException(err.buildError(MicFwErrorCode::eMicFwFileImageBad,
                           ": Bad Firmware Image = '" + fileNameFromPath(imageFile_) + "'"));
        return 1;
    }

    int FlashFileVersion::processHashTable(HashTable & ht, const DataFile & f,
                                           const FlashImage & image) const
    {
        int count = 0;
        bool isFile = false;
        bool isHash = false;
        string rawData(f.rawData.begin(), f.rawData.end());
        istringstream issRawData(rawData);
        string tFile;
        string tHash;
        string line;
        while (getline(issRawData, line))
        {
            istringstream iss(line);
            string type, eq, val;
            /* type gets the first word from the line */
            if (!(iss >> type ))
                return false;
            else if (type[0] == '#')
                continue;
            /* eq gets the second word and val the third one. std::ws Extracts
               as many whitespace characters as possible from the current
               position in the input sequence */
            else if (!(iss >> eq >> val >> std::ws) || eq != "=" || iss.get() != EOF)
                return false;
            if (type.compare("version") == 0)
            {
                ht.imageVersion = val;
                continue;
            }
            if (type.compare("file") == 0)
            {
                isFile = true;
                tFile = val;
            }
            if (type.compare("hash") == 0)
            {
                isHash = true;
                tHash = val.substr (0,64);
            }
            if (isFile && isHash)
            {
                isFile = false;
                isHash = false;
                int id = image.getFileID(tFile);
                count++;
                ht.fileName[id] = tFile;
                ht.fileHash[id] = tHash;
            }
        }
        return count;
    }
}; // namespace micfw