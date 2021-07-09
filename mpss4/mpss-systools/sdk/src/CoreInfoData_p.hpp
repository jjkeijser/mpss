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

#ifndef MICMGMT_COREINFODATA_HPP
#define MICMGMT_COREINFODATA_HPP

//  PROJECT INCLUDES
//
#include    "MicValue.hpp"
#include    "MicVoltage.hpp"
#include    "MicFrequency.hpp"

//  SYSTEM INCLUDES
//
#include    <cstdint>

// NAMESPACE
//
namespace  micmgmt {


//----------------------------------------------------------------------------
//  STRUCT:  CoreInfoData

struct  CoreInfoData
{
    MicValueUInt64        mCoreCount;
    MicValueUInt64        mCoreThreadCount;
    MicFrequency          mFrequency;
    MicVoltage            mVoltage;

    CoreInfoData()
    {
    }

    void  set( const CoreInfoData& that )
    {
        mCoreCount       = that.mCoreCount;
        mCoreThreadCount = that.mCoreThreadCount;
        mVoltage         = that.mVoltage;
        mFrequency       = that.mFrequency;
    }

    void  clear()
    {
        mVoltage.clearValue();
        mFrequency.clearValue();
        mCoreCount.reset();
        mCoreThreadCount.reset();
    }
};

//----------------------------------------------------------------------------

}

#endif // MICMGMT_COREINFODATA_HPP
