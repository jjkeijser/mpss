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

#ifndef MICMGMT_MEMORYINFODATA_HPP
#define MICMGMT_MEMORYINFODATA_HPP

//  PROJECT INCLUDES
//
#include    "MicFrequency.hpp"
#include    "MicMemory.hpp"
#include    "MicSpeed.hpp"

//  SYSTEM INCLUDES
//
#include    <cstdint>
#include    <string>

// NAMESPACE
//
namespace  micmgmt {


//----------------------------------------------------------------------------
//  STRUCT:  MemoryInfoData

struct  MemoryInfoData
{
    std::string   mVendorName;
    std::string   mMemoryType;
    std::string   mTechnology;
    MicSpeed      mSpeed;
    MicFrequency  mFrequency;
    MicMemory     mDensity;
    MicMemory     mSize;
    uint16_t      mRevision;
    bool          mEcc:1;
    bool          mValid:1;

    MemoryInfoData() :
        mRevision( 0 ),
        mEcc( false ),
        mValid( false )
    {}

    void  set( const MemoryInfoData& that )
    {
        mVendorName = that.mVendorName;
        mMemoryType = that.mMemoryType;
        mTechnology = that.mTechnology;
        mDensity    = that.mDensity;
        mSize       = that.mSize;
        mSpeed      = that.mSpeed;
        mFrequency  = that.mFrequency;
        mRevision   = that.mRevision;
        mEcc        = that.mEcc;
        mValid      = that.mValid;
    }

    void  clear()
    {
        mVendorName.clear();
        mMemoryType.clear();
        mTechnology.clear();
        mSpeed.clearValue();
        mFrequency.clearValue();
        mDensity.clearValue();
        mSize.clearValue();

        mRevision  = 0;
        mEcc       = false;
        mValid     = false;
    }
};

//----------------------------------------------------------------------------

}

#endif // MICMGMT_MEMORYINFODATA_HPP
