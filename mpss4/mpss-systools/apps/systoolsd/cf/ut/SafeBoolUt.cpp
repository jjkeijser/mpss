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

#include <gtest/gtest.h>
#include "SafeBool.hpp"

namespace micmgmt
{
    TEST(common_framework, TC_KNL_mpsstools_SafeBool_001)
    {
        {
            SafeBool sb(true);
            EXPECT_TRUE(sb.value_);
        }
        {
            SafeBool sb;
            EXPECT_FALSE(sb.value_);
        }
        SafeBool tru(true);
        SafeBool fls;

        EXPECT_TRUE(tru != fls);
        EXPECT_FALSE(tru == fls);
        EXPECT_TRUE(tru == true);
        EXPECT_TRUE(fls == false);
        EXPECT_TRUE(true == tru);
        EXPECT_TRUE(false == fls);
        EXPECT_TRUE(tru == true);
        EXPECT_TRUE(fls == false);
        EXPECT_TRUE(true == tru);
        EXPECT_TRUE(false == fls);

        {
            SafeBool sb;
            sb = true;
            EXPECT_TRUE(sb.value_);
            sb = false;
            EXPECT_FALSE(sb.value_);
        }

        {
            SafeBool sb1;
            SafeBool sb2;
            sb1 = true;
            sb2 = sb1;
            EXPECT_TRUE(sb2);
            EXPECT_TRUE(sb1 = sb1);
        }
    }
}; // namespace micmgmt
