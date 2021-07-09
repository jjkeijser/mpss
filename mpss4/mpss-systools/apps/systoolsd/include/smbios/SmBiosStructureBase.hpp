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

#ifndef _SYSTOOLS_SYSTOOLSD_SMBIOSSTRUCTUREBASE_HPP_
#define _SYSTOOLS_SYSTOOLSD_SMBIOSSTRUCTUREBASE_HPP_

#include <string>
#include <memory>
#include <vector>

#include "SmBiosTypes.hpp"

struct SmBiosStructHeader;

class SmBiosStructureBase
{
public:
    virtual ~SmBiosStructureBase() { };
    static std::shared_ptr<SmBiosStructureBase> create_structure(SmBiosStructureType type);
    static std::vector<std::string> get_strings(SmBiosStructHeader &hdr, const uint8_t *const buf, size_t len);
    static void set_string(std::string &str_to_set, std::vector<std::string> &strings, uint8_t which);
    virtual void validate_and_build(SmBiosStructHeader &hdr, const uint8_t *const buf, size_t len) = 0;
    static void check_size(SmBiosStructHeader &hdr, uint8_t expected_size);
    typedef std::shared_ptr<SmBiosStructureBase> ptr;
};

#endif //_SYSTOOLS_SYSTOOLSD_SMBIOSSTRUCTUREBASE_HPP_
