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

#include "CommonUt.hpp"

using namespace std;
using namespace micmgmt;

namespace
{
    vector<string> sVersionCL_1 = { "--version" };
    vector<string> sVersionCL_2 = { "--version", "--silent" };
    string sVersionTemplate = "* version *\nCopyright (C) *, Intel Corporation.\n";
}; // empty namespace

namespace micfw
{
    TEST(micfw, TC_KNL_mpsstools_micfw_version_001)
    {
        auto& app = Utilities::knlTestSetup(sVersionCL_1);

        EXPECT_NO_THROW(app.first->run());

        EXPECT_TRUE(Utilities::compareWithMask(Utilities::stream().str(), sVersionTemplate)) << Utilities::stream().str();

        Utilities::knlTestTearDown();
    }

    TEST(micfw, TC_KNL_mpsstools_micfw_version_002)
    {
        auto& app = Utilities::knlTestSetup(sVersionCL_2);

        EXPECT_NE(0, app.first->run()) << Utilities::stream().str();

        Utilities::knlTestTearDown();
    }
}; // namespace micfw
