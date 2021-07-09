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

#ifndef _SYSTOOLS_SYSTOOLSD_CACHEDDATAGROUPINTERFACE_HPP_
#define _SYSTOOLS_SYSTOOLSD_CACHEDDATAGROUPINTERFACE_HPP_

#include <cstdint>

#ifdef UNIT_TESTS
#define PROTECTED public
#else
#define PROTECTED protected
#endif

/* DataGroupInterface
 * This interface defines the functions to be implemented by data groups
 * used by the Daemon class.
 */
class DataGroupInterface
{
public:
    virtual ~DataGroupInterface() { };
    virtual size_t get_size() = 0;
    virtual void copy_data_to(char *buf, size_t *size) = 0;
    virtual void *get_raw_data(bool force_refresh=false) = 0;
    virtual void force_refresh() = 0;

PROTECTED:
    virtual void refresh_data() = 0;
};

#endif //SYSTOOLS_SYSTOOLSD_CACHEDDATAGROUPINTERFACE_HPP__
