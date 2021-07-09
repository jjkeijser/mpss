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

#ifndef MICMGMT_MICDEVICEINFO_HPP
#define MICMGMT_MICDEVICEINFO_HPP

// SYSTEM INCLUDES
//
#include    <string>
#include    <memory>

// NAMESPACE
//
namespace  micmgmt
{

// FORWARD DECLARATIONS
//
class  MicDevice;
class  MicPciConfigInfo;
class  MicProcessorInfo;
class  MicVersionInfo;


//============================================================================
//  CLASS:  MicDeviceInfo

class  MicDeviceInfo
{

public:

    MicDeviceInfo( MicDevice* device=0 );
   ~MicDeviceInfo();

    int                      deviceNumber() const;
    std::string              deviceName() const;
    std::string              deviceType() const;
    std::string              deviceSku() const;
    const MicPciConfigInfo&  pciConfigInfo() const;
    const MicProcessorInfo&  processorInfo() const;
    const MicVersionInfo&    versionInfo() const;

    void                     clear();


private:

    struct PrivData;
    std::unique_ptr<PrivData>  m_pData;


private:    // DISABLE

    MicDeviceInfo( const MicDeviceInfo& );
    MicDeviceInfo&  operator = ( const MicDeviceInfo& );

};

//----------------------------------------------------------------------------

}   // namespace micmgmt

//----------------------------------------------------------------------------

#endif // MICMGMT_MICDEVICEINFO_HPP
