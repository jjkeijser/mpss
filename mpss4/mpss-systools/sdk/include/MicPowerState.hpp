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

#ifndef MICMGMT_MICPOWERSTATE_HPP
#define MICMGMT_MICPOWERSTATE_HPP

// SYSTEM INCLUDES
//
#include    <string>

// NAMESPACE
//
namespace  micmgmt
{


//============================================================================
//  CLASS:  MicPowerState

class  MicPowerState
{

public:

    enum  State  { eCpuFreq, eCoreC6, ePc3, ePc6  };


public:

    explicit MicPowerState( State state, bool enabled=false );
    MicPowerState( const MicPowerState& that );
   ~MicPowerState();

    MicPowerState&  operator = ( const MicPowerState& that );

    bool            isAvailable() const;
    bool            isEnabled() const;
    State           state() const;
    std::string     name() const;

    void            setAvailable( bool state );
    void            setEnabled( bool state );


public: // STATIC

    static std::string  stateAsString( State state );


private:

    State  m_State;
    bool   m_Enabled;
    bool   m_Available;


private: // DISABLE

    MicPowerState();

};

//----------------------------------------------------------------------------

}   // namespace micmgmt

//----------------------------------------------------------------------------

#endif // MICMGMT_MICPOWERSTATE_HPP
