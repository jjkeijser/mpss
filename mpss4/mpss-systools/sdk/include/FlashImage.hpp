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

#ifndef MICMGMT_FLASHIMAGE_HPP
#define MICMGMT_FLASHIMAGE_HPP

//  PROJECT INCLUDES
//
#include    "FlashHeaderInterface.hpp"
#include    "FATFileReaderBase.hpp"

//  SYSTEM INCLUDES
//
#include    <memory>
#include    <string>
#include    <vector>
//  NAMESPACE
//
namespace  micmgmt {

struct DataFile;

struct FabImagesPaths
{
    std::string nameA;
    std::string nameB;
    std::string nameX;
    std::string nameT;
    FabImagesPaths() {}
    FabImagesPaths(const std::string & nameA_, const std::string & nameB_,
                   const std::string & nameX_, const std::string & nameT_)
        : nameA(nameA_), nameB(nameB_), nameX(nameX_), nameT(nameT_) {}
};

struct FwToUpdate
{
    bool mBios;
    bool mSMC;
    bool mME;
    bool mBT;
    FwToUpdate() : mBios(false), mSMC(false), mME(false), mBT(false) {}
    FwToUpdate(const bool & mBios_, const bool & mSMC_, const bool & mME_)
        : mBios(mBios_), mSMC(mSMC_), mME(mME_), mBT(false) {}
};

//  FORWARD DECLARATIONS
//
class  FlashHeaderBase;


//----------------------------------------------------------------------------
//  CLASS:  FlashImage

class  FlashImage : public FlashHeaderInterface
{

public:

    FlashImage();
    virtual ~FlashImage();

    bool              isValid() const;
    bool              isSigned() const;
    size_t            itemCount() const;
    ItemVersionMap    itemVersions() const;
    Platform          targetPlatform() const;
    bool              isCompatible( const MicDevice* device ) const;
    std::string       filePath() const;
    size_t            size() const;
    const char*       data() const;
    unsigned long     getPosNameFabA() const;
    unsigned long     getPosNameFabB() const;
    unsigned long     getPosNameFabX() const;
    unsigned long     getPosNameFabT() const;
    FabImagesPaths    getFabImagesPaths() const;
    std::vector<DataFile> getFiles() const;
    int               getFileID(const std::string & line) const;
    FwToUpdate        getFwToUpdate() const;

    virtual void      clear();
    void              setPosNameFabA(unsigned long pos);
    void              setPosNameFabB(unsigned long pos);
    void              setPosNameFabX(unsigned long pos);
    void              setPosNameFabT(unsigned long pos);
    void              setFabPath(const std::string& path);
    uint32_t          setFilePath( const std::string& path );
    void              setBios();
    void              setSMC();
    void              setME();
    void              setBootTest();

    static const std::string BIOS;
    static const std::string ME;
    static const std::string SMCFABA;
    static const std::string SMCFABB;

protected:

    FlashHeaderBase*  flashHeader() const;
    virtual uint32_t  updateInfo();
    void              clearImage();


private:

    struct PrivData;
    std::unique_ptr<PrivData>  m_pData;
    std::unique_ptr<FATFileReaderBase> m_pFat;


private: // DISABLE

    FlashImage( const FlashImage& );
    FlashImage&  operator = ( const FlashImage& );

};

//----------------------------------------------------------------------------

}   // namespace micmgmt

#endif // MICMGMT_FLASHIMAGE_HPP
