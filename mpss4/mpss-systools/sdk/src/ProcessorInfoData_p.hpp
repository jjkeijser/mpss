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

#ifndef MICMGMT_PROCESSORINFODATA_HPP
#define MICMGMT_PROCESSORINFODATA_HPP

//  SYSTEM INCLUDES
//
#include    <cstdint>
#include    <string>

// NAMESPACE
//
namespace  micmgmt {


//----------------------------------------------------------------------------
//  STRUCT:  ProcessorInfoData

struct  ProcessorInfoData
{
    MicValueString     mStepping;
    MicValueString     mSku;
    MicValueUInt16     mType;
    MicValueUInt16     mModel;
    MicValueUInt16     mFamily;
    MicValueUInt16     mExtendedModel;
    MicValueUInt16     mExtendedFamily;
    MicValueUInt32     mSteppingData;
    MicValueUInt32     mSubsteppingData;

    ProcessorInfoData()
    {
    }

    void  set( const ProcessorInfoData& that )
    {
        mStepping        = that.mStepping;
        mSku             = that.mSku;
        mType            = that.mType;
        mModel           = that.mModel;
        mFamily          = that.mFamily;
        mExtendedModel   = that.mExtendedModel;
        mExtendedFamily  = that.mExtendedFamily;
        mSteppingData    = that.mSteppingData;
        mSubsteppingData = that.mSubsteppingData;
    }

    void  clear()
    {
        mStepping.reset();
        mSku.reset();
        mType.reset();
        mModel.reset();
        mFamily.reset();
        mExtendedModel.reset();
        mExtendedFamily.reset();
        mSteppingData.reset();
        mSubsteppingData.reset();
    }
};

//----------------------------------------------------------------------------

}

#endif // MICMGMT_PROCESSORINFODATA_HPP
