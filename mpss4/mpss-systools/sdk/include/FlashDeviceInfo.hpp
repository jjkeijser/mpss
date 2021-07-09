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

#ifndef MICMGMT_FLASHDEVICEINFO_HPP
#define MICMGMT_FLASHDEVICEINFO_HPP

//  SYSTEM INCLUDES
//
#include    <cstdint>
#include    <string>

// NAMESPACE
//
namespace  micmgmt {


//----------------------------------------------------------------------------
//  CLASS:  FlashDeviceInfo

class  FlashDeviceInfo
{

public:

    explicit FlashDeviceInfo( uint32_t data=0 );
    FlashDeviceInfo( const FlashDeviceInfo& that );
   ~FlashDeviceInfo();

    FlashDeviceInfo&  operator = ( uint32_t data );
    FlashDeviceInfo&  operator = ( const FlashDeviceInfo& that );

    bool              isValid() const;
    int               manufacturerId() const;
    int               familyId() const;
    int               modelId() const;
    std::string       manufacturer() const;
    std::string       family() const;
    std::string       model() const;
    std::string       info() const;

    void              clear();


private:

    uint32_t  m_data;

};

//----------------------------------------------------------------------------

}   // namespace micmgmt

#endif // MICMGMT_FLASHDEVICEINFO_HPP
