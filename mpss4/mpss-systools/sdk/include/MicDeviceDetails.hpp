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

#ifndef MICMGMT_MICDEVICEDETAILS_HPP
#define MICMGMT_MICDEVICEDETAILS_HPP

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
class  MicCoreInfo;
class  MicMemoryInfo;
class  MicPlatformInfo;


//============================================================================
//  CLASS:  MicDeviceDetails

class  MicDeviceDetails
{

public:

    MicDeviceDetails( MicDevice* device=0 );
   ~MicDeviceDetails();

    int                      deviceNumber() const;
    std::string              deviceName() const;
    std::string              deviceType() const;
    const MicCoreInfo&       coreInfo() const;
    const MicMemoryInfo&     memoryInfo() const;
    const MicPlatformInfo&   micPlatformInfo() const;

    void                     clear();


private:

    struct PrivData;
    std::unique_ptr<PrivData>  m_pData;


private:    // DISABLE

    MicDeviceDetails( const MicDeviceDetails& );
    MicDeviceDetails&  operator = ( const MicDeviceDetails& );

};

//----------------------------------------------------------------------------

}   // namespace micmgmt

//----------------------------------------------------------------------------

#endif // MICMGMT_MICDEVICEDETAILS_HPP
