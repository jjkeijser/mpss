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

#ifndef MICMGMT_MICDEVICEMANAGER_P_HPP
#define MICMGMT_MICDEVICEMANAGER_P_HPP

// PROJECT INCLUDES
//
#include    "micmgmtCommon.hpp"

// SYSTEM INCLUDES
//
#include    <string>
#include    <vector>

// NAMESPACE
//
namespace  micmgmt
{

// FORWARD DECLARATIONS
//
class  MpssStackBase;


//---------------------------------------------------------------------
//  CLASS:  MicDeviceManager::PrivImpl

class  MicDeviceManager::PrivImpl
{

public:

    PrivImpl();
   ~PrivImpl();

    bool         isInitialized() const;
    std::string  deviceBaseName() const;
    size_t       deviceCount( int type=eDeviceTypeAny ) const;
    int          deviceNumberAt( size_t index ) const;
    int          deviceItemIndex( size_t number ) const;
    int          deviceType( size_t number ) const;
    bool         isValidConfigCard( size_t number ) const;

    uint32_t     initialize();


private:

    uint32_t     updateMpssVersion();
    uint32_t     updateDriverVersion();
    uint32_t     updateDeviceCount();


public:

    typedef std::pair<size_t,int>    DeviceItem;
    typedef std::vector<DeviceItem>  DeviceList;

    MpssStackBase*  m_pMpssStack;
    std::string     m_mpssVersion;
    std::string     m_driverVersion;
    DeviceList      m_deviceItems;
    DeviceList      m_invalidDevices;
    int             m_initError;


private: // DISABLE

    PrivImpl( const PrivImpl& );
    PrivImpl&  operator = ( const PrivImpl& );

};

//----------------------------------------------------------------------------

}   // namespace micmgmt

//----------------------------------------------------------------------------

#endif // MICMGMT_MICDEVICEMANAGER_P_HPP
