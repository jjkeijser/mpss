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

#include "MicSmcCliError.hpp"
#include <unordered_map>
#include <stdexcept>

namespace micsmccli
{
    using namespace std;
    namespace {
#define NEW_ERROR_STR(code,msg) std::pair<uint32_t, std::string>(code,msg)
        const unordered_map<uint32_t, std::string> errorMap_ = {
            NEW_ERROR_STR(eMicSmcCliGeneralError, "General error"),
            NEW_ERROR_STR(eMicSmcCliUnknownError, "Unknown error"),
            NEW_ERROR_STR(eMicSmcCliOutOfMemoryError, "Out of memory"),
            NEW_ERROR_STR(eMicSmcCliMpssNotPresent, "The coprocessor software is not installed on the current system"),
            NEW_ERROR_STR(eMicSmcCliDriverNotPresent, "The coprocessor driver is not currently loaded"),
            NEW_ERROR_STR(eMicSmcCliFailedToOpenXmlFile, "Failed to open the XML output file: "),
            NEW_ERROR_STR(eMicSmcCliBadDeviceList, "The coprocessor list supplied was not correctly formatted, please see the '--help' for details"),
            NEW_ERROR_STR(eMicSmcCliFailedToOpenLogFile, "Failed to open the log file"),
            NEW_ERROR_STR(eMicSmcCliInsufficientPrivileges, "Insufficient privileges to execute command"),
            NEW_ERROR_STR(eMicSmcCliTimeoutOcurred, "Tool time-out occurred"),
            NEW_ERROR_STR(eMicSmcCliCommandOptionPairIllegal, "The command/option combination is illegal"),
            NEW_ERROR_STR(eMicSmcCliDeviceOpenFailed, "Coprocessor failed to open"),
            NEW_ERROR_STR(eMicSmcCliDeviceCreateFailed, "Coprocessor failed to create"),
            NEW_ERROR_STR(eMicSmcCliDeviceBusy, "Coprocessor is in use by another application and is busy"),
            NEW_ERROR_STR(eMicSmcCliDeviceOptionIllegal, "The '--device' option cannot be used with the subcommand"),
            NEW_ERROR_STR(eMicSmcCliNoDevicesFound, "No coprocessors were found on this host platform"),
            NEW_ERROR_STR(eMicSmcCliDeviceNotOnline, "One or more of the coprocessor(s) are not online"),
            NEW_ERROR_STR(eMicSmcCliDevicesSpecifiedExceedCount, "No such device"),
            NEW_ERROR_STR(eMicSmcCliMutuallyExclusiveOptionsFound, "One or more mutually exclusive options were encountered on the command line"),
            NEW_ERROR_STR(eMicSmcCliUsageError, "There are no coprocessors detected, please run this tool on a system with coprocessors present and coprocessor software loaded and running"),
        };
#undef NEW_ERROR
    };

    MicSmcCliError::MicSmcCliError(const std::string& toolName) : toolName_(toolName)
    {
    }

    MicSmcCliError::~MicSmcCliError()
    {
    }

    uint32_t MicSmcCliError::signature() const
    {
        return 0x20080100; // This tool's full error code signature from HLD.
    }

    std::string MicSmcCliError::lookupError(uint32_t code) const
    {
        try
        {
            return errorMap_.at(code & ~E_MASK_);
        }
        catch (out_of_range&)
        {
            throw out_of_range("MicConfigError: The error code given was not found in the error list");
        }
    }

} // namespace micconfig
