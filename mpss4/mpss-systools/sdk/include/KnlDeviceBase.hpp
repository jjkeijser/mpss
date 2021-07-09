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

#ifndef MICMGMT_KNLDEVICEBASE_HPP
#define MICMGMT_KNLDEVICEBASE_HPP

// PROJECT INCLUDES
//
#include    "MicDeviceImpl.hpp"

#ifdef UNIT_TESTS
#define PROTECTED public
#else
#define PROTECTED protected
#endif // UNIT_TESTS

// NAMESPACE
//
namespace  micmgmt
{

class KnlDeviceEnums
{
public:

    enum  SmBusAddress  { eSmBusAddress_0x30, eSmBusAddress_0x40 };
    enum  FlashStage   { eIdleStage = 0, eBiosStage, eMeStage, eMcdramStage,
                         eSmcStage, ePlxStage, eNextStage = 14, eErrorStage = 15 };

PROTECTED:

    enum  TempSensor   { eCpuTemp,  eExhaustTemp, eVccpTemp, eVccclrTemp,
                         eVccmpTemp, eWestTemp, eEastTemp };
    enum  VoltSensor   { eVccpVolt, eVccuVolt, eVccclrVolt, eVccmlbVolt,
                         eVccmpVolt, eNtb1Volt, eVccpioVolt, eVccsfrVolt,
                         ePchVolt, eVccmfuseVolt,
                         eNtb2Volt, eVppVolt };
    enum  PowerSensor  { ePciePower, e2x3Power, e2x4Power, eAvg0Power, eCurrentPower,
                         eMaximumPower, eVccpPower, eVccuPower,
                         eVccclrPower, eVccmlbPower, eVccmpPower,
                         eNtb1Power };
};


//============================================================================
//  CLASS:  KnlDeviceBase

class  KnlDeviceBase : public MicDeviceImpl, public KnlDeviceEnums
{

public:

   virtual ~KnlDeviceBase();



protected:

    explicit KnlDeviceBase( int number=0 );

    virtual std::string  deviceType() const;
    virtual uint32_t     getDeviceMaintenanceMode( bool* enabled, bool* available ) const;
    virtual uint32_t     getDevicePowerStates( std::list<MicPowerState>* states ) const;
    virtual uint32_t     getDeviceSmcPersistenceState( bool* enabled, bool* available=0 ) const;
    virtual uint32_t     getDeviceFlashDeviceInfo( FlashDeviceInfo* info ) const;
    virtual int          smcRegisterSize( uint8_t offset, AccessMode* access=0 ) const;

    virtual uint32_t     setDeviceEccModeEnabled( bool state, int timeout=0, int* stage=0 );
    virtual uint32_t     setDeviceMaintenanceModeEnabled( bool state, int timeout );
    virtual uint32_t     setDevicePowerStates( const std::list<MicPowerState>& states );
    virtual uint32_t     setDeviceSmcPersistenceEnabled( bool state );
    virtual uint32_t     setDevicePowerThreshold( MicDevice::PowerWindow window, uint16_t power, uint16_t time );


PROTECTED: // STATIC

    static std::string   flashStageAsText( int stage );
    static std::string   tempSensorAsText( TempSensor sensor );
    static std::string   voltSensorAsText( VoltSensor sensor );
    static std::string   powerSensorAsText( PowerSensor sensor );
    static std::string   flashErrorAsText( int stage, int code );


private:    // DISABLE

    KnlDeviceBase( const KnlDeviceBase& );
    KnlDeviceBase&  operator = ( const KnlDeviceBase& );

};

//----------------------------------------------------------------------------

}   // namespace micmgmt

//----------------------------------------------------------------------------

#endif // MICMGMT_KNLDEVICEBASE_HPP
