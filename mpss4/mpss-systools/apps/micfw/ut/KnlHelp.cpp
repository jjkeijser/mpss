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
    vector<string> sKnlHelpCL_1 = { "--help" };
    vector<string> sKnlHelpCL_2 = { "device-version", "--help" };
    vector<string> sKnlHelpCL_3 = { "file-version", "--help" };
    vector<string> sKnlHelpCL_4 = { "update", "--help" };

    string sHelpTemplate_1 = "\
* version *\nCopyright (C) *, Intel Corporation.\n*Usage:\n*Description:\n*Subcommands:\n*\
device-version\n*file-version <image-filename>\n*<image-filename>:\n*update <device-list>\n\
*<device-list>:\n*Options:\n*-d, --device <device-list>\n*\
device-list:\n*--file <image-filename>\n*image-filename:\n*-h, --help\n*\
--interleave\n*-v, --verbose\n*--version\n*";

    string sHelpTemplate_2 = "* version *\nCopyright (C) *, Intel Corporation.\n*Usage:\n*\
 device-version --help\n* device-version [OPTIONS]\n*Subcommands:\n*device-version\n*";

    string sHelpTemplate_3 = "* version *\nCopyright (C) *, Intel Corporation.\n*Usage:\n*\
 file-version --help\n* file-version [OPTIONS] <image-filename>\n*Subcommands:\n*file-version \
<image-filename>\n*<image-filename>:\n*";

    string sHelpTemplate_4 = "* version *\nCopyright (C) *, Intel Corporation.\n*Usage:\n*\
update --help\n* update [OPTIONS] <device-list>\n*Subcommands:\n*update \
<device-list>\n*<device-list>:\n*";

    string sHelpTemplate_5 = "micfw: Parser error: The '--help' option can only be used on \
the command\nline by itself or with a subcommand\n\n*Option:\n*-h, --help\n*";
}; // empty namespace

namespace micfw
{
    TEST(micfw, TC_KNL_mpsstools_micfw_knl_help_001)
    {
        auto& app = Utilities::knlTestSetup(sKnlHelpCL_1);

        EXPECT_NO_THROW(app.first->run());

        EXPECT_TRUE(Utilities::compareWithMask(Utilities::stream().str(), sHelpTemplate_1)) << Utilities::stream().str();

        Utilities::knlTestTearDown();
    }

    TEST(micfw, TC_KNL_mpsstools_micfw_knl_help_002)
    {
        auto& app = Utilities::knlTestSetup(sKnlHelpCL_2);

        EXPECT_NO_THROW(app.first->run());

        EXPECT_TRUE(Utilities::compareWithMask(Utilities::stream().str(), sHelpTemplate_2)) << Utilities::stream().str();

        Utilities::knlTestTearDown();
    }

    TEST(micfw, TC_KNL_mpsstools_micfw_knl_help_003)
    {
        auto& app = Utilities::knlTestSetup(sKnlHelpCL_3);

        EXPECT_NO_THROW(app.first->run());

        EXPECT_TRUE(Utilities::compareWithMask(Utilities::stream().str(), sHelpTemplate_3)) << Utilities::stream().str();

        Utilities::knlTestTearDown();
    }

    TEST(micfw, TC_KNL_mpsstools_micfw_knl_help_004)
    {
        auto& app = Utilities::knlTestSetup(sKnlHelpCL_4);

        EXPECT_NO_THROW(app.first->run());

        EXPECT_TRUE(Utilities::compareWithMask(Utilities::stream().str(), sHelpTemplate_4)) << Utilities::stream().str();

        Utilities::knlTestTearDown();
    }

}; // namespace micfw
