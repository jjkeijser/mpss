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

#ifndef MICMGMT_WORKITEMINTERFACE_HPP
#define MICMGMT_WORKITEMINTERFACE_HPP

#include "SafeBool.hpp"

namespace micmgmt
{
    class WorkItemInterface
    {
    public:
        virtual ~WorkItemInterface() {};
        virtual void Run(SafeBool& stopSignal) = 0;
    };
}; // namepsace micmgmt

#endif // MICMGMT_WORKITEMINTERFACE_HPP
