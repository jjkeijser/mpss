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

#ifndef MICMGMT_MICPCICONFIGINFO_HPP
#define MICMGMT_MICPCICONFIGINFO_HPP

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
class   PciAddress;
struct  PciConfigData;


//----------------------------------------------------------------------------
//  CLASS:  MicPciConfigInfo

class  MicPciConfigInfo
{

public:

    enum  AspmState  { eUnknown, eL0sL1Disabled, eL0sEnabled, eL1Enabled, eL0sL1Enabled };


public:

    MicPciConfigInfo();
    explicit MicPciConfigInfo( const PciConfigData& data );
    MicPciConfigInfo( const MicPciConfigInfo& that );
   ~MicPciConfigInfo();

    MicPciConfigInfo&   operator = ( const MicPciConfigInfo& that );

    bool                isValid() const;
    bool                hasFullAccess() const;
    bool                isNoSnoopEnabled() const;
    bool                isExtendedTagEnabled() const;
    bool                isRelaxedOrderEnabled() const;
    AspmState           aspmControlState() const;
    PciAddress          address() const;
    uint16_t            vendorId() const;
    uint16_t            deviceId() const;
    uint8_t             revisionId() const;
    uint16_t            subsystemId() const;
    MicSpeed            linkSpeed() const;
    uint32_t            linkWidth() const;
    uint32_t            payloadSize() const;
    uint32_t            requestSize() const;
    uint32_t            classCode() const;

    void                clear();


public: // STATIC

    static std::string  aspmStateAsString( AspmState state );


private:

    std::unique_ptr<PciConfigData>  m_pData;

};

//----------------------------------------------------------------------------

}

#endif // MICMGMT_MICPCICONFIGINFO_HPP
