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

#ifndef MICFW_FLASHFILEVERSION_HPP
#define MICFW_FLASHFILEVERSION_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <memory>

#include "FlashImage.hpp"

#ifdef UNIT_TESTS
#define PRIVATE public
#else
#define PRIVATE private
#endif

using namespace micmgmt;

struct HashTable
{
    std::string imageVersion;
    std::string fileName[eMaxCount];
    std::string fileHash[eMaxCount];
    HashTable() : imageVersion("unavailable")
    {
    }
};

namespace micmgmt
{ // Forward references...
    class MicOutput;
    struct DataFile;
    class FlashImage;
    struct FabImagesPaths;
}

namespace micfw
{
    class FlashFileVersion
    {
    public: // constructor/destructors
        FlashFileVersion(const std::string& imageFile, std::shared_ptr<micmgmt::MicOutput>& output, const std::string& toolName);
        ~FlashFileVersion();

    public: // API
        bool run();
        int runCheck();
        bool createImageVersions(const micmgmt::FabImagesPaths & fip) const;

    private: // Hidden special members
        FlashFileVersion();
        FlashFileVersion(const FlashFileVersion&);
        FlashFileVersion& operator=(const FlashFileVersion&);
        int processHashTable(HashTable &, const DataFile &, const FlashImage &) const;
        int verifyHash(FlashImage & );

    private: // Implementation

    PRIVATE: // Fields
        FlashImage image_;
        std::string imageFile_;
        std::shared_ptr<micmgmt::MicOutput> output_;
        std::string toolName_;
    };
} // namespace micfw

#endif // MICFW_FLASHFILEVERSION_HPP