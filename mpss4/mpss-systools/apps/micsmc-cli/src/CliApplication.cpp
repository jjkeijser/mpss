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

// Allication Includes
#include "CliApplication.hpp"
#include "MicSmcCliError.hpp"
#include "ChangeSettingsWorkItem.hpp"

// Common Framework Includes
#include "CliParser.hpp"
#include "ConsoleOutputFormatter.hpp"
#include "XmlOutputFormatter.hpp"
#include "MicOutput.hpp"
#include "commonStrings.hpp"
#include "MicException.hpp"
#include "MicLogger.hpp"
#include "MsTimer.hpp"
#include "SafeBool.hpp"
#include "ThreadPool.hpp"

// SDK Includes
#include "MicDeviceManager.hpp"
#include "MicDeviceFactory.hpp"
#include "MicDevice.hpp"
#include "MicMemoryInfo.hpp"
#include "MicPowerState.hpp"
#include "MicDeviceDetails.hpp"
#include "MicVersionInfo.hpp"
#include "MicPlatformInfo.hpp"
#include "MicPciConfigInfo.hpp"
#include "MicCoreInfo.hpp"
#include "MicCoreUsageInfo.hpp"
#include "MicProcessorInfo.hpp"
#include "MicThermalInfo.hpp"
#include "MicTemperature.hpp"
#include "MicPowerUsageInfo.hpp"
#include "MicPower.hpp"
#include "MicFrequency.hpp"
#include "MicMemoryUsageInfo.hpp"
#include "MicMemory.hpp"
#include "micsdkerrno.h"
#include "MicDeviceError.hpp"
#ifdef MICMGMT_MOCK_ENABLE
#include "MicBootConfigInfo.hpp"
#include "KnlMockDevice.hpp"
#endif

// C++ Includes
#include <string>
#include <sstream>
#include <unordered_set>
#include <list>
#include <algorithm>
#include <limits>
#include <iomanip>
#include <cmath>
#include <cassert>
#include <map>

using namespace std;
using namespace micmgmt;

namespace
{
    typedef MicDeviceManager::DeviceType DT;

    const int    sNameWidth             = 25;
    const int    sIndentSize            = 3;
    const string sXmlFileExtension      = ".xml";
    const string sTimeoutSecondsDefault = "120";
    const char   sVariableDelimiter     = '$';
    const bool   sSuppressExtraLine     = false;


#ifdef _WIN32
    uint32_t sErrorMask = 0xffffffff;
#else
    uint32_t sErrorMask = 0x000000ff;
#endif

    class StatusData {
    public:
        StatusData() : name_(""), knlRead_(false), knlWrite_(false) {}
        StatusData(const std::string& n, bool lr, bool lw)
            : name_(n), knlRead_(lr), knlWrite_(lw) { }
        bool canRead() const { return knlRead_; }
        bool canWrite() const { return knlWrite_; }
        const std::string& name() const { return name_; }

    private:
        std::string name_;
        bool knlRead_;
        bool knlWrite_;
    }; // class StatusData
    map<string, StatusData> sStatusData;

    ///////////////////////////////////////////////////////////////////
    // OPTION: --info
    string sInfoOptionName = "info";
    char sInfoOptionShort = 'i';
    vector<string> sInfoOptionHelp = {
        "This 'show-data' filter option selects to display static coprocessor information for the \
        selected coprocessor(s).",
    };

    ///////////////////////////////////////////////////////////////////
    // OPTION: --cores
    string sCoresOptionName = "cores";
    char sCoresOptionShort = 'c';
    vector<string> sCoresOptionHelp = {
        "This 'show-data' filter option selects to display dynamic (instant) CPU core information for \
        the selected coprocessor(s).",
    };

    ///////////////////////////////////////////////////////////////////
    // OPTION: --mem
    string sMemOptionName = "mem";
    char sMemOptionShort = 'm';
    vector<string> sMemOptionHelp = {
        "This 'show-data' filter option selects to display dynamic (instance) memory usage information \
        for the selected coprocessor(s). Note that the displayed total memory also includes the memory \
        cached and buffered by the kernel, thus the sum of free and used memory will not add up to \
        the displayed total. See the documentation of /proc/meminfo for more information.",
    };

    ///////////////////////////////////////////////////////////////////
    // OPTION: --freq
    string sFreqOptionName = "freq";
    char sFreqOptionShort = 'f';
    vector<string> sFreqOptionHelp = {
        "This 'show-data' filter option selects to display dynamic (instance) frequency and power usage \
        for the selected coprocessor(s).",
    };

    ///////////////////////////////////////////////////////////////////
    // OPTION: --temp
    string sTempOptionName = "temp";
    char sTempOptionShort = 't';
    vector<string> sTempOptionHelp = {
        "This 'show-data' filter option selects to display dynamic (instant snapshot) temperature information for \
        the selected coprocessor(s).",
    };

    ///////////////////////////////////////////////////////////////////
    // OPTION: --all
    string sAllOptionName = commonOptDeviceArgDefVal;
    char sAllOptionShort = 'a';
    vector<string> sAllOptionHelp = {
        "This 'show-data' filter option is the equivalent of selection all of --info, --cores, --mem, --freq, \
        --temp for the selected coprocessor(s).",
    };

    ///////////////////////////////////////////////////////////////////
    // OPTION: --offline
    string sOfflineOptionName = "offline";
    vector<string> sOfflineOptionHelp = {
        "This 'show-data' filter option will display a comma separated list of offline coprocessor(s).",
    };

    ///////////////////////////////////////////////////////////////////
    // OPTION: --online
    string sOnlineOptionName = "online";
    vector<string> sOnlineOptionHelp = {
        "This 'show-data' filter option will display a comma separated list of online coprocessor(s).",
    };


    ///////////////////////////////////////////////////////////////////
    // SUBCOMMAND: show-data
    string sShowDataCmdName = "show-data";
    vector<string> sShowDataCmdHelp = {
        "This subcommand displays general information based on option switches for the selected coprocessor(s).",
        "The options are --info, --cores, --mem, --freq, --temp, --online, --offline, and \
        --all.  The --all switch is the default but excludes --online and --offline switches in the output.",
    };
    vector<string> sDisplayOptions = { sInfoOptionName, sTempOptionName, sFreqOptionName,
                                       sMemOptionName, sCoresOptionName, sOnlineOptionName,
                                       sOfflineOptionName };

    ///////////////////////////////////////////////////////////////////
    // SUBCOMMAND: status
    string sShowSettingsCmdName = "show-settings";
    vector<string> sStatusCmdHelp = {
        "This subcommand displays the current values of specific settings for the selected coprocessor(s).",
    };
    string sReadFilterArgName = "setting-read-list";
    vector<string> sReadFilterArgHelp = { // Uses a dynamic variable...
        "This argument must be one (or more separated by commas with no space) of $FILTER_READ$ or the \
        special word 'all' to display all setting values.",
    };

    ///////////////////////////////////////////////////////////////////
    // SUBCOMMAND: enable
    string sEnableCmdName = "enable";
    string sWriteFilterArgName = "setting-write-list";
    vector<string> sEnableCmdHelp = {
        "This subcommand enables the current values of one or more of the settings below for the selected \
        coprocessor(s).",
    };
    vector<string> sWriteFilterArgHelp = { // Uses a dynamic variable...
        "This argument must be one (or more separated by commas with no space) of $FILTER_WRITE$ or the special \
        word 'all' to enable all status values.",
    };
    string sEccException = "NOTE: 'ecc' must be the only filter argument specified when enabing or disabling. No \
                           other filters can be mixed with the filter argument. All other filters can be mixed \
                           with each other in one tool execution. To set 'ecc' the card must be in the 'ready' \
                           state not the online state. All other settings require the 'online' state to be set.";

    ///////////////////////////////////////////////////////////////////
    // SUBCOMMAND: disable
    string sDisableCmdName = "disable";
    vector<string> sDisableCmdHelp = {
        "This subcommand disables the current values of one or more of the settings below for the selected \
        coprocessor(s).",
    };
}; // empty namespace

namespace micsmccli
{
    CliApplication::CliApplication(int argc, char* argv[], const string& toolVersion, const string& toolName, const string& toolDescription)
        : platform_(DT::eDeviceTypeUndefined), timeoutSeconds_(0), handler_(NULL)
    {
#ifdef UNIT_TESTS
        utFakeAdministratorMode_ = false;
        utFailMPSS_ = false;
        utFailDriver_ = false;
#endif
        parser_ = make_shared<CliParser>(argc, argv, toolVersion, string(__DATE__).erase(0, 7), toolName, toolDescription);
        assert((bool)parser_ == true);

        stdOut_ = shared_ptr<MicOutputFormatterBase>(new ConsoleOutputFormatter(&cout, sNameWidth, sIndentSize));
        stdErr_ = shared_ptr<MicOutputFormatterBase>(new ConsoleOutputFormatter(&cerr, sNameWidth, sIndentSize));
        output_ = shared_ptr<MicOutput>(new MicOutput(parser_->toolName(), stdOut_.get(), stdErr_.get(), fileOut_.get()));

        parser_->setOutputFormatters(stdOut_.get(), stdErr_.get());

        deviceManager_ = shared_ptr<MicDeviceManager>(new MicDeviceManager());
        if ((deviceManagerInitError_ = deviceManager_->initialize()) == 0)
        {
            if (deviceManager_->deviceCount(MicDeviceManager::DeviceType::eDeviceTypeKnl) > 0)
            {
                platform_ = MicDeviceManager::DeviceType::eDeviceTypeKnl;
            }
        }

        if (sStatusData.size() == 0)
        {
            // Fill status data (bools in order are: knl read, knl write)...
            sStatusData["corec6"]  = StatusData("corec6",  false, false);
            sStatusData["cpufreq"] = StatusData("cpufreq", false, false);
            sStatusData["ecc"]     = StatusData("ecc",     true, false); // KNC Write ECC is LONG operation!
            sStatusData["led"]     = StatusData("led",     true,  true);
            sStatusData["pc6"]     = StatusData("pc6",     false, false);
            sStatusData["pc3"]     = StatusData("pc3",     false, false);
            sStatusData["turbo"]   = StatusData("turbo",   true,  true);
        }

        for (auto it = sStatusData.begin(); it != sStatusData.end(); ++it)
        {
            if (it->second.canRead() == true)
            {
                statusReadStrings_.push_back(it->first);
            }
            if (it->second.canWrite() == true)
            {
                statusWriteStrings_.push_back(it->first);
            }
        }

        if (platform_ != DT::eDeviceTypeUndefined)
        {
            // Common Switch Options
            //parser_->addOption(commonOptXmlName, commonOptXmlHelp, 0, false, commonOptXmlArgName, commonOptXmlArgHelp, parser_->toolName() + sXmlFileExtension, this);
            //parser_->addOption(commonOptSilentName, commonOptSilentHelp, 0, false, "", commonEmptyHelp, "");
            //parser_->addOption(commonOptTimeoutName, commonOptTimeoutHelp, 0, true, commonOptTimeoutArgName, commonOptTimeoutArgHelp, "", this);
            parser_->addOption(commonOptDeviceName, commonOptDeviceHelp, 'd', true, commonOptDeviceArgName, commonOptDeviceArgHelp, commonOptDeviceArgDefVal, this);
            parser_->addOption(commonOptVerboseName, commonOptVerboseHelp, 'v', false, "", commonEmptyHelp, "");

            // Display Filter Options
            parser_->addOption(sInfoOptionName, sInfoOptionHelp, sInfoOptionShort, false, "", commonEmptyHelp, "");
            parser_->addOption(sTempOptionName, sTempOptionHelp, sTempOptionShort, false, "", commonEmptyHelp, "");
            parser_->addOption(sFreqOptionName, sFreqOptionHelp, sFreqOptionShort, false, "", commonEmptyHelp, "");
            parser_->addOption(sMemOptionName, sMemOptionHelp, sMemOptionShort, false, "", commonEmptyHelp, "");
            parser_->addOption(sCoresOptionName, sCoresOptionHelp, sCoresOptionShort, false, "", commonEmptyHelp, "");
            parser_->addOption(sAllOptionName, sAllOptionHelp, sAllOptionShort, false, "", commonEmptyHelp, "");
            parser_->addOption(sOfflineOptionName, sOfflineOptionHelp, 0, false, "", commonEmptyHelp, "");
            parser_->addOption(sOnlineOptionName, sOnlineOptionHelp, 0, false, "", commonEmptyHelp, "");

            // Subcommands (read)
            parser_->addSubcommand(sShowDataCmdName, sShowDataCmdHelp);

            parser_->addSubcommand(sShowSettingsCmdName, sStatusCmdHelp);
            parser_->addSubcommandPositionalArg(sShowSettingsCmdName, sReadFilterArgName, stringVariableSubstitution(sReadFilterArgHelp), this);

            // Subcommands (write)
            vector<string> specialHelp = stringVariableSubstitution(sWriteFilterArgHelp);

            parser_->addSubcommand(sEnableCmdName, sEnableCmdHelp);
            parser_->addSubcommandPositionalArg(sEnableCmdName, sWriteFilterArgName, specialHelp, this);
            parser_->addSubcommandPositionalArg(sEnableCmdName, commonOptDeviceArgName, commonOptDeviceArgHelp);

            parser_->addSubcommand(sDisableCmdName, sDisableCmdHelp);
            parser_->addSubcommandPositionalArg(sDisableCmdName, sWriteFilterArgName, specialHelp, this);
            parser_->addSubcommandPositionalArg(sDisableCmdName, commonOptDeviceArgName, commonOptDeviceArgHelp);
        }

        // --all is the implied default...
        for (auto it = sDisplayOptions.begin(); it != sDisplayOptions.end(); ++it)
        {
            displayFilter_[*it] = true;
        }
    }

    CliApplication::~CliApplication()
    {
        for (auto it = deviceList_.begin(); it != deviceList_.end(); ++it)
        {
            MicDevice* device = static_cast<MicDevice*>(it->get());
            if (device->isOpen() == true)
            {
                device->close();
            }
        }
        parser_.reset();
        output_.reset();
        stdErr_.reset();
        stdOut_.reset();
        if ((bool)fileOut_ == true)
        {
            fileOut_.reset();
            xmlFile_->close();
            xmlFile_.reset();
        }
    }

    void CliApplication::setTrapSignalHandler(micmgmt::trapSignalHandler handler)
    {
        handler_ = handler;
    }

    int CliApplication::run(unsigned int toolTimeoutSeconds, const DeviceImpls& deviceImpls)
    {
#ifdef UNIT_TESTS
        utMockDeviceImplementations_.insert(utMockDeviceImplementations_.end(), deviceImpls.begin(), deviceImpls.end());
#else
        (void)deviceImpls;
#endif
        timeoutSeconds_ = toolTimeoutSeconds;

        if (platform_ == DT::eDeviceTypeUndefined)
        {
            output_->outputLine("*** Warning: " + parser_->toolName() + ": There were no driver or coprocessors detected! The tool functionality is greatly reduced.");
        }

        if (parser_->parse() == false)
        {
            return 1;
        }

        // Any enviromental reason to not continue?
        if (platform_ == MicDeviceManager::DeviceType::eDeviceTypeUndefined)
        {
#ifdef UNIT_TESTS // Application level failures involving MicDeviceManager.
            if (utFailMPSS_ == true)
            {
                return buildError(MicSmcCliErrorCode::eMicSmcCliMpssNotPresent);
            }
            else if (utFailDriver_ == true)
            {
                return buildError(MicSmcCliErrorCode::eMicSmcCliDriverNotPresent);
            }
#endif
            switch (deviceManagerInitError_ & 0x000000ff)
            {
            case MICSDKERR_NO_MPSS_STACK:
                return buildError(MicSmcCliErrorCode::eMicSmcCliMpssNotPresent);
            case MICSDKERR_DRIVER_NOT_LOADED:
            case MICSDKERR_DRIVER_NOT_INITIALIZED:
                return buildError(MicSmcCliErrorCode::eMicSmcCliDriverNotPresent);
            default:
                return buildError(MicSmcCliErrorCode::eMicSmcCliUsageError);
            }
        }

        // --verbose handling
        if (get<0>(parser_->parsedOption(commonOptVerboseName)) == true)
            INIT_LOGGER(&cout);
        LOG(INFO_MSG, "Tool '" + parser_->toolName() + "' started.");

        // --xml handling
        auto tup = parser_->parsedOption(commonOptXmlName);
        string xmlFile = parser_->toolName() + sXmlFileExtension;
        if (get<0>(tup) == true)
        {
            string xmlFile = parser_->toolName() + sXmlFileExtension;
            if (get<1>(tup) == true)
            {
                xmlFile = get<2>(tup);
                if (micmgmt::pathIsFolder(xmlFile) == true)
                {
                    if (xmlFile[xmlFile.size() - 1] != filePathSeparator()[0])
                    {
                        xmlFile += filePathSeparator();
                    }
                    xmlFile += parser_->toolName() + sXmlFileExtension;
                }
            }
            ifstream ifs(xmlFile);
            if (ifs.good() == true)
            {
                ifs.close();
                return buildError(MicSmcCliErrorCode::eMicSmcCliFailedToOpenXmlFile, "file already exists: " + xmlFile);
            }

            ofstream* ofs = new ofstream(xmlFile);
            if (ofs->bad() == true || ofs->fail() == true)
            {
                return buildError(MicSmcCliErrorCode::eMicSmcCliFailedToOpenXmlFile, "could not open file: " + xmlFile);
            }
            else
            {
                xmlFile_ = shared_ptr<ofstream>(ofs);
                fileOut_ = shared_ptr<MicOutputFormatterBase>(new XmlOutputFormatter(xmlFile_.get(), parser_->toolName()));
                output_ = make_shared<MicOutput>(parser_->toolName(), stdOut_.get(), stdErr_.get(), fileOut_.get());
            }
        }

        // --silent handling
        tup = parser_->parsedOption(commonOptSilentName);
        if (get<0>(tup) == true)
        {
            output_->setSilent(true);
#ifndef UNIT_TESTS
            if (get<0>(parser_->parsedOption(commonOptXmlName)) == false)
            {
                cerr << "Warning: '--" + commonOptSilentName + "' was specifed without the '--" + commonOptXmlName + "' option, there will be no further output." << endl;
            }
#endif
        }

        // --timeout handling
        tup = parser_->parsedOption(commonOptTimeoutName);
        if (get<0>(tup) == true)
        {
            stringstream ss;
            ss.str(get<2>(tup));
            ss >> timeoutSeconds_;
            timeoutTimer_ = shared_ptr<MsTimer>(new MsTimer(timeoutSeconds_ * 1000));
        }
        else if (timeoutSeconds_ > 0)
        {
            timeoutTimer_ = shared_ptr<MsTimer>(new MsTimer(timeoutSeconds_ * 1000));
        }

        // --device or device-list position parameter handling
        if (parser_->parsedSubcommand().empty() == true)
        { // options only...
            buildDeviceListFromDeviceOption();
        }
        else
        { // subcommand present...
            if (parser_->parsedSubcommand() == sEnableCmdName || parser_->parsedSubcommand() == sDisableCmdName)
            {
#ifdef UNIT_TESTS
                if (utFakeAdministratorMode_ == false)
#else
                if (micmgmt::isAdministrator() == false) // First place where required privileges are known...
#endif // UNIT_TESTS
                {
                    return buildError(MicSmcCliErrorCode::eMicSmcCliInsufficientPrivileges, ": '" + sEnableCmdName +
                                                                                            "' or '" + sDisableCmdName +
                                                                                            "' require administrator privileges");
                }
                buildDeviceListFromPositionalArgument(1);
            }
            else
            {
                buildDeviceListFromDeviceOption();
            }
        }

        if (deviceNameList_.size() == 0)
        {
            return buildError(MicSmcCliErrorCode::eMicSmcCliBadDeviceList);
        }

        // Special error condition checks...
        {
            int error = 0;

            error = checkSubcommandAndptionUsage();

            if (error != 0)
            {
                return error & sErrorMask;
            }
        }

        // Setup display filter...
        string action = parser_->parsedSubcommand();
        if (action == sShowDataCmdName)
        {
            if (get<0>(parser_->parsedOption(sAllOptionName)) == false)
            {
                vector<string> displayOpts = { sInfoOptionName, sTempOptionName, sFreqOptionName,
                                               sMemOptionName, sCoresOptionName, sOnlineOptionName,
                                               sOfflineOptionName };
                bool clearDefault = false;
                for (auto it = displayOpts.begin(); it != displayOpts.end(); ++it)
                {
                    auto opt = parser_->parsedOption(*it);
                    if (get<0>(opt) == true)
                    {
                        clearDefault = true;
                        break;
                    }
                }
                for (auto it = displayOpts.begin(); it != displayOpts.end(); ++it)
                {
                    auto opt = parser_->parsedOption(*it);
                    if (clearDefault == true)
                    {
                        displayFilter_[*it] = false;
                    }
                    if (get<0>(opt) == true)
                    {
                        displayFilter_[*it] = true;
                    }
                }
            }
        }

        // Device creation...
        int error = MICSDKERR_INTERNAL_ERROR;
        try
        {
            createAndOpenDevices(&error);
        }
        catch(const MicException& excp)
        {
            output_->outputError(excp.what(), excp.getMicErrNo());
            if (excp.getMicErrNo() == MICSDKERR_NO_SUCH_DEVICE)
                return MICSDKERR_NO_SUCH_DEVICE;
        }

        if (deviceList_.empty())
            return MICSDKERR_NO_DEVICES;

        // Dispatch action
        if (action == sShowDataCmdName)
        {
            return displayData() + error;
        }
        else if (action == sShowSettingsCmdName)
        {
            return displaySettings() + error;
        }
        else if (action == sEnableCmdName)
        {
            return enableSettings() + error;
        }
        else if (action == sDisableCmdName)
        {
            return disableSettings() + error;
        }
        return buildError(MicSmcCliErrorCode::eMicSmcCliUnknownError, ": no actions were perfomed");
    }

    bool CliApplication::validateDeviceList(const std::string& list) const
    {
        if (micmgmt::trim(list) == "all")
            return true;

        bool fail = true;
        micmgmt::deviceNamesFromListRange(list, &fail);
        return !fail;
    }

    std::vector<std::string> CliApplication::makeListFromCommaSeparatedList(const std::string& sList) const
    {
        vector<string> newList;

        size_t current = 0;
        size_t pos = sList.find(',');
        if (pos != string::npos)
        { // comma(s) present
            while (pos != string::npos)
            {
                string item = trim(sList.substr(current, pos - current));
                if (item.empty() == false)
                {
                    newList.push_back(item);
                }
                current = pos + 1;
                pos = sList.find(',', current);
                if (pos == string::npos)
                {
                    item = trim(sList.substr(current));
                    if (item.empty() == false)
                    {
                        newList.push_back(item);
                    }
                }
            }
        }
        else
        { // no comma
            if (sList.empty() == false)
            {
                string singleItem = trim(sList);
                if (singleItem.empty() == false)
                {
                    newList.push_back(singleItem);
                }
            }
        }
        return newList;
    }

    bool CliApplication::checkArgumentValue(EvaluateArgumentType type,
                                            const std::string& typeName,
                                            const std::string& argName,
                                            const std::string& value)
    {
        bool rv = true;
        if (type == EvaluateArgumentType::eOptionArgument)
        { // pre-process option arguments
            if (typeName == commonOptDeviceArgName)
            {
                rv = validateDeviceList(value);
            }
            else if (typeName == commonOptXmlArgName)
            {
                // Do nothing right now, this may change...PASS
            }
            else if (typeName == commonOptTimeoutArgName)
            {
                if (value.find_first_not_of("0123456789") != string::npos)
                {
                    rv = false;
                }
                else
                {
                    unsigned int val = static_cast<unsigned int>(stoul(value));
                    if (val > 3600) // Cap value at 1 hour (needs to be a somewhat realistic value).
                    {
                        rv = false;
                    }
                }
            }
            else
            {
                rv = false; // Fail unknown option argument names.
            }
        }
        else if (type == EvaluateArgumentType::ePositionalArgument)
        { // pre-process subcommand positional arguments
            if (argName == sEnableCmdName || argName == sDisableCmdName)
            { // write operations
                if (typeName == sWriteFilterArgName)
                {
                    if (value != commonOptDeviceArgDefVal)
                    {
                        vector<string> list = makeListFromCommaSeparatedList(value);
                        if (list.size() > 0)
                        {
                            unordered_set<string> found;
                            for (auto it = list.begin(); it != list.end(); ++it)
                            {
                                for (auto it2 = statusWriteStrings_.begin(); it2 != statusWriteStrings_.end(); ++it2)
                                {
                                    if (*it2 == toLower(*it))
                                    {
                                        found.insert(toLower(*it));
                                        break;
                                    }
                                }
                            }
                            rv = (found.size() == list.size());
                        }
                        else
                        {
                            rv = false;
                        }
                    }
                }
                else if (typeName == commonOptDeviceArgName)
                {
                    rv = validateDeviceList(value);
                }
                else
                {
                    rv = false; // Fail unknown subcommand positional argument names.
                }
            }
            else
            { // read operations
                if (typeName == sReadFilterArgName)
                {
                    if (value != commonOptDeviceArgDefVal)
                    {
                        vector<string> list = makeListFromCommaSeparatedList(value);
                        if (list.size() > 0)
                        {
                            unordered_set<string> found;
                            for (auto it = list.begin(); it != list.end(); ++it)
                            {
                                for (auto it2 = statusReadStrings_.begin(); it2 != statusReadStrings_.end(); ++it2)
                                {
                                    if (*it2 == toLower(*it))
                                    {
                                        found.insert(toLower(*it));
                                        break;
                                    }
                                }
                            }
                            rv = (found.size() == list.size());
                        }
                        else
                        {
                            rv = false;
                        }
                    }
                }
                else
                {
                    rv = false; // Fail unknown subcommand positional argument names.
                }
            }

        }
        return rv;
    }


    int CliApplication::buildError(uint32_t code, const std::string& specialMessage)
    {
        MicSmcCliError e(parser_->toolName());
        auto err = e.buildError(code, specialMessage);
        output_->outputError(err.second, err.first);
        return ((int)code > 0)? 1:0;
    }

    micmgmt::MicException CliApplication::buildException(uint32_t code, const std::string& specialMessage) const
    {
        MicSmcCliError err(parser_->toolName());
        return MicException(err.buildError(code, specialMessage));
    }

    void CliApplication::buildDeviceListFromDeviceOption()
    {
        if (get<2>(parser_->parsedOption(commonOptDeviceName)) == commonOptDeviceArgDefVal)
        {
            buildDeviceListAll();
        }
        else
        {
            deviceNameList_ = parser_->parsedOptionArgumentAsDeviceList(commonOptDeviceName);
        }
    }

    void CliApplication::buildDeviceListFromPositionalArgument(size_t position)
    {
        if (parser_->parsedPositionalArgumentAt(position) == commonOptDeviceArgDefVal)
        {
            buildDeviceListAll();
        }
        else
        {

            deviceNameList_ = parser_->parsedPositionalArgumentAsDeviceList(position);
        }
    }

    void CliApplication::buildDeviceListAll()
    {
        size_t deviceCount = deviceManager_->deviceCount(platform_);
        deviceNameList_.clear();
        deviceNameList_.resize(deviceCount);
        stringstream ss;
        for (size_t i = 0; i < deviceCount; ++i)
        {
            ss.str(""); // clear
            ss << deviceManager_->deviceBaseName() << i;
            deviceNameList_[i] = ss.str();
        }
    }

    int CliApplication::deviceNumFromName(const string& name) const
    {
        size_t pos = name.find_first_of("0123456789");
        if (pos != string::npos)
        {
            return std::atoi(name.substr(pos).c_str());
        }
        else
        {
            throw runtime_error("CliApplication::deviceNumFromName: Malformed device name");
        }
    }

    void CliApplication::createAndOpenDevices(int* error)
    {
        MicDeviceFactory factory(deviceManager_.get());
        deviceList_.clear();
        int deviceError = 0;
        int internalError = 0;
        for (auto cardName : deviceNameList_)
        {
            deviceError = 0;
#ifdef UNIT_TESTS
            auto devNum = deviceNumFromName(cardName);
            // Klocwork needs this check...
            if (static_cast<uint32_t>(devNum) >= deviceManager_->deviceCount())
                throw buildException(MicSmcCliErrorCode::eMicSmcCliDevicesSpecifiedExceedCount,
                        ": " + cardName);
            MicDevice* device = factory.createDevice(this->utMockDeviceImplementations_[devNum]);
#else
            MicDevice* device = factory.createDevice(cardName);
#endif
            if ((deviceError = factory.errorCode()) != MICSDKERR_SUCCESS ||
                (device == NULL) || // Short-circuit if NULL, avoid dereferencing below
                (deviceError = device->open()) != MICSDKERR_SUCCESS)
            {
                if(deviceError == MICSDKERR_NO_SUCH_DEVICE)
                {
                    // Abort if non-existent device has been requested
                    if (error)
                        *error = MICSDKERR_NO_SUCH_DEVICE;
                    deviceList_.clear();
                    throw buildException(
                        MicSmcCliErrorCode::eMicSmcCliDevicesSpecifiedExceedCount,
                                         ": " + cardName);
                }
                // Report error and continue
                std::stringstream ss;
                ss << cardName << ": " <<
                    MicDeviceError::errorText(deviceError);
                output_->outputError(ss.str(), deviceError);
                internalError = deviceError;
                continue; // Attempt to create more devices.
            }

            // So far, so good. Add device to list.
            deviceList_.push_back(shared_ptr<MicDevice>(device));
        }


        if (error)
            *error = internalError;
    }

    std::string CliApplication::createReadListString(const std::string& conjunction) const
    {
        size_t count = statusReadStrings_.size();
        stringstream ss;
        for (size_t i = 0; i < count; ++i)
        {
            if (i > 0)
            {
                if (count > 2)
                {
                    ss << ", ";
                }
                if (i == (count - 1) && conjunction.empty() == false)
                {
                    ss << conjunction << " ";
                }
            }
            ss << "'" << statusReadStrings_[i] << "'";
        }
        return ss.str();
    }

    std::string CliApplication::createWriteListString(const std::string& conjunction) const
    {
        size_t count = statusWriteStrings_.size();
        if (count == 0)
        {
            return string(); // no items to print
        }
        stringstream ss;
        for (size_t i = 0; i < (count - 1); ++i)
        {
            ss << "'" << statusWriteStrings_[i] << "'";
            if (i < (count - 2)) // To get here; count must be at least 2 (no underflow)
            {
                ss << ", "; // To get here; count must be at least 3
            }
        }
        if (count > 1 && conjunction.empty() == false)
        {
            ss << " " << conjunction << " "; // use conjunction on at least 2 items.
        }
        ss << "'" << statusWriteStrings_[count - 1] << "'";
        return ss.str();
    }

    std::string CliApplication::stringVariableSubstitution(const std::string& str) const
    {
        string localStr = str;
        size_t current = 0;
        size_t varStart = localStr.find(sVariableDelimiter);
        while (varStart != string::npos)
        {
            string var;
            size_t varEnd = localStr.find(sVariableDelimiter, varStart + 1);
            if (varEnd != string::npos)
            {
                var = localStr.substr(varStart + 1, varEnd - varStart - 1);
                if (var.empty() == false)
                {
                    localStr.erase(varStart, varEnd - varStart + 1);
                    if (var == "FILTER_READ")
                    {
                        string newText = createReadListString("or");
                        localStr.insert(varStart, newText);
                        current = varStart + newText.size();
                    }
                    else if (var == "FILTER_WRITE")
                    {
                        string newText = createWriteListString("or");
                        localStr.insert(varStart, newText);
                        current = varStart + newText.size();
                    }
                    else
                    {
                        current = varStart;
                    }
                }
            }
            else
            {
                break;
            }
            varStart = localStr.find(sVariableDelimiter, current);
        }
        return localStr;
    }

    std::vector<std::string> CliApplication::stringVariableSubstitution(const std::vector<std::string>& strVector) const
    {
        vector<string> newVector;
        for (auto it = strVector.begin(); it != strVector.end(); ++it)
        {
            newVector.push_back(stringVariableSubstitution(*it));
        }
        return newVector;
    }

    int CliApplication::checkSubcommandAndptionUsage()
    {
        string subcommand = parser_->parsedSubcommand();
        auto deviceOpt = parser_->parsedOption(commonOptDeviceName);

        // --device used for write commands?
        if (subcommand.empty() == false &&
            (subcommand == sEnableCmdName || subcommand == sDisableCmdName)
            && get<0>(deviceOpt) == true)
        {
            stringstream ss;
            ss << ": --" << commonOptDeviceName << " used for with '" << subcommand << "'";
            return buildError(MicSmcCliErrorCode::eMicSmcCliCommandOptionPairIllegal, ss.str());
        }
        return 0;
    }

    string CliApplication::msgFromSDKError(uint32_t error)
    {
        stringstream ss;
        ss << "Failed: " << MicDeviceError::errorText(error & 0xff);
        return ss.str();
    }

    std::tuple<double, double, double> CliApplication::percentOfWorkload(uint64_t potentialWork, uint64_t systemWork, uint64_t userWork, uint64_t idleWork)
    {
        double idlePercent;
        double systemPercent;
        double userPercent;
        double idlePercentCalc;

        systemPercent = ((double)systemWork * 100.0) / (double)potentialWork;
        userPercent = ((double)userWork * 100.0) / (double)potentialWork;
        idlePercent = ((double)idleWork * 100.0) / (double)potentialWork;
        idlePercentCalc = 100.0 - userPercent - systemPercent;

        if (fabs(idlePercent - idlePercentCalc) > 1.0) // error tolerance = 1%
        {
            stringstream ss;
            ss << "Calculations for core utilization are off by " << setprecision(2) << std::abs(idlePercent - idlePercentCalc) << "%";
            LOG(INFO_MSG, ss.str());
            idlePercent = idlePercentCalc; // something is wrong use calculated percent.
        }

        return tuple<double, double, double>(userPercent, systemPercent, idlePercent);
    }

    // Action Methods
    int CliApplication::displayData()
    {
        stringstream ss;
        int rv = 0;
        if (get<0>(parser_->parsedOption(sOnlineOptionName)) == true)
        {
            string cards = "";
            for (auto it = deviceList_.begin(); it != deviceList_.end(); ++it)
            {
                MicDevice* device = it->get();
                MicDevice::DeviceState state = device->deviceState();
                if (state == MicDevice::DeviceState::eOnline)
                {
                    if (cards.empty() == false)
                    {
                        cards += ",";
                    }
                    cards += device->deviceName();
                }
            }
            if (cards.empty() == true)
            {
                cards = "None";
            }
            output_->outputNameValuePair("Online Cards", cards);
            displayFilter_[sOnlineOptionName] = false;
        }
        if (get<0>(parser_->parsedOption(sOfflineOptionName)) == true)
        {
            string cards = "";
            // Don't use deviceList_, as it does not contain offline devices...
            for (size_t cardNum = 0; cardNum < deviceManager_->deviceCount(); ++cardNum)
            {
                MicDeviceFactory factory(deviceManager_.get());
                MicDevice* device = nullptr;
                //TODO: figure out a way to avoid these hax
#ifdef UNIT_TESTS
                device = factory.createDevice(utMockDeviceImplementations_[cardNum]);
#else
                device = factory.createDevice(cardNum);
#endif
                if (!device)
                    continue;

                // Skip a device if it was not selected from the command line
                auto beg = deviceNameList_.begin();
                auto end = deviceNameList_.end();
                auto skip = std::find(beg, end, device->deviceName()) == end;
                if (skip)
                    continue;

                MicDevice::DeviceState state = device->deviceState();
                (void)device->open(); // Attempt to open, ignore return code
                if (state != MicDevice::DeviceState::eOnline || ! device->isOpen())
                {
                    if (cards.empty() == false)
                    {
                        cards += ",";
                    }
                    cards += device->deviceName();
                }
                device->close();
#ifndef UNIT_TESTS
                // Don't delete when in unit tests, because underlying impl class
                // will be deleted as well, leading to a segfault in
                // CliApplication's dtor
                delete device;
#endif
            }
            if (cards.empty() == true)
            {
                cards = "None";
            }
            output_->outputNameValuePair("Offline Cards", cards);
            displayFilter_[sOfflineOptionName] = false;
        }
        bool anyMore = false;
        for (auto it = displayFilter_.begin(); it != displayFilter_.end(); ++it)
        {
            anyMore = (anyMore || it->second);
        }
        if (anyMore == false)
        {
            return rv;
        }
        vector<string> filters;
        for (auto it = displayFilter_.begin(); it != displayFilter_.end(); ++it)
        {
            if (it->first != sOnlineOptionName && it->first != sOfflineOptionName && it->second == true)
            {
                filters.push_back(it->first);
            }
        }
        sort(filters.begin(), filters.end());
        for (auto it = deviceList_.begin(); it != deviceList_.end(); ++it)
        {
            MicDevice* device = it->get();
            output_->startSection(device->deviceName(), sSuppressExtraLine);
            if (device->deviceState() != MicDevice::DeviceState::eOnline)
            {
                buildError(eMicSmcCliDeviceNotOnline, ": " + device->deviceName());
                rv = 1;
                output_->endSection();
                continue;
            }

            // Used for --cores and --freq
            MicCoreUsageInfo coreInfo1;
            coreInfo1.clear();

            string filter;
            for (auto it2 = filters.begin(); it2 != filters.end(); ++it2)
            {
                filter = *it2;
                if (filter == sInfoOptionName)
                {
                    string family = device->deviceType();
                    output_->startSection("Coprocessor Information", sSuppressExtraLine);

                    output_->outputNameValuePair("Device Series", family);

                    MicPciConfigInfo pci;
                    uint32_t pciErr = device->getPciConfigInfo(&pci);
                    if (pciErr == 0)
                    {
                        ss.str("");
                        ss << "0x" << hex << setw(4) << setfill('0') << pci.deviceId();
                        output_->outputNameValuePair("Device ID", ss.str());
                    }
                    else
                    {
                        ss.str("");
                        ss << ": " << MicDeviceError::errorText(pciErr) <<  ": " << device->deviceName();
                        buildError(eMicSmcCliUnknownError, ss.str());
                        rv = 1;
                    }

                    ss.str("");
                    ss << dec << setw(0) << device->deviceDetails().coreInfo().coreCount();
                    output_->outputNameValuePair("Number Of Cores", ss.str());

                    MicValueString coprocessorOsVersion = device->deviceDetails().micPlatformInfo().coprocessorOsVersion();

                    if( coprocessorOsVersion.isValid() == false )
                        rv = 1;

                    output_->outputNameMicValuePair("Coprocessor OS Version", coprocessorOsVersion);

                    MicVersionInfo version;
                    uint32_t versionErr = device->getVersionInfo(&version);
                    if (device->deviceType() == "x100")
                    {
                        output_->outputNameMicValuePair("Flash Version", version.flashVersion());
                        output_->outputNameMicValuePair("Driver Version", version.driverVersion());
                    }
                    else
                    {
                        output_->outputNameMicValuePair("BIOS Version", version.biosVersion());
                    }


                    MicProcessorInfo procInfo;
                    uint32_t procErr = device->getProcessorInfo(&procInfo);
                    if (procErr == 0)
                    {
                        output_->outputNameValuePair("Stepping", procInfo.stepping().value());

                        ss.str("");
                        ss << "0x" << hex << setw(1) << procInfo.substeppingId().value();
                        output_->outputNameValuePair("Substepping", ss.str());
                    }
                    else
                    {
                        ss.str("");
                        ss << ": " << MicDeviceError::errorText(versionErr) << ": " << device->deviceName();
                        buildError(eMicSmcCliUnknownError, ss.str());
                        rv = 1;
                    }

                    output_->endSection();
                }
                else if (filter == sTempOptionName)
                {
                    output_->startSection("Thermal Information", sSuppressExtraLine);
                    MicThermalInfo thermal;
                    uint32_t thermErr = device->getThermalInfo(&thermal);
                    if (thermErr == 0)
                    {
                        for (size_t sensor = 0; sensor < thermal.sensorCount(); ++sensor)
                        {
                            MicTemperature t = thermal.sensorValueAt(sensor);
                            ss.str("");
                            if (t.isAvailable() == true)
                            {
                                ss << dec << setw(0) << t.celsius();
                                output_->outputNameValuePair(t.name(), ss.str(), "C");
                            }
                            else
                            {
                                output_->outputNameValuePair(t.name(), "Sensor Unavailable");
                            }
                        }
                    }
                    else
                    {
                        ss.str("");
                        ss << ": " << MicDeviceError::errorText(thermErr) << ": " << device->deviceName();
                        buildError(eMicSmcCliUnknownError, ss.str());
                        rv = 1;
                    }
                    output_->endSection();
                }
                else if (filter == sFreqOptionName)
                {
                    uint32_t cuErr1 = 0;
                    if (coreInfo1.isValid() == false)
                    {
                        cuErr1 = device->getCoreUsageInfo(&coreInfo1);
                    }
                    output_->startSection("Observed Frequency and Power Usage", sSuppressExtraLine);
                    if (cuErr1 == 0)
                    {
                        if (coreInfo1.isValid() == true)
                        {
                            ss.str("");
                            ss << fixed << setw(5) << setprecision(4) << coreInfo1.frequency().scaledValue(MicFrequency::eGiga);
                            output_->outputNameValuePair("Core Frequency", ss.str(), "GHz");
                        }
                        else
                        {
                            ss.str("");
                            ss << ": Frequency Unavailable: " << device->deviceName();
                            buildError(eMicSmcCliUnknownError, ss.str());
                            rv = 1;
                        }
                    }
                    else
                    {
                        ss.str("");
                        ss << ": " << MicDeviceError::errorText(cuErr1) << ": " << device->deviceName();
                        buildError(eMicSmcCliUnknownError, ss.str());
                        rv = 1;
                    }
                    MicPowerUsageInfo pwrInfo;
                    uint32_t pwrErr = device->getPowerUsageInfo(&pwrInfo);
                    if (pwrErr == 0)
                    {
                        for (size_t sensor = 0; sensor < pwrInfo.sensorCount(); ++sensor)
                        {
                            MicPower p = pwrInfo.sensorValueAt(sensor);
                            ss.str("");
                            if (p.isAvailable() == true)
                            {
                                ss << fixed << setw(4) << setprecision(1) << p.scaledValue(MicFrequency::eBase);
                                output_->outputNameValuePair(p.name(), ss.str(), "Watts");
                            }
                            else
                            {
                                output_->outputNameValuePair(p.name(), "Sensor Unavailable");
                            }
                        }
                    }
                    else
                    {
                        ss.str("");
                        ss << ": " << MicDeviceError::errorText(pwrErr) << ": " << device->deviceName();
                        buildError(eMicSmcCliUnknownError, ss.str());
                        rv = 1;
                    }
                    output_->endSection();
                }
                else if (filter == sMemOptionName)
                {
                    MicMemoryUsageInfo memInfo;
                    uint32_t memErr = device->getMemoryUsageInfo(&memInfo);
                    if (memErr == 0)
                    {
                        double total = memInfo.total().scaledValue(MicMemory::eMega);
                        double free = memInfo.free().scaledValue(MicMemory::eMega);
                        double used = memInfo.used().scaledValue(MicMemory::eMega);
                        output_->startSection("Memory Usage", sSuppressExtraLine);
                        ss.str("");
                        ss << fixed << setprecision(2) << total;
                        output_->outputNameValuePair("Total Memory", ss.str(), "MB");

                        ss.str("");
                        ss << fixed << setprecision(2) << free;
                        output_->outputNameValuePair("Free Memory", ss.str(), "MB");

                        ss.str("");
                        ss << fixed << setprecision(2) << used;
                        output_->outputNameValuePair("Used Memory", ss.str(), "MB");
                        output_->endSection();
                    }
                    else
                    {
                        ss.str("");
                        ss << ": " << MicDeviceError::errorText(memErr) << ": " << device->deviceName();
                        buildError(eMicSmcCliUnknownError, ss.str());
                        rv = 1;
                    }
                }
                else if (filter == sCoresOptionName)
                {
                    MicCoreUsageInfo coreInfo2;
                    uint32_t cuErr1 = device->getCoreUsageInfo(&coreInfo1);
                    MsTimer::sleep(100); // 100ms delay
                    uint32_t cuErr2 = device->getCoreUsageInfo(&coreInfo2);
                    if (cuErr1 == 0 && cuErr2 == 0)
                    {
                        size_t nThreads = device->deviceDetails().coreInfo().coreCount().value() *
                                        device->deviceDetails().coreInfo().coreThreadCount().value();
                        size_t delta = coreInfo2.tickCount() - coreInfo1.tickCount();
                        auto totals = percentOfWorkload(delta,
                                        coreInfo2.counterTotal(MicCoreUsageInfo::eSystem) - coreInfo1.counterTotal(MicCoreUsageInfo::eSystem),
                                        (coreInfo2.counterTotal(MicCoreUsageInfo::eUser) - coreInfo1.counterTotal(MicCoreUsageInfo::eUser)) +
                                        (coreInfo2.counterTotal(MicCoreUsageInfo::eNice) - coreInfo1.counterTotal(MicCoreUsageInfo::eNice)),
                                        coreInfo2.counterTotal(MicCoreUsageInfo::eIdle) - coreInfo1.counterTotal(MicCoreUsageInfo::eIdle));
                        output_->startSection("Core Utilization", sSuppressExtraLine);
                        output_->startSection("Total Utilization", sSuppressExtraLine);
                        ss.str("");
                        ss << fixed << setw(6) << setprecision(2) << get<0>(totals); // User
                        output_->outputNameValuePair("User", ss.str(), "%");
                        ss.str("");
                        ss << fixed << setw(6) << setprecision(2) << get<1>(totals); // System
                        output_->outputNameValuePair("System", ss.str(), "%");
                        ss.str("");
                        ss << fixed << setw(6) << setprecision(2) << get<2>(totals); // Idle
                        output_->outputNameValuePair("Idle", ss.str(), "%");
                        output_->endSection();

                        output_->startSection("Utilization Per Logical Core", sSuppressExtraLine);
                        for (size_t thread = 0; thread < nThreads; ++thread)
                        {
                            size_t coreDelta = coreInfo2.counterValue(MicCoreUsageInfo::eTotal, thread) - coreInfo1.counterValue(MicCoreUsageInfo::eTotal, thread);
                            ss.str("");
                            ss << "Core #" << dec << setw(3) << setfill(' ') << thread;
                            output_->startSection(ss.str(), sSuppressExtraLine);
                            totals = percentOfWorkload(coreDelta,
                                        coreInfo2.counterValue(MicCoreUsageInfo::eSystem, thread) - coreInfo1.counterValue(MicCoreUsageInfo::eSystem, thread),
                                        (coreInfo2.counterValue(MicCoreUsageInfo::eUser, thread) - coreInfo1.counterValue(MicCoreUsageInfo::eUser, thread)) +
                                        (coreInfo2.counterValue(MicCoreUsageInfo::eNice, thread) - coreInfo1.counterValue(MicCoreUsageInfo::eNice, thread)),
                                        coreInfo2.counterValue(MicCoreUsageInfo::eIdle, thread) - coreInfo1.counterValue(MicCoreUsageInfo::eIdle, thread));

                            ss.str("");
                            ss << fixed << setw(6) << setprecision(2) << get<0>(totals); // User
                            output_->outputNameValuePair("User", ss.str(), "%");
                            ss.str("");
                            ss << fixed << setw(6) << setprecision(2) << get<1>(totals); // System
                            output_->outputNameValuePair("System", ss.str(), "%");
                            ss.str("");
                            ss << fixed << setw(6) << setprecision(2) << get<2>(totals); // Idle
                            output_->outputNameValuePair("Idle", ss.str(), "%");
                            output_->endSection();
                        }
                        output_->endSection();
                        output_->endSection();
                    }
                    else
                    {
                        ss.str("");
                        ss << ": " << MicDeviceError::errorText((cuErr1 != 0)?cuErr1:cuErr2) << ": " << device->deviceName();
                        buildError(eMicSmcCliUnknownError, ss.str());
                        rv = 1;
                    }
                }
            }
            output_->endSection(); // Coprocessor
        }
        return rv;
    }

    int CliApplication::displaySettings()
    {
        int rv = 0;
        vector<string> list = parser_->parsedPositionalArgumentAsVector(0);
        if (list.size() == 1 && toLower(list[0]) == commonOptDeviceArgDefVal)
        {
            list.clear();
            for (auto it = sStatusData.begin(); it != sStatusData.end(); ++it)
            {
                if (it->second.canRead() == true)
                {
                    list.push_back(it->first);
                }
            }
        }
        for (size_t i = 0; i < list.size(); ++i)
        {
            list[i] = toLower(list[i]);
        }
        sort(list.begin(), list.end());
        output_->startSection("Display Current Settings", true);
        for (auto it = deviceList_.begin(); it != deviceList_.end(); ++it)
        {
            MicDevice* device = it->get();
            output_->startSection(device->deviceName(), true);
            if (device->deviceState() != MicDevice::DeviceState::eOnline)
            {
                buildError(eMicSmcCliDeviceNotOnline, ": " + device->deviceName());
                rv = 1;
            }
            else
            {
                uint32_t err;
                sort(list.begin(), list.end());
                for (auto it2 = list.begin(); it2 != list.end(); ++it2)
                {
                    stringstream ss;
                    if (*it2 == "led")
                    {
                        uint32_t led;
                        err = device->getLedMode(&led);
                        if (err != 0)
                        {
                            output_->outputNameValuePair(*it2, msgFromSDKError(err));
                            rv = 1;
                        }
                        else
                        {
                            led &= 0x00000001;
                            output_->outputNameValuePair(*it2, (led == 1) ? "enabled" : "disabled");
                        }
                    }
                    else if (*it2 == "ecc")
                    {
                        bool val = false;
                        bool avail = true;
                        err = device->getEccMode(&val, &avail);
                        if (err != 0)
                        {
                            output_->outputNameValuePair(*it2, msgFromSDKError(err));
                            rv = 1;
                        }
                        else
                        {
                            if (avail == true)
                            {
                                output_->outputNameValuePair(*it2, (val == true) ? "enabled" : "disabled");
                            }
                            else
                            {
                                output_->outputNameValuePair(*it2, "ECC not available");
                            }
                        }
                    }
                    else if (*it2 == "turbo")
                    {
                        bool turbo;
                        err = device->getTurboMode(&turbo);
                        if (err != 0)
                        {
                            output_->outputNameValuePair(*it2, msgFromSDKError(err));
                            rv = 1;
                        }
                        else
                        {
                            output_->outputNameValuePair(*it2, (turbo == true) ? "enabled" : "disabled");
                        }
                    }
                    else if (device->deviceType() == "x100" && *it2 == "corec6")
                    {
                        std::list<MicPowerState> states;
                        err = device->getPowerStates(&states);
                        if (err != 0)
                        {
                            output_->outputNameValuePair(*it2, msgFromSDKError(err));
                            rv = 1;
                        }
                        else
                        {
                            for (auto it3 = states.begin(); it3 != states.end(); ++it3)
                            {
                                if (it3->state() == MicPowerState::State::eCoreC6)
                                {
                                    output_->outputNameValuePair(*it2, (it3->isEnabled() == true) ? "enabled" : "disabled");
                                    break;
                                }
                            }
                        }
                    }
                    else if (device->deviceType() == "x100" && *it2 == "cpufreq")
                    {
                        std::list<MicPowerState> states;
                        err = device->getPowerStates(&states);
                        if (err != 0)
                        {
                            output_->outputNameValuePair(*it2, msgFromSDKError(err));
                            rv = 1;
                        }
                        else
                        {
                            for (auto it3 = states.begin(); it3 != states.end(); ++it3)
                            {
                                if (it3->state() == MicPowerState::State::eCpuFreq)
                                {
                                    output_->outputNameValuePair(*it2, (it3->isEnabled() == true) ? "enabled" : "disabled");
                                    break;
                                }
                            }
                        }
                    }
                    else if (device->deviceType() == "x100" && *it2 == "pc3")
                    {
                        std::list<MicPowerState> states;
                        err = device->getPowerStates(&states);
                        if (err != 0)
                        {
                            output_->outputNameValuePair(*it2, msgFromSDKError(err));
                        }
                        else
                        {
                            for (auto it3 = states.begin(); it3 != states.end(); ++it3)
                            {
                                if (it3->state() == MicPowerState::State::ePc3)
                                {
                                    output_->outputNameValuePair(*it2, (it3->isEnabled() == true) ? "enabled" : "disabled");
                                    break;
                                }
                            }
                        }
                    }
                    else if (device->deviceType() == "x100" && *it2 == "pc6")
                    {
                        std::list<MicPowerState> states;
                        err = device->getPowerStates(&states);
                        if (err != 0)
                        {
                            output_->outputNameValuePair(*it2, msgFromSDKError(err));
                        }
                        else
                        {
                            for (auto it3 = states.begin(); it3 != states.end(); ++it3)
                            {
                                if (it3->state() == MicPowerState::State::ePc6)
                                {
                                    output_->outputNameValuePair(*it2, (it3->isEnabled() == true) ? "enabled" : "disabled");
                                    break;
                                }
                            }
                        }
                    }
                    else if (device->deviceType() != "x100")
                    {
                        output_->outputNameValuePair(*it2, msgFromSDKError(MICSDKERR_NOT_SUPPORTED));
                        rv = 1;
                    }
                }
            }
            output_->endSection();
        }
        output_->endSection();
        return rv;
    }

    int CliApplication::enableSettings()
    {
        return changeSettings(true);
    }

    int CliApplication::disableSettings()
    {
        return changeSettings(false);
    }

    int CliApplication::changeSettings(bool enable)
    {
        int rv = 0;
        map<int,map<string, uint32_t>> results;
        vector<string> list = parser_->parsedPositionalArgumentAsVector(0);
        vector<MicDevice*> availableDevices;
        if (list.size() == 1 && toLower(list[0]) == commonOptDeviceArgDefVal)
        {
            list.clear();
            for (auto it = sStatusData.begin(); it != sStatusData.end(); ++it)
            {
                if (it->second.canWrite() == true)
                {
                    list.push_back(it->first);
                }
            }
        }
        for (size_t i = 0; i < list.size(); ++i)
        {
            list[i] = toLower(list[i]);
        }
        sort(list.begin(), list.end());
        for (auto it = deviceList_.begin(); it != deviceList_.end(); ++it)
        {
            MicDevice* device = it->get();
            bool validCardResult = deviceManager_->isValidConfigCard(device->deviceNum());
            if(validCardResult == false)
            {
                continue;
            }
            availableDevices.push_back(device);
            map<string, uint32_t> deviceResults;
            for (size_t i = 0; i < list.size(); ++i)
            {
                deviceResults[list[i]] = UINT32_MAX;
            }
            results[device->deviceNum()] = deviceResults;
        }
        vector<shared_ptr<WorkItemInterface>> items;
        ThreadPool pool(min<unsigned int>(ThreadPool::hardwareThreads(), static_cast<unsigned int>(availableDevices.size())), handler_);
        for (auto it = availableDevices.begin(); it != availableDevices.end(); ++it)
        {
            MicDevice* device = (*it);
            shared_ptr<WorkItemInterface> item = shared_ptr<WorkItemInterface>(new ChangeSettingsWorkItem(output_.get(),
                                                                               timeoutTimer_.get(), device, enable, list,
                                                                               results[device->deviceNum()]));
            items.push_back(item);
            pool.addWorkItem(item);
        }
        int timeout = numeric_limits<int>::max();
        if ((bool)timeoutTimer_ == true && timeoutTimer_->timerValue() > 0)
        {
            timeout = static_cast<int>(timeoutTimer_->elapsed().count() / 1000L);
        }
        if(pool.wait(timeout) == false)
        { // Timeout Error
            rv = 1;
            pool.stopPool();
            for (auto it = results.begin(); it != results.end(); ++it)
            {
                map<string, uint32_t>& dResult = it->second;
                for (auto it2 = dResult.begin(); it2 != dResult.end(); ++it2)
                {
                    if (it2->second == numeric_limits<uint32_t>::max())
                    {
                        it2->second = MICSDKERR_TIMEOUT;
                    }
                }
            }
        }

        // Display Summary
        output_->startSection((enable == true)?"Enable Setting Results":"Disable Setting Results", true);
        for (auto it = deviceList_.begin(); it != deviceList_.end(); ++it)
        {
            MicDevice* device = it->get();
            bool validResult = deviceManager_->isValidConfigCard(device->deviceNum());
            if(validResult == false)
            {
                stringstream sstr;
                sstr << device->deviceName() << ": Device not available";
                output_->outputError(sstr.str(), validResult);
                LOG(ERROR_MSG, sstr.str());
                continue;
            }
            map<string, uint32_t>& dResult = results[device->deviceNum()];
            output_->startSection(device->deviceName(), true);
            for (auto it = dResult.begin(); it != dResult.end(); ++it)
            {
                string val;
                if (it->second == 0)
                {
                    val = "Successful";
                }
                else
                {
                    val = msgFromSDKError(it->second);
                }
                output_->outputNameValuePair(it->first, val);
            }
            output_->endSection();
        }
        output_->endSection();

        return rv;
    }
}; // namespace micsmc-cli
