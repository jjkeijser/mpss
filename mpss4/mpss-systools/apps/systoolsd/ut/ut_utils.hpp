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

#ifndef SYSTOOLS_SYSTOOLSD_UTUTILS_HPP_
#define SYSTOOLS_SYSTOOLSD_UTUTILS_HPP_

#include <iterator>
#include <string>
#include <vector>

#include "smbios/SmBiosTypes.hpp"

typedef uint8_t byte;

template<typename T>
void pack_struct(std::vector<byte> &out_vec, const T *to_pack)
{
    if(!to_pack)
        throw std::invalid_argument("NULL T*");

    const byte *p = reinterpret_cast<const byte*>(to_pack);
    std::copy(p, p + sizeof(T), back_inserter(out_vec));
}

void pack_string(std::vector<byte> &out_vec, const std::string &to_pack);
SmBiosStructHeader get_header(const std::vector<byte> &buf);

#endif //SYSTOOLS_SYSTOOLSD_UTUTILS_HPP_
