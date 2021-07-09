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
#include <vector>
#include <string>

// PROJECT INCLUDES
#include "FATMockFileReader.hpp"

using namespace std;
using namespace micmgmt;

#define UNUSED(expr) do { (void)(expr); } while ((void)0,0)

  // NAMESPACE
using namespace micmgmt;

FATMockFileReader::FATMockFileReader()
{
    string mockDataBios = "Mock Data File Bios";
    m_vecBios.assign(mockDataBios.begin(), mockDataBios.end());
    m_vecBios.push_back(static_cast<char>('\0'));
    string mockDataiFlash = "Mock Data File iFlash32";
    m_vecFlash32.assign(mockDataiFlash.begin(), mockDataiFlash.end());
    m_vecFlash32.push_back(static_cast<char>('\0'));
    string mockDataFlashTemp = "Mock Data File iFlash Temp";
    m_vecFlashT.assign(mockDataFlashTemp.begin(), mockDataFlashTemp.end());
    m_vecFlashT.push_back(static_cast<char>('\0'));
    string mockDataIPMI = "Mock Data File IPMI";
    m_vecIPMI.assign(mockDataIPMI.begin(), mockDataIPMI.end());
    m_vecIPMI.push_back(static_cast<char>('\0'));
    string mockDataME = "Mock Data File ME";
    m_vecME.assign(mockDataME.begin(), mockDataME.end());
    m_vecME.push_back(static_cast<char>('\0'));
    string mockDataStart = "Mock Data File Startup";
    m_vecStart.assign(mockDataStart.begin(), mockDataStart.end());
    m_vecStart.push_back(static_cast<char>('\0'));
    string mockDataBMC = "Mock Data File BMC";
    m_vecSMC.assign(mockDataBMC.begin(), mockDataBMC.end());
    m_vecSMC.push_back(static_cast<char>('\0'));
    string mockDataCapHead =
    "# Micfw format image version\n"
    "version = 3.0\n"
    "# iflash32.efi File\n"
    "file = iflash32.efi\n"
    "hash = 23a324b8d8509a3402de2ec7b32c2c9a6be690730540f377dc207936d9c31bae\n";
    string mockDataCapHeadErr =
    "# Micfw format image version\n"
    "version = 9.9\n"
    "# iflash32.efi File\n"
    "file = iflash32.efi\n"
    "hash = 23a324b8d8509a3402de2ec7b32c2c9a6be690730540f377dc207936d9c31bae\n";
    string mockDataCapBios =
    "# Bios File\n"
    "file = GVPRCRB8.86B.0011.R99.9999999999_LB.cap\n"
    "hash = da9b2c57f90170763949a1b0d07a94653068e861eaee10bb86e0f6c0a3cb1124\n";
    string mockDataCapsMC =
    "# SMC FabA\n"
    "file = uBMC_OP_KNL_999.99.99999.999_LB_FabA.cap\n"
    "hash = 3660ef4e220ff8719d4bd43caa15383c2160bd2525670ca99df39f60ae8d4a93\n"
    "# SMC FabB\n"
    "file = uBMC_OP_KNL_999.99.99999.999_LB_FabB.cap\n"
    "hash = 3660ef4e220ff8719d4bd43caa15383c2160bd2525670ca99df39f60ae8d4a93\n";
    string mockDataCapME =
    "# ME File\n"
    "file = ME_SPS_PHI_99.99.99.999.9_LB.signed.cap\n"
    "hash = 10c56360b96776671ab069ef7768f951ccce0fbdbe687bfa89ca4790370b6c05\n";
    string mockDataCapTail =
    "# startup.nsh FabA\n"
    "file = faba.nsh\n"
    "hash = 64eb226fe46f765b598bbf69990f1f6c268b4c39796460335c51a7ff88ea48c3\n"
    "# startup.nsh FabB\n"
    "file = fabb.nsh\n"
    "hash = 64eb226fe46f765b598bbf69990f1f6c268b4c39796460335c51a7ff88ea48c3\n"
    "# ipmi.efi File\n"
    "file = ipmi.efi\n"
    "hash = b7a69bbce282551ca637f72204a05f24281b3e69bcbee04ca8df8dfbb53672cd\n"
    "# iflash32_temp.efi File\n"
    "file = iflash32_temp.efi\n"
    "hash = ca3bc5263a63171c834f292a78ebc61d50a1d4195e7b04692589a7fcdf3a74cc";
    string mockDataCapErr1 =
    "# ME File\n"
    "file > ME_SPS_PHI_99.99.99.999.9_LB.signed.cap\n"
    "hash = 10c56360b96776671ab069ef7768f951ccce0fbdbe687bfa89ca4790370b6c05\n";
    string mockDataCapErr2 =
    "# ME File\n"
    "file ="
    "hash = 10c56360b96776671ab069ef7768f951ccce0fbdbe687bfa89ca4790370b6c05\n";
    string mockDataCapErr3 =
    "# iflash32_temp.efi File\n"
    "hash = ca3bc5263a63171c834f292a78ebc61d50a1d4195e7b04692589a7fcdf3a74cc"
    "file = iflash32_temp.efi\n";
    string mockDataCapErr4 =
    "# ME File\n"
    "\n"
    "hash = 10c56360b96776671ab069ef7768f951ccce0fbdbe687bfa89ca4790370b6c05\n";
    string mockDataCapErr5 =
    "# ME File\n"
    "file = ME_SPS_PHI_99.99.99.999.9_LB.signed.cap make_error systools file\n"
    "hash = 10c56360b96776671ab069ef7768f951ccce0fbdbe687bfa89ca4790370b6c05\n";
    string mockDataCapErr6 =
    "# ME File\n"
    "file \n"
    "hash = 10c56360b96776671ab069ef7768f951ccce0fbdbe687bfa89ca4790370b6c05\n";
    string mockDataCapSwap =
    "# ME File\n"
    "hash = 10c56360b96776671ab069ef7768f951ccce0fbdbe687bfa89ca4790370b6c05\n"
    "file = ME_SPS_PHI_99.99.99.999.9_LB.signed.cap\n";
    string mockDataCap;
    mockDataCap = mockDataCapHead + mockDataCapBios + mockDataCapsMC + mockDataCapME + mockDataCapTail;
    m_vecCap.assign(mockDataCap.begin(), mockDataCap.end());
    m_vecCap.push_back(static_cast<char>('\0'));
    mockDataCap = mockDataCapHead + mockDataCapsMC + mockDataCapME + mockDataCapTail;
    m_vecCapNoBios.assign(mockDataCap.begin(), mockDataCap.end());
    m_vecCapNoBios.push_back(static_cast<char>('\0'));
    mockDataCap = mockDataCapHead + mockDataCapBios + mockDataCapME + mockDataCapTail;
    m_vecCapNoSMC.assign(mockDataCap.begin(), mockDataCap.end());
    m_vecCapNoSMC.push_back(static_cast<char>('\0'));
    mockDataCap = mockDataCapHead + mockDataCapBios + mockDataCapsMC + mockDataCapTail;
    m_vecCapNoME.assign(mockDataCap.begin(), mockDataCap.end());
    m_vecCapNoME.push_back(static_cast<char>('\0'));
    mockDataCap = mockDataCapHeadErr + mockDataCapBios + mockDataCapsMC + mockDataCapTail;
    m_vecVerErr.assign(mockDataCap.begin(), mockDataCap.end());
    m_vecVerErr.push_back(static_cast<char>('\0'));
    mockDataCap = mockDataCapHead + mockDataCapBios + mockDataCapsMC + mockDataCapErr1 + mockDataCapTail;
    m_vecCapErr1.assign(mockDataCap.begin(), mockDataCap.end());
    m_vecCapErr1.push_back(static_cast<char>('\0'));
    mockDataCap = mockDataCapHead + mockDataCapBios + mockDataCapsMC + mockDataCapErr2 + mockDataCapTail;
    m_vecCapErr2.assign(mockDataCap.begin(), mockDataCap.end());
    m_vecCapErr2.push_back(static_cast<char>('\0'));
    mockDataCap = mockDataCapHead + mockDataCapBios + mockDataCapsMC + mockDataCapME + mockDataCapErr3;
    m_vecCapErr3.assign(mockDataCap.begin(), mockDataCap.end());
    m_vecCapErr3.push_back(static_cast<char>('\0'));
    mockDataCap = mockDataCapHead + mockDataCapBios + mockDataCapsMC + mockDataCapErr4 + mockDataCapTail;
    m_vecCapErr4.assign(mockDataCap.begin(), mockDataCap.end());
    m_vecCapErr4.push_back(static_cast<char>('\0'));
    mockDataCap = mockDataCapHead + mockDataCapBios + mockDataCapsMC + mockDataCapErr5 + mockDataCapTail;
    m_vecCapErr5.assign(mockDataCap.begin(), mockDataCap.end());
    m_vecCapErr5.push_back(static_cast<char>('\0'));
    mockDataCap = mockDataCapHead + mockDataCapBios + mockDataCapsMC + mockDataCapErr6 + mockDataCapTail;
    m_vecCapErr6.assign(mockDataCap.begin(), mockDataCap.end());
    m_vecCapErr6.push_back(static_cast<char>('\0'));
    mockDataCap = mockDataCapHead + mockDataCapBios + mockDataCapSwap + mockDataCapsMC + mockDataCapTail;
    m_vecCapSwap.assign(mockDataCap.begin(), mockDataCap.end());
    m_vecCapSwap.push_back(static_cast<char>('\0'));
}

bool FATMockFileReader::process(const std::string & FATImageName)
{
    if(FATImageName.find("BIOS_SHA_ERROR") != string::npos)
        m_vecBios.insert(m_vecBios.end(), FATImageName.begin(), FATImageName.end());
    if(FATImageName.find("IFLASH_SHA_ERROR") != string::npos)
        m_vecFlash32.insert(m_vecFlash32.end(), FATImageName.begin(), FATImageName.end());
    if(FATImageName.find("TEMP_SHA_ERROR") != string::npos)
        m_vecFlashT.insert(m_vecFlashT.end(), FATImageName.begin(), FATImageName.end());
    if(FATImageName.find("IPMI_SHA_ERROR") != string::npos)
        m_vecIPMI.insert(m_vecIPMI.end(), FATImageName.begin(), FATImageName.end());
    if(FATImageName.find("ME_SHA_ERROR") != string::npos)
        m_vecME.insert(m_vecME.end(), FATImageName.begin(), FATImageName.end());
    if(FATImageName.find("NSH_SHA_ERROR") != string::npos)
        m_vecStart.insert(m_vecStart.end(), FATImageName.begin(), FATImageName.end());
    if(FATImageName.find("SMC_SHA_ERROR") != string::npos)
        m_vecSMC.insert(m_vecSMC.end(), FATImageName.begin(), FATImageName.end());

    if(FATImageName.find("VERSION_ERROR") != string::npos)
        m_FilesInFat.emplace_back(0, 0, 0, "capsule.cfg", "", getFileID("capsule.cfg"),m_vecVerErr);
    else if(FATImageName.find("NO_BIOS_CAPSULE") != string::npos)
        m_FilesInFat.emplace_back(0, 0, 0, "capsule.cfg", "", getFileID("capsule.cfg"),m_vecCapNoBios);
    else if(FATImageName.find("NO_SMC_CAPSULE") != string::npos)
        m_FilesInFat.emplace_back(0, 0, 0, "capsule.cfg", "", getFileID("capsule.cfg"),m_vecCapNoSMC);
    else if(FATImageName.find("NO_ME_CAPSULE") != string::npos)
        m_FilesInFat.emplace_back(0, 0, 0, "capsule.cfg", "", getFileID("capsule.cfg"),m_vecCapNoME);
    else if(FATImageName.find("CAPSULE_EMPTY") != string::npos)
        m_FilesInFat.emplace_back(0, 0, 0, "capsule.cfg", "", getFileID("capsule.cfg"),m_vecCapEmpty);
    else if(FATImageName.find("TABLE_ERROR1") != string::npos)
        m_FilesInFat.emplace_back(0, 0, 0, "capsule.cfg", "", getFileID("capsule.cfg"),m_vecCapErr1);
    else if(FATImageName.find("TABLE_ERROR2") != string::npos)
        m_FilesInFat.emplace_back(0, 0, 0, "capsule.cfg", "", getFileID("capsule.cfg"),m_vecCapErr2);
    else if(FATImageName.find("TABLE_ERROR3") != string::npos)
        m_FilesInFat.emplace_back(0, 0, 0, "capsule.cfg", "", getFileID("capsule.cfg"),m_vecCapErr3);
    else if(FATImageName.find("TABLE_ERROR4") != string::npos)
        m_FilesInFat.emplace_back(0, 0, 0, "capsule.cfg", "", getFileID("capsule.cfg"),m_vecCapErr4);
    else if(FATImageName.find("TABLE_ERROR5") != string::npos)
        m_FilesInFat.emplace_back(0, 0, 0, "capsule.cfg", "", getFileID("capsule.cfg"),m_vecCapErr5);
    else if(FATImageName.find("TABLE_ERROR6") != string::npos)
        m_FilesInFat.emplace_back(0, 0, 0, "capsule.cfg", "", getFileID("capsule.cfg"),m_vecCapErr6);
    else if(FATImageName.find("TABLE_SWAP") != string::npos)
        m_FilesInFat.emplace_back(0, 0, 0, "capsule.cfg", "", getFileID("capsule.cfg"),m_vecCapSwap);
    else if(FATImageName.find("CAPSULE_ERROR") == string::npos)
        m_FilesInFat.emplace_back(0, 0, 0, "capsule.cfg", "", getFileID("capsule.cfg"),m_vecCap);


    if(FATImageName.find("NO_BIOS_CAPSULE") == string::npos)
        m_FilesInFat.emplace_back(0, 0, 0, "GVPRCRB8.86B.0011.R99.9999999999_LB.cap", "",
                                  getFileID("GVPRCRB8.86B.0011.R99.9999999999_LB.cap"), m_vecBios);
    if(FATImageName.find("NO_SMC_CAPSULE") == string::npos)
    {
        m_FilesInFat.emplace_back(0, 0, 0, "uBMC_OP_KNL_999.99.99999.999_LB_FabA.cap", "",
                                  getFileID("uBMC_OP_KNL_999.99.99999.999_LB_FabA.cap"), m_vecSMC);
        m_FilesInFat.emplace_back(0, 0, 0, "uBMC_OP_KNL_999.99.99999.999_LB_FabB.cap", "",
                                  getFileID("uBMC_OP_KNL_999.99.99999.999_LB_FabB.cap"), m_vecSMC);
    }
    if(FATImageName.find("NO_ME_CAPSULE") == string::npos)
        m_FilesInFat.emplace_back(0, 0, 0, "ME_SPS_PHI_99.99.99.999.9_LB.signed.cap", "",
                                  getFileID("ME_SPS_PHI_99.99.99.999.9_LB.signed.cap"), m_vecME);

    m_FilesInFat.emplace_back(0, 0, 0, "iflash32.efi", "", getFileID("iflash32.efi"),
                              m_vecFlash32);
    m_FilesInFat.emplace_back(0, 0, 0, "iflash32_temp.efi", "",
                              getFileID("iflash32_temp.efi"), m_vecFlashT);
    m_FilesInFat.emplace_back(0, 0, 0, "ipmi.efi", "", getFileID("ipmi.efi"),
                              m_vecIPMI);
    m_FilesInFat.emplace_back(0, 0, 0, "faba.nsh", "", getFileID("faba.nsh"), m_vecStart);
    m_FilesInFat.emplace_back(0, 0, 0, "fabb.nsh", "", getFileID("fabb.nsh"), m_vecStart);
    m_FilesInFat.emplace_back(0, 0, 0, "fabx.nsh", "", getFileID("fabb.nsh"), m_vecStart);

    if(FATImageName.find("EMPTY_ERROR") != string::npos)
        return false;
    if(FATImageName.find("EXTRA_FILE") != string::npos)
        m_FilesInFat.emplace_back(0, 0, 0, "extra.bin", "", getFileID("extra.bin"),m_vecCap);
    return true;
}