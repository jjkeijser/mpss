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

#ifndef SYSTOOLS_SYSTOOLSD_POPEN_HPP_
#define SYSTOOLS_SYSTOOLSD_POPEN_HPP_

#include <string>

#include "PopenInterface.hpp"

class Popen : public PopenInterface
{
public:
    Popen();
    virtual ~Popen();
    virtual void run(const char *cmd);
    virtual std::string get_output();
    virtual int get_retcode();

private:
    Popen(const Popen&);
    Popen &operator=(Popen&);
    std::string out;
    FILE *fpipe;
};

#endif //SYSTOOLS_SYSTOOLSD_POPEN_HPP_
