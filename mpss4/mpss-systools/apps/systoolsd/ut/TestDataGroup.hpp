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

#ifndef _SYSTOOLS_SYSTOOLSD_TESTDATAGROUP_HPP_
#define _SYSTOOLS_SYSTOOLSD_TESTDATAGROUP_HPP_

#include "info/CachedDataGroupBase.hpp"

struct TestStruct
{
    uint32_t val1;
    uint32_t val2;
};

class TestDataGroup : public CachedDataGroupBase<TestStruct>
{
public:
    TestDataGroup(uint64_t timeout);
    const static uint32_t val1_initial;
    const static uint32_t val2_initial;

protected:
    void refresh_data();

};

#endif //SYSTOOLS_SYSTOOLSD_TESTDATAGROUP_HPP__
