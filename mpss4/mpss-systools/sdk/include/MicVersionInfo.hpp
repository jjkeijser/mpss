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

#ifndef MICMGMT_MICVERSIONINFO_HPP
#define MICMGMT_MICVERSIONINFO_HPP

//  SYSTEM INCLUDES
//
#include    <string>
#include    <memory>

// PROJECT INCLUDES
//
#include    "MicValue.hpp"

// NAMESPACE
//
namespace  micmgmt {

// FORWARD DECLARATIONS
//
struct  VersionInfoData;


//----------------------------------------------------------------------------
//  CLASS:  MicVersionInfo

class  MicVersionInfo
{

public:
    MicVersionInfo();
    explicit MicVersionInfo( const VersionInfoData& data );
    MicVersionInfo( const MicVersionInfo& that );
   ~MicVersionInfo();

    MicVersionInfo&  operator = ( const MicVersionInfo& that );

    bool                       isValid() const;
    const MicValueString&      flashVersion() const;
    const MicValueString&      biosReleaseDate() const;
    const MicValueString       biosVersion() const;
    const MicValueString&      oemStrings() const;
    const MicValueString       ntbVersion() const;
    const MicValueString       fabVersion() const;
    const MicValueString       smcVersion() const;
    const MicValueString&      smcBootLoaderVersion() const;
    const MicValueString&      ramControllerVersion() const;
    const MicValueString       meVersion() const;
    const MicValueString&      mpssStackVersion() const;
    const MicValueString&      driverVersion() const;

    void                clear();


private:
    std::unique_ptr<VersionInfoData>  m_pData;
    //std::string m_FabVersion;

};

//----------------------------------------------------------------------------

}

#endif // MICMGMT_MICVERSIONINFO_HPP
