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
#include "ArgumentDef_p.hpp"
#include "EvaluateArgumentCallbackInterface.hpp"

namespace micmgmt
{
    ArgumentDef::ArgumentDef(std::string name, EvaluateArgumentType type, std::vector<std::string> help, bool required,
                const std::string& defVal, EvaluateArgumentCallbackInterface* callback)
                : nameHelp_(name, help), argumentType_(type), required_(required), defaultValue_(defVal),
                  callback_(callback)
    {
        feTrim(defaultValue_);
    };

    const std::string& ArgumentDef::name() const
    {
        return nameHelp_.name();
    };

    const std::vector<std::string>& ArgumentDef::help() const
    {
        return nameHelp_.help();
    };

    bool ArgumentDef::isRequired() const
    {
        return required_;
    };

    const std::string& ArgumentDef::defaultValue() const
    {
        return defaultValue_;
    };

    bool ArgumentDef::checkArgumentValue(const std::string& value, std::string optionOrSubcommandName) const
    {
        if (callback_ == NULL)
        {
            return true;
        }
        else
        {
            return callback_->checkArgumentValue(argumentType_, nameHelp_.name(), optionOrSubcommandName, value);
        }
    };

    EvaluateArgumentType ArgumentDef::type() const
    {
        return argumentType_;
    }
}; // namespace micmgmt
