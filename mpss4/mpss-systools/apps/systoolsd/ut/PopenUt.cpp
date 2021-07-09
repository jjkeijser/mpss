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

#include <string>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "Popen.hpp"
#include "SystoolsdException.hpp"

TEST(PopenTest, TC_run_001)
{
    Popen p;
    //check empty string and non-zero return code when nothing has been executed
    ASSERT_EQ(std::string(""), p.get_output());
    ASSERT_NE(0, p.get_retcode());

    ASSERT_NO_THROW(p.run("ls /"));
    ASSERT_NE(std::string(""), p.get_output());
    ASSERT_EQ(0, p.get_retcode());

    ASSERT_NO_THROW(p.run("ls /tmp/this_directory_better_not_exist"));
    ASSERT_NE(0, p.get_retcode());
}
