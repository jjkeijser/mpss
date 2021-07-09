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

#ifndef SYSTOOLS_SYSTOOLSD_SYSTOOLSDEXCEPTION_HPP
#define SYSTOOLS_SYSTOOLSD_SYSTOOLSDEXCEPTION_HPP

#include <stdexcept>
#include <string>

#include "systoolsd_api.h"

class SystoolsdException : public std::runtime_error
{
public:
    SystoolsdException(SystoolsdError systoolsd_error, const char *errmsg) :
        std::runtime_error(errmsg), error(systoolsd_error) { }
    SystoolsdException(SystoolsdError systoolsd_error, std::string &errmsg) :
        std::runtime_error(errmsg), error(systoolsd_error) { }

    const char *error_type() const;
    uint16_t error_nr() const;

private:
    SystoolsdError error;
};

#endif //SYSTOOLS_SYSTOOLSD_SYSTOOLSDEXCEPTION_HPP
