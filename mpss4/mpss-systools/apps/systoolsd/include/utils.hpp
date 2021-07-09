/*
 * Copyright (c) 2017, Intel Corporation.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
*/

#ifndef _SYSTOOLS_SYSTOOLSD_UTILS_HPP_
#define _SYSTOOLS_SYSTOOLSD_UTILS_HPP_

#include <cstdint>
#include <string>

namespace utils
{
    uint64_t get_u64_from_line(const std::string &line);
    uint64_t get_u64_from_line(const char *line);
}

#endif //_SYSTOOLS_SYSTOOLSD_UTILS_HPP_
