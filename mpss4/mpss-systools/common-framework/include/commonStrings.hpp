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

#ifndef MICMGMT_COMMONSTRINGS_HPP
#define MICMGMT_COMMONSTRINGS_HPP

#include <string>
#include <vector>

namespace micmgmt
{
    //////////////////////////////////////////////////////////
    /* Common Format for Options (Replace X)
     *  const std::string              commonOptXName;
     *  const std::vector<std::string> commonOptXHelp;
     *  const std::string              commonOptXArgName;
     *  const std::string              commonOptXArgDefVal;
     *  const std::vector<std::string> commonOptXArgHelp;
     */

    /* Common Format for Subcommands (Replace X and N; Very rare that these are common!!!)
     *  const std::string              commonSubXName;
     *  const std::vector<std::string> commonSubXHelp;
     *  const std::string              commonSubXPosNName;
     *  const std::vector<std::string> commonSubXPosNHelp;
     */

    //////////////////////////////////////////////////////////
    // <common empty help varaible>
    const std::vector<std::string> commonEmptyHelp = { };

    //////////////////////////////////////////////////////////
    // --help (Used automatically by CliParser classes)
    const std::string              commonOptHelpName = "help";
    const std::vector<std::string> commonOptHelpHelp = {
        "Display this help."
    };

    //////////////////////////////////////////////////////////
    // --version (Used automatically by CliParser classes)
    const std::string              commonOptVersionName = "version";
    const std::vector<std::string> commonOptVersionHelp = {
        "Display tool version."
    };

    //////////////////////////////////////////////////////////
    // --device
    const std::string              commonOptDeviceName = "device";
    const std::vector<std::string> commonOptDeviceHelp = {
        "Specify the device(s) to use. If omitted, 'all' is used."
    };
    const std::string              commonOptDeviceArgName = "device-list";
    const std::string              commonOptDeviceArgDefVal = "all";
    const std::vector<std::string> commonOptDeviceArgHelp = {
        "Device(s) id(s) in the form 'micX' where 'X' is the device number.\
         Lists and ranges are accepted.",

        "Examples:  'mic0-mic3', 'mic1,mic3-mic5', 'mic2'."
    };

    //////////////////////////////////////////////////////////
    // --xml
    const std::string              commonOptXmlName = "xml";
    const std::vector<std::string> commonOptXmlHelp = {
        "Create an XML output file containing the same information displayed to the console."
    };
    const std::string              commonOptXmlArgName = "xml-filename";
    const std::vector<std::string> commonOptXmlArgHelp = {
        "Specify the XML output file. If omitted, the file name will be the tool name with '.xml' extension in the current working directory."
    };

    //////////////////////////////////////////////////////////
    // --silent
    const std::string              commonOptSilentName = "silent";
    const std::vector<std::string> commonOptSilentHelp = {
        "Display only warnings and suppress remaining output to the console.",
    };

    //////////////////////////////////////////////////////////
    // --verbose
    const std::string              commonOptVerboseName = "verbose";
    const std::vector<std::string> commonOptVerboseHelp = {
        "Enable verbose in the output.",
    };

    //////////////////////////////////////////////////////////
    // --timeout
    const std::string              commonOptTimeoutName = "timeout";
    const std::vector<std::string> commonOptTimeoutHelp = {
        "Set a timeout. If timeout is reached and execution is not completed, the tool will exit with an error.",
    };
    const std::string              commonOptTimeoutArgName = "timeout-seconds";
    const std::vector<std::string> commonOptTimeoutArgHelp = {
        "The timeout in seconds. A value of 0 means infinite timeout."
    };

    //////////////////////////////////////////////////////////
    // error messages
    const std::string pathError = "Error: Environment variable INTEL_MPSS_HOME does not exist. Please restart"
                                  " parent process to reload environment variables or check if MPSS has been installed properly.\n";
}; // namespace micmgmt
#endif // MICMGMT_COMMONSTRINGS_HPP
