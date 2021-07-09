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
#include "FATFileReaderBase.hpp"
#include "MicLogger.hpp"
#include "MicException.hpp"

using namespace std;
using namespace micmgmt;

//============================================================================
//  P U B L I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     FATFileReaderBase::FATFileReaderBase( )
*  @param  image   Pointer to parent image
*
*  Construct a FAT image reader object
*/

FATFileReaderBase::FATFileReaderBase()
{
    //Nothing to do
}

//----------------------------------------------------------------------------
/** @fn    FATFileReaderBase::~FATFileReaderBase( )
*
*  Cleanup.
*/

FATFileReaderBase::~FATFileReaderBase()
{
    //Nothing to do
}

//----------------------------------------------------------------------------
/** @fn     FATFileReaderBase::getFileID(const std::string & line)
*  @return file ID
*
*  Each hddimg image have to have 10 files to work by using name patterns this
*  fucntion identifies each type of file such as bios, me, iflas32 and so on.
*  We use this this ID as an index of an array were files are stored before
*  they are processed.
*/

int FATFileReaderBase::getFileID(const std::string & line) const
{
    Hddimg hddimg = eError;
    size_t posLB = line.find("_LB");

    if (!line.compare(0, BIOS_PATTERN.size(), BIOS_PATTERN) && posLB != string::npos)
        hddimg = eBios;
    if (!line.compare(0, ME_PATTERN.size(), ME_PATTERN) && posLB != string::npos)
        hddimg = eME;
    if (!line.compare(0, SMC_PATTERN.size(), SMC_PATTERN) && line.find("_LB_FabA") != string::npos)
        hddimg = eSMCFabA;
    if (!line.compare(0, SMC_PATTERN.size(), SMC_PATTERN) && line.find("_LB_FabB") != string::npos)
        hddimg = eSMCFabB;
    if (!line.compare(IFLASH_PATTERN))
        hddimg = eIflash;
    if (!line.compare(FABA_PATTERN))
        hddimg = eStartupA;
    if (!line.compare(FABB_PATTERN))
        hddimg = eStartupB;
    if (!line.compare(FABX_PATTERN))
        hddimg = eStartupX;
    if (!line.compare(TEST_PATTERN))
        hddimg = eStartupT;
    if (!line.compare(IPMI_PATTERN))
        hddimg = eIPMI;
    if (!line.compare(TEMP_PATTERN))
        hddimg = eIftemp;
    if (!line.compare(SHA_PATTERN))
        hddimg = eCapsule;
    return hddimg;
}

//----------------------------------------------------------------------------
/** @fn     FATFileReaderBase::getFiles()
*  @return files read
*
*  Ruturns files read from hddimg
*/

std::vector<DataFile> FATFileReaderBase::getFiles() const
{
    return m_FilesInFat;
}

bool FATFileReaderBase::getFile(FILE * in, DataFile & datafile, BootSector & bootsector,
                                vector<unsigned char> & bufferFile)
{
    unsigned long startAddress = ftell(in);
    unsigned long fatStart = bootsector.reservedSec * bootsector.sectorSize;
    unsigned long dataStart = bootsector.rootDirAddr + bootsector.rootEntries * ROOTDIRSIZE;
    int count = 0;
    unsigned short cluster = (unsigned short)datafile.startingCluster;
    size_t bytesRead, bytesToRead, fileToGo = datafile.fileSize;
    size_t clusterToGo = bootsector.sectorPerCluster * bootsector.sectorSize;

    if (datafile.fileSize > bufferFile.size())
        return false;
    // Go to first data cluster
    if (fseek(in, dataStart + (bootsector.sectorPerCluster * bootsector.sectorSize)
              * (datafile.startingCluster - CLUSTEROFFSET), SEEK_SET))
        return false;
    // Read until we run out of file or clusters
    while (fileToGo > 0 && datafile.startingCluster != 0xFFFF)
    {
        bytesToRead = 4096;
        // Detect EOF
        if (bytesToRead > fileToGo)
            bytesToRead = fileToGo;
        if (bytesToRead > clusterToGo)
            bytesToRead = clusterToGo;
        // read data from cluster
        bytesRead = fread(bufferFile.data() + (bootsector.sectorPerCluster*bootsector.sectorSize)
                          * count, 1, bytesToRead, in);
        if (bytesRead != bytesToRead)
            return false;
        // decrease byte counters for current cluster and whole file
        clusterToGo -= bytesRead;
        fileToGo -= bytesRead;
        // if we have read the whole cluster, read next cluster from File Alocation Table
        if (clusterToGo == 0)
        {
            if (fseek(in, fatStart + cluster * CLUSTEROFFSET, SEEK_SET))
                return false;
            if (!fread(&cluster, CLUSTEROFFSET, 1, in))
                return false;
            if (fseek(in, dataStart + (bootsector.sectorPerCluster * bootsector.sectorSize)
                      * (cluster - CLUSTEROFFSET), SEEK_SET))
                return false;
            // reset cluster byte counter
            clusterToGo = (bootsector.sectorPerCluster * bootsector.sectorSize);
        }
        count++;
    }
    // Restore pointer to File Alocation Table
    if (fseek(in, startAddress, SEEK_SET))
        return false;
    return true;
}

bool FATFileReaderBase::getRootDirOffset(FILE* in, BootSector & bootsector)
{
    if (fseek(in, 0x0B, SEEK_SET))
        return false;
    if (!fread(&bootsector.sectorSize, sizeof(uint16_t), 1, in))
        return false;
    // Number of sectors per cluster
    if (fseek(in, 0x0D, SEEK_SET))
        return false;
    if (!fread(&bootsector.sectorPerCluster, sizeof(uint8_t), 1, in))
        return false;
    // Reserved sectors (including the boot sector)
    if (fseek(in, 0x0E, SEEK_SET))
        return false;
    if (!fread(&bootsector.reservedSec, sizeof(uint16_t), 1, in))
        return false;
    // Number of FATs
    if (fseek(in, 0x10, SEEK_SET))
        return false;
    if (!fread(&bootsector.numberFats, sizeof(uint8_t), 1, in))
        return false;
    // Number of directory entries in the root directory
    if (fseek(in, 0x11, SEEK_SET))
        return false;
    if (!fread(&bootsector.rootEntries, sizeof(uint16_t), 1, in))
        return false;
    if(fseek(in, 0x16, SEEK_SET))
        return false;
    if (!fread(&bootsector.sectorPerFat, sizeof(uint16_t), 1, in))
        return false;
    bootsector.rootDirAddr = (bootsector.reservedSec + bootsector.numberFats
                              * bootsector.sectorPerFat) * bootsector.sectorSize;
    return true;
}

string FATFileReaderBase::getLongNamePortionFromEntry(const array<unsigned char, ROOTDIRSIZE> &entry)
{
    // We assume entry is a 32-byte FAT entry
    string fileName;
    for (int i = 1; i < (int)entry.size(); i++)
    {
        switch (i)
        {
            case 11: // File attributes
            case 12: // Reserved
            case 13: // Checksum
            case 26: // Reserved
            case 27: // Reserved
                continue;
            default: // 1-10,14-15,28-31 File name characters
                break;
        }
        // Check the end of the string, convert from unicode to ascii and store result
        if (entry[i] != 0xff && entry[i] != 0x00)
            fileName.push_back(static_cast<char>(entry[i]));
    }
    return fileName;
}

/* bit 12.3 = lowercase basename
   bit 12.4 = lowercase extension */
DataFile FATFileReaderBase::getShortNameFromEntry(const array<unsigned char, ROOTDIRSIZE> &entry)
{
    // We assume entry is a 32-byte FAT entry
    string fileName;
    unsigned int startingCluster = 0x00 << 24 | 0x00 << 16 | entry[27] << 8 | entry[26];
    unsigned int fileSize = entry[31] << 24 | entry[30] << 16 | entry[29] << 8 | entry[28];
    char lc = 0;
    for (int i = 0; i < (int)entry.size(); i++)
    {
        switch (i)
        {
            case 0: // File name
            case 1: //
            case 2: //
            case 3: //
            case 4: //
            case 5: //
            case 6: //
            case 7: //
                lc = ((entry[i] >= 'A' && entry[i] <= 'Z') && (entry[12] & 0x08)) ? 0x01 << 5 : 0x00;
                break;
            case 8: // File extension
                fileName.push_back(static_cast<char>('.'));
                lc = ((entry[i] >= 'A' && entry[i] <= 'Z') && (entry[12] & 0x10)) ? 0x01 << 5 : 0x00;
                break;
            case 9: //
            case 10: //
                lc = ((entry[i] >= 'A' && entry[i] <= 'Z') && (entry[12] & 0x10)) ? 0x01 << 5 : 0x00;
                break;
            default: //
                continue;
        }
        // Check the end of the string and store result
        if (entry[i] != 0x20)
            fileName.push_back(static_cast<char>(entry[i] | lc));
    }
    DataFile fileEntry(fileSize, startingCluster, fileName, "");
    return fileEntry;
}
