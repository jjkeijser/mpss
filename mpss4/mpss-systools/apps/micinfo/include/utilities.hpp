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

#ifndef MICINFOAPP_HPP
#define MICINFOAPP_HPP

#include <string>
#include <vector>
#include <iostream>
#include <tuple>
#include <cstdlib>
#include <sstream>
#include <inttypes.h>
#include <memory>
#include <bitset>

#include "CliParser.hpp"
#include "micmgmtCommon.hpp"
#include "MicException.hpp"
#include "MicDeviceFactory.hpp"
#include "MicDeviceManager.hpp"
#include "MicDevice.hpp"
#include "MicDeviceInfo.hpp"
#include "MicCoreInfo.hpp"
#include "MicProcessorInfo.hpp"
#include "MicThermalInfo.hpp"
#include "MicTemperature.hpp"
#include "MicMemoryInfo.hpp"
#include "MicPciConfigInfo.hpp"
#include "MicPlatformInfo.hpp"
#include "MicMemoryUsageInfo.hpp"
#include "MicPowerUsageInfo.hpp"
#include "MicVoltageInfo.hpp"
#include "ThrottleInfo.hpp"
#include "PciAddress.hpp"
#include "MicVoltage.hpp"
#include "MicPower.hpp"
#include "MicSpeed.hpp"
#include "MicOutput.hpp"
#include "XmlOutputFormatter.hpp"
#include "ConsoleOutputFormatter.hpp"

#ifdef UNIT_TESTS
    #define PRIVATE public
#else
    #define PRIVATE private
#endif

namespace micinfo
{
const std::string TOOLNAME = "micinfo";
const std::string NOT_AVAILABLE = "Not Available";
const std::string INSUFFICIENT_PRIVILEGES = "Insufficient Privileges";

enum GRP
{
    GRP_SYS = 0,
    GRP_VER,
    GRP_BOARD,
    GRP_CORES,
    GRP_THER,
    GRP_GDDR,
    GRP_SIZE /* to identify last element */
};

struct Options {
    std::bitset<GRP_SIZE>     groups;
    std::vector<unsigned int> devices;
};

void fillGroupList( Options& options, bool value );
void parseGroups(const micmgmt::CliParser& parser, Options& options);
template <class T> std::string format( const T& in, bool valid, const std::string& leading="", const std::string& trailing="");
template <class T> std::string format( const micmgmt::MicValue<T>& value);
template <class T> std::string formatUnit( const T& in, std::string unit, bool valid );
template <class T> std::string formatHex( const T& in, int width, bool valid );

class MicInfoApp {

public:
    MicInfoApp(const micmgmt::MicDeviceManager& manager, std::ostream* outputStream, const std::unique_ptr<std::ostream>& xmlFStream, bool isSilent);
    void dispatch(const Options& opts);

 PRIVATE:
    const micmgmt::MicDeviceManager& deviceManager;
    std::unique_ptr<micmgmt::ConsoleOutputFormatter> stdOut;
    std::unique_ptr<micmgmt::XmlOutputFormatter> xmlOut;
    std::unique_ptr<micmgmt::MicOutput> output;

    /*Function to gain basic information*/
    /* Device name, Device No, Device Type*/
    std::string basicInfo(const std::unique_ptr<micmgmt::MicDevice>& micDevice);

    /*Function to print user required information*/
    void show_select_info(const std::unique_ptr<micmgmt::MicDevice>& micDevice, int group_id);

    /*Function to generate System Information*/
    void system_info();

    /*Function to generate Version Information*/
    void version_info(const std::unique_ptr<micmgmt::MicDevice>& micDevice);

    /*Function to generate Board Information*/
    void board_info(const std::unique_ptr<micmgmt::MicDevice>& micDevice);

    /*Function to generate Core Information*/
    void core_info(const std::unique_ptr<micmgmt::MicDevice>& micDevice);

    /*Function to generate Thermal Information*/
    void thermal_info(const std::unique_ptr<micmgmt::MicDevice>& micDevice);

    /*Function to generate Memory Info*/
    void memory_info(const std::unique_ptr<micmgmt::MicDevice>& micDevice);
};
}
#endif
