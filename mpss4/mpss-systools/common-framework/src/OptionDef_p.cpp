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
#include "micmgmtCommon.hpp"
#include "OptionDef_p.hpp"

namespace micmgmt
{
    OptionDef::OptionDef()
    {
    };

    OptionDef::OptionDef(const std::string& optionLongName, const std::vector<std::string>& optionHelp,
              const char& shortLetter, const bool& argumentRequired,
              const std::string& argumentName, const std::vector<std::string>& argumentHelp,
              const std::string& defVal, EvaluateArgumentCallbackInterface* callback)
              : shortLetter_(shortLetter), optionNameHelp_(optionLongName, optionHelp)
    {
        std::string localArgName = trim(argumentName);
        if (localArgName.empty() == false)
        {
            argumentDef_ = std::make_shared<ArgumentDef>(localArgName, EvaluateArgumentType::eOptionArgument,
                                                         argumentHelp, argumentRequired, defVal, callback);
        }
        else if (argumentRequired == true)
        {
            throw std::invalid_argument("micmgmt::OptionDef::ctor: argumentRequired/argumentName: argument name missing when the argument is required!");
        }
    };

    const std::string& OptionDef::optionName() const
    {
        return optionNameHelp_.name();
    };

    const std::vector<std::string>& OptionDef::optionHelp() const
    {
        return optionNameHelp_.help();
    };

    const std::shared_ptr<ArgumentDef>& OptionDef::argumentDef() const
    {
        return argumentDef_;
    };

    char OptionDef::shortName() const
    {
        return shortLetter_;
    };

    bool OptionDef::hasArgument() const
    {
        return (bool)argumentDef_;
    };

    bool OptionDef::checkArgumentValue(const std::string& value) const
    {
        if (hasArgument() == true)
        {
            return argumentDef_->checkArgumentValue(value, optionNameHelp_.name());
        }
        else
        {
            return false;
        }
    };

}; // namespace micmgmt
