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
#include "SubcommandDef_p.hpp"

namespace micmgmt
{
    class EvaluatorSubcommandDef : public EvaluateArgumentCallbackInterface
    {
    public:
        virtual bool checkArgumentValue(EvaluateArgumentType type,
                                        const std::string& typeName,
                                        const std::string& subcommandOrOptionName,
                                        const std::string& value)
        {
            EXPECT_EQ(EvaluateArgumentType::ePositionalArgument, type);
            EXPECT_STREQ("argument", typeName.c_str());
            EXPECT_STREQ("subcommandName", subcommandOrOptionName.c_str());
            if (value == "enable" || value == "disable")
            {
                return true;
            }
            return false;
        }
    };

    TEST(common_framework, TC_KNL_mpsstools_SubcommandDef_001)
    {
        std::vector<std::string> good_help = { "paragraph 1  ", " paragraph    2", "  paragraph         3 \n " };
        std::vector<std::string> bad_help = { "   \t \n ", "paragraph 2", "paragraph 3" };
        std::vector<std::string> empty_help = {};

        EXPECT_THROW(SubcommandDef sd("", good_help), std::invalid_argument);
        EXPECT_THROW(SubcommandDef sd("\n subcommandname\t", bad_help), std::invalid_argument);
        EXPECT_THROW(SubcommandDef sd(" subcommandname\n", empty_help), std::invalid_argument);
        {
            SubcommandDef sd("subcommandName", good_help);
            EXPECT_STREQ("subcommandName", sd.subcommandName().c_str());
            EXPECT_STREQ("paragraph 1", sd.subcommandHelp().at(0).c_str());
            EXPECT_STREQ("paragraph 2", sd.subcommandHelp().at(1).c_str());
            EXPECT_STREQ("paragraph 3", sd.subcommandHelp().at(2).c_str());
        }
    }

    TEST(common_framework, TC_KNL_mpsstools_SubcommandDef_002)
    {
        std::vector<std::string> good_help = { "paragraph 1  ", " paragraph    2", "  paragraph         3 \n " };
        SubcommandDef sd("subcommandName", good_help);
        ASSERT_NO_THROW(sd.addArgument("arg1", good_help, NULL));
        ASSERT_NO_THROW(sd.addArgument(" arg2", good_help, NULL));
        ASSERT_NO_THROW(sd.addArgument("arg3 ", good_help, NULL));
        ASSERT_NO_THROW(sd.addArgument(" arg4 ", good_help, NULL));

        ASSERT_EQ((unsigned int)4, sd.positionalArgumentSize());
        EXPECT_STREQ("arg1", sd.positionalArgumentAt(0).name().c_str());
        EXPECT_STREQ("arg2", sd.positionalArgumentAt(1).name().c_str());
        EXPECT_STREQ("arg3", sd.positionalArgumentAt(2).name().c_str());
        EXPECT_STREQ("arg4", sd.positionalArgumentAt(3).name().c_str());
        EXPECT_THROW(sd.positionalArgumentAt(4).name(), std::out_of_range);
        EXPECT_THROW(sd.positionalArgumentAt(1000).name(), std::out_of_range);

        EXPECT_STREQ("paragraph 2", sd.positionalArgumentAt(0).help().at(1).c_str());
        EXPECT_STREQ("paragraph 1", sd.positionalArgumentAt(2).help().at(0).c_str());
        EXPECT_STREQ("paragraph 3", sd.positionalArgumentAt(1).help().at(2).c_str());

        EXPECT_EQ(EvaluateArgumentType::ePositionalArgument, sd.positionalArgumentAt(0).type());
        EXPECT_TRUE(sd.positionalArgumentAt(2).isRequired());
        EXPECT_STREQ("", sd.positionalArgumentAt(1).defaultValue().c_str());
    }

    TEST(common_framework, TC_KNL_mpsstools_SubcommandDef_003)
    {
        std::vector<std::string> good_help = { "paragraph 1  ", " paragraph    2", "  paragraph         3 \n " };
        EvaluatorSubcommandDef eval = EvaluatorSubcommandDef();
        SubcommandDef sd("subcommandName", good_help);
        ASSERT_NO_THROW(sd.addArgument("argument", good_help, &eval));
        ASSERT_EQ((unsigned int)1, sd.positionalArgumentSize());
        EXPECT_TRUE(sd.positionalArgumentAt(0).checkArgumentValue("enable", "subcommandName"));
        EXPECT_TRUE(sd.positionalArgumentAt(0).checkArgumentValue("disable", "subcommandName"));
        EXPECT_FALSE(sd.positionalArgumentAt(0).checkArgumentValue("crazyEddie", "subcommandName"));
    }
}; // namespace micmgmt
