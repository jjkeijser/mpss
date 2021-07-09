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

#ifndef MICMGMT_KNLDEVICE_HPP
#define MICMGMT_KNLDEVICE_HPP

// SYSTEM INCLUDES
//
#include    <iomanip>

// PROJECT INCLUDES
//
#include    "KnlDevice.hpp"
#include    "KnlDeviceBase.hpp"
#include    "KnlPropKeys.hpp"
#include    "FlashImage.hpp"
#include    "FlashStatus.hpp"
#include    "MicCoreInfo.hpp"
#include    "MicCoreUsageInfo.hpp"
#include    "MicBootConfigInfo.hpp"
#include    "MicDevice.hpp"
#include    "MicDeviceError.hpp"
#include    "MicLogger.hpp"
#include    "MicMemoryInfo.hpp"
#include    "MicMemoryUsageInfo.hpp"
#include    "MicPciConfigInfo.hpp"
#include    "MicPlatformInfo.hpp"
#include    "MicPowerUsageInfo.hpp"
#include    "MicPowerThresholdInfo.hpp"
#include    "MicProcessorInfo.hpp"
#include    "MicThermalInfo.hpp"
#include    "MicVoltageInfo.hpp"
#include    "MicVersionInfo.hpp"
#include    "SystoolsdConnection.hpp"
#include    "MpssStackBase.hpp"
#include    "ScifRequest.hpp"
#include    "micmgmtCommon.hpp"
//
#include    "CoreInfoData_p.hpp"
#include    "CoreUsageData_p.hpp"
#include    "MemoryInfoData_p.hpp"
#include    "MemoryUsageData_p.hpp"
#include    "PciConfigData_p.hpp"
#include    "PlatformInfoData_p.hpp"
#include    "PowerUsageData_p.hpp"
#include    "PowerThresholdData_p.hpp"
#include    "ProcessorInfoData_p.hpp"
#include    "ThermalInfoData_p.hpp"
#include    "VersionInfoData_p.hpp"
#include    "VoltageInfoData_p.hpp"

// CFW INCLUDES
//
#include    "MsTimer.hpp"

// SYSTOOLSD PROTOCOL INCLUDES
//
#include    "systoolsd_api.h"

#ifdef UNIT_TESTS
#define PRIVATE public
#else
#define PRIVATE private
#endif // UNIT_TESTS

namespace
{
    const uint8_t MAX_RETRIES = 3;
    const uint16_t RETRY_DELAY_MS = 300;

    struct FwUpdateStatus
    {
        bool bios;
        bool me;
        bool smc;
        int lastStagenum;
        FwUpdateStatus() : bios(false), me(false), smc(false), lastStagenum(0)
        {
        }
    };


}

// NAMESPACE
//
namespace  micmgmt
{

//============================================================================
//  CLASS:  KnlDeviceAbstract

template <class Base, class Mpss, class MpssCreator, class ScifDev>
class  KnlDeviceAbstract : public Base
{

public:

    explicit KnlDeviceAbstract( int number=0 );
   ~KnlDeviceAbstract();


PRIVATE:

    bool         isDeviceOpen() const;
    uint32_t     getDeviceState( MicDevice::DeviceState* state ) const;
    uint32_t     getDevicePciConfigInfo( MicPciConfigInfo* info ) const;
    uint32_t     getDeviceProcessorInfo( MicProcessorInfo* info ) const;
    uint32_t     getDeviceVersionInfo( MicVersionInfo* info ) const;
    uint32_t     getDeviceMemoryInfo( MicMemoryInfo* info ) const;
    uint32_t     getDeviceCoreInfo( MicCoreInfo* info ) const;
    uint32_t     getDevicePlatformInfo( MicPlatformInfo* info ) const;
    uint32_t     getDeviceThermalInfo( MicThermalInfo* info ) const;
    uint32_t     getDeviceVoltageInfo( MicVoltageInfo* info) const;
    uint32_t     getDeviceCoreUsageInfo( MicCoreUsageInfo* info ) const;
    uint32_t     getDevicePowerUsageInfo( MicPowerUsageInfo* info ) const;
    uint32_t     getDevicePowerThresholdInfo( MicPowerThresholdInfo* info ) const;
    uint32_t     getDeviceMemoryUsageInfo( MicMemoryUsageInfo* info ) const;
    uint32_t     getDeviceLedMode( uint32_t* mode ) const;
    uint32_t     getDeviceEccMode( bool* enabled, bool* available=0 ) const;
    uint32_t     getDeviceTurboMode( bool* enabled, bool* available=0, bool* active=0 ) const;
    uint32_t     getDeviceSmBusAddressTrainingStatus( bool* busy, int* eta=0 ) const;
    uint32_t     getDeviceSmcRegisterData( uint8_t offset, uint8_t* data, size_t* size ) const;
    uint32_t     getSELEntrySelectionRegisterSize( size_t* size ) const;
    uint32_t     getDeviceFlashUpdateStatus( FlashStatus* status ) const;
    uint32_t     getDevicePostCode( std::string* code ) const;

    uint32_t     deviceOpen();
    void         deviceClose();
    uint32_t     deviceBoot( const MicBootConfigInfo& info );
    uint32_t     deviceReset( bool force );
    uint32_t     setDeviceLedMode( uint32_t mode );
    uint32_t     setDeviceTurboModeEnabled( bool state );
    uint32_t     setDeviceSmcRegisterData( uint8_t offset, const uint8_t* data, size_t size );
    uint32_t     setDevicePowerThreshold( MicDevice::PowerWindow window, uint32_t power, uint32_t time );
    uint32_t     startDeviceSmBusAddressTraining( int hint, int timeout=5000 );
    uint32_t     startDeviceFlashUpdate( FlashImage* image, bool force );
    uint32_t     startDeviceFlashUpdate (std::string & path, std::unique_ptr<micmgmt::FwToUpdate> & fwToUpdate );
    uint32_t     finalizeDeviceFlashUpdate( bool abort );


private:

    struct PrivData;
    FwUpdateStatus m_fwToUpdate;
    mutable FwUpdateStatus m_completed;
    std::unique_ptr<PrivData>  m_pData;


private:    // DISABLE

    KnlDeviceAbstract( const KnlDeviceAbstract& );
    KnlDeviceAbstract&  operator = ( const KnlDeviceAbstract& );

PRIVATE:
    ScifDev*    getScifDev() const;
    Mpss*       getMpss() const;
    // For UT purposes
    void        setBinariesToUpdate( bool bios, bool me, bool smc, int lastStagenum );
};


//============================================================================
/** @class    micmgmt::KnlDevice      KnlDevice.hpp
 *  @ingroup  sdk
 *  @brief    The class represents a KNL device
 *
 *  The \b %KnlDevice class encapsulates a KNL device.
 */
typedef KnlDeviceAbstract<KnlDeviceBase, MpssStackBase, MpssStackBase, SystoolsdConnection> KnlDevice;


//============================================================================
//  BEGIN IMPLEMENTATION OF TEMPLATE METHODS
//============================================================================


namespace knldevice_detail {
const char* const  PROPVAL_DEVICE_MODE_UNKNOWN   = "N/A";
const char* const  PROPVAL_DEVICE_MODE_NORMAL    = "linux";
const char* const  PROPVAL_DEVICE_MODE_FLASH     = "flash";
//
const char* const  NOT_AVAILABLE                 = "";
const char* const  NOT_SUPPORTED                 = "n/a";
const uint32_t     LED_MODE_MASK                 = 0x00000001;
const unsigned int SMBUS_TRAINING_DURATION_MS    = 5000;
const uint8_t      SEL_ENTRY_SELECTION_REGISTER  = 0x20;
}

// PrivData definition
template <class Base, class Mpss, class MpssCreator, class ScifDev>
struct KnlDeviceAbstract<Base, Mpss, MpssCreator, ScifDev>::PrivData
{
    std::unique_ptr<Mpss>                 mpMpssStack;
    std::unique_ptr<ScifDev>              mpScifDevice;
    mutable std::unique_ptr<MsTimer>      mpSmBusTimer;
    mutable bool                          mSmBusBusy;
};

// Define initialize structure macro
#define  STRUCTINIT( x )  memset( &x, 0, sizeof( x ) )

//============================================================================
//  P U B L I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     KnlDevice::KnlDevice( int number )
 *  @param  number  Device number
 *
 *  Construct a KNL device with given device \a number.
 */

template <class Base, class Mpss, class MpssCreator, class ScifDev>
KnlDeviceAbstract<Base, Mpss, MpssCreator, ScifDev>::KnlDeviceAbstract( int number ) :
    Base( number ),
    m_pData( new PrivData )
{
    m_pData->mpMpssStack.reset( MpssCreator::createMpssStack( number ) );
    m_pData->mpScifDevice.reset( new ScifDev( number ) );
    m_pData->mpSmBusTimer.reset( new MsTimer( knldevice_detail::SMBUS_TRAINING_DURATION_MS ) );
    m_pData->mSmBusBusy = false;
}


//----------------------------------------------------------------------------
/** @fn     KnlDevice::~KnlDevice()
 *
 *  Cleanup.
 */

template <class Base, class Mpss, class MpssCreator, class ScifDev>
KnlDeviceAbstract<Base, Mpss, MpssCreator, ScifDev>::~KnlDeviceAbstract()
{
    // Nothing to do
}


//============================================================================
//  P R I V A T E   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     bool  KnlDevice::isDeviceOpen() const
 *  @return device open state
 *
 *  Returns \c true if the physical device is open.
 *
 *  This virtual function may be reimplemented by deriving classes.
 *  This implementation returns \c true if the SCIF connection is open.
 */

template <class Base, class Mpss, class MpssCreator, class ScifDev>
bool  KnlDeviceAbstract<Base, Mpss, MpssCreator, ScifDev>::isDeviceOpen() const
{
    return  m_pData->mpScifDevice->isOpen();
}

//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlDevice::getDeviceState( MicDevice::DeviceState* state ) const
 *  @param  state   Device state return
 *  @return error code
 *
 *  Retrieve current device \a state.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_INTERNAL_ERROR
 */

template <class Base, class Mpss, class MpssCreator, class ScifDev>
uint32_t  KnlDeviceAbstract<Base, Mpss, MpssCreator, ScifDev>::getDeviceState( MicDevice::DeviceState* state ) const
{
    if (!state)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    int  stateval = MpssStackBase::eStateInvalid;
    uint32_t  result = m_pData->mpMpssStack->getDeviceState( &stateval );
    if (MicDeviceError::isError( result ))
        return  result;

    switch (stateval)
    {
      case  MpssStackBase::eStateReady:
        *state = MicDevice::eReady;
        break;

      case  MpssStackBase::eStateBooting:
        *state = MicDevice::eReboot;
        break;

      case  MpssStackBase::eStateNoResponse:
      case  MpssStackBase::eStateShutdown:
      case  MpssStackBase::eStateResetFailed:
      case  MpssStackBase::eStateBootFailed:
        *state = MicDevice::eOffline;
        break;

      case  MpssStackBase::eStateOnline:
        *state = MicDevice::eOnline;
        break;

      case  MpssStackBase::eStateNodeLost:
        *state = MicDevice::eLost;
        break;

      case  MpssStackBase::eStateResetting:
        *state = MicDevice::eReset;
        break;

      case  MpssStackBase::eStateOnlineFW:
        *state = MicDevice::eOnlineFW;
        break;

      case  MpssStackBase::eStateBootingFW:
        *state = MicDevice::eBootingFW;
        break;

      case  MpssStackBase::eStateInvalid:
      default:
        *state = MicDevice::eError;
        break;
    }

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlDevice::getDevicePciConfigInfo( MicPciConfigInfo* info ) const
 *  @param  info    Pointer to PCI config info return
 *  @return error code
 *
 *  Retrieve PCI config info data into specified \a info object.
 *
 *  The PCI config info is available at all times and independent of the
 *  device state.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_INTERNAL_ERROR
 */

template <class Base, class Mpss, class MpssCreator, class ScifDev>
uint32_t  KnlDeviceAbstract<Base, Mpss, MpssCreator, ScifDev>::getDevicePciConfigInfo( MicPciConfigInfo* info ) const
{
    if (!info)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    PciConfigData  data;

    uint32_t  result = m_pData->mpMpssStack->getDevicePciConfigData( &data );
    if (MicDeviceError::isError( result ))
        return  result;

    *info = MicPciConfigInfo( data );

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlDevice::getDeviceProcessorInfo( MicProcessorInfo* info ) const
 *  @param  info  Pointer to processor info return
 *  @return error code
 *
 *  Retrieve processor info data into specified \a info object.
 *
 *  The processor info is available at all times and independent of the
 *  device state.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_INTERNAL_ERROR
 */

template <class Base, class Mpss, class MpssCreator, class ScifDev>
uint32_t  KnlDeviceAbstract<Base, Mpss, MpssCreator, ScifDev>::getDeviceProcessorInfo( MicProcessorInfo* info ) const
{
    if (!info)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    ProcessorInfoData  data;

    uint32_t  result = 0;
    uint32_t  value  = 0;

    result = m_pData->mpMpssStack->getDeviceProperty( &value, PROPKEY_PROC_MODEL, 16 );
    if (result == MICSDKERR_DRIVER_NOT_LOADED)
    {
        *info = MicProcessorInfo();
        return result;
    }

    if (!MicDeviceError::isError( result ))
        data.mModel = static_cast<uint16_t>( value );

    result = m_pData->mpMpssStack->getDeviceProperty( &value, PROPKEY_PROC_TYPE, 16 );
    if (!MicDeviceError::isError( result ))
        data.mType = static_cast<uint16_t>( value );

    result = m_pData->mpMpssStack->getDeviceProperty( &value, PROPKEY_PROC_FAMILY, 16 );
    if (!MicDeviceError::isError( result ))
        data.mFamily = static_cast<uint16_t>( value );

    result = m_pData->mpMpssStack->getDeviceProperty( &value, PROPKEY_PROC_STEPPING, 16 );
    if (!MicDeviceError::isError( result ))
        data.mSteppingData = value;

    std::string  sdata;
    result = m_pData->mpMpssStack->getDeviceProperty( &sdata, PROPKEY_BOARD_STEPPING );
    if (!MicDeviceError::isError( result ))
        data.mStepping = sdata;

    result = m_pData->mpMpssStack->getDeviceProperty( &sdata, PROPKEY_SYS_SKU );
    if (!MicDeviceError::isError( result ))
        data.mSku = sdata;

    *info = MicProcessorInfo( data );

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlDevice::getDeviceVersionInfo( MicVersionInfo* info ) const
 *  @param  info    Pointer to version return
 *  @return error code
 *
 *  Retrieve and return device version \a info.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_INTERNAL_ERROR
 *  - MICSDKERR_DEVICE_IO_ERROR
 */

template <class Base, class Mpss, class MpssCreator, class ScifDev>
uint32_t  KnlDeviceAbstract<Base, Mpss, MpssCreator, ScifDev>::getDeviceVersionInfo( MicVersionInfo* info ) const
{
    if (!info)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    info->clear();

    VersionInfoData  data;
    uint32_t         result = 0;

    // Retrieve information that is always available first
    std::string dataStr;
    result = m_pData->mpMpssStack->getDeviceProperty( &dataStr, PROPKEY_BIOS_VERSION );
    // If driver is not loaded, fail fast and invalidate all data
    if(result == MicDeviceError::errorCode( MICSDKERR_DRIVER_NOT_LOADED ))
    {
        *info = MicVersionInfo( data );
        return result;
    }

    if (!MicDeviceError::isError( result ))
    {
        feTrim(dataStr);
        data.mBiosVersion = dataStr;
    }

    dataStr.clear();
    result = m_pData->mpMpssStack->getDeviceProperty( &dataStr, PROPKEY_BIOS_RELDATE );
    if (!MicDeviceError::isError( result ))
    {
        feTrim(dataStr);
        data.mBiosRelDate = dataStr;
    }

    dataStr.clear();
    result = m_pData->mpMpssStack->getDeviceProperty( &dataStr, PROPKEY_OEM_STRINGS );
    if (!MicDeviceError::isError( result ))
    {
        feTrim(dataStr);
        data.mOemStrings = dataStr;
    }

    dataStr.clear();
    result = m_pData->mpMpssStack->getDeviceProperty( &dataStr, PROPKEY_SMC_VERSION );
    if (!MicDeviceError::isError( result ))
    {
        feTrim(dataStr);
        data.mSmcVersion = dataStr;
    }

    dataStr.clear();
    result = m_pData->mpMpssStack->getDeviceProperty( &dataStr, PROPKEY_ME_VERSION );
    if (!MicDeviceError::isError( result ))
    {
        feTrim(dataStr);
        data.mMeVersion = dataStr;
    }

    dataStr.clear();
    result = m_pData->mpMpssStack->getDeviceProperty( &dataStr, PROPKEY_NTB_VERSION );
    if (!MicDeviceError::isError( result ))
    {
        feTrim(dataStr);
        data.mPlxVersion = dataStr;
    }

    dataStr.clear();
    result = m_pData->mpMpssStack->getDeviceProperty( &dataStr, PROPKEY_FAB_VERSION );
    if (!MicDeviceError::isError( result ))
    {
        feTrim(dataStr);
        data.mFabVersion = dataStr;
    }

    dataStr.clear();
    result = m_pData->mpMpssStack->getDeviceProperty( &dataStr, PROPKEY_MCDRAM_VERSION );
    if (!MicDeviceError::isError( result ))
    {
        feTrim(dataStr);
        data.mRamCtrlVersion = dataStr;
    }

    dataStr.clear();
    result = m_pData->mpMpssStack->getSystemMpssVersion( &dataStr );
    if (!MicDeviceError::isError( result ))
    {
        feTrim(dataStr);
        data.mMpssVersion = dataStr;
    }

    dataStr.clear();
    result = m_pData->mpMpssStack->getSystemDriverVersion( &dataStr );
    if (!MicDeviceError::isError( result ))
    {
        feTrim(dataStr);
        data.mDriverVersion = dataStr;
    }

    data.mFlashVersion   = data.mBiosVersion;


    // Retrieve information only available when online

    if (isDeviceOpen())
    {
        /// \todo Check if this version is not the same as sysfs/wmi smc_version
        struct  DeviceInfo  devinfo;
        STRUCTINIT( devinfo );
        ScifRequest  devinforeq( GET_DEVICE_INFO, (char*) &devinfo, sizeof( devinfo ) );
        result = m_pData->mpScifDevice->request( &devinforeq );
        if (!MicDeviceError::isError( result ))
        {
            std::stringstream  strm;
            strm << static_cast<int>( (devinfo.boot_fw_version >> 24) & 0x00ff );
            strm << ".";
            strm << static_cast<int>( (devinfo.boot_fw_version >> 16) & 0x00ff );
            strm << ".";
            strm << static_cast<int>( (devinfo.boot_fw_version >>  0) & 0xffff );
            data.mSmcBootVersion = strm.str();
        }
    }

    *info = MicVersionInfo( data );

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlDevice::getDeviceMemoryInfo( MicMemoryInfo* info ) const
 *  @param  info  Pointer to memory info return
 *  @return error code
 *
 *  Retrieve memory information into specified \a info object.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_INTERNAL_ERROR
 *  - MICSDKERR_DEVICE_IO_ERROR
 */

template <class Base, class Mpss, class MpssCreator, class ScifDev>
uint32_t  KnlDeviceAbstract<Base, Mpss, MpssCreator, ScifDev>::getDeviceMemoryInfo( MicMemoryInfo* info ) const
{
    if (!info)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    MemoryInfoData  data;

    struct  MemoryInfo  meminfo;
    STRUCTINIT( meminfo );
    ScifRequest  meminforeq( GET_MEMORY_INFO, (char*) &meminfo, sizeof( meminfo ) );
    if (MicDeviceError::isError( m_pData->mpScifDevice->request( &meminforeq ) ))
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_IO_ERROR );

    data.mSize = MicMemory( meminfo.total_size, MicSpeed::eMega );
    data.mSpeed = MicSpeed( meminfo.speed, MicSpeed::eMega );
    data.mFrequency = MicFrequency( meminfo.frequency, MicFrequency::eMega );
    data.mMemoryType = "DRAM";
    data.mTechnology = "MCDRAM";
    data.mEcc = meminfo.ecc_enabled ? true : false;

    size_t vendorNameLen = strnlen( meminfo.manufacturer, sizeof( meminfo.manufacturer ) );
    data.mVendorName.assign( meminfo.manufacturer, vendorNameLen );

    data.mValid = true;

    *info = MicMemoryInfo( data );

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlDevice::getDeviceCoreInfo( MicCoreInfo* info ) const
 *  @param  info  Pointer to core info return
 *  @return error code
 *
 *  Retrieve core information into specified \a info object.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_DEVICE_IO_ERROR
 */

template <class Base, class Mpss, class MpssCreator, class ScifDev>
uint32_t  KnlDeviceAbstract<Base, Mpss, MpssCreator, ScifDev>::getDeviceCoreInfo( MicCoreInfo* info ) const
{
    if (!info)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    CoreInfoData  data;

    struct  CoresInfo  coreinfo;
    STRUCTINIT( coreinfo );
    ScifRequest  coreinforeq( GET_CORES_INFO, (char*) &coreinfo, sizeof( coreinfo ) );
    if (MicDeviceError::isError( m_pData->mpScifDevice->request( &coreinforeq ) ))
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_IO_ERROR );

    uint32_t voltage = coreinfo.cores_voltage;
    // Following logic comes from the SMBIOS spec
    // Interpret and convert to millivolts
    if(voltage & (1 << 7)) // If bit 7 (base 0) is set
    {
        // The remaining 7 bits represent the voltage **times 10**
        voltage = (voltage & 0x7f) * 100;
    }
    else
    {
        // If bit
        //      0 : 5V
        //      1 : 3.3V
        //      2 : 2.9V
        if(voltage & (1 << 0))
            voltage = 5000;
        else if(voltage & (1 << 1))
            voltage = 3300;
        else if(voltage & (1 << 2))
            voltage = 2900;
        else
            // This would mean "unknown" according to SMBIOS
            voltage = 0;
    }

    data.mVoltage = MicVoltage( "Core Voltage", voltage, MicVoltage::eMilli );
    data.mFrequency = MicFrequency( "Core Frequency", coreinfo.cores_freq, MicFrequency::eMega );
    data.mCoreCount = coreinfo.num_cores;
    data.mCoreThreadCount = coreinfo.threads_per_core;

    *info = MicCoreInfo( data );

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlDevice::getDevicePlatformInfo( MicPlatformInfo* info ) const
 *  @param  info  Pointer to MIC platform info return
 *  @return error code
 *
 *  Retrieve MIC platform info into specified \a info object.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_INTERNAL_ERROR
 *  - MICSDKERR_DEVICE_IO_ERROR
 */

template <class Base, class Mpss, class MpssCreator, class ScifDev>
uint32_t  KnlDeviceAbstract<Base, Mpss, MpssCreator, ScifDev>::getDevicePlatformInfo( MicPlatformInfo* info ) const
{
    if (!info)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    PlatformInfoData  data;

    // Device info
    struct  DeviceInfo  devinfo;
    STRUCTINIT( devinfo );
    ScifRequest  devinforeq( GET_DEVICE_INFO, (char*) &devinfo, sizeof( devinfo ) );

    bool error = MicDeviceError::isError( m_pData->mpScifDevice->request( &devinforeq ) );
    error |= devinforeq.isError();

    if (!error)
    {
        // The SMC has been updated and the part_number field is now 8 bytes long, instead of 16.
        data.mPartNumber = std::string( devinfo.part_number, strnlen( devinfo.part_number, 8 ) );
        data.mCoprocessorOs = std::string( devinfo.os_version, strnlen( devinfo.os_version, sizeof(devinfo.os_version) ) );

        std::string featureSet;
        switch (devinfo.hw_revision & 0x00030000)
        {
          case  0x00000000:
            featureSet = "RP";   /// \todo Define these in KnlDeviceBase
            break;

          case  0x00010000:
            featureSet = "RPRD";
            break;

          case  0x00020000:
            featureSet = "STHI";
            break;

          default:  // Does not happen
            break;
        }

        switch (devinfo.hw_revision & 0x00000300)
        {
          case  0x00000000:
            featureSet += " Fab A";   /// \todo Define these in KnlDeviceBase
            break;

          case  0x00000100:
            featureSet += " Fab B";
            break;

          case  0x00000200:
            featureSet += " Fab C";
            break;

          case  0x00000300:
            featureSet += " Fab D";
            break;

          default:  // Does not happen
            break;
        }
            //TDP
        if (devinfo.hw_revision & 0x2)
            featureSet += " 300W";
        else
            featureSet += " 225W";

        if (devinfo.hw_revision & 0x4)
        {
            featureSet += " Active";
            data.mCoolActive  = true;
        }
        else
        {
            featureSet += " Passive"; /// \todo Define these in KnlDeviceBase
            data.mCoolActive  = false;
        }


        data.mFeatureSet = featureSet;

        data.mStrapInfo    = data.mFeatureSet;      /// \todo Obsolete strap info maybe?
        data.mStrapInfoRaw = devinfo.hw_revision;   /// \todo Not 100% correct
        data.mSmBusAddress = static_cast<uint8_t>( devinfo.pci_smba & 0xff );

        // Date-time encoding:
        // Byte     Description
        // 0        second
        // 1        minute
        // 2        hour
        // 3        day (1-based)
        // 4        month (0 based)
        // 5        year (0 = 2011)

        std::stringstream dateTime;
        dateTime << static_cast<uint32_t>( devinfo.manufacture_date[5] + 2011 ); // Year
        dateTime << "-";
        dateTime << std::setfill('0') << std::setw(2) << static_cast<uint32_t>( devinfo.manufacture_date[4] ) + 1; // Month
        dateTime << "-";
        dateTime << std::setfill('0') << std::setw(2) << static_cast<uint32_t>( devinfo.manufacture_date[3] ); // Day

        data.mManufactDate = dateTime.str();

        dateTime.str("");
        dateTime.clear();
        dateTime << std::setfill('0') << std::setw(2) << static_cast<uint32_t>( devinfo.manufacture_date[2] );
        dateTime << ":";
        dateTime << std::setfill('0') << std::setw(2) << static_cast<uint32_t>( devinfo.manufacture_date[1] );
        dateTime << ":";
        dateTime << std::setfill('0') << std::setw(2) << static_cast<uint32_t>( devinfo.manufacture_date[0] );

        data.mManufactTime = dateTime.str();

        data.mMaxPower     = MicPower( "TDP", devinfo.card_tdp & 0xFFFF );  // Watts
    }

    uint32_t  result = 0;

    std::string dataStr;
    result = m_pData->mpMpssStack->getDeviceProperty( &dataStr, PROPKEY_CPU_VENDOR );
    if (result == MICSDKERR_DRIVER_NOT_LOADED)
    {
        *info = MicPlatformInfo();
        return result;
    }

    if (!MicDeviceError::isError( result ))
    {
        feTrim(dataStr);
        data.mCoprocessorBrand = dataStr;
    }

    dataStr.clear();
    result = m_pData->mpMpssStack->getDeviceProperty( &dataStr, PROPKEY_SYS_SKU );
    if (!MicDeviceError::isError( result ))
    {
        feTrim(dataStr);
        data.mBoardSku = dataStr;
    }

    dataStr.clear();
    result = m_pData->mpMpssStack->getDeviceProperty( &dataStr, PROPKEY_BOARD_TYPE );
    if (!MicDeviceError::isError( result ))
    {
        feTrim(dataStr);
        data.mBoardType = dataStr;
    }

    dataStr.clear();
    result = m_pData->mpMpssStack->getDeviceProperty( &dataStr, PROPKEY_SYS_UUID );
    if (!MicDeviceError::isError( result ))
    {
        feTrim(dataStr);
        data.mUuid = dataStr;
    }

    dataStr.clear();
    result = m_pData->mpMpssStack->getDeviceProperty( &dataStr, PROPKEY_SYS_SERIAL );
    if (!MicDeviceError::isError( result ))
    {
        feTrim(dataStr);
        data.mSerialNo = dataStr;
    }

    *info = MicPlatformInfo( data );

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlDevice::getDeviceThermalInfo( MicThermalInfo* info ) const
 *  @param  info  Pointer to thermal info return
 *  @return error code
 *
 *  Retrieve thermal information into specified \a info object.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_DEVICE_IO_ERROR
 */

template <class Base, class Mpss, class MpssCreator, class ScifDev>
uint32_t  KnlDeviceAbstract<Base, Mpss, MpssCreator, ScifDev>::getDeviceThermalInfo( MicThermalInfo* info ) const
{
    if (!info)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    ThermalInfoData  data;

    struct  ThermalInfo  temp;
    STRUCTINIT( temp );
    ScifRequest  tempreq( GET_THERMAL_INFO, (char*) &temp, sizeof( temp ) );
    if (MicDeviceError::isError( m_pData->mpScifDevice->request( &tempreq ) ))
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_IO_ERROR );

    data.mSensors.push_back( MicTemperature( Base::tempSensorAsText( Base::eCpuTemp ),    temp.temp_cpu ) );
    data.mSensors.push_back( MicTemperature( Base::tempSensorAsText( Base::eExhaustTemp ), temp.temp_exhaust ) );
    data.mSensors.push_back( MicTemperature( Base::tempSensorAsText( Base::eVccpTemp ),   temp.temp_vccp ) );
    data.mSensors.push_back( MicTemperature( Base::tempSensorAsText( Base::eVccclrTemp ), temp.temp_vccclr ) );
    data.mSensors.push_back( MicTemperature( Base::tempSensorAsText( Base::eVccmpTemp ),  temp.temp_vccmp ) );
    data.mSensors.push_back( MicTemperature( Base::tempSensorAsText( Base::eWestTemp ),   temp.temp_west ) );
    data.mSensors.push_back( MicTemperature( Base::tempSensorAsText( Base::eEastTemp ),   temp.temp_east ) );

    if( !( temp.fan_tach & 0xC0000000 ) ) // If upper two bits are set, info is unavailable
        data.mFanRpm   = temp.fan_tach & 0x0000FFFF; // Take lower two bytes
    else
        LOG( WARNING_MSG, "invalid reading: fan_tach" );

    if( !( temp.fan_pwm & 0xC0000000 ) ) // If upper two bits are set, info is unavailable
        data.mFanPwm   = temp.fan_pwm & 0x000000FF; // Take lower byte
    else
        LOG( WARNING_MSG, "invalid reading: fan_pwm" );

    if( !( temp.fan_pwm_adder & 0xC0000000 ) ) // If upper two bits are set, info is unavailable
        data.mFanAdder   = temp.fan_tach & 0x000000FF; // Take lower byte
    else
        LOG( WARNING_MSG, "invalid reading: fan_tach" );

    data.mControl  = MicTemperature( temp.tcontrol );
    data.mCritical = MicTemperature( temp.tcritical );

    // Unsupported information: no longer provided by SMC
    data.mThrottleInfo = ThrottleInfo();

    data.mValid = true;

    *info = MicThermalInfo( data );

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlDevice::getDeviceVoltageInfo( MicVoltageInfo* info ) const
 *  @param  info  Pointer to voltage info return
 *  @return error code
 *
 *  Retrieve voltage information into specified \a info object.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_DEVICE_IO_ERROR
 */

template <class Base, class Mpss, class MpssCreator, class ScifDev>
uint32_t  KnlDeviceAbstract<Base, Mpss, MpssCreator, ScifDev>::getDeviceVoltageInfo( MicVoltageInfo* info ) const
{
    if (!info)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    VoltageInfoData  data;

    struct  VoltageInfo  vinfo;
    STRUCTINIT( vinfo );
    ScifRequest  vinforeq( GET_VOLTAGE_INFO, (char*) &vinfo, sizeof( vinfo ) );
    if (MicDeviceError::isError( m_pData->mpScifDevice->request( &vinforeq ) ))
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_IO_ERROR );

    data.mSensors.push_back( MicVoltage( Base::voltSensorAsText( Base::eVccpVolt ),     vinfo.voltage_vccp,     MicVoltage::eMilli ) );
    data.mSensors.push_back( MicVoltage( Base::voltSensorAsText( Base::eVccuVolt ),     vinfo.voltage_vccu,     MicVoltage::eMilli ) );
    data.mSensors.push_back( MicVoltage( Base::voltSensorAsText( Base::eVccclrVolt ),   vinfo.voltage_vccclr,   MicVoltage::eMilli ) );
    data.mSensors.push_back( MicVoltage( Base::voltSensorAsText( Base::eVccmlbVolt ),   vinfo.voltage_vccmlb,   MicVoltage::eMilli ) );
    data.mSensors.push_back( MicVoltage( Base::voltSensorAsText( Base::eVccmpVolt ),    vinfo.voltage_vccmp,    MicVoltage::eMilli ) );
    data.mSensors.push_back( MicVoltage( Base::voltSensorAsText( Base::eNtb1Volt ),     vinfo.voltage_ntb1,     MicVoltage::eMilli ) );
    data.mSensors.push_back( MicVoltage( Base::voltSensorAsText( Base::eVccpioVolt ),   vinfo.voltage_vccpio,   MicVoltage::eMilli ) );
    data.mSensors.push_back( MicVoltage( Base::voltSensorAsText( Base::eVccsfrVolt ),   vinfo.voltage_vccsfr,   MicVoltage::eMilli ) );
    data.mSensors.push_back( MicVoltage( Base::voltSensorAsText( Base::ePchVolt ),      vinfo.voltage_pch,      MicVoltage::eMilli ) );
    data.mSensors.push_back( MicVoltage( Base::voltSensorAsText( Base::eVccmfuseVolt ), vinfo.voltage_vccmfuse, MicVoltage::eMilli ) );
    data.mSensors.push_back( MicVoltage( Base::voltSensorAsText( Base::eNtb2Volt ),     vinfo.voltage_ntb2,     MicVoltage::eMilli ) );
    data.mSensors.push_back( MicVoltage( Base::voltSensorAsText( Base::eVppVolt ),      vinfo.voltage_vpp,      MicVoltage::eMilli ) );

    data.mValid = true;

    *info = MicVoltageInfo( data );

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlDevice::getDeviceCoreUsageInfo( MicCoreUsageInfo* info ) const
 *  @param  info  Pointer to core usage info return
 *  @return error code
 *
 *  Retrieve core usage info data into specified \a info object.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_INTERNAL_ERROR
 *  - MICSDKERR_DEVICE_IO_ERROR
 */

template <class Base, class Mpss, class MpssCreator, class ScifDev>
uint32_t  KnlDeviceAbstract<Base, Mpss, MpssCreator, ScifDev>::getDeviceCoreUsageInfo( MicCoreUsageInfo* info ) const
{
    if (!info)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    /// \todo Look into creating dynamic data structure handling in ScifRequest class

    // Currently, the ScifRequest class handles the SCIF request and read of response
    // in one go in the request() function. This means that the data structure/buffer
    // for the SCIF read has to be allocated prior calling the request() function.
    // Therefore, we do not support dynamic data structure allocation at this point
    // and we need to pre-allocate enough buffer to receive the SCIF response.

    // In this case (which is the only case of dynamic response data), we determine
    // the amount of data we expect. This will be the size of the CoreUsageInfo plus
    // the size of CoreCounters * number_of_cores.
    // To fullfil this task, we get the number of cores first and do our math.

    MicCoreInfo  coreinfo;

    uint32_t  result = getDeviceCoreInfo( &coreinfo );
    if (MicDeviceError::isError( result ))
        return  result;

    if (coreinfo.coreCount().value() == 0)
        return  MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );

    // Determine and allocate our response buffer size

    size_t  num_threads = coreinfo.coreCount().value() * coreinfo.coreThreadCount().value();
    size_t  buffersize  = sizeof( CoreUsageInfo ) + sizeof( CoreCounters ) * num_threads;
    char*   responsebuf = new char[buffersize];

    // Do our request

    ScifRequest  usageinforeq( GET_CORE_USAGE, responsebuf, buffersize );
    result = m_pData->mpScifDevice->request( &usageinforeq );

    if (MicDeviceError::isSuccess( result ))
    {
        CoreUsageInfo*  usageinfo = reinterpret_cast<CoreUsageInfo*>( responsebuf );

        if (!usageinfo || (usageinfo->num_cores * usageinfo->threads_per_core != num_threads))
        {
            result = MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );
        }
        else
        {
            info->setCoreCount( usageinfo->num_cores );
            info->setCoreThreadCount( usageinfo->threads_per_core );
            info->setFrequency( usageinfo->frequency );
            info->setTickCount( usageinfo->ticks );
            info->setTicksPerSecond( usageinfo->clocks_per_sec );

            info->setCounterTotal( MicCoreUsageInfo::eSystem, usageinfo->sum.system );
            info->setCounterTotal( MicCoreUsageInfo::eUser,   usageinfo->sum.user );
            info->setCounterTotal( MicCoreUsageInfo::eNice,   usageinfo->sum.nice );
            info->setCounterTotal( MicCoreUsageInfo::eIdle,   usageinfo->sum.idle );
            info->setCounterTotal( MicCoreUsageInfo::eTotal,  usageinfo->sum.total );

            CoreCounters*  usagecounters = reinterpret_cast<CoreCounters*>( responsebuf + sizeof( CoreUsageInfo ) );

            for (size_t thread=0; thread<num_threads; thread++)
            {
                info->setUsageCount( MicCoreUsageInfo::eSystem, thread, usagecounters[thread].system );
                info->setUsageCount( MicCoreUsageInfo::eUser,   thread, usagecounters[thread].user );
                info->setUsageCount( MicCoreUsageInfo::eNice,   thread, usagecounters[thread].nice );
                info->setUsageCount( MicCoreUsageInfo::eIdle,   thread, usagecounters[thread].idle );
                info->setUsageCount( MicCoreUsageInfo::eTotal,  thread, usagecounters[thread].total );
            }

            info->setValid( true );
        }
    }

    delete [] responsebuf;

    return  result;
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlDevice::getDevicePowerUsageInfo( MicPowerUsageInfo* info ) const
 *  @param  info  Pointer to power info return
 *  @return error code
 *
 *  Retrieve power information into specified \a info object.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_DEVICE_IO_ERROR
 */

template <class Base, class Mpss, class MpssCreator, class ScifDev>
uint32_t  KnlDeviceAbstract<Base, Mpss, MpssCreator, ScifDev>::getDevicePowerUsageInfo( MicPowerUsageInfo* info ) const
{
    if (!info)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    PowerUsageData  data;

    struct PowerUsageInfo  pinfo;
    STRUCTINIT( pinfo );
    ScifRequest  pinforeq( GET_POWER_USAGE, (char*) &pinfo, sizeof( pinfo ) );
    if (MicDeviceError::isError( m_pData->mpScifDevice->request( &pinforeq ) ))
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_IO_ERROR );

    data.mSensors.push_back( MicPower( Base::powerSensorAsText( Base::ePciePower ),    pinfo.pwr_pcie ) );
    data.mSensors.push_back( MicPower( Base::powerSensorAsText( Base::e2x3Power ),     pinfo.pwr_2x3 ) );
    data.mSensors.push_back( MicPower( Base::powerSensorAsText( Base::e2x4Power ),     pinfo.pwr_2x4 ) );
    data.mSensors.push_back( MicPower( Base::powerSensorAsText( Base::eAvg0Power ),    pinfo.avg_power_0 ) );
    data.mSensors.push_back( MicPower( Base::powerSensorAsText( Base::eCurrentPower ), pinfo.inst_power ) );
    data.mSensors.push_back( MicPower( Base::powerSensorAsText( Base::eMaximumPower ), pinfo.inst_power_max ) );
    data.mSensors.push_back( MicPower( Base::powerSensorAsText( Base::eVccpPower ),    pinfo.power_vccp ) );
    data.mSensors.push_back( MicPower( Base::powerSensorAsText( Base::eVccuPower ),    pinfo.power_vccu ) );
    data.mSensors.push_back( MicPower( Base::powerSensorAsText( Base::eVccclrPower ),  pinfo.power_vccclr ) );
    data.mSensors.push_back( MicPower( Base::powerSensorAsText( Base::eVccmlbPower ),  pinfo.power_vccmlb ) );
    data.mSensors.push_back( MicPower( Base::powerSensorAsText( Base::eVccmpPower ),   pinfo.power_vccmp ) );
    data.mSensors.push_back( MicPower( Base::powerSensorAsText( Base::eNtb1Power ),    pinfo.power_ntb1 ) );

    /// \todo Implement power throttle as soon as systoolsd provides that

    data.mValid = true;

    *info = MicPowerUsageInfo( data );

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlDevice::getDevicePowerThresholdInfo( MicPowerThresholdInfo* info ) const
 *  @param  info  Pointer to power threshold info return
 *  @return error code
 *
 *  Retrieve power threshold information into specified \a info object.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_DEVICE_IO_ERROR
 */

template <class Base, class Mpss, class MpssCreator, class ScifDev>
uint32_t  KnlDeviceAbstract<Base, Mpss, MpssCreator, ScifDev>::getDevicePowerThresholdInfo( MicPowerThresholdInfo* info ) const
{
    if (!info)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    PowerThresholdData  data;

    struct PowerThresholdsInfo  pinfo;
    STRUCTINIT( pinfo );
    ScifRequest  pinforeq( GET_PTHRESH_INFO, (char*) &pinfo, sizeof( pinfo ) );
    if (MicDeviceError::isError( m_pData->mpScifDevice->request( &pinforeq ) ))
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_IO_ERROR );

    data.mMaximum          = MicPower(pinfo.max_phys_power, MicPower::eMicro);
    data.mLoThreshold      = MicPower(pinfo.low_threshold, MicPower::eMicro);
    data.mHiThreshold      = MicPower(pinfo.hi_threshold, MicPower::eMicro);
    data.mWindow0Time      = pinfo.w0.time_window;
    data.mWindow0Threshold = MicPower(pinfo.w0.threshold, MicPower::eMicro);
    data.mWindow1Time      = pinfo.w1.time_window;
    data.mWindow1Threshold = MicPower(pinfo.w1.threshold, MicPower::eMicro);
    data.mPersistence      = false;     /// \todo Check if this answer is correct
    data.mValid            = true;

    *info = MicPowerThresholdInfo( data );

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlDevice::getDeviceMemoryUsageInfo( MicMemoryUsageInfo* info ) const
 *  @param  info  Pointer to memory usage info return
 *  @return error code
 *
 *  Retrieve memory usage information into specified \a info object.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_DEVICE_IO_ERROR
 */

template <class Base, class Mpss, class MpssCreator, class ScifDev>
uint32_t  KnlDeviceAbstract<Base, Mpss, MpssCreator, ScifDev>::getDeviceMemoryUsageInfo( MicMemoryUsageInfo* info ) const
{
    if (!info)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    MemoryUsageData  data;

    struct MemoryUsageInfo  minfo;
    STRUCTINIT( minfo );
    ScifRequest  minforeq( GET_MEMORY_UTILIZATION, (char*) &minfo, sizeof( minfo ) );
    if (MicDeviceError::isError( m_pData->mpScifDevice->request( &minforeq ) ))
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_IO_ERROR );

    data.mTotal    = MicMemory( minfo.total, MicMemory::eKilo );
    data.mUsed     = MicMemory( minfo.used, MicMemory::eKilo );
    data.mFree     = MicMemory( minfo.free, MicMemory::eKilo );
    data.mBuffers  = MicMemory( minfo.buffers, MicMemory::eKilo );
    data.mCached   = MicMemory( minfo.cached, MicMemory::eKilo );

    data.mValid = true;

    *info = MicMemoryUsageInfo( data );

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlDevice::getDeviceFlashUpdateStatus( FlashStatus* status ) const
 *  @param  status  Pointer to status return object
 *  @return error code
 *
 *  Retrieves and returns flash update operation \a status.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_NO_ACCESS
 *  - MICSDKERR_DEVICE_IO_ERROR
 *  - MICSDKERR_PROPERTY_NOT_FOUND
 *  - MICSDKERR_INTERNAL_ERROR
 */

template <class Base, class Mpss, class MpssCreator, class ScifDev>
uint32_t  KnlDeviceAbstract<Base, Mpss, MpssCreator, ScifDev>::getDeviceFlashUpdateStatus( FlashStatus* status ) const
{
    if (!status)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t  statSpad = 0;
    uint32_t  result   = 0;

    result = m_pData->mpMpssStack->getDeviceProperty( &statSpad, PROPKEY_SPAD_STATUS, 16 );
    if (MicDeviceError::isError(result))
        return  result;

    int   flashStatus = statSpad & 0x000f;
    int   progress  = (statSpad >>  8) & 0x007f;
    int   stagenum  = (statSpad >> 16) & 0x000f;
    int   updateComplete = (statSpad & 0x7);

    typename Base::FlashStage currentStage =  Base::eIdleStage;

    /*
    We have only three valid transitions. One for each firmware capsule.

    eIdleStage -> eBiosStage(eBusy) -> eBiosStage(eStageComplete) -> eNextStage
    eIdleStage -> eSmcStage(eBusy)  -> eBiosStage(eStageComplete) -> eNextStage
    eIdleStage -> eMeStage(eBusy)   -> eBiosStage(eStageComplete) -> eNextStage

    We also have some error transitions defined

    eIdleStage -> eNextStage : iflash failed to start.
    eIdleStage -> eErrorStage : we reached the efi script end without complete flashing process.
    eIdleStage -> e***Stage(eBusy) -> e***Stage(eFailed) : iflash32 failed to update firmware.
    eIdleStage -> e***Stage(eBusy) -> e***Stage(eAuthFailedVM) : Firmware Update Protection is enabled.

    Any other path will cause a failed state.
    */

    switch (stagenum & 0xf) //spad4 capsule type
    {
      case 0:
        currentStage = Base::eIdleStage;
        break;

      case 1:
        currentStage = Base::eBiosStage;
        m_completed.bios = m_fwToUpdate.bios ? (updateComplete == 2) : true;
        break;

      case 2:
        currentStage = Base::eMeStage;
        m_completed.me = m_fwToUpdate.me ? (updateComplete == 2) : true;
        break;

      case 4:
        currentStage = Base::eSmcStage;
        m_completed.smc = m_fwToUpdate.smc ? (updateComplete == 2) : true;
        break;

      case 14:   // Stage barrier
        currentStage = Base::eNextStage;
        break;

      case 15:   // Error, reached the end of the efi script without complete flashing process
        currentStage = Base::eErrorStage;
        break;

      default:
        currentStage = Base::eErrorStage;
        break;
    }

     m_completed.lastStagenum = stagenum;

    switch (flashStatus & 0x000f)
    {
      case  0:      // Idle
        status->set( FlashStatus::eIdle, progress, currentStage );
        break;

      case  1:      // In progress
        status->set( FlashStatus::eBusy, progress, currentStage );
        break;

      case  2:      // Complete
        status->set( FlashStatus::eStageComplete, 100, currentStage );
        break;

      case  3:      // Failed
        status->set( FlashStatus::eFailed, 0, currentStage, flashStatus );
        status->setErrorText( Base::flashErrorAsText( currentStage, flashStatus ) );
        break;
      case  4:      // Authentication failed
        status->set( FlashStatus::eAuthFailedVM, 0, currentStage, flashStatus );
        break;

      default:
        status->set( FlashStatus::eFailed );
        break;
    }
    //We compare each current completion status against its expected outcome.
    //If a mismatch occurs after the  whole update process
    //times out, then an error occurred.
    if ((m_completed.bios == m_fwToUpdate.bios) &&
        (m_completed.me == m_fwToUpdate.me) &&
        (m_completed.smc == m_fwToUpdate.smc))
    {
        status->set(FlashStatus::eDone, progress, currentStage);
    }

    if (currentStage == Base::eErrorStage)
    {
        return  MicDeviceError::errorCode(MICSDKERR_INTERNAL_ERROR);
    }

    status->setStageText( Base::flashStageAsText( status->stage() ) );

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlDevice::getDeviceLedMode( uint32_t* mode ) const
 *  @param  mode    Pointer to LED mode return
 *  @return error code
 *
 *  Retrieve and return the current LED \a mode.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_DEVICE_IO_ERROR
 */

template <class Base, class Mpss, class MpssCreator, class ScifDev>
uint32_t  KnlDeviceAbstract<Base, Mpss, MpssCreator, ScifDev>::getDeviceLedMode( uint32_t* mode ) const
{
    if (!mode)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    struct DiagnosticsInfo  dinfo;
    STRUCTINIT( dinfo );
    ScifRequest  dinforeq( GET_DIAGNOSTICS_INFO, (char*) &dinfo, sizeof( dinfo ) );
    if (MicDeviceError::isError( m_pData->mpScifDevice->request( &dinforeq ) ))
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_IO_ERROR );

    *mode = dinfo.led_blink;

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlDevice::getDeviceEccMode( bool* enabled, bool* available ) const
 *  @param  enabled    Enabled state return
 *  @param  available  Available state return (optional)
 *  @return error code
 *
 *  Retrieve and return the current ECC mode enabled state.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_DEVICE_IO_ERROR
 */

template <class Base, class Mpss, class MpssCreator, class ScifDev>
uint32_t  KnlDeviceAbstract<Base, Mpss, MpssCreator, ScifDev>::getDeviceEccMode( bool* enabled, bool* available ) const
{
    if (!enabled)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    struct  MemoryInfo  meminfo;
    STRUCTINIT( meminfo );
    ScifRequest  meminforeq( GET_MEMORY_INFO, (char*) &meminfo, sizeof( meminfo ) );
    if (MicDeviceError::isError( m_pData->mpScifDevice->request( &meminforeq ) ))
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_IO_ERROR );

    *enabled = meminfo.ecc_enabled ? true : false;

    if (available)
        *available = true;      /// \todo Is this 100% certain?

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlDevice::getDeviceTurboMode( bool* enabled, bool* available, bool* active ) const
 *  @param  enabled    Pointer to enabled state return
 *  @param  available  Pointer to available state return (optional)
 *  @param  active     Pointer to active state return (optional)
 *  @return error code
 *
 *  Determine the current turbo mode \a enabled state, \a available state,
 *  and \a active state.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_DEVICE_IO_ERROR
 */

template <class Base, class Mpss, class MpssCreator, class ScifDev>
uint32_t  KnlDeviceAbstract<Base, Mpss, MpssCreator, ScifDev>::getDeviceTurboMode( bool* enabled, bool* available, bool* active ) const
{
    if (!enabled)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    struct TurboInfo  turboinfo;
    STRUCTINIT( turboinfo );
    ScifRequest  turboinforeq( GET_TURBO_INFO, (char*) &turboinfo, sizeof( turboinfo ) );
    if (MicDeviceError::isError( m_pData->mpScifDevice->request( &turboinforeq ) ))
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_IO_ERROR );

    *enabled = turboinfo.enabled ? true : false;

    if (available)
        *available = true;      // Oh yes, we support turbo

    if (active)
        *active = ((turboinfo.turbo_pct > 0) && turboinfo.enabled) ? true : false;  // We can't really tell

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlDevice::getSmBusAddressTrainingStatus( bool* busy, int* eta ) const
 *  @param  busy    Pointer to busy return variable
 *  @param  eta     Pointer to estimated time remaining return variable (optional)
 *  @return error code
 *
 *  Retrieve and return the SMBus training state and remaining time in
 *  milliseconds before completion.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_DEVICE_IO_ERROR
 *  -
 */

template <class Base, class Mpss, class MpssCreator, class ScifDev>
uint32_t  KnlDeviceAbstract<Base, Mpss, MpssCreator, ScifDev>::getDeviceSmBusAddressTrainingStatus( bool* busy, int* eta ) const
{
    if (!busy)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    struct SmbaInfo  smbainfo;
    STRUCTINIT( smbainfo );
    ScifRequest  smbainforeq( GET_SMBA_INFO, (char*) &smbainfo, sizeof( smbainfo ) );
    int status = m_pData->mpScifDevice->request( &smbainforeq );
    if ( MicDeviceError::isError( status ) )
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_IO_ERROR );

    *busy = smbainfo.is_busy ? true : false;

    if (eta)
        *eta = smbainfo.ms_remaining;

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlDevice::getDeviceSmcRegisterData( uint8_t offset, uint8_t* data, size_t* size ) const
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
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_INVALID_SMC_REG_OFFSET
 *  - MICSDKERR_SMC_OP_NOT_PERMITTED
 *  - MICSDKERR_NOT_SUPPORTED
 */

template <class Base, class Mpss, class MpssCreator, class ScifDev>
uint32_t  KnlDeviceAbstract<Base, Mpss, MpssCreator, ScifDev>::getDeviceSmcRegisterData( uint8_t offset, uint8_t* data, size_t* size ) const
{
    typename Base::AccessMode  access;
    long int nbytes = Base::smcRegisterSize( offset, &access );
    if (nbytes < 0)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_SMC_REG_OFFSET );

    if ((access == Base::eNoAccess) || (access == Base::eWriteOnly))
        return  MicDeviceError::errorCode( MICSDKERR_SMC_OP_NOT_PERMITTED );

    if (!data && size && (*size == 0))   // User asking for required data buffer size?
    {
        if(offset == knldevice_detail::SEL_ENTRY_SELECTION_REGISTER)
            return getSELEntrySelectionRegisterSize(size);
        *size = nbytes;
        return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
    }

    if (!data || !size)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );         // No returns

    // Temporary buffer for the SCIF request
    std::vector<char> buf(nbytes);
    ScifRequest  smcRequest( READ_SMC_REG, offset, &buf[0], static_cast<size_t>(nbytes) );
    if (MicDeviceError::isError( m_pData->mpScifDevice->smcRequest( &smcRequest ) ))
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_IO_ERROR );

    // Copy data from temporary buffer, adjust nbytes to fit in user's buffer
    nbytes = (*size <= static_cast<size_t>(nbytes)) ?
              static_cast<long int>(*size) :
              nbytes;
    std::copy(&buf[0], &buf[nbytes], data);

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}

template <class Base, class Mpss, class MpssCreator, class ScifDev>
uint32_t  KnlDeviceAbstract<Base, Mpss, MpssCreator, ScifDev>::getSELEntrySelectionRegisterSize(size_t* size) const
{

    if (!size)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    char data;
    ScifRequest  smcRequest( READ_SMC_REG, knldevice_detail::SEL_ENTRY_SELECTION_REGISTER, &data, 1 );
    if (MicDeviceError::isError( m_pData->mpScifDevice->smcRequest( &smcRequest ) ))
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_IO_ERROR );

    *size = 1;
    /* 1 byte of cmd plus:
    0x40  Get Info
    0x41  Get Alloc Info
    0x43  Get Entry (6 byte request arguments)
    0x44  Add Entry (16 byte request arguments)
    0x47  SEL Clear (6 byte request arguments)
    0x48  Get Time
    0x49  Set Time (4 byte request arguments)
    All other values reserved
    */
    switch(data)
    {
    case 0x43:
    case 0x47:
        *size += 6;
        break;
    case 0x44:
        *size += 16;
        break;
    case 0x49:
        *size += 4;
        break;
    default:
        break; // If unknown command, just return it
    }

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}
//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlDevice::getDevicePostCode( std::string* code ) const
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
 *  - MICSDKERR_INTERNAL_ERROR
 */

template <class Base, class Mpss, class MpssCreator, class ScifDev>
uint32_t  KnlDeviceAbstract<Base, Mpss, MpssCreator, ScifDev>::getDevicePostCode( std::string* code ) const
{
    if (!code)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t  result = m_pData->mpMpssStack->getDeviceProperty( code, PROPKEY_POST_CODE );
    if (MicDeviceError::isError( result ))
        return  result;

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlDevice::deviceOpen()
 *  @return error code
 *
 *  Open device and return success state.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_DEVICE_OPEN_FAILED
 *  - MICSDKERR_DEVICE_IO_ERROR
 *  - MICSDKERR_SHARED_LIBRARY_ERROR
 *  - MICSDKERR_VERSION_MISMATCH
 */

template <class Base, class Mpss, class MpssCreator, class ScifDev>
uint32_t  KnlDeviceAbstract<Base, Mpss, MpssCreator, ScifDev>::deviceOpen()
{
    // Systoolsd may take some time to start when the card
    // is coming out of a reboot. Retry a few times and sleep
    // when failure happens.
    uint32_t error = MICSDKERR_DEVICE_OPEN_FAILED;
    for (uint8_t retry = 0; retry < MAX_RETRIES; ++retry)
    {
        error = m_pData->mpScifDevice->open();
        if (error)
        {
            MsTimer::sleep(RETRY_DELAY_MS);
            continue;
        }
        break;
    }
    if(MicDeviceError::isError(error))
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_OPEN_FAILED );

#if defined( SYSTOOLSD_MAJOR_VER ) && defined( SYSTOOLSD_MINOR_VER )

    struct SystoolsdInfo  protoinfo;
    STRUCTINIT( protoinfo );
    ScifRequest  protoinforeq( GET_SYSTOOLSD_INFO, (char*) &protoinfo, sizeof( protoinfo ) );
    if (MicDeviceError::isError( m_pData->mpScifDevice->request( &protoinforeq ) ))
        return MicDeviceError::errorCode( MICSDKERR_DEVICE_IO_ERROR );

    if ((protoinfo.major_ver != SYSTOOLSD_MAJOR_VER) || (protoinfo.minor_ver != SYSTOOLSD_MINOR_VER))
        return  MicDeviceError::errorCode( MICSDKERR_VERSION_MISMATCH );

#endif

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     void  KnlDevice::deviceClose()
 *
 *  Close device.
 */

template <class Base, class Mpss, class MpssCreator, class ScifDev>
void  KnlDeviceAbstract<Base, Mpss, MpssCreator, ScifDev>::deviceClose()
{
    m_pData->mpScifDevice->close();
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlDevice::deviceBoot( const MicBootConfigInfo& info )
 *  @param  info    Boot configuration
 *  @return error code
 *
 *  Boots the device.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_INTERNAL_ERROR
 *  - MICSDKERR_INVALID_DEVICE_NUMBER
 *  - MICSDKERR_SHARED_LIBRARY_ERROR
 *  - MICSDKERR_NO_ACCESS
 */

template <class Base, class Mpss, class MpssCreator, class ScifDev>
uint32_t KnlDeviceAbstract<Base, Mpss, MpssCreator, ScifDev>::deviceBoot( const MicBootConfigInfo& info )
{
    if (!m_pData->mpMpssStack)
        return  MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );

    return  m_pData->mpMpssStack->deviceBoot( info );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlDevice::deviceReset( bool force )
 *  @param  force   Force reset flag
 *  @return error code
 *
 *  Resets the device.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INTERNAL_ERROR
 */

template <class Base, class Mpss, class MpssCreator, class ScifDev>
uint32_t  KnlDeviceAbstract<Base, Mpss, MpssCreator, ScifDev>::deviceReset( bool force )
{
    if (!m_pData->mpMpssStack)
        return  MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );

    return  m_pData->mpMpssStack->deviceReset( force );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlDevice::setDeviceLedMode( uint32_t mode )
 *  @param  mode    LED mode
 *  @return error code
 *
 *  Activate specified LED \a mode.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_DEVICE_IO_ERROR
 */

template <class Base, class Mpss, class MpssCreator, class ScifDev>
uint32_t  KnlDeviceAbstract<Base, Mpss, MpssCreator, ScifDev>::setDeviceLedMode( uint32_t mode )
{
    ScifRequest  dinforeq( SET_LED_BLINK, (mode & knldevice_detail::LED_MODE_MASK) );
    if (MicDeviceError::isError( m_pData->mpScifDevice->request( &dinforeq ) ))
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_IO_ERROR );

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlDevice::setDeviceTurboModeEnabled( bool state )
 *  @param  state   Enabled state
 *  @return error code
 *
 *  Enable or disable the device turbo mode.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_DEVICE_IO_ERROR
 */

template <class Base, class Mpss, class MpssCreator, class ScifDev>
uint32_t  KnlDeviceAbstract<Base, Mpss, MpssCreator, ScifDev>::setDeviceTurboModeEnabled( bool state )
{
    ScifRequest  turboreq( SET_TURBO, (state ? 1 : 0) );
    if (MicDeviceError::isError( m_pData->mpScifDevice->request( &turboreq ) ))
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_IO_ERROR );

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlDevice::setDeviceSmcRegisterData( uint8_t offset, const uint8_t* data, size_t size )
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

template <class Base, class Mpss, class MpssCreator, class ScifDev>
uint32_t  KnlDeviceAbstract<Base, Mpss, MpssCreator, ScifDev>::setDeviceSmcRegisterData( uint8_t offset, const uint8_t* data, size_t size )
{
    if (!data || !size)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    typename Base::AccessMode  access;
    int         nbytes = Base::smcRegisterSize( offset, &access );
    if (nbytes < 0)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_SMC_REG_OFFSET );

    if ((access == Base::eNoAccess) || (access == Base::eReadOnly))
        return  MicDeviceError::errorCode( MICSDKERR_SMC_OP_NOT_PERMITTED );

    ScifRequest  smcRequest( WRITE_SMC_REG, offset, (char*) data, size );
    if (MicDeviceError::isError( m_pData->mpScifDevice->smcRequest( &smcRequest ) ))
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_IO_ERROR );

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlDevice::setDevicePowerThreshold( MicDevice::PowerWindow window, uint32_t power, uint32_t time )
 *  @param  window  Power window
 *  @param  power   Power threshold in microWatts
 *  @param  time    Window duration in microseconds
 *  @return error code
 *
 *  Set the \a power threshold for specified power \a window in microWatts and set the
 *  window duration to \a time Microseconds.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_DEVICE_IO_ERROR
 */

template <class Base, class Mpss, class MpssCreator, class ScifDev>
uint32_t  KnlDeviceAbstract<Base, Mpss, MpssCreator, ScifDev>::setDevicePowerThreshold( MicDevice::PowerWindow window, uint32_t power, uint32_t time )
{
    int  command;

    switch  (window)
    {
      case  MicDevice::eWindow0:
        command = SET_PTHRESH_W0;
        break;

      case  MicDevice::eWindow1:
        command = SET_PTHRESH_W1;
        break;

      default:
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );
    }

    /*
     * Time is always rounded to follow the formula: time = (N*61)<<4 where N > 0
     * Check that N is not zero.
     */
    if (( time>>4 )/61 == 0)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    struct PowerWindowInfo  pwinfo;
    pwinfo.threshold   = power;
    pwinfo.time_window = time;

    ScifRequest  pwinforeq( command, (const char*) &pwinfo, sizeof( pwinfo ) );
    if (MicDeviceError::isError( m_pData->mpScifDevice->request( &pwinforeq ) ))
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_IO_ERROR );

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlDevice::startDeviceSmBusAddressTraining( int hint, int timeout )
 *  @param  hint     SMBus base address hint
 *  @param  timeout  Timeout in milliseconds optional)
 *  @return error code
 *
 *  Start the SMBus address training procedure using given base address
 *  \a hint.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_DEVICE_BUSY
 *  - MICSDKERR_DEVICE_IO_ERROR
 */

template <class Base, class Mpss, class MpssCreator, class ScifDev>
uint32_t  KnlDeviceAbstract<Base, Mpss, MpssCreator, ScifDev>::startDeviceSmBusAddressTraining( int hint, int timeout )
{
    bool      busy   = false;
    uint32_t  result = getDeviceSmBusAddressTrainingStatus( &busy );

    if (MicDeviceError::isError( result ))
        return  result;

    if (busy)
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_BUSY );

    ScifRequest  smbaresetreq( RESTART_SMBA, static_cast<uint32_t>( hint & 0xff ) );
    if (MicDeviceError::isError( m_pData->mpScifDevice->request( &smbaresetreq ) ))
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_IO_ERROR );

    // Keep track of our long winding operation
    m_pData->mpSmBusTimer->reset( knldevice_detail::SMBUS_TRAINING_DURATION_MS );
    m_pData->mSmBusBusy = true;

    if (timeout > 0)
        MsTimer::sleep( static_cast<unsigned int>( timeout ) );

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlDevice::startDeviceFlashUpdate( FlashImage* image, bool force )
 *  @param  image   Pointer to flash image
 *  @param  force   Force flash flag
 *  @return error code
 *
 *  Initiate a flash update with specified flash \a image.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_SHARED_LIBRARY_ERROR
 *  - MICSDKERR_NO_ACCESS
 */

template <class Base, class Mpss, class MpssCreator, class ScifDev>
uint32_t  KnlDeviceAbstract<Base, Mpss, MpssCreator, ScifDev>::startDeviceFlashUpdate( FlashImage* image, bool force )
{
    if (!image || !image->isValid())
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    /// \todo Implement version check and determine need for force flag
    (void) force;

    MicBootConfigInfo  bootinfo( image->filePath() );
    bootinfo.setCustomState(true);
    FlashHeaderInterface::ItemVersionMap versions = image->itemVersions();
    for(auto it = versions.begin();it != versions.end(); ++it){
        if(it->first.compare(FlashImage::BIOS) == 0){
            m_fwToUpdate.bios = true;
        }
        else if(it->first.compare(FlashImage::ME) == 0){
            m_fwToUpdate.me = true;
        }
        else if(it->first.compare(FlashImage::SMCFABA) == 0){
            m_fwToUpdate.smc = true;
        }
        else if(it->first.compare(FlashImage::SMCFABB) == 0){
            m_fwToUpdate.smc = true;
        }
    }
    return  m_pData->mpMpssStack->deviceBoot( bootinfo );
}

template <class Base, class Mpss, class MpssCreator, class ScifDev>
uint32_t  KnlDeviceAbstract<Base, Mpss, MpssCreator, ScifDev>::startDeviceFlashUpdate( std::string & path, std::unique_ptr<micmgmt::FwToUpdate> & fwToUpdate )
{
    if (path.empty())
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );
    MicBootConfigInfo  bootinfo(path);
    bootinfo.setCustomState(true);
    m_fwToUpdate.bios = fwToUpdate->mBios;
    m_fwToUpdate.smc = fwToUpdate->mSMC;
    m_fwToUpdate.me = fwToUpdate->mME;
    return  m_pData->mpMpssStack->deviceBoot( bootinfo );
}

//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlDevice::finalizeDeviceFlashUpdate( bool abort )
 *  @param  abort   Abort flag
 *  @return error code
 *
 *  Finialize the flash update sequence.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_DEVICE_BUSY
 *  - MICSDKERR_SHARED_LIBRARY_ERROR
 *  - MICSDKERR_NO_ACCESS
 */

template <class Base, class Mpss, class MpssCreator, class ScifDev>
uint32_t  KnlDeviceAbstract<Base, Mpss, MpssCreator, ScifDev>::finalizeDeviceFlashUpdate( bool abort )
{
    FlashStatus  status;
    uint32_t     result = getDeviceFlashUpdateStatus( &status );
    if (MicDeviceError::isError( result ))
        return  result;

    if (!abort && !status.isCompleted())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_BUSY );

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}

template <class Base, class Mpss, class MpssCreator, class ScifDev>
ScifDev* KnlDeviceAbstract<Base, Mpss, MpssCreator, ScifDev>::getScifDev() const
{
    return m_pData->mpScifDevice.get();
}

template <class Base, class Mpss, class MpssCreator, class ScifDev>
Mpss* KnlDeviceAbstract<Base, Mpss, MpssCreator, ScifDev>::getMpss() const
{
    return m_pData->mpMpssStack.get();
}

template <class Base, class Mpss, class MpssCreator, class ScifDev>
void KnlDeviceAbstract<Base, Mpss, MpssCreator, ScifDev>::setBinariesToUpdate(
    bool bios, bool me, bool smc, int lastStagenum )
{
    m_fwToUpdate.bios = bios;
    m_fwToUpdate.me = me;
    m_fwToUpdate.smc = smc;
    m_fwToUpdate.lastStagenum = lastStagenum;
}
//----------------------------------------------------------------------------

}   // namespace micmgmt

//----------------------------------------------------------------------------

#endif // MICMGMT_KNLDEVICE_HPP
