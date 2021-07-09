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

#ifndef MICFW_FLASHINGCALLBACKINTERFACE_HPP
#define MICFW_FLASHINGCALLBACKINTERFACE_HPP

#include "FlashStatus.hpp"

#include <string>

namespace micfw
{
    class FlashingCallbackInterface
    {
    public:
        virtual ~FlashingCallbackInterface() {};
        virtual void flashStatusUpdate(const int deviceNumber, const int currentLine,
                                       const std::string& msg, const int errorCode = 0) = 0;
        virtual void flashStatusUpdate(const int deviceNumber, const micmgmt::FlashStatus& status,
                     const int currentLine, /*int*/ micmgmt::FlashStatus::ProgressStatus& progressStatus) = 0;
    };
} // namespace micfw

#endif // MICFW_FLASHINGCALLBACKINTERFACE_HPP
