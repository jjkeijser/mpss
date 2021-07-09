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

#include "InternalLogger.hpp"

#include "micmgmtCommon.hpp"

#include <stdexcept>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#endif

using namespace std;
using namespace micmgmt;

namespace
{
#ifdef UNIT_TESTS
    const int sMaxBackupCount = 5;
    const int sMaxLineCount = 18;
#else
    const int sMaxBackupCount = 5; // MUST be 1 or more
    const int sMaxLineCount = 1000; // SHOULD be 500 or more
#endif
    const string sLogfileExt = ".log";
    const string sLogFolderName = "logs";
    const string sFatalErrorMsg = "Logging failed fatally; disabling logging";

    bool makeFolder(const string& folderName)
    {
        if (folderName.empty() == true)
        {
            return false;
        }
#ifdef _WIN32
        return (CreateDirectoryA((LPCSTR)folderName.c_str(), NULL) == TRUE);
#else
        return (mkdir(folderName.c_str(), S_IRUSR | S_IWUSR | S_IXUSR) == 0);
#endif
    }

    bool renameFile(const string& oldFile, const string& newFile)
    {
        if (oldFile.empty() == true || newFile.empty() == true)
        {
            return false;
        }
        return (std::rename(oldFile.c_str(), newFile.c_str()) == 0);
    }

    bool deleteFile(const string& filename)
    {
        if (filename.empty() == true)
        {
            return false;
        }
        return (remove(filename.c_str()) == 0);
    }

    bool fileExists(const string& filename)
    {
        if (filename.empty() == true)
        {
            return false;
        }
        ifstream ifs(filename);
        return (ifs.fail() == false); // This correctly ignores the eofbit...
    }
} // Empty Namespace

namespace micmgmt
{
    InternalLogger::InternalLogger(const std::string& nameOnly) : disabled_(false)
    {
        baseFilename_ = userHomePath();
#ifdef UNIT_TESTS // required for the "docker" environment.
        if (baseFilename_.empty() == true)
        {
    #ifndef _WIN32
            baseFilename_ = "/tmp";
    #endif
        }
#endif
        if (baseFilename_.empty() == true)
        {
            throw std::runtime_error("Logging was requested but cannot be done due to user environment restrictions: internal reference #2");
        }
        if (baseFilename_[baseFilename_.size() - 1] != filePathSeparator()[0])
        {
            baseFilename_ += filePathSeparator();
        }
        baseFilename_ += sLogFolderName;
        if (pathIsFolder(baseFilename_) == false && makeFolder(baseFilename_) == false)
        {
            throw std::runtime_error("Logging was requested but cannot be done due to user environment restrictions: internal reference #3");
        }
        baseFilename_ += filePathSeparator() + nameOnly + sLogfileExt;
    }

    InternalLogger::~InternalLogger()
    {
    }

    const std::string& InternalLogger::getBaseFilename() const
    {
        return baseFilename_;
    }

    void InternalLogger::logLine(const std::string& line)
    {
        if (disabled_ == true)
        {
            return;
        }
        if (lineCount() > sMaxLineCount)
        {
            deleteAndShiftBackups();
        }
        lock_guard<mutex> cs(criticalSection_);
        ofstream ofs;
        if (fileExists(baseFilename_) == true)
        {
            ofs.open(baseFilename_, ios::out | ios::app);
        }
        else
        {
            ofs.open(baseFilename_, ios::out | ios::trunc);
        }
        if (ofs.good() == true)
        {
            ofs << line << endl;
            ofs.close();
        }
        else
        {
            cerr << sFatalErrorMsg << endl;
            disabled_ = true;
        }
    }

    // Implementation
    int InternalLogger::lineCount()
    {
        int rv = -1;
        static char slBuffer[4096];
        int count = 0;
        lock_guard<mutex> cs(criticalSection_);
        ifstream ifs(baseFilename_);
        while (ifs.good() == true && ifs.eof() == false)
        {
            slBuffer[0] = 0;
            ifs.getline(slBuffer, 4096);
            ++count;
        }
        if (ifs.eof() == true)
        {
            rv = count;
        }
        ifs.close();
        return rv;
    }

    void InternalLogger::deleteAndShiftBackups()
    {
        lock_guard<mutex> cs(criticalSection_);

        // Delete oldest backup file...
        if (fileExists(getBackupFilename(sMaxBackupCount)) == true)
        {
            if (deleteFile(getBackupFilename(sMaxBackupCount)) == false)
            {
                cerr << sFatalErrorMsg << endl;
                disabled_ = true;
            }
        }

        // Rotate backup files 1 to (n -1)...
        for (int n = (sMaxBackupCount - 1); n >= 1; --n)
        {
            if (fileExists(getBackupFilename(n)) == false)
            {
                continue;
            }
            if (renameFile(getBackupFilename(n), getBackupFilename(n + 1)) == false)
            {
                cerr << sFatalErrorMsg << endl;
                disabled_ = true;
            }
        }

        // Backup current log file to backup 1...
        if (renameFile(baseFilename_, getBackupFilename(1)) == false)
        {
            cerr << sFatalErrorMsg << endl;
            disabled_ = true;
        }
    }

    std::string InternalLogger::getBackupFilename(int n) const
    {
        if (n < 1 || n > sMaxBackupCount)
        {
            return string();
        }
        stringstream ss;
        ss << baseFilename_ << "." << n;
        return ss.str();
    }
} // namespace micmgmt
