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

#include "ToolSettings.hpp"
#include "MicLogger.hpp"

using namespace std;
using namespace micmgmt;

namespace
{
    // Filenaem of settings file.
    const string sFilename = "systools.conf";

    std::string getConfigPath()
    {
#ifdef UNIT_TESTS
        return sFilename; // Default for UT
#else
    #ifdef _WIN32
        string file = micmgmt::mpssInstallFolder();// Default Windows install location
        if(file.empty() != true && file[file.size() -1] != micmgmt::filePathSeparator()[0])
        {
            file += micmgmt::filePathSeparator();
        }
        return file + sFilename; 
    #else
        return std::string("/etc/mpss/" + sFilename); // Default Linux location for MPSS
    #endif
#endif
    }

    int sMaxLineLength = 4095;
    char sBuffer[4096];
} // empty namespace

namespace micmgmt
{
    ToolSettings::ToolSettings() : loaded_(false)
    {
        loaded_ = loadSettings();
    }

    ToolSettings::~ToolSettings()
    {
    }

    std::string ToolSettings::getSettingFilename()
    {
        return sFilename;
    }

    std::string ToolSettings::getValue(const std::string& valueName, const std::string& sectionName) const
    {
        string key;
        if (sectionName.empty() == false)
        {
            key = sectionName + ".";
        }
        key += valueName;
        if (data_.find(key) != data_.end())
        {
            return data_.at(key);
        }
        else
        {
            return string();
        }
    }

    bool ToolSettings::isLoaded() const
    {
        return loaded_;
    }

    bool ToolSettings::loadSettings()
    {
        string section;
        string line;
        ifstream strm(getConfigPath());
        if (strm.good() == true)
        {
            while (strm.eof() == false)
            {
                line.clear();
                sBuffer[0] = 0;
                strm.getline(sBuffer, sMaxLineLength);
                if (strm.fail() == false)
                {
                    line = sBuffer;
                    feTrim(line);
                    if (line.empty() == false && line[0] != '[' && line[0] != '#')
                    { // Not a comment, blank line, or new section...
                        string::size_type pos = line.find('=');
                        if (pos == 0)
                        { // Ignore malformed line...
                            LOG(DEBUG_LVL, "Malformed settings line encountered: '" + line + "'");
                            continue;
                        }
                        string name = line.substr(0, pos);
                        string value = line.substr(pos + 1);
                        feTrim(name);
                        feTrim(value);
                        pos = value.find_last_of("\"\'#");
                        while (pos != string::npos && value[pos] == '#')
                        { // has trailing comment...
                            value.erase(pos);
                            feTrim(value);
                            pos = value.find_last_of("\"\'#");
                        }
                        if (section.empty() == false)
                        { // build key for data value if in a section...
                            name = section + "." + name;
                        }
                        if (value.empty() == false && value.length() > 1 && (value[0] == '\'' || value[0] == '\"'))
                        { // Value in quotes...
                            if (value[value.length() - 1] == value[0])
                            {
                                value = value.substr(1, value.length() - 2);
                            }
                            // Don't trim what is in quotes!
                        }
                        data_[name] = value;
                    }
                    else
                    {
                        if (line.empty() == false && line.length() > 2 && line[0] == '[')
                        { // Possible new section...
                            string::size_type pos = line.find_last_of("]#");
                            while (pos != string::npos && line[pos] == '#')
                            { // has trailing comment...
                                line.erase(pos);
                                feTrim(line);
                                pos = line.find_last_of("]#");
                            }
                            if (line[line.length() - 1] == ']')
                            {
                                section = line.substr(1, line.length() - 2);
                                feTrim(section);
                            }
                        }
                    }
                }
            }
            strm.close();
            return true;
        }
        else
        {
            return false;
        }
    }
} // namespace micmgmt
