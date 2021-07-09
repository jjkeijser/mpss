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
#include <stdexcept>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "KnlDevice.hpp"
#include "KnlCustomBootManager.hpp"
#include "Filesystem.hpp"
#include "MicDeviceError.hpp"

#include "mocks.hpp"

using ::testing::_;
using ::testing::Return;

namespace
{
    const size_t bufsize = 80;
    const std::string systools_firmware_dir = "systools_flash";
    const std::string realpathValue("/tmp/somefile");
    const std::string basenameValue("somefile");

    const std::string mic0 = "mic0";
    const std::string expectedFwRelPath = "systools_flash/somefile_mic0";
    const std::string expectedFwAbsPath = "/lib/firmware/systools_flash/somefile_mic0";
}

namespace micmgmt
{
    class KnlCustomBootManagerTest : public ::testing::Test
    {
    protected:
        // Our KnlCustomBootManagerT for test:
        typedef KnlCustomBootManagerT<MockFilesystem> BootMgr;

        // Our char*'s to be returned by realpath, basename
        char realpathBuf[bufsize];
        char basenameBuf[bufsize];

        // Our MockFilesystem-returning-and-configuring function
        MockFilesystem *getFilesystem()
        {
            auto mockFs = new MockFilesystem;
            std::memset(realpathBuf, 0, bufsize);
            std::memset(basenameBuf, 0, bufsize);
            std::memcpy(realpathBuf, realpathValue.c_str(), realpathValue.size());
            std::memcpy(basenameBuf, basenameValue.c_str(), basenameValue.size());

            // Default actions:
            ON_CALL(*mockFs, mkdir(_, _))
                .WillByDefault(Return(0));
            ON_CALL(*mockFs, symlink(_, _))
                .WillByDefault(Return(0));
            ON_CALL(*mockFs, realpath(_, _))
                .WillByDefault(Return(realpathBuf));
            ON_CALL(*mockFs, basename(_))
                .WillByDefault(Return(basenameBuf));
            ON_CALL(*mockFs, rmdir(_))
                .WillByDefault(Return(0));
            ON_CALL(*mockFs, unlink(_))
                .WillByDefault(Return(0));

            return mockFs;
        }

        virtual void SetUp()
        {
            std::memset(realpathBuf, 0, bufsize);
            std::memset(basenameBuf, 0, bufsize);
            std::memcpy(realpathBuf, realpathValue.c_str(), realpathValue.size());
            std::memcpy(basenameBuf, basenameValue.c_str(), basenameValue.size());
        }
    };

    TEST_F(KnlCustomBootManagerTest, TC_ctor_001)
    {
        CustomBootManager *mgr;
        ASSERT_NO_THROW(
            mgr = new BootMgr ( getFilesystem(),
                                mic0,
                                basenameValue )
        );
        ASSERT_EQ(expectedFwRelPath,
                  mgr->fwRelPath());
        ASSERT_EQ(expectedFwAbsPath,
                  mgr->fwAbsPath());
        delete mgr;

        BootMgr mgr2( getFilesystem(), mic0, basenameValue );
        ASSERT_EQ(expectedFwRelPath,
                  mgr2.fwRelPath());
        ASSERT_EQ(expectedFwAbsPath,
                  mgr2.fwAbsPath());

        auto mgr3 = std::move(mgr2);
        ASSERT_EQ(expectedFwRelPath,
                  mgr3.fwRelPath());
        ASSERT_EQ(expectedFwAbsPath,
                  mgr3.fwAbsPath());
    }


    TEST_F(KnlCustomBootManagerTest, TC_ctor_realpath_fail_001)
    {
        auto mockFs = getFilesystem();
        EXPECT_CALL(*mockFs, realpath(_, _))
            .WillOnce(Return(nullptr)); // Oh snap
        ASSERT_THROW(
            BootMgr( mockFs,
                     mic0,
                     basenameValue ),
            std::runtime_error
        );
    }

    TEST_F(KnlCustomBootManagerTest, TC_ctor_fail_excp_cleanup_001)
    {
        // Have a function in createDirectory() fail, so that an exception
        // is thrown and caught in the ctor, and make sure the cleanUp()
        // function is called.
        auto mockFs = getFilesystem();
        errno = EACCES;
        EXPECT_CALL(*mockFs, mkdir(_, _))
            .WillOnce(Return(-1));
        ASSERT_THROW(
            BootMgr( mockFs,
                     mic0,
                     basenameValue ),
            std::runtime_error
        );
        errno = 0;
    }

    TEST_F(KnlCustomBootManagerTest, TC_ctor_symlink_fail_001)
    {
        auto mockFs = getFilesystem();
        EXPECT_CALL(*mockFs, symlink(_, _))
            .WillOnce(Return(-1));
        ASSERT_THROW(
            BootMgr( mockFs,
                     mic0,
                     basenameValue ),
            std::runtime_error
        );
    }

    TEST_F(KnlCustomBootManagerTest, TC_ctor_cleanup_fail_001)
    {
        // Have first call to unlink succeed, second one for clean up fail
        auto mockFs = getFilesystem();
        EXPECT_CALL(*mockFs, unlink(_))
            .WillOnce(Return(0))
            .WillOnce(Return(-1));
        // Make sure this doesn't throw, since cleanUp() is called from destructor
        ASSERT_NO_THROW(
            BootMgr( mockFs,
                     mic0,
                     basenameValue )
        );

        // Now have rmdir fail for reason != ENOTEMPTY
        auto mockFs2 = getFilesystem();
        EXPECT_CALL(*mockFs2, rmdir(_))
            .WillOnce(Return(-1));
        errno = EACCES;
        ASSERT_NO_THROW(
            BootMgr( mockFs2,
                     mic0,
                     basenameValue )
        );

        // Now have rmdir fail for reason = ENOTEMPTY
        auto mockFs3 = getFilesystem();
        EXPECT_CALL(*mockFs3, rmdir(_))
            .WillOnce(Return(-1));
        errno = ENOTEMPTY;
        ASSERT_NO_THROW(
            BootMgr( mockFs3,
                     mic0,
                     basenameValue )
        );

        errno = 0; // Better not to mess with this :o
    }

} // namespace micmgmt
