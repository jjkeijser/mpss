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

#include <cstring>
#include <iterator>
#include <stdexcept>

#include "ut_utils.hpp"

void pack_string(std::vector<byte> &out_vec, const std::string &to_pack)
{
    std::copy(to_pack.begin(), to_pack.end(), back_inserter(out_vec));
}

SmBiosStructHeader get_header(const std::vector<byte> &buf)
{
    if(buf.size() < sizeof(SmBiosStructHeader))
    {
        throw std::invalid_argument("buffer length must be >= sizeof(SmBiosStructHeader)");
    }
    SmBiosStructHeader hdr;
    memcpy(&hdr, &buf[0], sizeof(hdr));
    return hdr;
}

