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

#include <string.h>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <iostream>
#include <tuple>
#include <fstream>
#include <map>
#include <list>

#include "CliParser.hpp"
#include "MicException.hpp"
#include "MicDeviceError.hpp"
#include "micmgmtCommon.hpp"
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
#include "ConsoleOutputFormatter.hpp"
#include "XmlOutputFormatter.hpp"
#include "MicLogger.hpp"
#include "utilities.hpp"
#include "commonStrings.hpp"

using namespace std;
using namespace micmgmt;
using namespace micinfo;


int main( int argc, char* argv[])
{
    const string description = "Display information of the installed Intel(R) Xeon Phi(TM) Coprocessor(s).\
    To get full information the coprocessor(s) should be in 'online' state and run with administrative privileges.";

    CliParser parser(argc, argv, mpssBuildVersion(), string(__DATE__).erase(0,7), "", description);

    /*Help for --group option*/
    vector<string> groupHelp = {
        "Specify the group name to extract particular group information."
    };

    /*Help for grouplist argument*/
    vector<string> groupListHelp = {
        "Valid groups are: 'system', 'version', 'board', 'core', 'thermal', 'memory' or any combination of them.",

        "Example:  'system,board,memory'."};

    unique_ptr<ostream> xmlFStream;
    Options opts;
    bool isSilent = false;
    // Parse command line
    try
    {
        /* Add all necessary options here */
        parser.addOption("group", groupHelp, 'g', true, "group-list", groupListHelp, "all");
        parser.addOption(commonOptDeviceName, commonOptDeviceHelp, 'd', true, commonOptDeviceArgName, commonOptDeviceArgHelp, "all");
        parser.addOption(commonOptVerboseName, commonOptVerboseHelp, 'v', false, "", commonEmptyHelp, "");

        if ( parser.parse() == false )
            return parser.parseError() ? 1 : 0;

        if (get<0>(parser.parsedOption(commonOptVerboseName)) == true)
            INIT_LOGGER(&cout);

        LOG(INFO_MSG, "Tool '" + parser.toolName() + "' started.");

        /* Create the DeviceManager Object & Initialize it*/
        MicDeviceManager deviceManager;
        uint32_t res = deviceManager.initialize();
        if ( MicDeviceError::isError( res ) )
        {
            cerr << MicDeviceError::errorText (res) << endl;
            return 1;
        }

        parseDevices(parser, opts.devices, deviceManager.deviceCount());

        parseGroups(parser, opts);

        MicInfoApp app(deviceManager, &cout, xmlFStream, isSilent);
        app.dispatch(opts);
    }
    catch ( MicException& ex )
    {
        cerr << ex.what() << endl;
        return 1;
    }
    return 0;
}
