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

// UT Includes
#include "KnxMockUtDevice.hpp"

// SDK Includes
#include "MicDeviceManager.hpp"
#include "KnlMockDevice.hpp"
#include "micsdkerrno.h"
#include "MicCoreUsageInfo.hpp"
#include "MicPowerState.hpp"

// CF Includes
#include "MsTimer.hpp"

using namespace std;
using namespace micmgmt;

namespace
{
    const uint64_t sJiffyFactor = 3841; // randomly selected multiplier for simulated jiffies.
}; // empty namespace
namespace micsmccli
{
    typedef MicDeviceManager::DeviceType DT;

    ///////////////////////////////////////////////////////////////////
    // UT Public API
    KnxMockUtDevice::KnxMockUtDevice(int type, int num, bool online, bool admin)
        : MicDeviceImpl(num), deviceType_(type), initialOnlineMode_(online), initialAdminMode_(admin)
    {
        if (type == (int)DT::eDeviceTypeKnl)
        {
            internalDevice_ = shared_ptr<MicDeviceImpl>(new KnlMockDevice(num));
        }

        mockReset();
    }

    KnxMockUtDevice::~KnxMockUtDevice()
    {
    }

    micmgmt::KnlMockDevice* KnxMockUtDevice::getKnl() const
    {
        return dynamic_cast<KnlMockDevice*>(internalDevice_.get());
    }

    bool KnxMockUtDevice::isKnl() const
    {
        return (deviceType_ == (int)DT::eDeviceTypeKnl);
    }

    void KnxMockUtDevice::mockReset()
    {
        deviceOpenError_ = 0;

        powerStates_.clear();

        shared_ptr<MicPowerState> p1 = make_shared<MicPowerState>(MicPowerState::State::eCpuFreq, false);
        powerStates_[p1->state()] = p1;

        shared_ptr<MicPowerState> co6 = make_shared<MicPowerState>(MicPowerState::State::eCoreC6, false);
        powerStates_[co6->state()] = co6;

        shared_ptr<MicPowerState> pc3 = make_shared<MicPowerState>(MicPowerState::State::ePc3, true);
        powerStates_[pc3->state()] = pc3;

        shared_ptr<MicPowerState> pc6 = make_shared<MicPowerState>(MicPowerState::State::ePc6, true);
        powerStates_[pc6->state()] = pc6;

        fakeErrorSetECC_        = 0;
        fakeErrorSetTurbo_      = 0;
        fakeErrorSetLED_        = 0;
        fakeErrorSetPwrFeature_ = 0;
        fakeErrorGetThermal_    = 0;
        fakeErrorGetPwrUsage_   = 0;
        fakeErrorGetMemUsage_   = 0;
        fakeErrorGetCoreUsage_  = 0;
        fakeErrorGetECC_        = 0;
        fakeErrorGetTurbo_      = 0;
        fakeErrorGetLED_        = 0;
        fakeLongOperation_      = false;
    }

    ///////////////////////////////////////////////////////////////////
    // UT Private Methods
    uint64_t KnxMockUtDevice::getJiffy() const
    {
        return (uint64_t)jiffySim_.elapsed().count() * sJiffyFactor;
    }

    ///////////////////////////////////////////////////////////////////
    // Changed MicDeviceImpl APIs
    uint32_t KnxMockUtDevice::deviceOpen()
    {
        uint32_t rv = 0;
        if (isKnl() == true)
        {
            getKnl()->setInitialDeviceState((initialOnlineMode_ == true) ? MicDevice::DeviceState::eOnline : MicDevice::DeviceState::eReady);
            getKnl()->setAdmin(initialAdminMode_);
            rv = (deviceOpenError_ != 0)?deviceOpenError_:internalDevice_->deviceOpen();
        }
        else
        {
            rv = MICSDKERR_NOT_SUPPORTED;
        }
        return rv;
    }

    uint32_t KnxMockUtDevice::getDeviceCoreUsageInfo(MicCoreUsageInfo* info) const
    {
        if (fakeErrorGetCoreUsage_ == 0)
        {
            uint32_t rv = internalDevice_->getDeviceCoreUsageInfo(info);
            uint64_t jiffy = getJiffy();
            info->setTickCount(jiffy);
            for (size_t core = 0; core < info->coreCount(); ++core)
            {
                if (((core + 1) % 5) == 0)
                {
                    info->setUsageCount(MicCoreUsageInfo::Counter::eUser, core, jiffy / 7);
                }
            }
            return rv;
        }
        else
        {
            return fakeErrorGetCoreUsage_;
        }
    }

    uint32_t KnxMockUtDevice::getDevicePowerStates(std::list<MicPowerState>* states) const
    {
        if (states == NULL || states->size() > 0)
        {
            return MICSDKERR_INVALID_ARG;
        }

        return MICSDKERR_NOT_SUPPORTED;
    }

    uint32_t KnxMockUtDevice::setDevicePowerStates(const std::list<MicPowerState>& states)
    {
        if (states.size() == 0)
        {
            return MICSDKERR_INVALID_ARG;
        }

        return MICSDKERR_NOT_SUPPORTED;
    }

    uint32_t KnxMockUtDevice::setDeviceLedMode(uint32_t mode)
    {
        if (fakeErrorSetLED_ == 0)
        {
            return internalDevice_->setDeviceLedMode(mode);
        }
        else
        {
            return fakeErrorSetLED_;
        }
    }

    uint32_t KnxMockUtDevice::setDeviceTurboModeEnabled(bool state)
    {
        if (fakeErrorSetTurbo_ == 0)
        {
            return internalDevice_->setDeviceTurboModeEnabled(state);
        }
        else
        {
            return fakeErrorSetTurbo_;
        }
    }

    uint32_t KnxMockUtDevice::setDeviceEccModeEnabled(bool state, int timeout, int* stage)
    {
        if (fakeErrorSetECC_ == 0)
        {
            uint32_t rv = MICSDKERR_SUCCESS;
            if (fakeLongOperation_ == true && timeout > 0 && timeout < 3000)
            {
                rv = MICSDKERR_TIMEOUT;
            }
            if (rv == MICSDKERR_SUCCESS)
            {
                return internalDevice_->setDeviceEccModeEnabled(state, timeout, stage);
            }
            else
            {
                return rv;
            }
        }
        else
        {
            return fakeErrorSetECC_;
        }
    }

    uint32_t KnxMockUtDevice::getDeviceThermalInfo(MicThermalInfo* info) const
    {
        if (fakeErrorGetThermal_ == 0)
        {
            return internalDevice_->getDeviceThermalInfo(info);
        }
        else
        {
            return fakeErrorGetThermal_;
        }
    }

    uint32_t KnxMockUtDevice::getDevicePowerUsageInfo(MicPowerUsageInfo* info) const
    {
        if (fakeErrorGetPwrUsage_ == 0)
        {
            return internalDevice_->getDevicePowerUsageInfo(info);
        }
        else
        {
            return fakeErrorGetPwrUsage_;
        }
    }

    uint32_t KnxMockUtDevice::getDeviceMemoryUsageInfo(MicMemoryUsageInfo* info) const
    {
        if (fakeErrorGetMemUsage_ == 0)
        {
            return internalDevice_->getDeviceMemoryUsageInfo(info);
        }
        else
        {
            return fakeErrorGetMemUsage_;
        }
    }

    uint32_t KnxMockUtDevice::getDeviceTurboMode(bool* enabled, bool* available, bool* active) const
    {
        if (fakeErrorGetTurbo_ == 0)
        {
            return internalDevice_->getDeviceTurboMode(enabled, available, active);
        }
        else
        {
            return fakeErrorGetTurbo_;
        }
    }

    uint32_t KnxMockUtDevice::getDeviceLedMode(uint32_t* mode) const
    {
        if (fakeErrorGetLED_ == 0)
        {
            return internalDevice_->getDeviceLedMode(mode);
        }
        else
        {
            return fakeErrorGetLED_;
        }
    }

    uint32_t KnxMockUtDevice::getDeviceEccMode(bool* enabled, bool* available) const
    {
        if (fakeErrorGetECC_ == 0)
        {
            return internalDevice_->getDeviceEccMode(enabled, available);
        }
        else
        {
            return fakeErrorGetECC_;
        }
    }

    ///////////////////////////////////////////////////////////////////
    // Unchanged MicDeviceImpl APIs
    bool KnxMockUtDevice::isDeviceOpen() const
    {
        return internalDevice_->isDeviceOpen();
    }

    std::string KnxMockUtDevice::deviceType() const
    {
        return internalDevice_->deviceType();
    }

    uint32_t KnxMockUtDevice::getDeviceProcessorInfo(MicProcessorInfo* info) const
    {
        return internalDevice_->getDeviceProcessorInfo(info);
    }

    uint32_t KnxMockUtDevice::getDeviceVersionInfo(MicVersionInfo* info) const
    {
        return internalDevice_->getDeviceVersionInfo(info);
    }

    uint32_t KnxMockUtDevice::getDevicePciConfigInfo(MicPciConfigInfo* info) const
    {
        return internalDevice_->getDevicePciConfigInfo(info);
    }

    uint32_t KnxMockUtDevice::getDeviceState(MicDevice::DeviceState* state) const
    {
        return internalDevice_->getDeviceState(state);
    }

    uint32_t KnxMockUtDevice::getDeviceMemoryInfo(MicMemoryInfo* info) const
    {
        return internalDevice_->getDeviceMemoryInfo(info);
    }

    uint32_t KnxMockUtDevice::getDeviceCoreInfo(MicCoreInfo* info) const
    {
        return internalDevice_->getDeviceCoreInfo(info);
    }

    uint32_t KnxMockUtDevice::getDevicePlatformInfo(MicPlatformInfo* info) const
    {
        return internalDevice_->getDevicePlatformInfo(info);
    }

    uint32_t KnxMockUtDevice::getDeviceVoltageInfo(MicVoltageInfo* info) const
    {
        return internalDevice_->getDeviceVoltageInfo(info);
    }

    uint32_t KnxMockUtDevice::getDevicePowerThresholdInfo(MicPowerThresholdInfo* info) const
    {
        return internalDevice_->getDevicePowerThresholdInfo(info);
    }

    uint32_t KnxMockUtDevice::getDevicePostCode(std::string* code) const
    {
        return internalDevice_->getDevicePostCode(code);
    }

    uint32_t KnxMockUtDevice::getDeviceMaintenanceMode(bool* enabled, bool* available) const
    {
        return internalDevice_->getDeviceMaintenanceMode(enabled, available);
    }

    uint32_t KnxMockUtDevice::getDeviceSmBusAddressTrainingStatus(bool* busy, int* eta) const
    {
        return internalDevice_->getDeviceSmBusAddressTrainingStatus(busy, eta);
    }

    uint32_t KnxMockUtDevice::getDeviceSmcPersistenceState(bool* enabled, bool* available) const
    {
        return internalDevice_->getDeviceSmcPersistenceState(enabled, available);
    }

    uint32_t KnxMockUtDevice::getDeviceSmcRegisterData(uint8_t offset, uint8_t* data, size_t* size) const
    {
        return internalDevice_->getDeviceSmcRegisterData(offset, data, size);
    }

    uint32_t KnxMockUtDevice::getDeviceFlashDeviceInfo(FlashDeviceInfo* info) const
    {
        return internalDevice_->getDeviceFlashDeviceInfo(info);
    }

    uint32_t KnxMockUtDevice::getDeviceFlashUpdateStatus(FlashStatus* status) const
    {
        return internalDevice_->getDeviceFlashUpdateStatus(status);
    }

    void KnxMockUtDevice::deviceClose()
    {
        internalDevice_->deviceClose();
    }

    uint32_t KnxMockUtDevice::deviceBoot(const MicBootConfigInfo& info)
    {
        return internalDevice_->deviceBoot(info);
    }

    uint32_t KnxMockUtDevice::deviceReset(bool force)
    {
        return internalDevice_->deviceReset(force);
    }

    uint32_t KnxMockUtDevice::setDeviceMaintenanceModeEnabled(bool state, int timeout)
    {
        return internalDevice_->setDeviceMaintenanceModeEnabled(state, timeout);
    }

    uint32_t KnxMockUtDevice::setDeviceSmcPersistenceEnabled(bool state)
    {
        return internalDevice_->setDeviceSmcPersistenceEnabled(state);
    }

    uint32_t KnxMockUtDevice::setDeviceSmcRegisterData(uint8_t offset, const uint8_t* data, size_t size)
    {
        return internalDevice_->setDeviceSmcRegisterData(offset, data, size);
    }

    uint32_t KnxMockUtDevice::setDevicePowerThreshold(MicDevice::PowerWindow window, uint16_t power, uint16_t time)
    {
        return internalDevice_->setDevicePowerThreshold(window, power, time);
    }

    uint32_t KnxMockUtDevice::setDevicePowerThreshold(MicDevice::PowerWindow window, uint32_t power, uint32_t time)
    {
        return internalDevice_->setDevicePowerThreshold(window, power, time);
    }

    uint32_t KnxMockUtDevice::startDeviceSmBusAddressTraining(int hint, int timeout)
    {
        return internalDevice_->startDeviceSmBusAddressTraining(hint, timeout);
    }

    uint32_t KnxMockUtDevice::startDeviceFlashUpdate(FlashImage* image, bool force)
    {
        return internalDevice_->startDeviceFlashUpdate(image, force);
    }

    uint32_t KnxMockUtDevice::startDeviceFlashUpdate(std::string & path,
                                                     std::unique_ptr<micmgmt::FwToUpdate> & fwToUpdate)
    {
        return internalDevice_->startDeviceFlashUpdate(path, fwToUpdate);
    }

    uint32_t KnxMockUtDevice::finalizeDeviceFlashUpdate(bool abort)
    {
        return internalDevice_->finalizeDeviceFlashUpdate(abort);
    }
}; // namespace micsmccli
