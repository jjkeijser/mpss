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

#include "micmgmtCommon.hpp"
#include "SubcommandDef_p.hpp"

#include <stdexcept>


namespace micmgmt
{
    SubcommandDef::SubcommandDef()
    {
    }

    SubcommandDef::SubcommandDef(const std::string& subcommand, const std::vector<std::string> help)
        : nameHelp_(subcommand, help)
    {
    }

    void SubcommandDef::addArgument(const std::string& name, const std::vector<std::string>& help,
                                    EvaluateArgumentCallbackInterface* callback)
    {
        positionArguments_.push_back(ArgumentDef(name, EvaluateArgumentType::ePositionalArgument,
                                     help, true, "", callback));
    };

    ArgumentDefVector::size_type SubcommandDef::positionalArgumentSize() const
    {
        return positionArguments_.size();
    }

    ArgumentDefVector::iterator SubcommandDef::positionalArgumentBegin()
    {
        return positionArguments_.begin();
    };

    ArgumentDefVector::iterator SubcommandDef::positionalArgumentEnd()
    {
        return positionArguments_.end();
    };

    const ArgumentDef& SubcommandDef::positionalArgumentAt(ArgumentDefVector::size_type index) const
    {
        if (index >= positionArguments_.size())
        {
            throw std::out_of_range("ProgramError: SubcommandDef::positionalArgumentAt(ArgumentDefVector::size_type index): index out of range");
        }
        return positionArguments_.at(index);
    }

    const std::string& SubcommandDef::subcommandName() const
    {
        return nameHelp_.name();
    };

    const std::vector<std::string>& SubcommandDef::subcommandHelp() const
    {
        return nameHelp_.help();
    };

}; // namespace micmgmt
