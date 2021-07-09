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

#ifndef MICMGMT_KNLMOCKSTATECHANGEDEVICE_HPP
#define MICMGMT_KNLMOCKSTATECHANGEDEVICE_HPP

// PROJECT INCLUDES
//
#include    "KnlMockDevice.hpp"

//============================================================================
//  CLASS:  KnlMockDevice2.hpp
//

namespace micmgmt
{

// FORWARD DECLARATIONS
//
class MicBootConfigInfo;
class MicPowerUsageInfo;
class MicThermalInfo;

class  KnlMockDevice2 : public KnlMockDevice
{
public:
    explicit KnlMockDevice2( int number = 0 );
    virtual ~KnlMockDevice2();

protected: // From KnlMockDevice
    virtual uint32_t   getDeviceCoreInfo( MicCoreInfo* info ) const;
    virtual uint32_t   getDeviceCoreUsageInfo( MicCoreUsageInfo* info ) const;
    virtual uint32_t   getDeviceState( MicDevice::DeviceState* state ) const;
    virtual uint32_t   getDevicePowerUsageInfo( MicPowerUsageInfo* info ) const;
    virtual uint32_t   getDeviceThermalInfo( MicThermalInfo* info ) const;
    virtual uint32_t   deviceBoot( const MicBootConfigInfo& info );
    virtual uint32_t   deviceReset( bool force );
};

}// namespace micmgmt

#endif // MICMGMT_KNLMOCKSTATECHANGEDEVICE_HPP
