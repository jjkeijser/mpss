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

#ifndef MICMGMT_MICPLATFORMINFO_HPP
#define MICMGMT_MICPLATFORMINFO_HPP

//  PROJECT INCLUDES
//
#include    "MicValue.hpp"

//  SYSTEM INCLUDES
//
#include    <cstdint>
#include    <memory>
#include    <string>

// NAMESPACE
//
namespace  micmgmt {

// FORWARD DECLARATIONS
//
class   MicPower;
struct  PlatformInfoData;


//----------------------------------------------------------------------------
//  CLASS:  MicPlatformInfo

class  MicPlatformInfo
{

public:

    MicPlatformInfo();
    explicit MicPlatformInfo( const PlatformInfoData& data );
    MicPlatformInfo( const MicPlatformInfo& that );
   ~MicPlatformInfo();

    MicPlatformInfo&  operator = ( const MicPlatformInfo& that );
    bool              isValid() const;

    MicValueBool      isCoolingActive() const;
    MicValueString    partNumber() const;
    MicValueString    manufactureDate() const;
    MicValueString    manufactureTime() const;
    MicValueString    serialNo() const;
    MicValueString    uuid() const;
    MicValueString    featureSet() const;
    MicValueString    coprocessorOs() const;
    MicValueString    coprocessorOsVersion() const;
    MicValueString    coprocessorBrand() const;
    MicValueString    boardSku() const;
    MicValueString    boardType() const;
    MicValueString    strapInfo() const;
    MicValueUInt32    strapInfoRaw() const;
    MicValueUInt8     smBusBaseAddress() const;
    MicPower          maxPower() const;

    void              clear();


private:

    std::unique_ptr<PlatformInfoData>  m_pData;

};

//----------------------------------------------------------------------------

}

#endif // MICMGMT_MICPLATFORMINFO_HPP
