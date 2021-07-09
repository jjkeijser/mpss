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
#include    "KnlUnitTestDevice.hpp"


#include    "micmgmtCommon.hpp"
#include    "MicPlatformInfo.hpp"
#include    "MicPowerThresholdInfo.hpp"
#include    "MicVersionInfo.hpp"
#include    "MicDeviceError.hpp"
#include    "FlashImage.hpp"
#include    "MicPower.hpp"
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
#include    "CoreUsageData_p.hpp"
#include    "PowerUsageData_p.hpp"
#include    "PowerThresholdData_p.hpp"
#include    "MemoryUsageData_p.hpp"
#include    "VersionInfoData_p.hpp"
#include    "MpssStackBase.hpp"

// SYSTEM INCLUDES
//
#include    <iostream>
#include    <sstream>
#include    <iomanip>
#include    <unordered_map>
#include    <chrono>
#include    <mutex>

// LOCAL CONSTANTS
//
namespace  {
const int          KNC_FLASH_TIME    = 15000;

// To get randomness...
uint64_t gSeed_ = 0;
std::mutex gSeedMtx_;
}

// PRIVATE DATA
//
namespace  micmgmt {
struct  KnlUnitTestDevice::PrivData
{
    // For UT Scenarios...
    uint32_t startFlashErrorCode_;
    uint8_t flashFailIteration_;
    std::string lastStageForError_;
    uint32_t flashFailErrorCode_;
    uint8_t flashMaxIterations_;
    uint8_t flashCurrentIteration_;

    uint32_t getDeviceVersionInfoErrorCode_;

    size_t nextSmcRegisterNSize_;
    unsigned int powerThresholds_[2];
    unsigned int timeWindows_[2];
    int postCode_;
    std::vector<uint8_t> kncWritableRegisters_;
    std::unordered_map<uint8_t, std::vector<uint8_t>> knlRegisterData_;
    int SMBABusyCounter_;
    uint8_t SMBAddress_;
    unsigned int mockSMBATimeout_;
    uint32_t apiInjectedError_;
    FlashStatus::State currentState_;
};
}

// NAMESPACES
//
using namespace  micmgmt;
using namespace  std;


//============================================================================
/** @class    micmgmt::KnlUnitTestDevice      KnlUnitTestDevice.hpp
 *  @ingroup  sdk
 *  @brief    The class represents a KNC unit test device
 *
 *  The \b %KnlUnitTestDevice class encapsulates a KNC unit test device, which
 *  is a specialized KNC mock device supporting test scenarios.
 */


//============================================================================
//  P U B L I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     KnlUnitTestDevice::KnlUnitTestDevice( int number )
 *  @param  number  Device number
 *
 *  Construct a KNC unit test device with given device \a number.
 */

KnlUnitTestDevice::KnlUnitTestDevice( int number ) :
    KnlMockDevice( number ),
    m_pData( new PrivData )
{
    scenarioReset();
}


//----------------------------------------------------------------------------
/** @fn     KnlUnitTestDevice::~KnlUnitTestDevice()
 *
 *  Cleanup.
 */

KnlUnitTestDevice::~KnlUnitTestDevice()
{
    delete  m_pData;
}


//============================================================================
//  P R I V A T E   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlUnitTestDevice::getDeviceVersionInfo( MicVersionInfo* info ) const
 *  @param  info    Pointer to version return
 *  @return error code
 *
 *  Retrieve and return device version \a info.
 */

uint32_t  KnlUnitTestDevice::getDeviceVersionInfo( MicVersionInfo* info ) const
{
    if (m_pData->getDeviceVersionInfoErrorCode_ != 0)
    {
        uint32_t error = m_pData->getDeviceVersionInfoErrorCode_;
        m_pData->getDeviceVersionInfoErrorCode_ = 0;
        return MicDeviceError::errorCode(error);
    }

    return  KnlMockDevice::getDeviceVersionInfo( info );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlUnitTestDevice::getDevicePlatformInfo( MicPlatformInfo* info ) const
 *  @param  info  Pointer to platform info return
 *  @return error code
 *
 *  Retrieve platform information into specified \a info object.
 */

uint32_t  KnlUnitTestDevice::getDevicePlatformInfo( MicPlatformInfo* info ) const
{
    if (m_pData->apiInjectedError_ != 0)
    { // Programmed Mock Error...
        uint32_t error = m_pData->apiInjectedError_;
        m_pData->apiInjectedError_ = 0;
        return MicDeviceError::errorCode(error);
    }

    return  KnlMockDevice::getDevicePlatformInfo( info );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlUnitTestDevice::getDevicePowerThresholdInfo( MicPowerThresholdInfo* info ) const
 *  @param  info  Pointer to power threshold info return
 *  @return error code
 *
 *  Retrieve power threshold information into specified \a info object.
 */

uint32_t  KnlUnitTestDevice::getDevicePowerThresholdInfo( MicPowerThresholdInfo* info ) const
{
    if (m_pData->apiInjectedError_ != 0)
    { // Programmed Mock Error...
        uint32_t error = m_pData->apiInjectedError_;
        m_pData->apiInjectedError_ = 0;
        return MicDeviceError::errorCode(error);
    }

    if (!info)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    PowerThresholdData data;
    data.mWindow0Threshold = m_pData->powerThresholds_[0];
    data.mWindow1Threshold = m_pData->powerThresholds_[1];
    data.mWindow0Time = m_pData->timeWindows_[0];
    data.mWindow1Time = m_pData->timeWindows_[1];
    data.mValid = true;
    data.mHiThreshold = data.mWindow1Threshold;
    data.mLoThreshold = data.mLoThreshold;
    data.mPersistence = m_SmcPersist;
    data.mMaximum = 0;
    MicPowerThresholdInfo idata(data);
    *info = idata;

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlUnitTestDevice::getDeviceFlashUpdateStatus( FlashStatus* status ) const
 *  @param  status  Pointer to status return object
 *  @return error code
 *
 *  Retrieves and returns flash operation \a status.
 */

uint32_t  KnlUnitTestDevice::getDeviceFlashUpdateStatus( FlashStatus* status ) const
{
    if (!status)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    if(getMockStatus())
        return  MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );

    bool isError;
    int  flashStatus = m_Spad & 0x000f;
    int  progress = (m_Spad >>  8) & 0x007f;
    int  stagenum = (m_Spad >> 16) & 0x000f;
    int  updateComplete = (m_Spad & 0x7);

    KnlDeviceBase::FlashStage currentStage =  KnlDeviceBase::eIdleStage;

    isError = m_completed.lastStagenum == stagenum || !m_completed.lastStagenum;
    switch (stagenum & 0xf) //spad4 capsule type
    {
      case 0:
        currentStage = KnlDeviceBase::eIdleStage;
        break;

      case 1:
        currentStage = isError? KnlDeviceBase::eBiosStage : KnlDeviceBase::eErrorStage;
        m_completed.bios = m_fwToUpdate.bios ? (updateComplete == 2) : true;
        break;

      case 2:
        currentStage = isError? KnlDeviceBase::eMeStage : KnlDeviceBase::eErrorStage;
        m_completed.me = m_fwToUpdate.me ? (updateComplete == 2) : true;
        break;

      case 4:
        currentStage = isError? KnlDeviceBase::eSmcStage : KnlDeviceBase::eErrorStage;
        m_completed.smc = m_fwToUpdate.smc ? (updateComplete == 2) : true;
        break;

      case 14:   // Stage barrier
        currentStage = KnlDeviceBase::eNextStage;
        break;

      case 15:   // Error, reached the end of the efi script without complete flashing process
        currentStage = KnlDeviceBase::eErrorStage;
        break;

      default:
        currentStage = KnlDeviceBase::eErrorStage;
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
        status->setErrorText( KnlDeviceBase::flashErrorAsText( currentStage, flashStatus ) );
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

    if (currentStage == KnlDeviceBase::eErrorStage)
    {
        return  MicDeviceError::errorCode(MICSDKERR_INTERNAL_ERROR);
    }

    status->setStageText( KnlDeviceBase::flashStageAsText( status->stage() ) );

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}

bool KnlUnitTestDevice::getMockStatus() const
{

    if(m_StartDelay)
    {
        m_StartDelay--;
        return false;
    }
    switch(m_FSM)
    {
      // Normal
      case   0: m_Spad= 0x00000000; break; // Idle
      case   1: m_Spad= 0x00010001; break; // Bios 0%
      case   2: m_Spad= 0x00011901; break; // Bios 25%
      case   3: m_Spad= 0x00013201; break; // Bios 50%
      case   4: m_Spad= 0x00014b01; break; // Bios 75%
      case   5: m_Spad= 0x00016402; break; // Bios 100%
      case   6: m_Spad= 0x000e0000; break; // Sentinel
      case   7: m_Spad= 0x00000000; break; // Idle
      case   8: m_Spad= 0x00040001; break; // SMC 0%
      case   9: m_Spad= 0x00041901; break; // SMC 25%
      case  10: m_Spad= 0x00043201; break; // SMC 50%
      case  11: m_Spad= 0x00044b01; break; // SMC 75%
      case  12: m_Spad= 0x00046402; break; // SMC 100%
      case  13: m_Spad= 0x000e0000; break; // Sentinel
      case  14: m_Spad= 0x00000000; break; // Idle
      case  15: m_Spad= 0x00020001; break; // ME 0%
      case  16: m_Spad= 0x00021901; break; // ME 25%
      case  17: m_Spad= 0x00023201; break; // ME 50%
      case  18: m_Spad= 0x00024b01; break; // ME 75%
      case  19: m_Spad= 0x00026402; break; // ME 100%
      case  20: m_Spad= 0x000e0000; break; // Sentinel
      // Only Bios
      case 100: m_Spad= 0x00000000; break; // Idle
      case 101: m_Spad= 0x00010001; break; // Bios 0%
      case 102: m_Spad= 0x00011901; break; // Bios 25%
      case 103: m_Spad= 0x00013201; break; // Bios 50%
      case 104: m_Spad= 0x00014b01; break; // Bios 75%
      case 105: m_Spad= 0x00016402; break; // Bios 100%
      case 106: m_Spad= 0x000e0000; break; // Sentinel
      case 107: m_Spad= 0x000f0000; break; // Sentinel error
      // Only SMC
      case 200: m_Spad= 0x00000000; break; // Idle
      case 201: m_Spad= 0x00040001; break; // SMC 0%
      case 202: m_Spad= 0x00041901; break; // SMC 25%
      case 203: m_Spad= 0x00043201; break; // SMC 50%
      case 204: m_Spad= 0x00044b01; break; // SMC 75%
      case 205: m_Spad= 0x00046402; break; // SMC 100%
      case 206: m_Spad= 0x000e0000; break; // Sentinel
      case 207: m_Spad= 0x000f0000; break; // Sentinel error
      // Only ME
      case 300: m_Spad= 0x00000000; break; // Idle
      case 301: m_Spad= 0x00020001; break; // ME 0%
      case 302: m_Spad= 0x00021901; break; // ME 25%
      case 303: m_Spad= 0x00023201; break; // ME 50%
      case 304: m_Spad= 0x00024b01; break; // ME 75%
      case 305: m_Spad= 0x00026402; break; // ME 100%
      case 306: m_Spad= 0x000e0000; break; // Sentinel
      case 307: m_Spad= 0x000f0000; break; // Sentinel error
      // VM Error
      case 400: m_Spad= 0x00000000; break; // Idle
      case 401: m_Spad= 0x00010001; break; // Bios 0%
      case 402: m_Spad= 0x00011901; break; // Bios 25%
      case 403: m_Spad= 0x00013201; break; // Bios 50%
      case 404: m_Spad= 0x00014b01; break; // Bios 75%
      case 405: m_Spad= 0x00014b04; break; // Bios VM
      // Flash Error
      case 500: m_Spad= 0x00000000; break; // Idle
      case 501: m_Spad= 0x00010001; break; // Bios 0%
      case 502: m_Spad= 0x00011901; break; // Bios 25%
      case 503: m_Spad= 0x00013201; break; // Bios 50%
      case 504: m_Spad= 0x00014b01; break; // Bios 75%
      case 505: m_Spad= 0x00016403; break; // Bios flash error
      // No BIOS
      case 600: m_Spad= 0x00000000; break; // Idle
      case 601: m_Spad= 0x00040001; break; // SMC 0%
      case 602: m_Spad= 0x00041901; break; // SMC 25%
      case 603: m_Spad= 0x00043201; break; // SMC 50%
      case 604: m_Spad= 0x00044b01; break; // SMC 75%
      case 605: m_Spad= 0x00046402; break; // SMC 100%
      case 606: m_Spad= 0x000e0000; break; // Sentinel
      case 607: m_Spad= 0x00000000; break; // Idle
      case 608: m_Spad= 0x00020001; break; // ME 0%
      case 609: m_Spad= 0x00021901; break; // ME 25%
      case 610: m_Spad= 0x00023201; break; // ME 50%
      case 611: m_Spad= 0x00024b01; break; // ME 75%
      case 612: m_Spad= 0x00026402; break; // ME 100%
      case 613: m_Spad= 0x000e0000; break; // Sentinel
      // No SMC
      case 700: m_Spad= 0x00000000; break; // Idle
      case 701: m_Spad= 0x00010001; break; // Bios 0%
      case 702: m_Spad= 0x00011901; break; // Bios 25%
      case 703: m_Spad= 0x00013201; break; // Bios 50%
      case 704: m_Spad= 0x00014b01; break; // Bios 75%
      case 705: m_Spad= 0x00016402; break; // Bios 100%
      case 706: m_Spad= 0x000e0000; break; // Sentinel
      case 707: m_Spad= 0x00000000; break; // Idle
      case 708: m_Spad= 0x00020001; break; // ME 0%
      case 709: m_Spad= 0x00021901; break; // ME 25%
      case 710: m_Spad= 0x00023201; break; // ME 50%
      case 711: m_Spad= 0x00024b01; break; // ME 75%
      case 712: m_Spad= 0x00026402; break; // ME 100%
      case 713: m_Spad= 0x000e0000; break; // Sentinel
      // No ME
      case 800: m_Spad= 0x00000000; break; // Idle
      case 801: m_Spad= 0x00010001; break; // Bios 0%
      case 802: m_Spad= 0x00011901; break; // Bios 25%
      case 803: m_Spad= 0x00013201; break; // Bios 50%
      case 804: m_Spad= 0x00014b01; break; // Bios 75%
      case 805: m_Spad= 0x00016402; break; // Bios 100%
      case 806: m_Spad= 0x000e0000; break; // Sentinel
      case 807: m_Spad= 0x00000000; break; // Idle
      case 808: m_Spad= 0x00040001; break; // SMC 0%
      case 809: m_Spad= 0x00041901; break; // SMC 25%
      case 810: m_Spad= 0x00043201; break; // SMC 50%
      case 811: m_Spad= 0x00044b01; break; // SMC 75%
      case 812: m_Spad= 0x00046402; break; // SMC 100%
      case 813: m_Spad= 0x000e0000; break; // Sentinel
      // iflash error
      case 900: m_Spad= 0x00000000; break; // Idle
      case 901: m_Spad= 0x000e0000; break; // Sentinel

      default: m_Spad= 0x000f0000; break;
    }
    m_FSM++;
    return false;
}

//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlUnitTestDevice::getDeviceSmcRegisterData( uint8_t offset, uint8_t* data, size_t* size ) const
 *  @param  offset  Register offset
 *  @param  data    Pointer to data buffer
 *  @param  size    Pointer to data size
 *  @return error code
 *
 *  Read SMC register content from register with given \a offset and return
 *  \a size bytes in specified \a data buffer.
 */

uint32_t  KnlUnitTestDevice::getDeviceSmcRegisterData( uint8_t offset, uint8_t* data, size_t* size ) const
{
    if (m_pData->apiInjectedError_ != 0)
    { // Programmed Mock Error...
        uint32_t error = m_pData->apiInjectedError_;
        m_pData->apiInjectedError_ = 0;
        return MicDeviceError::errorCode(error);
    }

    if (size == NULL || (*size != 0 && data == NULL)) // Added the corner case pointed out in a code review...
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );     // No returns

    AccessMode  access;
    int         nbytes = smcRegisterSize( offset, &access );
    if (offset == 0x20)
        nbytes = 4; // Special case read to sizes for 0x21 and 0x23....
    else if (offset == 0x21 || offset == 0x23)
    {
        nbytes = static_cast<int>(m_pData->knlRegisterData_[offset].size());
    }

    if ((nbytes < 0) || (access == eNoAccess) || (access == eWriteOnly))
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );     // Offset or access issue

    if (static_cast<int>(*size) > nbytes && data != NULL)
        return  MicDeviceError::errorCode(MICSDKERR_INVALID_ARG);     // Too much asked for

    if (*size == 0)
    {
        if (nbytes != 0)
        {
            *size = nbytes;
        }
        else
        {
            *size = m_pData->knlRegisterData_[offset].size(); // Mock behavior only...
        }
    }

    if (data != NULL)
    {
        if (offset != 0x20)
        {
            for (size_t i = 0; i < *size; i++)
                data[i] = static_cast<uint8_t>(m_pData->knlRegisterData_[offset][i]);    // Return something
        }
        else
        {
            uint32_t retSize = static_cast<uint32_t>(m_pData->knlRegisterData_[offset].size());
            memcpy((void*)data, (void*)&retSize, 4);
        }
    }
    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlUnitTestDevice::getDevicePostCode( std::string* code ) const
 *  @param  code    Pointer to post code return
 *  @return error code
 *
 *  Retrieve and return the post \a code string. The post code is often
 *  expressed as hexdecimal number, but it can also contain other character
 *  combinations. Therefore, the best practice if to leave the post code as
 *  string value.
 */

uint32_t  KnlUnitTestDevice::getDevicePostCode( std::string* code ) const
{
    if (m_pData->apiInjectedError_ != 0)
    { // Programmed Mock Error...
        uint32_t error = m_pData->apiInjectedError_;
        m_pData->apiInjectedError_ = 0;
        return MicDeviceError::errorCode(error);
    }

    if (!code)
        return  MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );

    if (m_pData->postCode_ < 0)
    {
        switch (m_DeviceState)
        {
        case  MicDevice::eReady:
        case  MicDevice::eOnline:
            code->assign("FF");
            break;
        default:
            code->assign("00");
            break;
        }
    }
    else
    {
        stringstream ss;
        ss << hex << setw(2) << setfill('0') << (m_pData->postCode_ & 0xff);
        (*code) = ss.str();
    }

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlUnitTestDevice::setDeviceSmcRegisterData( uint8_t offset, const uint8_t* data, size_t size )
 *  @param  offset  Register offset
 *  @param  data    Pointer to data buffer
 *  @param  size    Size of data buffer
 *  @return error code
 *
 *  Write specified \a data of specified \a size to the  SMC register with
 *  given \a offset.
 */

uint32_t  KnlUnitTestDevice::setDeviceSmcRegisterData( uint8_t offset, const uint8_t* data, size_t size )
{
    if (m_pData->apiInjectedError_ != 0)
    { // Programmed Mock Error...
        uint32_t error = m_pData->apiInjectedError_;
        m_pData->apiInjectedError_ = 0;
        return MicDeviceError::errorCode(error);
    }

    if (!data || (size == 0))
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    AccessMode  access;
    int         nbytes = smcRegisterSize( offset, &access );
    if ((nbytes < 0) || (access == eNoAccess) || (access == eReadOnly))
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );     // Offset or access issue

    if (nbytes != 0)
    {
        if (static_cast<int>(size) > nbytes)
            return  MicDeviceError::errorCode(MICSDKERR_INVALID_ARG);     // Too much asked for
    }

    m_pData->knlRegisterData_[offset].resize(size);
    for (size_t i = 0; i < size; ++i)
    {
        m_pData->knlRegisterData_[offset][i] = data[i];
    }

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlUnitTestDevice::startDeviceFlashUpdate( FlashImage* image, bool force )
 *  @param  image   Pointer to flash image
 *  @param  force   Force flash flag
 *  @return error code
 *
 *  Initiate a flash update with specified flash \a image.
 */

uint32_t  KnlUnitTestDevice::startDeviceFlashUpdate( FlashImage* image, bool force )
{
    if (m_pData->startFlashErrorCode_ != 0)
    { // Programmed Mock Error...
        uint32_t error = m_pData->startFlashErrorCode_;
        m_pData->startFlashErrorCode_ = 0;
        return MicDeviceError::errorCode( error );
    }
    if (!image || !image->isValid())
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    if (m_pFlashStatus->isBusy())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_BUSY );

    (void) force;

    string ext = image->filePath().substr(image->filePath().size() - 7);
    if (ext == ".css_ab")
    {
        m_pFlashStatus->set(FlashStatus::eBusy, 0, eSmcStage);
        m_pFlashStatus->setStageText(flashStageAsText(eSmcStage));
    }
    else
    {
        m_pFlashStatus->set( FlashStatus::eBusy, 0, KnlDeviceBase::FlashStage::eBiosStage);
        m_pFlashStatus->setStageText(flashStageAsText(KnlDeviceBase::FlashStage::eBiosStage));
    }
    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}

uint32_t  KnlUnitTestDevice::startDeviceFlashUpdate( std::string & path, std::unique_ptr<micmgmt::FwToUpdate> & fwToUpdate )
{
    if (m_pData->startFlashErrorCode_ != 0)
    { // Programmed Mock Error...
        uint32_t error = m_pData->startFlashErrorCode_;
        m_pData->startFlashErrorCode_ = 0;
        return MicDeviceError::errorCode( error );
    }
    if (path.empty())
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );
    if (m_pFlashStatus->isBusy())
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_BUSY );
    m_fwToUpdate.bios = fwToUpdate->mBios;
    m_fwToUpdate.smc = fwToUpdate->mSMC;
    m_fwToUpdate.me = fwToUpdate->mME;
    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}

//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlUnitTestDevice::finalizeDeviceFlashUpdate( bool abort )
 *  @param  abort   Abort flag
 *  @return error code
 *
 *  Finialize the flash update sequence.
 */

uint32_t  KnlUnitTestDevice::finalizeDeviceFlashUpdate( bool abort )
{
    (void) abort;   // Not used

    m_pFlashStatus->set( FlashStatus::eIdle, 0, eIdleStage );
    m_pFlashStatus->setStageText( flashStageAsText( eIdleStage ) );

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  KnlUnitTestDevice::setDevicePowerThreshold( MicDevice::PowerWindow window, uint32_t power, uint32_t time )
*  @param  window  Power window
*  @param  power   Power threshold in microWatts
*  @param  time    Window duration in microseconds
*  @return error code
*
*  Set the \a power threshold for specified power \a window and set the
*  window duration to \a time milliseconds.
*/

uint32_t  KnlUnitTestDevice::setDevicePowerThreshold( MicDevice::PowerWindow window, uint32_t power, uint32_t time)
{
    if (m_pData->apiInjectedError_ != 0)
    { // Programmed Mock Error...
        uint32_t error = m_pData->apiInjectedError_;
        m_pData->apiInjectedError_ = 0;
        return MicDeviceError::errorCode(error);
    }

    if ((window != MicDevice::PowerWindow::eWindow0) && (window != MicDevice::PowerWindow::eWindow1))
        return  MicDeviceError::errorCode(MICSDKERR_INVALID_ARG);

    int w = 0; // eWindows0
    if (window == MicDevice::PowerWindow::eWindow1)
    {
        w = 1;
    }

    m_pData->powerThresholds_[w] = power;
    m_pData->timeWindows_[w] = time;

    return  MicDeviceError::errorCode(MICSDKERR_SUCCESS);
}


uint32_t  KnlUnitTestDevice::startDeviceSmBusAddressTraining(int hint, int timeout)
{
    if (m_pData->apiInjectedError_ != 0)
    { // Programmed Mock Error...
        uint32_t error = m_pData->apiInjectedError_;
        m_pData->apiInjectedError_ = 0;
        return MicDeviceError::errorCode(error);
    }
    bool busy;
    if (getDeviceSmBusAddressTrainingStatus(&busy, NULL) != 0 || busy == true)
    {
        return MicDeviceError::errorCode(MICSDKERR_DEVICE_BUSY);
    }

    uint64_t r;
    chrono::high_resolution_clock::time_point t = chrono::high_resolution_clock::now();
    {
        lock_guard<mutex> guard(gSeedMtx_);
        gSeed_ = (((uint64_t)chrono::duration_cast<chrono::microseconds>(t.time_since_epoch()).count() ^ gSeed_) + 0x9f538a45) & 0x0fffffffffffffff;
        r = gSeed_;
    }
    // Pick 2 bits out of the random number bits (0x4080 was randomly picked). There is a 1:4 chance of the 2 bits both being set (1).
    if ((r & 0x4080) != 0x4080) // We want a 3:4 chance to get the requested hint range so reverse the comparison.
    { // I get my way!
        if (hint == KnlDeviceBase::eSmBusAddress_0x30)
        {
            m_pData->SMBAddress_ = (uint8_t)0x30;
        }
        else
        {
            m_pData->SMBAddress_ = (uint8_t)0x40;
        }
    }
    else
    { // I don't get my way!
        if (hint == KnlDeviceBase::eSmBusAddress_0x30)
        {
            m_pData->SMBAddress_ = (uint8_t)0x40;
        }
        else
        {
            m_pData->SMBAddress_ = (uint8_t)0x30;
        }
    }
    m_pData->SMBAddress_ |= (uint8_t)(r & 0x0f);

    if (timeout == 0)
    {
        m_pData->SMBABusyCounter_ = m_pData->mockSMBATimeout_; // Count of calls to getDeviceSmBusAddressTrainingStatus
    }
    else
    {
        m_pData->SMBABusyCounter_ = 0; // Pretend we waited an returned for Mock.
    }

    return  MicDeviceError::errorCode(MICSDKERR_SUCCESS);
}

uint32_t     KnlUnitTestDevice::getDeviceSmBusAddressTrainingStatus(bool* busy, int* eta) const
{
    if (m_pData->apiInjectedError_ != 0)
    { // Programmed Mock Error...
        uint32_t error = m_pData->apiInjectedError_;
        m_pData->apiInjectedError_ = 0;
        return MicDeviceError::errorCode(error);
    }

    if (m_pData->SMBABusyCounter_ > 0)
    {
        --(m_pData->SMBABusyCounter_);
    }
    if (eta != NULL)
    {
        *eta = m_pData->SMBABusyCounter_;
    }
    *busy = (m_pData->SMBABusyCounter_ > 0) ? true : false;

    return  MicDeviceError::errorCode(MICSDKERR_SUCCESS);
}

uint32_t  KnlUnitTestDevice::setDeviceSmcPersistenceEnabled(bool state)
{
    if (m_pData->apiInjectedError_ != 0)
    { // Programmed Mock Error...
        uint32_t error = m_pData->apiInjectedError_;
        m_pData->apiInjectedError_ = 0;
        return MicDeviceError::errorCode(error);
    }

    m_SmcPersist = state;

    return  MicDeviceError::errorCode(MICSDKERR_SUCCESS);
}

namespace
{
    vector<pair<uint8_t, vector<uint8_t>>> rawRegisterDataSet = {
        { 0x10, { 0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90, 0xa0, 0xb0, 0xc0, 0xd0, 0xe0, 0xf0 } },
        { 0x15, { 0x54, 0x65, 0x73, 0x74, 0x32, 0x53, 0x65, 0x72, 0x69, 0x61, 0x6c, 0x23 } }, // "Test Serial#"
        { 0x18, { 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70 } }, // "abcdefghijklmnop"
        { 0x19, { 0x00, 0x00, 0x0c, 0x29, 0x01, 0x04 } }, // 2014-01-29T12:00
        { 0x21, { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17 } }, // 24 bytes
        { 0x23, { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17 } }, // 24 bytes

        { 0x07, { 0x38, 0x00, 0x00, 0x00 } }, // SMBA 0x38
        { 0x11, { 0xff, 0x1f, 0x03, 0x02 } }, // 2.3.8191
        { 0x12, { 0x55, 0xaa, 0x00, 0x40 } },
        { 0x13, { 0x00, 0x00, 0x00, 0x00 } },
        { 0x14, { 0x00, 0x02, 0x01, 0x00 } }, // RPRD; Fab C; Passive; 225W
        { 0x16, { 0x39, 0x30, 0x0c, 0x05 } }, // 5.12.12345
        { 0x1a, { 0x00, 0x00, 0x00, 0x00 } },
        { 0x1c, { 0x00, 0x00, 0x00, 0x00 } }, // A0 stepping
        { 0x1e, { 0xe1, 0x00, 0x00, 0x00 } }, // 225W
        { 0x20, { 0x00, 0x18, 0x00, 0x00 } }, // 24 bytes
        { 0x22, { 0x00, 0x18, 0x00, 0x00 } }, // 24 bytes
        { 0x28, { 0x19, 0x00, 0x00, 0x00 } }, // 25W
        { 0x29, { 0x7d, 0x00, 0x00, 0x00 } }, // 125W
        { 0x2a, { 0xaf, 0x00, 0x00, 0x00 } }, // 175W
        { 0x2b, { 0x00, 0x00, 0x00, 0x00 } }, // Throttle reason not forced.
        { 0x35, { 0xb7, 0x00, 0x00, 0x00 } }, // 183W, status OK
        { 0x36, { 0xcc, 0x00, 0x00, 0x00 } }, // 204W, status OK
        { 0x3a, { 0x94, 0x00, 0x00, 0x00 } }, // 148W, status OK
        { 0x3b, { 0xe7, 0x00, 0x00, 0x00 } }, // 231W, status OK
        { 0x40, { 0x49, 0x00, 0x00, 0x00 } }, // 73C, status OK
        { 0x41, { 0x25, 0x00, 0x00, 0x00 } }, // 37C, status OK
        { 0x42, { 0x3e, 0x00, 0x00, 0x00 } }, // 62C, status OK
        { 0x43, { 0x4a, 0x00, 0x00, 0x00 } }, // 74C, status OK
        { 0x44, { 0x46, 0x00, 0x00, 0x00 } }, // 70C, status OK
        { 0x45, { 0x48, 0x00, 0x00, 0x00 } }, // 72C, status OK
        { 0x46, { 0x40, 0x00, 0x00, 0x00 } }, // 64C, status OK
        { 0x47, { 0x41, 0x00, 0x00, 0x00 } }, // 65C, status OK
        { 0x48, { 0x42, 0x00, 0x00, 0x00 } }, // 66C, status OK
        { 0x49, { 0xdc, 0x05, 0x00, 0x00 } }, // 1500RPM
        { 0x4a, { 0x3c, 0x00, 0x00, 0x00 } }, // 60%
        { 0x4b, { 0x00, 0x05, 0x00, 0x00 } }, // 0%
        { 0x4c, { 0x63, 0x00, 0x00, 0x00 } }, // 99C
        { 0x4d, { 0x69, 0x00, 0x00, 0x00 } }, // 105C
        { 0x4e, { 0x2c, 0x01, 0x00, 0x00 } }, // 300ms
        { 0x4f, { 0x00, 0x00, 0x00, 0x00 } }, // no throttling
        { 0x50, { 0x64, 0x00, 0x00, 0x00 } }, // 100mV; status OK
        { 0x51, { 0x65, 0x00, 0x00, 0x00 } }, // 101mV; status OK
        { 0x52, { 0x66, 0x00, 0x00, 0x00 } }, // 102mV; status OK
        { 0x53, { 0x67, 0x00, 0x00, 0x00 } }, // 103mV; status OK
        { 0x54, { 0x68, 0x00, 0x00, 0x00 } }, // 104mV; status OK
        { 0x55, { 0x69, 0x00, 0x00, 0x00 } }, // 105mV; status OK
        { 0x56, { 0x6a, 0x00, 0x00, 0x00 } }, // 106mV; status OK
        { 0x57, { 0x6b, 0x00, 0x00, 0x00 } }, // 107mV; status OK
        { 0x58, { 0x6c, 0x00, 0x00, 0x00 } }, // 108mV; status OK
        { 0x59, { 0x6d, 0x00, 0x00, 0x00 } }, // 109mV; status OK
        { 0x5a, { 0x6e, 0x00, 0x00, 0x00 } }, // 110mV; status OK
        { 0x5b, { 0x6f, 0x00, 0x00, 0x00 } }, // 111mV; status OK
        { 0x60, { 0x00, 0x00, 0x00, 0x00 } }, // Normal Blink
        { 0x70, { 0xc8, 0x00, 0x00, 0x00 } }, // 200W; status OK
        { 0x71, { 0xc9, 0x00, 0x00, 0x00 } }, // 201W; status OK
        { 0x73, { 0xca, 0x00, 0x00, 0x00 } }, // 202W; status OK
        { 0x74, { 0xcb, 0x00, 0x00, 0x00 } }, // 203W; status OK
        { 0x75, { 0xcc, 0x00, 0x00, 0x00 } }, // 204W; status OK
        { 0x76, { 0xcd, 0x00, 0x00, 0x00 } }  // 205W; status OK
    };

}; // empty namespace
void KnlUnitTestDevice::scenarioReset()
{
    m_pData->startFlashErrorCode_ = 0;
    m_pData->flashFailIteration_ = 0;
    m_pData->flashFailErrorCode_ = 0;
    m_pData->lastStageForError_ = "";
    m_pData->flashCurrentIteration_ = 0;
    m_pData->flashMaxIterations_ = 10;
    m_SmcPersist = false;

    m_pData->getDeviceVersionInfoErrorCode_ = 0;

    m_pData->apiInjectedError_ = 0;
    m_pData->nextSmcRegisterNSize_ = 4;
    for (int i = 0; i < 2; ++i)
    {
        m_pData->powerThresholds_[i] = 0;
        m_pData->timeWindows_[i] = 0;
    }
    m_pData->postCode_ = -1;

    m_pData->kncWritableRegisters_ = { 0x1a, 0x20, 0x22, 0x2b, 0x4b, 0x60 };

    m_pData->knlRegisterData_.clear();
    for (auto it = rawRegisterDataSet.begin(); it != rawRegisterDataSet.end(); ++it)
    {
        m_pData->knlRegisterData_.insert(*it);
    }
    m_pData->SMBAddress_ = (uint8_t)0x30;
    m_pData->mockSMBATimeout_ = 500;
    m_pData->SMBABusyCounter_ = 0;
    m_pData->currentState_ = FlashStatus::State::eIdle;
    m_Spad = 0;
    m_FSM = 0;
    m_StartDelay = 5;
}

void KnlUnitTestDevice::scenarioStartFlashFails(uint32_t errorCode)
{
    m_pData->startFlashErrorCode_ = errorCode;
}

void KnlUnitTestDevice::scenarioSetState(FlashStatus::State state, uint32_t errorCode)
{
    m_pData->currentState_ = state;
    m_pData->flashFailErrorCode_ = errorCode;
}

void KnlUnitTestDevice::scenarioFlashTimeChange(int maxIterations)
{
    m_pData->flashMaxIterations_ = static_cast<uint8_t>(maxIterations);
}

void KnlUnitTestDevice::scenarioGetDevicePlatformInfoFails(uint32_t errorCode)
{
    m_pData->getDeviceVersionInfoErrorCode_ = errorCode;
}


void KnlUnitTestDevice::scenarioApiErrorInject(uint32_t errorCode)
{
    m_pData->apiInjectedError_ = errorCode;
}

void KnlUnitTestDevice::scenarioSetRegisterNSize(size_t size)
{
    m_pData->nextSmcRegisterNSize_ = size;
}

void KnlUnitTestDevice::scenarioSetPostCode(int postCode)
{
    m_pData->postCode_ = postCode;
}

void KnlUnitTestDevice::scenarioSetSMBARetrainTime(unsigned int timeToRetrain)
{
    m_pData->mockSMBATimeout_ = timeToRetrain;
}

void KnlUnitTestDevice::scenarioSetPersistence(bool persist)
{
    m_SmcPersist = persist;
}
void KnlUnitTestDevice::setFabVersion(const std::string fab)
{
    m_pVersionInfoData->mFabVersion = fab;
}

void KnlUnitTestDevice::setFSM(const unsigned int beginFSM)
{
    m_FSM = beginFSM;
}