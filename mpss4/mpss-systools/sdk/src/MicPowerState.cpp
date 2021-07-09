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

// PROJECT INCLUDES
//
#include    "MicPowerState.hpp"

// SYSTEM INCLUDES
//
#include    <string>
#include    <map>

// LOCAL CONSTANTS
//
namespace  {
typedef std::map<micmgmt::MicPowerState::State,std::string>  StateInfo;
const StateInfo  STATES = {
    { micmgmt::MicPowerState::eCpuFreq, "cpufreq" },
    { micmgmt::MicPowerState::eCoreC6,  "corec6" },
    { micmgmt::MicPowerState::ePc3,     "pc3" },
    { micmgmt::MicPowerState::ePc6,     "pc6" } };
}

// NAMESPACES
//
using namespace  micmgmt;
using namespace  std;


//============================================================================
/** @class    micmgmt::MicPowerState      MicPowerState.hpp
 *  @ingroup  sdk
 *  @brief    The class represents a power state.
 *
 *  The \b %MicPowerState abstract a device power state.
 */


//============================================================================
//  P U B L I C   C O N S T A N T S
//============================================================================

//----------------------------------------------------------------------------
/** @enum   micmgmt::MicPowerState::State
 *
 *  Enumerations of power states.
 */

/** @var    MicPowerState::State  MicPowerState::eCpuFreq
 *
 *  CPU freq state.
 */

/** @var    MicPowerState::State  MicPowerState::eCoreC6
 *
 *  Core C6 state.
 */

/** @var    MicPowerState::State  MicPowerState::ePc3
 *
 *  PC3 state.
 */

/** @var    MicPowerState::State  MicPowerState::ePc6
 *
 *  PC6 state.
 */


//============================================================================
//  P U B L I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     MicPowerState::MicPowerState( State state, bool enabled )
 *  @param  state    Power state type
 *  @param  enabled  Enabled state (optional)
 *
 *  Construct a power state object representing given power \a state.
 *  Optionally, the power state can be flagged as enabled. By default, the
 *  power state is flagged as disabled.
 *
 *  The power state is set available by default.
 */

MicPowerState::MicPowerState( State state, bool enabled ) :
    m_State( state ),
    m_Enabled( enabled ),
    m_Available( true )
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicPowerState::MicPowerState( const MicPowerState& that )
 *  @param  that    Other data sample object
 *
 *  Construct a data sample object as deep copy of \a that object.
 */

MicPowerState::MicPowerState( const MicPowerState& that )
{
    m_State     = that.m_State;
    m_Enabled   = that.m_Enabled;
    m_Available = that.m_Available;
}


//----------------------------------------------------------------------------
/** @fn     MicPowerState::~MicPowerState()
 *
 *  Cleanup.
 */

MicPowerState::~MicPowerState()
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicPowerState&  MicPowerState::operator = ( const MicPowerState& that )
 *  @param  that    Other power state object
 *  @return updated power state
 *
 *  Assign \a that object to this object and return the updated object.
 */

MicPowerState&  MicPowerState::operator = ( const MicPowerState& that )
{
    if (&that != this)
    {
        m_State     = that.m_State;
        m_Enabled   = that.m_Enabled;
        m_Available = that.m_Available;
    }

    return  *this;
}


//----------------------------------------------------------------------------
/** @fn     bool  MicPowerState::isAvailable() const
 *  @return availability state
 *
 *  Returns \c true if the power state is available.
 *
 *  Some power states are not available on all platforms. This function can
 *  be used to find out if the platform supports this power state or not.
 */

bool  MicPowerState::isAvailable() const
{
    return  m_Available;
}


//----------------------------------------------------------------------------
/** @fn     bool  MicPowerState::isEnabled() const
 *  @return set state
 *
 *  Returns \c true if the power state is valid and set.
 */

bool  MicPowerState::isEnabled() const
{
    return  (m_Available && m_Enabled);
}


//----------------------------------------------------------------------------
/** @fn     MicPowerState::State  MicPowerState::state() const
 *  @return power state type
 *
 *  Returns the power state type.
 */

MicPowerState::State  MicPowerState::state() const
{
    return  m_State;
}


//----------------------------------------------------------------------------
/** @fn     std::string  MicPowerState::name() const
 *  @return state name
 *
 *  Returns the state name. This is equal to stateAsString( state() ).
 */

std::string  MicPowerState::name() const
{
    return  stateAsString( state() );
}


//----------------------------------------------------------------------------
/** @fn     void  MicPowerState::setAvailable( bool state )
 *  @param  state   Availability state
 *
 *  Flag the power state as available or unavailable depending on specified
 *  \a state.
 */

void  MicPowerState::setAvailable( bool state )
{
    m_Available = state;
}


//----------------------------------------------------------------------------
/** @fn     void  MicPowerState::setEnabled( bool state )
 *  @param  state   Enabled state
 *
 *  Enable or disable the power state.
 */

void  MicPowerState::setEnabled( bool state )
{
    m_Enabled = state;
}


//============================================================================
//  P U B L I C   S T A T I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     std::string  MicPowerState::stateAsString( State state )
 *  @param  state   Power state
 *  @return state string
 *
 *  Return a string representing specified power \a state.
 */

std::string  MicPowerState::stateAsString( State state )
{
    StateInfo::const_iterator  iter = STATES.find( state );

    return  ((iter == STATES.end()) ? "???" : iter->second);
}

