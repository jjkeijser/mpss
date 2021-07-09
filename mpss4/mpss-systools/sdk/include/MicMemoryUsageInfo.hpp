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

#ifndef MICMGMT_MICMEMORYUSAGEINFO_HPP
#define MICMGMT_MICMEMORYUSAGEINFO_HPP

//  SYSTEM INCLUDES
//
#include    <cstdint>
#include    <memory>

// NAMESPACE
//
namespace  micmgmt {

// FORWARD DECLARATIONS
//
class   MicMemory;
struct  MemoryUsageData;


//----------------------------------------------------------------------------
//  CLASS:  MicMemoryUsageInfo

class  MicMemoryUsageInfo
{

public:
    MicMemoryUsageInfo();
    explicit MicMemoryUsageInfo( const MemoryUsageData& data );
    MicMemoryUsageInfo( const MicMemoryUsageInfo& that );
   ~MicMemoryUsageInfo();

    MicMemoryUsageInfo&  operator = ( const MicMemoryUsageInfo& that );

    bool            isValid() const;
    MicMemory       total() const;
    MicMemory       used() const;
    MicMemory       free() const;
    MicMemory       buffers() const;
    MicMemory       cached() const;

    void            clear();


private:
    std::unique_ptr<MemoryUsageData>  m_pData;

};

//----------------------------------------------------------------------------

}

#endif // MICMGMT_MICMEMORYUSAGEINFO_HPP
