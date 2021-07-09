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
#include "MicOutputError_p.hpp"
#include <unordered_map>

namespace micmgmt
{
    using namespace std;

    namespace
    {
        const unordered_map<uint32_t, string> rawErrorStrings = {
            pair<uint32_t, string>(MicOutputErrorCode::eMicOutputOutputError, "A MicOutput error occured writing to the std::cout formatter"),
            pair<uint32_t, string>(MicOutputErrorCode::eMicOutputErrorError, "A MicOutput error occured writing to the std::cerr formatter"),
            pair<uint32_t, string>(MicOutputErrorCode::eMicOutputStartSectionError, "A MicOutput error occured starting a new section"),
            pair<uint32_t, string>(MicOutputErrorCode::eMicOutputEndSectionError, "A MicOutput error occured ending a new section"),
        };

        const MicOutputError errorBuilder("tool_name");

        std::pair<uint32_t, std::string> goldenAlgorithm(uint32_t code, const std::string& specialMessage)
        {
            string msg = "Error: ";
            code &= 0xff;
            uint32_t fullCode = (code != 0) ? (code | 0x20030000) : (0);
            msg += "tool_name: ";
            if (code > (uint32_t)0)
            {
                msg += rawErrorStrings.at(code);
            }
            if (specialMessage.empty() == false)
            {
                msg += specialMessage;
            }
            return pair<uint32_t, string>(fullCode, msg);
        }
    }

    TEST(common_framework, TC_KNL_mpsstools_MicOutputError_001)
    {
        auto golden = goldenAlgorithm(MicOutputErrorCode::eMicOutputSuccess, "");
        pair<uint32_t, string> result;

        EXPECT_STREQ("tool_name", errorBuilder.toolName().c_str());

        EXPECT_NO_THROW(result = errorBuilder.buildError(MicOutputErrorCode::eMicOutputSuccess, ""));
        EXPECT_EQ(golden.first, result.first);
        EXPECT_STREQ(golden.second.c_str(), result.second.c_str());

        golden = goldenAlgorithm(MicOutputErrorCode::eMicOutputOutputError, "");
        EXPECT_NO_THROW(result = errorBuilder.buildError(MicOutputErrorCode::eMicOutputOutputError, ""));
        EXPECT_EQ(golden.first, result.first);
        EXPECT_STREQ(golden.second.c_str(), result.second.c_str());

        golden = goldenAlgorithm(MicOutputErrorCode::eMicOutputErrorError, "");
        EXPECT_NO_THROW(result = errorBuilder.buildError(MicOutputErrorCode::eMicOutputErrorError, ""));
        EXPECT_EQ(golden.first, result.first);
        EXPECT_STREQ(golden.second.c_str(), result.second.c_str());

        golden = goldenAlgorithm(MicOutputErrorCode::eMicOutputStartSectionError, "");
        EXPECT_NO_THROW(result = errorBuilder.buildError(MicOutputErrorCode::eMicOutputStartSectionError, ""));
        EXPECT_EQ(golden.first, result.first);
        EXPECT_STREQ(golden.second.c_str(), result.second.c_str());

        golden = goldenAlgorithm(MicOutputErrorCode::eMicOutputEndSectionError, "");
        EXPECT_NO_THROW(result = errorBuilder.buildError(MicOutputErrorCode::eMicOutputEndSectionError, ""));
        EXPECT_EQ(golden.first, result.first);
        EXPECT_STREQ(golden.second.c_str(), result.second.c_str());
    }


    TEST(common_framework, TC_KNL_mpsstools_MicOutputError_002)
    {
        auto golden = goldenAlgorithm(MicOutputErrorCode::eMicOutputSuccess, ": Other text");
        pair<uint32_t, string> result;

        EXPECT_STREQ("tool_name", errorBuilder.toolName().c_str());

        EXPECT_NO_THROW(result = errorBuilder.buildError(MicOutputErrorCode::eMicOutputSuccess, ": Other text"));
        EXPECT_EQ(golden.first, result.first);
        EXPECT_STREQ(golden.second.c_str(), result.second.c_str());

        golden = goldenAlgorithm(MicOutputErrorCode::eMicOutputOutputError, ": Other text");
        EXPECT_NO_THROW(result = errorBuilder.buildError(MicOutputErrorCode::eMicOutputOutputError, ": Other text"));
        EXPECT_EQ(golden.first, result.first);
        EXPECT_STREQ(golden.second.c_str(), result.second.c_str());

        golden = goldenAlgorithm(MicOutputErrorCode::eMicOutputErrorError, ": Other text");
        EXPECT_NO_THROW(result = errorBuilder.buildError(MicOutputErrorCode::eMicOutputErrorError, ": Other text"));
        EXPECT_EQ(golden.first, result.first);
        EXPECT_STREQ(golden.second.c_str(), result.second.c_str());

        golden = goldenAlgorithm(MicOutputErrorCode::eMicOutputStartSectionError, ": Other text");
        EXPECT_NO_THROW(result = errorBuilder.buildError(MicOutputErrorCode::eMicOutputStartSectionError, ": Other text"));
        EXPECT_EQ(golden.first, result.first);
        EXPECT_STREQ(golden.second.c_str(), result.second.c_str());

        golden = goldenAlgorithm(MicOutputErrorCode::eMicOutputEndSectionError, ": Other text");
        EXPECT_NO_THROW(result = errorBuilder.buildError(MicOutputErrorCode::eMicOutputEndSectionError, ": Other text"));
        EXPECT_EQ(golden.first, result.first);
        EXPECT_STREQ(golden.second.c_str(), result.second.c_str());
    }

    TEST(common_framework, TC_KNL_mpsstools_MicOutputError_003)
    {
        EXPECT_THROW(errorBuilder.lookupError(0xff), std::out_of_range);
    }
};
