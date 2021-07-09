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

#ifndef MICFW_KNXFLASHING_HPP
#define MICFW_KNXFLASHING_HPP

#include "Common.hpp"
#include "FlashingCallbackInterface.hpp"
#include "FlashImage.hpp"
#include "FlashWorkItem.hpp"

#include <unordered_map>
#include <tuple>

// Forward references...
namespace micmgmt
{
    class MicOutput;
    class ThreadPool;
    struct FabImagesPaths;
}; // namespace micmgmt

namespace micmgmt
{
    class MicDeviceManager;
    class MicDevice;
    class MicDeviceManager;
    class FlashImage;
    class FlashStatus;
    class MicDeviceFactory;
}; // namespace micmgmt

namespace micfw
{
#ifdef UNIT_TESTS
    typedef micmgmt::MicDevice* (*mockCreateDeviceCallback)(micmgmt::MicDeviceFactory& factory, const std::string& cardName, int type);
    typedef void(*modifyDeviceCallback)(micmgmt::MicDevice* device);
#else
    typedef void *mockCreateDeviceCallback;
    typedef void *modifyDeviceCallback;
#endif

    class KnxFlashing : public FlashingCallbackInterface
    {
    public: // types
        typedef std::shared_ptr<micmgmt::FlashImage> FlashImagePtr;
        typedef std::vector<std::tuple<micmgmt::MicDevice*, FlashImagePtr, micmgmt::FabImagesPaths&>> FlashData;

    public: // constructor/destructors
        KnxFlashing(std::shared_ptr<micmgmt::MicDeviceManager> manager, std::shared_ptr<micmgmt::MicOutput>& output,
                    const std::string& toolName, MicFwCommands command, const std::vector<size_t>& deviceList,
                    micmgmt::FabImagesPaths& fip, mockCreateDeviceCallback callback = NULL);
        virtual ~KnxFlashing();

    public: // API
        void setTrapHandler(micmgmt::trapSignalHandler handler);
        void setImageFiles(const std::string& imagePath);
        void setOptions(bool silent, bool interleave, bool verbose);
        int run(modifyDeviceCallback callback2);

        void signalEvent(int sigInt);

    public: // Interface Implementations
        virtual void flashStatusUpdate(const int deviceNumber, const int currentLine,
                                       const std::string& msg, const int errorCode = 0);
        virtual void flashStatusUpdate(const int deviceNumber, const micmgmt::FlashStatus& status,
                     const int currentLine, /*int*/ micmgmt::FlashStatus::ProgressStatus& progressStatus);

    private: // Hidden special members
        KnxFlashing();
        KnxFlashing(const KnxFlashing&);
        KnxFlashing& operator=(const KnxFlashing&);

    private: // Implementation
        int buildError(uint32_t code, const std::string& specialMessage = "") const;
        int flashDevices(FlashData& data);
        int getDeviceVersions();

    private: // Fields
        std::shared_ptr<micmgmt::MicDeviceManager>       deviceManager_;
        std::shared_ptr<micmgmt::MicOutput>              output_;
        std::string                                      toolName_;
        MicFwCommands                                    command_;
        bool                                             interleave_;
        bool                                             verbose_;
        bool                                             silentUpdate_;
        std::string                                      imagePath_;
        std::vector<micmgmt::MicDevice*>                 deviceList_;
        int                                              timeoutSeconds_;
        std::unique_ptr<micmgmt::ThreadPool>             threadPool_;
        std::unordered_map<size_t, std::string>          flashSummary_;
        micmgmt::trapSignalHandler                       handler_;
        micmgmt::FabImagesPaths&                         fip_;
        std::map<std::string, std::string>               targetFirmware_;
    };
} // namespace micfw

#endif // MICFW_KNXFLASHING_HPP
