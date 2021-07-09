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

#ifndef SYSTOOLS_SYSTOOLSD_SYSTOOLSDINFOGROUP_HPP_
#define SYSTOOLS_SYSTOOLSD_SYSTOOLSDINFOGROUP_HPP_

#include "CachedDataGroupBase.hpp"
#include "systoolsd_api.h"

class SystoolsdInfoGroup : public CachedDataGroupBase<SystoolsdInfo>
{
public:
    SystoolsdInfoGroup();

protected:
    void refresh_data();

private:
    SystoolsdInfoGroup(const SystoolsdInfoGroup &);
    SystoolsdInfoGroup &operator=(const SystoolsdInfoGroup &);
};

#endif //SYSTOOLS_SYSTOOLSD_SYSTOOLSDINFOGROUP_HPP_
