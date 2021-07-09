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

#ifndef MICFW_UTILS_HPP
#define MICFW_UTILS_HPP

#if defined( __linux__ )
#  include "UtilsLinux.hpp"
#
#elif defined( _WIN32 )
#  include "UtilsWindows.hpp"
#else
#  error  "*** UNSUPPORTED MPSS ENVIRONMENT ***"
#endif

#include <cstdio>
#include <string>
#include <fstream>

namespace micfw
{
    class PathNamesGenerator : public PathNamesGeneratorBase
    {
    public:
        std::string getPathA()
        {
            return std::string(m_NameA) + ".hddimg";
        }
        std::string getPathB()
        {
            return std::string(m_NameB) + ".hddimg";
        }
        std::string getPathX()
        {
            return std::string(m_NameX) + ".hddimg";
        }
        std::string getPathT()
        {
            return std::string(m_NameT) + ".hddimg";
        }
    };

    class CalculateSHA256 : public CalculateSHA256Base
    {
    public:
        CalculateSHA256()
        {
            // Nothing to do
        }
        ~CalculateSHA256()
        {
            // Nothing to do
        }
    };

    class ImageCreator
    {
    public:
        ImageCreator()
        {
            // Nothing to do
        }
        ~ImageCreator()
        {
            // Nothing to do
        }
        bool createFabImages(const FlashImage & image, const micmgmt::FabImagesPaths & filePaths)
        {
            const std::string startup = "startup";
            bool rv = true;
            m_Source.open(image.filePath(), ios::binary);
            m_DestA.open(filePaths.nameA, ios::binary);
            m_DestB.open(filePaths.nameB, ios::binary);
            m_DestX.open(filePaths.nameX, ios::binary);
            m_DestT.open(filePaths.nameT, ios::binary);
            if(!(m_Source.is_open() && m_DestA.is_open() && m_DestB.is_open() &&
                 m_DestX.is_open() && m_DestT.is_open()))
                goto exit;
            // create copys of hddimg image
            m_DestA << m_Source.rdbuf();
            m_DestA.seekp (image.getPosNameFabA());
            m_Source.seekg(0, ios::beg);
            m_DestB << m_Source.rdbuf();
            m_DestB.seekp (image.getPosNameFabB());
            m_Source.seekg(0, ios::beg);
            m_DestX << m_Source.rdbuf();
            m_DestX.seekp (image.getPosNameFabX());
            m_Source.seekg(0, ios::beg);
            m_DestT << m_Source.rdbuf();
            m_DestT.seekp (image.getPosNameFabT());
            if(!(m_DestA.good() && m_DestB.good() &&
                 m_DestX.good() && m_DestT.good()))
                goto exit;
            // rename file faba.nsh to startup.nsh
            m_DestA.write(startup.c_str(), startup.size());
            // rename file fabb.nsh to startup.nsh
            m_DestB.write(startup.c_str(), startup.size());
            // rename file fabx.nsh to startup.nsh
            m_DestX.write(startup.c_str(), startup.size());
            // rename file test.nsh to startup.nsh
            m_DestT.write(startup.c_str(), startup.size());

            if(!(m_DestA.good() && m_DestB.good() &&
                 m_DestX.good() && m_DestT.good()))
                goto exit;
            rv = false;
        exit:
            return rv;
        }
    private:
        ifstream m_Source;
        ofstream m_DestA;
        ofstream m_DestB;
        ofstream m_DestX;
        ofstream m_DestT;
    };
} // namespace micfw
#endif // MICFW_UTILS_HPP
