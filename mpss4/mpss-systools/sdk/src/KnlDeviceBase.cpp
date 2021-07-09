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
#include    "KnlDeviceBase.hpp"
#include    "MicDeviceError.hpp"

// LOCAL CONSTANTS
//
namespace  {
const char* const  DEVICE_TYPE_STR_KNL = "x200";

enum  SmcAccessMode
{
    DISABLE = micmgmt::MicDeviceImpl::eNoAccess,
    RO_MODE = micmgmt::MicDeviceImpl::eReadOnly,
    WO_MODE = micmgmt::MicDeviceImpl::eWriteOnly,
    RW_MODE = micmgmt::MicDeviceImpl::eReadWrite
};

struct SmcRegisterInfo
{
    uint8_t        offset;
    int            nbytes;
    SmcAccessMode  access;
};

SmcRegisterInfo  SMC_REGISTER_INFO[] = { /// \todo Double check access modes (current KNL SMAS 0.75)
//                                       { 0x07,  4, RO_MODE },   // PCI MBus Manageability Address
                                       { 0x10, 16, RO_MODE },   // UUID
                                       { 0x11,  4, RO_MODE },   // SMC Firmware Version
                                       { 0x12,  4, RO_MODE },   // SMC Exe Domain / Progress Code
                                       { 0x13,  4, RO_MODE },   // SMC Self-Test Results
                                       { 0x14,  4, RO_MODE },   // SMC Hardware Revision
                                       { 0x15, 12, RO_MODE },   // Unique serial number
                                       { 0x16,  4, RO_MODE },   // Boot firmware version
//                                       { 0x17,  4, WO_MODE },   // Restart SMBus addr negotiation
                                       { 0x18, 16, RO_MODE },   // Part number
                                       { 0x19,  6, RO_MODE },   // Manufacture date
                                       { 0x1a,  4, RW_MODE },   // POST Register
//                                       { 0x1b,  4, RW_MODE },   // Zombie mode enabled
                                       { 0x1c,  4, RO_MODE },   // CPU Identifier
                                       { 0x1d,  1, RW_MODE },   // NTB write enabled
                                       { 0x1e,  4, RO_MODE },   // Card TDP
                                       { 0x20,  1, RW_MODE },   // SEL Entry Selection Register, variable size, at least one byte
                                       { 0x21,  4, RO_MODE },   // SEL Data register
                                       { 0x22,  4, RW_MODE },   // SDR Entry Selection Register
                                       { 0x23,  4, RO_MODE },   // SDR Data register
                                       { 0x28,  4, RO_MODE },   // PCIe Power Reading
                                       { 0x29,  4, RO_MODE },   // 2x3 Power Reading
                                       { 0x2a,  4, RO_MODE },   // 2x4 Power Reading
                                       { 0x2b,  4, RW_MODE },   // Forced Throttle
                                       { 0x35,  4, RO_MODE },   // Average Power 0
//                                       { 0x36,  4, RO_MODE },   // Average Power 1
                                       { 0x3a,  4, RO_MODE },   // Instantaneous Power Reading
                                       { 0x3b,  4, RO_MODE },   // Maximum Observed Instantaneous Power Reading
                                       { 0x40,  4, RO_MODE },   // CPU DIE Temperature
                                       { 0x41,  4, RO_MODE },   // Card Exhaust Temperature
                                       { 0x43,  4, RO_MODE },   // VCCP VR Temperature
                                       { 0x44,  4, RO_MODE },   // VCCCLR VR Temperature
                                       { 0x45,  4, RO_MODE },   // VCCCMP VR Temperature
                                       { 0x47,  4, RO_MODE },   // West Temperature
                                       { 0x48,  4, RO_MODE },   // East Temperature
                                       { 0x49,  4, RO_MODE },   // Fan RPM
                                       { 0x4a,  4, RO_MODE },   // Fan PWM Percent
                                       { 0x4b,  4, RW_MODE },   // Fan PWM Adder
                                       { 0x4c,  4, RO_MODE },   // KNL Tcritical temperature
                                       { 0x4d,  4, RO_MODE },   // KNL Tcontrol temperature
                                       { 0x4e,  4, RO_MODE },   // WEST temperature
                                       { 0x4f,  4, RO_MODE },   // NTB temperature
                                       { 0x50,  4, RO_MODE },   // VCCP VR Output Voltage
                                       { 0x51,  4, RO_MODE },   // VCCU VR Output Voltage
                                       { 0x52,  4, RO_MODE },   // VCCCLR VR Output Voltage
                                       { 0x53,  4, RO_MODE },   // VCCMLB VR Output Voltage
                                       { 0x56,  4, RO_MODE },   // VCCMP Voltage
                                       { 0x57,  4, RO_MODE },   // NTB1 Voltage
                                       { 0x58,  4, RO_MODE },   // VCCPIO Voltage
                                       { 0x59,  4, RO_MODE },   // VCCSFR Voltage
                                       { 0x5a,  4, RO_MODE },   // PCH Voltage
                                       { 0x5b,  4, RO_MODE },   // VCCMFUSE Voltage
                                       { 0x5c,  4, RO_MODE },   // NTB2 Voltage
                                       { 0x5d,  4, RO_MODE },   // VPP Voltage
                                       { 0x60,  4, RW_MODE },   // LED blink code
                                       { 0x70,  4, RO_MODE },   // VCCP VR Output Power
                                       { 0x71,  4, RO_MODE },   // VCCU VR Output Power
                                       { 0x72,  4, RO_MODE },   // VCCCLR VR Output Power
                                       { 0x73,  4, RO_MODE },   // VCCMLB VR Output Power
                                       { 0x76,  4, RO_MODE },   // VCCMP VR Output Power
                                       { 0x77,  4, RO_MODE }    // NTB1 VR Output Power
};

}

// NAMESPACES
//
using namespace  micmgmt;
using namespace  std;


//============================================================================
/** @class    micmgmt::KnlDeviceBase      KnlDeviceBase.hpp
 *  @ingroup  sdk
 *  @brief    The class represents a KNL device base class
 *
 *  The \b %KnlDeviceBase class encapsulates KNL device common functionality.
 *
 *  The %KnlDeviceBase is a specialization of MicDevice that provides common
 *  functions for both real and mock KNL devices.
 */


//============================================================================
//  P U B L I C   C O N S T A N T S
//============================================================================

//----------------------------------------------------------------------------
/** @enum   micmgmt::KnlDeviceBase::SmBusAddress
 *
 *  Enumerations of SMBus base addresses.
 */


//----------------------------------------------------------------------------
/** @enum   micmgmt::KnlDeviceBase::eSmBusAddress_0x30
 *
 *  SMBus base address of 0x30.
 */


//----------------------------------------------------------------------------
/** @enum   micmgmt::KnlDeviceBase::eSmBusAddress_0x40
 *
 *  SMBus base address of 0x40.
 */


//============================================================================
//  P R O T E C T E D   C O N S T A N T S
//============================================================================

//----------------------------------------------------------------------------
/** @enum   micmgmt::KnlDeviceBase::FlashStage
 *
 *  Enumerations of flash stages.
 */


//----------------------------------------------------------------------------
/** @enum   micmgmt::KnlDeviceBase::TempSensor
 *
 *  Enumerations of available temperature sensors.
 */


//----------------------------------------------------------------------------
/** @enum   micmgmt::KnlDeviceBase::VoltSensor
 *
 *  Enumerations of voltage sensors.
 */


//----------------------------------------------------------------------------
/** @enum   micmgmt::KnlDeviceBase::PowerSensor
 *
 *  Enumerations of power sensors.
 */


//============================================================================
//  P U B L I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     KnlDeviceBase::~KnlDeviceBase()
 *
 *  Cleanup.
 */

KnlDeviceBase::~KnlDeviceBase()
{
    // Nothing to do
}


//============================================================================
//  P R O T E C T E D   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     KnlDeviceBase::KnlDeviceBase( int number )
 *  @param  number  Device number
 *
 *  Construct a KNL device base object with given device \a number.
 */

KnlDeviceBase::KnlDeviceBase( int number ) :
    MicDeviceImpl( number )
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     std::string  KnlDeviceBase::deviceType() const
 *  @return device type
 *
 *  Returns the card device type string.
 */

std::string  KnlDeviceBase::deviceType() const
{
    return  string( DEVICE_TYPE_STR_KNL );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlDeviceBase::getDeviceMaintenanceMode( bool* enabled, bool* available ) const
 *  @param  enabled    Enabled state
 *  @param  available  Available state (optional)
 *  @return error code
 *
 *  Returns device maintenance mode enabled and availability state.
 *
 *  This function will always return a not supported error, because the
 *  maintenance mode does not exists for KNL.
 *
 *  This virtual function may be reimplemented by deriving classes.
 *  This implementation always returns MICSDKERR_NOT_SUPPORTED.
 *
 *  Possible return codes:
 *  - MICSDKERR_NOT_SUPPORTED
 */

uint32_t  KnlDeviceBase::getDeviceMaintenanceMode( bool* enabled, bool* available ) const
{
    if (enabled)
        *enabled = false;

    if (available)
        *available = false;

    return  MicDeviceError::errorCode( MICSDKERR_NOT_SUPPORTED );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlDeviceBase::getDevicePowerStates( std::list<MicPowerState>* states ) const
 *  @param  states  Pointer to power states return
 *  @return error code
 *
 *  Retrieve and return power states.
 *
 *  KNL does not support online management of power states. Therefore, this
 *  function will always return MICSDKERR_NOT_SUPPORTED.
 *
 *  Possible return codes:
 *  - MICSDKERR_NOT_SUPPORTED
 */

uint32_t  KnlDeviceBase::getDevicePowerStates( std::list<MicPowerState>* states ) const
{
    (void) states;

    return  MicDeviceError::errorCode( MICSDKERR_NOT_SUPPORTED );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlDeviceBase::getDeviceSmcPersistenceState( bool* enabled, bool* available ) const
 *  @param  enabled    Pointer to enabled flag return
 *  @param  available  Pointer to available flag return (optional)
 *  @return error code
 *
 *  Check and return the SMC persistence state.
 *
 *  KNL does not support persistence state.
 *
 *  Possible return codes:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_NOT_SUPPORTED
 */

uint32_t  KnlDeviceBase::getDeviceSmcPersistenceState( bool* enabled, bool* available ) const
{
    if (!enabled)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    *enabled = false;

    if (available)
        *available = false;

    return  MicDeviceError::errorCode( MICSDKERR_NOT_SUPPORTED );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlDeviceBase::getDeviceFlashDeviceInfo( FlashDeviceInfo* info ) const
 *  @param  info    Pointer to device info return object
 *  @return error code
 *
 *  Retrieves and returns flash device \a info.
 *
 *  This function will always return a not supported error, because the
 *  maintenance mode does not exists for KNL.
 *
 *  Possible return codes:
 *  - MICSDKERR_NOT_SUPPORTED
 */

uint32_t  KnlDeviceBase::getDeviceFlashDeviceInfo( FlashDeviceInfo* info ) const
{
    (void) info;

    return  MicDeviceError::errorCode( MICSDKERR_NOT_SUPPORTED );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlDeviceBase::setDeviceEccModeEnabled( bool state, int timeout, int* stage )
 *  @param  state    Enabled state
 *  @param  timeout  Wait timeout in milliseconds (optional)
 *  @param  stage    Pointer to stage return (optional)
 *  @return error code
 *
 *  Enable or disable ECC mode.
 *
 *  This virtual function may be reimplemented by deriving classes.
 *  The default implementation always return MICSDKERR_NOT_SUPPORTED.
 *
 *  Possible return codes:
 *  - MICSDKERR_NOT_SUPPORTED
 */

uint32_t  KnlDeviceBase::setDeviceEccModeEnabled( bool state, int timeout, int* stage )
{
    (void) state;
    (void) timeout;
    (void) stage;

    return  MicDeviceError::errorCode( MICSDKERR_NOT_SUPPORTED );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlDeviceBase::setDeviceMaintenanceModeEnabled( bool state, int timeout )
 *  @param  state   Enabled state
 *  @param  timeout  Wait timeout in milliseconds (optional)
 *  @return error code
 *
 *  Enable or disable maintenance mode.
 *
 *  This function will always return a not supported error, because the
 *  maintenance mode does not exists for KNL.
 *
 *  Possible return codes:
 *  - MICSDKERR_NOT_SUPPORTED
 */

uint32_t  KnlDeviceBase::setDeviceMaintenanceModeEnabled( bool state, int timeout )
{
    (void) state;
    (void) timeout;

    return  MicDeviceError::errorCode( MICSDKERR_NOT_SUPPORTED );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlDeviceBase::setDevicePowerStates( const std::list<MicPowerState>& states )
 *  @param  states  List of power states
 *  @return error code
 *
 *  Set specified power states.
 *
 *  This function will always return a not supported error, because the
 *  power states cannot be set for KNL.
 *
 *  Possible return codes:
 *  - MICSDKERR_NOT_SUPPORTED
 */

uint32_t  KnlDeviceBase::setDevicePowerStates( const std::list<MicPowerState>& states )
{
    (void) states;

    return  MicDeviceError::errorCode( MICSDKERR_NOT_SUPPORTED );
}

//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlDeviceBase::setDevicePowerThreshold( MicDevice::PowerWindow window, uint16_t power, uint16_t time )
 *  @param  window       The Power window for the coprocessor.
 *  @param  power        The power limit to set for the coprocessor.
 *  @param  time_window  The time window to set for the coprocessor.
 *  @return error code
 *
 *  Set specified power states.
 *
 *  This function will always return a not supported error, because the
 *  set power threshold using 16 bit integers is used for KNC devices only.
 *  For KNL devices use the function with uint32_t values.
 *
 *  Possible return codes:
 *  - MICSDKERR_NOT_SUPPORTED
 */

uint32_t  KnlDeviceBase::setDevicePowerThreshold( MicDevice::PowerWindow window, uint16_t power, uint16_t time )
{
    (void) window;
    (void) power;
    (void) time;

    return  MicDeviceError::errorCode( MICSDKERR_NOT_SUPPORTED );
}



//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlDeviceBase::setDeviceSmcPersistenceEnabled( bool state )
 *  @param  state   Enabled state
 *  @return error code
 *
 *  Enable or disable SMC persistence. When enabled, the SMC settings will
 *  survive over a boot sequence.
 *
 *  KNL does not support SMC persistence, so this function always returns
 *  not supported.
 *
 *  Possible return codes:
 *  - MICSDKERR_NOT_SUPPORTED
 */

uint32_t  KnlDeviceBase::setDeviceSmcPersistenceEnabled( bool state )
{
    (void) state;

    return  MicDeviceError::errorCode( MICSDKERR_NOT_SUPPORTED );
}


//============================================================================
//  P R O T E C T E D   S T A T I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     std::string  KnlDeviceBase::flashStageAsText( int stage )
 *  @param  stage   Stage level
 *  @return stage level string
 *
 *  Returns the specified \a stage level as string.
 */

std::string  KnlDeviceBase::flashStageAsText( int stage )
{
    switch (stage)
    {
      case  eIdleStage:
        return  std::string( "Idle" );

      case  eBiosStage:
        return  std::string( "BIOS" );

      case  eMeStage:
        return  std::string( "ME" );

      case  eMcdramStage:
        return  std::string( "MCDRAM Controller" );

      case  eSmcStage:
        return  std::string( "SMC" );

      case  ePlxStage:
        return  std::string( "NTB EEPROM" );

      default:
        break;
    }

    return  std::string( "Unknown" );
}


//----------------------------------------------------------------------------
/** @fn     std::string  KnlDeviceBase::tempSensorAsText( TempSensor sensor )
 *  @param  sensor  Sensor type
 *  @return sensor name string
 *
 *  Returns the specified \a sensor type as string.
 */

std::string  KnlDeviceBase::tempSensorAsText( TempSensor sensor )
{
    switch (sensor)
    {
      case  eCpuTemp:
        return  std::string( "Die" );

      case  eExhaustTemp:
        return  std::string( "Fan Exhaust" );

      case  eVccpTemp:
        return  std::string( "VCCP" );

      case  eVccclrTemp:
        return  std::string( "VCCCLR" );

      case  eVccmpTemp:
        return  std::string( "VCCMP" );

      case  eWestTemp:
        return  std::string( "West" );

      case  eEastTemp:
        return  std::string( "East" );

      default:
        break;
    }

    return  std::string( "Unknown" );
}


//----------------------------------------------------------------------------
/** @fn     std::string  KnlDeviceBase::voltSensorAsText( VoltSensor sensor )
 *  @param  sensor  Sensor type
 *  @return sensor name string
 *
 *  Returns the specified \a sensor type as string.
 */

std::string  KnlDeviceBase::voltSensorAsText( VoltSensor sensor )
{
    switch (sensor)
    {
      case  eVccpVolt:
        return  std::string( "VCCP" );

      case  eVccuVolt:
        return  std::string( "VCCU" );

      case  eVccclrVolt:
        return  std::string( "VCCCLR" );

      case  eVccmlbVolt:
        return  std::string( "VCCMLB" );

      case  eVccmpVolt:
        return  std::string( "VCCMP" );

      case  eNtb1Volt:
        return  std::string( "NTB1" );

      case  eVccpioVolt:
        return  std::string( "VCCPIO" );

      case  eVccsfrVolt:
        return  std::string( "VCCSFR" );

      case  ePchVolt:
        return  std::string( "PCH" );

      case  eVccmfuseVolt:
        return  std::string( "VCCMFUSE" );

      case  eNtb2Volt:
        return  std::string( "NTB2" );

      case  eVppVolt:
        return  std::string( "VPP" );

      default:
        break;
    }

    return  std::string( "Unknown" );
}


//----------------------------------------------------------------------------
/** @fn     std::string  KnlDeviceBase::powerSensorAsText( PowerSensor sensor )
 *  @param  sensor  Sensor type
 *  @return sensor name string
 *
 *  Returns the specified \a sensor type as string.
 */

std::string  KnlDeviceBase::powerSensorAsText( PowerSensor sensor )
{
    switch (sensor)
    {
      case  ePciePower:
        return  std::string( "PCIe" );

      case  e2x3Power:
        return  std::string( "2x3" );

      case  e2x4Power:
        return  std::string( "2x4" );

      case  eAvg0Power:
        return  std::string( "Average 0" );

      case  eCurrentPower:
        return  std::string( "Current" );

      case  eMaximumPower:
        return  std::string( "Maximum" );

      case  eVccpPower:
        return  std::string( "VCCP" );

      case  eVccuPower:
        return  std::string( "VCCU" );

      case  eVccclrPower:
        return  std::string( "VCCCLR" );

      case  eVccmlbPower:
        return  std::string( "VCCMLB" );

      case  eVccmpPower:
        return  std::string( "VCCMP" );

      case  eNtb1Power:
        return  std::string( "NTB1" );

      default:
        break;
    }

    return  std::string( "Unknown" );
}


//----------------------------------------------------------------------------
/** @fn     std::string  KnlDeviceBase::flashErrorAsText( int stage, int code )
 *  @param  stage   Stage level
 *  @param  code    Error code
 *  @return flash error string
 *
 *  Returns the flash error string associated with specified error \a code at
 *  given flash \a stage level.
 */

std::string  KnlDeviceBase::flashErrorAsText( int stage, int code )
{
    /// \todo Define and implement

    (void) stage;
    (void) code;

    return  "Unknown error";
}


//----------------------------------------------------------------------------
/** @fn     int  KnlDeviceBase::smcRegisterSize( uint8_t offset, AccessMode* access )
 *  @param  offset  Register offset
 *  @param  access  Register access mode return (optional)
 *  @return byte count
 *
 *  Returns the byte count for SMC address at specified \a offset. If no such
 *  register exists, -1 is returned. If the register has no fixed byte count
 *  defined, 0 is returned.
 *
 *  Optionally, a pointer to a user \a access variable can be passed in as
 *  parameter, which will in that case return the access mode of the register.
 */

int  KnlDeviceBase::smcRegisterSize( uint8_t offset, AccessMode* access ) const
{
    const size_t  regcount = sizeof( SMC_REGISTER_INFO ) / sizeof( *SMC_REGISTER_INFO );

    for (size_t i=0; i<regcount; i++)
    {
        if (SMC_REGISTER_INFO[i].offset == offset)
        {
            if (access)
                *access = static_cast<AccessMode>( SMC_REGISTER_INFO[i].access );

            return  SMC_REGISTER_INFO[i].nbytes;
        }
    }

    return  -1;
}

