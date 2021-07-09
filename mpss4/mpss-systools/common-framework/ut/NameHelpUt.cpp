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
#include <stdexcept>
#include "NameHelp_p.hpp"

namespace micmgmt
{
    TEST(common_framework, TC_KNL_mpsstools_NameHelp_001)
    {
        EXPECT_THROW(NameHelp nh("   \t \n ", "Real help."), std::invalid_argument);
        EXPECT_THROW(NameHelp nh("Real_Name", "   \t \n "), std::invalid_argument);
        EXPECT_THROW(NameHelp nh("   \t \n ", "   \t \n "), std::invalid_argument);
        EXPECT_THROW(NameHelp nh("Real Name", "   \t \n "), std::invalid_argument);

        std::vector<std::string> good_help = { "paragraph 1  ", " paragraph    2", "  paragraph         3 \n " };
        std::vector<std::string> bad_help = { "   \t \n ", "paragraph 2", "paragraph 3" };
        std::vector<std::string> empty_help = { };

        EXPECT_THROW(NameHelp nh("Real_Name", bad_help), std::invalid_argument);
        EXPECT_THROW(NameHelp nh("Real_Name", empty_help), std::invalid_argument);

        NameHelp nh("\n\tReal_Name\n", good_help);
        ASSERT_STREQ("Real_Name", nh.name().c_str());
        ASSERT_STREQ("paragraph 1", nh.help().at(0).c_str());
        ASSERT_STREQ("paragraph 2", nh.help().at(1).c_str());
        ASSERT_STREQ("paragraph 3", nh.help().at(2).c_str());
    }
}; // namespace micmgmt
