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

#ifndef MICSMCCLI_CHANGESETTINGSWORKITEM_HPP
#define MICSMCCLI_CHANGESETTINGSWORKITEM_HPP

// Common Framework Includes
#include "WorkItemInterface.hpp"

// C++ Includes
#include <map>
#include <string>
#include <vector>

namespace micmgmt // Forward references
{
    class MicOutput;
    class MicDevice;
    class SafeBool;
    class MsTimer;
}

namespace micsmccli
{
    class ChangeSettingsWorkItem : public micmgmt::WorkItemInterface
    {
    private:
        ChangeSettingsWorkItem();
        ChangeSettingsWorkItem& operator=(const ChangeSettingsWorkItem&);

    public:
        ChangeSettingsWorkItem(micmgmt::MicOutput* output, micmgmt::MsTimer* timer, micmgmt::MicDevice* device, bool enable,
                               const std::vector<std::string>& settings,
                               std::map<std::string,uint32_t>& results);
        ChangeSettingsWorkItem(const ChangeSettingsWorkItem& lhs);

        virtual ~ChangeSettingsWorkItem();

        virtual void Run(micmgmt::SafeBool& stopSignal);

    private: // Fields
        bool enable_;
        micmgmt::MsTimer* timer_;
        micmgmt::MicDevice* device_;
        micmgmt::MicOutput* output_;
        const std::vector<std::string>& settings_;
        std::map<std::string, uint32_t>& results_;
    };
}; // namespace micsmccli
#endif // MICSMCCLI_CHANGESETTINGSWORKITEM_HPP
