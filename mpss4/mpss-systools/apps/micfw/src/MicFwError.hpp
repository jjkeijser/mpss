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

#ifndef MICFW_MICFWERROR_HPP
#define MICFW_MICFWERROR_HPP

#include "MicErrorBase.hpp"

namespace micfw
{
    enum MicFwErrorCode : uint32_t
    {
        eMicFwSuccess = 0,
        eMicFwGeneralError,
        eMicFwUnknownError,
        eMicFwOutOfMemoryError,
        eMicFwMpssNotPresent,
        eMicFwDriverNotPresent,
        eMicFwFailedToOpenXmlFile,
        eMicFwVerboseSilent,
        eMicFwNoDowngradeForceUpdate,
        eMicFwBadDeviceList,
        eMicFwDeviceNotReady,
        eMicFwFailedToOpenLogFile,
        eMicFwFlashImageNotFound,
        eMicFwInsufficientPrivileges,
        eMicFwImageFileNotCompatible,
        eMicFwBootloaderImageFileNotCompatible,
        eMicFwAbortedByUser,
        eMicFwTimeoutOcurred,
        eMicFwFlashFailed,
        eMicFwFailedtoStartFlashing,
        eMicFwCommandOptionPairIllegal,
        eMicFwDeviceOpenFailed,
        eMicFwDeviceCreateFailed,
        eMicFwDeviceListBad,
        eMicFwDeviceBusy,
        eMicFwFileImageBad,
        eMicFwReadyTimeoutOcurred,
        eMicFwDeviceNotOnline,
        eMicAuthFailedVM,
        eMicFwMissMatchVersion,
        eMicFwUnknownFabVersion,
        eMicFwUpdateDevNotAvailable
    };

    class MicFwError : public micmgmt::MicErrorBase
    {
    public:
        MicFwError(const std::string& toolName = "");
        virtual ~MicFwError();

    protected: // derived class virtual implementation
        virtual uint32_t signature() const;
        virtual std::string lookupError(uint32_t code) const;

    private: // hide copy constructor and assignment operator so that this object cannot be copied.
        MicFwError(const MicFwError&);
        MicFwError& operator=(const MicFwError&);

    private: // Fields
        std::string toolName_;
    };
} // namespace micfw
#endif // MICFW_MICFWERROR_HPP