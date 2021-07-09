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

#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "info/ProcStatCore.hpp"

TEST(ProcStatCoreTest, TC_all_values_4_1_001)
{
    std::unique_ptr<ProcStatCoreBase> core = ProcStatCoreBase::get_procstat_core();

    std::string reading = "cpu100 6426 0 10 1097106 0 0 0 0 0 0";
    const int16_t core_id = 100;
    const uint64_t user = 6426;
    const uint64_t nice = 0;
    const uint64_t system = 10;
    const uint64_t idle = 1097106;
    const uint64_t iowait = 0;
    const uint64_t irq = 0;
    const uint64_t softirq = 0;
    const uint64_t steal = 0;
    const uint64_t guest = 0;
    const uint64_t guest_nice = 0;

    EXPECT_EQ(0, core->core_id());
    EXPECT_EQ(0, core->user());
    EXPECT_EQ(0, core->nice());
    EXPECT_EQ(0, core->idle());
    EXPECT_EQ(0, core->system());

    core->reset(reading);
    EXPECT_EQ(core_id, core->core_id());
    EXPECT_EQ(user - guest, core->user());
    EXPECT_EQ(nice - guest_nice, core->nice());
    EXPECT_EQ(idle + iowait, core->idle());
    EXPECT_EQ(system + irq + softirq, core->system());
    EXPECT_EQ(
        (user - guest) + (nice - guest_nice) + (system + irq + softirq) +
            (idle + iowait) + steal + (guest + guest_nice),
        core->total()
    );

    reading = "cpu 6426 0 10 1097106 0 0 0 0 0 0";
    core->reset(reading);
    EXPECT_EQ(-1, core->core_id()); // We expect -1, indicating aggregate line
    EXPECT_EQ(user - guest, core->user());
    EXPECT_EQ(nice - guest_nice, core->nice());
    EXPECT_EQ(idle + iowait, core->idle());
    EXPECT_EQ(system + irq + softirq, core->system());
    EXPECT_EQ(
        (user - guest) + (nice - guest_nice) + (system + irq + softirq) +
            (idle + iowait) + steal + (guest + guest_nice),
        core->total()
    );

}
