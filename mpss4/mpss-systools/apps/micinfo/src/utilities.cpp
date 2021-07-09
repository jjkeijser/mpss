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

#include <string>
#include <vector>
#include <iostream>
#include <tuple>
#include <ctime>
#include <iomanip>
#include <algorithm>

// COMMON FRAMEWORK
//
#include "CliParser.hpp"
#include "MicException.hpp"
#include "micmgmtCommon.hpp"
#include "MicOutput.hpp"
#include "ConsoleOutputFormatter.hpp"
#include "XmlOutputFormatter.hpp"
#include "MicLogger.hpp"
#include "commonStrings.hpp"

// MICDEVICE INCLUDE
// //
#include "MicValue.hpp"
#include "MicDeviceFactory.hpp"
#include "MicDeviceManager.hpp"
#include "MicDevice.hpp"
#include "MicDeviceInfo.hpp"
#include "MicDeviceDetails.hpp"
#include "MicDeviceError.hpp"
#include "MicCoreInfo.hpp"
#include "MicProcessorInfo.hpp"
#include "MicThermalInfo.hpp"
#include "MicTemperature.hpp"
#include "MicMemory.hpp"
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
#include "MicVersionInfo.hpp"
#include "MicFrequency.hpp"

#ifdef MICMGMT_MOCK_ENABLE
#include "MicDeviceError.hpp"
#include "MicBootConfigInfo.hpp"
#include "KnlMockDevice.hpp"
#endif

// MICINFO INCLUDE
//
#include "utilities.hpp"
#ifdef UNIT_TESTS
#include "ut-utilities.hpp"
#endif

using namespace micmgmt;
using namespace std;

namespace micinfo {
    const uint64_t BYTES_PER_MB = 1024 * 1024;
    const size_t SCALE_GIGA = 1000 * 1000 * 1000;
    const std::string::size_type OUTPUT_LENGTH = 30;

string dateTime() {
    time_t currentTime;
    time(&currentTime);
    stringstream out;
    const char* time;
#ifdef _WIN32
    // The return value string contains exactly 26 characters and has the form:
    // Wed Jan 02 02:03:55 1980\n\0
    static const int size = 26;
    char buffer[size];
    ctime_s(buffer, size, &currentTime);
    time = buffer;
#else
    time = ctime(&currentTime);
#endif
#ifdef UNIT_TESTS
    time = "Wed Jan 02 02:03:55 1980\n";
#endif
    out << "Created On " << time;
    return out.str();
}


void parseGroups(const CliParser& parser, Options& options) {
    /*Parse the command line to check if the "group" option is present or not*/
    if( get<0>(parser.parsedOption("group")) == false || // if the "group" option is not present or ...
        get<2>(parser.parsedOption("group")) == "all" )  // if the option is set to "all".
    {
        fillGroupList(options, true);
    }
    else
    {
        fillGroupList(options, false);
        auto grpList = parser.parsedOptionArgumentAsVector("group");
        grpList.erase( unique( grpList.begin(), grpList.end() ), grpList.end() );

        for (auto grp_list = grpList.begin(); grp_list != grpList.end(); ++grp_list)
        {
            if(*grp_list == "system")
                options.groups[GRP_SYS] = true;
            else if(*grp_list == "version")
                options.groups[GRP_VER] = true;
            else if(*grp_list == "board")
                options.groups[GRP_BOARD] = true;
            else if(*grp_list == "core")
                options.groups[GRP_CORES] = true;
            else if(*grp_list == "thermal")
                options.groups[GRP_THER] = true;
            else if(*grp_list == "memory")
                options.groups[GRP_GDDR] = true;
            else {
                stringstream sstr;
                sstr << "Invalid group name: " << *grp_list << ". Please refer to help to find correct group option.";
                throw MicException(sstr.str());
            }
        }
    }
}

void fillGroupList( Options& options, bool value ) {
    options.groups.reset();
    if (value)
        options.groups.flip();
}

template <class T>
string format( const T& in, bool valid, const string& leading, const string& trailing)
{
    if ( valid ) {
        stringstream sstr;
        sstr << fixed << setprecision(2) << leading << in << trailing;
        return sstr.str();
    } else {
        return NOT_AVAILABLE;
    }
}

template<class T>
string format( const MicValue<T>& value)
{
    return format(value.value(), value.isValid());
}

template <class T>
string formatUnit( const T& in, string unit, bool valid )
{
    return format(in, valid, "", unit);
}

template <class T>
string formatHex( const T& in, int width, bool valid )
{
    if ( valid ) {
        stringstream sstr;
        sstr << "0x" << setw(width) << setfill('0') << hex << in;
        return sstr.str();
    } else {
        return NOT_AVAILABLE;
    }
}

MicInfoApp::MicInfoApp(const MicDeviceManager& manager, ostream* outputStream, const unique_ptr<ostream>& xmlFStream, bool isSilent) :
        deviceManager(manager) {
    stdOut.reset( new ConsoleOutputFormatter(outputStream, OUTPUT_LENGTH));
    if(xmlFStream != NULL)
        xmlOut.reset( new XmlOutputFormatter(xmlFStream.get(), TOOLNAME));
    output.reset( new MicOutput( TOOLNAME, stdOut.get(), NULL, xmlOut.get()));
    output->setSilent(isSilent);
}

string MicInfoApp::basicInfo( const unique_ptr<MicDevice>& micDevice )
{
    stringstream out;
    out << "Device No: " << micDevice->deviceNum() << ", Device Name: " << micDevice->deviceName() << " [" << micDevice->deviceType() << "]";
    return out.str();
}

void MicInfoApp::dispatch(const Options &opts)
{
    uint32_t ret = MICSDKERR_SUCCESS;

#ifdef _WIN32
    // check if INTEL_MPSS_HOME enviromental variable is set
    char* buf = nullptr;
    size_t sz = 0;
    if (_dupenv_s(&buf, &sz, "INTEL_MPSS_HOME") != 0 || buf == nullptr)
    {
        output->outputError(pathError, false, true);
        LOG(ERROR_MSG, pathError);
        return;
    }
    else
    {
        free(buf);
    }
#endif

    output->outputLine("micinfo Utility Log");
    output->outputLine(dateTime());

    if(opts.groups[GRP_SYS])
        system_info();

    output->outputLine();

    // If we have a group referring to any device
    bool device_group = false;
    for (int i=1; i<GRP_SIZE ; i++ ) {
        if(opts.groups[i]) {
            device_group = true;
            break;
        }
    }

    if(device_group) {
        MicDeviceFactory deviceFactory( const_cast<MicDeviceManager*>(&deviceManager) );
        for ( unsigned int device_id = 0 ; device_id < opts.devices.size(); device_id++ )
        {
            bool validCardResult = deviceManager.isValidConfigCard(device_id);

            if ( validCardResult == false)
            {
                stringstream sstr;
                sstr << "mic" << device_id << ": Device not available";
                output->outputError(sstr.str(), validCardResult);
                LOG(ERROR_MSG, sstr.str());
                continue;
            }

            LOG(INFO_MSG, "Creating device %d", opts.devices[device_id]);
            unique_ptr<MicDevice> micDevice( deviceFactory.createDevice( opts.devices[device_id] ) );
            if( !micDevice )
            {
                stringstream sstr;
                sstr << "Error creating device " << opts.devices[device_id] << ": " <<
                        MicDeviceError::errorText( deviceFactory.errorCode());
                output->outputError(sstr.str(), deviceFactory.errorCode());
                LOG(ERROR_MSG, sstr.str());
                continue;
            }

            LOG(INFO_MSG, "Opening device %d", opts.devices[device_id]);
            ret = micDevice->open();
            if ( MicDeviceError::isError (ret) )
            {
                stringstream sstr;
                sstr << "Error opening device " << opts.devices[device_id] << ": " <<
                        MicDeviceError::errorText( ret);
                output->outputError(sstr.str(), ret);
                LOG(ERROR_MSG, sstr.str());
                continue;
            }
            output->outputLine(basicInfo( micDevice));

            for ( int group_id=1; group_id < GRP_SIZE ; group_id++ )
            {
                if(!opts.groups[group_id])
                    continue;

                show_select_info(micDevice, group_id);
            }
            output->outputLine();
            micDevice->close();
        }
    }

}

void MicInfoApp::system_info()
{
#ifndef UNIT_TESTS
    const string hostOS = micmgmt::hostOsType();
    const string hostOSVersion = micmgmt::hostOsVersion();
    const uint64_t hostMemory = micmgmt::hostMemory()/BYTES_PER_MB;
#else
    const string hostOS = "Test OS";
    const string hostOSVersion = "Test OS Version";
    const size_t hostMemory = 1000000;
#endif //UNIT_TESTS

    output->startSection("System Info");
    output->outputNameValuePair("Host OS", hostOS) ;
    output->outputNameValuePair("OS Version", hostOSVersion);

    output->outputNameValuePair("MPSS Version", deviceManager.mpssVersion().c_str());
    output->outputNameValuePair("Host Physical Memory", formatUnit(hostMemory, " MB", true));
    output->endSection();
}

void MicInfoApp::version_info( const unique_ptr<MicDevice>& micDevice)
{
    const MicDeviceDetails& devdetails = micDevice->deviceDetails();
    const MicPlatformInfo& platInfo = devdetails.micPlatformInfo();
    const MicDeviceInfo& devInfo = micDevice->deviceInfo();
    const MicVersionInfo& verInfo = devInfo.versionInfo();

    output->startSection("Version");
    if ( micDevice->deviceType() == "x100" )
    {
        output->outputNameValuePair("Flash Version", format(verInfo.flashVersion()));
        output->outputNameValuePair("SMC Boot Loader Version", format(verInfo.smcBootLoaderVersion()));
    }
    output->outputNameValuePair("SMC Firmware Version", format(verInfo.smcVersion()));
    output->outputNameValuePair("Coprocessor OS Version", format(platInfo.coprocessorOsVersion()));
    output->outputNameValuePair("Device Serial Number", format(platInfo.serialNo()));
    if ( micDevice->deviceType() == "x200" )
    {
        output->outputNameValuePair("BIOS Version", format(verInfo.biosVersion()));
        output->outputNameValuePair("BIOS Build date", format(verInfo.biosReleaseDate()));
        output->outputNameValuePair("ME Version", format(verInfo.meVersion()));
    }
    output->endSection();

}

void MicInfoApp::board_info( const unique_ptr<MicDevice>& micDevice )
{
    const MicDeviceInfo& devInfo = micDevice->deviceInfo();
    const MicPciConfigInfo& pci = devInfo.pciConfigInfo();
    const MicProcessorInfo& pinfo = devInfo.processorInfo();
    const MicDeviceDetails& devdetails = micDevice->deviceDetails();
    const MicPlatformInfo& platform = devdetails.micPlatformInfo();
    const MicMemoryInfo& minfo = devdetails.memoryInfo();

    output->startSection("Board");
    output->outputNameValuePair("Vendor ID", formatHex(pci.vendorId(), 4, pci.isValid()));
    output->outputNameValuePair("Device ID", formatHex(pci.deviceId(), 4, pci.isValid()));
    output->outputNameValuePair("Subsystem ID", formatHex(pci.subsystemId(), 4, pci.isValid()));
    output->outputNameValuePair("Coprocessor Stepping ID", formatHex(pinfo.steppingId().value(), 2,
            pinfo.steppingId().isValid()));

    if(isAdministrator())
        output->outputNameValuePair("UUID", format(platform.uuid()));
    else
        output->outputNameValuePair("UUID", INSUFFICIENT_PRIVILEGES);

    if ( pci.hasFullAccess() == true )
    {
        output->outputNameValuePair("PCIe Width", format(pci.linkWidth(), pci.isValid(), "x"));
        output->outputNameValuePair("PCIe Speed", formatUnit(pci.linkSpeed().scaledValue(MicSpeed::eGiga), " GT/s", pci.isValid()));
        output->outputNameValuePair("PCIe Ext Tag Field", (pci.isExtendedTagEnabled() ? "Enabled" : "Disabled"));
        output->outputNameValuePair("PCIe No Snoop", (pci.isNoSnoopEnabled() ? "Enabled" : "Disabled"));
        output->outputNameValuePair("PCIe Relaxed Ordering", (pci.isRelaxedOrderEnabled() ? "Enabled" : "Disabled"));
        output->outputNameValuePair("PCIe Max payload size", formatUnit(pci.payloadSize(), " bytes", pci.isValid()));
        output->outputNameValuePair("PCIe Max read request size", formatUnit(pci.requestSize(), " bytes", pci.isValid()));
    }
    else
    {
        output->outputNameValuePair("PCIe Width", INSUFFICIENT_PRIVILEGES);
        output->outputNameValuePair("PCIe Speed", INSUFFICIENT_PRIVILEGES);
        output->outputNameValuePair("PCIe Ext Tag Field", INSUFFICIENT_PRIVILEGES);
        output->outputNameValuePair("PCIe No Snoop", INSUFFICIENT_PRIVILEGES);
        output->outputNameValuePair("PCIe Relaxed Ordering", INSUFFICIENT_PRIVILEGES);
        output->outputNameValuePair("PCIe Max payload size", INSUFFICIENT_PRIVILEGES);
        output->outputNameValuePair("PCIe Max read request size", INSUFFICIENT_PRIVILEGES);
    }
    output->outputNameValuePair("Coprocessor Model", formatHex(pinfo.model().value(), 2,
            pinfo.model().isValid()));
    if ( micDevice->deviceType() == "x100" )
    {
        output->outputNameValuePair("Coprocessor Model Ext", formatHex(pinfo.extendedModel().value(), 2,
                pinfo.extendedModel().isValid()));
    }
    output->outputNameValuePair("Coprocessor Type", formatHex(pinfo.type().value(), 2,
            pinfo.type().isValid()));
    output->outputNameValuePair("Coprocessor Family", formatHex(pinfo.family().value(), 2,
            pinfo.family().isValid()));
    if ( micDevice->deviceType() == "x100" )
    {
        output->outputNameValuePair("Coprocessor Family Ext", formatHex(pinfo.extendedFamily().value(), 2,
                pinfo.extendedFamily().isValid()));
    }
    output->outputNameValuePair("Coprocessor Stepping", format(pinfo.stepping()));
    output->outputNameValuePair("Board SKU", format(platform.boardSku()));
    output->outputNameValuePair("ECC Mode", (minfo.isEccEnabled() ? "Enabled" : "Disabled"));
    if ( micDevice->deviceType() == "x100" )
    {
        output->outputNameValuePair("SMC HW Revision", format(platform.featureSet()));
    }
    output->outputNameValuePair("PCIe Bus Information", format(pci.address().asString(), pci.isValid()));
    output->outputNameValuePair("Coprocessor SMBus Address", formatHex((uint32_t)platform.smBusBaseAddress().value(), 8, platform.smBusBaseAddress().isValid()));
    if ( micDevice->deviceType() == "x200" )
    {
        output->outputNameValuePair("Coprocessor Brand", format(platform.coprocessorBrand()));
        output->outputNameValuePair("Coprocessor Board Type", format(platform.boardType()));
        output->outputNameValuePair("Coprocessor TDP", formatUnit(platform.maxPower(). normalizedValue(), " W", platform.isValid()));
    }
    output->endSection();
}

void MicInfoApp::core_info( const unique_ptr<MicDevice>& micDevice )
{
    const MicDeviceDetails& devdetails = micDevice->deviceDetails();
    const MicCoreInfo& cinfo = devdetails.coreInfo();

    output->startSection("Core");
    output->outputNameValuePair("Total No. of Active Cores", format(cinfo.coreCount()));
    output->outputNameValuePair("Threads per Core", format(cinfo.coreThreadCount()));
    output->outputNameValuePair("Voltage", formatUnit(cinfo.voltage().scaledValue(MicVoltage::eMilli), " mV", cinfo.isValid()));
    output->outputNameValuePair("Frequency", formatUnit(cinfo.frequency().scaledValue(MicFrequency::eGiga), " GHz", cinfo.isValid()));
    output->endSection();
}

void MicInfoApp::thermal_info( const unique_ptr<MicDevice>& micDevice )
{
    MicThermalInfo tinfo;
    const MicDeviceDetails& deviceDetails = micDevice->deviceDetails();
    const MicPlatformInfo& platformInfo = deviceDetails.micPlatformInfo();

    string fanRpm = NOT_AVAILABLE;
    string fanPwm = NOT_AVAILABLE;
    string cpuTemp = NOT_AVAILABLE;
    string dissipation = NOT_AVAILABLE;

    if (MicDeviceError::isSuccess( micDevice->getThermalInfo( &tinfo ) ))
    {
        auto cpuTempSensor = tinfo.sensorValueByName("Die");
        cpuTemp = formatUnit(cpuTempSensor.celsius(), " C", cpuTempSensor.isValid() && cpuTempSensor.isAvailable());

        if(platformInfo.isCoolingActive().value())
        {
            dissipation = "Active";
            fanRpm = format(tinfo.fanRpm());
            fanPwm = formatUnit(tinfo.fanPwm(), " %", tinfo.isValid());
        }
        else
        {
            dissipation = "Passive";
        }
    }

    output->startSection("Thermal");
    if ( micDevice->deviceType() == "x200" )
        output->outputNameValuePair("Thermal Dissipation", dissipation);

    output->outputNameValuePair("Fan RPM", fanRpm);
    output->outputNameValuePair("Fan PWM", fanPwm);
    output->outputNameValuePair("Die Temp", cpuTemp);
    output->endSection();
}

void MicInfoApp::memory_info( const unique_ptr<MicDevice>& micDevice )
{
    const MicDeviceDetails& devdetails = micDevice->deviceDetails();
    const MicMemoryInfo& minfo = devdetails.memoryInfo();

    output->startSection("Memory");
    if ( micDevice->deviceType() == "x100" )
    {
        output->outputNameValuePair("Vendor", format(minfo.vendorName(), minfo.isValid()));
        output->outputNameValuePair("Version", formatHex(minfo.revision(), 2, minfo.isValid()));
        output->outputNameValuePair("Density", formatUnit(minfo.density().scaledValue( MicMemory::eMega), " Mb", minfo.isValid()));
        output->outputNameValuePair("Size", formatUnit(minfo.size().scaledValue( MicMemory::eMega ), " MB", minfo.isValid()));
        output->outputNameValuePair("Technology", format(minfo.memoryType(), minfo.isValid()));
        output->outputNameValuePair("Speed", formatUnit(minfo.speed().normalizedValue() / SCALE_GIGA, " GT/s", minfo.isValid()));
        output->outputNameValuePair("Frequency", formatUnit(minfo.frequency().scaledValue( MicFrequency::eGiga ), " GHz", minfo.isValid()));
    }
    else
    {
        output->outputNameValuePair("Vendor", format(minfo.vendorName(), minfo.isValid()));
        output->outputNameValuePair("Size", formatUnit( minfo.size().scaledValue( MicMemory::eMega ), " MB", minfo.isValid()));
        output->outputNameValuePair("Technology", format(minfo.technology(), minfo.isValid()));
        /* TODO: Verify. I did some research about this. I found that the speed is the product of the clock frequency
         * by the number of rows per transfer (see SMBIOS type 20, Interleaved Data Depth).
         * The Interleaved Data Depth is 1 on current hardware. According to MCDRAM documentation, the speed of the current
         * stepping is 6.4 GT/s and the maximum will be 7.2 Gt/s. Verify rows per transfer remains in 1. */
        output->outputNameValuePair("Speed", formatUnit(minfo.speed().scaledValue(MicSpeed::eGiga), " GT/s", minfo.isValid()));
        output->outputNameValuePair("Frequency", formatUnit(minfo.frequency().scaledValue( MicFrequency::eGiga ), " GHz", minfo.isValid()));
    }
    output->endSection();
}

void MicInfoApp::show_select_info( const unique_ptr<MicDevice>& micDevice, int group_id)
{
    switch ( group_id )
    {
    case GRP_VER:
        version_info ( micDevice);
        break;
    case GRP_BOARD:
        board_info ( micDevice );
        break;
    case GRP_CORES:
        core_info ( micDevice );
        break;
    case GRP_THER:
        thermal_info ( micDevice );
        break;
    case GRP_GDDR:
        memory_info ( micDevice);
        break;
    default:
        break;
    }
}

}
