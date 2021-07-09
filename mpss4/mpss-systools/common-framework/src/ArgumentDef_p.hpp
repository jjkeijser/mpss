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

#ifndef MICMGMT_ARGUMENTDEF_HPP
#define MICMGMT_ARGUMENTDEF_HPP

#include "NameHelp_p.hpp"
#include "EvaluateArgumentCallbackInterface.hpp"

namespace micmgmt
{
    class ArgumentDef
    {
    protected:
        NameHelp                           nameHelp_;
        EvaluateArgumentType               argumentType_;
        bool                               required_;
        std::string                        defaultValue_;
        EvaluateArgumentCallbackInterface* callback_;

    public:
        ArgumentDef(std::string name, EvaluateArgumentType type, std::vector<std::string> help, bool required,
                    const std::string& defVal, EvaluateArgumentCallbackInterface* callback = NULL);

        const std::string& name() const;
        const std::vector<std::string>& help() const;
        bool isRequired() const;
        const std::string& defaultValue() const;
        EvaluateArgumentType type() const;
        bool checkArgumentValue(const std::string& value, std::string optionOrSubcommandName = "") const;
    }; // class ArgumentDef

    typedef std::vector<ArgumentDef> ArgumentDefVector;

}; // namespace micmgmt
#endif // MICMGMT_ARGUMENTDEF_HPP
