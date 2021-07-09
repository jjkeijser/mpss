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

#ifndef SYSTOOLS_SYSTOOLSD_SMBAINFOGROUP_HPP_
#define SYSTOOLS_SYSTOOLSD_SMBAINFOGROUP_HPP_

#include "CachedDataGroupBase.hpp"
#include "SystoolsdServices.hpp"

#include "systoolsd_api.h"

class I2cBase;

class SmbaInfoGroup : public CachedDataGroupBase<struct SmbaInfo>
{
public:
    SmbaInfoGroup(SystoolsdServices::ptr &services);
    const struct SmbaInfo &get_data(bool force_refresh=false);
    void *get_raw_data(bool force_refresh=false);

protected:
    void refresh_data();
    I2cBase *i2c;

private:
    SmbaInfoGroup();
};

#endif
