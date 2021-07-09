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

#ifndef MICMGMT_MICPROCESSORINFO_HPP
#define MICMGMT_MICPROCESSORINFO_HPP

//  SYSTEM INCLUDES
//
#include    <cstdint>
#include    <string>
#include    <memory>

// PROJECT INCLUDES
//
#include "MicValue.hpp"

// NAMESPACE
//
namespace  micmgmt {

// FORWARD DECLARATIONS
//
struct  ProcessorInfoData;


//----------------------------------------------------------------------------
//  CLASS:  MicProcessorInfo

class  MicProcessorInfo
{

public:
    MicProcessorInfo();
    explicit MicProcessorInfo( const ProcessorInfoData& data );
    MicProcessorInfo( const MicProcessorInfo& that );
   ~MicProcessorInfo();

    MicProcessorInfo&  operator = ( const MicProcessorInfo& that );

    bool                            isValid() const;
    const MicValueUInt16&           type() const;
    const MicValueUInt16&           model() const;
    const MicValueUInt16&           family() const;
    const MicValueUInt16&           extendedModel() const;
    const MicValueUInt16&           extendedFamily() const;
    const MicValueUInt32&           steppingId() const;
    const MicValueUInt32&           substeppingId() const;
    const MicValueString&           stepping() const;
    const MicValueString&           sku() const;

    void                            clear();


private:
    std::unique_ptr<ProcessorInfoData>  m_pData;

};

//----------------------------------------------------------------------------

}

#endif // MICMGMT_MICPROCESSORINFO_HPP
