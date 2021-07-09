/*
 * Copyright (c) 2017, Intel Corporation.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
*/

#include <gtest/gtest.h>
#include "MicException.hpp"

namespace micmgmt
{
    using namespace std;

    TEST(common_framework, TC_KNL_mpsstools_MicException_001)
    {
        uint32_t code = 0x20000001;
        string msg = "error string";

        MicException e(code);
        EXPECT_EQ(code, e.getMicErrNo());

        e = MicException(code, "");
        EXPECT_EQ(code, e.getMicErrNo());

        e = MicException(code, msg);
        EXPECT_EQ(code, e.getMicErrNo());
        string errStr = e.what();
        EXPECT_STREQ(msg.c_str(), errStr.c_str());
    }
}; // namespace micmgmt
