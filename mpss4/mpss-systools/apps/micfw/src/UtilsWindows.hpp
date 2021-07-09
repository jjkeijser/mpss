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

#ifndef MICFW_UTILSWINDOWS_HPP
#define MICFW_UTILSWINDOWS_HPP

#include <iomanip>
#include <sstream>
#include <cstdio>
#include <windows.h>
#include <Wincrypt.h>

using namespace  std;

namespace micfw
{
    class PathNamesGeneratorBase
    {
    public:
        PathNamesGeneratorBase()
        {
            m_NameA = NULL;
            m_NameB = NULL;
            m_NameX = NULL;
            m_NameT = NULL;
        }
        ~PathNamesGeneratorBase()
        {
            // Nothing to do
        }
        bool generatePaths()
        {
            if (((m_NameA = _tempnam("c:\\", "fabA")) == NULL) ||
                ((m_NameB = _tempnam("c:\\", "fabB")) == NULL) ||
                ((m_NameX = _tempnam("c:\\", "fabX")) == NULL) ||
                ((m_NameT = _tempnam("c:\\", "fabT")) == NULL))
            {
                return true;
            }
            return false;
        }
    protected:
        char *m_NameA;
        char *m_NameB;
        char *m_NameX;
        char *m_NameT;
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
            bool rv = true;
            DWORD hashlen;
            stringstream ss;
            HCRYPTPROV hProv = 0;
            HCRYPTHASH hHash;
            BYTE rgbHash[50];
            string errMsg;

            if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
            {
                errMsg = "CryptAcquireContext failed";
                goto cleanupA;
            }
            if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash))
            {
                errMsg = "CryptCreateHash failed";
                goto cleanupB;
            }
            if (!CryptHashData(hHash, &f.rawData[0], (DWORD)f.rawData.size(), 0))
            {
                errMsg = "CryptHashData failed";
                goto cleanupC;
            }
            if (!CryptGetHashParam(hHash, HP_HASHVAL, rgbHash, &hashlen, 0))
            {
                errMsg = "CryptGetHashParam failed";
                goto cleanupC;
            }
            for (unsigned int i = 0; i < hashlen; i++)
                ss << hex << setw(2) << setfill('0') << (int)rgbHash[i];
            f.fileHash = ss.str();
            rv = false;
        cleanupC:
            CryptDestroyHash(hHash);
        cleanupB:
            CryptReleaseContext(hProv, 0);
        cleanupA:
            return rv;
        }
    };
} // namespace micfw
#endif // MICFW_UTILSWINDOWS_HPP