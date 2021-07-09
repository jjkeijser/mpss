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

#include "ChangeSettingsWorkItem.hpp"

// SDK Includes
#include "MicDevice.hpp"
#include "MicPowerState.hpp"
#include "MicMemoryInfo.hpp"
#include "micsdkerrno.h"
#include "MicDeviceError.hpp"

// Common Framework Includes
#include "MsTimer.hpp"
#include "MicOutput.hpp"
#include "MicLogger.hpp"

// C++ Includes
#include <sstream>

using namespace std;
using namespace micmgmt;

namespace micsmccli
{
    ChangeSettingsWorkItem::ChangeSettingsWorkItem(MicOutput* output, MsTimer* timer, MicDevice* device, bool enable,
                                                   const vector<string>& settings,
                                                   map<string, uint32_t>& results)
                                                   : enable_(enable), timer_(timer), device_(device),
                                                     output_(output), settings_(settings), results_(results)
    {
    }

    ChangeSettingsWorkItem::ChangeSettingsWorkItem(const ChangeSettingsWorkItem& lhs)
        : enable_(lhs.enable_), timer_(lhs.timer_), device_(lhs.device_), output_(lhs.output_),
          settings_(lhs.settings_), results_(lhs.results_)
    {
    }

    ChangeSettingsWorkItem::~ChangeSettingsWorkItem()
    {
    }

    void ChangeSettingsWorkItem::Run(micmgmt::SafeBool& /*stopSignal*/)
    {
        for (auto it = settings_.begin(); it != settings_.end(); ++it)
        {
            string devType = device_->deviceType();
            MicDevice::DeviceState state = device_->deviceState();
            if (*it == "ecc")
            {
                if (devType == "x100" && state != MicDevice::DeviceState::eReady)
                {
                    results_[*it] = MICSDKERR_DEVICE_NOT_READY;
                }
                else if (devType == "x200" && state != MicDevice::DeviceState::eOnline)
                {
                    results_[*it] = MICSDKERR_DEVICE_NOT_ONLINE;
                }
                else
                {
                    if (devType == "x100")
                    {
                        output_->outputLine("Warning: " + device_->deviceName() + ": Setting ECC modes takes quite a long time!");
                    }
                    int stageError = -1;
                    if (timer_ != NULL)
                    {
                        int ms = (int)(((size_t)timer_->timerValue() - (size_t)timer_->elapsed().count()) & (size_t)0x7fffffff);
                        results_[*it] = device_->setEccModeEnabled(enable_, ms, &stageError);
                    }
                    else
                    {
                        results_[*it] = device_->setEccModeEnabled(enable_, 0, &stageError);
                    }
                    if (devType == "x100" && results_[*it] != 0)
                    {
                        stringstream ss;
                        ss << device_->deviceName() << ": Error '" << micmgmt::MicDeviceError::errorText(results_[*it] & 0xff) << "' occured with error stage value of " << stageError;
                        LOG(WARNING_MSG, ss.str());
                    }
                }
            }
            else if (*it == "turbo")
            {
                if (state != MicDevice::DeviceState::eOnline)
                {
                    results_[*it] = MICSDKERR_DEVICE_NOT_ONLINE;
                }
                else
                {
                    results_[*it] = device_->setTurboModeEnabled(enable_);
                }
            }
            else if (*it == "led")
            {
                if (state != MicDevice::DeviceState::eOnline)
                {
                    results_[*it] = MICSDKERR_DEVICE_NOT_ONLINE;
                }
                else
                {
                    results_[*it] = device_->setLedMode(enable_);
                }
            }
            else if (*it == "cpufreq")
            {
                if (devType != "x100")
                {
                    results_[*it] = MICSDKERR_NOT_SUPPORTED;
                }
                else
                {
                    if (state != MicDevice::DeviceState::eOnline)
                    {
                        results_[*it] = MICSDKERR_DEVICE_NOT_ONLINE;
                    }
                    else
                    {
                        list<MicPowerState> states;
                        states.push_back(MicPowerState(MicPowerState::State::eCpuFreq, enable_));
                        results_[*it] = device_->setPowerStates(states);
                    }
                }
            }
            else if (*it == "corec6")
            {
                if (devType != "x100")
                {
                    results_[*it] = MICSDKERR_NOT_SUPPORTED;
                }
                else
                {
                    if (state != MicDevice::DeviceState::eOnline)
                    {
                        results_[*it] = MICSDKERR_DEVICE_NOT_ONLINE;
                    }
                    else
                    {
                        list<MicPowerState> states;
                        states.push_back(MicPowerState(MicPowerState::State::eCoreC6, enable_));
                        results_[*it] = device_->setPowerStates(states);
                    }
                }
            }
            else if (*it == "pc3")
            {
                if (devType != "x100")
                {
                    results_[*it] = MICSDKERR_NOT_SUPPORTED;
                }
                else
                {
                    if (state != MicDevice::DeviceState::eOnline)
                    {
                        results_[*it] = MICSDKERR_DEVICE_NOT_ONLINE;
                    }
                    else
                    {
                        list<MicPowerState> states;
                        states.push_back(MicPowerState(MicPowerState::State::ePc3, enable_));
                        results_[*it] = device_->setPowerStates(states);
                    }
                }
            }
            else if (*it == "pc6")
            {
                if (devType != "x100")
                {
                    results_[*it] = MICSDKERR_NOT_SUPPORTED;
                }
                else
                {
                    if (state != MicDevice::DeviceState::eOnline)
                    {
                        results_[*it] = MICSDKERR_DEVICE_NOT_ONLINE;
                    }
                    else
                    {
                        list<MicPowerState> states;
                        states.push_back(MicPowerState(MicPowerState::State::ePc6, enable_));
                        results_[*it] = device_->setPowerStates(states);
                    }
                }
            }
        }
    }
}; // namespace micsmccli
