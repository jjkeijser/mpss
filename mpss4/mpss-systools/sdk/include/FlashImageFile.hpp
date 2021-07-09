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

#ifndef MICMGMT_FLASHIMAGEFILE_HPP
#define MICMGMT_FLASHIMAGEFILE_HPP

//  PROJECT INCLUDES
//
#include    "FlashImage.hpp"

// NAMESPACE
//
namespace  micmgmt {

// FORWARD DECLARATIONS
//
class  FlashImageInfo;


//----------------------------------------------------------------------------
//  CLASS:  FlashImageFile

class  FlashImageFile : public FlashImage
{

public:

    FlashImageFile();
    FlashImageFile( const std::string& path );
   ~FlashImageFile();

    std::string  pathName() const;

    void         setPathName( const std::string& path );
    uint32_t     load();


private:

    std::string  m_path;


private: // DISABLE

    FlashImageFile( const FlashImageFile& );
    FlashImageFile&  operator = ( const FlashImageFile& );

};

//----------------------------------------------------------------------------

}   // namespace micmgmt

#endif // MICMGMT_FLASHIMAGEFILE_HPP
