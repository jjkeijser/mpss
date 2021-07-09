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

#ifndef MICMGMT_MICDEVICEIMPL_HPP
#define MICMGMT_MICDEVICEIMPL_HPP

// PROJECT INCLUDES
//
#include    "MicDevice.hpp"

// NAMESPACE
//
namespace  micmgmt
{


//============================================================================
//  CLASS:  MicDeviceImpl

class  MicDeviceImpl        // Abstract Interface
{

public:

    enum  AccessMode   { eNoAccess, eReadOnly, eWriteOnly, eReadWrite };


public:

    virtual ~MicDeviceImpl();

    virtual bool             isDeviceOpen() const = 0;
    virtual std::string      deviceType() const = 0;
    int                      deviceNum() const;

    virtual uint32_t         getDeviceState( MicDevice::DeviceState* state ) const = 0;
    virtual uint32_t         getDevicePciConfigInfo( MicPciConfigInfo* info ) const = 0;
    virtual uint32_t         getDeviceProcessorInfo( MicProcessorInfo* info ) const = 0;
    virtual uint32_t         getDeviceVersionInfo( MicVersionInfo* info ) const = 0;
    virtual uint32_t         getDeviceMemoryInfo( MicMemoryInfo* info ) const = 0;
    virtual uint32_t         getDeviceCoreInfo( MicCoreInfo* info ) const = 0;
    virtual uint32_t         getDevicePlatformInfo( MicPlatformInfo* info ) const = 0;
    virtual uint32_t         getDeviceThermalInfo( MicThermalInfo* info ) const = 0;
    virtual uint32_t         getDeviceVoltageInfo( MicVoltageInfo* info) const = 0;
    virtual uint32_t         getDeviceCoreUsageInfo( MicCoreUsageInfo* info ) const = 0;
    virtual uint32_t         getDevicePowerUsageInfo( MicPowerUsageInfo* info ) const = 0;
    virtual uint32_t         getDevicePowerThresholdInfo( MicPowerThresholdInfo* info ) const = 0;
    virtual uint32_t         getDeviceMemoryUsageInfo( MicMemoryUsageInfo* info ) const = 0;
    virtual uint32_t         getDevicePostCode( std::string* code ) const = 0;
    virtual uint32_t         getDeviceLedMode( uint32_t* mode ) const = 0;
    virtual uint32_t         getDeviceEccMode( bool* enabled, bool* available=0 ) const = 0;
    virtual uint32_t         getDeviceTurboMode( bool* enabled, bool* available=0, bool* active=0 ) const = 0;
    virtual uint32_t         getDeviceMaintenanceMode( bool* enabled, bool* available=0 ) const = 0;
    virtual uint32_t         getDeviceSmBusAddressTrainingStatus( bool* busy, int* eta=0 ) const = 0;
    virtual uint32_t         getDeviceSmcPersistenceState( bool* enabled, bool* available=0 ) const = 0;
    virtual uint32_t         getDeviceSmcRegisterData( uint8_t offset, uint8_t* data, size_t* size ) const = 0;
    virtual uint32_t         getDevicePowerStates( std::list<MicPowerState>* states ) const = 0;
    virtual uint32_t         getDeviceFlashDeviceInfo( FlashDeviceInfo* info ) const = 0;
    virtual uint32_t         getDeviceFlashUpdateStatus( FlashStatus* status ) const = 0;

    virtual uint32_t         deviceOpen() = 0;
    virtual void             deviceClose() = 0;
    virtual uint32_t         deviceBoot( const MicBootConfigInfo& info ) = 0;
    virtual uint32_t         deviceReset( bool force ) = 0;
    virtual uint32_t         setDeviceLedMode( uint32_t mode ) = 0;
    virtual uint32_t         setDeviceTurboModeEnabled( bool state ) = 0;
    virtual uint32_t         setDeviceEccModeEnabled( bool state, int timeout=0, int* stage=0 ) = 0;
    virtual uint32_t         setDeviceMaintenanceModeEnabled( bool state, int timeout=0 ) = 0;
    virtual uint32_t         setDeviceSmcPersistenceEnabled( bool state ) = 0;
    virtual uint32_t         setDeviceSmcRegisterData( uint8_t offset, const uint8_t* data, size_t size ) = 0;
    virtual uint32_t         setDevicePowerThreshold( MicDevice::PowerWindow window, uint16_t power, uint16_t time ) = 0;
    virtual uint32_t         setDevicePowerThreshold( MicDevice::PowerWindow window, uint32_t power, uint32_t time ) = 0;
    virtual uint32_t         setDevicePowerStates( const std::list<MicPowerState>& states ) = 0;
    virtual uint32_t         startDeviceSmBusAddressTraining( int hint, int timeout ) = 0;
    virtual uint32_t         startDeviceFlashUpdate( FlashImage* image, bool force ) = 0;
    virtual uint32_t         startDeviceFlashUpdate( std::string & path, std::unique_ptr<micmgmt::FwToUpdate> & fwToUpdate ) = 0;
    virtual uint32_t         finalizeDeviceFlashUpdate( bool abort ) = 0;


protected:

    MicDeviceImpl( int number=0 );


private:

    int  m_Number;


private: // DISABLE

    MicDeviceImpl( const MicDeviceImpl& );
    MicDeviceImpl&  operator = ( const MicDeviceImpl& );

};

//----------------------------------------------------------------------------

}   // namespace micmgmt

//----------------------------------------------------------------------------

#endif // MICMGMT_MICDEVICEIMPL_HPP
