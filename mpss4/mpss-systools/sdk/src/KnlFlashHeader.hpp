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

#ifndef MICMGMT_KNLFLASHHEADER_HPP
#define MICMGMT_KNLFLASHHEADER_HPP

//  PROJECT INCLUDES
//
#include    "FATFileReaderBase.hpp"
#include    "FlashHeaderBase.hpp"
#include    <vector>
// NAMESPACE
//
namespace  micmgmt
{
//----------------------------------------------------------------------------
//  CLASS:  KnlFlashHeader

class FATFileReaderBase;
class  KnlFlashHeader : public FlashHeaderBase
{

public:

    KnlFlashHeader( FATFileReaderBase & ptr, FlashImage* image = 0);
    virtual ~KnlFlashHeader();

    bool    processData();
    std::vector<DataFile> getFiles() const;
    int getFileID(const std::string &) const;

private: // DISABLE
    KnlFlashHeader( const KnlFlashHeader& );
    KnlFlashHeader&  operator = ( const KnlFlashHeader& );
    void classifyFile(int fileID, std::string fileName);
    void cleanVersionName(std::string & str, bool removeLast = true) const;
};

//----------------------------------------------------------------------------

}   // namespace micmgmt

#endif // MICMGMT_KNLFLASHHEADER_HPP