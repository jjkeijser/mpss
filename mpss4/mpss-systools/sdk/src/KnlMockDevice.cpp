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
#include    "KnlMockDevice.hpp"
#include    "micmgmtCommon.hpp"
#include    "MicBootConfigInfo.hpp"
#include    "MicProcessorInfo.hpp"
#include    "MicPciConfigInfo.hpp"
#include    "MicMemoryInfo.hpp"
#include    "MicCoreInfo.hpp"
#include    "MicPlatformInfo.hpp"
#include    "MicThermalInfo.hpp"
#include    "MicVoltageInfo.hpp"
#include    "MicCoreUsageInfo.hpp"
#include    "MicPowerUsageInfo.hpp"
#include    "MicPowerThresholdInfo.hpp"
#include    "MicMemoryUsageInfo.hpp"
#include    "MicVersionInfo.hpp"
#include    "MicDeviceError.hpp"
#include    "FlashDeviceInfo.hpp"
#include    "FlashStatus.hpp"
#include    "FlashImage.hpp"
#include    "ThrottleInfo.hpp"
#include    "PciAddress.hpp"
#include    "MicVoltage.hpp"
#include    "MicPower.hpp"
#include    "MicDeviceInfo.hpp"
#include    "MsTimer.hpp"
#include    "MicLogger.hpp"
//
#include    "ProcessorInfoData_p.hpp"   // Private
#include    "PciConfigData_p.hpp"
#include    "MemoryInfoData_p.hpp"
#include    "CoreInfoData_p.hpp"
#include    "PlatformInfoData_p.hpp"
#include    "ThermalInfoData_p.hpp"
#include    "VoltageInfoData_p.hpp"
#include    "PowerUsageData_p.hpp"
#include    "MemoryUsageData_p.hpp"
#include    "VersionInfoData_p.hpp"
#include    "MpssStackBase.hpp"
#include    "PowerThresholdData_p.hpp"

// SYSTEM INCLUDES
//
#include    <cstring>

// LOCAL CONSTANTS
//
namespace  {
const uint32_t    MEMORY_DENSITY = 2048;
const uint32_t    MEMORY_SIZE    = 4 * MEMORY_DENSITY;
const char* const UUID           = "01020304-0506-0708-090A-0B0C0D0E0F10";
const size_t      CORE_COUNT     = 62;
const size_t      CORE_THREADS   = 4;

const char* const  NOT_SUPPORTED = "n/a";
}

// NAMESPACES
//
using namespace  micmgmt;
using namespace  std;


//============================================================================
/** @class    micmgmt::KnlMockDevice      KnlMockDevice.hpp
 *  @ingroup  sdk
 *  @brief    The class represents a KNL mock device
 *
 *  The \b %KnlMockDevice class encapsulates a KNL mock device.
 */


//============================================================================
//  P U B L I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     KnlMockDevice::KnlMockDevice( int number )
 *  @param  number  Device number
 *
 *  Construct a KNL mock device with given device \a number.
 *
 *  The %KncMockDevice provides a fake KNC device implementation. The default
 *  implementation will represent a KNC device in the online state. The device
 *  will return predefined configuration and status information. This data and
 *  the functional behavior are more or less static, with few exceptions.
 *
 *  If the moc device should return different data or different error codes,
 *  a specialized class should be created that derives from this class.
 *  If only different data or dynamic data should be returned on all data
 *  queries, it may be sufficient to set the corresponding data members. In
 *  case individual member functions should behave differently, those functions
 *  can be reimplemented.
 */

KnlMockDevice::KnlMockDevice( int number ) :
    KnlDeviceBase( number ),
    m_pProcessorInfoData( new ProcessorInfoData ),
    m_pPciConfigInfoData( new PciConfigData ),
    m_pPlatformInfoData( new PlatformInfoData ),
    m_pPowerThresholdData( new PowerThresholdData ),
    m_pVersionInfoData( new VersionInfoData ),
    m_pMemoryInfoData( new MemoryInfoData ),
    m_pCoreInfoData( new CoreInfoData ),
    m_pThermalInfoData( new ThermalInfoData ),
    m_pVoltageInfoData( new VoltageInfoData ),
    m_pPowerUsageData( new PowerUsageData ),
    m_pMemoryUsageData( new MemoryUsageData ),
    m_pCoreUsageInfo( new MicCoreUsageInfo ),
    m_pFlashStatus( new FlashStatus )
{
    mockReset();
}


//----------------------------------------------------------------------------
/** @fn     KnlMockDevice::~KnlMockDevice()
 *
 *  Cleanup.
 */

KnlMockDevice::~KnlMockDevice()
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     void  KnlMockDevice::mockReset()
 *
 *  Reset all mocked data to default values.
 */

void  KnlMockDevice::mockReset()
{
    m_DeviceState    = MicDevice::eOnline;
    m_LedMode        = 0;
    m_DevOpen        = false;
    m_IsAdmin        = micmgmt::isAdministrator();
    m_TurboAvailable = true;
    m_TurboEnabled   = false;
    m_TurboActive    = false;
    m_SmcPersist     = false;
    m_EccMode        = false;

    // Prepare fake processor info data
    m_pProcessorInfoData->mType            = 0x00;
    m_pProcessorInfoData->mModel           = 0x01;
    m_pProcessorInfoData->mFamily          = 0x0b;
    m_pProcessorInfoData->mExtendedModel   = 0x00;
    m_pProcessorInfoData->mExtendedFamily  = 0x00;
    m_pProcessorInfoData->mSteppingData    = 0xb0;
    m_pProcessorInfoData->mSubsteppingData = 0x00;
    m_pProcessorInfoData->mStepping        = "B0";
    m_pProcessorInfoData->mSku             = "B0PO-SKU1";

    // Prepare fake PCI config data
    m_pPciConfigInfoData->mClassCode   = 0xb4000;
    m_pPciConfigInfoData->mPciAddress  = PciAddress( 0, deviceNum() ).asString();
    m_pPciConfigInfoData->mVendorId    = 0x8086;
    m_pPciConfigInfoData->mDeviceId    = 0x2258;
    m_pPciConfigInfoData->mRevisionId  = 0;
    m_pPciConfigInfoData->mSubsystemId = 0x2500;
    m_pPciConfigInfoData->mLinkSpeed   = m_IsAdmin ? 8 : 0;
    m_pPciConfigInfoData->mLinkWidth   = m_IsAdmin ? 8 : 0;
    m_pPciConfigInfoData->mPayloadSize = m_IsAdmin ? 8 : 0;
    m_pPciConfigInfoData->mRequestSize = m_IsAdmin ? 8 : 0;
    m_pPciConfigInfoData->mHasAccess   = m_IsAdmin;
    m_pPciConfigInfoData->mValid       = true;

    // Power threshold data
    m_pPowerThresholdData->mMaximum          = 300;
    m_pPowerThresholdData->mLoThreshold      = 275;
    m_pPowerThresholdData->mHiThreshold      = 300;
    m_pPowerThresholdData->mWindow0Threshold = 265;
    m_pPowerThresholdData->mWindow1Threshold = 265;
    m_pPowerThresholdData->mWindow0Time      =  50;    // ms
    m_pPowerThresholdData->mWindow1Time      = 300;    // ms
    m_pPowerThresholdData->mPersistence      = false;
    m_pPowerThresholdData->mValid            = true;

    // Prepare fake version data
    m_pVersionInfoData->mFlashVersion   = NOT_SUPPORTED;
    m_pVersionInfoData->mBiosVersion    = "1.2.3.4";
    m_pVersionInfoData->mBiosRelDate    = "2014-12-05";
    m_pVersionInfoData->mPlxVersion     = "2.3.4.5";
    m_pVersionInfoData->mOemStrings     = "mOemStrings";
    m_pVersionInfoData->mSmcVersion     = "3.4.5.6";
    m_pVersionInfoData->mSmcBootVersion = NOT_SUPPORTED;
    m_pVersionInfoData->mRamCtrlVersion = "5.6.7.8";
    m_pVersionInfoData->mMeVersion      = "6.7.8.9";
    m_pVersionInfoData->mMpssVersion    = "4.0.0";
    m_pVersionInfoData->mDriverVersion  = "4.0.0-driver";
    m_pVersionInfoData->mFabVersion     = "0";
    m_pVersionInfoData->mPlxVersion     = "01020003";
//    m_pData->mpMpssStack->getSystemMpssVersion( &data.mMpssVersion );
//    m_pData->mpMpssStack->getSystemDriverVersion( &data.mDriverVersion );

    // Prepare fake memory info data
    m_pMemoryInfoData->mVendorName = "Elpida";
    m_pMemoryInfoData->mMemoryType = "DRAM";
    m_pMemoryInfoData->mTechnology = "MCDRAM";
    m_pMemoryInfoData->mRevision   = 0x1;
    m_pMemoryInfoData->mDensity    = MicMemory( MEMORY_DENSITY, MicMemory::eMega );
    m_pMemoryInfoData->mSize       = MicMemory( MEMORY_SIZE, MicMemory::eMega );
    m_pMemoryInfoData->mSpeed      = MicSpeed( 5500, MicSpeed::eMega );
    m_pMemoryInfoData->mFrequency  = MicFrequency( 2750000, MicFrequency::eKilo );
    m_pMemoryInfoData->mEcc        = false;
    m_pMemoryInfoData->mValid      = true;

    // Prepare fake core info data
    m_pCoreInfoData->mCoreCount       = CORE_COUNT;
    m_pCoreInfoData->mCoreThreadCount = CORE_THREADS;
    m_pCoreInfoData->mFrequency       = MicFrequency( 1, MicFrequency::eGiga );
    m_pCoreInfoData->mVoltage         = MicVoltage( 997000, MicVoltage::eMicro );

    // Prepare fake platform info data
    m_pPlatformInfoData->mPartNumber        = "000000000000000";   // 15 bytes
    m_pPlatformInfoData->mSerialNo          = "00000000001";       // 11 bytes
    m_pPlatformInfoData->mFeatureSet        = "300W Active NCS";
    m_pPlatformInfoData->mCoprocessorOs     = "Linux";
    m_pPlatformInfoData->mCoprocessorBrand  = "Intel";
    m_pPlatformInfoData->mBoardSku          = "B0PO-SKU1";
    m_pPlatformInfoData->mBoardType         = "Board type";
    m_pPlatformInfoData->mStrapInfo         = "Active Heatsink, Fab A, RP";
    m_pPlatformInfoData->mStrapInfoRaw      = 0x00000004;
    m_pPlatformInfoData->mSmBusAddress      = 0x30;
    m_pPlatformInfoData->mMaxPower          = MicPower( "TDP", 300 );
    m_pPlatformInfoData->mUuid              = std::string( UUID );
    m_pPlatformInfoData->mCoolActive        = true;
    m_pPlatformInfoData->mManufactDate      = "2014-20-12";
    m_pPlatformInfoData->mManufactTime      = "12:00:00";

    // Prepare initial fake thermal info data
    m_pThermalInfoData->mFanRpm          = 2700;
    m_pThermalInfoData->mFanPwm          = 50;
    m_pThermalInfoData->mFanAdder        = 50;
    m_pThermalInfoData->mControl         = 85;
    m_pThermalInfoData->mCritical        = 95;
    m_pThermalInfoData->mThrottleInfo    = ThrottleInfo( 8, true, 12, 486 );
    m_pThermalInfoData->mSensors.push_back( MicTemperature( tempSensorAsText( eCpuTemp ),    58 ) );
    m_pThermalInfoData->mSensors.push_back( MicTemperature( tempSensorAsText( eExhaustTemp ), 52 ) );
    m_pThermalInfoData->mSensors.push_back( MicTemperature( tempSensorAsText( eVccpTemp ),   60 ) );
    m_pThermalInfoData->mSensors.push_back( MicTemperature( tempSensorAsText( eVccclrTemp ), 58 ) );
    m_pThermalInfoData->mSensors.push_back( MicTemperature( tempSensorAsText( eVccmpTemp ),  55 ) );
    m_pThermalInfoData->mSensors.push_back( MicTemperature( tempSensorAsText( eWestTemp ),   57 ) );
    m_pThermalInfoData->mSensors.push_back( MicTemperature( tempSensorAsText( eEastTemp ),   60 ) );

    /// \todo More sensors
    m_pThermalInfoData->mValid = true;

    // Prepare initial fake voltage info data
    m_pVoltageInfoData->mSensors.push_back( MicVoltage( voltSensorAsText( eVccpVolt ),   997, MicVoltage::eMilli ) );
    m_pVoltageInfoData->mSensors.push_back( MicVoltage( voltSensorAsText( eVccuVolt ),   989, MicVoltage::eMilli ) );
    m_pVoltageInfoData->mSensors.push_back( MicVoltage( voltSensorAsText( eVccclrVolt ), 993, MicVoltage::eMilli ) );
    m_pVoltageInfoData->mValid = true;

    // Prepare initial fake power info data
    m_pPowerUsageData->mThrottle     = MicPower( "Threshold", 190 );
    m_pPowerUsageData->mThrottleInfo = ThrottleInfo( 20, false, 3, 96 );
    m_pPowerUsageData->mSensors.push_back( MicPower( powerSensorAsText( ePciePower ), 153 ) );
    m_pPowerUsageData->mSensors.push_back( MicPower( powerSensorAsText( e2x3Power ),   67 ) );
    m_pPowerUsageData->mSensors.push_back( MicPower( powerSensorAsText( e2x4Power ),   37 ) );
    m_pPowerUsageData->mValid = true;

    // Prepare initial fake memory usage data
    m_pMemoryUsageData->mTotal     = MicMemory( MEMORY_SIZE >> 2, MicMemory::eMega );
    m_pMemoryUsageData->mUsed      = MicMemory( MEMORY_SIZE >> 3, MicMemory::eMega );
    m_pMemoryUsageData->mFree      = MicMemory( MEMORY_SIZE >> 4, MicMemory::eMega );
    m_pMemoryUsageData->mBuffers   = MicMemory( MEMORY_SIZE >> 6, MicMemory::eMega );
    m_pMemoryUsageData->mCached    = MicMemory( MEMORY_SIZE >> 7, MicMemory::eMega );
    m_pMemoryUsageData->mValid     = true;

    // Prepare initial core usage info
    m_pCoreUsageInfo->setCoreCount( m_pCoreInfoData->mCoreCount.value() );
    m_pCoreUsageInfo->setCoreThreadCount( m_pCoreInfoData->mCoreThreadCount.value() );
    m_pCoreUsageInfo->setTickCount( 0 );
    m_pCoreUsageInfo->setTicksPerSecond( 200 );
    m_pCoreUsageInfo->setCounterTotal( MicCoreUsageInfo::eSystem, 0 );
    m_pCoreUsageInfo->setCounterTotal( MicCoreUsageInfo::eUser, 0 );
    m_pCoreUsageInfo->setCounterTotal( MicCoreUsageInfo::eNice, 0 );
    m_pCoreUsageInfo->setCounterTotal( MicCoreUsageInfo::eIdle, 0 );
}


//----------------------------------------------------------------------------
/** @fn     void  KnlMockDevice::setAdmin( bool state )
 *  @param  state   Admin state
 *
 *  Enable or disable adminstrative permissions.
 */

void  KnlMockDevice::setAdmin( bool state )
{
    m_IsAdmin = state;
}


//----------------------------------------------------------------------------
/** @fn     void  KnlMockDevice::setInitialDeviceState( MicDevice::DeviceState state )
 *  @param  state
 *
 *  Set the initial \a state for this device.
 */

void  KnlMockDevice::setInitialDeviceState( MicDevice::DeviceState state )
{
    m_DeviceState = state;
}


//============================================================================
//  P R O T E C T E D   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     bool  KnlMockDevice::isDeviceOpen() const
 *  @return device open state
 *
 *  Returns \c true if the physical device is open.
 *
 *  This virtual function may be reimplemented by deriving classes.
 */

bool  KnlMockDevice::isDeviceOpen() const
{
    return  m_DevOpen;
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlMockDevice::getDeviceState( MicDevice::DeviceState* state ) const
 *  @param  state   Device state return
 *  @return error code
 *
 *  Retrieve current device \a state.
 *
 *  This virtual function may be reimplemented in deriving classes.
 *  The default implementation return the stored state. If the device is in
 *  reset state, the internal state will transition to ready state as part of
 *  this function call. If the device is in reboot state at the time this
 *  function is called, the device state will automatically transition to the
 *  online state.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 */

uint32_t  KnlMockDevice::getDeviceState( MicDevice::DeviceState* state ) const
{
    if (!state)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    *state = m_DeviceState;

    // In the case of a reset() or boot(), advance the device state.

    if (m_DeviceState == MicDevice::DeviceState::eReset)
        m_DeviceState = MicDevice::DeviceState::eReady;
    else if (m_DeviceState == MicDevice::DeviceState::eReboot)
        m_DeviceState = MicDevice::DeviceState::eOnline;

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlMockDevice::getDevicePciConfigInfo( MicPciConfigInfo* info ) const
 *  @param  info    Pointer to PCI config return
 *  @return error code
 *
 *  Retrieve PCI configuration into specified PCI \a info object.
 *
 *  This virtual function may be reimplemented in deriving classes.
 *  The default implementation returns the stored PCI configuration info.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 */

uint32_t  KnlMockDevice::getDevicePciConfigInfo( MicPciConfigInfo* info ) const
{
    if (!info)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    *info = MicPciConfigInfo( *m_pPciConfigInfoData );

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlMockDevice::getDeviceProcessorInfo( MicProcessorInfo* info ) const
 *  @param  info  Pointer to processor info return
 *  @return error code
 *
 *  Retrieve processor information into specified \a info object.
 *
 *  This virtual function may be reimplemented in deriving classes.
 *  The default implementation returns the stored processor info.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 */

uint32_t  KnlMockDevice::getDeviceProcessorInfo( MicProcessorInfo* info ) const
{
    if (!info)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    *info = MicProcessorInfo( *m_pProcessorInfoData );

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlMockDevice::getDeviceVersionInfo( MicVersionInfo* info ) const
 *  @param  info    Pointer to version return
 *  @return error code
 *
 *  Retrieve and return device version \a info.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 */

uint32_t  KnlMockDevice::getDeviceVersionInfo( MicVersionInfo* info ) const
{
    if (!info)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    *info = MicVersionInfo( *m_pVersionInfoData );

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlMockDevice::getDeviceMemoryInfo( MicMemoryInfo* info ) const
 *  @param  info  Pointer to memory info return
 *  @return error code
 *
 *  Retrieve memory information into specified \a info object.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 */

uint32_t  KnlMockDevice::getDeviceMemoryInfo( MicMemoryInfo* info ) const
{
    if (!info)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    if (isDeviceOpen())
        *info = MicMemoryInfo( *m_pMemoryInfoData );
    else
        info->clear();

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlMockDevice::getDeviceCoreInfo( MicCoreInfo* info ) const
 *  @param  info  Pointer to core info return
 *  @return error code
 *
 *  Retrieve core information into specified \a info object.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 */

uint32_t  KnlMockDevice::getDeviceCoreInfo( MicCoreInfo* info ) const
{
    if (!info)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    if (isDeviceOpen())
        *info = MicCoreInfo( *m_pCoreInfoData );
    else
        info->clear();

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlMockDevice::getDevicePlatformInfo( MicPlatformInfo* info ) const
 *  @param  info    Pointer to SMC info return
 *  @return error code
 *
 *  Retrieve SMC config into specified \a info object.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 */

uint32_t  KnlMockDevice::getDevicePlatformInfo( MicPlatformInfo* info ) const
{
    if (!info)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    if (isDeviceOpen())
        *info = MicPlatformInfo( *m_pPlatformInfoData );
    else
        info->clear();

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlMockDevice::getDeviceThermalInfo( MicThermalInfo* info ) const
 *  @param  info  Pointer to thermal info return
 *  @return error code
 *
 *  Retrieve thermal information into specified \a info object.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 */

uint32_t  KnlMockDevice::getDeviceThermalInfo( MicThermalInfo* info ) const
{
    if (!info)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    if (isDeviceOpen())
        *info = MicThermalInfo( *m_pThermalInfoData );
    else
        *info = MicThermalInfo();

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlMockDevice::getDeviceVoltageInfo( MicVoltageInfo* info ) const
 *  @param  info  Pointer to voltage info return
 *  @return error code
 *
 *  Retrieve voltage information into specified \a info object.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 */

uint32_t  KnlMockDevice::getDeviceVoltageInfo( MicVoltageInfo* info ) const
{
    if (!info)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    if (isDeviceOpen())
        *info = MicVoltageInfo( *m_pVoltageInfoData );
    else
        *info = MicVoltageInfo();

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlMockDevice::getDeviceCoreUsageInfo( MicCoreUsageInfo* info ) const
 *  @param  info  Pointer to core info return
 *  @return error code
 *
 *  Retrieve core information into specified \a info object.
 * *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG

 */

uint32_t  KnlMockDevice::getDeviceCoreUsageInfo( MicCoreUsageInfo* info ) const
{
    if (!info)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    if (!isDeviceOpen())
    {
        info->clear();
        return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
    }

    info->setCoreCount( m_pCoreUsageInfo->coreCount() );
    info->setCoreThreadCount( m_pCoreUsageInfo->coreThreadCount() );
    info->setTickCount( m_pCoreUsageInfo->tickCount() );
    info->setTicksPerSecond( m_pCoreUsageInfo->ticksPerSecond() );
    info->setCounterTotal( MicCoreUsageInfo::eSystem, m_pCoreUsageInfo->counterTotal( MicCoreUsageInfo::eSystem ) );
    info->setCounterTotal( MicCoreUsageInfo::eUser,   m_pCoreUsageInfo->counterTotal( MicCoreUsageInfo::eUser ) );
    info->setCounterTotal( MicCoreUsageInfo::eNice,   m_pCoreUsageInfo->counterTotal( MicCoreUsageInfo::eNice ) );
    info->setCounterTotal( MicCoreUsageInfo::eIdle,   m_pCoreUsageInfo->counterTotal( MicCoreUsageInfo::eIdle ) );
    info->setUsageCount( MicCoreUsageInfo::eSystem,   m_pCoreUsageInfo->usageCounters( MicCoreUsageInfo::eSystem ) );
    info->setUsageCount( MicCoreUsageInfo::eUser,     m_pCoreUsageInfo->usageCounters( MicCoreUsageInfo::eUser ) );
    info->setUsageCount( MicCoreUsageInfo::eNice,     m_pCoreUsageInfo->usageCounters( MicCoreUsageInfo::eNice ) );
    info->setUsageCount( MicCoreUsageInfo::eIdle,     m_pCoreUsageInfo->usageCounters( MicCoreUsageInfo::eIdle ) );
    info->setValid( true );

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlMockDevice::getDevicePowerUsageInfo( MicPowerUsageInfo* info ) const
 *  @param  info  Pointer to power info return
 *  @return error code
 *
 *  Retrieve power information into specified \a info object.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 */

uint32_t  KnlMockDevice::getDevicePowerUsageInfo( MicPowerUsageInfo* info ) const
{
    if (!info)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    if (isDeviceOpen())
        *info = MicPowerUsageInfo( *m_pPowerUsageData );
    else
        *info = MicPowerUsageInfo();

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlMockDevice::getDevicePowerThresholdInfo( MicPowerThresholdInfo* info ) const
 *  @param  info  Pointer to power threshold info return
 *  @return error code
 *
 *  Retrieve power threshold information into specified \a info object.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 */

uint32_t  KnlMockDevice::getDevicePowerThresholdInfo( MicPowerThresholdInfo* info ) const
{
    if (!info)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    if (isDeviceOpen())
        *info = MicPowerThresholdInfo( *m_pPowerThresholdData );
    else
        *info = MicPowerThresholdInfo();

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlMockDevice::getDeviceMemoryUsageInfo( MicMemoryUsageInfo* info ) const
 *  @param  info  Pointer to memory info return
 *  @return error code
 *
 *  Retrieve memory information into specified \a info object.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 */

uint32_t  KnlMockDevice::getDeviceMemoryUsageInfo( MicMemoryUsageInfo* info ) const
{
    if (!info)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    if (isDeviceOpen())
        *info = MicMemoryUsageInfo( *m_pMemoryUsageData );
    else
        *info = MicMemoryUsageInfo();

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlMockDevice::getDevicePostCode( std::string* code ) const
 *  @param  code    Pointer to post code return
 *  @return error code
 *
 *  Retrieve and return the post \a code string. The post code is often
 *  expressed as hexdecimal number, but it can also contain other character
 *  combinations. Therefore, the best practice if to leave the post code as
 *  string value.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 */

uint32_t  KnlMockDevice::getDevicePostCode( std::string* code ) const
{
    if (!code)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    switch (m_DeviceState)
    {
      case  MicDevice::eReady:
      case  MicDevice::eOnline:
        code->assign( "FF" );
        break;

      default:
        code->assign( "00" );   /// \todo How does reality looks like?
        break;
    }

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlMockDevice::getDeviceLedMode( uint32_t* mode ) const
 *  @param  mode    Pointer to LED mode return
 *  @return error code
 *
 *  Retrieve and return the current LED \a mode.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 */

uint32_t  KnlMockDevice::getDeviceLedMode( uint32_t* mode ) const
{
    if (!mode)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    *mode = m_LedMode;

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlMockDevice::getDeviceEccMode( bool* enabled, bool* available ) const
 *  @param  enabled    Enabled state return
 *  @param  available  Available state return (optional)
 *  @return error code
 *
 *  Retrieve and return the current ECC mode enabled state.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 */

uint32_t  KnlMockDevice::getDeviceEccMode( bool* enabled, bool* available ) const
{
    if (!enabled)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    *enabled = m_EccMode;

    if (available)
        *available = true;

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlMockDevice::getDeviceTurboMode( bool* enabled, bool* available, bool* active ) const
 *  @param  enabled    Pointer to enabled state return
 *  @param  available  Pointer to available state return (optional)
 *  @param  active     Pointer to active state return (optional)
 *  @return error code
 *
 *  Determine device turbo mode state.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 */

uint32_t  KnlMockDevice::getDeviceTurboMode( bool* enabled, bool* available, bool* active ) const
{
    if (!enabled)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    *enabled = m_TurboAvailable && m_TurboEnabled;

    if (available)
        *available = m_TurboAvailable;

    if (active)
        *active = m_TurboActive;

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlMockDevice::getSmBusAddressTrainingStatus( bool* busy, int* eta ) const
 *  @param  busy    Pointer to busy return variable
 *  @param  eta     Pointer to estimated time remaining return variable
 *  @return error code
 *
 *  Retrieve and return the SMBus training state and remaining time before
 *  completion.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 */

uint32_t  KnlMockDevice::getDeviceSmBusAddressTrainingStatus( bool* busy, int* eta ) const
{
    if (!busy)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    *busy = false;

    if (eta)
        *eta = 0;

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlMockDevice::getDeviceSmcPersistenceState( bool* enabled, bool* available ) const
 *  @param  enabled    Pointer to enabled flag return
 *  @param  available  Pointer to available flag return (optional)
 *  @return error code
 *
 *  Check and return the SMC persistence state.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 */

uint32_t  KnlMockDevice::getDeviceSmcPersistenceState( bool* enabled, bool* available ) const
{
    if (!enabled)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    *enabled = m_SmcPersist;

    if (available)
        *available = true;

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlMockDevice::getDeviceSmcRegisterData( uint8_t offset, uint8_t* data, size_t* size ) const
 *  @param  offset  Register offset
 *  @param  data    Pointer to data buffer
 *  @param  size    Pointer to data size
 *  @return error code
 *
 *  Read SMC register content from register with given \a offset and return
 *  \a size bytes in specified \a data buffer.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_NOT_SUPPORTED

 */

uint32_t  KnlMockDevice::getDeviceSmcRegisterData( uint8_t offset, uint8_t* data, size_t* size ) const
{
    (void) offset;
    (void) data;
    (void) size;

    /// \todo Come up with some realistic implementation

    return  MicDeviceError::errorCode( MICSDKERR_NOT_SUPPORTED );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlMockDevice::getDeviceFlashUpdateStatus( FlashStatus* status ) const
 *  @param  status  Pointer to status return object
 *  @return error code
 *
 *  Retrieves and returns flash operation \a status.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 */

uint32_t  KnlMockDevice::getDeviceFlashUpdateStatus( FlashStatus* status ) const
{
    if (!status)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    *status = *m_pFlashStatus;

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlMockDevice::deviceOpen()
 *  @return error code
 *
 *  Open device.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  Currently this function never fails.
 */

uint32_t  KnlMockDevice::deviceOpen()
{
    m_DevOpen = true;

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     void  KnlMockDevice::deviceClose()
 *
 *  Close device.
 */

void  KnlMockDevice::deviceClose()
{
    m_DevOpen = false;
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KncMockDevice::deviceBoot( const MicBootConfigInfo& info )
 *  @param  info    Boot configuration
 *  @return error code
 *
 *  Boots the device.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_DEVICE_NOT_READY
 *  - MICSDKERR_NO_ACCESS
 */

uint32_t  KnlMockDevice::deviceBoot( const MicBootConfigInfo& info )
{
    if (m_DeviceState != MicDevice::DeviceState::eReady)
        return MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_READY );

    // Since we don't boot, we ignore the configuration
    (void) info;

    // Our real device calls would fail if not privileged, so...
    if (!m_IsAdmin)
        return MicDeviceError::errorCode( MICSDKERR_NO_ACCESS );

    // Boot our device
    m_DeviceState = MicDevice::DeviceState::eReboot;

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlMockDevice::deviceReset( bool force )
 *  @param  force   Force reset flag
 *  @return error code
 *
 *  Resets the device.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_NO_ACCESS
 */

uint32_t  KnlMockDevice::deviceReset( bool force )
{
    // Our real device calls would fail if not privileged, so...
    if (!m_IsAdmin)
        return MicDeviceError::errorCode( MICSDKERR_NO_ACCESS );

    // Our mock device does not hang
    (void) force;

    // Let's go for a reset round
    m_DeviceState = MicDevice::DeviceState::eReset;

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlMockDevice::setDeviceLedMode( uint32_t mode )
 *  @param  mode    LED mode
 *  @return error code
 *
 *  Activate specified LED \a mode.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 */

uint32_t  KnlMockDevice::setDeviceLedMode( uint32_t mode )
{
    // Only modes 0 and 1 are supported
    if ((mode != 0) && (mode != 1))
    {
        /// \todo Should we really treat this as error?
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );
    }

    m_LedMode = mode;

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlMockDevice::setDeviceTurboModeEnabled( bool state )
 *  @param  state   Enabled state
 *  @return error code
 *
 *  Enable or disable the device turbo mode.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_NOT_SUPPORTED
 */

uint32_t  KnlMockDevice::setDeviceTurboModeEnabled( bool state )
{
    if (!m_TurboAvailable)
        return  MicDeviceError::errorCode( MICSDKERR_NOT_SUPPORTED );

    m_TurboEnabled = state;

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlMockDevice::setDeviceSmcPersistenceEnabled( bool state )
 *  @param  state   Enabled state
 *  @return error code
 *
 *  Enable or disable SMC persistence. When enabled, the SMC settings will
 *  survive over a boot sequence.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  Currently this function never fails.
 */

uint32_t  KnlMockDevice::setDeviceSmcPersistenceEnabled( bool state )
{
    /// \todo Do we require special privileges?

    m_SmcPersist = state;

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlMockDevice::setDeviceSmcRegisterData( uint8_t offset, const uint8_t* data, size_t size )
 *  @param  offset  Register offset
 *  @param  data    Pointer to data buffer
 *  @param  size    Size of data buffer
 *  @return error code
 *
 *  Write specified \a data of specified \a size to the  SMC register with
 *  given \a offset.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_NOT_SUPPORTED
 */

uint32_t  KnlMockDevice::setDeviceSmcRegisterData( uint8_t offset, const uint8_t* data, size_t size )
{
    if (!data || (size == 0))
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    AccessMode  access;
    int         nbytes = smcRegisterSize( offset, &access );
    if ((nbytes < 0) || (access == eNoAccess) || (access == eReadOnly))
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );     // Offset or access issue

    // We checked at least basic errors, but now we have to admit our mock state

    return  MicDeviceError::errorCode( MICSDKERR_NOT_SUPPORTED );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlMockDevice::setDevicePowerThreshold( MicDevice::PowerWindow window, uint32_t power, uint32_t time )
 *  @param  window  Power window
 *  @param  power   Power threshold in microWatts
 *  @param  time    Window duration in microseconds
 *  @return error code
 *
 *  Set the \a power threshold for specified power \a window and set the
 *  window duration to \a time milliseconds.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 */

uint32_t  KnlMockDevice::setDevicePowerThreshold( MicDevice::PowerWindow window, uint32_t power, uint32_t time )
{
    if (window == MicDevice::eWindow0)
    {
        m_pPowerThresholdData->mWindow0Threshold = power;
        m_pPowerThresholdData->mWindow0Time      = time;
    }
    else if (window == MicDevice::eWindow1)
    {
        m_pPowerThresholdData->mWindow1Threshold = power;
        m_pPowerThresholdData->mWindow1Time      = time;
    }
    else
    {
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );
    }

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlMockDevice::startDeviceSmBusAddressTraining( int hint, int timeout )
 *  @param  hint     SMBus base address hint
 *  @param  timeout  Timeout in milliseconds optional)
 *  @return error code
 *
 *  Start the SMBus address training procedure using specified base address
 *  \a hint.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_NO_ACCESS
 */

uint32_t  KnlMockDevice::startDeviceSmBusAddressTraining( int hint, int timeout )
{
    if (!m_IsAdmin)
        return  MicDeviceError::errorCode( MICSDKERR_NO_ACCESS );

    (void) hint;
    (void) timeout;

    return  MicDeviceError::errorCode(MICSDKERR_SUCCESS);
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlMockDevice::startDeviceFlashUpdate( FlashImage* image, bool force )
 *  @param  image   Pointer to flash image
 *  @param  force   Force flash flag
 *  @return error code
 *
 *  Initiate a flash update with specified flash \a image.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_NO_ACCESS
 *  - MICSDKERR_DEVICE_BUSY
 *  - MICSDKERR_NOT_SUPPORTED
 */

uint32_t  KnlMockDevice::startDeviceFlashUpdate( FlashImage* image, bool force )
{
    if (!image || !image->isValid())
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    // Our real device calls would fail if not privileged, so...
    if (!m_IsAdmin)
        return MicDeviceError::errorCode( MICSDKERR_NO_ACCESS );

    if (m_pFlashStatus->isBusy())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_BUSY );

    /// \todo Determine what we use the force parameter for
    (void) force;

    return  MicDeviceError::errorCode( MICSDKERR_NOT_SUPPORTED );
}


uint32_t  KnlMockDevice::startDeviceFlashUpdate( std::string & path, std::unique_ptr<micmgmt::FwToUpdate> & fwToUpdate )
{
    if (path.empty())
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );
    // Our real device calls would fail if not privileged, so...
    if (!m_IsAdmin)
        return MicDeviceError::errorCode( MICSDKERR_NO_ACCESS );
    if (m_pFlashStatus->isBusy())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_BUSY );
    (void) fwToUpdate;
    return  MicDeviceError::errorCode( MICSDKERR_NOT_SUPPORTED );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlMockDevice::finalizeDeviceFlashUpdate( bool abort )
 *  @param  abort   Abort flag
 *  @return error code
 *
 *  Finialize the flash update sequence.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  Currently this function never fails.
 */

uint32_t  KnlMockDevice::finalizeDeviceFlashUpdate( bool abort )
{
    (void) abort;   // Not used

    /// \todo We may want to check our current flash state

    m_pFlashStatus->set( FlashStatus::eIdle, 0, eIdleStage );
    m_pFlashStatus->setStageText( flashStageAsText( eIdleStage ) );

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}

