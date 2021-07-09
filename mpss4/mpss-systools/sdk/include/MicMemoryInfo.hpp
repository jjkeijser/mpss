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

#ifndef MICMGMT_MICMEMORYINFO_HPP
#define MICMGMT_MICMEMORYINFO_HPP

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
class   MicSpeed;
class   MicMemory;
class   MicVoltage;
class   MicFrequency;
struct  MemoryInfoData;


//----------------------------------------------------------------------------
//  CLASS:  MicMemoryInfo

class  MicMemoryInfo
{

public:
    MicMemoryInfo();
    explicit MicMemoryInfo( const MemoryInfoData& data );
    MicMemoryInfo( const MicMemoryInfo& that );
   ~MicMemoryInfo();

    MicMemoryInfo&  operator = ( const MicMemoryInfo& that );

    bool            isValid() const;
    std::string     vendorName() const;
    std::string     memoryType() const;
    std::string     technology() const;
    uint16_t        revision() const;
    MicMemory       density() const;
    MicMemory       size() const;
    MicSpeed        speed() const;
    MicFrequency    frequency() const;
    bool            isEccEnabled() const;

    void            clear();


private:
    std::unique_ptr<MemoryInfoData>  m_pData;

};

//----------------------------------------------------------------------------

}

#endif // MICMGMT_MICMEMORYINFO_HPP
