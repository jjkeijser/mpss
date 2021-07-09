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

#ifndef MICMGMT_POWERTHRESHOLDDATA_HPP
#define MICMGMT_POWERTHRESHOLDDATA_HPP

//  PROJECT INCLUDES
//
#include    "MicPower.hpp"

// NAMESPACE
//
namespace  micmgmt {


//----------------------------------------------------------------------------
//  STRUCT:  PowerThresholdData

struct  PowerThresholdData
{
    MicPower  mLoThreshold;
    MicPower  mHiThreshold;
    MicPower  mMaximum;
    MicPower  mWindow0Threshold;
    MicPower  mWindow1Threshold;
    uint32_t  mWindow0Time;
    uint32_t  mWindow1Time;
    bool      mPersistence;
    bool      mValid;

    PowerThresholdData() :
        mWindow0Time( 0 ),
        mWindow1Time( 0 ),
        mPersistence( false ),
        mValid( false )
    {}

    void  set( const PowerThresholdData& that )
    {
        mLoThreshold      = that.mLoThreshold;
        mHiThreshold      = that.mHiThreshold;
        mMaximum          = that.mMaximum;
        mWindow0Threshold = that.mWindow0Threshold;
        mWindow1Threshold = that.mWindow1Threshold;
        mWindow0Time      = that.mWindow0Time;
        mWindow1Time      = that.mWindow1Time;
        mPersistence      = that.mPersistence;
        mValid            = that.mValid;
    }
};

//----------------------------------------------------------------------------

}

#endif // MICMGMT_POWERTHRESHOLDDATA_HPP
