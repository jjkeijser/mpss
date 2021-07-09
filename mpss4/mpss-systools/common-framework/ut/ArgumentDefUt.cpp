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
#include "ArgumentDef_p.hpp"

namespace micmgmt
{
    class ValidEval : public EvaluateArgumentCallbackInterface
    {
    public:
        virtual bool checkArgumentValue(EvaluateArgumentType type,
                                        const std::string& typeName,
                                        const std::string& subcommand,
                                        const std::string& value)
        {
            std::string tmp1 = subcommand;
            EXPECT_EQ(EvaluateArgumentType::ePositionalArgument, type);
            EXPECT_STREQ("Real_Name", typeName.c_str());
            EXPECT_STREQ("Fred", value.c_str());
            return true;
        }
    };

    class InvalidEval : public EvaluateArgumentCallbackInterface
    {
    public:
        virtual bool checkArgumentValue(EvaluateArgumentType type,
                                        const std::string& typeName,
                                        const std::string& subcommand,
                                        const std::string& value)
        {
            EXPECT_EQ(EvaluateArgumentType::eOptionArgument, type);
            EXPECT_STREQ("MyName", typeName.c_str());
            EXPECT_STREQ("John", value.c_str());
            EXPECT_STREQ("subcommand", subcommand.c_str());
            return false;
        }
    };

    TEST(common_framework, TC_KNL_mpsstools_ArgumentDef_001)
    {
        std::vector<std::string> good_help = { "paragraph 1  ", " paragraph    2", "  paragraph         3 \n " };
        std::vector<std::string> bad_help = { "   \t \n ", "paragraph 2", "paragraph 3" };
        std::vector<std::string> empty_help = {};

        EXPECT_THROW(ArgumentDef ad("     \t \n\n  \f \r\n", EvaluateArgumentType::ePositionalArgument, good_help, false, "", NULL), std::invalid_argument);
        EXPECT_THROW(ArgumentDef ad("Real_Name", EvaluateArgumentType::ePositionalArgument, empty_help, false, "", NULL), std::invalid_argument);
        EXPECT_THROW(ArgumentDef ad("Real_Name", EvaluateArgumentType::ePositionalArgument, bad_help, false, "", NULL), std::invalid_argument);

        {
            ArgumentDef ad("Real_Name", EvaluateArgumentType::ePositionalArgument, good_help, false, "", NULL);
            EXPECT_STREQ("Real_Name", ad.name().c_str());
            EXPECT_STREQ("paragraph 1", ad.help().at(0).c_str());
            EXPECT_STREQ("paragraph 2", ad.help().at(1).c_str());
            EXPECT_STREQ("paragraph 3", ad.help().at(2).c_str());
            EXPECT_FALSE(ad.isRequired());
            EXPECT_STREQ("", ad.defaultValue().c_str());
        }
        {
            ArgumentDef ad("Real_Name", EvaluateArgumentType::ePositionalArgument, good_help, true, "defValue", NULL);
            EXPECT_TRUE(ad.isRequired());
            EXPECT_STREQ("defValue", ad.defaultValue().c_str());
        }
        {
            ValidEval eval=ValidEval();
            ArgumentDef ad("Real_Name", EvaluateArgumentType::ePositionalArgument, good_help, true, "defValue", &eval);
            EXPECT_TRUE(ad.isRequired());
            EXPECT_STREQ("defValue", ad.defaultValue().c_str());
            EXPECT_TRUE(ad.checkArgumentValue("Fred"));
        }
        {
            InvalidEval eval = InvalidEval();
            ArgumentDef ad("MyName", EvaluateArgumentType::eOptionArgument, good_help, true, "defValue", &eval);
            EXPECT_TRUE(ad.isRequired());
            EXPECT_STREQ("defValue", ad.defaultValue().c_str());
            EXPECT_FALSE(ad.checkArgumentValue("John", "subcommand"));
        }
    }
}; // namespace micmgmt
