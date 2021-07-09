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

#ifndef MICMGMT_VOLTAGEINFODATA_HPP
#define MICMGMT_VOLTAGEINFODATA_HPP

//  PROJECT INCLUDES
//
#include    "MicVoltage.hpp"

//  SYSTEM INCLUDES
//
#include    <cstdint>
#include    <vector>

// NAMESPACE
//
namespace  micmgmt {


//----------------------------------------------------------------------------
//  STRUCT:  VoltageInfoData

struct  VoltageInfoData
{
    std::vector<MicVoltage>  mSensors;
    bool                     mValid;

    VoltageInfoData() :
        mValid( false )
    {}

    void  set( const VoltageInfoData& that )
    {
        mSensors = that.mSensors;
        mValid   = that.mValid;
    }
};

//----------------------------------------------------------------------------

}

#endif // MICMGMT_VOLTAGEINFODATA_HPP
