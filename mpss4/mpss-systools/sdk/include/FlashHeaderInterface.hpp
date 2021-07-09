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

#ifndef MICMGMT_FLASHHEADERINTERFACE_HPP
#define MICMGMT_FLASHHEADERINTERFACE_HPP

//  SYSTEM INCLUDES
//
#include    <map>
#include    <string>

// NAMESPACE
//
namespace  micmgmt {

//  FORWARD DECLARATIONS
//
class  MicDevice;


//----------------------------------------------------------------------------
//  CLASS:  FlashHeaderInterface

class  FlashHeaderInterface     // Abstract
{

public:

    enum  Platform  { eUnknownPlatform, eKncPlatform, eKnlPlatform };
    typedef std::map<std::string,std::string>  ItemVersionMap;


public:

    virtual ~FlashHeaderInterface() {}

    virtual bool            isValid() const = 0;
    virtual bool            isSigned() const = 0;
    virtual size_t          itemCount() const = 0;
    virtual ItemVersionMap  itemVersions() const = 0;
    virtual Platform        targetPlatform() const = 0;
    virtual bool            isCompatible( const MicDevice* device ) const = 0;

};

//----------------------------------------------------------------------------

}   // namespace micmgmt

#endif // MICMGMT_FLASHHEADERINTERFACE_HPP
