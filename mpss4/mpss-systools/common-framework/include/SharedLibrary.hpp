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

#ifndef MICMGMT_SHAREDLIBRARY_HPP
#define MICMGMT_SHAREDLIBRARY_HPP

//  SYSTEM INCLUDES
//
#include <memory>
#include <string>

//  NAMESPACE
//
namespace micmgmt
{


//----------------------------------------------------------------------------
//  CLASS:  SharedLibrary

class  SharedLibrary
{

public:

    enum  LoadMode  { eLoadNow, eLoadLazy };

public:

    SharedLibrary();
    SharedLibrary( const std::string& name );
   ~SharedLibrary();

    bool         isLoaded() const;
    std::string  fileName() const;
    std::string  errorText() const;
    std::string  version() const;

    bool         unload();
    bool         load( int mode=eLoadNow );
    void*        lookup( const std::string& symbol );
    void         setFileName( const std::string& name );
    void         setVersion( int major, int minor=-1, int patch=-1 );


private:

    struct  PrivData;
    std::unique_ptr<PrivData>  m_pData;


private:    // DISABLE

    SharedLibrary( const SharedLibrary& );
    SharedLibrary&  operator = ( const SharedLibrary& );

};


//---------------------------------------------------------------------------

}   // namespace micmgmt

#endif // MICMGMT_SHAREDLIBRARY_HPP
