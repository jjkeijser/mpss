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
#include "MicErrorBase.hpp"
#include <unordered_map>

namespace micmgmt
{
    using namespace std;

    namespace
    {
        const char* TOOLNAME  = "tool_name";
        const char* EXTRATEXT = "Extra Text";

        const unordered_map<uint32_t, string> rawErrorStrings = {
            pair<uint32_t, string>(MicErrorCode::eGeneralError, "General error"),
            pair<uint32_t, string>(MicErrorCode::eUnknownError, "Unknown error"),
            pair<uint32_t, string>(MicErrorCode::eOutOfMemoryError, "Out of memory")
        };

        const MicErrorBase errorBuilder(TOOLNAME);

        std::pair<uint32_t, std::string> goldenAlgorithm(uint32_t code, const std::string& specialMessage)
        {
            string msg = "Error: ";
            code &= 0xff;
            uint32_t fullCode = (code != 0) ? (code | 0x20000000) : (0);
            msg += TOOLNAME;
            msg += ": ";
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

    TEST(common_framework, TC_KNL_mpsstools_MicErrorBase_001)
    {
        auto golden = goldenAlgorithm(MicErrorCode::eSuccess, "");
        pair<uint32_t,string> result;

        EXPECT_STREQ(TOOLNAME, errorBuilder.toolName().c_str());

        EXPECT_NO_THROW(result = errorBuilder.buildError(MicErrorCode::eSuccess, ""));
        EXPECT_EQ(golden.first, result.first);
        EXPECT_STREQ(golden.second.c_str(), result.second.c_str());

        golden = goldenAlgorithm(MicErrorCode::eGeneralError, "");
        EXPECT_NO_THROW(result = errorBuilder.buildError(MicErrorCode::eGeneralError, ""));
        EXPECT_EQ(golden.first, result.first);
        EXPECT_STREQ(golden.second.c_str(), result.second.c_str());

        golden = goldenAlgorithm(MicErrorCode::eUnknownError, "");
        EXPECT_NO_THROW(result = errorBuilder.buildError(MicErrorCode::eUnknownError, ""));
        EXPECT_EQ(golden.first, result.first);
        EXPECT_STREQ(golden.second.c_str(), result.second.c_str());

        golden = goldenAlgorithm(MicErrorCode::eOutOfMemoryError, "");
        EXPECT_NO_THROW(result = errorBuilder.buildError(MicErrorCode::eOutOfMemoryError, ""));
        EXPECT_EQ(golden.first, result.first);
        EXPECT_STREQ(golden.second.c_str(), result.second.c_str());

        EXPECT_THROW(result = errorBuilder.buildError(55, ""), std::out_of_range);
    }

    TEST(common_framework, TC_KNL_mpsstools_MicErrorBase_002)
    {
        auto golden = goldenAlgorithm(MicErrorCode::eSuccess, EXTRATEXT);
        pair<uint32_t, string> result;

        EXPECT_STREQ(TOOLNAME, errorBuilder.toolName().c_str());

        EXPECT_NO_THROW(result = errorBuilder.buildError(MicErrorCode::eSuccess, EXTRATEXT));
        EXPECT_EQ(golden.first, result.first);
        EXPECT_STREQ(golden.second.c_str(), result.second.c_str());

        golden = goldenAlgorithm(MicErrorCode::eGeneralError, EXTRATEXT);
        EXPECT_NO_THROW(result = errorBuilder.buildError(MicErrorCode::eGeneralError, EXTRATEXT));
        EXPECT_EQ(golden.first, result.first);
        EXPECT_STREQ(golden.second.c_str(), result.second.c_str());

        golden = goldenAlgorithm(MicErrorCode::eUnknownError, EXTRATEXT);
        EXPECT_NO_THROW(result = errorBuilder.buildError(MicErrorCode::eUnknownError, EXTRATEXT));
        EXPECT_EQ(golden.first, result.first);
        EXPECT_STREQ(golden.second.c_str(), result.second.c_str());

        golden = goldenAlgorithm(MicErrorCode::eOutOfMemoryError, EXTRATEXT);
        EXPECT_NO_THROW(result = errorBuilder.buildError(MicErrorCode::eOutOfMemoryError, EXTRATEXT));
        EXPECT_EQ(golden.first, result.first);
        EXPECT_STREQ(golden.second.c_str(), result.second.c_str());
    }

    TEST(common_framework, TC_KNL_mpsstools_MicErrorBase_003)
    {
        auto golden = rawErrorStrings.at(2);
        uint32_t code = 0x20000002;
        EXPECT_STREQ(golden.c_str(), errorBuilder.localErrorStringFromGlobalCode(code).c_str());

        code = 0x20330002;
        EXPECT_THROW(errorBuilder.localErrorStringFromGlobalCode(code), std::invalid_argument);

        code = 0x20000052;
        EXPECT_THROW(errorBuilder.localErrorStringFromGlobalCode(code), std::out_of_range);
    }
}; // namespace micmgmt
