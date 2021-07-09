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

#ifndef MICMGMT_MPSS4STACKMOCK_HPP
#define MICMGMT_MPSS4STACKMOCK_HPP

// PROJECT INCLUDES
//
#include    "Mpss4StackBase.hpp"

// SYSTEM INCLUDES
//
#include    <memory>

// NAMESPACE
//
namespace  micmgmt
{


//============================================================================
//  CLASS:  Mpss4StackMock

class  Mpss4StackMock : public Mpss4StackBase
{

public:

    explicit Mpss4StackMock( int device=-1 );
   ~Mpss4StackMock();

    std::string  mpssHomePath() const;
    std::string  mpssBootPath() const;
    std::string  mpssFlashPath() const;

//    uint32_t     getSystemMpssVersion( std::string* version ) const;
    uint32_t     getSystemDriverVersion( std::string* version ) const;
    uint32_t     getSystemDeviceNumbers( std::vector<size_t>* list ) const;
    uint32_t     getSystemDeviceType( int* type, size_t number ) const;
    uint32_t     getSystemProperty( std::string* data, const std::string& name ) const;

    uint32_t     getDeviceMode( int* mode ) const;
    uint32_t     getDeviceState( int* state ) const;
    uint32_t     getDevicePciConfigData( PciConfigData* data ) const;
    uint32_t     getDeviceProperty( std::string* data, const std::string& name ) const;

    uint32_t     deviceReset( bool force=false );


private:

    struct  PrivData;
    std::unique_ptr<PrivData>  m_pData;


private: // DISABLE

    Mpss4StackMock( const Mpss4StackMock& );
    Mpss4StackMock&  operator = ( const Mpss4StackMock& );

};

//----------------------------------------------------------------------------

}   // namespace micmgmt

//----------------------------------------------------------------------------

#endif // MICMGMT_MPSS4STACKMOCK_HPP
