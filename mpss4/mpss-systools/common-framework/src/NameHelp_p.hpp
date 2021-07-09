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

#ifndef MICMGMT_NAMEHELP_HPP
#define MICMGMT_NAMEHELP_HPP

#include <string>
#include <vector>

namespace micmgmt
{
    class NameHelp
    {
    private:
        std::string              name_;
        std::vector<std::string> help_;

    public:
        NameHelp();
        NameHelp(const std::string& name, const std::vector<std::string>& help);
        NameHelp(const std::string& name, const std::string& help);
        NameHelp(const NameHelp& copyFrom);

        const std::string& name() const;
        const std::vector<std::string>& help() const;
    }; // class NameHelp

}; // namespace micmgmt

#endif // MICMGMT_NAMEHELP_HPP
