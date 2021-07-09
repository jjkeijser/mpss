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

#ifndef MICMGMT_MICCOREINFO_HPP
#define MICMGMT_MICCOREINFO_HPP

//  SYSTEM INCLUDES
//
#include    <cstdint>
#include    <memory>

//  PROJECT INCLUDES
//
#include "MicValue.hpp"

// NAMESPACE
//
namespace  micmgmt {

// FORWARD DECLARATIONS
//
class   MicVoltage;
class   MicFrequency;
struct  CoreInfoData;


//----------------------------------------------------------------------------
//  CLASS:  MicCoreInfo

class  MicCoreInfo
{

public:

    MicCoreInfo();
    explicit MicCoreInfo( const CoreInfoData& data );
    MicCoreInfo( const MicCoreInfo& that );
   ~MicCoreInfo();

    MicCoreInfo&  operator = ( const MicCoreInfo& that );

    bool                        isValid() const;
    const MicValueUInt64&       coreCount() const;
    const MicValueUInt64&       coreThreadCount() const;
    MicFrequency                frequency() const;
    MicVoltage                  voltage() const;

    void                        clear();


private:

    std::unique_ptr<CoreInfoData>  m_pData;

};

//----------------------------------------------------------------------------

}   // namespace micmgmt

#endif // MICMGMT_MICCOREINFO_HPP
