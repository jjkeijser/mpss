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

#ifndef MICMGMT_MICDEVICEFACTORY_HPP
#define MICMGMT_MICDEVICEFACTORY_HPP

// SYSTEM INCLUDES
//
#include    <cstdint>
#include    <string>

// NAMESPACE
//
namespace  micmgmt
{

// FORWARD DECLARATIONS
//
class  MicDevice;
class  MicDeviceManager;
class  MicDeviceImpl;


//---------------------------------------------------------------------
//  CLASS:  MicDeviceFactory

class  MicDeviceFactory
{

public:

    MicDeviceFactory( MicDeviceManager* manager );
   ~MicDeviceFactory();

    MicDevice*  createDevice( size_t number=0 ) const;
    MicDevice*  createDevice( const std::string& name ) const;
    MicDevice*  createDevice( MicDeviceImpl* device ) const;
    uint32_t    errorCode() const;


private:

    MicDeviceManager*  m_pManager;
    mutable uint32_t   m_errorCode;


private:    // DISABLE

    MicDeviceFactory();
    MicDeviceFactory( const MicDeviceFactory& );
    MicDeviceFactory&  operator = ( const MicDeviceFactory& );

};

//----------------------------------------------------------------------------

}   // namespace micmgmt

//----------------------------------------------------------------------------

#endif // MICMGMT_MICDEVICEFACTORY_HPP
