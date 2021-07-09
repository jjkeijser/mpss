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

#ifndef _SYSTOOLS_SYSTOOLSD_COREUSAGEINFOGROUP_HPP_
#define _SYSTOOLS_SYSTOOLSD_COREUSAGEINFOGROUP_HPP_

#include <vector>

#include "CachedDataGroupBase.hpp"
#include "SystoolsdServices.hpp"

#include "systoolsd_api.h"

class CoreUsageInfoGroup : public CachedDataGroupBase<std::vector<CoreCounters>>
{
public:
    CoreUsageInfoGroup(SystoolsdServices::ptr &services);
    ~CoreUsageInfoGroup();
    size_t get_size();
    void copy_data_to(char *buf, size_t *size);
    void *get_raw_data(bool force_refresh=false); //from DataGroupInterface
    CoreCounters *get_data(bool force_refresh);

protected:
    CoreUsageInfoGroup();
    void refresh_data();
    CoreUsageInfo usage_info;
    char *raw_buf;
    const KernelInterface *kernel;

private:
    CoreUsageInfoGroup(const CoreUsageInfoGroup&);
    CoreUsageInfoGroup &operator=(const CoreUsageInfoGroup&);
};

#endif //_SYSTOOLS_SYSTOOLSD_COREUSAGEINFOGROUP_HPP_
