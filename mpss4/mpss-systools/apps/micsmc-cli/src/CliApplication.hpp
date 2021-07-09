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

#ifndef MICSMCCLI_CLIAPPLICATION_HPP
#define MICSMCCLI_CLIAPPLICATION_HPP

// Common Framework Includes
#include "EvaluateArgumentCallbackInterface.hpp"
#include "micmgmtCommon.hpp"

// SDK Includes

// C++ Includes
#include <memory>
#include <unordered_map>
#include <vector>
#include <fstream>
#include <tuple>

// Forward References
namespace micmgmt
{
    class CliParser;
    class MicOutputFormatterBase;
    class MicOutput;
    class MicDevice;
    class MicException;
    class MsTimer;
    class MicDeviceManager;
#ifdef UNIT_TESTS
    class MicDeviceImpl;
#endif
}; // namespace micmgmt

#ifdef UNIT_TESTS
#define PRIVATE public
#else
#define PRIVATE private
#endif

namespace micsmccli
{
#ifdef UNIT_TESTS
    typedef std::vector<micmgmt::MicDeviceImpl*> DeviceImpls;
#else
    typedef uint32_t DeviceImpls;
#endif

    class CliApplication : micmgmt::EvaluateArgumentCallbackInterface
    {
    public:
        CliApplication(int argc, char* argv[], const std::string& toolVersion, const std::string& toolName, const std::string& toolDescription);
        virtual ~CliApplication();

        void setTrapSignalHandler(micmgmt::trapSignalHandler handler);

        int run(unsigned int toolTimeoutSeconds, const DeviceImpls& deviceImpls);

    PRIVATE: // Fields
        int                                              platform_;
        unsigned int                                     timeoutSeconds_;
        std::shared_ptr<micmgmt::CliParser>              parser_;
        std::shared_ptr<micmgmt::MicOutputFormatterBase> stdOut_;
        std::shared_ptr<micmgmt::MicOutputFormatterBase> stdErr_;
        std::shared_ptr<micmgmt::MicOutputFormatterBase> fileOut_;
        std::shared_ptr<micmgmt::MicOutput>              output_;
        std::shared_ptr<std::ofstream>                   xmlFile_;
        micmgmt::trapSignalHandler                       handler_;
        std::unordered_map<std::string, bool>            displayFilter_;
        std::shared_ptr<micmgmt::MsTimer>                timeoutTimer_;
        std::shared_ptr<micmgmt::MicDeviceManager>       deviceManager_;
        uint32_t                                         deviceManagerInitError_;
        std::vector<std::string>                         deviceNameList_;
        std::vector<std::shared_ptr<micmgmt::MicDevice>> deviceList_;
        std::vector<std::string>                         statusReadStrings_;
        std::vector<std::string>                         statusWriteStrings_;

#ifdef UNIT_TESTS
        std::vector<micmgmt::MicDeviceImpl*>             utMockDeviceImplementations_;
        bool                                             utFakeAdministratorMode_;
        bool                                             utFailMPSS_;
        bool                                             utFailDriver_;
#endif

    private: // Implementation
        std::vector<std::string> makeListFromCommaSeparatedList(const std::string& sList) const;
        bool validateDeviceList(const std::string& list) const;
        virtual bool checkArgumentValue(micmgmt::EvaluateArgumentType type,
                                        const std::string& typeName,
                                        const std::string& subcommand,
                                        const std::string& value);

        int buildError(uint32_t code, const std::string& specialMessage = "");
        micmgmt::MicException buildException(uint32_t code, const std::string& specialMessage = "") const;

        void buildDeviceListFromDeviceOption();
        void buildDeviceListFromPositionalArgument(size_t position);
        void buildDeviceListAll();
        void createAndOpenDevices(int* error=nullptr);
        std::string createReadListString(const std::string& conjunction = "or") const;
        std::string createWriteListString(const std::string& conjunction = "or") const;
        std::string stringVariableSubstitution(const std::string& str) const;
        std::vector<std::string> stringVariableSubstitution(const std::vector<std::string>& strVector) const;
        int checkSubcommandAndptionUsage();
        std::string msgFromSDKError(uint32_t error);
        std::tuple<double,double,double> percentOfWorkload(uint64_t potentialWork, uint64_t systemWork, uint64_t userWork, uint64_t idleWork);
        int deviceNumFromName(const std::string& name) const;

        // Action Functions
        int displayData();
        int displaySettings();
        int enableSettings();
        int disableSettings();
        int changeSettings(bool enable);
    };
}; // namespace micsmccli

#endif // MICSMCCLI_CLIAPPLICATION_HPP
