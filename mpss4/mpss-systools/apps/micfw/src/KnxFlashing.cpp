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

// PROJECT INCLUDES
//
#include "KnxFlashing.hpp"
#include "MicFwError.hpp"
#include "FlashWorkItem.hpp"
#include "CountdownMutex.hpp"

#include "micmgmtCommon.hpp"
#include "MicOutput.hpp"
#include "MicLogger.hpp"
#include "ThreadPool.hpp"
#include "MsTimer.hpp"
#include "MicException.hpp"
#include "MicDeviceManager.hpp"
#include "MicDeviceFactory.hpp"
#include "MicDevice.hpp"
#include "FlashImage.hpp"
#include "FlashStatus.hpp"
#include "MicVersionInfo.hpp"
#include "micsdkerrno.h"
#include "KnlDeviceBase.hpp"

// SYSTEM INCLUDES
//
#include <map>
#include <mutex>
#include <iostream>
#include <algorithm>
#include <csignal>
#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#endif

using namespace std;
using namespace micmgmt;

// LOCAL CONSTANTS
//
namespace
{
    // 180 seconds delay used as timeout for card reset
    const size_t resetTimeout = 180000;
    // 1 second delay used as polling time to read reset postcode
    const size_t sleepTime    = 1000;

    std::mutex callbackCriticalSection; // For flashing callback...

    const int maxColumnSize = 79;
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO currentCoordinate = { 0 };
    static HANDLE h = NULL;
#endif
    std::string PrintInLine(int currentLine)
    {
        static unsigned newLine = 1;
        int m = currentLine - newLine;
        newLine = currentLine;
#ifdef _WIN32
        std::string outValue;
        COORD new_coordinate = { 0, static_cast<SHORT>(m) };
        if (m < 0)
        {
            //Move up by adding the current relative print possition for Windows CMD
            new_coordinate.Y = (new_coordinate.Y * -1) + currentCoordinate.dwCursorPosition.Y - 1;
            SetConsoleCursorPosition(h, new_coordinate);
            return "\r";
        }
        else
        {
            //Move down a given amount of rows for printing
            outValue = "\r" + std::string(m, '\n');
        }
        return outValue;
#else
    //Moves either up or down using ANSI sequences
    return "\r" + (m < 0 ? "\33[" + std::to_string(-m) + 'A' : std::string(m, '\n'));
#endif
    }

} // empty namespace

// PRIVATE DATA
//
namespace micfw
{


//============================================================================
/** @class    micfw::KnxFlashing   KnxFlashing.hpp
 *  @ingroup  micfw
 *  @brief    This class encapsulates the flashing process
 *
 *  The \b %KnxFlashing class encapsulates the update process.
 */


//============================================================================
//  P U B L I C   I N T E R F A C E
//============================================================================


//----------------------------------------------------------------------------
/** @fn     KnxFlashing::KnxFlashing(MicDeviceManager* manager, std::shared_ptr<micmgmt::MicOutput>& output,
 *                           const std::string& toolname, MicFwCommands command,
 *                           const std::vector<size_t>& deviceList, micmgmt::FabImagesPaths& fip,
 *                           mockCreateDeviceCallback callback)
 *
 *  @param  manager  Pointer to \a MicDeviceManager implementation
 *  @param  output Reference to \a MicOutput implementation
 *  @param  toolname The name of the tool
 *  @param  command Enumerator of micfw commands
 *  @param  deviceList std::vector with the list of devices to update
 *  @param  fip \c struct that contains all valid image paths
 *  @param  callback Pointer to a function used to set up UTs
 *
 *  Construct a KnxFlashing object.
 */

    KnxFlashing::KnxFlashing(std::shared_ptr<micmgmt::MicDeviceManager> manager,
                             std::shared_ptr<micmgmt::MicOutput>& output,
                             const std::string& toolname, MicFwCommands command,
                             const std::vector<size_t>& deviceList, micmgmt::FabImagesPaths& fip,
                             mockCreateDeviceCallback callback)
        : deviceManager_(manager), output_(output), toolName_(toolname), command_(command),
          interleave_(false), verbose_(false), handler_(NULL), fip_(fip)
    {
        (void)callback; // Avoid compiler error.

        if (deviceList[deviceList.size() - 1] >= deviceManager_->deviceCount(MicDeviceManager::DeviceType::eDeviceTypeAny))
        {
            MicFwError err(toolName_);
            throw MicException(err.buildError(eMicFwDeviceListBad, ": '" +
                micmgmt::deviceName(static_cast<unsigned int>(deviceList[deviceList.size() - 1])) + "'"));
        }
        MicDeviceFactory factory(deviceManager_.get());
        string notReady = "";
        string notOnline = "";
        vector<MicDevice*> list;
        if (command_ == MicFwCommands::eUpdate || command == MicFwCommands::eGetDeviceVersion)
        {
            for (auto it = deviceList.begin(); it != deviceList.end(); ++it)
            {
                MicDevice* device = NULL;
#ifdef UNIT_TESTS
                if(callback != NULL)
                {
                    device = callback(factory, micmgmt::deviceName((unsigned int)*it),
                                      MicDeviceManager::DeviceType::eDeviceTypeKnl);
                }
                else
                {
                    device = factory.createDevice(*it);
                }
#else
                device = factory.createDevice(*it);
#endif
                if (device == NULL)
                {
                    MicFwError err(toolName_);
                    throw MicException(err.buildError(MicFwErrorCode::eMicFwDeviceCreateFailed,
                                                      ": " + to_string((long long)*it)));
                }
                if (device->open() != 0)
                {
                    MicFwError err(toolName_);
                    throw MicException(err.buildError(MicFwErrorCode::eMicFwDeviceOpenFailed,
                                                      ": " + device->deviceName()));
                }
                if (command_ == MicFwCommands::eUpdate && device->deviceState() != MicDevice::DeviceState::eReady)
                {
                    if (notReady.empty() == false)
                    {
                        notReady += ", ";
                    }
                    notReady += device->deviceName();
                    device->close();
                    delete device;
                }
                else
                {
                    list.push_back(device);
                }
            }
        }

        if (notReady.empty() == false)
        {
            for (auto it = list.begin(); it != list.end(); ++it)
            {
                MicDevice* d = (*it);
                delete d;
            }
            list.clear();
            MicFwError err(toolName_);
            throw MicException(err.buildError(MicFwErrorCode::eMicFwDeviceNotReady, notReady));
        }
        else if (notOnline.empty() == false)
        {
            for (auto it = list.begin(); it != list.end(); ++it)
            {
                MicDevice* d = (*it);
                delete d;
            }
            list.clear();
            MicFwError err(toolName_);
            throw MicException(err.buildError(MicFwErrorCode::eMicFwDeviceNotOnline, notOnline));
        }
        else
        {
            for (auto it = list.begin(); it != list.end(); ++it)
            {
                deviceList_.push_back(*it);
            }
            list.clear();
        }
    }


//----------------------------------------------------------------------------
/** @fn     KnxFlashing::~KnxFlashing()
 *
 *  Cleanup.
 */

    KnxFlashing::~KnxFlashing()
    {
        for (auto it = deviceList_.begin(); it != deviceList_.end(); ++it)
        {
            MicDevice* d = (*it);
            d->close();
            delete d;
        }
        deviceList_.clear();
        // Erase temporary files
        remove(fip_.nameA.c_str());
        remove(fip_.nameB.c_str());
        remove(fip_.nameX.c_str());
        remove(fip_.nameT.c_str());
    }


//----------------------------------------------------------------------------
/** @fn     KnxFlashing::setTrapHandler(trapSignalHandler handler)
 *  @param  handler The signal trap handler that will be registered
 *
 *  This function registers the signal handler
 */

    void KnxFlashing::setTrapHandler(trapSignalHandler handler)
    {
        handler_ = handler;
    }


//----------------------------------------------------------------------------
/** @fn     KnxFlashing::setImageFiles(const string& imagePath)
 *  @param  imagePath Path of the firmware image that will be processed
 *
 *  This function registers the path of the firmware image that will be processed
 */

    void KnxFlashing::setImageFiles(const string& imagePath)
    {
        imagePath_ = imagePath;
    }


//----------------------------------------------------------------------------
/** @fn     KnxFlashing::setOptions(bool silent, bool interleave, bool verbose)
 *  @param  silent Enables/disables the silent option
 *  @param  interleave Enables/disables the interleave option
 *  @param  verbose Enables/disables the verbose option
 *
 *  This function registers the subcommands
 */

    void KnxFlashing::setOptions(bool silent, bool interleave, bool verbose)
    {
        interleave_ = interleave;
        verbose_ = verbose;
        output_->setSilent(silent);
    }


//----------------------------------------------------------------------------
/** @fn     KnxFlashing::run(modifyDeviceCallback callback2)
 *  @param  callback2 function used to set up UTs
 *
 *  This function performs the update and get the device version
 */

    int KnxFlashing::run(modifyDeviceCallback callback2)
    {
        (void)callback2; // Avoid compiler error.
        int rv = 0;
#ifdef UNIT_TESTS
        if (callback2 != NULL)
        {
            for (auto it = deviceList_.begin(); it != deviceList_.end(); ++it)
            {
                MicDevice* device = (*it);
                callback2(device);
            }
        }
#endif
        if (command_ == MicFwCommands::eUpdate)
        {
            /* Creates the FlashImage object and sets it up with the path to the
             * firmware image that will be used to update the selected cards
             */
            FlashData data;
            FlashImagePtr fi = shared_ptr<FlashImage>(new FlashImage());
            if(fi->setFilePath(imagePath_))
            {
                MicFwError err(toolName_);
                throw MicException(err.buildError(MicFwErrorCode::eMicFwFileImageBad, ": Image is empty"));
            }
            for (auto it = deviceList_.begin(); it != deviceList_.end(); ++it)
            {
                MicDevice* device = (*it);
                data.push_back(tuple<MicDevice*, FlashImagePtr, micmgmt::FabImagesPaths&>(device, fi, fip_));
                if (!deviceManager_->isValidConfigCard(device->deviceNum()))
                {
                    LOG(WARNING_MSG, device->deviceName() +
                        ": is in invalid state and will be skipped from the firmware update");
                }
            }
            targetFirmware_ = fi->itemVersions();

            MsTimer start(0);
            registerTrapSignalHandler(handler_);

            // Flash all selected cards
            rv = flashDevices(data);

            unregisterTrapSignalHandler(handler_);

            if (rv == MicFwErrorCode::eMicFwSuccess)
            {
                output_->outputLine("*** NOTE: You must reboot the host for the KNL changes to take effect!");
            }
            else if (rv == MicFwErrorCode::eMicFwUpdateDevNotAvailable)
            {
                output_->outputLine("*** NOTE: One or more devices are in an invalid state and were skipped from the firmware update. Please restart the host system and try again, or contact your Intel (R) representative for further assistance");
            }
            else if (rv == MicFwErrorCode::eMicFwUnknownFabVersion)
            {
                output_->outputLine("*** NOTE: The SMC firmware was skipped during the update. Please run the micfw update command again to complete the firmware update!");
            }
            else
            {
                output_->outputLine("*** NOTE: One or more KNL devices failed on firmware update, reflash the failed cards!");
            }
        }
        /* Default option MicFwCommands::eGetDeviceVersion
         * All invalid subcommands throw an exception in the Parser
         */
        else
        {
            rv = getDeviceVersions();
        }

        return rv;
    }


//----------------------------------------------------------------------------
/** @fn     KnxFlashing::flashStatusUpdate(const int deviceNumber, const int currentLine,
 *                                         std::string& msg, const int errorCode)
 *  @param  deviceNumber Card id
 *  @param  currentLine number of row to print
 *  @param  msg message to print
 *  @param  errorCode [optional] Error code
 *
 *  This function prints messages and error messages to a specific row
 */

    void KnxFlashing::flashStatusUpdate(const int deviceNumber, const int currentLine,
                                        const std::string& msg, const int errorCode)
    {
        lock_guard<mutex> guard(callbackCriticalSection);
        // Print error message
        if (errorCode > 0)
        {
            MicFwError err(toolName_);
            auto e = err.buildError(errorCode, msg);
            if(!interleave_)
            {
                int padding = maxColumnSize - static_cast<int>(e.second.length());
                output_->outputError(PrintInLine(currentLine) + e.second
                                     + (padding > 0 ? std::string(padding,' '):""),e.first, true);
            }
            else
            {
                output_->outputError(e.second, e.first);
            }
            flashSummary_.insert(std::make_pair(deviceNumber, "Failed"));
        }
        // Print message
        else
        {
            string errMsg = deviceName((unsigned int)deviceNumber) + ": " + msg;
            if(!interleave_)
            {
                //Pad columns using white spaces in case the current row length is
                //shorter than the previous one
                int padding = maxColumnSize - static_cast<int>(errMsg.length());
                output_->outputLine(PrintInLine(currentLine) + errMsg + (padding > 0 ?
                                    std::string(padding,' '):""), false, true);
            }
            else
            {
                output_->outputLine(errMsg, false, true);
            }
        }
    }


//----------------------------------------------------------------------------
/** @fn     KnxFlashing::flashStatusUpdate(const int deviceNumber, const micmgmt::FlashStatus& status,
 *                                 const int currentLine, FlashStatus::ProgressStatus& progressStatus)
 *  @param  deviceNumber Card id
 *  @param  status Reference to \a FlashStatus implementation
 *  @param  currentLine number of row to print
 *  @param  ProgressStatus Enumerator with the current status of the flashing process
 *
 *  This function prints the stage flashing and its progress
 */

    void KnxFlashing::flashStatusUpdate(const int deviceNumber, const micmgmt::FlashStatus& status,
                      const int currentLine, /*int */FlashStatus::ProgressStatus& progressStatus)
    {
        lock_guard<mutex> guard(callbackCriticalSection);
        string msg = deviceName((unsigned int)deviceNumber) + ": ";
        switch(status.state())
        {
            case FlashStatus::State::eIdle:
            case FlashStatus::State::eBusy:
                switch(status.stage())
                {
                    case KnlDeviceBase::FlashStage::eIdleStage:
                        msg += "iflash32.efi is starting...";
                        break;
                    case KnlDeviceBase::FlashStage::eNextStage:
                        msg += "Waiting for iflash32.efi to start...";
                        break;
                    case KnlDeviceBase::FlashStage::eBiosStage:
                    case KnlDeviceBase::FlashStage::eSmcStage:
                    case KnlDeviceBase::FlashStage::eMeStage:
                        msg += "'" + status.stageText() + "' firmware update progress is "
                               + to_string(status.progress()) + "%...";
                        break;
                    case KnlDeviceBase::FlashStage::eErrorStage:
                        progressStatus = FlashStatus::ProgressStatus::eProgressError;
                        break;
                    default:
                        break;
                }
                break;
            case FlashStatus::State::eFailed:
                msg += "'" + status.stageText() + "' iflash32.efi reported an error.";
                progressStatus = FlashStatus::ProgressStatus::eProgressError;
                break;
            case FlashStatus::State::eAuthFailedVM:
                msg += "'" + status.stageText() + "' firmware protection is enabled.";
                progressStatus = FlashStatus::ProgressStatus::eVMError;
                break;
            case FlashStatus::State::eStageComplete:
                msg += "'" + status.stageText() + "' firmware update completed.";
                break;
            case FlashStatus::State::eDone:
                progressStatus = FlashStatus::ProgressStatus::eComplete;
                msg += "All firmware updates for this coprocessor completed successfully.";
                break;
            case FlashStatus::State::eUnknownFab:
                flashSummary_.insert(std::make_pair(deviceNumber, "Warning"));
                LOG(INFO_MSG, msg + "Pass");
                return;
            case FlashStatus::State::eVerify:
                flashSummary_.insert(std::make_pair(deviceNumber, "Successful"));
                LOG(INFO_MSG, msg + "Pass");
                return;
            default:
                break;
        }
        if(verbose_)
        {
            LOG(INFO_MSG, msg);
        }
        else
        {
            if(!interleave_)
            {
                //Pad columns using white spaces in case the current row length is
                //shorter than the previous one
                int padding = maxColumnSize - static_cast<int>(msg.length());
                output_->outputLine(PrintInLine(currentLine) + msg + (padding > 0 ?
                                    std::string(padding,' '):""), false, true);
            }
            else
            {
                output_->outputLine(msg, false, true);
            }
        }
    }


//============================================================================
//  P R I V A T E   I N T E R F A C E
//============================================================================

    int KnxFlashing::buildError(uint32_t code, const std::string& specialMessage) const
    {
        MicFwError e(toolName_);
        auto err = e.buildError(code, specialMessage);
        LOG(ERROR_MSG, err.second);
        output_->outputError(err.second, err.first);
        return (int)code;
    }

    int KnxFlashing::flashDevices(FlashData& data)
    {
        int rv = 0;
        vector<shared_ptr<FlashWorkItem>> flashObjects;
        threadPool_ = unique_ptr<ThreadPool>(new ThreadPool((unsigned int)data.size()));
        flashSummary_.clear();
        CountdownMutex mtx(data.size());
        output_->outputLine("The update process may take several minutes, please be patient:");
        unsigned currentLine = 1;
        output_->outputLine("Flashing process with file:");
        for (auto& it : targetFirmware_)
        {
            const string& name = it.first;
            const string& version = it.second;
            output_->outputNameValuePair(name, version);
        }
#ifdef _WIN32
        //It is mandatory to compute the current coordinate(for printing)
        //in this point for windows because the windows console does not support
        //ANSI ESC sequences
        h = GetStdHandle(STD_OUTPUT_HANDLE);
        GetConsoleScreenBufferInfo(h, &currentCoordinate);
#endif
        for (auto it = data.begin(); it != data.end(); ++it)
        {
            auto dataItem = *it;
            auto device = get<0>(dataItem);
            auto image = get<1>(dataItem);
            auto& iPaths = get<2>(dataItem);
            if (!deviceManager_->isValidConfigCard(device->deviceNum()))
            {
                if(!verbose_)
                {
                    output_->outputLine(PrintInLine(currentLine) + device->deviceName() +
                        ": Device is in invalid state", false, true);
                }
                mtx--;
                currentLine++;
                rv = MicFwErrorCode::eMicFwUpdateDevNotAvailable;
                continue;
            }
            auto work = shared_ptr<FlashWorkItem>(new FlashWorkItem(device, &mtx,
                                                  currentLine, this, iPaths));
            work->setFirmwareImage(image.get());
            work->setInterleave(interleave_);
            flashObjects.push_back(work);
            threadPool_->addWorkItem(work);
            currentLine++;
        }
        mtx.waitForZero();
#ifdef _WIN32
        output_->outputLine("");
#else
        output_->outputLine(PrintInLine(currentLine * !verbose_) + "", false, true);
#endif
        output_->startSection("Summary");
        for (auto it = deviceList_.begin(); it != deviceList_.end(); ++it)
        {
            MicDevice* device = (*it);
            string result = "Skipped";
            try
            {
                result = flashSummary_.at(static_cast<size_t>(device->deviceNum()));
            }
            catch (exception&) { }
            output_->outputNameValuePair(device->deviceName(), result);
            if (rv == 0 && result == "Failed")
            {
                rv = MicFwErrorCode::eMicFwGeneralError;
                continue;
            }
            if (rv == 0 && result == "Warning")
            {
                rv = MicFwErrorCode::eMicFwUnknownFabVersion;
            }
        }
        output_->endSection(); // Summary
        output_->outputLine();
        return rv;
    }

    int KnxFlashing::getDeviceVersions()
    {
        for (auto it = deviceList_.begin(); it != deviceList_.end(); ++it)
        {
            auto& device = *it;
            MicVersionInfo info;
            bool validCardResult = deviceManager_->isValidConfigCard(device->deviceNum());
            if (!validCardResult)
            {
                LOG(WARNING_MSG, device->deviceName() + ": is in an invalid state");
            }

            uint32_t rv = device->getVersionInfo(&info);
            if (rv != 0)
            {
                switch (rv)
                {
                case MICSDKERR_DEVICE_NOT_OPEN:
                case MICSDKERR_DEVICE_IO_ERROR:
                case MICSDKERR_INTERNAL_ERROR:
                    LOG(INFO_MSG, "getVersionInfo() failed with error code: " + to_string((long long)rv));
                    throw MicException(buildError(MicFwErrorCode::eMicFwDeviceOpenFailed, ": " + device->deviceName()));
                    break;
                case MICSDKERR_DEVICE_BUSY:
                    throw MicException(buildError(MicFwErrorCode::eMicFwDeviceBusy, ": " + device->deviceName()));
                    break;
                default:
                    LOG(INFO_MSG, "getVersionInfo() failed with error code: " + to_string((long long)rv));
                    throw MicException(buildError(MicFwErrorCode::eMicFwUnknownError, ": " + device->deviceName()));
                    break;
                }
            }
            LOG(INFO_MSG, device->deviceName() + ": PCB Version: " + info.fabVersion().value());

            output_->startSection(device->deviceName());
            output_->outputNameMicValuePair("BIOS Version",       info.biosVersion());
            output_->outputNameMicValuePair("ME Version",         info.meVersion());
            output_->outputNameMicValuePair("SMC Version",        info.smcVersion());
            output_->outputNameMicValuePair("NTB EEPROM Version", info.ntbVersion());
            output_->endSection();
        }
        return 0;
    }
} // namespace micfw
