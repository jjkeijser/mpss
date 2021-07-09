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

#ifndef MICMGMT_TOOLSETTINGS_HPP
#define MICMGMT_TOOLSETTINGS_HPP

#include "micmgmtCommon.hpp"

#include <fstream>
#include <string>
#include <unordered_map>

#ifdef UNIT_TESTS
#define PRIVATE public
#else
#define PRIVATE private
#endif

namespace micmgmt
{
    class ToolSettings
    {
    public:
        explicit ToolSettings();
        ~ToolSettings();

        std::string getValue(const std::string& valueName, const std::string& sectionName = "") const;
        bool isLoaded() const;

        static std::string getSettingFilename();

    private: // Implementation
        bool loadSettings();

    PRIVATE: // Fields
        std::unordered_map<std::string,std::string> data_;
        bool loaded_;
    };
} // namespace micmgmt

#endif // MICMGMT_TOOLSETTINGS_HPP
