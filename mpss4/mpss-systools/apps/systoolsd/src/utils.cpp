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

#include <stdexcept>
#include <string>

#include "utils.hpp"

namespace utils
{

uint64_t get_u64_from_line(const std::string &line)
{
    const char numbers[] = "0123456789";
    std::string svalue;
    uint64_t value = -1;
    //Find idx where numeric chars start
    std::size_t idx = line.find_first_of(numbers);
    if(idx == std::string::npos)
    {
        throw std::logic_error("no number to extract in line");
    }

    svalue = line.substr(idx);
    //Find idx where numeric chars end
    idx = svalue.find_first_not_of(numbers);
    if(idx == std::string::npos)
    {
        //line does not specify units
        return atoi(svalue.c_str());
    }

    value = atoi(svalue.erase(idx).c_str());
    return value;
}

uint64_t get_u64_from_line(const char *line)
{
    std::string l(line);
    return get_u64_from_line(l);
}

} //namespace utils
