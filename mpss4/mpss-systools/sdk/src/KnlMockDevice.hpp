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

#ifndef MICMGMT_KNLMOCKDEVICE_HPP
#define MICMGMT_KNLMOCKDEVICE_HPP

// PROJECT INCLUDES
//
#include    "KnlDeviceBase.hpp"

// FORWARD DECLARATIONS
//
namespace  micmgmt {
class   FlashStatus;
class   FlashDeviceInfo;
struct  PciConfigData;
struct  PlatformInfoData;
struct  ProcessorInfoData;
struct  PowerThresholdData;
struct  VersionInfoData;
struct  MemoryInfoData;
struct  CoreInfoData;
struct  ThermalInfoData;
struct  VoltageInfoData;
struct  CoreUsageData;
struct  PowerUsageData;
struct  MemoryUsageData;
}

// NAMESPACE
//
namespace  micmgmt
{
    struct FwUpdateStatus
    {
        bool bios;
        bool me;
        bool smc;
        int lastStagenum;
        FwUpdateStatus() : bios(false), me(false), smc(false), lastStagenum(0)
        {
        }
    };

//============================================================================
//  CLASS:  KnlMockDevice

class  KnlMockDevice : public KnlDeviceBase
{

public:

    explicit KnlMockDevice( int number=0 );
    virtual ~KnlMockDevice();

    void                mockReset();
    void                setAdmin( bool state );
    void                setInitialDeviceState( MicDevice::DeviceState state );


protected:

    virtual bool        isDeviceOpen() const;
    virtual uint32_t    getDeviceState( MicDevice::DeviceState* state ) const;
    virtual uint32_t    getDevicePciConfigInfo( MicPciConfigInfo* info ) const;
    virtual uint32_t    getDeviceProcessorInfo( MicProcessorInfo* info ) const;
    virtual uint32_t    getDeviceVersionInfo( MicVersionInfo* info ) const;
    virtual uint32_t    getDeviceMemoryInfo( MicMemoryInfo* info ) const;
    virtual uint32_t    getDeviceCoreInfo( MicCoreInfo* info ) const;
    virtual uint32_t    getDevicePlatformInfo( MicPlatformInfo* info ) const;
    virtual uint32_t    getDeviceThermalInfo( MicThermalInfo* info ) const;
    virtual uint32_t    getDeviceVoltageInfo( MicVoltageInfo* info) const;
    virtual uint32_t    getDeviceCoreUsageInfo( MicCoreUsageInfo* info ) const;
    virtual uint32_t    getDevicePowerUsageInfo( MicPowerUsageInfo* info ) const;
    virtual uint32_t    getDevicePowerThresholdInfo( MicPowerThresholdInfo* info ) const;
    virtual uint32_t    getDeviceMemoryUsageInfo( MicMemoryUsageInfo* info ) const;
    virtual uint32_t    getDevicePostCode( std::string* code ) const;
    virtual uint32_t    getDeviceLedMode( uint32_t* mode ) const;
    virtual uint32_t    getDeviceEccMode( bool* enabled, bool* available=0 ) const;
    virtual uint32_t    getDeviceTurboMode( bool* enabled, bool* available=0, bool* active=0 ) const;
    virtual uint32_t    getDeviceSmBusAddressTrainingStatus( bool* busy, int* eta=0 ) const;
    virtual uint32_t    getDeviceSmcPersistenceState( bool* enabled, bool* available=0 ) const;
    virtual uint32_t    getDeviceSmcRegisterData( uint8_t offset, uint8_t* data, size_t* size ) const;
    virtual uint32_t    getDeviceFlashUpdateStatus( FlashStatus* status ) const;

    virtual uint32_t    deviceOpen();
    virtual void        deviceClose();
    virtual uint32_t    deviceBoot( const MicBootConfigInfo& info );
    virtual uint32_t    deviceReset( bool force );
    virtual uint32_t    setDeviceLedMode( uint32_t mode );
    virtual uint32_t    setDeviceTurboModeEnabled( bool state );
    virtual uint32_t    setDeviceSmcPersistenceEnabled( bool state );
    virtual uint32_t    setDeviceSmcRegisterData( uint8_t offset, const uint8_t* data, size_t size );
    virtual uint32_t    setDevicePowerThreshold( MicDevice::PowerWindow window, uint32_t power, uint32_t time );
    virtual uint32_t    startDeviceSmBusAddressTraining( int hint, int timeout );
    virtual uint32_t    startDeviceFlashUpdate( FlashImage* image, bool force );
    virtual uint32_t    startDeviceFlashUpdate( std::string & path, std::unique_ptr<micmgmt::FwToUpdate> & fwToUpdate );
    virtual uint32_t    finalizeDeviceFlashUpdate( bool abort );


protected:

    std::unique_ptr<ProcessorInfoData>   m_pProcessorInfoData;
    std::unique_ptr<PciConfigData>       m_pPciConfigInfoData;
    std::unique_ptr<PlatformInfoData>    m_pPlatformInfoData;
    std::unique_ptr<PowerThresholdData>  m_pPowerThresholdData;
    std::unique_ptr<VersionInfoData>     m_pVersionInfoData;
    std::unique_ptr<MemoryInfoData>      m_pMemoryInfoData;
    std::unique_ptr<CoreInfoData>        m_pCoreInfoData;
    std::unique_ptr<ThermalInfoData>     m_pThermalInfoData;
    std::unique_ptr<VoltageInfoData>     m_pVoltageInfoData;
    std::unique_ptr<PowerUsageData>      m_pPowerUsageData;
    std::unique_ptr<MemoryUsageData>     m_pMemoryUsageData;
    std::unique_ptr<MicCoreUsageInfo>    m_pCoreUsageInfo;
    std::unique_ptr<FlashStatus>         m_pFlashStatus;
    mutable MicDevice::DeviceState       m_DeviceState;
    mutable FwUpdateStatus               m_completed;
    FwUpdateStatus                       m_fwToUpdate;
    uint32_t                             m_LedMode;
    bool                                 m_DevOpen;
    bool                                 m_IsAdmin;
    bool                                 m_TurboAvailable;
    bool                                 m_TurboEnabled;
    bool                                 m_TurboActive;
    bool                                 m_SmcPersist;
    bool                                 m_EccMode;


private:     // DISABLE
    KnlMockDevice( const KnlMockDevice& );
    KnlMockDevice&  operator = ( const KnlMockDevice& );

};

//----------------------------------------------------------------------------

}   // namespace micmgmt

//----------------------------------------------------------------------------

#endif // MICMGMT_KNLMOCKDEVICE_HPP
