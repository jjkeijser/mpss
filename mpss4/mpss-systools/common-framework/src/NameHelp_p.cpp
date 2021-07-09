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
#include <algorithm>
#include "micmgmtCommon.hpp"
#include "NameHelp_p.hpp"

namespace micmgmt
{
    NameHelp::NameHelp()
    {
    }

    NameHelp::NameHelp(const std::string& name, const std::vector<std::string>& help)
        : name_(name), help_(help)
    {
        feTrim(name_);
        if (verifyCanonicalName(name_) == false)
        {
            throw std::invalid_argument("micmgmt::NameHelp::ctor: name: The name is not valid!");
        }
        if (name_.find_first_of("\n\r\f\v\t ") != std::string::npos)
        {
            throw std::invalid_argument("micmgmt::NameHelp::ctor: name: Trimmed string contains whitespace!");
        }
        if (help.size() == 0)
        {
            throw std::invalid_argument("micmgmt::NameHelp::ctor: help: Vector is empty!");
        }
        for_each(help_.begin(), help_.end(), feNormalize);
        if (help_.begin()->size() == 0)
        {
            throw std::invalid_argument("micmgmt::NameHelp::ctor: help: vector at index 0 is empty!");
        }
    }

    NameHelp::NameHelp(const std::string& name, const std::string& help)
        : name_(name)
    {
        feTrim(name_);
        if (name_.empty() == true)
        {
            throw std::invalid_argument("micmgmt::NameHelp::ctor: name: Trimmed name is empty!");
        }
        std::string localHelp = normalize(help);
        if (localHelp.empty() == true)
        {
            throw std::invalid_argument("micmgmt::NameHelp::ctor: help: Trimmed help is empty!");
        }
        help_.push_back(localHelp);
    };

    NameHelp::NameHelp(const NameHelp& copyFrom)
    {
        name_ = copyFrom.name_;
        help_ = copyFrom.help_;
    };

    const std::string& NameHelp::name() const
    {
        return name_;
    };

    const std::vector<std::string>& NameHelp::help() const
    {
        return help_;
    };

}; // namespace micmgmt
