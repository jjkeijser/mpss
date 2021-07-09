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

#include "MicFwError.hpp"
#include <unordered_map>
#include <stdexcept>

namespace micfw
{
    using namespace std;
    namespace {
        const unordered_map<uint32_t, std::string> errorMap_ = {
            std::pair<uint32_t, std::string>(MicFwErrorCode::eMicFwGeneralError, "General error"),
            std::pair<uint32_t, std::string>(MicFwErrorCode::eMicFwUnknownError, "Unknown error"),
            std::pair<uint32_t, std::string>(MicFwErrorCode::eMicFwOutOfMemoryError, "Out of memory"),
            std::pair<uint32_t, std::string>(MicFwErrorCode::eMicFwMpssNotPresent, "The MPSS software is not installed on the current system"),
            std::pair<uint32_t, std::string>(MicFwErrorCode::eMicFwDriverNotPresent, "The MPSS driver is not currently loaded"),
            std::pair<uint32_t, std::string>(MicFwErrorCode::eMicFwFailedToOpenXmlFile, "Failed to open the XML output file: "),
            std::pair<uint32_t, std::string>(MicFwErrorCode::eMicFwVerboseSilent, "Cannot use both '--silent' and '--verbose' on the same command line"),
            std::pair<uint32_t, std::string>(MicFwErrorCode::eMicFwNoDowngradeForceUpdate, "Cannot use both '--no-downgrade' and '--force-update' on the same command line"),
            std::pair<uint32_t, std::string>(MicFwErrorCode::eMicFwBadDeviceList, "The device list supplied was not correctly formatted, please see the '--help' for details"),
            std::pair<uint32_t, std::string>(MicFwErrorCode::eMicFwDeviceNotReady, "Devices not in 'Ready' state. Please set to 'Ready' state: "),
            std::pair<uint32_t, std::string>(MicFwErrorCode::eMicFwFailedToOpenLogFile, "Failed to open the log file"),
            std::pair<uint32_t, std::string>(MicFwErrorCode::eMicFwFlashImageNotFound, "Unable to find an appropriate flash image"),
            std::pair<uint32_t, std::string>(MicFwErrorCode::eMicFwInsufficientPrivileges, "Insufficient privileges to execute command"),
            std::pair<uint32_t, std::string>(MicFwErrorCode::eMicFwImageFileNotCompatible, "One or more of the devices did not find a compatible firmware image"),
            std::pair<uint32_t, std::string>(MicFwErrorCode::eMicFwBootloaderImageFileNotCompatible, "One or more of the devices did not find a compatible bootloader firmware image"),
            std::pair<uint32_t, std::string>(MicFwErrorCode::eMicFwAbortedByUser, "User aborted flashing, coprocessors in unknown states"),
            std::pair<uint32_t, std::string>(MicFwErrorCode::eMicFwTimeoutOcurred, "Tool timeout occured, coprocessors in unknown states"),
            std::pair<uint32_t, std::string>(MicFwErrorCode::eMicFwFlashFailed, "Flashing new firmware failed"),
            std::pair<uint32_t, std::string>(MicFwErrorCode::eMicFwFailedtoStartFlashing, "Failed to start flashing, make sure the tool is run with administrator privileges and that coprocessors should be in the 'ready' state"),
            std::pair<uint32_t, std::string>(MicFwErrorCode::eMicFwCommandOptionPairIllegal, "The command/option combination is illegal"),
            std::pair<uint32_t, std::string>(MicFwErrorCode::eMicFwDeviceOpenFailed, "Coprocessor failed to open"),
            std::pair<uint32_t, std::string>(MicFwErrorCode::eMicFwDeviceCreateFailed, "Coprocessor failed to create"),
            std::pair<uint32_t, std::string>(MicFwErrorCode::eMicFwDeviceListBad, "The devices passed on the command line exceeded the number of devices on the system"),
            std::pair<uint32_t, std::string>(MicFwErrorCode::eMicFwDeviceBusy, "Device is in use by another application and is busy"),
            std::pair<uint32_t, std::string>(MicFwErrorCode::eMicFwFileImageBad, "Invalid firmware image"),
            std::pair<uint32_t, std::string>(MicFwErrorCode::eMicFwReadyTimeoutOcurred, "Not all cards entered the 'Ready' state before the tool timeout period expired"),
            std::pair<uint32_t, std::string>(MicFwErrorCode::eMicFwDeviceNotOnline, "One or more of the devices specified are not 'online', please boot these devices: "),
            std::pair<uint32_t, std::string>(MicFwErrorCode::eMicAuthFailedVM, "Guest OS is not allowed to perform firmware update"),
            std::pair<uint32_t, std::string>(MicFwErrorCode::eMicFwMissMatchVersion, "Unexpected error when booting the system using updated firmware, system reverted the update process and it is back on the original firmware version. Please restart your host and try again or contact your Intel (R) representative for further assistance."),
        };
    };

    MicFwError::MicFwError(const std::string& toolName) : toolName_(toolName)
    {
    }

    MicFwError::~MicFwError()
    {
    }

    uint32_t MicFwError::signature() const
    {
        return 0x20070100; // This tool's full error code signature from HLD.
    }

    std::string MicFwError::lookupError(uint32_t code) const
    {
        try
        {
            return errorMap_.at(code & ~E_MASK_);
        }
        catch (out_of_range&)
        {
            throw out_of_range("MicFwError: The error code given was not found in the error list");
        }
    }

} // namespace micfw