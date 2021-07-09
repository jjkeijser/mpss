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

#ifndef MICMGMT_POWERUSAGEDATA_HPP
#define MICMGMT_POWERUSAGEDATA_HPP

//  PROJECT INCLUDES
//
#include    "MicPower.hpp"
#include    "ThrottleInfo.hpp"

//  SYSTEM INCLUDES
//
#include    <cstdint>
#include    <vector>

// NAMESPACE
//
namespace  micmgmt {


//----------------------------------------------------------------------------
//  STRUCT:  PowerUsageData

struct  PowerUsageData
{
    std::vector<MicPower>  mSensors;
    MicPower               mThrottle;
    ThrottleInfo           mThrottleInfo;
    bool                   mValid;

    PowerUsageData() :
        mValid( false )
    {}

    void  set( const PowerUsageData& that )
    {
        mSensors      = that.mSensors;
        mThrottle     = that.mThrottle;
        mThrottleInfo = that.mThrottleInfo;
        mValid        = that.mValid;
    }
};

//----------------------------------------------------------------------------

}

#endif // MICMGMT_POWERUSAGEDATA_HPP
