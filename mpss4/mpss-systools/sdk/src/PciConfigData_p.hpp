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

#ifndef MICMGMT_PCICONFIGDATA_HPP
#define MICMGMT_PCICONFIGDATA_HPP

//  PROJECT INCLUDES
//
#include    "MicSpeed.hpp"
#include    "PciAddress.hpp"

//  SYSTEM INCLUDES
//
#include    <cstdint>
#include    <string>

// NAMESPACE
//
namespace  micmgmt {


//----------------------------------------------------------------------------
//  STRUCT:  PciConfigData  (PRIVATE)

struct  PciConfigData
{
    PciAddress   mPciAddress;
    MicSpeed     mLinkSpeed;
    uint32_t     mLinkWidth;
    uint32_t     mPayloadSize;
    uint32_t     mRequestSize;
    uint16_t     mVendorId;
    uint16_t     mDeviceId;
    uint16_t     mSubsystemId;
    uint32_t     mClassCode;
    uint8_t      mRevisionId;
    uint8_t      mAspmControl;
    bool         mNoSnoop:1;
    bool         mExtTagEnable:1;
    bool         mRelaxedOrder:1;
    bool         mHasAccess:1;
    bool         mValid:1;

    PciConfigData() :
        mLinkSpeed( 0 ),
        mLinkWidth( 0 ),
        mPayloadSize( 0 ),
        mRequestSize( 0 ),
        mVendorId( 0 ),
        mDeviceId( 0 ),
        mSubsystemId( 0 ),
        mClassCode( 0 ),
        mRevisionId( 0 ),
        mAspmControl( 0 ),
        mNoSnoop( false ),
        mExtTagEnable( false ),
        mRelaxedOrder( false ),
        mHasAccess( false ),
        mValid( false )
    {}

    void  set( const PciConfigData& that )
    {
        mPciAddress   = that.mPciAddress;
        mLinkSpeed    = that.mLinkSpeed;
        mLinkWidth    = that.mLinkWidth;
        mPayloadSize  = that.mPayloadSize;
        mRequestSize  = that.mRequestSize;
        mVendorId     = that.mVendorId;
        mDeviceId     = that.mDeviceId;
        mSubsystemId  = that.mSubsystemId;
        mClassCode    = that.mClassCode;
        mRevisionId   = that.mRevisionId;
        mAspmControl  = that.mAspmControl;
        mNoSnoop      = that.mNoSnoop;
        mExtTagEnable = that.mExtTagEnable;
        mRelaxedOrder = that.mRelaxedOrder;
        mHasAccess    = that.mHasAccess;
        mValid        = that.mValid;
    }

    void  clear()
    {
        mPciAddress.clear();
        mLinkSpeed    = 0;
        mLinkWidth    = 0;
        mPayloadSize  = 0;
        mRequestSize  = 0;
        mVendorId     = 0;
        mDeviceId     = 0;
        mSubsystemId  = 0;
        mClassCode    = 0;
        mRevisionId   = 0;
        mAspmControl  = 0;
        mNoSnoop      = false;
        mExtTagEnable = false;
        mRelaxedOrder = false;
        mHasAccess    = false;
        mValid        = false;
    }
};

//----------------------------------------------------------------------------

}

#endif // MICMGMT_PCICONFIGDATA_HPP
