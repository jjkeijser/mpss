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

#include <cstring>
#include <sstream>
#include <string>

#include "Popen.hpp"
#include "SystoolsdException.hpp"

#include "daemonlog.h"
#include "systoolsd_api.h"

Popen::Popen() :
    out(""), fpipe(NULL)
{
}

Popen::~Popen()
{
    if(fpipe)
    {
        pclose(fpipe);
        fpipe = NULL;
    }
}

std::string Popen::get_output()
{
    if(!fpipe)
        return out;
    const size_t size = 512;
    char buf[size] = {0};
    while(fgets(buf, size, fpipe) != NULL)
    {
        out += buf;
    }
    return out;
}

int Popen::get_retcode()
{
    //call get_output() so output is not lost when closing
    //fpipe
    if(!fpipe)
        return -1;
    (void)get_output();
    int ret = pclose(fpipe);
    fpipe = NULL;
    return WEXITSTATUS(ret);
}

void Popen::run(const char *cmd)
{
    //reset fpipe
    if(fpipe)
    {
        pclose(fpipe);
        errno = 0;
        fpipe = NULL;
    }

    fpipe = popen(cmd, "r");
    if(!fpipe)
    {
        throw SystoolsdException(SYSTOOLSD_IO_ERROR, "popen");
    }
    //restart output string
    out = "";
}

