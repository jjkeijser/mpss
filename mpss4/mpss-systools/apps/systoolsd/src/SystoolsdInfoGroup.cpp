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

#include "info/SystoolsdInfoGroup.hpp"
#include "systoolsd_api.h"

SystoolsdInfoGroup::SystoolsdInfoGroup() : CachedDataGroupBase<SystoolsdInfo>(0)
{

}

void SystoolsdInfoGroup::refresh_data()
{
    data.major_ver = SYSTOOLSD_MAJOR_VER;
    data.minor_ver = SYSTOOLSD_MINOR_VER;
}
