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
// PROJECT INCLUDES
//
#include "SharedLibraryAdapter.hpp"

// CF INCLUDES
//
#include "SharedLibrary.hpp"

using namespace micmgmt;

SharedLibraryAdapter::SharedLibraryAdapter() : SharedLibrary()
{
    // Nothing to do
}

bool SharedLibraryAdapter::isLoaded() const
{
    return SharedLibrary::isLoaded();
}

std::string SharedLibraryAdapter::fileName() const
{
    return SharedLibrary::fileName();
}

std::string SharedLibraryAdapter::errorText() const
{
    return SharedLibrary::errorText();
}

std::string SharedLibraryAdapter::version() const
{
    return SharedLibrary::version();
}


bool SharedLibraryAdapter::unload()
{
    return SharedLibrary::unload();
}

bool SharedLibraryAdapter::load( int mode )
{
    return SharedLibrary::load(mode);
}

void* SharedLibraryAdapter::lookup( const std::string& symbol )
{
    return SharedLibrary::lookup(symbol);
}

void SharedLibraryAdapter::setFileName( const std::string& name )
{
    SharedLibrary::setFileName(name);
}

void SharedLibraryAdapter::setVersion( int major, int minor, int patch )
{
    SharedLibrary::setVersion(major, minor, patch);
}

