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

#include "Application.hpp"
#include "FlashFileVersion.hpp"
#include "KnxFlashing.hpp"
#include "MicFwError.hpp"

#include "CliParser.hpp"
#include "ToolSettings.hpp"
#include "MicException.hpp"
#include "XmlOutputFormatter.hpp"
#include "ConsoleOutputFormatter.hpp"
#include "MicOutput.hpp"
#include "MicLogger.hpp"
#include "micmgmtCommon.hpp"
#include "commonStrings.hpp"
#include "MicDeviceManager.hpp"
#include "Utils.hpp"
#ifdef UNIT_TESTS
#include "MicDevice.hpp"
#endif

#include <memory>
#include <vector>
#include <tuple>
#include <map>

#ifndef _WIN32
#include <unistd.h>
namespace
{
    const char* const  MPSS_DEFAULT_FLASH_DIR        = "flash";
    const char* const  FIRMWARE_PATH                 = "/lib/firmware/";
}
#endif

namespace
{ // Global implementation data that is not part of the object state.
    using namespace std;

    typedef micmgmt::MicDeviceManager::DeviceType DT;

#ifndef _WIN32
    // Default Linux config path.
    const string sLinuxConfigPath = "/usr/mpss/";
#endif
    const string sSectionName     = "Flash";
    const string sVariableName    = "DefaultImageFolder";

    // File extensions for files used in this application...
    string sXmlFileExtension  = ".xml";
    string sKnlFirmwareMask   = "*.hddimg";

    string sDevicePrefix           = micmgmt::deviceName(0).erase(3);
    // Mask for filtering error codes for Linux, Windows uses the entire code.
#ifdef _WIN32
    uint32_t sErrorMask = 0xffffffff;
#else
    uint32_t sErrorMask = 0x000000ff;
#endif

    // Subcommand, option, and argument names...
    std::string sDeviceVersionCommand = "device-version";
    std::string sFileVersionCommand = "file-version";
    std::string sImageFileArgument = "image-filename";
    std::string sInterleaveOption = "interleave";
    std::string sFileOption = "file";
    std::string sUpdateCommand = "update";

    // Help strings...
    vector<string>imageFileHelp = {
        "This is the filename containing the flash image to flash to selected devices."
    };

    vector<string> sFileVersionHelp = {
        "This command will retrieve the flash version for the file specified as the next argument on the command line.",
        "This subcommand is incompatible with --device (-d) option since devices are not required to complete retrieving the flash file version."
    };

    vector<string> sDeviceVersionHelp = {
        "This command will retrieve the flash version for the device(s) specified as the next argument on the command line."
    };

    vector<string> sUpdateFwHelp = {
        "This command will attempt to flash the firmware of the Xeon Phi coprocessor.",
        "Use '--file' to specify the exact image to attempt to flash or a directory in which the first compatible image \
        will be used. If omitted then the first compatible image in default flash directory (OS dependent) will be used.",
        "This subcommand is incompatible with --device (-d) option since a device list is required as a positional argument on the command line."
    };

    vector<string> sUpdateBootloaderHelp = {
        "This option will attempt to flash the bootloader of the Intel(R) Xeon Phi(TM) coprocessor first generation before the normal BIOS and SMC are flashed.",
        "The best available bootloader image will be selected for the firmaware flash process if omitted."
    };

    vector<string>sFileHelp = {
        "This option is used to specify the image file with its required argument that will be used to flash the selected devices."
    };

    vector<string> sInterleaveHelp = {
        "This option will show one flashing step per line. If --verbose is used this option is implied."
    };

    vector<string> sForceUgradeHelp = {
        "Intel(R) Xeon Phi(TM) first generation only. This option is used to force an update even if the version being flashed is older than the installed flash."
    };
}; // empty namespace

namespace micfw
{
    using namespace std;
    using namespace micmgmt;

    Application::Application(CliParser* parser)
        : deviceCount_(0), appType_(DT::eDeviceTypeUndefined),
        interleave_(false), verbose_(false), forceUpdate_(false), commandString_(""), deviceManagerErr_(0)
    {
        stdOut_ = unique_ptr<MicOutputFormatterBase>(new ConsoleOutputFormatter(&cout, 24, 3));
        stdErr_ = unique_ptr<MicOutputFormatterBase>(new ConsoleOutputFormatter(&cerr, 24, 3));
        output_ = make_shared<MicOutput>(parser->toolName(), stdOut_.get(), stdErr_.get(), fileOut_.get());
        logOut_ = &cout;

        parser_ = unique_ptr<CliParser>(parser);

#ifdef UNIT_TESTS
        scenarioReset();
#endif
    }

    Application::~Application()
    {
        parser_.reset();
        output_.reset();
    }

    void Application::setTrapSignalHandle(trapSignalHandler handler)
    {
        handler_ = handler;
    }

    int Application::run(mockCreateDeviceCallback callback, modifyDeviceCallback callback2)
    {
        (void)callback; // Avoid compiler errors
        (void)callback2; // Avoid compiler errors

        vector<string> firmwareImages;
        vector<string> validFirmwareImages;

        initialize();

        if (appType_ == DT::eDeviceTypeUndefined)
        {
            output_->outputLine("*** Warning: " + parser_->toolName() + ": There were no driver or coprocessors detected! The tool functionality is greatly reduced.");
        }

        try
        {
            if (parser_->parse() == false)
            {
                return parser_->parseError() ? 1 : 0;
            }

           // Common option and command for all instances of this tool.
            commandString_ = parser_->parsedSubcommand();
            auto tup = parser_->parsedOption(commonOptXmlName);
            if (get<0>(tup) == true)
            {
                string xmlFilename;
                if (get<1>(tup) == false)
                {
                    xmlFilename = parser_->toolName() + sXmlFileExtension;
                }
                else
                {
                    xmlFilename = get<2>(tup);
                    if (micmgmt::pathIsFolder(xmlFilename) == true)
                    {
                        if (xmlFilename[xmlFilename.size() - 1] != filePathSeparator()[0])
                        {
                            xmlFilename += filePathSeparator();
                        }
                        xmlFilename += parser_->toolName() + sXmlFileExtension;
                    }
                }
                ifstream ifs(xmlFilename);
                if (ifs.good() == true)
                {
                    ifs.close();
                    throw buildException(MicFwErrorCode::eMicFwFailedToOpenXmlFile, ": file already exists");
                }
                xmlFile_ = unique_ptr<ofstream>(new ofstream(xmlFilename));
                if (xmlFile_->bad())
                {
                    throw buildException(MicFwErrorCode::eMicFwFailedToOpenXmlFile, get<2>(tup));
                }
                fileOut_ = unique_ptr<MicOutputFormatterBase>(new XmlOutputFormatter(xmlFile_.get(), parser_->toolName()));
                parser_->setOutputFormatters(stdOut_.get(), stdErr_.get(), fileOut_.get());
                output_ = make_shared<MicOutput>(parser_->toolName(), stdOut_.get(), stdErr_.get(), fileOut_.get());
            }
            tup = parser_->parsedOption(commonOptTimeoutName);
            if (get<0>(tup) == true)
            {
                toolTimeoutSeconds_ = (int)strtol(get<2>(tup).c_str(), NULL, 10);
            }
            tup = parser_->parsedOption(commonOptSilentName);
            if (get<0>(tup) == true)
            {
                output_->setSilent(true);
            }

            tup = parser_->parsedOption(sInterleaveOption);
            if (get<0>(tup) == true)
            {
                interleave_ = true;
            }

            tup = parser_->parsedOption(commonOptVerboseName);
            if (get<0>(tup) == true)
            {
                INIT_LOGGER(logOut_);
                interleave_ = true;
                verbose_ = true;
                if (output_->isSilent() == true)
                {
                    throw buildException(MicFwErrorCode::eMicFwVerboseSilent);
                }
            }
            // Check illegal combinations...
            if (commandString_ == sDeviceVersionCommand && get<0>(parser_->parsedOption(sFileOption)) == true)
            {
                MicFwError err(parser_->toolName());
                throw MicException(err.buildError(eMicFwCommandOptionPairIllegal, ": '" + commandString_ + "' and '--" + sFileOption + "'"));
            }
            if ((commandString_ == sFileVersionCommand || commandString_ == sUpdateCommand) && get<0>(parser_->parsedOption(commonOptDeviceName)) == true)
            {
                MicFwError err(parser_->toolName());
                throw MicException(err.buildError(eMicFwCommandOptionPairIllegal, ": '" + commandString_ + "' and '--" + commonOptDeviceName + "/-d'"));
            }
            if (commandString_ == sFileVersionCommand && get<0>(parser_->parsedOption(sFileOption)) == true)
            {
                MicFwError err(parser_->toolName());
                throw MicException(err.buildError(eMicFwCommandOptionPairIllegal, ": '" + commandString_ + "' and '--" + sFileOption + "'"));
            }

            // Check for subcommands...
            LOG(INFO_MSG, "Command and option combinations check out.");
            string firmwareImage = "";
            MicFwCommands commandToken = eUnknownCommand;
            if (commandString_ == sFileVersionCommand)
            {
                LOG(INFO_MSG, "Parsed command '" + sFileVersionCommand + "'.");
                firmwareImage = parser_->parsedPositionalArgumentAt(0);
                if (verifyFileExistsInFlashPath(firmwareImage) == false || pathIsFolder(firmwareImage))
                {
                    MicFwError err(parser_->toolName());
                    throw MicException(err.buildError(eMicFwFileImageBad, ": Firmware Image = '" + firmwareImage + "'"));
                }
                string fullPath = getFullPathToFlashFile(firmwareImage);
                FlashFileVersion runObject(fullPath, output_, parser_->toolName());
                commandToken = MicFwCommands::eGetFileVersion;
                return runObject.run();
            }
            if (commandString_ == sDeviceVersionCommand)
            {
                LOG(INFO_MSG, "Parsed command '" + sDeviceVersionCommand + "'.");
                commandToken = MicFwCommands::eGetDeviceVersion;
            }
            else if (commandString_ == sUpdateCommand)
            {
                LOG(INFO_MSG, "Parsed command '" + sUpdateCommand + "'.");
                commandToken = MicFwCommands::eUpdate;
                if (isAdministrator() == false)
                {
#ifndef UNIT_TESTS
                    MicFwError err(parser_->toolName());
                    throw MicException(err.buildError(MicFwErrorCode::eMicFwInsufficientPrivileges, ": " + commandString_));
#endif
                }
            }
            LOG(INFO_MSG, "Getting device list...");
            vector<size_t> deviceList;
            try
            {
                deviceList = retrieveDeviceList();
            }
            catch (MicException&)
            {
                if (commandToken == MicFwCommands::eGetDeviceVersion)
                {
                    throw buildException(MicFwErrorCode::eMicFwBadDeviceList, ": '" + get<2>(parser_->parsedOption(commonOptDeviceName)) + "'");
                }
                else
                {
                    throw buildException(MicFwErrorCode::eMicFwBadDeviceList, ": '" + parser_->parsedPositionalArgumentAt(0) + "'");
                }
            }
            LOG(INFO_MSG, "Got device list.");
            string bootloaderImage = "";
            if (get<0>(parser_->parsedOption(sFileOption)) == true)
            {
                firmwareImage = get<2>(parser_->parsedOption(sFileOption));
#ifdef _WIN32
                if (firmwareImage.empty() != true && firmwareImage[firmwareImage.size() - 1] == '"')
                { // Fix for cmd.exe poorly parsing pathnames that end in '\\'.
                    firmwareImage.erase(firmwareImage.size() - 1);
                }
#endif
                if (firmwareImage.substr(firmwareImage.size() - 1) != filePathSeparator() && pathIsFolder(firmwareImage) == true)
                {
                    firmwareImage += filePathSeparator();
                }
                if (firmwareImage[firmwareImage.size() - 1] == filePathSeparator()[0])
                {
                    firmwareImages = findAppropriateFlashImages(firmwareImage);
                    if (firmwareImages.size() == 0)
                    {
                        MicFwError err(parser_->toolName());
                        throw MicException(err.buildError(eMicFwFlashImageNotFound, ": Firmware Image"));
                    }
                }
                else if (verifyFileExistsInFlashPath(firmwareImage) == false)
                {
                    LOG(INFO_MSG, "Specified firmware image was NOT found: '" + firmwareImage + "'");
                    MicFwError err(parser_->toolName());
                    throw MicException(err.buildError(eMicFwFlashImageNotFound, ": Firmware Image = '" + firmwareImage + "'"));
                }
            }
            else if (commandToken == MicFwCommands::eUpdate)
            {
                LOG(INFO_MSG, "Finding firmware flash image in default location.");
                firmwareImages = findAppropriateFlashImages();
                if (firmwareImages.size() == 0)
                {
                    MicFwError err(parser_->toolName());
                    throw MicException(err.buildError(eMicFwFlashImageNotFound, ": Firmware Image"));
                }
            }
            if (firmwareImages.size() == 0)
            {
                firmwareImages.push_back(firmwareImage);
            }
            PathNamesGenerator names;
            if(names.generatePaths())
            {
                throw buildException(MicFwErrorCode::eMicFwFailedtoStartFlashing, ": Cannot create temporary paths");
            }

            micmgmt::FabImagesPaths fip(names.getPathA(), names.getPathB(),
                                        names.getPathX(), names.getPathT());
            KnxFlashing runObject(deviceManager_, output_, parser_->toolName(),
                commandToken, deviceList, fip, callback);
            runObject.setTrapHandler(handler_);
            if (commandToken != MicFwCommands::eGetDeviceVersion)
            {
                string validImagePath;
                for (auto image : firmwareImages)
                {
                    FlashFileVersion ffv(image, output_, parser_->toolName());
                    if (ffv.runCheck())
                    {
                            continue;
                    }
                    if (ffv.createImageVersions(fip))
                    {
                        throw buildException(MicFwErrorCode::eMicFwFailedtoStartFlashing,
                                                ": Cannot create temporary files");
                    }
                    /* We found a valid image */
                    validImagePath = image;
                    break;
                }
                runObject.setImageFiles(validImagePath);
                runObject.setOptions(output_->isSilent(), interleave_, verbose_);
            }
            return runObject.run(callback2);
        }
        catch (MicException& e)
        {
            LOG(FATAL_MSG, e.what());
            output_->outputError(e.what(), e.getMicErrNo());
            return (e.getMicErrNo() & sErrorMask);
        }
    }

    // Private Implementation Section
    micmgmt::MicException Application::buildException(uint32_t code, const std::string& specialMessage)
    {
        MicFwError err(parser_->toolName());
        return MicException(err.buildError(code, specialMessage));
    }

    uint32_t Application::initialize()
    {
        deviceManager_ = shared_ptr<MicDeviceManager>(new MicDeviceManager());
        if ((deviceManagerErr_ = deviceManager_->initialize()) == 0)
        {
            deviceCount_ = deviceManager_->deviceCount(MicDeviceManager::eDeviceTypeAny);
            if (deviceCount_ == 0)
            {
                fillNoDeviceParseData();
            }
            else
            {
                fillCommonParserData();
                fillKnlParserData();
                appType_ = DT::eDeviceTypeKnl;
            }
        }
        else
        {
            fillNoDeviceParseData();
        }
        return deviceManagerErr_;
    }

    void Application::fillCommonParserData()
    {
        // Feature "update"
        parser_->addSubcommand(sUpdateCommand, sUpdateFwHelp);
        parser_->addSubcommandPositionalArg(sUpdateCommand, commonOptDeviceArgName, commonOptDeviceArgHelp, this);
        parser_->addOption(sFileOption, sFileHelp, 0, true, sImageFileArgument, imageFileHelp, "");

        // Feature "get flash version(s) from file"
        parser_->addSubcommand(sFileVersionCommand, sFileVersionHelp);
        parser_->addSubcommandPositionalArg(sFileVersionCommand, sImageFileArgument, imageFileHelp);

        //Register general options
        parser_->addOption(commonOptDeviceName, commonOptDeviceHelp, 'd', true, commonOptDeviceArgName, commonOptDeviceArgHelp, commonOptDeviceArgDefVal, this);
        // Interleave
        parser_->addOption(sInterleaveOption, sInterleaveHelp, 0, false, "", commonEmptyHelp, "");
        // Verbose
        parser_->addOption(commonOptVerboseName, commonOptVerboseHelp, 'v', false, "", commonEmptyHelp, "");
        // Silent
        parser_->addOption(commonOptSilentName, commonOptSilentHelp, 's', false, "", commonEmptyHelp, "");
    }

    void Application::fillKnlParserData()
    {
        // Feature "get flash version(s) from devices"
        parser_->addSubcommand(sDeviceVersionCommand, sDeviceVersionHelp);
    }

    void Application::fillNoDeviceParseData()
    {
        // Feature "get flash version(s) from file"
        parser_->addSubcommand(sFileVersionCommand, sFileVersionHelp);
        parser_->addSubcommandPositionalArg(sFileVersionCommand, sImageFileArgument, imageFileHelp);

        // Register general options
        parser_->addOption(sInterleaveOption, sInterleaveHelp, 0, false, "", commonEmptyHelp, "");
    }

    bool Application::checkArgumentValue(EvaluateArgumentType type,
        const std::string& typeName,
        const std::string& subcommand,
        const std::string& value)
    {
        if (type == EvaluateArgumentType::eOptionArgument)
        { // Option Argument
            if (subcommand == commonOptDeviceName && typeName == commonOptDeviceArgName)
            {
                return checkDevices(value);
            }
        }
        else
        { // Subcommand argument
            string command = subcommand;
            if ((command == sDeviceVersionCommand || command == sUpdateCommand) && typeName == commonOptDeviceArgName)
            {
                return checkDevices(value);
            }
            // Nothing evaluated yet.
        }
        return true;
    }

    bool Application::checkDeviceName(const string& devName)
    {
        string nameBase = deviceManager_->deviceBaseName();
        if (devName.size() <= nameBase.size())
        { // not large enough to be valid...
            return false;
        }
        if (devName.compare(0, nameBase.size(), nameBase) != 0)
        { // wrong prefix...
            return false;
        }
        string number = devName.substr(nameBase.size());
        size_t pos = number.find_first_not_of("0123456789");
        if (pos != string::npos || number.size() > 3)
        { // a non digit was found or the number is too large (> 3 digits)...
            return false;
        }
        size_t devNum = (size_t)std::strtol(number.c_str(), NULL, 10);
        if (devNum >= deviceManager_->deviceCount(appType_))
        {
            return false;
        }

        return true;
    }

    bool Application::checkDevices(const string& rawList)
    {
        string devices = trim(rawList);
        if (devices.empty() == true)
        {
            return false;
        }
        vector<string> csList;
        size_t pos = devices.find_first_of(",-");
        while (pos != string::npos)
        {
            string dev = trim(devices.substr(0, pos));
            if (dev.empty() == true)
            {
                return false;
            }
            devices = devices.substr(pos + 1);
            if (checkDeviceName(dev) == false)
            {
                return false;
            }
            pos = devices.find_first_of(",-");
        }
        return true;
    }

    vector<size_t> Application::retrieveDeviceList()
    {
        vector<string> list;
        string rawArg;
        if (commandString_ == sUpdateCommand)
        {
            rawArg = parser_->parsedPositionalArgumentAt(0);
            if (rawArg != commonOptDeviceArgDefVal)
            {
                list = parser_->parsedPositionalArgumentAsVector(0, sDevicePrefix);
            }
        }
        else if (commandString_ == sDeviceVersionCommand)
        {
            rawArg = get<2>(parser_->parsedOption(commonOptDeviceName));
            if (rawArg != commonOptDeviceArgDefVal)
            {
                list = parser_->parsedOptionArgumentAsVector(commonOptDeviceName, sDevicePrefix);
            }
        }
        else
        {
            return vector<size_t>();
        }
        vector<size_t> result;
        if (rawArg == commonOptDeviceArgDefVal)
        {
            for (uint32_t i = 0; i < deviceCount_; ++i)
            {
                result.push_back(i);
            }
        }
        else
        {
            for (auto it = list.begin(); it != list.end(); ++it)
            {
                string prefix = deviceName(0);
                prefix.erase(prefix.size() - 1);
                string number = it->substr(prefix.size());
                uint32_t n = (uint32_t)strtoul(number.c_str(), NULL, 10);
                result.push_back(n);
            }
        }
        return result;
    }

    std::string Application::getFlashPath()
    {
        const char sep = micmgmt::filePathSeparator()[0];
        string installFolder = micmgmt::mpssInstallFolder();
        if (installFolder.empty() != true && installFolder[installFolder.size() - 1] != sep)
        {
            installFolder += sep;
        }
        string folder;
#ifdef _WIN32
        folder = installFolder; // Default if no setting variable or its empty
#else
        folder = installFolder + "flash" + sep; // Default if no setting variable or its empty
#endif
#ifdef UNIT_TESTS
        if (utFlashFolder_.empty() != true)
        {
            folder = utFlashFolder_;
        }
#endif // UNIT_TESTS
        if (folder.empty() != true && folder[folder.size() - 1] != sep)
        {
            folder += sep;
        }
        return folder;
    }

    std::vector<std::string> Application::findAppropriateFlashImages(const std::string& path)
    {
        vector<string> list;
        string localPath = getFlashPath();
        if (path.empty() == false)
        {
            localPath = path;
        }
#ifndef _WIN32
        if(localPath.empty() == true || localPath[0] != micmgmt::filePathSeparator()[0])
        {
            LOG(WARNING_MSG, "Linux flash search paths MUST be absolute not relative to current working directory");
        }
#endif
        list = findMatchingFilesInFolder(localPath, sKnlFirmwareMask);
#ifdef UNIT_TESTS
        if(!findFile_)
            list.clear();
#endif
        return list;
    }

    std::string Application::getFullPathToFlashFile(const std::string& file)
    {
        string localFilename = file;

        // Try path given...
        ifstream stream1(localFilename.c_str());
        bool found = stream1.good();
        if (found == true)
        {
            stream1.close();
            return localFilename;
        }
        else
        { // Try path in flash folder
            localFilename = getFlashPath() + fileNameFromPath(file);
            ifstream stream2(localFilename);
            found = stream2.good();
            if (found == true)
            {
                stream2.close();
                return localFilename;
            }
            else
            {
                return string();
            }
        }
    }

    bool Application::verifyFileExistsInFlashPath(std::string& filename)
    {
        string localFilename = filename;

        // Try path given...
        ifstream stream1(localFilename.c_str());
        bool rv = (stream1.rdstate() & ifstream::failbit) == 0;
        if (rv == true)
        {
            stream1.close();
        }
        else
        { // Not found
            rv = false;
        }
        return rv;
    }

#ifdef UNIT_TESTS
    void Application::redirectOutput(std::ostream& redirectStream)
    {
        stdOut_ = unique_ptr<MicOutputFormatterBase>(new ConsoleOutputFormatter(&redirectStream, 24, 3));
        stdErr_ = unique_ptr<MicOutputFormatterBase>(new ConsoleOutputFormatter(&redirectStream, 24, 3));
        output_ = make_shared<MicOutput>(parser_->toolName(), stdOut_.get(), stdErr_.get(), fileOut_.get());
        logOut_ = &redirectStream;
        parser_->setOutputFormatters(stdOut_.get(), stdErr_.get(), fileOut_.get());
    }

    void Application::scenarioReset()
    {
        fileVerification_ = true;
        findFile_ = true;
        RESET_LOGGER();
    }

    void Application::scenarioFailToFindFile()
    {
        findFile_ = false;
    }

    void Application::setInstallFolder(const std::string& folder)
    {
        utFlashFolder_ = folder;
    }

    bool Application::testImageCreator(const string& imagePath,
                                       const string& destA,
                                       const string& destB,
                                       const string& destC,
                                       const string& destD)
    {
        FlashFileVersion ffv(imagePath, output_, parser_->toolName());
        ffv.runCheck();
        micmgmt::FabImagesPaths fip(destA, destB, destC, destD);
        return ffv.createImageVersions(fip);
    }
#endif
} // namespace micfw
