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

#ifndef MICMGMT_FATMOCKFILEREADER_HPP
#define MICMGMT_FATMOCKFILEREADER_HPP

#include <vector>
#include <cstdint>

#include "FATFileReaderBase.hpp"

// NAMESPACE
//
namespace  micmgmt
{
    class FATMockFileReader : public FATFileReaderBase
    {
        public:
            FATMockFileReader();
            bool process(const std::string&);
        private:
            std::vector<unsigned char>m_vecBios;
            std::vector<unsigned char>m_vecFlash32;
            std::vector<unsigned char>m_vecFlashT;
            std::vector<unsigned char>m_vecIPMI;
            std::vector<unsigned char>m_vecME;
            std::vector<unsigned char>m_vecStart;
            std::vector<unsigned char>m_vecSMC;
            std::vector<unsigned char>m_vecCap;
            std::vector<unsigned char>m_vecCapNoBios;
            std::vector<unsigned char>m_vecCapNoSMC;
            std::vector<unsigned char>m_vecCapNoME;
            std::vector<unsigned char>m_vecCapEmpty;
            std::vector<unsigned char>m_vecCapErr1;
            std::vector<unsigned char>m_vecCapErr2;
            std::vector<unsigned char>m_vecCapErr3;
            std::vector<unsigned char>m_vecCapErr4;
            std::vector<unsigned char>m_vecCapErr5;
            std::vector<unsigned char>m_vecCapErr6;
            std::vector<unsigned char>m_vecCapSwap;
            std::vector<unsigned char>m_vecVerErr;
    };
}   // namespace micmgmt


#endif //MICMGMT_FATMOCKFILEREADER_HPP