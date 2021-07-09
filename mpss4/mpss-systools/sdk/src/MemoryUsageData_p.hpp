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

#ifndef MICMGMT_MEMORYUSAGEDATA_HPP
#define MICMGMT_MEMORYUSAGEDATA_HPP

//  PROJECT INCLUDES
//
#include    <MicMemory.hpp>

//  SYSTEM INCLUDES
//
#include    <cstdint>

// NAMESPACE
//
namespace  micmgmt {


//----------------------------------------------------------------------------
//  STRUCT:  MemoryUsageData

struct  MemoryUsageData
{
    MicMemory  mTotal;
    MicMemory  mUsed;
    MicMemory  mFree;
    MicMemory  mBuffers;
    MicMemory  mCached;
    bool       mValid:1;

    MemoryUsageData() :
        mValid( false )
    {}

    void  set( const MemoryUsageData& that )
    {
        mTotal     = that.mTotal;
        mUsed      = that.mUsed;
        mFree      = that.mFree;
        mBuffers   = that.mBuffers;
        mCached    = that.mCached;
        mValid     = that.mValid;
    }

    void  clear()
    {
        mTotal.clearValue();
        mUsed.clearValue();
        mFree.clearValue();
        mBuffers.clearValue();
        mCached.clearValue();

        mValid     = false;
    }
};

//----------------------------------------------------------------------------

}

#endif // MICMGMT_MEMORYUSAGEDATA_HPP
