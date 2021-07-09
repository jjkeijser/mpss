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

#ifndef MICFW_UTILSLINUX_HPP
#define MICFW_UTILSLINUX_HPP

#include <iomanip>
#include <sstream>
#include <cstdio>
#include <openssl/sha.h>

using namespace  std;

namespace micfw
{
    class PathNamesGeneratorBase
    {
    public:
        PathNamesGeneratorBase()
        {
            // Nothing to do
        }
        ~PathNamesGeneratorBase()
        {
            // Nothing to do
        }
        bool generatePaths()
        {
            if (tmpnam(m_NameA) == NULL || tmpnam(m_NameB) == NULL ||
                tmpnam(m_NameX) == NULL || tmpnam(m_NameT) == NULL)
            {
                return true;
            }
            return false;
        }
    protected:
        char m_NameA[L_tmpnam];
        char m_NameB[L_tmpnam];
        char m_NameX[L_tmpnam];
        char m_NameT[L_tmpnam];
    };

    class CalculateSHA256Base
    {
    public:
        CalculateSHA256Base()
        {
            // Nothing to do
        }
        ~CalculateSHA256Base()
        {
            // Nothing to do
        }
        bool calculateSHA256(DataFile & f)
        {
            stringstream ss;
            unsigned char hash[SHA256_DIGEST_LENGTH];
            SHA256_CTX sha256;
            if(!SHA256_Init(&sha256))
                goto error;
            if(!SHA256_Update(&sha256, &f.rawData[0], f.rawData.size()))
                goto error;
            if(!SHA256_Final(hash, &sha256))
                goto error;
            for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
                ss << hex << setw(2) << setfill('0') << (int)hash[i];
            f.fileHash = ss.str();
            return false;
        error:
            return true;
        }
    };
} // namespace micfw
#endif // MICFW_UTILSLINUX_HPP
