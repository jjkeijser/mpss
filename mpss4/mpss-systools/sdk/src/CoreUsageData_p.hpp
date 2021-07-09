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

#ifndef MICMGMT_COREUSAGEDATA_HPP
#define MICMGMT_COREUSAGEDATA_HPP

//  PROJECT INCLUDES
//
#include    "MicCoreUsageInfo.hpp"
#include    "MicFrequency.hpp"

//  SYSTEM INCLUDES
//
#include    <cstdint>

namespace{
    const size_t NCOUNTERS = 5;
}

// NAMESPACE
//
namespace  micmgmt {


//----------------------------------------------------------------------------
//  STRUCT:  CoreUsageData

struct  CoreUsageData
{
    typedef MicCoreUsageInfo::Counters  Counters;   // Vector of counts

    size_t         mCoreCount;
    size_t         mCoreThreadCount;
    MicFrequency   mFrequency;
    uint64_t       mTickCount;
    uint64_t       mTicksPerSecond;
    uint64_t       mCounterTotal[NCOUNTERS];    // One for each counter
    Counters*      mpCounters[NCOUNTERS];       // Ditto
    bool           mCleanup[NCOUNTERS];         // Ditto
    bool           mValid;

    CoreUsageData() :
        mCoreCount( 0 ),
        mCoreThreadCount( 0 ),
        mFrequency( 0 ),
        mTickCount( 0 ),
        mTicksPerSecond( 0 ),
        mValid( false )
    {
        for (size_t i=0; i<NCOUNTERS; i++)
        {
            mCounterTotal[i] = 0;
            mpCounters[i] = 0;
            mCleanup[i] = true;         // Assume, we clean up
        }
    }
};

//----------------------------------------------------------------------------

}

#endif // MICMGMT_COREUSAGEDATA_HPP
