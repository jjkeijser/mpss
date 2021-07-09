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

#ifndef MICFW_COMMON_HPP
#define MICFW_COMMON_HPP

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <memory>

#include "micmgmtCommon.hpp"

#include "MicFwError.hpp"

namespace micfw
{
    enum MicFwCommands
    {
        eUnknownCommand,
        eGetFileVersion,
        eGetDeviceVersion,
        eUpdate
    };
}

#endif // MICFW_COMMON_HPP
