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

#ifndef MICMGMT_MPSS3STACKBASE_HPP
#define MICMGMT_MPSS3STACKBASE_HPP

// PROJECT INCLUDES
//
#include    "MpssStackBase.hpp"

// NAMESPACE
//
namespace  micmgmt
{


//============================================================================
//  CLASS:  Mpss3StackBase

class  Mpss3StackBase : public MpssStackBase
{

public:

   ~Mpss3StackBase();

    virtual uint32_t    getDeviceMode( int* mode ) const;
    virtual uint32_t    getDeviceState( int* state ) const;


public:  // STATIC

    static std::string  mpssStackVersion();


protected:

    explicit Mpss3StackBase( int device=-1 );


private: // DISABLE

    Mpss3StackBase( const Mpss3StackBase& );
    Mpss3StackBase&  operator = ( const Mpss3StackBase& );

};

//----------------------------------------------------------------------------

}   // namespace micmgmt

//----------------------------------------------------------------------------

#endif // MICMGMT_MPSS3STACKBASE_HPP
