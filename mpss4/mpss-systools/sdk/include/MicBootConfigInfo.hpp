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

#ifndef MICMGMT_MICBOOTCONFIGINFO_HPP
#define MICMGMT_MICBOOTCONFIGINFO_HPP

//  SYSTEM INCLUDES
//
#include    <memory>
#include    <string>

// NAMESPACE
//
namespace  micmgmt {


//----------------------------------------------------------------------------
//  CLASS:  MicBootConfigInfo

class  MicBootConfigInfo
{

public:

    MicBootConfigInfo();
    MicBootConfigInfo( const std::string& image, bool custom=false );
    MicBootConfigInfo( const std::string& image, const std::string& initramfs, bool custom=false );
    MicBootConfigInfo( const MicBootConfigInfo& that );
    virtual ~MicBootConfigInfo();

    MicBootConfigInfo&  operator = ( const MicBootConfigInfo& that );

    virtual bool        isValid() const;
    bool                isCustom() const;
    std::string         imagePath() const;
    std::string         initRamFsPath() const;
    std::string         systemMapPath() const;

    virtual void        clear();
    void                setCustomState( bool custom );
    void                setImagePath( const std::string& path );
    void                setInitRamFsPath( const std::string& path );
    void                setSystemMapPath( const std::string& path );


private:

    struct  PrivData;
    std::unique_ptr<PrivData>  m_pData;

};

//----------------------------------------------------------------------------

}

#endif // MICMGMT_MICBOOTCONFIGINFO_HPP
