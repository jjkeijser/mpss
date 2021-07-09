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

#ifndef MICMGMT_SCIFCONNECTIONBASE_HPP
#define MICMGMT_SCIFCONNECTIONBASE_HPP

// SYSTEM INCLUDES
//
#include    <cstdint>
#include    <memory>
#include    <string>

// PROJECT INCLUDES
//
#include    "LoaderInterface.hpp"


// NAMESPACE
//
namespace  micmgmt
{

// FORWARD DECLARATIONS
//
class  ScifRequestInterface;
#ifdef UNIT_TESTS
class ScifFunctions;
#endif // UNIT_TESTS

#ifdef UNIT_TESTS
#define PROTECTED public
#define PRIVATE public
#else
#define PROTECTED protected
#define PRIVATE private
#endif // UNIT_TESTS


//============================================================================
//  CLASS:  ScifConnectionBase

class  ScifConnectionBase
{

public:

    virtual ~ScifConnectionBase();

    bool              isOpen() const;
    int               deviceNum() const;
    std::string       errorText() const;

    uint32_t          open();
    void              close();

    virtual uint32_t  request( ScifRequestInterface* request ) = 0;


PROTECTED:

    explicit ScifConnectionBase( int devnum );

#ifdef UNIT_TESTS
    ScifConnectionBase( int devnum, int portnum, int handle, bool online,
            std::string errorText, ScifFunctions& functions );
#endif // UNIT_TESTS

    int               portNum() const;

    bool              lock();
    bool              unlock();
    int               send( const char* data, int len );
    int               receive( char* data, int len );
    void              setPortNum( int port );
    void              setErrorText( const std::string& text, int error=0 );
    int               setLoader( LoaderInterface* loader );


PRIVATE:

    int               systemErrno() const;

    bool              loadLib();
    void              unloadLib();


private:

    struct  PrivData;
    std::unique_ptr<PrivData>  m_pData;
    std::unique_ptr<LoaderInterface> m_loader;


private: // DISABLE

    ScifConnectionBase();
    ScifConnectionBase( const ScifConnectionBase& );
    ScifConnectionBase&  operator = ( const ScifConnectionBase& );

};

//----------------------------------------------------------------------------

}   // namespace micmgmt

//----------------------------------------------------------------------------

#endif // MICMGMT_SCIFCONNECTIONBASE_HPP
