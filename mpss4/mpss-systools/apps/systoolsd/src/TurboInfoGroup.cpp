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

#include "Daemon.hpp"
#include "info/TurboInfoGroup.hpp"

TurboInfoGroup::TurboInfoGroup(Services::ptr &services) :
    CachedDataGroupBase<TurboInfo>(300), turbo(0)
{
    if(!services->get_turbo_srv())
        throw std::invalid_argument("NULL turbo");

    turbo = services->get_turbo_srv();
}

void TurboInfoGroup::refresh_data()
{
    data.enabled = turbo->is_turbo_enabled();
    data.turbo_pct = turbo->get_turbo_pct();
}

