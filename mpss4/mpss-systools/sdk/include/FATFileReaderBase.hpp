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

#ifndef MICMGMT_FATFILEREADERBASE_HPP
#define MICMGMT_FATFILEREADERBASE_HPP

#include <vector>
#include <cstdint>
#include <array>

// NAMESPACE
//
namespace  micmgmt
{
    enum Hddimg { eError = 0, eStartupA, eStartupB, eStartupX, eStartupT,
                  eBios, eME, eSMCFabA, eSMCFabB, eIflash, eIPMI, eIftemp,
                  eCount, eCapsule, eMaxCount};
    const int MAXFILESIZE            = 10485760;
    const std::string BIOS_PATTERN   = "GVP"; //Find either debug or release BIOSes
    const std::string ME_PATTERN     = "ME_SPS_PHI_";
    const std::string SMC_PATTERN    = "uBMC_OP_KNL_";
    const std::string IFLASH_PATTERN = "iflash32.efi";
    const std::string FABA_PATTERN   = "faba.nsh";
    const std::string FABB_PATTERN   = "fabb.nsh";
    const std::string FABX_PATTERN   = "fabx.nsh";
    const std::string TEST_PATTERN   = "test.nsh";
    const std::string SHA_PATTERN    = "capsule.cfg";
    const std::string IPMI_PATTERN   = "ipmi.efi";
    const std::string TEMP_PATTERN   = "iflash32_temp.efi";
    const std::string FABA_FILE      = "fabA.hddimg";
    const std::string FABB_FILE      = "fabB.hddimg";
    const std::string FABX_FILE      = "fabX.hddimg";
    const unsigned char ROOTDIRSIZE = 0x20;
    const uint16_t SECTOR_SIZE = 512;
    // The first cluster of the data area is cluster #2
    const size_t CLUSTEROFFSET = 2;
    const size_t BUFFERSIZE = 4096;

    struct BootSector
    {
        uint32_t sectorSize;
        uint32_t sectorPerCluster;
        uint32_t reservedSec;
        uint32_t numberFats;
        uint32_t rootEntries;
        uint32_t sectorPerFat;
        uint32_t rootDirAddr;
        BootSector()
            : sectorSize(0), sectorPerCluster(0), reservedSec(0), numberFats(0), rootEntries(0),
                       sectorPerFat(0), rootDirAddr(0) {}
        BootSector(const uint32_t sectorSize_, const uint32_t sectorPerCluster_, const uint32_t reservedSec_,
                   const uint32_t numberFats_, const uint32_t rootEntries_, const uint32_t sectorPerFat_,
                   const uint32_t rootDirAddr_)
            : sectorSize(sectorSize_), sectorPerCluster(sectorPerCluster_),reservedSec(reservedSec_),
            numberFats(numberFats_), rootEntries(rootEntries_), sectorPerFat(sectorPerFat_),
            rootDirAddr(rootDirAddr_) {}
    };

    struct DataFile
    {
        unsigned int fileSize;
        unsigned int startingCluster;
        unsigned long entryPosition;
        std::string fileName;
        std::string fileHash;
        int fileID;
        std::vector<unsigned char>rawData;
        DataFile(const unsigned int fileSize_, const unsigned int startingCluster_, const std::string & fileName_,
                 const std::string & fileHash_)
            : fileSize(fileSize_), startingCluster(startingCluster_), fileName(fileName_), fileHash(fileHash_) {}
        DataFile(const unsigned int fileSize_, const unsigned int startingCluster_,
                 const unsigned long entryPosition_, const std::string & fileName_, const std::string & fileHash_,
                 const int fileID_, std::vector<unsigned char> rawData_)
            : fileSize(fileSize_), startingCluster(startingCluster_), entryPosition(entryPosition_),
            fileName(fileName_), fileHash(fileHash_), fileID(fileID_), rawData(rawData_) {}
    };

    class FATFileReaderBase
    {
        public:
            FATFileReaderBase();
            virtual ~FATFileReaderBase();
            virtual bool process(const std::string&) = 0;
            std::vector<DataFile> getFiles() const;
            int numFiles() const;
            int getFileID(const std::string &) const;

        protected:
            bool getFile(FILE *, DataFile &, BootSector &, std::vector<unsigned char> &);
            bool getRootDirOffset(FILE*, BootSector&);
            std::string getLongNamePortionFromEntry(const std::array<unsigned char, ROOTDIRSIZE> &);
            DataFile getShortNameFromEntry(const std::array<unsigned char, ROOTDIRSIZE> &);
            FATFileReaderBase(const FATFileReaderBase&);
            FATFileReaderBase&  operator = (const FATFileReaderBase&);
            std::vector<DataFile> m_FilesInFat;
            BootSector m_BootSector;
    };
}   // namespace micmgmt

#endif //MICMGMT_FATFILEREADERBASE_HPP