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

#include <cstring>
#include <TestDataGroup.hpp>

const uint32_t TestDataGroup::val1_initial = 1;
const uint32_t TestDataGroup::val2_initial = 2;

TestDataGroup::TestDataGroup(uint64_t timeout) :
    CachedDataGroupBase<TestStruct>(timeout)
{
    memset(&data, 0, sizeof(data));
}

void TestDataGroup::refresh_data()
{
    data.val1 += 1;
    data.val2 += 2;
}
