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
#ifndef MICMGMT_LOADERINTERFACE_HPP_
#define MICMGMT_LOADERINTERFACE_HPP_

// SYSTEM INCLUDES
//
#include <string>

namespace micmgmt
{

class LoaderInterface
{
public:
    virtual ~LoaderInterface() { }
    virtual bool         isLoaded() const = 0;
    virtual std::string  fileName() const = 0;
    virtual std::string  errorText() const = 0;
    virtual std::string  version() const = 0;

    virtual bool         unload() = 0;
    virtual bool         load( int mode=0) = 0;
    virtual void*        lookup( const std::string& symbol ) = 0;
    virtual void         setFileName( const std::string& name ) = 0;
    virtual void         setVersion( int major, int minor=-1, int patch=-1 ) = 0;
};

} // namespace micmgmt

#endif // MICMGMT_LOADERINTERFACE_HPP_
