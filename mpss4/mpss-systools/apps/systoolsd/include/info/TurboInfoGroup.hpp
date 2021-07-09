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

#include "CachedDataGroupBase.hpp"
#include "SystoolsdServices.hpp"

#include "systoolsd_api.h"

struct DaemonInfoSources;
class TurboSettingsInterface;

class TurboInfoGroup : public CachedDataGroupBase<TurboInfo>
{
public:
    TurboInfoGroup(SystoolsdServices::ptr &services);

protected:
    void refresh_data();
    TurboSettingsInterface *turbo;

private:
    TurboInfoGroup();
    TurboInfoGroup (const TurboInfoGroup&);
    TurboInfoGroup &operator=(const TurboInfoGroup&);
};
