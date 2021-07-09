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

#ifndef MICCONFIG_MICFWERROR_HPP
#define MICCONFIG_MICFWERROR_HPP

#include "MicErrorBase.hpp"

namespace micsmccli
{
    enum MicSmcCliErrorCode : uint32_t
    {
        eMicSmcCliSuccess = 0,
        eMicSmcCliGeneralError,
        eMicSmcCliUnknownError,
        eMicSmcCliOutOfMemoryError,
        eMicSmcCliMpssNotPresent,
        eMicSmcCliDriverNotPresent,
        eMicSmcCliFailedToOpenXmlFile,
        eMicSmcCliBadDeviceList,
        eMicSmcCliDeviceNotOnline,
        eMicSmcCliFailedToOpenLogFile,
        eMicSmcCliInsufficientPrivileges,
        eMicSmcCliTimeoutOcurred,
        eMicSmcCliCommandOptionPairIllegal,
        eMicSmcCliDeviceOpenFailed,
        eMicSmcCliDeviceCreateFailed,
        eMicSmcCliDeviceBusy,
        eMicSmcCliDeviceOptionIllegal,
        eMicSmcCliNoDevicesFound,
        eMicSmcCliDevicesSpecifiedExceedCount,
        eMicSmcCliMutuallyExclusiveOptionsFound,
        eMicSmcCliUsageError,
    };

    class MicSmcCliError : public micmgmt::MicErrorBase
    {
    public:
        MicSmcCliError(const std::string& toolName = "");
        virtual ~MicSmcCliError();

    protected: // derived class virtual implementation
        virtual uint32_t signature() const;
        virtual std::string lookupError(uint32_t code) const;

    private: // hide copy constructor and assignment operator so that this object cannot be copied.
        MicSmcCliError(const MicSmcCliError&);
        MicSmcCliError& operator=(const MicSmcCliError&);

    private: // Fields
        std::string toolName_;
    };
} // namespace micconfig
#endif // MICCONFIG_MICFWERROR_HPP
