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

#ifndef MICMGMT_MICDEVICE_HPP
#define MICMGMT_MICDEVICE_HPP

// SYSTEM INCLUDES
//
#include    <cstdint>
#include    <string>
#include    <memory>
#include    <list>

// NAMESPACE
//
namespace  micmgmt
{

// FORWARD DECLARATIONS
//
class  FlashImage;
class  FlashStatus;
class  FlashDeviceInfo;
class  MicDeviceInfo;
class  MicDeviceDetails;
class  MicProcessorInfo;
class  MicVersionInfo;
class  MicMemoryInfo;
class  MicCoreInfo;
class  MicPciConfigInfo;
class  MicPlatformInfo;
class  MicThermalInfo;
class  MicVoltageInfo;
class  MicPowerThresholdInfo;
class  MicPowerUsageInfo;
class  MicCoreUsageInfo;
class  MicMemoryUsageInfo;
class  MicPowerState;
class  MicBootConfigInfo;
class  MicDeviceImpl;
struct FwToUpdate;


//============================================================================
//  CLASS:  MicDevice

class  MicDevice
{

public:

    enum  DeviceState  { eOffline, eOnline, eReady, eReset, eReboot, eLost, eBootingFW, eOnlineFW, eError };
    enum  PowerWindow  { eWindow0, eWindow1 };


public:

   ~MicDevice();

    bool                     isOpen() const;
    bool                     isOnline() const;
    bool                     isReady() const;
    bool                     isCompatibleFlashImage( const FlashImage& image ) const;
    int                      deviceNum() const;
    std::string              deviceName() const;
    std::string              deviceType() const;
    const MicDeviceInfo&     deviceInfo() const;
    const MicDeviceDetails&  deviceDetails() const;
    DeviceState              deviceState() const;

    uint32_t                 getProcessorInfo( MicProcessorInfo* info ) const;
    uint32_t                 getPciConfigInfo( MicPciConfigInfo* info ) const;
    uint32_t                 getVersionInfo( MicVersionInfo* info ) const;
    uint32_t                 getMemoryInfo( MicMemoryInfo* info ) const;
    uint32_t                 getCoreInfo( MicCoreInfo* info ) const;
    uint32_t                 getPlatformInfo( MicPlatformInfo* info ) const;
    uint32_t                 getThermalInfo( MicThermalInfo* info ) const;
    uint32_t                 getVoltageInfo( MicVoltageInfo* info ) const;
    uint32_t                 getCoreUsageInfo( MicCoreUsageInfo* info ) const;
    uint32_t                 getPowerUsageInfo( MicPowerUsageInfo* info ) const;
    uint32_t                 getPowerThresholdInfo( MicPowerThresholdInfo* info ) const;
    uint32_t                 getMemoryUsageInfo( MicMemoryUsageInfo* info ) const;
    uint32_t                 getPostCode( std::string* code ) const;
    uint32_t                 getLedMode( uint32_t* mode ) const;
    uint32_t                 getEccMode( bool* enabled, bool* available=0 ) const;
    uint32_t                 getTurboMode( bool* enabled, bool* available=0, bool* active=0 ) const;
    uint32_t                 getMaintenanceMode( bool* enabled, bool* available=0 ) const;
    uint32_t                 getSmBusAddressTrainingStatus( bool* busy, int* eta=0 ) const;
    uint32_t                 getSmcPersistenceState( bool* enabled, bool* available=0 ) const;
    uint32_t                 getSmcRegisterData( uint8_t offset, uint8_t* data, size_t* size ) const;
    uint32_t                 getPowerStates( std::list<MicPowerState>* states ) const;
    uint32_t                 getFlashDeviceInfo( FlashDeviceInfo* info ) const;
    uint32_t                 getFlashUpdateStatus( FlashStatus* status ) const;

    uint32_t                 open();
    void                     close();
    uint32_t                 boot( const MicBootConfigInfo& info );
    uint32_t                 reset( bool force=false );
    uint32_t                 setLedMode( uint32_t mode );
    uint32_t                 setTurboModeEnabled( bool state );
    uint32_t                 setEccModeEnabled( bool state, int timeout=0, int* stage=0 );
    uint32_t                 setMaintenanceModeEnabled( bool state, int timeout=0 );
    uint32_t                 setSmcPersistenceEnabled( bool state );
    uint32_t                 setSmcRegisterData( uint8_t offset, const uint8_t* data, size_t size );
    uint32_t                 setPowerThreshold( PowerWindow window, uint32_t power, uint32_t time );
    uint32_t                 setPowerStates( const std::list<MicPowerState>& states );
    uint32_t                 startSmBusAddressTraining( int hint, int timeout=5000 );
    uint32_t                 startFlashUpdate( FlashImage* image, bool force=false );
    uint32_t                 startFlashUpdate( std::string & mPath, std::unique_ptr<micmgmt::FwToUpdate> & mFwToUpdate );
    uint32_t                 finalizeFlashUpdate( bool abort=false );
    uint32_t                 updateDeviceDetails();

    MicDeviceImpl*           injector() const;

public: // STATIC

    static std::string       deviceStateAsString( DeviceState state );

    explicit MicDevice( MicDeviceImpl* device );

    friend class  MicDeviceFactory;

private:

    struct PrivData;
    std::unique_ptr<PrivData>  m_pData;


private:    // DISABLE

    MicDevice();
    MicDevice( const MicDevice& );
    MicDevice&  operator = ( const MicDevice& );

};

//----------------------------------------------------------------------------

}   // namespace micmgmt

//----------------------------------------------------------------------------

#endif // MICMGMT_MICDEVICE_HPP
