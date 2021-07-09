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

#ifndef MICFW_APPLICATION_HPP
#define MICFW_APPLICATION_HPP

#include "EvaluateArgumentCallbackInterface.hpp"
#include "MicException.hpp"
#include "Common.hpp"

#include <fstream>
#include <sstream>

#ifdef UNIT_TESTS
#define PRIVATE public
#else
#define PRIVATE private
#endif

namespace micmgmt
{ // Forward references...
    class CliParser;
    class MicOutputFormatterBase;
    class MicOutput;
    class MicDeviceManager;
    class MicDeviceFactory;
    class MicDevice;
}

namespace micfw
{
#ifdef UNIT_TESTS
    typedef micmgmt::MicDevice* (*mockCreateDeviceCallback)(micmgmt::MicDeviceFactory& factory, const std::string& cardName, int type);
    typedef void(*modifyDeviceCallback)(micmgmt::MicDevice* device);
#else
    typedef void *mockCreateDeviceCallback;
    typedef void *modifyDeviceCallback;
#endif

    class Application : micmgmt::EvaluateArgumentCallbackInterface
    {
    public:
        Application(micmgmt::CliParser* parser);
        virtual ~Application();

        int run(mockCreateDeviceCallback callback = NULL, modifyDeviceCallback callback2 = NULL);
        void setTrapSignalHandle(micmgmt::trapSignalHandler handler);

    private: // Hide special members
        Application();
        Application(const Application&);
        Application& operator=(const Application&);

    private: // Implementation
        uint32_t initialize();
        micmgmt::MicException buildException(uint32_t code, const std::string& specialMessage = "");
        void fillCommonParserData();
        void fillKnlParserData();
        void fillNoDeviceParseData();

        virtual bool checkArgumentValue(micmgmt::EvaluateArgumentType type,
                                        const std::string& typeName,
                                        const std::string& subcommand,
                                        const std::string& value);
        bool checkDevices(const std::string& rawList);
        bool checkDeviceName(const std::string& devName);

        std::vector<size_t> retrieveDeviceList();

        std::string getFlashPath();
        std::string getFullPathToFlashFile(const std::string& file);
        std::vector<std::string> findAppropriateFlashImages(const std::string& = "");
        bool verifyFileExistsInFlashPath(std::string& filename);

    PRIVATE: // Fields
        std::size_t                                      deviceCount_;
        std::shared_ptr<micmgmt::MicDeviceManager>       deviceManager_;
        int                                              appType_;
        std::unique_ptr<micmgmt::CliParser>              parser_;
        std::shared_ptr<micmgmt::MicOutput>              output_;
        std::unique_ptr<std::ofstream>                   xmlFile_;
        bool                                             interleave_;
        bool                                             verbose_;
        bool                                             forceUpdate_;
        int                                              toolTimeoutSeconds_;
        std::string                                      commandString_;
        std::unique_ptr<micmgmt::MicOutputFormatterBase> stdOut_;
        std::unique_ptr<micmgmt::MicOutputFormatterBase> stdErr_;
        std::unique_ptr<micmgmt::MicOutputFormatterBase> fileOut_;
        micmgmt::trapSignalHandler                       handler_;
        uint32_t                                         deviceManagerErr_;
        std::ostream *                                   logOut_;
#ifdef UNIT_TESTS
    public:
        void redirectOutput(std::ostream& redirectStream);
        void scenarioReset();
        void scenarioFailToFindFile();
        void setInstallFolder(const std::string& folder);
        bool testImageCreator(const std::string& imagePath,
                              const std::string& destA,
                              const std::string& destB,
                              const std::string& destC,
                              const std::string& destD);

    private:
        bool fileVerification_;
        bool findFile_;
        std::string utFlashFolder_;
#ifndef _WIN32
#endif
#endif
    };
} // namespace micfw
#endif // MICFW_APPLICATION_HPP
