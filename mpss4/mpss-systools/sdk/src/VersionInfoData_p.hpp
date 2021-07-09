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

#ifndef MICMGMT_VERSIONINFODATA_HPP
#define MICMGMT_VERSIONINFODATA_HPP

//  SYSTEM INCLUDES
//
#include    <string>

//  PROJECT INCLUDES
//
#include    "MicValue.hpp"

// NAMESPACE
//
namespace  micmgmt {


//----------------------------------------------------------------------------
//  STRUCT:  VersionInfoData

struct  VersionInfoData
{
    MicValueString  mFlashVersion;
    MicValueString  mBiosVersion;
    MicValueString  mBiosRelDate;
    MicValueString  mOemStrings;
    MicValueString  mPlxVersion;
    MicValueString  mFabVersion;
    MicValueString  mSmcVersion;
    MicValueString  mSmcBootVersion;
    MicValueString  mRamCtrlVersion;
    MicValueString  mMeVersion;
    MicValueString  mMpssVersion;
    MicValueString  mDriverVersion;

    void  set( const VersionInfoData& that )
    {
        mFlashVersion   = that.mFlashVersion;
        mBiosVersion    = that.mBiosVersion;
        mBiosRelDate    = that.mBiosRelDate;
        mOemStrings     = that.mOemStrings;
        mPlxVersion     = that.mPlxVersion;
        mFabVersion     = that.mFabVersion;
        mSmcVersion     = that.mSmcVersion;
        mSmcBootVersion = that.mSmcBootVersion;
        mRamCtrlVersion = that.mRamCtrlVersion;
        mMeVersion      = that.mMeVersion;
        mMpssVersion    = that.mMpssVersion;
        mDriverVersion  = that.mDriverVersion;
    }

    void  clear()
    {
        mFlashVersion.reset();
        mBiosVersion.reset();
        mBiosRelDate.reset();
        mOemStrings.reset();
        mPlxVersion.reset();
        mFabVersion.reset();
        mSmcVersion.reset();
        mSmcBootVersion.reset();
        mRamCtrlVersion.reset();
        mMeVersion.reset();
        mMpssVersion.reset();
        mDriverVersion.reset();
    }
};

//----------------------------------------------------------------------------

}

#endif // MICMGMT_VERSIONINFODATA_HPP
