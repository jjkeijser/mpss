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
#include    "MicPciConfigInfo.hpp"
//
#include    "PciConfigData_p.hpp"   // Private

// NAMESPACE
//
using namespace  micmgmt;
using namespace  std;


//============================================================================
/** @class    micmgmt::MicPciConfigInfo   MicPciConfigInfo.hpp
 *  @ingroup  sdk
 *  @brief    This class encapsulates PCIe configuration
 *
 *  The \b %MicPciConfigInfo class encapsulates PCIe information of a MIC device
 *  and provides accessors to the relevant information.
 *
 *  Please \b note that all accessors only return valid data if isValid()
 *  returns \c true.
 */


//============================================================================
//  P U B L I C   C O N S T A N T S
//============================================================================

//----------------------------------------------------------------------------
/** @enum   micmgmt::MicPciConfigInfo::AspmState
 *
 *  Enumerations of ASPM control states.
 */

/** @var    MicPciConfigInfo::AspmState  MicPciConfigInfo::eUnknown
 *
 *  Unknown state.
 */

/** @var    MicPciConfigInfo::AspmState  MicPciConfigInfo::eL0sL1Disabled
 *
 *  Both L0s and L1 control state disabled.
 */

/** @var    MicPciConfigInfo::AspmState  MicPciConfigInfo::eL0sEnabled
 *
 *  L0s control state enabled.
 */

/** @var    MicPciConfigInfo::AspmState  MicPciConfigInfo::eL1Enabled
 *
 *  L1 control state enabled.
 */

/** @var    MicPciConfigInfo::AspmState  MicPciConfigInfo::eL0sL1Enabled
 *
 *  Both L0s and L1 control state enabled.
 */


//============================================================================
//  P U B L I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     MicPciConfigInfo::MicPciConfigInfo()
 *
 *  Construct an empty PCI configuration object.
 */

MicPciConfigInfo::MicPciConfigInfo() :
    m_pData( new PciConfigData )
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicPciConfigInfo::MicPciConfigInfo( const PciConfigData& data )
 *  @param  data    Mic PCI config data
 *
 *  Construct a mic PCI config object based on specified PCI \a data.
 */

MicPciConfigInfo::MicPciConfigInfo( const PciConfigData& data ) :
    m_pData( new PciConfigData )
{
    m_pData->set( data );
}


//----------------------------------------------------------------------------
/** @fn     MicPciConfigInfo::MicPciConfigInfo( const MicPciConfigInfo& that )
 *  @param  that    Other mic PCI config object
 *
 *  Construct a mic PCI config object as deep copy of \a that object.
 */

MicPciConfigInfo::MicPciConfigInfo( const MicPciConfigInfo& that ) :
    m_pData( new PciConfigData )
{
    m_pData->set( *(that.m_pData) );
}


//----------------------------------------------------------------------------
/** @fn     MicPciConfigInfo::~MicPciConfigInfo()
 *
 *  Cleanup.
 */

MicPciConfigInfo::~MicPciConfigInfo()
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicPciConfigInfo&  MicPciConfigInfo::operator = ( const MicPciConfigInfo& that )
 *  @param  that    Other mic PCI config object
 *  @return reference to this object
 *
 *  Assign \a that object to this object and return the updated object.
 */

MicPciConfigInfo&  MicPciConfigInfo::operator = ( const MicPciConfigInfo& that )
{
    if (&that != this)
        m_pData->set( *(that.m_pData) );

    return  *this;
}


//----------------------------------------------------------------------------
/** @fn     bool  MicPciConfigInfo::isValid() const
 *  @return valid state
 *
 *  Returns \c true if the PCI configuration data is valid.
 */

bool  MicPciConfigInfo::isValid() const
{
    return  (m_pData ? m_pData->mValid : false);
}


//----------------------------------------------------------------------------
/** @fn     bool  MicPciConfigInfo::hasFullAccess() const
 *  @return access state
 *
 *  Returns \c true if the user has full access to all PCI configuration info.
 *  Some PCI configuration properties are only accessible to users with root
 *  or administrator rights.
 */

bool  MicPciConfigInfo::hasFullAccess() const
{
    return  (isValid() ? m_pData->mHasAccess : false);
}


//----------------------------------------------------------------------------
/** @fn     bool  MicPciConfigInfo::isNoSnoopEnabled() const
 *  @return no snoop enabled state
 *
 *  Returns \c true if the PCIe no snoop feature is enabled.
 */

bool  MicPciConfigInfo::isNoSnoopEnabled() const
{
    return  (isValid() ? m_pData->mNoSnoop : false);
}


//----------------------------------------------------------------------------
/** @fn     bool  MicPciConfigInfo::isExtendedTagEnabled() const
 *  @return extended tag enabled state
 *
 *  Returns \c true if the PCIe extended tag feature is enabled.
 */

bool  MicPciConfigInfo::isExtendedTagEnabled() const
{
    return  (isValid() ? m_pData->mExtTagEnable : false);
}


//----------------------------------------------------------------------------
/** @fn     bool  MicPciConfigInfo::isRelaxedOrderEnabled() const
 *  @return relaxed ordering enabled state
 *
 *  Returns \c true if the relaxed ordering feature is enabled.
 */

bool  MicPciConfigInfo::isRelaxedOrderEnabled() const
{
    return  (isValid() ? m_pData->mRelaxedOrder : false);
}


//----------------------------------------------------------------------------
/** @fn     MicPciConfigInfo::AspmState  MicPciConfigInfo::aspmControlState() const
 *  @return ASPM control state
 *
 *  Returns the ASPM control state.
 */

MicPciConfigInfo::AspmState  MicPciConfigInfo::aspmControlState() const
{
    if (!isValid())
        return  MicPciConfigInfo::eUnknown;

    switch (m_pData->mAspmControl & 0x3)
    {
      case  0:
        return  MicPciConfigInfo::eL0sL1Disabled;

      case  1:
        return  MicPciConfigInfo::eL0sEnabled;

      case  2:
        return  MicPciConfigInfo::eL1Enabled;

      case  3:
        return  MicPciConfigInfo::eL0sL1Enabled;

      default:
        break;
    }

    return  MicPciConfigInfo::eUnknown;
}


//----------------------------------------------------------------------------
/** @fn     PciAddress  MicPciConfigInfo::address() const
 *  @return PCIe address
 *
 *  Returns the PCIe address.
 */

PciAddress  MicPciConfigInfo::address() const
{
    return  (isValid() ? m_pData->mPciAddress : PciAddress());
}


//----------------------------------------------------------------------------
/** @fn     uint16_t  MicPciConfigInfo::vendorId() const
 *  @return vendor ID
 *
 *  Returns the vendor ID.
 */

uint16_t  MicPciConfigInfo::vendorId() const
{
    return  (isValid() ? m_pData->mVendorId : 0);
}


//----------------------------------------------------------------------------
/** @fn     uint16_t  MicPciConfigInfo::deviceId() const
 *  @return device ID
 *
 *  Returns the device ID.
 */

uint16_t  MicPciConfigInfo::deviceId() const
{
    return  (isValid() ? m_pData->mDeviceId : 0);
}


//----------------------------------------------------------------------------
/** @fn     uint8_t  MicPciConfigInfo::revisionId() const
 *  @return revision ID
 *
 *  Returns the revision ID.
 */

uint8_t  MicPciConfigInfo::revisionId() const
{
    return  (isValid() ? m_pData->mRevisionId : 0);
}


//----------------------------------------------------------------------------
/** @fn     uint16_t  MicPciConfigInfo::subsystemId() const
 *  @return subsystem ID
 *
 *  Returns the subsystem ID.
 */

uint16_t  MicPciConfigInfo::subsystemId() const
{
    return  (isValid() ? m_pData->mSubsystemId : 0);
}


//----------------------------------------------------------------------------
/** @fn     MicSpeed  MicPciConfigInfo::linkSpeed() const
 *  @return link speed
 *
 *  Returns the link speed. The base unit is transactions per second.
 *
 *  \b Note: This function only returns valid data if the caller has root or
 *  administrator rights. Please use hasFullAccess() to determine if this
 *  function would return valid data.
 */

MicSpeed  MicPciConfigInfo::linkSpeed() const
{
    /// \todo This does not seem to return the right value (2 vs 5).
    return  (isValid() ? m_pData->mLinkSpeed : MicSpeed());
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicPciConfigInfo::linkWidth() const
 *  @return link width
 *
 *  Returns the link width.
 *
 *  \b Note: This function only returns valid data if the caller has root or
 *  administrator rights. Please use hasFullAccess() to determine if this
 *  function would return valid data.
 */

uint32_t  MicPciConfigInfo::linkWidth() const
{
    return  (isValid() ? m_pData->mLinkWidth : 0);
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicPciConfigInfo::payloadSize() const
 *  @return payload size
 *
 *  Returns the payload size in bytes.
 *
 *  \b Note: This function only returns valid data if the caller has root or
 *  administrator rights. Please use hasFullAccess() to determine if this
 *  function would return valid data.
 */

uint32_t  MicPciConfigInfo::payloadSize() const
{
    return  (isValid() ? m_pData->mPayloadSize : 0);
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicPciConfigInfo::requestSize() const
 *  @return request size
 *
 *  Returns the read request size in bytes.
 *
 *  \b Note: This function only returns valid data if the caller has root or
 *  administrator rights. Please use hasFullAccess() to determine if this
 *  function would return valid data.
 */

uint32_t  MicPciConfigInfo::requestSize() const
{
    return  (isValid() ? m_pData->mRequestSize : 0);
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  MicPciConfigInfo::classCode() const
 *  @return class code
 *
 *  Returns the class code.
 */

uint32_t  MicPciConfigInfo::classCode() const
{
    return  (isValid() ? m_pData->mClassCode : 0);
}


//----------------------------------------------------------------------------
/** @fn     void  MicPciConfigInfo::clear()
 *
 *  Clear (invalidate) the PCI config info data.
 */

void  MicPciConfigInfo::clear()
{
    m_pData->clear();
}


//============================================================================
//  P U B L I C   S T A T I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     std::string  MicPciConfigInfo::aspmStateAsString( AspmState state )
 *  @param  state   ASPM state
 *  @return state as string
 *
 *  Returns specified ASPM \a state as string.
 */

std::string  MicPciConfigInfo::aspmStateAsString( AspmState state )
{
    switch (state)
    {
      case  eUnknown:
        return  "unknown";

      case  eL0sL1Disabled:
        return  "Disabled";

      case  eL0sEnabled:
        return  "L0s";

      case  eL1Enabled:
        return  "L1";

      case  eL0sL1Enabled:
        return  "L0s & L1";

      default:
        break;
    }

    return  "invalid";
}


