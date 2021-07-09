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

#ifndef MICMGMT_PLATFORMINFODATA_HPP
#define MICMGMT_PLATFORMINFODATA_HPP

//  SYSTEM INCLUDES
//
#include    <cstdint>
#include    <cstring>
#include    <string>

//  PROJECT INCLUDES
//
#include    "MicPower.hpp"
#include    "MicValue.hpp"

// NAMESPACE
//
namespace  micmgmt {


//----------------------------------------------------------------------------
//  STRUCT:  PlatformInfoData

struct  PlatformInfoData
{
    MicValueString      mPartNumber;
    MicValueString      mSerialNo;
    MicValueString      mFeatureSet;
    MicValueString      mCoprocessorOs;
    MicValueString      mCoprocessorBrand;
    MicValueString      mBoardSku;
    MicValueString      mBoardType;
    MicValueString      mStrapInfo;
    MicValueUInt32      mStrapInfoRaw;
    MicValueString      mManufactDate;
    MicValueString      mManufactTime;
    MicValueUInt8       mSmBusAddress;
    MicValueString      mUuid;
    MicPower            mMaxPower;
    MicValueBool        mCoolActive;

    void  set( const PlatformInfoData& that )
    {
        mPartNumber       = that.mPartNumber;
        mSerialNo         = that.mSerialNo;
        mFeatureSet       = that.mFeatureSet;
        mCoprocessorOs    = that.mCoprocessorOs;
        mCoprocessorBrand = that.mCoprocessorBrand;
        mBoardSku         = that.mBoardSku;
        mBoardType        = that.mBoardType;
        mStrapInfo        = that.mStrapInfo;
        mStrapInfoRaw     = that.mStrapInfoRaw;
        mManufactDate     = that.mManufactDate;
        mManufactTime     = that.mManufactTime;
        mSmBusAddress     = that.mSmBusAddress;
        mUuid             = that.mUuid;
        mMaxPower         = that.mMaxPower;
        mCoolActive       = that.mCoolActive;
    }

    void  clear()
    {
        mPartNumber.reset();
        mSerialNo.reset();
        mFeatureSet.reset();
        mCoprocessorOs.reset();
        mCoprocessorBrand.reset();
        mBoardSku.reset();
        mBoardType.reset();
        mStrapInfo.reset();
        mManufactDate.reset();
        mManufactTime.reset();
        mUuid.reset();

        mStrapInfoRaw.reset();
        mSmBusAddress.reset();

        mCoolActive.reset();

        mMaxPower.clearValue();
    }
};

//----------------------------------------------------------------------------

}

#endif // MICMGMT_PLATFORMINFODATA_HPP
