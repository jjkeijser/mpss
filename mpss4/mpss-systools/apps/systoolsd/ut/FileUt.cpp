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

#include <fstream>
#include <string>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "FileInterface.hpp"
#include "SystoolsdException.hpp"

namespace
{
    const std::string filename("/tmp/.systoolsd_testfile");
    const std::string contents("These are the test file's contents");
    const size_t bufsize = 100;
    char buf[bufsize];
}

class FileTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        std::ofstream testfile;
        testfile.open(filename);
        ASSERT_TRUE(testfile.good());
        testfile << contents;
        testfile.close();
        bzero(buf, bufsize);
    }

    virtual void TearDown()
    {
        unlink(filename.c_str());
    }
};

TEST_F(FileTest, TC_read_001)
{
    File f;
    int fd;
    ASSERT_NE(-1, (fd = f.open(filename, O_RDONLY)));
    //use EXPECT_* to reach call to close()
    EXPECT_EQ(contents.length(), f.read(fd, buf, bufsize));
    EXPECT_EQ(std::string(buf), contents);
    f.close(fd);
}

TEST_F(FileTest, TC_read_throw_001)
{
    File f;
    int fd;
    ASSERT_NE(-1, (fd = f.open(filename, O_RDONLY)));
    EXPECT_THROW(f.read(fd, NULL, bufsize), std::invalid_argument);
    EXPECT_THROW(f.read(fd, buf, 0), std::invalid_argument);
    f.close(fd);
}

TEST_F(FileTest, TC_write_001)
{
    File f;
    int fd;
    const std::string to_write(". This is new data.");
    const std::string expected(contents + to_write);
    ASSERT_NE(-1, (fd = f.open(filename, O_RDWR)));
    //go to the end of the file
    lseek(fd, 0, SEEK_END);
    EXPECT_EQ(to_write.length(), f.write(fd, to_write.c_str(), to_write.length()));
    lseek(fd, 0, SEEK_SET);
    EXPECT_EQ(expected.length(), f.read(fd, buf, bufsize));
    EXPECT_EQ(expected, std::string(buf));
    f.close(fd);
}

TEST_F(FileTest, TC_write_002)
{
    File f;
    int fd;
    //4-digit number, expect 4 more bytes in file
    const std::string to_write_str("1234");
    const std::string expected(contents + to_write_str);
    std::stringstream ss(to_write_str);;
    int to_write;
    ss >> to_write;
    ASSERT_NE(-1, (fd = f.open(filename, O_RDWR)));
    //go to the end of the file
    lseek(fd, 0, SEEK_END);
    EXPECT_EQ(to_write_str.length(), f.write(fd, to_write));
    lseek(fd, 0, SEEK_SET);
    EXPECT_EQ(expected.length(), f.read(fd, buf, bufsize));
    EXPECT_EQ(expected, std::string(buf));
    f.close(fd);
}

TEST_F(FileTest, TC_rewind_001)
{
    File f;
    int fd;
    //read file and check contents
    ASSERT_NE(-1, (fd = f.open(filename, O_RDONLY)));
    EXPECT_EQ(contents.length(), f.read(fd, buf, bufsize));
    EXPECT_EQ(contents, std::string(buf));
    //call lseek
    bzero(buf, bufsize);
    EXPECT_EQ(0, f.lseek(fd, 0, SEEK_SET));
    EXPECT_EQ(contents.length(), f.read(fd, buf, bufsize));
    EXPECT_EQ(contents, std::string(buf));
    //call rewind
    bzero(buf, bufsize);
    EXPECT_NO_THROW(f.rewind(fd));
    EXPECT_EQ(contents.length(), f.read(fd, buf, bufsize));
    EXPECT_EQ(contents, std::string(buf));
    f.close(fd);
}

TEST_F(FileTest, TC_rewind_throw_001)
{
    File f;
    int fd;
    ASSERT_NE(-1, (fd = f.open(filename, O_RDONLY)));
    //close file and try to rewind it
    f.close(fd);
    EXPECT_THROW(f.rewind(fd), SystoolsdException);
}


