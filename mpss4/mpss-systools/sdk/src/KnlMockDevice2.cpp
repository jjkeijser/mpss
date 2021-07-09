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
#include <fstream>
#include <sstream>

#include "MicBootConfigInfo.hpp"
#include "MicCoreInfo.hpp"
#include "MicCoreUsageInfo.hpp"
#include "MicDevice.hpp"
#include "MicDeviceError.hpp"
#include "MicPowerUsageInfo.hpp"
#include "MicThermalInfo.hpp"
#include "MpssStackBase.hpp"
#include "KnlMockDevice.hpp"
#include "KnlMockDevice2.hpp"

#include "micmgmtCommon.hpp"

#include "CoreInfoData_p.hpp"
#include "PowerUsageData_p.hpp"
#include "ThermalInfoData_p.hpp"

namespace
{
    const char* const MIC_PROPS_DIR_ENV     = "MIC_PROPS_DIR";
    const char* const DEFAULT_MIC_PROPS_DIR = "/tmp/mic_props";
    const char* const STATE_FILE             = "state";

    const char* const  ONLINE                = "online";
    const char* const  READY                 = "ready";
    const char* const  BOOTING               = "booting";
    const char* const  RESETTING             = "resetting";
    const char* const  UNKNOWN               = "unknown";

    const size_t       THREAD_COUNT          = 4;
    const uint64_t     TICKS_PER_SECOND      = 200;

    class MicProperties
    {
    public:
        // directory: The directory from which properties will be read. Defaults to empty string.
        MicProperties( int deviceNum, std::string directory = "" ) : m_directory()
        {
            std::pair<bool, std::string> statesDirTuple = micmgmt::getEnvVar(MIC_PROPS_DIR_ENV);
            if (std::get<0>(statesDirTuple))
                m_directory = std::get<1>(statesDirTuple);
            else
                m_directory = DEFAULT_MIC_PROPS_DIR;

            m_directory = m_directory + micmgmt::filePathSeparator() +
                micmgmt::deviceName(deviceNum) + micmgmt::filePathSeparator() +
                directory;
        }

        template<typename T>
        uint32_t readProperty( const std::string& property, T* value )
        {
            if(!value)
                return MICSDKERR_INVALID_ARG;
            std::string path = m_directory + micmgmt::filePathSeparator() + property;
            std::ifstream file(path);
            if(!file.is_open())
                return MICSDKERR_FILE_IO_ERROR;
            file >> *value;
            file.close();
            return MICSDKERR_SUCCESS;
        }

        uint32_t readProperty( const std::string& property, std::string* value )
        {
            if(!value)
                return MICSDKERR_INVALID_ARG;
            const size_t max = 256;
            char buf[max] = {0};
            std::string path = m_directory + micmgmt::filePathSeparator() + property;
            std::ifstream file(path);
            if(!file.is_open())
                return MICSDKERR_FILE_IO_ERROR;
            file.getline(buf, max);
            file.close();
            value->assign(buf);
            return MICSDKERR_SUCCESS;
        }

        template<typename T>
        uint32_t writeProperty( const std::string& property, T value)
        {
            std::string path = m_directory + micmgmt::filePathSeparator() + property;
            std::ofstream file(path);
            if(!file.is_open())
                return MICSDKERR_FILE_IO_ERROR;
            file << value << "\n";
            file.close();
            return MICSDKERR_SUCCESS;
        }

        MicProperties() = delete;

    private:
        std::string m_directory;
    };
}

namespace micmgmt
{

KnlMockDevice2::KnlMockDevice2( int number ) :
    KnlMockDevice( number )
{
    // Nothing to do
}

KnlMockDevice2::~KnlMockDevice2()
{
    // Nothing to do
}

uint32_t  KnlMockDevice2::getDeviceCoreInfo( MicCoreInfo* info ) const
{
    if (!info)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    MicProperties props(deviceNum(), "core");
    uint32_t coreCount = 0;
    uint32_t status = props.readProperty("core_count", &coreCount);
    if ( MicDeviceError::isError(status) )
        return status;

    CoreInfoData coreinfo;
    coreinfo.mCoreCount       = coreCount;
    coreinfo.mCoreThreadCount = THREAD_COUNT;
    coreinfo.mFrequency       = MicFrequency( 1, MicFrequency::eGiga );
    coreinfo.mVoltage         = MicVoltage( 997000, MicVoltage::eMicro );
    *info = MicCoreInfo( coreinfo );

    return MICSDKERR_SUCCESS;

}

uint32_t  KnlMockDevice2::getDeviceCoreUsageInfo( MicCoreUsageInfo* info ) const
{
    if (!info)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    MicProperties props(deviceNum(), "core");
    size_t coreCount = 0;
    uint32_t status = props.readProperty("core_count", &coreCount);
    if ( MicDeviceError::isError(status) )
        return status;

    info->setCoreCount( coreCount );
    info->setCoreThreadCount( THREAD_COUNT );
    info->setTicksPerSecond( TICKS_PER_SECOND );

    uint64_t total = 0, sysTotal = 0, userTotal = 0,
             niceTotal = 0, idleTotal = 0;
    for (size_t core = 0; core <= coreCount; ++core)
    {
        uint64_t sys = 0, user = 0, nice = 0, idle = 0;
        std::string statLine;
        std::stringstream path;
        path << "core_" << core;
        status = props.readProperty(path.str(), &statLine);
        std::stringstream statInfo(statLine);
        statInfo >> sys >> user >> nice >> idle;
        if ( MicDeviceError::isError(status) || statInfo.fail() )
            // if error, set to 0
            sys = user = nice = idle = 0;

        info->setUsageCount( MicCoreUsageInfo::eSystem, core, sys );
        info->setUsageCount( MicCoreUsageInfo::eUser,   core, user );
        info->setUsageCount( MicCoreUsageInfo::eNice,   core, nice );
        info->setUsageCount( MicCoreUsageInfo::eIdle,   core, idle );
        info->setUsageCount( MicCoreUsageInfo::eTotal,  core, sys + user + nice + idle);
        total += sys + user + nice + idle;
        sysTotal += sys;
        userTotal += user;
        niceTotal += nice;
        idleTotal += idle;
    }

    info->setTickCount(total);

    info->setCounterTotal( MicCoreUsageInfo::eSystem, sysTotal );
    info->setCounterTotal( MicCoreUsageInfo::eUser,   userTotal );
    info->setCounterTotal( MicCoreUsageInfo::eNice,   niceTotal );
    info->setCounterTotal( MicCoreUsageInfo::eIdle,   idleTotal );
    info->setCounterTotal( MicCoreUsageInfo::eTotal,  total );
    info->setValid( true );
    return MICSDKERR_SUCCESS;
}

uint32_t  KnlMockDevice2::getDeviceState( MicDevice::DeviceState* state ) const
{
    if (!state)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    std::string stateStr;
    MicProperties props(deviceNum());
    uint32_t status = props.readProperty("state", &stateStr);
    if ( MicDeviceError::isError(status) )
        return status;

    if (stateStr == READY)
        *state = MicDevice::DeviceState::eReady;
    else if (stateStr == BOOTING)
        *state = MicDevice::DeviceState::eReboot;
    else if (stateStr == RESETTING)
        *state = MicDevice::DeviceState::eReset;
    else if (stateStr == ONLINE)
        *state = MicDevice::DeviceState::eOnline;
    else if (stateStr == UNKNOWN)
        *state = MicDevice::DeviceState::eError;
    else
    {
        *state = MicDevice::DeviceState::eError;
        return  MicDeviceError::errorCode(MICSDKERR_INTERNAL_ERROR);
    }

    return MicDeviceError::errorCode(MICSDKERR_SUCCESS);

}

uint32_t  KnlMockDevice2::getDevicePowerUsageInfo( MicPowerUsageInfo* info ) const
{
    if (!info)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    if (!isDeviceOpen())
    {
        *info = MicPowerUsageInfo();
        return MicDeviceError::errorCode( MICSDKERR_SUCCESS );
    }

    MicProperties props(deviceNum(), "power");
    uint32_t currentPower = 0;
    uint32_t status = props.readProperty("current", &currentPower);
    if ( MicDeviceError::isError(status) )
        return status;

    // Prepare fake power info data
    PowerUsageData data;
    data.mThrottle     = MicPower( "Threshold", 190 );
    data.mThrottleInfo = ThrottleInfo( 20, false, 3, 96 );
    data.mSensors.push_back( MicPower( powerSensorAsText( ePciePower ), 153 ) );
    data.mSensors.push_back( MicPower( powerSensorAsText( e2x3Power ),   67 ) );
    data.mSensors.push_back( MicPower( powerSensorAsText( e2x4Power ),   37 ) );
    data.mSensors.push_back( MicPower( powerSensorAsText( eCurrentPower ), currentPower ) );
    data.mValid = true;

    *info = MicPowerUsageInfo( data );

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}

uint32_t  KnlMockDevice2::getDeviceThermalInfo( MicThermalInfo* info ) const
{
    if (!info)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    if (!isDeviceOpen())
    {
        *info = MicThermalInfo();
        return MicDeviceError::errorCode( MICSDKERR_SUCCESS );
    }

    MicProperties props(deviceNum(), "thermal");
    uint32_t cpuTemp = 0;
    uint32_t status = props.readProperty("cpu", &cpuTemp);
    if ( MicDeviceError::isError(status) )
        return status;

    ThermalInfoData tinfo;

    // Prepare initial fake thermal info data
    tinfo.mFanRpm          = 2700;
    tinfo.mFanPwm          = 50;
    tinfo.mFanAdder        = 50;
    tinfo.mControl         = 85;
    tinfo.mCritical        = 95;
    tinfo.mThrottleInfo    = ThrottleInfo( 8, true, 12, 486 );
    tinfo.mSensors.push_back( MicTemperature( tempSensorAsText( eCpuTemp ),    cpuTemp ) );
    tinfo.mSensors.push_back( MicTemperature( tempSensorAsText( eExhaustTemp ), 52 ) );
    /// \todo More sensors
    tinfo.mValid = true;

    *info = MicThermalInfo(tinfo);

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}

uint32_t  KnlMockDevice2::deviceBoot( const MicBootConfigInfo& info )
{
    if (m_DeviceState != MicDevice::DeviceState::eReady)
        return MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_READY );

    // Since we don't boot, we ignore the configuration
    (void) info;

    // Our real device calls would fail if not privileged, so...
    if (!m_IsAdmin)
        return MicDeviceError::errorCode( MICSDKERR_NO_ACCESS );

    MicProperties props(deviceNum());
    return props.writeProperty("state", "online");
}

uint32_t  KnlMockDevice2::deviceReset( bool force )
{
    // Our real device calls would fail if not privileged, so...
    if (!m_IsAdmin)
        return MicDeviceError::errorCode( MICSDKERR_NO_ACCESS );

    // Our mock device does not hang
    (void) force;

    MicProperties props(deviceNum());
    return props.writeProperty("state", "ready");
}


} // namespace micmgmt
