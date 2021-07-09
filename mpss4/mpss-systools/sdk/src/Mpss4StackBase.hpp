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

#ifndef MICMGMT_MPSS4STACKBASE_HPP
#define MICMGMT_MPSS4STACKBASE_HPP

// PROJECT INCLUDES
//
#include    "MpssStackBase.hpp"

// NAMESPACE
//
namespace  micmgmt
{


//============================================================================
//  CLASS:  Mpss4StackBase

class  Mpss4StackBase : public MpssStackBase
{

public:

   virtual ~Mpss4StackBase();

public:  // STATIC

    static std::string  mpssStackVersion();


protected:

    explicit Mpss4StackBase( int device=-1 );


private: // DISABLE

    Mpss4StackBase( const Mpss4StackBase& );
    Mpss4StackBase&  operator = ( const Mpss4StackBase& );

};

//----------------------------------------------------------------------------

}   // namespace micmgmt

//----------------------------------------------------------------------------

#endif // MICMGMT_MPSS4STACKBASE_HPP
