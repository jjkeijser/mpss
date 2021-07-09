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

#include <gtest/gtest.h>
#include "KnlMockDevice.hpp"
#include "MicBootConfigInfo.hpp"
#include "MicDeviceError.hpp"
#include "MicDeviceInfo.hpp"
#include "MicDeviceDetails.hpp"
#include "MicPciConfigInfo.hpp"
#include "MicProcessorInfo.hpp"
#include "MicMemoryInfo.hpp"
#include "MicCoreInfo.hpp"
#include "MicPlatformInfo.hpp"
#include "MicPowerState.hpp"
#include "MicThermalInfo.hpp"
#include "MicVersionInfo.hpp"
#include "MicVoltageInfo.hpp"
#include "MicCoreUsageInfo.hpp"
#include "MicPowerUsageInfo.hpp"
#include "MicPowerThresholdInfo.hpp"
#include "MicMemoryUsageInfo.hpp"
#include "FlashDeviceInfo.hpp"
#include "FlashImage.hpp"
#include "FlashStatus.hpp"
#include "MicVoltage.hpp"

namespace micmgmt
{
    using namespace std;

    TEST(sdk, TC_KNL_mpsstools_KnlMockDevice_001)
    {
        uint32_t LED_MODE;
        bool     TURBO_ENABLED;
        bool     TURBO_AVAIL;
        bool     MAINT_ENABLED;
        bool     MAINT_AVAIL;
        bool     SMC_ENABLED;
        bool     SMC_AVAIL;
        uint8_t  SMC_DATA;

        const int                DEVICE_NUM = 0;
        const std::string        DEVICE_NAME( "mic0" );
        const std::string        DEVICE_TYPE( "x200" );
        const MicBootConfigInfo  BOOT_CONFIG( "bzImage" );
        std::string              POST_CODE( "" );
        const std::string        DEVICE_READY( "ready" );
        const std::string        DEVICE_ONLINE( "online" );

        const uint8_t SMC_OFFSET_RO           = 0x07; // SMC REG PCI MBus Manageability Address
        const uint8_t SMC_OFFSET_WO           = 0x17; // SMC REG Restart SMBus addr negotiation
        const uint8_t SMC_OFFSET_INVALID      = 0x1;
        size_t SMC_SIZE                       = 4; //matches nbytes for offsets 0x00 and 0x17
        size_t SMC_SIZE_INVALID               = (SMC_SIZE + 1);
        const uint32_t PT_POWER               = 1;
        const uint32_t PT_TIME                = 2;
        const uint32_t ERR_SUCCESS            = MicDeviceError::errorCode( MICSDKERR_SUCCESS );
        const uint32_t ERR_INVALID_ARG        = MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );
        const uint32_t ERR_DEVICE_NOT_OPEN    = MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_OPEN );
        const uint32_t ERR_DEVICE_NOT_ONLINE  = MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_ONLINE );
        const uint32_t ERR_NOT_SUPP           = MicDeviceError::errorCode( MICSDKERR_NOT_SUPPORTED );
        const uint32_t ERR_NOT_READY          = MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_READY );
        const MicDevice::PowerWindow WINDOW_0 = MicDevice::PowerWindow::eWindow0;
        const MicDevice::PowerWindow WINDOW_1 = MicDevice::PowerWindow::eWindow1;

        MicPowerThresholdInfo POWER_THRESHOLD_INFO;
        MicProcessorInfo      PROCESSOR_INFO;
        MicPciConfigInfo      PCI_CONFIG_INFO;
        MicVersionInfo        VERSION_INFO;
        MicMemoryInfo         MEMORY_INFO;
        MicCoreInfo           CORE_INFO;
        MicPlatformInfo       PLATFORM_INFO;
        MicThermalInfo        THERMAL_INFO;
        MicVoltageInfo        VOLTAGE_INFO;
        MicCoreUsageInfo      CORE_USAGE_INFO;
        MicPowerUsageInfo     POWER_USAGE_INFO;
        MicMemoryUsageInfo    MEMORY_USAGE_INFO;
        FlashDeviceInfo       FLASH_DEVICE_INFO;
        FlashImage            FLASH_IMAGE;
        FlashStatus           FLASH_STATUS;
        std::list<MicPowerState> STATES;

        {
            // Test cases organized by device state
            //
            // Case1: Unopened device tests.
            KnlMockDevice* knlmock = new KnlMockDevice( DEVICE_NUM );
            MicDevice  knldev( knlmock );  // Takes ownership of knlmock

            EXPECT_FALSE( knldev.isOpen() );
            EXPECT_FALSE( knldev.isOnline() );
            EXPECT_FALSE( knldev.isReady() );
            EXPECT_FALSE( knldev.isCompatibleFlashImage( FLASH_IMAGE ) );
            EXPECT_EQ( DEVICE_NUM, knldev.deviceNum() );
            EXPECT_EQ( DEVICE_NAME, knldev.deviceName() );
            EXPECT_EQ( DEVICE_TYPE, knldev.deviceType() );
            EXPECT_EQ( MicDevice::eOnline, knldev.deviceState() );

            EXPECT_EQ( ERR_INVALID_ARG, knldev.getProcessorInfo( NULL ) );
            EXPECT_EQ( ERR_INVALID_ARG, knldev.getPciConfigInfo( NULL ) );
            EXPECT_EQ( ERR_INVALID_ARG, knldev.getVersionInfo( NULL ) );
            EXPECT_EQ( ERR_INVALID_ARG, knldev.getMemoryInfo( NULL ) );
            EXPECT_EQ( ERR_INVALID_ARG, knldev.getCoreInfo( NULL ) );
            EXPECT_EQ( ERR_INVALID_ARG, knldev.getPlatformInfo( NULL ) );
            EXPECT_EQ( ERR_INVALID_ARG, knldev.getThermalInfo( NULL ) );
            EXPECT_EQ( ERR_INVALID_ARG, knldev.getVoltageInfo( NULL ) );
            EXPECT_EQ( ERR_INVALID_ARG, knldev.getCoreUsageInfo( NULL ) );
            EXPECT_EQ( ERR_INVALID_ARG, knldev.getPowerUsageInfo( NULL ) );
            EXPECT_EQ( ERR_INVALID_ARG, knldev.getPowerThresholdInfo( NULL ) );
            EXPECT_EQ( ERR_INVALID_ARG, knldev.getMemoryUsageInfo( NULL ) );
            EXPECT_EQ( ERR_INVALID_ARG, knldev.getFlashDeviceInfo( NULL ) );
            EXPECT_EQ( ERR_INVALID_ARG, knldev.getFlashUpdateStatus( NULL ) );
            EXPECT_EQ( ERR_INVALID_ARG, knldev.getPostCode( NULL ) );
            EXPECT_EQ( ERR_INVALID_ARG, knldev.getLedMode( NULL ) );
            EXPECT_EQ( ERR_INVALID_ARG, knldev.getTurboMode( NULL ) );
            EXPECT_EQ( ERR_INVALID_ARG, knldev.getMaintenanceMode( NULL ) );
            EXPECT_EQ( ERR_INVALID_ARG, knldev.getSmcPersistenceState( NULL ) );
            EXPECT_EQ( ERR_INVALID_ARG, knldev.getPowerStates( NULL ) );
            EXPECT_EQ( ERR_INVALID_ARG, knldev.startFlashUpdate( NULL ) );

            EXPECT_EQ( ERR_DEVICE_NOT_OPEN, knldev.getProcessorInfo( &PROCESSOR_INFO ) );
            EXPECT_EQ( ERR_DEVICE_NOT_OPEN, knldev.getPciConfigInfo( &PCI_CONFIG_INFO ) );
            EXPECT_EQ( ERR_DEVICE_NOT_OPEN, knldev.getVersionInfo( &VERSION_INFO ) );
            EXPECT_EQ( ERR_DEVICE_NOT_OPEN, knldev.getMemoryInfo( &MEMORY_INFO ) );
            EXPECT_EQ( ERR_DEVICE_NOT_OPEN, knldev.getCoreInfo( &CORE_INFO ) );
            EXPECT_EQ( ERR_DEVICE_NOT_OPEN, knldev.getPlatformInfo( &PLATFORM_INFO ) );
            EXPECT_EQ( ERR_DEVICE_NOT_OPEN, knldev.getThermalInfo( &THERMAL_INFO ) );
            EXPECT_EQ( ERR_DEVICE_NOT_OPEN, knldev.getVoltageInfo( &VOLTAGE_INFO ) );
            EXPECT_EQ( ERR_DEVICE_NOT_OPEN, knldev.getCoreUsageInfo( &CORE_USAGE_INFO ) );
            EXPECT_EQ( ERR_DEVICE_NOT_OPEN, knldev.getPowerUsageInfo( &POWER_USAGE_INFO ) );
            EXPECT_EQ( ERR_DEVICE_NOT_OPEN, knldev.getPowerThresholdInfo( &POWER_THRESHOLD_INFO ) );
            EXPECT_EQ( ERR_DEVICE_NOT_OPEN, knldev.getMemoryUsageInfo( &MEMORY_USAGE_INFO ) );
            EXPECT_EQ( ERR_DEVICE_NOT_OPEN, knldev.getFlashDeviceInfo( &FLASH_DEVICE_INFO ) );
            EXPECT_EQ( ERR_DEVICE_NOT_OPEN, knldev.getFlashUpdateStatus( &FLASH_STATUS ) );

            EXPECT_EQ( ERR_DEVICE_NOT_OPEN, knldev.getPostCode( &POST_CODE ) );
            EXPECT_EQ( ERR_DEVICE_NOT_OPEN, knldev.getLedMode( &LED_MODE ) );
            EXPECT_EQ( ERR_DEVICE_NOT_OPEN, knldev.getTurboMode( &TURBO_ENABLED ) );
            EXPECT_EQ( ERR_DEVICE_NOT_OPEN, knldev.getMaintenanceMode( &MAINT_ENABLED, &MAINT_AVAIL) );
            EXPECT_EQ( ERR_DEVICE_NOT_OPEN, knldev.getSmcPersistenceState( &SMC_ENABLED, &SMC_AVAIL ) );
            EXPECT_EQ( ERR_DEVICE_NOT_OPEN, knldev.getSmcRegisterData( SMC_OFFSET_RO, &SMC_DATA, &SMC_SIZE ) );
            EXPECT_EQ( ERR_DEVICE_NOT_OPEN, knldev.getPowerStates( &STATES ) );

            EXPECT_EQ( ERR_DEVICE_NOT_OPEN, knldev.startFlashUpdate( &FLASH_IMAGE ) );

            EXPECT_EQ( ERR_DEVICE_NOT_OPEN, knldev.finalizeFlashUpdate() );
            EXPECT_EQ( ERR_DEVICE_NOT_OPEN, knldev.setLedMode( 1 ) );
            EXPECT_EQ( ERR_DEVICE_NOT_OPEN, knldev.setTurboModeEnabled( true ) );
            EXPECT_EQ( ERR_DEVICE_NOT_OPEN, knldev.setMaintenanceModeEnabled( true ) );
            EXPECT_EQ( ERR_DEVICE_NOT_OPEN, knldev.setSmcPersistenceEnabled( true ) );
            EXPECT_EQ( ERR_DEVICE_NOT_OPEN, knldev.setSmcRegisterData( SMC_OFFSET_WO, &SMC_DATA, SMC_SIZE ) );
            EXPECT_EQ( ERR_DEVICE_NOT_OPEN, knldev.setPowerThreshold( WINDOW_0, PT_POWER, PT_TIME ) );
            EXPECT_EQ( ERR_DEVICE_NOT_OPEN, knldev.setPowerStates( STATES ) );
            EXPECT_EQ( ERR_DEVICE_NOT_OPEN, knldev.updateDeviceDetails() );

            // code coverage for device not open.
            EXPECT_EQ( ERR_DEVICE_NOT_OPEN, knldev.boot(BOOT_CONFIG) );

            knldev.close();

            // Case2: Open (but not online) tests.
            EXPECT_EQ( ERR_SUCCESS, knldev.open() );
            EXPECT_TRUE( knldev.isOpen() );
            knlmock->setAdmin( true );
            knldev.reset();
            knlmock->setAdmin( false );
            EXPECT_FALSE( knldev.isOnline() );
            EXPECT_TRUE( knldev.isReady() );
            EXPECT_FALSE( knldev.isCompatibleFlashImage( FLASH_IMAGE ) );
            EXPECT_EQ( MicDevice::eReady, knldev.deviceState() );

            EXPECT_FALSE( PROCESSOR_INFO.isValid() );
            EXPECT_EQ( ERR_SUCCESS, knldev.getProcessorInfo( &PROCESSOR_INFO ) );
            EXPECT_TRUE( PROCESSOR_INFO.isValid() );

            EXPECT_FALSE( PCI_CONFIG_INFO.isValid() );
            EXPECT_EQ( ERR_SUCCESS, knldev.getPciConfigInfo( &PCI_CONFIG_INFO ) );
            EXPECT_TRUE( PCI_CONFIG_INFO.isValid() );

            EXPECT_FALSE( VERSION_INFO.isValid() );
            EXPECT_EQ( ERR_SUCCESS, knldev.getVersionInfo( &VERSION_INFO ) );
            EXPECT_TRUE( VERSION_INFO.isValid() );

            EXPECT_EQ( ERR_DEVICE_NOT_ONLINE, knldev.getMemoryInfo( &MEMORY_INFO ) );
            EXPECT_EQ( ERR_DEVICE_NOT_ONLINE, knldev.getCoreInfo( &CORE_INFO ) );
            EXPECT_EQ( ERR_SUCCESS, knldev.getPlatformInfo( &PLATFORM_INFO ) );
            EXPECT_EQ( ERR_DEVICE_NOT_ONLINE, knldev.getThermalInfo( &THERMAL_INFO ) );
            EXPECT_EQ( ERR_DEVICE_NOT_ONLINE, knldev.getVoltageInfo( &VOLTAGE_INFO ) );
            EXPECT_EQ( ERR_DEVICE_NOT_ONLINE, knldev.getCoreUsageInfo( &CORE_USAGE_INFO ) );
            EXPECT_EQ( ERR_DEVICE_NOT_ONLINE, knldev.getPowerUsageInfo( &POWER_USAGE_INFO ) );
            EXPECT_EQ( ERR_DEVICE_NOT_ONLINE, knldev.getPowerThresholdInfo( &POWER_THRESHOLD_INFO ) );
            EXPECT_EQ( ERR_DEVICE_NOT_ONLINE, knldev.getMemoryUsageInfo( &MEMORY_USAGE_INFO ) );

            EXPECT_FALSE( FLASH_DEVICE_INFO.isValid() );
            EXPECT_EQ( ERR_NOT_SUPP, knldev.getFlashDeviceInfo( &FLASH_DEVICE_INFO ) );
/// \todo Prepare Flash device EXPECT_TRUE( FLASH_DEVICE_INFO.isValid() );

            EXPECT_EQ( 0, FLASH_STATUS.progress() );
            EXPECT_EQ( ERR_SUCCESS, knldev.getFlashUpdateStatus( &FLASH_STATUS ) );

            EXPECT_EQ( ERR_SUCCESS, knldev.getPostCode( &POST_CODE ) );
            EXPECT_EQ( ERR_DEVICE_NOT_ONLINE, knldev.getLedMode( &LED_MODE ) );
            EXPECT_EQ( ERR_DEVICE_NOT_ONLINE, knldev.getTurboMode( &TURBO_ENABLED ) );
            EXPECT_EQ( ERR_NOT_SUPP, knldev.getMaintenanceMode( &MAINT_ENABLED, &MAINT_AVAIL) );
            EXPECT_EQ( ERR_DEVICE_NOT_ONLINE, knldev.getSmcPersistenceState( &SMC_ENABLED, &SMC_AVAIL ) );
            EXPECT_EQ( ERR_DEVICE_NOT_ONLINE, knldev.getSmcRegisterData( SMC_OFFSET_RO, &SMC_DATA, &SMC_SIZE ) );
            EXPECT_EQ( ERR_DEVICE_NOT_ONLINE, knldev.getPowerStates( &STATES ) );

            // Code coverage tests.
            EXPECT_EQ( ERR_SUCCESS, knldev.open() );
            EXPECT_EQ( ERR_INVALID_ARG, knldev.startFlashUpdate( &FLASH_IMAGE ) );

            EXPECT_EQ( ERR_SUCCESS, knldev.finalizeFlashUpdate() );

            EXPECT_EQ( ERR_DEVICE_NOT_ONLINE, knldev.setLedMode( 1 ) );
            EXPECT_EQ( ERR_DEVICE_NOT_ONLINE, knldev.setTurboModeEnabled( true ) );

            knlmock->setAdmin(true); //Fake Admin
            EXPECT_EQ( ERR_NOT_SUPP, knldev.setMaintenanceModeEnabled( true ) );
            knlmock->setAdmin(false); //reset

            EXPECT_EQ( ERR_DEVICE_NOT_ONLINE, knldev.setSmcPersistenceEnabled( true ) );
            EXPECT_EQ( ERR_DEVICE_NOT_ONLINE, knldev.setSmcRegisterData( SMC_OFFSET_WO, &SMC_DATA, SMC_SIZE ) );
            EXPECT_EQ( ERR_DEVICE_NOT_ONLINE, knldev.setPowerThreshold( WINDOW_0, PT_POWER, PT_TIME ) );
            EXPECT_EQ( ERR_NOT_SUPP, knldev.setPowerStates( STATES ) );

            EXPECT_EQ( ERR_DEVICE_NOT_ONLINE, knldev.updateDeviceDetails() );

            // Case3: BOOT (online) tests
            knlmock->setAdmin(true); // Fake Admin
            EXPECT_EQ( ERR_SUCCESS, knldev.boot(BOOT_CONFIG) );
            knlmock->setAdmin(false);

            EXPECT_EQ( MicDevice::DeviceState::eReboot, knldev.deviceState() );
            EXPECT_EQ( MicDevice::DeviceState::eOnline, knldev.deviceState() ); // device state advanced.
            EXPECT_EQ( ERR_SUCCESS, knldev.updateDeviceDetails() );

            EXPECT_TRUE( knldev.isOnline() );
            EXPECT_FALSE( knldev.isReady() );
            EXPECT_EQ( MicDevice::eOnline, knldev.deviceState() );

            EXPECT_FALSE( MEMORY_INFO.isValid() );
            EXPECT_EQ( ERR_SUCCESS, knldev.getMemoryInfo( &MEMORY_INFO ) );
            EXPECT_TRUE( MEMORY_INFO.isValid() );

            EXPECT_FALSE( CORE_INFO.isValid() );
            EXPECT_EQ( ERR_SUCCESS, knldev.getCoreInfo( &CORE_INFO ) );
            EXPECT_TRUE( CORE_INFO.isValid() );

            PLATFORM_INFO.clear();
            EXPECT_FALSE( PLATFORM_INFO.isValid() );
            EXPECT_EQ( ERR_SUCCESS, knldev.getPlatformInfo( &PLATFORM_INFO ) );
            EXPECT_TRUE( PLATFORM_INFO.isValid() );

            EXPECT_FALSE( THERMAL_INFO.isValid() );
            EXPECT_EQ( ERR_SUCCESS, knldev.getThermalInfo( &THERMAL_INFO ) );
            EXPECT_TRUE( THERMAL_INFO.isValid() );

            EXPECT_FALSE( VOLTAGE_INFO.isValid() );
            EXPECT_EQ( ERR_SUCCESS, knldev.getVoltageInfo( &VOLTAGE_INFO ) );
            EXPECT_TRUE( VOLTAGE_INFO.isValid() );

            EXPECT_FALSE( CORE_USAGE_INFO.isValid() );
            EXPECT_EQ( ERR_SUCCESS, knldev.getCoreUsageInfo( &CORE_USAGE_INFO ) );
/// \todo   EXPECT_TRUE( CORE_USAGE_INFO.isValid() );

            EXPECT_FALSE( POWER_USAGE_INFO.isValid() );
            EXPECT_EQ( ERR_SUCCESS, knldev.getPowerUsageInfo( &POWER_USAGE_INFO ) );
            EXPECT_TRUE( POWER_USAGE_INFO.isValid() );

            EXPECT_FALSE( POWER_THRESHOLD_INFO.isValid() );
            EXPECT_EQ(ERR_SUCCESS, knldev.getPowerThresholdInfo(&POWER_THRESHOLD_INFO));
            EXPECT_TRUE( POWER_THRESHOLD_INFO.isValid() );

            EXPECT_FALSE( MEMORY_USAGE_INFO.isValid() );
            EXPECT_EQ( ERR_SUCCESS, knldev.getMemoryUsageInfo( &MEMORY_USAGE_INFO ) );
            EXPECT_TRUE( MEMORY_USAGE_INFO.isValid() );

            // LED mode tests.
            EXPECT_EQ( ERR_SUCCESS, knldev.setLedMode( 1 ) );
            EXPECT_EQ( ERR_SUCCESS, knldev.getLedMode( &LED_MODE ) );
            EXPECT_EQ( 1U, LED_MODE );
            EXPECT_EQ( ERR_SUCCESS, knldev.setLedMode( 0 ) );
            EXPECT_EQ( ERR_SUCCESS, knldev.getLedMode( &LED_MODE ) );
            EXPECT_EQ( 0U, LED_MODE );
            EXPECT_EQ( ERR_INVALID_ARG, knldev.setLedMode( 2 ) );
            EXPECT_EQ( ERR_SUCCESS, knldev.getLedMode( &LED_MODE ) );
            EXPECT_EQ( 0U, LED_MODE );

            EXPECT_EQ( ERR_SUCCESS, knldev.setTurboModeEnabled( true ) );
            EXPECT_EQ( ERR_SUCCESS, knldev.getTurboMode( &TURBO_ENABLED, &TURBO_AVAIL ) );
            EXPECT_EQ( ERR_NOT_SUPP, knldev.getMaintenanceMode( &MAINT_ENABLED, NULL ) );
            EXPECT_FALSE( MAINT_ENABLED );

            EXPECT_EQ( ERR_SUCCESS, knldev.setSmcPersistenceEnabled( true ) );
            EXPECT_EQ( ERR_SUCCESS, knldev.getSmcPersistenceState( &SMC_ENABLED, NULL ) );
            EXPECT_EQ( ERR_SUCCESS, knldev.getSmcPersistenceState( &SMC_ENABLED, &SMC_AVAIL ) );

            // SMC register set/get tests.
            EXPECT_EQ( ERR_INVALID_ARG, knldev.setSmcRegisterData( SMC_OFFSET_WO, NULL, SMC_SIZE ) );
            EXPECT_EQ( ERR_INVALID_ARG, knldev.setSmcRegisterData( SMC_OFFSET_WO, &SMC_DATA, 0 ) );
            EXPECT_EQ( ERR_INVALID_ARG, knldev.getSmcRegisterData( SMC_OFFSET_RO, NULL, NULL ) );
            EXPECT_EQ( ERR_INVALID_ARG, knldev.setSmcRegisterData( SMC_OFFSET_INVALID, &SMC_DATA, SMC_SIZE ) );
            EXPECT_EQ( ERR_INVALID_ARG, knldev.setSmcRegisterData( SMC_OFFSET_RO, &SMC_DATA, SMC_SIZE ) );
            EXPECT_EQ( ERR_INVALID_ARG, knldev.setSmcRegisterData( SMC_OFFSET_WO, &SMC_DATA, SMC_SIZE_INVALID ) );

            EXPECT_EQ( ERR_INVALID_ARG, knldev.setSmcRegisterData( SMC_OFFSET_WO, &SMC_DATA, SMC_SIZE ) );

            EXPECT_EQ( ERR_SUCCESS, knldev.setPowerThreshold( WINDOW_0, PT_POWER, PT_TIME ) );
            EXPECT_EQ( ERR_SUCCESS, knldev.setPowerThreshold( WINDOW_1, PT_POWER, PT_TIME ) );

            EXPECT_EQ( ERR_NOT_SUPP, knldev.getPowerStates( &STATES ) );

            // Case4: Try booting when the device is online.
            EXPECT_EQ( ERR_NOT_READY, knldev.boot(BOOT_CONFIG) );

            knlmock->setAdmin(true); // Fake Admin
            EXPECT_EQ( ERR_SUCCESS, knldev.reset() );
            knlmock->setAdmin(false);

            EXPECT_EQ( MicDevice::DeviceState::eReset, knldev.deviceState() );
            EXPECT_EQ( MicDevice::DeviceState::eReady, knldev.deviceState() );
            EXPECT_TRUE( knldev.isReady() );

            // Case5: Reboot tests
            knlmock->setAdmin(true);
            EXPECT_EQ( ERR_SUCCESS, knldev.boot(BOOT_CONFIG) );
            knlmock->setAdmin(false);

            EXPECT_EQ( MicDevice::DeviceState::eReboot, knldev.deviceState() );
            EXPECT_EQ( MicDevice::DeviceState::eOnline, knldev.deviceState() );

            EXPECT_EQ( ERR_SUCCESS, knldev.updateDeviceDetails() );

            knldev.close();
        }

    } // sdk.TC_KNL_mpsstools_KnlMockDevice_001

}   // namespace micmgmt
