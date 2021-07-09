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

// SYSTEM INCLUDES
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>
#include <array>
#include <string>
#include <sstream>
#include <iomanip>

// PROJECT INCLUDES
#include "FATFileReader.hpp"
#include "FATFileReaderBase.hpp"
#include "MicLogger.hpp"
#include "MicException.hpp"

using namespace std;
using namespace micmgmt;

//----------------------------------------------------------------------------
/** @fn     FATFileReader::process(const std::string& FATImageName)
*  @return success state
*
*  Process FAT image contents and returns sucess state
*  If this function returns \c false; it could not process the FAT image properly
*/

bool FATFileReader::process(const std::string & FATImageName)
{
    FILE* fatImage;
    bool error = true;
#if defined(_WIN32)
    if (fopen_s(&fatImage, FATImageName.c_str(), "rb") != 0)
    {
#else
    if ((fatImage = fopen(FATImageName.c_str(), "rb")) == NULL)
    {
#endif
        LOG(ERROR_MSG, "Failed to open file: " + FATImageName);
        return false;
    }

    array<unsigned char, ROOTDIRSIZE> text = { 0 };

    if (!getRootDirOffset(fatImage, m_BootSector))
    {
        LOG(ERROR_MSG, "Failed to process file: " + FATImageName);
        return false;
    }
    int rootDirAddr = m_BootSector.rootDirAddr;

    // go to the root dir address
    if (fseek(fatImage, rootDirAddr, SEEK_SET))
    {
        LOG(ERROR_MSG, "Failed to process file: " + FATImageName);
        return false;
    }
    vector<unsigned char>bufferFile(MAXFILESIZE);
    int count = 0;
    string fileName;
    for (unsigned int i = 0; i < m_BootSector.rootEntries; ++i)
    {
        unsigned long pos = ftell(fatImage);
        error = (fread(&text, 1, ROOTDIRSIZE, fatImage) != ROOTDIRSIZE);
        if (error)
            break;
        if (text[0] == 0x00)
            // No-more-entries marker
            break;

        // Calculate the number of entries per name
        // Offset 11 contains file attibutes. If offset 11
        // is 0x0f, it means it is a long file name (LFN).
        if (text[0] == 0x41 && text[11] == 0x0f && count == 0)
            m_FilesInFat.emplace_back(0, 0, getLongNamePortionFromEntry(text), "");
        else if (text[11] == 0x0f)
        {
            // FAT spec ORs the highest sequence number with 0x40
            if ((text[0] | 0x40) == text[0])
                count = text[0] - 0x40; // ... so subtract 0x40 to get entries per name
        }

        // Extract each chunk string from each entry
        if (text[11] == 0x0f && count > 0)
        {
            string fileNamePortion = getLongNamePortionFromEntry(text);
            fileName.insert(fileName.begin(), fileNamePortion.begin(),
                fileNamePortion.end());

            count--;

            if (count == 0)
            {
                // We finished reading all portions of our long file name
                error = (fread(&text, 1, ROOTDIRSIZE, fatImage) != ROOTDIRSIZE);
                if (error)
                    break;
                DataFile datafile = getShortNameFromEntry(text);
                error = !getFile(fatImage, datafile, m_BootSector, bufferFile);
                if (error)
                    break;
                vector<unsigned char>vec;
                vec.insert(vec.end(), bufferFile.data(), bufferFile.data() + datafile.fileSize);
                m_FilesInFat.emplace_back(datafile.fileSize, datafile.startingCluster, pos, fileName, "", getFileID(fileName), vec);
                fileName.clear();
            }
        }
        else if (text[11] == 0x20) /* Short name. If offset 11 is 0x20, it means it is an archive 8.3 name */
        {
            DataFile datafile = getShortNameFromEntry(text);
            error = !getFile(fatImage, datafile, m_BootSector, bufferFile);
            if (error)
                break;
            vector<unsigned char>vec;
            vec.insert(vec.end(), bufferFile.data(), bufferFile.data() + datafile.fileSize);
            m_FilesInFat.emplace_back(datafile.fileSize, datafile.startingCluster, pos, datafile.fileName, "", getFileID(datafile.fileName), vec);
        }
    }
    fclose(fatImage);
    if(error)
    {
        LOG(ERROR_MSG, "Failed to process file: " + FATImageName);
        return false;
    }

    if (m_FilesInFat.empty())
    {
        LOG(ERROR_MSG, "The fat image is empty");
        return false;
    }
    return true;
}