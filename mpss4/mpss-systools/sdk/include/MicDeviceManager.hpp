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

#ifndef MICMGMT_MICDEVICEMANAGER_HPP
#define MICMGMT_MICDEVICEMANAGER_HPP

// SYSTEM INCLUDES
//
#include    <cstdint>
#include    <string>

// NAMESPACE
//
namespace  micmgmt
{


//---------------------------------------------------------------------
//  CLASS:  MicDeviceManager

class  MicDeviceManager
{

public:

    enum  DeviceType
    {
        eDeviceTypeUndefined = -1,
        eDeviceTypeAny       =  0,
        eDeviceTypeKnc       =  1,
        eDeviceTypeKnl       =  2
    };


public:

    MicDeviceManager();
    virtual ~MicDeviceManager();

    bool         isInitialized() const;
    bool         isMpssAvailable() const;
    std::string  mpssVersion() const;
    std::string  driverVersion() const;
    std::string  deviceBaseName() const;
    size_t       deviceCount( int type=eDeviceTypeAny ) const;
    bool         isDeviceType( size_t number, int type ) const;
    bool         isDeviceAvailable( size_t number ) const;
    bool         isDeviceAvailable( const std::string& name ) const;
    uint32_t     initializationError() const;
    bool         isValidConfigCard( size_t number ) const;

    uint32_t     initialize();


private:

    class  PrivImpl;
    PrivImpl*  m_pImpl;


private: // DISABLE

    MicDeviceManager( const MicDeviceManager& );
    MicDeviceManager&  operator = ( const MicDeviceManager& );

};

//----------------------------------------------------------------------------

}   // namespace micmgmt

//----------------------------------------------------------------------------

#endif // MICMGMT_MICDEVICEMANAGER_HPP
