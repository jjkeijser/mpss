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

#ifndef MICMGMT_THERMALINFODATA_HPP
#define MICMGMT_THERMALINFODATA_HPP

//  PROJECT INCLUDES
//
#include    "MicTemperature.hpp"
#include    "MicValue.hpp"
#include    "ThrottleInfo.hpp"

//  SYSTEM INCLUDES
//
#include    <cstdint>
#include    <vector>

// NAMESPACE
//
namespace  micmgmt {


//----------------------------------------------------------------------------
//  STRUCT:  ThermalInfoData

struct  ThermalInfoData
{
    std::vector<MicTemperature>  mSensors;
    MicTemperature               mCritical;         // Throttle start threshold
    MicTemperature               mControl;          // Fan activation threshold
    MicValueUInt32               mFanRpm;
    MicValueUInt32               mFanPwm;
    MicValueUInt32               mFanAdder;
    ThrottleInfo                 mThrottleInfo;
    bool                         mValid;

    ThermalInfoData() :
        mValid(false)
    {}

    void  set( const ThermalInfoData& that )
    {
        mSensors      = that.mSensors;
        mCritical     = that.mCritical;
        mControl      = that.mControl;
        mFanRpm       = that.mFanRpm;
        mFanPwm       = that.mFanPwm;
        mFanAdder     = that.mFanAdder;
        mThrottleInfo = that.mThrottleInfo;
        mValid        = that.mValid;
    }
};

//----------------------------------------------------------------------------

}

#endif // MICMGMT_THERMALINFODATA_HPP
