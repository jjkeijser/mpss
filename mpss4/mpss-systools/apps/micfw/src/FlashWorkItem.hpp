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

#ifndef MICFW_FLASHWORKITEM_HPP
#define MICFW_FLASHWORKITEM_HPP

#include "WorkItemInterface.hpp"
#include "FlashImage.hpp"
#include "FlashStatus.hpp"
#include "FlashingCallbackInterface.hpp"

#include <string>

namespace micmgmt
{
    class MsTimer;
    class FlashImage;
    class MicDevice;
    class FlashStatus;
}; // namespace micmgmt

namespace micfw
{
    class CountdownMutex;
    class FlashingCallbackInterface;

    class FlashWorkItem : public micmgmt::WorkItemInterface
    {
    public: // construction/destruction
        FlashWorkItem(micmgmt::MicDevice* device, CountdownMutex* mtx,
                      int currentLine, FlashingCallbackInterface* callback, micmgmt::FabImagesPaths& fip);
        virtual ~FlashWorkItem();

    public: // Interface implementations...
        virtual void Run(micmgmt::SafeBool& stopSignal);

    public: // API
        void setFirmwareImage(const micmgmt::FlashImage* image);
        void setInterleave(bool interleave);

    private: // Disabled...
        FlashWorkItem();
        FlashWorkItem(const FlashWorkItem&);
        FlashWorkItem& operator=(const FlashWorkItem&);

    private: // Implementation
        std::string toHex32(uint32_t code);
        bool verifyUpdate(micmgmt::FlashStatus::ProgressStatus progressStatus) const;

    private: // Fields
        micmgmt::FlashImage*       image_;
        micmgmt::MicDevice*        device_;
        micmgmt::FabImagesPaths&   FabImagesPaths_;
        int                        currentLine_;
        FlashingCallbackInterface* callback_;
        CountdownMutex*            countdownMutex_;
        bool                       interleave_;
        std::string                mPath_;
        std::unique_ptr<micmgmt::FwToUpdate> mFwToUpdate_;
        std::map<std::string, std::string> mTargetFirmware;
        bool                       unknownFab_;
        bool                       skipBootTest_;
    };
} // namespace micfw

#endif // MICFW_FLASHWORKITEM_HPP