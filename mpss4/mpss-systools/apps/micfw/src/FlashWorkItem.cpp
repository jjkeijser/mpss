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
#include "Common.hpp"
#include "FlashWorkItem.hpp"
#include "MicFwError.hpp"
#include "CountdownMutex.hpp"

#include "MsTimer.hpp"
#include "SafeBool.hpp"
#include "MicLogger.hpp"
#include "ConsoleOutputFormatter.hpp"
#include "MicVersionInfo.hpp"
#include "MicException.hpp"

#include <sstream>
#include <iomanip>
#include <stack>

// SYSTEM INCLUDES
//
#include "MicDevice.hpp"
#include "FlashStatus.hpp"
#include "FlashImage.hpp"
#include "micsdkerrno.h"

#ifdef UNIT_TESTS
    #define DELAY(x)
#else
    #define DELAY(x) x
#endif

using namespace micmgmt;
using namespace std;

// LOCAL CONSTANTS
//
namespace
{
    // 50ms delay used as polling time to read spads
    const unsigned int sLoopIterationDelay  = 50;
    // 1 second delay used as timeout for read state
    const size_t sReadDelay                 = 1000;
    // 30 seconds delay used as timeout for boot process
    const size_t sBootTimeout               = 30000;
    // 90 seconds delay used as timeout for reset process
    const size_t sResetTimeout              = 90000;
    // 900 seconds delay used as timeout for flash process
    const size_t sUpdateTimeout             = 900000;
} // empty namespace (constants)

// PRIVATE DATA
//
namespace micfw
{


//============================================================================
/** @class    micfw::FlashWorkItem   FlashWorkItem.hpp
 *  @ingroup  micfw
 *  @brief    This class encapsulates the flashing process
 *
 *  There will be one instance of FlashWorkItem performing the flashing process
 *  per card installed on the system.
 */


//============================================================================
//  P U B L I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     FlashWorkItem::FlashWorkItem(micmgmt::MicDevice* device, CountdownMutex* mtx,
 *                               int currentLine, FlashingCallbackInterface* callback, micmgmt::FabImagesPaths& fip)
 *
 *  @param  device  Pointer to \a device implementation
 *  @param  mtx Pointer to \c mutex implementation
 *  @param  currentLine row number to print
 *  @param  callback function that represents the work to be accomplished
 *  @param  fip \c struct that contains all valid paths of images.
 *
 *  The \b %FlashWorkItem class encapsulates the flash process.
 */

    FlashWorkItem::FlashWorkItem(micmgmt::MicDevice* device, CountdownMutex* mtx,
                                 int currentLine, FlashingCallbackInterface* callback, micmgmt::FabImagesPaths& fip)
        : device_(device), FabImagesPaths_(fip), currentLine_(currentLine),
        callback_(callback), countdownMutex_(mtx), interleave_(false), unknownFab_(false),
        skipBootTest_(false)
    {
    }


//----------------------------------------------------------------------------
/** @fn     FlashWorkItem::~FlashWorkItem()
 *
 *  Cleanup.
 */

    FlashWorkItem::~FlashWorkItem()
    {
        // Nothing to do here...
    }


//----------------------------------------------------------------------------
/** @fn     FlashWorkItem::setFirmwareImage(const micmgmt::FlashImage* image)
 *  @param  image target flash image
 *
 *  Assign the flash image to be used to update the cards. All instances
 *  receive the same flash image. Each FlashWorkItem selects the firmware
 *  that will be updated from the flash image depending on the fab version.
 *  This allows flashing multi-card configurations with different
 *  PCB versions.
 */

    void FlashWorkItem::setFirmwareImage(const micmgmt::FlashImage* image)
    {
        MicVersionInfo info;
        if (image == NULL)
        {
            (*countdownMutex_)--;
            return;
        }
        mFwToUpdate_.reset(new micmgmt::FwToUpdate(image->getFwToUpdate()));
        mTargetFirmware = image->itemVersions();
        // Get PCB version
        if(device_->getVersionInfo(&info))
        {
            LOG(WARNING_MSG, deviceName((unsigned int)device_->deviceNum())
                + ": Failed to get device version");
            (*countdownMutex_)--;
            return;
        }
        LOG(INFO_MSG, deviceName((unsigned int)device_->deviceNum())
            + ": Fab version detected: " + info.fabVersion().value());
        // if fab A choose SMC firmware for fab A
        if (!info.fabVersion().value().compare("Fab_A"))
        {
            mPath_ = FabImagesPaths_.nameA;
            return;
        }
        // if fab B choose SMC firmware for fab B
        if (!info.fabVersion().value().compare("Fab_B"))
        {
            mPath_ = FabImagesPaths_.nameB;
            return;
        }
        // Otherwise discard SMC firmware
        mPath_ = FabImagesPaths_.nameX;
        mFwToUpdate_->mSMC = false;
        LOG(WARNING_MSG, deviceName((unsigned int)device_->deviceNum())
            + ": Unknown PCB version, skipping SMC update");
        unknownFab_ = true;
        skipBootTest_ = mFwToUpdate_->mBT;
    }


//----------------------------------------------------------------------------
/** @fn     FlashWorkItem::setInterleave(bool interleave)
 *  @param  interleave Enable or disable interleave
 *
 *  Change the way the flashing update process is printed. When interleave is
 *  disabled, flashing percentage is overwritten in the same row.
 */

    void FlashWorkItem::setInterleave(bool interleave)
    {
        interleave_ = interleave;
    }


//----------------------------------------------------------------------------
/** @fn     FlashWorkItem::Run(micmgmt::SafeBool& stopSignal)
 *  @param  stopSignal This is a thread safe bool that signals to the
 *                     WorkItemInterface::Run method to halt processing.
 *
 *  A call to this function is used to update each card.
 *  \a FlashWorkItem::Run boots the card using the hddimg image created according
 *  to the PCB version. This function verifies that the \a online_fw state is
 *  reached and then supervises the flash process. Any error during the flash update
 *  causes this function to abort the process. After the flash update is
 *  finished or aborted the card is reseted.
 */

    void FlashWorkItem::Run(micmgmt::SafeBool& stopSignal)
    {
        uint32_t result = 0;
        MsTimer timer;
        FlashStatus::ProgressStatus progressStatus = FlashStatus::ProgressStatus::eWorking;
        FlashStatus status;
        FlashStatus lastStatus;
        callback_->flashStatusUpdate(device_->deviceNum(), currentLine_,
                                     "Starting firmware updates for this coprocessor...");
        lastStatus.set( FlashStatus::eFailed );
        LOG(INFO_MSG, deviceName((unsigned int)device_->deviceNum()) +
                                 ": FlashWorkItem::Run ==>> Calling 'MicDevice->startFlashUpdate()'...");
        /* Notify Micdevice which capsules will be updated and the path to the
           hddimg image where are located.
         */
        result = device_->startFlashUpdate(mPath_, mFwToUpdate_);
        LOG(INFO_MSG, deviceName((unsigned int)device_->deviceNum()) +
                                  ": FlashWorkItem::Run ==>> Called 'MicDevice->startFlashUpdate()' = 0x" + toHex32(result));
#ifndef UNIT_TESTS
        if (result != MICSDKERR_SUCCESS && result != MICSDKERR_DEVICE_IO_ERROR)
#else
        if (result != MICSDKERR_SUCCESS)
#endif
        {
            callback_->flashStatusUpdate(device_->deviceNum(), currentLine_, "",
                                         MicFwErrorCode::eMicFwFailedtoStartFlashing);
            (*countdownMutex_)--;
            return;
        }
        LOG(INFO_MSG, deviceName((unsigned int)device_->deviceNum()) + ": Booting Firmware Image...");
        /* We make sure that the online_fw state is reached from the ready_to_boot state
         * A timeout of 30 secons is used.
         */
        timer.reset(sBootTimeout);
#ifndef UNIT_TESTS
        while(device_->deviceState() == MicDevice::eBootingFW)
        {
            if (timer.expired())
            {
                callback_->flashStatusUpdate(device_->deviceNum(), currentLine_,
                                             ": Failed to boot image", MicFwErrorCode::eMicFwFlashFailed);
                progressStatus = FlashStatus::ProgressStatus::eProgressError;
                break;
            }
            DELAY(MsTimer::sleep(sLoopIterationDelay));
        }
        if (progressStatus != FlashStatus::ProgressStatus::eWorking)
        {
            if (device_->deviceState() != MicDevice::eOnlineFW)
            {
                callback_->flashStatusUpdate(device_->deviceNum(), currentLine_,
                                             ": Failed to boot image", MicFwErrorCode::eMicFwFlashFailed);
                progressStatus = FlashStatus::ProgressStatus::eProgressError;
            }
            LOG(INFO_MSG, deviceName((unsigned int)device_->deviceNum()) + ": Firmware Image is Online");
        }
#endif
        /* After booting the card the startup.nsh script is executed. The first command changes
         * the postcode from 0x7D to 0xE0 wich notifies the driver that the online_fw state was
         * reached. Then startup.nsh executes iflash32.efi. One time for each capsule file.
         * A timeout of 900 seconds is used.
         */
        timer.reset(sUpdateTimeout);
        MsTimer displayTimer(0);
        while ((bool)stopSignal == false && timer.expired() == false &&
               progressStatus == FlashStatus::ProgressStatus::eWorking)
        {
#ifndef UNIT_TESTS
            /* Make sure that the cards is in online_fw state while flashing. Otherwise
             * abort the flashing process.
             */
            if (device_->deviceState() != MicDevice::eOnlineFW)
            {
                callback_->flashStatusUpdate(device_->deviceNum(), currentLine_,
                           ": Firmware stopped working", MicFwErrorCode::eMicFwFlashFailed);
                progressStatus = FlashStatus::ProgressStatus::eProgressError;
                break;
            }
#endif
            // Read the spad values
            result = device_->getFlashUpdateStatus(&status);
            if (result != MICSDKERR_SUCCESS)
            {
                progressStatus = FlashStatus::ProgressStatus::eProgressError;
                break;
            }
            // Avoid printing if there is no change
            if (lastStatus == status)
            {
                DELAY(MsTimer::sleep(sLoopIterationDelay));
                continue;
            }
            callback_->flashStatusUpdate(device_->deviceNum(), status, currentLine_, progressStatus);
            // Make sure that we have a valid transition
            if (!status.isValidTransition(lastStatus))
            {
                progressStatus = FlashStatus::ProgressStatus::eProgressError;
                LOG(ERROR_MSG, deviceName((unsigned int)device_->deviceNum()) + ": Invalid transition");
                break;
            }
            lastStatus = status;
        }
        if (timer.expired())
        { // timed out; critical error!!!!
            callback_->flashStatusUpdate(device_->deviceNum(), currentLine_,
                                         "", MicFwErrorCode::eMicFwTimeoutOcurred);
        }
        else if (progressStatus == FlashStatus::ProgressStatus::eVMError)
        { // Other error; already reported.
            callback_->flashStatusUpdate(device_->deviceNum(), currentLine_,
                                         "", MicFwErrorCode::eMicAuthFailedVM);
        }
        else if (progressStatus != FlashStatus::ProgressStatus::eComplete)
        { // Other error; already reported.
            LOG(INFO_MSG, deviceName((unsigned int)device_->deviceNum())
                + ": Aborting update process");
            callback_->flashStatusUpdate(device_->deviceNum(), currentLine_,
                                         "", MicFwErrorCode::eMicFwFlashFailed);
        }

        /* Apply first reset */
        device_->reset(true);
        timer.reset(sResetTimeout);
        LOG(INFO_MSG, deviceName((unsigned int)device_->deviceNum()) +
            ": Resetting...");
#ifndef UNIT_TESTS
        while (timer.expired() == false)
        {
            if (device_->deviceState() == MicDevice::DeviceState::eReady)
            {
                break;
            }
            DELAY(MsTimer::sleep(sLoopIterationDelay));
        }
        if (timer.expired())
            goto out;
        if (progressStatus == FlashStatus::ProgressStatus::eProgressError)
            goto out;
        LOG(INFO_MSG, deviceName((unsigned int)device_->deviceNum()) +
                        ": is ready to boot");
        /* Verify if the dummy image for the first boot is available */
        if(!mFwToUpdate_->mBT)
        {
            LOG(WARNING_MSG, deviceName((unsigned int)device_->deviceNum()) +
                        ": Firmware image for test boot was not found");
            goto out;
        }
        /* Apply first boot */
        DELAY(MsTimer::sleep(sReadDelay));
        timer.reset(sBootTimeout);
        LOG(INFO_MSG, deviceName((unsigned int)device_->deviceNum()) +
                        ": Applying first boot...");
        result = device_->startFlashUpdate(FabImagesPaths_.nameT, mFwToUpdate_);
        if (result != MICSDKERR_SUCCESS)
        {
            callback_->flashStatusUpdate(device_->deviceNum(),
                            currentLine_, "", MicFwErrorCode::eMicFwFlashFailed);
            (*countdownMutex_)--;
            return;
        }
        while (timer.expired() == false)
        {
            if (device_->deviceState() == MicDevice::DeviceState::eOnlineFW)
            {
                break;
            }
            DELAY(MsTimer::sleep(sLoopIterationDelay));
        }
        if (timer.expired())
            goto out;
        LOG(INFO_MSG, deviceName((unsigned int)device_->deviceNum()) +
                        ": First boot was successful");
        /* Apply second reset */
        device_->reset(true);
        timer.reset(sResetTimeout);
        LOG(INFO_MSG, deviceName((unsigned int)device_->deviceNum()) +
            ": Resetting...");
        while (timer.expired() == false)
        {
            if (device_->deviceState() == MicDevice::DeviceState::eReady)
            {
                break;
            }
            DELAY(MsTimer::sleep(sLoopIterationDelay));
        }
        LOG(INFO_MSG, deviceName((unsigned int)device_->deviceNum()) +
                        ": is ready to boot");
out:
#endif

        if (timer.expired())
        {
            callback_->flashStatusUpdate(device_->deviceNum(), currentLine_,
                                         "", MicFwErrorCode::eMicFwReadyTimeoutOcurred);
        }
        else if (verifyUpdate(progressStatus))
        {
            callback_->flashStatusUpdate(device_->deviceNum(), currentLine_,
                                         "", MicFwErrorCode::eMicFwMissMatchVersion);
        }
        else if ((progressStatus == FlashStatus::ProgressStatus::eComplete) && unknownFab_)
        {
            status.set(FlashStatus::eUnknownFab);
            callback_->flashStatusUpdate(device_->deviceNum(), status,
            currentLine_, progressStatus);
        }
        else if (progressStatus == FlashStatus::ProgressStatus::eComplete)
        {
            status.set(FlashStatus::eVerify);
            callback_->flashStatusUpdate(device_->deviceNum(), status,
                                         currentLine_, progressStatus);
        }
        (*countdownMutex_)--;
    }


//============================================================================
//  P R I V A T E   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     std::string FlashWorkItem::toHex32(uint32_t code)
 *  @param  code   error error code uint32_t
 *  @return error code in string format
 *
 *  Returns the error code in \c std::string format
 */

    std::string FlashWorkItem::toHex32(uint32_t code)
    {
        ostringstream stream;
        stream << std::setbase(16) << setw(8) << setfill('0') << code;
        stream.flush();
        return stream.str();
    }

//----------------------------------------------------------------------------

    bool FlashWorkItem::verifyUpdate(FlashStatus::ProgressStatus progressStatus) const
    {
        MicVersionInfo info;
        bool rv = false;
#ifdef UNIT_TESTS
        return rv;
#endif
        if (progressStatus != FlashStatus::ProgressStatus::eComplete)
        {
            LOG(INFO_MSG, deviceName((unsigned int)device_->deviceNum()) +
                                     ": Skipping firmware verification");
            return false;
        }
        if (device_->getVersionInfo(&info) != 0)
        {
            return true;
        }
        LOG(INFO_MSG, deviceName((unsigned int)device_->deviceNum()) +
            ": Verifying firmware updates...");
        if ((info.biosVersion().value().compare(mTargetFirmware.at(FlashImage::BIOS)) != 0) &&
            mFwToUpdate_->mBios)
        {
            LOG(ERROR_MSG, deviceName((unsigned int)device_->deviceNum()) +
                " BIOS update failed");
            LOG(ERROR_MSG, deviceName((unsigned int)device_->deviceNum()) +
                " Target BIOS version:  " + mTargetFirmware.at(FlashImage::BIOS));
            LOG(ERROR_MSG, deviceName((unsigned int)device_->deviceNum()) +
                " Current BIOS version: " + info.biosVersion().value());
            rv = true;
        }
        if ((info.fabVersion().value().compare("Fab_A") == 0) && mFwToUpdate_->mSMC && !unknownFab_)
        {
            if (mTargetFirmware.at(FlashImage::SMCFABA).find(info.smcVersion().value()) ==
                std::string::npos)
            {
                LOG(ERROR_MSG, deviceName((unsigned int)device_->deviceNum()) +
                    " SMC PCB A update failed");
                LOG(ERROR_MSG, deviceName((unsigned int)device_->deviceNum()) +
                    " Target SMC version:  " + mTargetFirmware.at(FlashImage::SMCFABA));
                LOG(ERROR_MSG, deviceName((unsigned int)device_->deviceNum()) +
                    " Current SMC version: " + info.smcVersion().value());
                rv = true;
            }
        }
        if ((info.fabVersion().value().compare("Fab_B") == 0) && mFwToUpdate_->mSMC && !unknownFab_)
        {
            if (mTargetFirmware.at(FlashImage::SMCFABB).find(info.smcVersion().value()) ==
                std::string::npos)
            {
                LOG(ERROR_MSG, deviceName((unsigned int)device_->deviceNum()) +
                    " SMC PCB B update failed");
                LOG(ERROR_MSG, deviceName((unsigned int)device_->deviceNum()) +
                    " Target SMC version:  " + mTargetFirmware.at(FlashImage::SMCFABB));
                LOG(ERROR_MSG, deviceName((unsigned int)device_->deviceNum()) +
                    " Current SMC version: " + info.smcVersion().value());
                rv = true;
            }
        }
        if ((info.meVersion().value().compare(mTargetFirmware.at(FlashImage::ME)) != 0) &&
            mFwToUpdate_->mME)
        {
            LOG(ERROR_MSG, deviceName((unsigned int)device_->deviceNum()) +
                " ME update failed");
            LOG(ERROR_MSG, deviceName((unsigned int)device_->deviceNum()) +
                " Target ME version:  " + mTargetFirmware.at(FlashImage::ME));
            LOG(ERROR_MSG, deviceName((unsigned int)device_->deviceNum()) +
                " Current ME version: " + info.meVersion().value());
            rv = true;
        }
        return rv;
    }
} // namespace micfw