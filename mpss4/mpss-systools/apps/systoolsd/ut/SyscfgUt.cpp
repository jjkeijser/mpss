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

#include <sstream>
#include <string>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "mocks.hpp"
#include "PopenInterface.hpp"
#include "Syscfg.hpp"
#include "SystoolsdException.hpp"

using ::testing::NotNull;
using ::testing::Return;

namespace
{

enum SyscfgValues
{
    cache = MemType::cache,
    flat = MemType::flat,
    hybrid = MemType::hybrid,
    disabled,
    enabled,
    auto_
};

std::string generate_scysfg_output(SyscfgValues value)
{
    std::stringstream ss;
    ss << "This is the category header" << std::endl;
    ss << "===========================" << std::endl;
    ss << "Current Value : ";
    switch(value)
    {
        case SyscfgValues::disabled:
            ss << "Disable";
            break;
        case SyscfgValues::enabled:
            ss << "Enable";
            break;
        case SyscfgValues::auto_:
            ss << "Auto";
            break;
        case SyscfgValues::cache:
            ss << "Cache";
            break;
        case SyscfgValues::flat:
            ss << "Flat";
            break;
        case SyscfgValues::hybrid:
            ss << "Hybrid";
            break;
        default:
            //shouldn't happen
            throw;
    }
    ss << std::endl;
    ss << "----------------------";
    return ss.str();
}

class SyscfgTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        mock = new MockPopen;
    }
    MockPopen *mock;
    const char *pass = "";
};

TEST_F(SyscfgTest, TC_syscfg_ecc_enabled_001)
{
    EXPECT_CALL(*mock, run(NotNull()))
        .Times(1);
    EXPECT_CALL(*mock, get_output())
        .WillOnce(Return(generate_scysfg_output(SyscfgValues::enabled)));
    EXPECT_CALL(*mock, get_retcode())
        .WillOnce(Return(0));
    Syscfg syscfg(mock);
    bool enabled = false;
    ASSERT_NO_THROW(enabled = syscfg.ecc_enabled());
    ASSERT_TRUE(enabled);
}

TEST_F(SyscfgTest, TC_syscfg_ecc_disabled_001)
{
    EXPECT_CALL(*mock, run(NotNull()))
        .Times(1);
    EXPECT_CALL(*mock, get_output())
        .WillOnce(Return(generate_scysfg_output(SyscfgValues::disabled)));
    EXPECT_CALL(*mock, get_retcode())
        .WillOnce(Return(0));
    Syscfg syscfg(mock);
    bool enabled = true;
    ASSERT_NO_THROW(enabled = syscfg.ecc_enabled());
    ASSERT_FALSE(enabled);
}

TEST_F(SyscfgTest, TC_syscfg_ecc_unkown_001)
{
    EXPECT_CALL(*mock, run(NotNull()))
        .Times(1);
    EXPECT_CALL(*mock, get_output())
        .WillOnce(Return(generate_scysfg_output(SyscfgValues::cache)));
    EXPECT_CALL(*mock, get_retcode())
        .WillOnce(Return(0));
    Syscfg syscfg(mock);
    //this should throw, because ECC values can only be Enabled or Disabled
    //mock will return Cached
    ASSERT_THROW(syscfg.ecc_enabled(), SystoolsdException);
}

TEST_F(SyscfgTest, TC_syscfg_ecc_throw_by_retcode_001)
{
    EXPECT_CALL(*mock, run(NotNull()))
        .Times(1);
    EXPECT_CALL(*mock, get_retcode())
        .WillOnce(Return(1));
    Syscfg syscfg(mock);
    ASSERT_THROW(syscfg.ecc_enabled(), SystoolsdException);
}

TEST_F(SyscfgTest, TC_syscfg_errinject_enabled_001)
{
    EXPECT_CALL(*mock, run(NotNull()))
        .Times(1);
    EXPECT_CALL(*mock, get_output())
        .WillOnce(Return(generate_scysfg_output(SyscfgValues::enabled)));
    EXPECT_CALL(*mock, get_retcode())
        .WillOnce(Return(0));
    Syscfg syscfg(mock);
    bool enabled = true;
    ASSERT_NO_THROW(enabled = syscfg.errinject_enabled());
    ASSERT_TRUE(enabled);
}

TEST_F(SyscfgTest, TC_syscfg_errinject_disabled_001)
{
    EXPECT_CALL(*mock, run(NotNull()))
        .Times(1);
    EXPECT_CALL(*mock, get_output())
        .WillOnce(Return(generate_scysfg_output(SyscfgValues::disabled)));
    EXPECT_CALL(*mock, get_retcode())
        .WillOnce(Return(0));
    Syscfg syscfg(mock);
    bool enabled = true;
    ASSERT_NO_THROW(enabled = syscfg.errinject_enabled());
    ASSERT_FALSE(enabled);
}

TEST_F(SyscfgTest, TC_syscfg_errinject_unkown_001)
{
    EXPECT_CALL(*mock, run(NotNull()))
        .Times(1);
    EXPECT_CALL(*mock, get_output())
        .WillOnce(Return(generate_scysfg_output(SyscfgValues::hybrid)));
    EXPECT_CALL(*mock, get_retcode())
        .WillOnce(Return(0));
    Syscfg syscfg(mock);
    ASSERT_THROW(syscfg.errinject_enabled(), SystoolsdException);
}

TEST_F(SyscfgTest, TC_syscfg_errinject_throw_by_retcode_001)
{
    EXPECT_CALL(*mock, run(NotNull()))
        .Times(1);
    EXPECT_CALL(*mock, get_retcode())
        .WillOnce(Return(1));
    Syscfg syscfg(mock);
    ASSERT_THROW(syscfg.errinject_enabled(), SystoolsdException);
}

TEST_F(SyscfgTest, TC_syscfg_eist_enabled_001)
{
    EXPECT_CALL(*mock, run(NotNull()))
        .Times(1);
    EXPECT_CALL(*mock, get_output())
        .WillOnce(Return(generate_scysfg_output(SyscfgValues::enabled)));
    EXPECT_CALL(*mock, get_retcode())
        .WillOnce(Return(0));
    Syscfg syscfg(mock);
    bool enabled = true;
    ASSERT_NO_THROW(enabled = syscfg.eist_enabled());
    ASSERT_TRUE(enabled);
}

TEST_F(SyscfgTest, TC_syscfg_eist_disabled_001)
{
    EXPECT_CALL(*mock, run(NotNull()))
        .Times(1);
    EXPECT_CALL(*mock, get_output())
        .WillOnce(Return(generate_scysfg_output(SyscfgValues::disabled)));
    EXPECT_CALL(*mock, get_retcode())
        .WillOnce(Return(0));
    Syscfg syscfg(mock);
    bool enabled = true;
    ASSERT_NO_THROW(enabled = syscfg.eist_enabled());
    ASSERT_FALSE(enabled);
}

TEST_F(SyscfgTest, TC_syscfg_eist_unkown_001)
{
    EXPECT_CALL(*mock, run(NotNull()))
        .Times(1);
    EXPECT_CALL(*mock, get_output())
        .WillOnce(Return(generate_scysfg_output(SyscfgValues::hybrid)));
    EXPECT_CALL(*mock, get_retcode())
        .WillOnce(Return(0));
    Syscfg syscfg(mock);
    ASSERT_THROW(syscfg.eist_enabled(), SystoolsdException);
}

TEST_F(SyscfgTest, TC_syscfg_eist_throw_by_retcode_001)
{
    EXPECT_CALL(*mock, run(NotNull()))
        .Times(1);
    EXPECT_CALL(*mock, get_retcode())
        .WillOnce(Return(1));
    Syscfg syscfg(mock);
    ASSERT_THROW(syscfg.eist_enabled(), SystoolsdException);
}

TEST_F(SyscfgTest, TC_syscfg_memtype_cache_001)
{
    EXPECT_CALL(*mock, run(NotNull()))
        .Times(1);
    EXPECT_CALL(*mock, get_output())
        .WillOnce(Return(generate_scysfg_output(SyscfgValues::cache)));
    EXPECT_CALL(*mock, get_retcode())
        .WillOnce(Return(0));
    Syscfg syscfg(mock);
    MemType type;
    ASSERT_NO_THROW(type = syscfg.get_mem_type());
    ASSERT_EQ(MemType::cache, type);
}

TEST_F(SyscfgTest, TC_syscfg_memtype_flat_001)
{
    EXPECT_CALL(*mock, run(NotNull()))
        .Times(1);
    EXPECT_CALL(*mock, get_output())
        .WillOnce(Return(generate_scysfg_output(SyscfgValues::flat)));
    EXPECT_CALL(*mock, get_retcode())
        .WillOnce(Return(0));
    Syscfg syscfg(mock);
    MemType type;
    ASSERT_NO_THROW(type = syscfg.get_mem_type());
    ASSERT_EQ(MemType::flat, type);
}

TEST_F(SyscfgTest, TC_syscfg_memtype_hybrid_001)
{
    EXPECT_CALL(*mock, run(NotNull()))
        .Times(1);
    EXPECT_CALL(*mock, get_output())
        .WillOnce(Return(generate_scysfg_output(SyscfgValues::hybrid)));
    EXPECT_CALL(*mock, get_retcode())
        .WillOnce(Return(0));
    Syscfg syscfg(mock);
    MemType type;
    ASSERT_NO_THROW(type = syscfg.get_mem_type());
    ASSERT_EQ(MemType::hybrid, type);
}

TEST_F(SyscfgTest, TC_syscfg_memtype_unkown_001)
{
    EXPECT_CALL(*mock, run(NotNull()))
        .Times(1);
    EXPECT_CALL(*mock, get_output())
        .WillOnce(Return(generate_scysfg_output(SyscfgValues::auto_)));
    EXPECT_CALL(*mock, get_retcode())
        .WillOnce(Return(0));
    Syscfg syscfg(mock);
    ASSERT_THROW(syscfg.get_mem_type(), SystoolsdException);
}

TEST_F(SyscfgTest, TC_syscfg_memtype_throw_by_retcode_001)
{
    EXPECT_CALL(*mock, run(NotNull()))
        .Times(1);
    EXPECT_CALL(*mock, get_retcode())
        .WillOnce(Return(1));
    Syscfg syscfg(mock);
    ASSERT_THROW(syscfg.get_mem_type(), SystoolsdException);
}

TEST_F(SyscfgTest, TC_syscfg_seterrinject_001)
{
    EXPECT_CALL(*mock, run(NotNull()))
        .Times(2);
    EXPECT_CALL(*mock, get_retcode())
        .WillOnce(Return(0))
        .WillOnce(Return(0));
    Syscfg syscfg(mock);
    ASSERT_NO_THROW(syscfg.set_errinject(true, pass));
    ASSERT_NO_THROW(syscfg.set_errinject(false, pass));
}

TEST_F(SyscfgTest, TC_syscfg_seterrinject_throw_by_retcode_001)
{
    EXPECT_CALL(*mock, run(NotNull()))
        .Times(1);
    EXPECT_CALL(*mock, get_retcode())
        .WillOnce(Return(1));
    Syscfg syscfg(mock);
    ASSERT_THROW(syscfg.set_errinject(true, pass), SystoolsdException);
}

TEST_F(SyscfgTest, TC_syscfg_seteist_001)
{
    EXPECT_CALL(*mock, run(NotNull()))
        .Times(2);
    EXPECT_CALL(*mock, get_retcode())
        .WillOnce(Return(0))
        .WillOnce(Return(0));
    Syscfg syscfg(mock);
    ASSERT_NO_THROW(syscfg.set_eist(true, pass));
    ASSERT_NO_THROW(syscfg.set_eist(false, pass));
}

TEST_F(SyscfgTest, TC_syscfg_seteist_throw_by_retcode_001)
{
    EXPECT_CALL(*mock, run(NotNull()))
        .Times(1);
    EXPECT_CALL(*mock, get_retcode())
        .WillOnce(Return(1));
    Syscfg syscfg(mock);
    ASSERT_THROW(syscfg.set_eist(true, pass), SystoolsdException);
}

}//namespace
