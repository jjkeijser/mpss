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

#ifndef MICMGMT_OPTIONDEF_HPP
#define MICMGMT_OPTIONDEF_HPP

#include <memory>
#include <map>
#include "ArgumentDef_p.hpp"

namespace micmgmt
{
    class OptionDef
    {
    protected:
        char                         shortLetter_;
        NameHelp                     optionNameHelp_;
        std::shared_ptr<ArgumentDef> argumentDef_;

    public:
        OptionDef(); // default constructor
        OptionDef(const std::string& optionLongName, const std::vector<std::string>& optionHelp,
                  const char& shortLetter, const bool& argumentRequired,
                  const std::string& argumentName, const std::vector<std::string>& argumentHelp,
                  const std::string& defVal, EvaluateArgumentCallbackInterface* callback);

        const std::string& optionName() const;

        const std::vector<std::string>& optionHelp() const;

        const std::shared_ptr<ArgumentDef>& argumentDef() const;

        char shortName() const;

        bool hasArgument() const;

        bool checkArgumentValue(const std::string& value) const;
    }; // class OptionDef

    typedef std::map<std::string, OptionDef> OptionDefMap;

}; // namespace micmgmt

#endif // MICMGMT_OPTIONDEF_HPP
