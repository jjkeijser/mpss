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

#ifndef MICSMCCLI_KNXMOCKUTDEVICE_HPP
#define MICSMCCLI_KNXMOCKUTDEVICE_HPP

// SDK Includes
#include "MicDeviceImpl.hpp"
#include "MicDevice.hpp"

// Common Framework Includes
#include "MsTimer.hpp"

// C++ Includes
#include <string>
#include <memory>
#include <unordered_map>

namespace micmgmt
{
    class KnlMockDevice;
    class MicPciConfigInfo;
    class MicProcessorInfo;
    class MicVersionInfo;
    class MicMemoryInfo;
    class MicCoreInfo;
    class MicPlatformInfo;
    class MicThermalInfo;
    class MicVoltageInfo;
    class MicCoreUsageInfo;
    class MicPowerUsageInfo;
    class MicPowerThresholdInfo;
    class MicMemoryUsageInfo;
    class FlashDeviceInfo;
    class FlashStatus;
    class MicBootConfigInfo;
    class FlashImage;
    class MicPowerState;
}; // namespace micmgmt

namespace micsmccli
{
    class KnxMockUtDevice : public micmgmt::MicDeviceImpl
    {
    private: // Mock Internals (not modifiable in UT)
        std::shared_ptr<micmgmt::MicDeviceImpl> internalDevice_;
        int                                     deviceType_;
        micmgmt::MsTimer                        jiffySim_;

    private:
        uint64_t getJiffy() const;

    public: // UT Modifiable Fields
        bool                                                             initialOnlineMode_;
        bool                                                             initialAdminMode_;
        uint32_t                                                         deviceOpenError_;
        std::unordered_map<int, std::shared_ptr<micmgmt::MicPowerState>> powerStates_;
        uint32_t                                                         fakeErrorSetECC_;
        uint32_t                                                         fakeErrorSetTurbo_;
        uint32_t                                                         fakeErrorSetLED_;
        uint32_t                                                         fakeErrorSetPwrFeature_;
        uint32_t                                                         fakeErrorGetThermal_;
        uint32_t                                                         fakeErrorGetPwrUsage_;
        uint32_t                                                         fakeErrorGetMemUsage_;
        uint32_t                                                         fakeErrorGetCoreUsage_;
        uint32_t                                                         fakeErrorGetECC_;
        uint32_t                                                         fakeErrorGetTurbo_;
        uint32_t                                                         fakeErrorGetLED_;
        bool                                                             fakeLongOperation_;

    public: // Construction, destruction, and UT API
        explicit KnxMockUtDevice(int num, int type, bool online, bool admin);
        virtual ~KnxMockUtDevice();

        micmgmt::KnlMockDevice* getKnl() const;
        bool isKnl() const;

        void mockReset();

    public: // Changed APIs
        virtual uint32_t deviceOpen();
        virtual uint32_t getDeviceCoreUsageInfo(micmgmt::MicCoreUsageInfo* info) const;

    public: // Unchanged APIs
        virtual bool        isDeviceOpen() const;
        virtual std::string deviceType() const;
        virtual uint32_t    getDeviceState(micmgmt::MicDevice::DeviceState* state) const;
        virtual uint32_t    getDevicePciConfigInfo(micmgmt::MicPciConfigInfo* info) const;
        virtual uint32_t    getDeviceProcessorInfo(micmgmt::MicProcessorInfo* info) const;
        virtual uint32_t    getDeviceVersionInfo(micmgmt::MicVersionInfo* info) const;
        virtual uint32_t    getDeviceMemoryInfo(micmgmt::MicMemoryInfo* info) const;
        virtual uint32_t    getDeviceCoreInfo(micmgmt::MicCoreInfo* info) const;
        virtual uint32_t    getDevicePlatformInfo(micmgmt::MicPlatformInfo* info) const;
        virtual uint32_t    getDeviceThermalInfo(micmgmt::MicThermalInfo* info) const;
        virtual uint32_t    getDeviceVoltageInfo(micmgmt::MicVoltageInfo* info) const;
        virtual uint32_t    getDevicePowerUsageInfo(micmgmt::MicPowerUsageInfo* info) const;
        virtual uint32_t    getDevicePowerThresholdInfo(micmgmt::MicPowerThresholdInfo* info) const;
        virtual uint32_t    getDeviceMemoryUsageInfo(micmgmt::MicMemoryUsageInfo* info) const;
        virtual uint32_t    getDevicePostCode(std::string* code) const;
        virtual uint32_t    getDeviceLedMode(uint32_t* mode) const;
        virtual uint32_t    getDeviceEccMode(bool* enabled, bool* available = 0) const;
        virtual uint32_t    getDeviceTurboMode(bool* enabled, bool* available = 0, bool* active = 0) const;
        virtual uint32_t    getDeviceMaintenanceMode(bool* enabled, bool* available = 0) const;
        virtual uint32_t    getDeviceSmBusAddressTrainingStatus(bool* busy, int* eta = 0) const;
        virtual uint32_t    getDeviceSmcPersistenceState(bool* enabled, bool* available = 0) const;
        virtual uint32_t    getDeviceSmcRegisterData(uint8_t offset, uint8_t* data, size_t* size) const;
        virtual uint32_t    getDevicePowerStates(std::list<micmgmt::MicPowerState>* states) const;
        virtual uint32_t    getDeviceFlashDeviceInfo(micmgmt::FlashDeviceInfo* info) const;
        virtual uint32_t    getDeviceFlashUpdateStatus(micmgmt::FlashStatus* status) const;
        virtual void        deviceClose();
        virtual uint32_t    deviceBoot(const micmgmt::MicBootConfigInfo& info);
        virtual uint32_t    deviceReset(bool force);
        virtual uint32_t    setDeviceLedMode(uint32_t mode);
        virtual uint32_t    setDeviceTurboModeEnabled(bool state);
        virtual uint32_t    setDeviceEccModeEnabled(bool state, int timeout = 0, int* stage = NULL);
        virtual uint32_t    setDeviceMaintenanceModeEnabled(bool state, int timeout = 0);
        virtual uint32_t    setDeviceSmcPersistenceEnabled(bool state);
        virtual uint32_t    setDeviceSmcRegisterData(uint8_t offset, const uint8_t* data, size_t size);
        virtual uint32_t    setDevicePowerThreshold(micmgmt::MicDevice::PowerWindow window, uint16_t power, uint16_t time);
        virtual uint32_t    setDevicePowerThreshold(micmgmt::MicDevice::PowerWindow window, uint32_t power, uint32_t time);
        virtual uint32_t    setDevicePowerStates(const std::list<micmgmt::MicPowerState>& states);
        virtual uint32_t    startDeviceSmBusAddressTraining(int hint, int timeout);
        virtual uint32_t    startDeviceFlashUpdate(micmgmt::FlashImage* image, bool force);
        virtual uint32_t    startDeviceFlashUpdate(std::string & path, std::unique_ptr<micmgmt::FwToUpdate> & fwToUpdate);
        virtual uint32_t    finalizeDeviceFlashUpdate(bool abort);
    }; // class KnxMockUtDevice
}; // namespace micsmccli
#endif // MICSMCCLI_KNXMOCKUTDEVICE_HPP
