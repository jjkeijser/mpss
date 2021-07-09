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

#ifndef MICMGMT_SUBCOMMANDDEF_HPP
#define MICMGMT_SUBCOMMANDDEF_HPP

#include <map>
#include "ArgumentDef_p.hpp"

namespace micmgmt
{
    class SubcommandDef
    {
    private:
        NameHelp                        nameHelp_;
        ArgumentDefVector               positionArguments_;

    public:
        SubcommandDef();
        SubcommandDef(const std::string& subcommand, const std::vector<std::string> help);

        void addArgument(const std::string& name, const std::vector<std::string>& help,
                         EvaluateArgumentCallbackInterface* callback);

        ArgumentDefVector::size_type positionalArgumentSize() const;
        ArgumentDefVector::iterator positionalArgumentBegin();
        ArgumentDefVector::iterator positionalArgumentEnd();
        const ArgumentDef& positionalArgumentAt(ArgumentDefVector::size_type index) const;

        const std::string& subcommandName() const;
        const std::vector<std::string>& subcommandHelp() const;
    }; // class SubcommandDef

    typedef std::map<std::string, SubcommandDef> SubcommandDefMap;

}; // namespace micmgmt
#endif // MICMGMT_SUBCOMMANDDEF_HPP
