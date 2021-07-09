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
#ifndef MICMGMT_DIRENTFUNCTIONS_HPP_
#define MICMGMT_DIRENTFUNCTIONS_HPP_

#include <dirent.h>

namespace micmgmt
{

class DirentFunctions
{
public:
    DIR* opendir(const char* name)
    {
        return ::opendir(name);
    }

    struct dirent*  readdir(DIR* dirp)
    {
        return ::readdir(dirp);
    }

    int closedir(DIR* dirp)
    {
        return ::closedir(dirp);
    }
};

} // namespace micmgmt

#endif // MICMGMT_DIRENTFUNCTIONS_HPP_

