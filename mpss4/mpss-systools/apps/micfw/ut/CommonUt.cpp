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

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#else
    #include <stdlib.h>
#endif // _WIN32
#include <cstring>
#include <cstdio>

#include "CommonUt.hpp"

using namespace std;
using namespace micmgmt;

//#define DEBUG_STATEMENT(x)
#define DEBUG_STATEMENT(x) x

namespace
{
    char* const gToolName_ = (char*)"micfw";
    const string gFlashFolderName_ = "micfw-ut-flash-testing";
    const string gEmptyFlashFolderName_ = "empty-folder";
    string gTempPath;

    vector<string> gKnlFlashFiles_ = {
        "GOLD_IMAGE.hddimg",
        "IFLASH_SHA_ERROR.hddimg",
        "BIOS_SHA_ERROR.hddimg",
        "SMC_SHA_ERROR.hddimg",
        "ME_SHA_ERROR.hddimg",
        "NSH_SHA_ERROR.hddimg",
        "IPMI_SHA_ERROR.hddimg",
        "TEMP_SHA_ERROR.hddimg",
        "CAPSULE_ERROR.hddimg",
        "VERSION_ERROR.hddimg",
        "ONLINE_ERROR.hddimg",
        "BIOS_FATAL_ERROR.hddimg",
        "BIOS_RACE_ERROR.hddimg",
        "BIOS_FLASH_ERROR.hddimg",
        "BIOS_VM_ERROR.hddimg",
        "SMC_FATAL_ERROR.hddimg",
        "SMC_RACE_ERROR.hddimg",
        "SMC_FLASH_ERROR.hddimg",
        "SMC_VM_ERROR.hddimg",
        "ME_FATAL_ERROR.hddimg",
        "ME_RACE_ERROR.hddimg",
        "ME_FLASH_ERROR.hddimg",
        "ME_VM_ERROR.hddimg",
        "FATAL_ERROR.hddimg",
        "EMPTY_ERROR.hddimg",
        "EXTRA_FILE.hddimg",
        "NO_BIOS_CAPSULE.hddimg",
        "NO_SMC_CAPSULE.hddimg",
        "NO_ME_CAPSULE.hddimg",
        "USING_A_VERY_LONG_NAME_FOR_A_FILE_IMAGE_IN_ORDER_TO_TEST_LONG_FILE_NAME_WITHOUT_A_CRASH.hddimg",
        "CAPSULE_EMPTY.hddimg",
        "TABLE_ERROR1.hddimg",
        "TABLE_ERROR2.hddimg",
        "TABLE_ERROR3.hddimg",
        "TABLE_ERROR4.hddimg",
        "TABLE_ERROR5.hddimg",
        "TABLE_ERROR6.hddimg",
        "TABLE_SWAP.hddimg"
    };
}

namespace micfw
{
    ArgV::ArgV(vector<string> commandLine)
    {
        argc_ = static_cast<int>(commandLine.size());
        argv_ = new char*[argc_ + 1];
        argv_[0] = gToolName_;
        for (int i = 0; i < argc_; ++i)
        {
            int size = static_cast<int>(commandLine[i].size());
            argv_[i + 1] = new char[size + 1];
            strncpy(argv_[i + 1], commandLine[i].c_str(), size);
            argv_[i + 1][size] = 0;
        }
    }

    ArgV::~ArgV()
    {
        for (int i = 0; i < argc_; ++i)
        {
            delete[] argv_[i + 1];
        }
        delete[] argv_;
    }

    int ArgV::argc() const
    {
        return (argc_ + 1);
    }

    char** ArgV::argv() const
    {
        return argv_;
    }

    ostringstream Utilities::sStream_;
    unique_ptr<pair<Application*, ArgV*>> Utilities::sCurrentPair_;
    string Utilities::sFlashFolder_;
    string Utilities::sEmptyFlashFolder_;
    vector<string> Utilities::sKnlFlashFiles_;

    bool Utilities::generatePaths()
    {
#ifdef _WIN32
        char *path=NULL;
        if ((path = _tempnam("c:\\", "temp-file")) == NULL)
#else
        char path[L_tmpnam];
        if (tmpnam(path) == NULL)
#endif
            return true;
        gTempPath = std::string(path);
        return false;
    }

    bool Utilities::setEnvVar(const string& name, const string& value)
    {
        if (name.empty() == true)
        {
            return false;
        }
#ifdef _WIN32
        BOOL rv = FALSE;
        if(value.empty() == true)
        {
            rv = SetEnvironmentVariableA((LPCSTR)name.c_str(), NULL);
        }
        else
        {
            rv = SetEnvironmentVariableA((LPCSTR)name.c_str(), (LPCSTR)value.c_str());
        }
        return (rv == TRUE);
#else
        int rv = -1;
        if(value.empty() == true)
        {
            rv = unsetenv(name.c_str());
        }
        else
        {
            rv = setenv(name.c_str(), value.c_str(), 1);
        }
        return (rv == 0);
#endif // _WIN32
    }

    bool Utilities::setKnlDeviceCount(int count)
    {
        string value = "";
        if (count > 0)
        {
            value = to_string((long long)count);
            setEnvVar( "MICMGMT_MPSS_MOCK_VERSION", "4.1.2.3" );  // Use a KNL compatible stack
        }

        return setEnvVar("KNL_MOCK_CNT", value);
    }

    pair<Application*,ArgV*>* Utilities::createApplication(std::vector<std::string> commandLine)
    {
        ArgV* argv = new ArgV(commandLine);
        Application* app = new Application(new CliParser(argv->argc(), argv->argv(), "1.0.1", "2014", "micfw", "UT micfw description."));
        return new pair<Application*, ArgV*>(app,argv);
    }

    bool Utilities::compareWithMask(const std::string& output, const std::string& mask)
    {
        if (mask.empty() == true)
        {
            return (output == mask);
        }
        // split on "*" then check each part.  Match if all marts match.
        vector<string> parts;
        size_t star = mask.find('*');
        size_t last = 0;
        if (star > 0)
        {
            parts.push_back(mask.substr(0, star));
        }
        while (star != string::npos)
        {
            size_t start = star + 1;
            size_t end = mask.find('*', start);
            if (end != string::npos)
            {
                --end;
            }
            string p = mask.substr(start, (end - start + 1));
            last = start;
            if (p.empty() == false)
            {
                parts.push_back(p);
            }
            star = mask.find('*', last);
        }
        if (parts.size() == 0)
        {
            parts.push_back(mask);
        }
        size_t count = 0;
        star = 0;
        for (auto it = parts.begin(); it != parts.end(); ++it)
        {
            string f = *it;
            star = output.find(f, star);
            if (star != string::npos)
            {
                ++count;
                star += f.length();
            }
            else
            {
                break;
            }
        }
        return (count == parts.size());
    }

    bool Utilities::fileExists(const std::string& file)
    {
        ifstream strm(file);
        return strm.good();
    }

    bool Utilities::deleteFile(const std::string& file)
    {
        if (fileExists(file) == true)
        {
            return (remove(file.c_str()) == 0) ? true : false;
        }
        return false;
    }

    void Utilities::mkdir(const std::string& folder)
    {
#ifdef _WIN32
        if (CreateDirectoryA((LPCSTR)folder.c_str(), NULL) != TRUE)
        {
            DEBUG_STATEMENT(cout << "Debug: Utilities::mkdir: Failed to create folder: " << folder << endl);
        }
#else
        struct stat st;

        if (stat(folder.c_str(), &st) == -1)
        {
            if(::mkdir(folder.c_str(), 0750) != 0)
            {
                DEBUG_STATEMENT(cout << "Debug: Utilities::mkdir: Failed to create folder: " << folder << endl);
            }
        }
#endif
    }

    void Utilities::rmdir(const std::string& folder)
    {
#ifdef _WIN32
        if (RemoveDirectoryA((LPCSTR)folder.c_str()) != TRUE)
        {
            DEBUG_STATEMENT(cout << "Debug: Utilities::rmdir: Failed to delete folder: " << folder << endl);
        }
#else
        struct stat st;

        if (stat(folder.c_str(), &st) != -1)
        {
            if(::rmdir(folder.c_str()) != 0)
            {
                DEBUG_STATEMENT(cout << "Debug: Utilities::rmdir: Failed to delete folder: " << folder << endl);
            }
        }
#endif
    }

    void Utilities::generateFlashFolderPathAndFiles()
    {
        if (sFlashFolder_.empty() == true)
        {
            std::string sep = micmgmt::filePathSeparator();
            generatePaths();
            mkdir(gTempPath);
            std::string folder = gTempPath;
            if (folder[folder.size() - 1] != sep[0])
            {
                folder += sep;
            }
            folder += gFlashFolderName_;
            sFlashFolder_ = folder;
            sEmptyFlashFolder_ = folder + sep + gEmptyFlashFolderName_;

            sKnlFlashFiles_.clear();

            folder += sep;
            string extra = "";

            for (auto it = gKnlFlashFiles_.begin(); it != gKnlFlashFiles_.end(); ++it)
            {
                string full = folder + extra + *it;
                sKnlFlashFiles_.push_back(full);
            }
        }
    }

    void Utilities::createKnlFlashFolder()
    {
        generateFlashFolderPathAndFiles();

        mkdir(sFlashFolder_);
        mkdir(sEmptyFlashFolder_);

        for (auto it = sKnlFlashFiles_.begin(); it != sKnlFlashFiles_.end(); ++it)
        {
            ofstream ostrm(*it, ios_base::trunc | ios_base::out);
            if (ostrm.good() == true)
            {
                ostrm << "micfw-ut: KNL: Dummy flash file." << endl;
                ostrm.close();
            }
            else
            {
                DEBUG_STATEMENT(cout << "Debug: Utilities::createKnlFlashFolder: failed to create file: " << *it << endl);
            }
        }
    }

    void Utilities::destroyKnlFlashFolder()
    {
        for (auto it = sKnlFlashFiles_.begin(); it != sKnlFlashFiles_.end(); ++it)
        {
            if (deleteFile(*it) == false)
            {
                DEBUG_STATEMENT(cout << "Debug: Utilities::destroyKnlFlashFolder: failed to delete file: " << *it << endl);
            }
        }
        rmdir(sEmptyFlashFolder_);
        string localFolder = sFlashFolder_;
        if (localFolder[localFolder.size() - 1] == filePathSeparator()[0])
        {
            localFolder.erase(localFolder.size() - 1);
        }
        rmdir(localFolder);
    }
    Utilities::~Utilities()
    {
        rmdir(gTempPath);
    }

    std::ostringstream& Utilities::stream()
    {
        return sStream_;
    }

    std::pair<Application*, ArgV*>& Utilities::knlTestSetup(const std::vector<std::string>& cmdline, int knlCount, bool emptyFolder)
    {
        setKnlDeviceCount(knlCount);
        createKnlFlashFolder();

        sCurrentPair_ = unique_ptr<pair<Application*, ArgV*>>(Utilities::createApplication(cmdline));
        if(emptyFolder)
            sCurrentPair_->first->setInstallFolder(sEmptyFlashFolder_);
        else
            sCurrentPair_->first->setInstallFolder(sFlashFolder_);
        sCurrentPair_->first->redirectOutput(sStream_);
        sStream_.str("");

        return *(sCurrentPair_.get());
    }

    void Utilities::knlTestTearDown()
    {
        destroyKnlFlashFolder();

        delete sCurrentPair_->first;
        delete sCurrentPair_->second;
        sCurrentPair_.reset();
    }

    void Utilities::knlTestTearDown(const std::string& a, const std::string& b,
                                    const std::string& c, const std::string& d)
    {
        Utilities::deleteFile(a);
        Utilities::deleteFile(b);
        Utilities::deleteFile(c);
        Utilities::deleteFile(d);
        destroyKnlFlashFolder();

        delete sCurrentPair_->first;
        delete sCurrentPair_->second;
        sCurrentPair_.reset();
    }

    std::string Utilities::knlFile(int index)
    {
        if (index >= 0 && index < (int)sKnlFlashFiles_.size())
        {
            return sKnlFlashFiles_[index];
        }
        else
        {
            DEBUG_STATEMENT(cout << "Debug: Utilities::knlFile: index out of range: " << index << endl);
            return string();
        }
    }

    std::string Utilities::flashFolder()
    {
        if (sFlashFolder_ == "")
        {
            generateFlashFolderPathAndFiles();
        }
        return sFlashFolder_;
    }
}; // namespace micfw
