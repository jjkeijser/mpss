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

#ifndef MICMGMT_FLASHHEADERBASE_HPP
#define MICMGMT_FLASHHEADERBASE_HPP

//  PROJECT INCLUDES
//

#include    "FlashHeaderInterface.hpp"
#include    "FlashImage.hpp"

// NAMESPACE
//
namespace  micmgmt {

// FORWARD DECLARATIONS
//
class  FlashImage;


//----------------------------------------------------------------------------
//  CLASS:  FlashHeaderBase

class  FlashHeaderBase : public FlashHeaderInterface
{

public:

    virtual ~FlashHeaderBase();

    bool               isValid() const;
    bool               isSigned() const;
    size_t             itemCount() const;
    ItemVersionMap     itemVersions() const;
    Platform           targetPlatform() const;
    virtual bool       isCompatible( const MicDevice* device ) const;
    virtual std::vector<DataFile> getFiles() const;
    virtual int        getFileID(const std::string &) const;

    virtual bool       processData() = 0;


protected:

    FlashHeaderBase( FATFileReaderBase & ptr, FlashImage* image=0 );

    FlashImage*        image();

    void               addItem( const std::string& name, const std::string& version );
    void               setTargetPlatform( Platform platform );
    void               setSigned( bool state );
    void               setValid( bool state );
    FATFileReaderBase & m_FatContents;


private:

    FlashImage*        m_pImage;
    ItemVersionMap     m_versions;
    Platform           m_platform;
    bool               m_signed;
    bool               m_valid;


private: // DISABLE

    FlashHeaderBase( const FlashHeaderBase& );
    FlashHeaderBase&  operator = ( const FlashHeaderBase& );

};

//----------------------------------------------------------------------------

}   // namespace micmgmt

#endif // MICMGMT_FLASHHEADERBASE_HPP