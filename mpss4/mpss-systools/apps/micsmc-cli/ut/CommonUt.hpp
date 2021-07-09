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

#ifndef MICSMCCLI_COMMONUT_HPP
#define MICSMCCLI_COMMONUT_HPP

// Google Test Include
#include <gtest/gtest.h>

// C++ Includes
#include <vector>
#include <memory>
#include <utility>
#include <ostream>

namespace micmgmt
{
    class MicDevice;
    class MicDeviceImpl;
}; // namespace micmgmt

namespace micsmccli
{
    class CliApplication;

    class ArgV
    {
    private:
        char** argv_;
        int    argc_;

    public:
        ArgV(std::vector<std::string> commandLine);
        ~ArgV();

        int argc() const;
        char** argv() const;
    };

    std::vector<micmgmt::MicDeviceImpl*> createDeviceImplVector(size_t count, int type, bool online = true, bool admin = false);

    std::shared_ptr<CliApplication> createApplicationInstance(int type, int cardCount, std::ostream& stream, std::vector<std::string> args);

    void clearTestEnvironment();

    bool compareWithMask(const std::string& output, const std::string& mask);
}; // namespace micsmccli
#endif // MICSMCCLI_COMMONUT_HPP
