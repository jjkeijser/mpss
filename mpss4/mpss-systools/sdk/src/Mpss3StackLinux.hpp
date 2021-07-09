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

#ifndef MICMGMT_MPSS3STACKLINUX_HPP
#define MICMGMT_MPSS3STACKLINUX_HPP

// PROJECT INCLUDES
//
#include    "Mpss3StackBase.hpp"

// SYSTEM INCLUDES
//
#include    <memory>

// NAMESPACE
//
namespace  micmgmt
{


//============================================================================
//  CLASS:  Mpss3StackLinux

class  Mpss3StackLinux : public Mpss3StackBase
{

public:

    explicit Mpss3StackLinux( int device=-1 );
   ~Mpss3StackLinux();

    std::string  mpssHomePath() const;
    std::string  mpssBootPath() const;
    std::string  mpssFlashPath() const;

    uint32_t     getSystemDriverVersion( std::string* version ) const;
    uint32_t     getSystemDeviceNumbers( std::vector<size_t>* list ) const;
    uint32_t     getSystemDeviceType( int* type, size_t number ) const;
    uint32_t     getSystemProperty( std::string* data, const std::string& name ) const;

    uint32_t     getDevicePciConfigData( PciConfigData* data ) const;
    uint32_t     getDeviceProperty( std::string* data, const std::string& name ) const;

    uint32_t     deviceReset( bool force=false );
    uint32_t     setDeviceProperty( const std::string& name, const std::string& data );
    uint32_t     setDevicePowerStates( const std::list<MicPowerState>& states );


private:

    uint32_t     getDeviceProperty( std::string* data, const std::string& name, const std::string& subkey, char sep='=' ) const;

    bool         loadLib();
    void         unloadLib();


private:

    struct  PrivData;
    std::unique_ptr<PrivData>  m_pData;


private: // DISABLE

    Mpss3StackLinux( const Mpss3StackLinux& );
    Mpss3StackLinux&  operator = ( const Mpss3StackLinux& );

};

//----------------------------------------------------------------------------

}   // namespace micmgmt

//----------------------------------------------------------------------------

#endif // MICMGMT_MPSS3STACKLINUX_HPP
