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
#include <string>
#include <vector>
#include <memory>
#include <utility>
#include <fstream>
#include <sstream>

// CF
#include "CliParser.hpp"
#include "micmgmtCommon.hpp"

// App
#include "Application.hpp"

namespace micfw
{
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

    class Utilities
    {
    private:
        static bool setEnvVar(const std::string& name, const std::string& value = "");

    public:
        ~Utilities();
        static bool setKnlDeviceCount(int count = 0);

        static std::pair<Application*,ArgV*>* createApplication(std::vector<std::string> commandLine);

        static bool compareWithMask(const std::string& output, const std::string& mask);

        static bool fileExists(const std::string& file);
        static bool deleteFile(const std::string& file);
        static void mkdir(const std::string& folder);
        static void rmdir(const std::string& folder);

        static void generateFlashFolderPathAndFiles();
        static bool generatePaths();

        static void createKnlFlashFolder();
        static void destroyKnlFlashFolder();

        static std::ostringstream& stream();
        static std::pair<Application*, ArgV*>& knlTestSetup(const std::vector<std::string>& cmdline,
                                                            int knlCount = 4, bool emptyFolder = false);
        static void knlTestTearDown();
        static void knlTestTearDown(const std::string& a, const std::string& b,
                                    const std::string& c, const std::string& d);
        static std::string knlFile(int index);
        static std::string flashFolder();


    private: // Fields
        static std::ostringstream sStream_;
        static std::unique_ptr<std::pair<Application*, ArgV*>> sCurrentPair_;
        static std::string sFlashFolder_;
        static std::string sEmptyFlashFolder_;
        static std::vector<std::string> sKnlFlashFiles_;

    }; // class Utilities

}; // namespace micfw
