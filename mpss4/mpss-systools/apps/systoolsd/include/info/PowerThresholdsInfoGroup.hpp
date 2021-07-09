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

#ifndef SYSTOOLS_SYSTOOLSD_POWERTHRESHOLDSINFOGROUP_HPP
#define SYSTOOLS_SYSTOOLSD_POWERTHRESHOLDSINFOGROUP_HPP

#include "CachedDataGroupBase.hpp"
#include "SystoolsdServices.hpp"

#include "systoolsd_api.h"

class Daemon;
class PThreshInterface;

class PowerThresholdsInfoGroup : public CachedDataGroupBase<struct PowerThresholdsInfo>
{
public:
    PowerThresholdsInfoGroup(SystoolsdServices::ptr &services);

protected:
    void refresh_data();
    PThreshInterface *pthresh;

private:
    PowerThresholdsInfoGroup();
};


#endif //SYSTOOLS_SYSTOOLSD_POWERTHRESHOLDSINFOGROUP_HPP
