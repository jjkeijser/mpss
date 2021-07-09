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
#ifndef MICMGMT_SHAREDLIBRARYADAPTER_HPP_
#define MICMGMT_SHAREDLIBRARYADAPTER_HPP_

// PROJECT INCLUDES
//
#include "LoaderInterface.hpp"

// CF INCLUDES
//
#include "SharedLibrary.hpp"

namespace micmgmt
{

// SharedLibraryAdapter class adapts common-framework's SharedLibrary class
// Required for injection of Loader objects into SCIF-user classes
class SharedLibraryAdapter : public LoaderInterface, private SharedLibrary
{
public:
    SharedLibraryAdapter();

    bool isLoaded() const;
    std::string fileName() const;
    std::string errorText() const;
    std::string version() const;

    bool unload();
    bool load( int mode=SharedLibrary::eLoadNow );
    void* lookup( const std::string& symbol );
    void setFileName( const std::string& name );
    void setVersion( int major, int minor=-1, int patch=-1 );
};

} // namespace micmgmt


#endif // MICMGMT_SHAREDLIBRARYADAPTER_HPP_
