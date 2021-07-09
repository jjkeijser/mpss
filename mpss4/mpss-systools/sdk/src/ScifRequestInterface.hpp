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

#ifndef MICMGMT_SCIFREQUESTINTERFACE_HPP
#define MICMGMT_SCIFREQUESTINTERFACE_HPP

//----------------------------------------------------------------------------
//  CLASS:  ScifRequestInterface

namespace micmgmt
{

class ScifRequestInterface     // Abstract
{
public:
    virtual ~ScifRequestInterface() { }

    virtual bool        isValid() const = 0;
    virtual bool        isError() const = 0;
    virtual bool        isSendRequest() const = 0;
    virtual int         command() const = 0;
    virtual uint32_t    parameter() const = 0;
    virtual char*       buffer() const = 0;
    virtual size_t      byteCount() const = 0;
    virtual int         errorCode() const = 0;

    virtual void        clearError() = 0;
    virtual void        setError( int error ) = 0;
    virtual void        setSendRequest( bool state ) = 0;

};

} // micmgmt

#endif // MICMGMT_SCIFREQUESTINTERFACE_HPP
