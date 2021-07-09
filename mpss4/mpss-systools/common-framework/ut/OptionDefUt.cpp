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
#include "OptionDef_p.hpp"

namespace micmgmt
{
    class EvaluatorOptionDef : public EvaluateArgumentCallbackInterface
    {
    public:
        virtual bool checkArgumentValue(EvaluateArgumentType type,
                                        const std::string& typeName,
                                        const std::string& subcommandOrOptionName,
                                        const std::string& value)
        {
            EXPECT_EQ(EvaluateArgumentType::eOptionArgument, type);
            EXPECT_STREQ("nameArg", typeName.c_str());
            EXPECT_STREQ("name", subcommandOrOptionName.c_str());
            if (value == "" || value == "enable" || value == "disable")
            {
                return true;
            }
            return false;
        }
    };

    TEST(common_framework, TC_KNL_mpsstools_OptionDef_001)
    {
        std::vector<std::string> good_help = { "paragraph 1  ", " paragraph    2", "  paragraph         3 \n " };
        std::vector<std::string> bad_help = { "   \t \n ", "paragraph 2", "paragraph 3" };
        std::vector<std::string> empty_help = {};

        EXPECT_THROW(OptionDef od(" \n \f \t \r    \n", good_help, 0, false, "", empty_help, "", NULL), std::invalid_argument);
        EXPECT_THROW(OptionDef od(" name\n", bad_help, 0, false, "", empty_help, "", NULL), std::invalid_argument);
        EXPECT_THROW(OptionDef od("\tname\r\n", empty_help, 0, false, "", empty_help, "", NULL), std::invalid_argument);
        EXPECT_THROW(OptionDef od("name ", empty_help, 0, false, "", empty_help, "", NULL), std::invalid_argument);
        EXPECT_THROW(OptionDef od(" name", good_help, 'n', false, "nameArg", empty_help, "enable", NULL), std::invalid_argument);
        EXPECT_THROW(OptionDef od("name", good_help, 'n', false, "nameArg", empty_help, "disable", NULL), std::invalid_argument);
        EXPECT_THROW(OptionDef od("name", good_help, 'n', true, "", good_help, "disable", NULL), std::invalid_argument);
        EXPECT_THROW(OptionDef od("name", good_help, 'n', true, "nameArg", empty_help, "disable", NULL), std::invalid_argument);
        {
            OptionDef od(" \tname\t\n ", good_help, 'n', false, "", empty_help, "", NULL);
            EXPECT_FALSE(od.hasArgument());
            EXPECT_STREQ("name", od.optionName().c_str());
            EXPECT_STREQ("paragraph 1", od.optionHelp().at(0).c_str());
            EXPECT_STREQ("paragraph 2", od.optionHelp().at(1).c_str());
            EXPECT_STREQ("paragraph 3", od.optionHelp().at(2).c_str());
            EXPECT_EQ('n', od.shortName());
        }
        {
            OptionDef od(" \tname\t\n ", good_help, 'n', true, "nameArg", good_help, "disable", NULL);
            ASSERT_TRUE(od.hasArgument());
            EXPECT_STREQ("name", od.optionName().c_str());
            EXPECT_STREQ("paragraph 1", od.optionHelp().at(0).c_str());
            EXPECT_STREQ("paragraph 2", od.optionHelp().at(1).c_str());
            EXPECT_STREQ("paragraph 3", od.optionHelp().at(2).c_str());
            EXPECT_EQ('n', od.shortName());
            EXPECT_TRUE(od.argumentDef()->isRequired());
            EXPECT_STREQ("nameArg", od.argumentDef()->name().c_str());
            EXPECT_STREQ("paragraph 1", od.argumentDef()->help().at(0).c_str());
            EXPECT_STREQ("paragraph 2", od.argumentDef()->help().at(1).c_str());
            EXPECT_STREQ("paragraph 3", od.argumentDef()->help().at(2).c_str());
            EXPECT_STREQ("disable", od.argumentDef()->defaultValue().c_str());
            EXPECT_EQ(EvaluateArgumentType::eOptionArgument, od.argumentDef()->type());
        }
        {
            OptionDef od(" \tname\t\n ", good_help, 0, false, "nameArg", good_help, "disable", NULL);
            ASSERT_TRUE(od.hasArgument());
            EXPECT_STREQ("name", od.optionName().c_str());
            EXPECT_STREQ("paragraph 1", od.optionHelp().at(0).c_str());
            EXPECT_STREQ("paragraph 2", od.optionHelp().at(1).c_str());
            EXPECT_STREQ("paragraph 3", od.optionHelp().at(2).c_str());
            EXPECT_EQ(0, od.shortName());
            EXPECT_FALSE(od.argumentDef()->isRequired());
            EXPECT_STREQ("nameArg", od.argumentDef()->name().c_str());
            EXPECT_STREQ("paragraph 1", od.argumentDef()->help().at(0).c_str());
            EXPECT_STREQ("paragraph 2", od.argumentDef()->help().at(1).c_str());
            EXPECT_STREQ("paragraph 3", od.argumentDef()->help().at(2).c_str());
            EXPECT_STREQ("disable", od.argumentDef()->defaultValue().c_str());
            EXPECT_EQ(EvaluateArgumentType::eOptionArgument, od.argumentDef()->type());
        }
    }

    TEST(common_framework, TC_KNL_mpsstools_OptionDef_002)
    {
        std::vector<std::string> good_help = { "paragraph 1  ", " paragraph    2", "  paragraph         3 \n " };
        {
            EvaluatorOptionDef eval = EvaluatorOptionDef();
            OptionDef od("name", good_help, 0, false, "nameArg", good_help, "disable", &eval);
            EXPECT_TRUE(od.checkArgumentValue(""));
            EXPECT_TRUE(od.checkArgumentValue("enable"));
            EXPECT_TRUE(od.checkArgumentValue("disable"));
            EXPECT_FALSE(od.checkArgumentValue("fred"));
        }
    }
}; // namespace micmgmt
