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

#ifndef _SYSTOOLS_SYSTOOLSD_VOLTAGEINFOGROUP_HPP_
#define _SYSTOOLS_SYSTOOLSD_VOLTAGEINFOGROUP_HPP_

#include "CachedDataGroupBase.hpp"
#include "SystoolsdServices.hpp"

#include "systoolsd_api.h"

class Daemon;
class I2cBase;
struct DaemonInfoSources;

class VoltageInfoGroup : public CachedDataGroupBase<struct VoltageInfo>
{
public:
    VoltageInfoGroup(SystoolsdServices::ptr &services);

protected:
    void refresh_data();
    I2cBase *i2c;

private:
    VoltageInfoGroup();
};

#endif //_SYSTOOLS_SYSTOOLSD_THERMALINFO_HPP_
