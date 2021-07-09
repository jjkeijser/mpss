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

#ifdef LIBDEBUG
#include <stdio.h>
#endif
#include <algorithm>
#include <new>
#include <string>
#include <sstream>
#include <stdlib.h>
#include "libsystools.h"
#include "libsystools_p.hpp"
#include "MicDeviceFactory.hpp"
#include "MicDeviceManager.hpp"
#include "MicDeviceError.hpp"
#include "MicDeviceInfo.hpp"
#include "MicPlatformInfo.hpp"
#include "MicPciConfigInfo.hpp"
#include "PciAddress.hpp"
#include "MicSpeed.hpp"
#include "MicVoltage.hpp"
#include "MicMemory.hpp"
#include "MicFrequency.hpp"
#include "MicMemoryInfo.hpp"
#include "ThrottleInfo.hpp"
#include "MicThermalInfo.hpp"
#include "MicPowerUsageInfo.hpp"
#include "MicPowerState.hpp"
#include "MicProcessorInfo.hpp"
#include "MicCoreInfo.hpp"
#include "MicVersionInfo.hpp"
#include "MicPowerThresholdInfo.hpp"
#include "MicMemoryUsageInfo.hpp"
#include "MicTemperature.hpp"
#include "MicPower.hpp"
#include "MicCoreUsageInfo.hpp"
#include "MicCoreInfo.hpp"

using namespace micmgmt;
namespace
{
    const int ENABLED  =  1;
    const int DISABLED =  0;

    bool isExpValid(int exp)
    {
        std::vector<int> valid = {-9, -6, -3, 0, 3, 6, 9};
        auto it = std::find(valid.begin(), valid.end(), exp);
        if (it == valid.end())
            return false;
        return true;
    }

    const string X100_TYPE_STR = "x100";
    const string X200_TYPE_STR = "x200";

}
miclib *initLib = miclib::getInstance();

/*
 * UNIT_TESTS Functions.
 */
#ifdef UNIT_TESTS
void reset_miclib_instance()
{
    initLib = miclib::resetInstance();
}
#endif

/**
 * @if HTML_MAINPAGE
 * @mainpage libsystools - The C library.
 * @endif
 * @if MANPAGE_INTRODUCTION
 * @page libsystools The C library.
 * @endif
 * @section BRIEF
 * \b libsystools — C-library to access and update Intel® Xeon Phi™ coprocessor parameters.\n
 *
 * @section SYNOPSIS
 * \b \#include\<libsystools.h\>
 * \n\n
 * The libsystools library provides a C-library interface to Intel® Xeon Phi™ coprocessors
 * (based on Intel® Many Integrated Core architecture - MIC) to applications running on the host system.
 * These are wrappers built on top of the MIC host driver interfaces that are available
 * in the form of sysfs entries(in Linux), WMI entries(in Microsoft Windows) and scif calls. Various
 * functions are available to query the state of each coprocessor, or list or modify some of its
 * parameters.\n
 * \n
 * Library functions are categorized into the following groups:\n
 * \n
 * \ref Group1  "01. Identify Available Coprocessors."\n
 * \ref Group2  "02. Error Reporting."\n
 * \ref Group3  "03. Device Access."\n
 * \ref Group4  "04. Query Coprocessor State."\n
 * \ref Group5  "05. General Device Information."\n
 * \ref Group6  "06. PCI Configuration Information."\n
 * \ref Group7  "07. Memory Information."\n
 * \ref Group8  "08. Processor Information."\n
 * \ref Group9  "09. Coprocessor Core Information."\n
 * \ref Group10 "10. Version Information."\n
 * \ref Group11 "11. LED Mode Information."\n
 * \ref Group12 "12. Turbo State Information."\n
 * \ref Group13 "13. SMC Configuration Information."\n
 * \ref Group14 "14. Throttle State Information."\n
 * \ref Group15 "15. Coprocessor OS Power Management Configuration."\n
 * \ref Group16 "16. Thermal Information."\n
 * \ref Group17 "17. Power Utilization Information."\n
 * \ref Group18 "18. Memory Utilization Information."\n
 * \ref Group19 "19. Power Limits."\n
 * \ref Group20 "20. Core Utilization Information."\n
 * \ref Group21 "21. Free Memory Utilities."\n
 *
 * @section RET RETURN VALUE
 * All \b libsystools functions return MICSDKERR_SUCCESS after successful completion, upon failure they
 * return one of the error codes defined in \b micsdkerrno.h file.
 *
 * @if MANPAGE_INTRODUCTION
 * @section NOTES
 * Help pages of a particular group can be viewed by typing man \b \<groupname\>.\n
 * e.g.\n
 *  man \b Core_Utilization_Information,
 *  man \b Memory_Information, etc.
 * \n\n
 * For individual function help, type man \b \<functionname\>.\n
 * e.g.\n
 *  man \b mic_get_memory_info,
 *  man \b mic_open_device, etc.
 * @endif
 *
 */

/**
 * @fn     uint32_t mic_get_ndevices(size_t *ndevices)
 *
 * @brief  Retrieves the number of coprocessors present in the system.
 *
 * @param  ndevices A pointer to the variable to receive the number of devices.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 *
 */

/*! \addtogroup Identify_Available_Coprocessors Identify Available Coprocessors.
 * NONE.
 * @{
 * @anchor Group1
 *
 */
uint32_t mic_get_ndevices(size_t *ndevices)
{
    if (NULL == ndevices)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    *ndevices = 0;
    uint32_t status = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    // Create and Init DeviceManager Object.
    MicDeviceManager deviceManager;

    status = deviceManager.initialize();

    if (false == MicDeviceError::isError( status ))
        *ndevices = deviceManager.deviceCount();

    return status;
}
/*! @} */

/*! \addtogroup Error_Reporting Error Reporting.
 * NONE.
 * @{
 * @anchor Group2
 *
 */

/**
 * @fn     const char *mic_get_error_string(uint32_t mic_error_num)
 *
 * @brief  Retrieves the error character string defining the specified
 *         error number.
 *
 * @param  mic_error_num The error code corresponding to the error message.
 *
 * This function is used to get a verbose message of the error code
 * returned from the most recent API call.
 *
 * @return An error string defining specified \b mic_error_num number.
 *
 */
const char *mic_get_error_string(uint32_t mic_error_num)
{
   return MicDeviceError::errorText( mic_error_num );
}

/*! @} */

/*! \addtogroup Device_Access Device Access.
 * NONE.
 * @{
 * @anchor Group3
 *
 */

/**
 * @fn     MIC *mic_open_device(int device_num, uint32_t *errcode)
 *
 * @brief  Opens a device using device number.
 *
 * @param  device_num  The number of the device to be opened.
 * @param  errcode     A pointer to the variable to receive the error code.
 *
 * On success, this function returns a handle to the open instance of the specified device.
 * The returned handle can be used to access the coprocessor board until it is closed by a
 * subsequent mic_close_device() function call.
 *
 * @return If the function succeeds, the return value is an open handle to the specified coprocessor.
 *         If the function fails, the return value is a NULL pointer and an errcode is set to indicate
 *         the error.
 *
 * On success, MICSDKERR_SUCCESS is set in the errcode argument.\n
 * On failure, one of the following error codes is set in the errcode argument:
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_NO_SUCH_DEVICE
 * - MICSDKERR_DEVICE_ALREADY_OPEN
 * - MICSDKERR_INTERNAL_ERROR
 * - MICSDKERR_DEVICE_IO_ERROR
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_VERSION_MISMATCH
 *
 */
MIC *mic_open_device(int device_num, uint32_t *errcode)
{
    MIC *device = NULL;

    if (NULL == errcode)
        return device;

    // Error if the library is not initialized
    if ((false == initLib->isInitialized())) {
        *errcode = initLib->getLastErrCode();
        return device;
    }
    // Check if a device really exist.
    MicDeviceManager deviceManager;

    uint32_t status = deviceManager.initialize();

    if (true == MicDeviceError::isError( status )) {
        *errcode = status;
        return device;
    }

    if (false == deviceManager.isDeviceAvailable( device_num )) {
        *errcode =  MicDeviceError::errorCode( MICSDKERR_NO_SUCH_DEVICE );
        return device;
    }

    // Now check if the device is already open.
    // Error if it is already open.
    *errcode = initLib->isDeviceOpen(device_num);

    if (true == MicDeviceError::isError( *errcode ))
        return device;

    // Open device, update the container and return handle.
    MicDeviceFactory factory( &deviceManager );
    MicDevice* micDev = factory.createDevice(device_num);

    if (NULL == micDev) {
        *errcode =  MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );
        return device;
    }

    status = micDev->open();

    if (true == MicDeviceError::isError( status )) {
        *errcode = status;
        delete micDev;
        return device;
    }

    *errcode = initLib->updateDevContainer(device_num, micDev, &device);
    return device;
}

/**
 * @fn     MIC *mic_open_device_by_name(const char *device_name, uint32_t *errcode)
 *
 * @brief  Opens a device using name.
 *
 * @param  device_name  The string name of the device to be opened.
 * @param  errcode      A pointer to the variable to receive the error code.
 *
 * On success, this function returns a handle to the open instance of the specified device.
 * The device name to be supplied is of the form mic\<n\>, where 'n' denotes a valid device number.
 * The returned handle can be used to access the coprocessor board until it is closed by a subsequent
 * \b mic_close_device() function call.
 *
 * @return If the function succeeds, the return value is an open handle to the specified coprocessor.
 *         If the function fails, the return value is a NULL pointer and an errcode is set to indicate
 *         the error.
 *
 * On success, MICSDKERR_SUCCESS is set in the errcode agument.\n
 * On failure, one of the following error codes is set in the errcode argument:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_NO_SUCH_DEVICE
 * - MICSDKERR_DEVICE_ALREADY_OPEN
 * - MICSDKERR_INTERNAL_ERROR
 * - MICSDKERR_DEVICE_IO_ERROR
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_VERSION_MISMATCH
 *
 */
MIC *mic_open_device_by_name(const char *device_name, uint32_t *errcode)
{
    MIC *device = NULL;

    if (NULL == errcode)
        return device;

    if (NULL == device_name) {
        *errcode = MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );
        return device;
    }

    // Error if the library is not initialized
    if (false == initLib->isInitialized()) {
        *errcode = initLib->getLastErrCode();
        return device;
    }

    // Check if a device really exist.
    MicDeviceManager deviceManager;

    uint32_t status = deviceManager.initialize();

    if (true == MicDeviceError::isError( status )) {
        *errcode = status;
        return device;
    }

    if (false == deviceManager.isDeviceAvailable( device_name )) {
        *errcode =  MicDeviceError::errorCode( MICSDKERR_NO_SUCH_DEVICE );
        return device;
    }

    int device_num = -1;
    string  name(device_name);
    string  numstr = name.substr(deviceManager.deviceBaseName().length());
    stringstream  strm( numstr );
    strm >> device_num;

    // Now check if the device is already open.
    // Error if it is already open.
    *errcode = initLib->isDeviceOpen(device_num);

    if (true == MicDeviceError::isError( *errcode ))
        return device;

    // Open device and update the container.
    MicDeviceFactory factory( &deviceManager );
    MicDevice* micDev = factory.createDevice(device_num);

    if (NULL == micDev) {
        *errcode =  MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );
        return device;
    }

    status = micDev->open();

    if (true == MicDeviceError::isError( status )) {
        *errcode = status;
        delete micDev;
        return device;
    }

    *errcode = initLib->updateDevContainer(device_num, micDev, &device);

    return device;
}

/**
 * @fn      uint32_t mic_close_device(MIC *device)
 *
 * @brief   Closes an open device.
 *
 * @param   device  A valid handle to an open device.
 *
 * This function closes and releases all resources associated with an open coprocessor handle.
 * The result is undefined if the function is called with a handle that was previously
 * closed, corrupted, or not returned by a previous open call.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 *
 */
uint32_t mic_close_device(MIC *devHandle)
{
    if (NULL == devHandle)
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    if (false == initLib->isInitialized()) {
        errCode = initLib->getLastErrCode();
        return errCode;
    }

    // Get device object and invalidate the list.
    MicDevice *device = initLib->getOpenDevice(devHandle, true);

    if (NULL != device) {
        device->close();
        // delete dev object because pDevContainer is reset for this device.
        delete device;
    }
    else
       errCode = MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_OPEN );

    return errCode;
}

/*! @} */

// Device mode switch/start/stop

/*! \addtogroup Query_Coprocessor_State Query Coprocessor State.
 * Query or Modify the state of the coprocessor.
 * @{
 * @anchor Group4
 *
 */
/**
 * @fn     uint32_t mic_enter_maint_mode(MIC *device)
 *
 * @brief  Enable device's maintenance mode.
 *
 * @param  device  A valid handle to an open device.
 *
 * This function is supported only in Intel® Xeon Phi™ x100 product family.
 * For Intel® Xeon Phi™ x200 product family the function returns a 'Operation not supported' error.
 *
 * The Intel® Xeon Phi™ coprocessor x100 product family provides a
 * maintenance operation mode that allows completing certain privileged tasks like upgrading
 * the contents of the on-board flash memory and System Management Controller, and
 * also implementing RAS features, and more. This function provides an interface to
 * switch the operation of the specified coprocessor to the maintenance mode. Note that
 * this mode may be entered only from the \b ready state, which is equivalent to the
 * coprocessor being reset and, therefore, not running any operating system and
 * applications.
 *
 * Since the maintenance mode is a trusted mode, in which many otherwise unavailable
 * registers are left in unlocked state, a well behaved application should perform
 * its maintenance task and promptly return to the ready state with a call to the
 * \b mic_leave_maint_mode() function.
 *
 * Switching modes is an asynchronous operation. This function returns
 * immediately after requesting a switch to the maintenance mode. On Linux, it is
 * done by writing to the \b /sys/class/mic/mic\<n\>/state file. On Microsoft Windows,
 * it is done by writing to the \b state WMI entry. It only returns an error if
 * this write fails. To ensure that the switch is successfully completed the state
 * must be polled with the mic_in_maint_mode() function.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_INTERNAL_ERROR
 * - MICSDKERR_TIMEOUT
 * - MICSDKERR_NOT_SUPPORTED
 *
 */
uint32_t mic_enter_maint_mode(MIC *devHandle)
{
    if (NULL == devHandle)
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);
    if (NULL == device)
        return errCode;

    // For Intel® Xeon Phi™ coprocessor x100 only.
    // Intel® Xeon Phi™ coprocessor x200 returns not supported error code.
    errCode = device->setMaintenanceModeEnabled(true);
    return errCode;
}

/**
 * @fn      uint32_t mic_leave_maint_mode(MIC *device)
 *
 * @brief   Cause the device to leave the maintenance mode.
 *
 * @param   device  A valid handle to an open device.
 *
 * This function is supported only in Intel® Xeon Phi™ x100 product family.
 * For Intel® Xeon Phi™ x200 product family the function returns a 'Operation not supported' error.
 *
 * This function may be used to begin transition back to the \b ready state from
 * the maintenance mode for the specified coprocessor. A well behaved application must
 * make sure that it is called after a successful change to the maintenance mode. This
 * is also an asynchronous operation and the \b mic_in_ready_state() function must
 * be polled to check the state of the board.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_INTERNAL_ERROR
 * - MICSDKERR_TIMEOUT
 * - MICSDKERR_NOT_SUPPORTED
 *
 */
uint32_t mic_leave_maint_mode(MIC *devHandle)
{
    if (NULL == devHandle)
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);
    if (NULL == device)
        return errCode;

    // For Intel® Xeon Phi™ coprocessor x100 only.
    // Intel® Xeon Phi™ coprocessor x200 returns not supported error code.
    errCode = device->setMaintenanceModeEnabled(false);
    return errCode;
}

/**
 * @fn     uint32_t mic_in_maint_mode(MIC *device, int *mode)
 *
 * @brief  Determine the current maintenance mode of the coprocessor.
 *
 * @param  device  A valid handle to an open device.
 * @param  mode    A pointer to the variable to receive the mode value.
 *
 * This function is supported only in Intel® Xeon Phi™ x100 product family.
 * For Intel® Xeon Phi™ x200 product family the function returns a 'Operation not supported' error.
 *
 * This function returns a non-zero value in the \b int \b *mode argument if
 * the specified coprocessor is in the maintenance mode, otherwise it is set to 0.
 * Applications must poll the state after requesting a change to the maintenance
 * mode using the \b mic_enter_maint_mode() function call. On Linux, the coprocessor
 * state is polled by reading the \b /sys/class/mic/mic\<n\>/state and
 * \b /sys/class/mic/mic\<n\>/mode files. On Microsoft Windows, the coprocessor state
 * is polled by reading the WMI entries \b state and \b mode respectively.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_INTERNAL_ERROR
 * - MICSDKERR_DEVICE_IO_ERROR
 * - MICSDKERR_NOT_SUPPORTED
 *
 */
uint32_t mic_in_maint_mode(MIC *devHandle, int *mode)
{
    if ( (NULL == devHandle) || (NULL == mode))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);
    if (NULL == device)
        return errCode;

    // For Intel® Xeon Phi™ coprocessor x100 only.
    // Intel® Xeon Phi™ coprocessor x200 returns not supported error code.
    bool enabled = false;
    errCode = device->getMaintenanceMode(&enabled);

    *mode = enabled ? ENABLED : DISABLED;
    return errCode;
}

/**
 * @fn     uint32_t mic_in_ready_state(MIC *device, int *ready_state)
 *
 * @brief  Determine if the coprocessor is in the ready state.
 *
 * @param  device       A valid handle to an open device.
 * @param  ready_state  A pointer to the variable to receive the state value.
 *
 * A call to \b mic_leave_maint_mode() may be followed by \b mic_in_ready_state().
 * The \b int \b *ready_state argument is set to a non-zero value if the transition to
 * the ready state is completed, otherwise it is set to 0. The board state is returned
 * from the \b /sys/class/mic/mic\<n\>/state file on a Linux machine.
 * On a Microsoft Windows machine, the board state is returned from the \b state
 * WMI entry.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 *
 */
uint32_t mic_in_ready_state(MIC *devHandle, int *ready_state)
{
    if ((NULL == devHandle) || (NULL == ready_state))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);
    if (NULL == device)
        return errCode;

    *ready_state = device->isReady() ? ENABLED : DISABLED;
    return errCode;
}

/**
 * @fn     uin32_t mic_get_state(MIC *devHandle, int *state)
 *
 * @brief  Get the current state of the coprocessor
 *
 * @param  device    A valid handle to an open device.
 * @param  state     A pointer to the variable to receive the state value
 *
 * Obtains the internal state of the coprocessor at the moment the function is called.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 *
 */

uint32_t mic_get_state(MIC *devHandle, int *state)
{
    if ((NULL == devHandle) || (NULL == state))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);
    if (NULL == device)
        return errCode;

    micmgmt::MicDevice::DeviceState internal_state = device->deviceState();
    switch(internal_state)
    {
        case micmgmt::MicDevice::eOffline:
            *state = e_offline;
            break;
        case micmgmt::MicDevice::eOnline:
            *state = e_online;
            break;
        case micmgmt::MicDevice::eReady:
            *state = e_ready;
            break;
        case micmgmt::MicDevice::eReset:
            *state = e_reset;
            break;
        case micmgmt::MicDevice::eReboot:
            *state = e_reboot;
            break;
        case micmgmt::MicDevice::eLost:
            *state = e_lost;
            break;
        case micmgmt::MicDevice::eBootingFW:
            *state = e_booting_fw;
            break;
        case micmgmt::MicDevice::eOnlineFW:
            *state = e_online_fw;
            break;
        case micmgmt::MicDevice::eError:
            *state = e_error;
            break;
        default:
            *state = e_unknown;
            break;
    }
    return errCode;
}

/*! @} */
/* General device information */

/*! \addtogroup General_Device_Information General Device Information.
 * NONE.
 * @{
 * @anchor Group5
 */

/**
 * @fn     uint32_t mic_get_post_code(MIC *device, char *postcode, size_t *size)
 *
 * @brief  Retrieves the current POST code of the specified coprocessor.
 *
 * @param  device      A valid handle to an open device.
 * @param  postcode    A pointer to the buffer to receive the post code.
 * @param  size        A pointer to receive the size of the data copied/required.
 *
 * This functions returns the current POST code of the specified coprocessor.
 * The \b size_t \b *size argument is an input/output argument. Its input
 * value is used to determine the maximum number of characters (including trailing
 * nul) that may be transferred to the buffer. The appropriate \b size value
 * can be queried by setting its value to 0. The behavior is undetermined
 * if the input size is greater than the actual buffer size, or if the buffer
 * references a NULL or invalid memory location. On a Linux machine, the current
 * POST code is read from \b /sys/class/mic/mic\<n\>/post_code (for Intel® Xeon Phi™ coprocessor x100) and
 * \b /sys/class/mic/mic\<n\>/device/spad/post_code (for Intel® Xeon Phi™ coprocessor x200). On Microsoft Windows,
 * the current POST code is read from \b post_code WMI entry.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 *
 */
uint32_t mic_get_post_code(MIC *devHandle, char *postcode, size_t *size)
{
    if ((NULL == devHandle) || (NULL == postcode) || (NULL == size))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);
    if (NULL == device)
        return errCode;

    string postStr("");
    errCode = device->getPostCode(&postStr);

    if (0 < *size) {
        strncpy(postcode, postStr.c_str(), *size);
        postcode[*size - 1] = '\0'; // safe null termination
    }

    if (*size < postStr.length() + 1)
        *size = postStr.length() + 1;

    return errCode;
}

/**
 * @fn     uint32_t mic_get_device_type(MIC *device, char *device_type, size_t *size)
 *
 * @brief  Retrieves the device type of the specified coprocessor.
 *
 * @param  device       A valid handle to an open device.
 * @param  device_type  A pointer to the buffer to receive the device type.
 * @param  size         A pointer to receive the size of the data copied/required.
 *
 * This function returns the type of the Intel® Xeon Phi™ coprocessor to the
 * \b device_type buffer. The \b size_t \b *size argument is an input/output argument.
 * Its input value is used to determine the maximum number of characters (including
 * trailing nul) that may be transferred to the buffer. The appropriate \b size value
 * can be queried by setting its value to 0. The behavior is undetermined if the input
 * size is greater than the actual buffer size, or if the buffer references a NULL or
 * invalid memory location.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 *
 */
uint32_t mic_get_device_type(MIC *devHandle, char *device_type, size_t *size)
{
    if ((NULL == devHandle) || (NULL == device_type) || (NULL == size))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);
    if (NULL == device)
        return errCode;

    string typeStr = device->deviceType();

    if (0 < *size) {
        strncpy(device_type, typeStr.c_str(), *size);
        device_type[*size - 1] = '\0';
    }

    if (*size < typeStr.length() + 1)
        *size = typeStr.length() + 1;

    return errCode;
}

/**
 * @fn     uint32_t mic_get_device_name(MIC *device, char *device_name, size_t *size)
 *
 * @brief  Retrieves the name of the coprocessor.
 *
 * @param  device       A valid handle to an open device.
 * @param  device_name  A pointer to the buffer to receive the device name.
 * @param  size         A pointer to receive the size of the data copied/required.
 *
 * This function returns the name of the Intel® Xeon Phi™ coprocessor to the
 * \b device_name buffer. On Linux, this is \b mic\<n\> in \b /sys/class/mic/mic\<n\>,
 * which is referenced by the input \b device argument. On Microsoft Windows, it is the
 * \b mic\<n\> in \b \<board_id\> WMI entry. The \b size_t \b *size argument is an input/output
 * argument. Its input value is used to determine the maximum number of characters (including
 * trailing nul) that may be transferred to the buffer. The appropriate \b size value
 * can be queried by setting its value to 0. The behavior is undetermined if the input
 * size is greater than the actual buffer size, or if the buffer references a NULL or
 * invalid memory location.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 *
 */
uint32_t mic_get_device_name(MIC *devHandle, char *device_name, size_t *size)
{
    if ((NULL == devHandle) || (NULL == device_name) || (NULL == size))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);

    if (NULL == device)
        return errCode;

    string nameStr = device->deviceName();

    if (0 < *size) {
        strncpy(device_name, nameStr.c_str(), *size);
        device_name[*size - 1] = '\0';
    }

    if (*size < nameStr.length() + 1)
        *size = nameStr.length() + 1;

    return errCode;
}

/**
 * @fn     uint32_t mic_get_silicon_sku(MIC *device, char *value, size_t *size)
 *
 * @brief  Retrieves the SKU string of the coprocessor.
 *
 * @param  device  A valid handle to an open device.
 * @param  value   A pointer to the buffer to receive the device SKU.
 * @param  size    A pointer to receive the size of the data copied/required.
 *
 * This function returns the SKU of the Intel® Xeon Phi™ coprocessor to the
 * \b value buffer. The \b size_t \b *size argument is an input/output argument.
 * Its input value is used to determine the maximum number of characters (including
 * trailing nul) that may be transferred to the buffer. The appropriate \b size value
 * can be queried by setting its value to 0. The behavior is undetermined if the input
 * size is greater than the actual buffer size, or if the buffer references a NULL or
 * invalid memory location.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 *
 */
uint32_t mic_get_silicon_sku(MIC *devHandle, char *value, size_t *size)
{
    if ((NULL == devHandle) || (NULL == value) || (NULL == size))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);

    if (NULL == device)
        return errCode;

    MicProcessorInfo pinfo;
    errCode = device->getProcessorInfo(&pinfo);
    if(errCode != MICSDKERR_SUCCESS)
        return errCode;

    string skuStr = pinfo.sku().value();

    if (0 < *size) {
        strncpy(value, skuStr.c_str(), *size);
        value[*size - 1] = '\0';
    }

    if (*size < skuStr.length() + 1)
        *size = skuStr.length() + 1;

    return errCode;
}

/**
 * @fn     uint32_t mic_get_serial_number(MIC *device, char *value, size_t *size)
 *
 * @brief  Retrieves the serial number string of the coprocessor.
 *
 * @param  device  A valid handle to an open device.
 * @param  value   A pointer to the buffer to receive the device serial number.
 * @param  size    A pointer to receive the size of the data copied/required.
 *
 * This function returns the serial number of the Intel® Xeon Phi™ coprocessor to the
 * \b value buffer. The \b size_t \b *size argument is an input/output argument.
 * Its input value is used to determine the maximum number of characters (including
 * trailing nul) that may be transferred to the buffer. The appropriate \b size value
 * can be queried by setting its value to 0. The behavior is undetermined if the input
 * size is greater than the actual buffer size, or if the buffer references a NULL or
 * invalid memory location.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_INTERNAL_ERROR
 * - MICSDKERR_DEVICE_NOT_ONLINE
 * - MICSDKERR_DEVICE_IO_ERROR
 *
 */
uint32_t mic_get_serial_number(MIC *devHandle, char *value, size_t *size)
{
    if ((NULL == devHandle) || (NULL == value) || (NULL == size))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);

    if (NULL == device)
        return errCode;

    if (device->isReady())
        return MICSDKERR_DEVICE_NOT_ONLINE;

    MicPlatformInfo platformInfo;
    errCode = device->getPlatformInfo(&platformInfo);

    if (true == MicDeviceError::isError(errCode))
        return errCode;

    string serialStr = platformInfo.serialNo().value();
    if (0 < *size) {
        strncpy(value, serialStr.c_str(), *size);
        value[*size - 1] = '\0';
    }

    if (*size < serialStr.length() + 1)
        *size = serialStr.length() + 1;

    return errCode;
}

/**
 * @fn     uint32_t mic_get_uuid(MIC *device, char *uuid, size_t *size)
 *
 * @brief  Retrieves the UUID string of the coprocessor.
 *
 * @param  device  A valid handle to an open device.
 * @param  uuid    A pointer to the buffer to receive the device UUID.
 * @param  size    A pointer to receive the size of the data copied/required.
 *
 * This function returns the UUID of the Intel® Xeon Phi™ coprocessor to the
 * \b uuid buffer. The \b size_t \b *size argument is an input/output argument.
 * Its input value is used to determine the maximum number of characters (including
 * trailing nul) that may be transferred to the buffer. The appropriate \b size value
 * can be queried by setting its value to 0. The behavior is undetermined if the input
 * size is greater than the actual buffer size, or if the buffer references a NULL or
 * invalid memory location.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_INTERNAL_ERROR
 * - MICSDKERR_DEVICE_NOT_ONLINE
 * - MICSDKERR_DEVICE_IO_ERROR
 *
 */
uint32_t mic_get_uuid(MIC *devHandle, char *uuid, size_t *size)
{
    if ((NULL == devHandle) || (NULL == uuid) || (NULL == size))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);
    if (NULL == device)
        return errCode;

    MicPlatformInfo platformInfo;
    errCode = device->getPlatformInfo(&platformInfo);

    if (true == MicDeviceError::isError(errCode))
        return errCode;

    string uuidStr = platformInfo.uuid().value();
    if (0 < *size) {
        strncpy(uuid, uuidStr.c_str(), *size);
        uuid[*size - 1] = '\0';
    }

    if (*size < uuidStr.length() + 1)
        *size = uuidStr.length() + 1;

    return errCode;
}

/**
 * @fn     uint32_t mic_is_device_avail(MIC *device, int *device_avail)
 *
 * @brief  Determine if the coprocessor is online.
 *
 * @param  device       A valid handle to an open device.
 * @param  device_avail A pointer to the variable to receive the online status.
 *
 * This function sets the variable pointed to by \b device_avail to zero if a
 * scif connection with the coprocessor cannot be established, otherwise a non-zero
 * value is set to indicate the scif connection is available.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 *
 */
uint32_t mic_is_device_avail(MIC *devHandle, int *device_avail)
{
    if ((NULL == devHandle) || (NULL == device_avail))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);
    if (NULL == device)
        return errCode;

    *device_avail = device->isOnline() ? ENABLED : DISABLED;
    return errCode;
}

/*! @} */

 /* Device Pci Configuration Information */

/*! \addtogroup PCI_Configuration_Information PCI Configuration Information.
 * NONE.
 * @{
 * @anchor Group6
 */

/**
 * @fn     uint32_t mic_get_pci_config(MIC *device, MIC_PCI_CONFIG *config)
 *
 * @brief  Retrieves the PCI Configuration information of the specified coprocessor.
 *
 * @param  device  A valid handle to an open device.
 * @param  config  A pointer to the MIC_PCI_CONFIG structure that receives the PCI configuration.
 *
 * If the function succeeds, the PCI configuration information data is contained in the buffer
 * pointed to by \b MIC_PCI_CONFIG \b *config parameter. The configuration information is available regardless of
 * the device state. \b MIC_PCI_CONFIG structure contains the following members:\n\n
 * \a domain :\n
 *    Represents a 16-bit PCI domain id.
 *
 * \a bus_no :\n
 *    Represents a PCI bus number. Valid values for the bus number is 0 to 255.
 *
 * \a link_speed :\n
 *    Represents a PCI Link speed in MT/s. The function only returns valid data
 *    if the caller has root or administrator privileges.
 *
 * \a link_width :\n
 *    Represents a PCI Link width. The function only returns valid data
 *    if the caller has root or administrator privileges.
 *
 * \a payload_size :\n
 *    Represents the maximum PCI data payload size in bytes. The function only returns
 *    valid data if the caller has root or administrator privileges.
 *
 * \a read_req_size :\n
 *    Represents the maximum PCI read request size in bytes. The function only returns
 *    valid data if the caller has root or administrator privileges.
 *
 * \a access_violation :\n
 *    Represents the access state, non-zero if a user does not have full access to all PCI configuration information.
 *
 * \a class_code :\n
 *    Represents the PCI Class code. It identifies the device type.
 *
 * \a device_no :\n
 *    Represents a PCI slot number (0 to 31) in memory.
 *
 * \a vendor_id :\n
 *    Represents a unique number describing the originator of the PCI device.This value is 0x8086 for Intel® PCI devices.
 *
 * \a device_id :\n
 *    Represents a unique number describing the device itself.
 *
 * \a subsystem_id :\n
 *    Represents a 16-bit PCI Subsystem ID of the coprocessor.
 *
 * \a revision_id :\n
 *    Represents an 8-bit PCI Revision ID of the coprocessor.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_INTERNAL_ERROR
 *
 */
uint32_t mic_get_pci_config(MIC *devHandle, MIC_PCI_CONFIG *config)
{
    if ((NULL == devHandle) || (NULL == config))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);
    if (NULL == device)
        return errCode;

    MicPciConfigInfo pciInfo;
    errCode = device->getPciConfigInfo(&pciInfo);

    if (false == MicDeviceError::isError(errCode)) {
        // Fill up the config data.
        config->domain           = pciInfo.address().domain();
        config->bus_no           = pciInfo.address().bus();
        config->device_no        = (uint16_t)pciInfo.address().device();
        config->vendor_id        = pciInfo.vendorId();
        config->device_id        = pciInfo.deviceId();
        config->revision_id      = pciInfo.revisionId();
        config->subsystem_id     = pciInfo.subsystemId();
        config->link_speed       = static_cast<uint32_t>(pciInfo.linkSpeed().scaledValue(MicSampleBase::eMega)); /* In MT/s */
        config->link_width       = pciInfo.linkWidth();
        config->payload_size     = pciInfo.payloadSize();
        config->read_req_size    = pciInfo.requestSize();
        config->access_violation = pciInfo.hasFullAccess() ? DISABLED : ENABLED;
        config->class_code       = pciInfo.classCode();
    }
    return errCode;
}

/*! @} */

/* Device Memory Information */

/*! \addtogroup Memory_Information Memory Information.
 * NONE.
 * @{
 * @anchor Group7
 */

/**
 * @fn     uint32_t mic_get_ecc_mode(MIC *device, uint16_t *mode)
 *
 * @brief  Determine if the ECC mode is enabled or disabled.
 *
 * @param  device  A valid handle to an open device.
 *
 * @param  mode    A pointer to the variable to receive the ecc mode value.
 *
 * If the function succeeds, the \b uint16_t \b *mode argument is set to a non-zero
 * value. The ECC mode can only be queried when the device is in the online state.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_DEVICE_NOT_ONLINE
 * - MICSDKERR_INTERNAL_ERROR
 * - MICSDKERR_DEVICE_IO_ERROR
 *
 */
uint32_t mic_get_ecc_mode(MIC *devHandle, uint16_t *mode)
{
    if ((NULL == devHandle) || (NULL == mode))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);
    if (NULL == device)
        return errCode;

    MicMemoryInfo memInfo;
    errCode = device->getMemoryInfo(&memInfo);

    if (false == MicDeviceError::isError(errCode))
        *mode = memInfo.isEccEnabled() ? ENABLED : DISABLED;

    return errCode;
}

/* Device memory info */
/**
 * @fn     uint32_t mic_get_memory_info(MIC *device, MIC_MEM_INFO *mem_info)
 *
 * @brief  Retrieves the Memory information of the specified coprocessor.
 *
 * @param  device   A valid handle to an open device.
 * @param  mem_info A pointer to the MIC_MEM_INFO structure that receives the Memory information.
 *
 * If the function succeeds, the memory information data is contained in the buffer
 * pointed to by \b MIC_MEM_INFO \b *mem_info parameter. The memory information is only available when
 * the device is online. \b MIC_MEM_INFO structure contains the following members:\n\n
 * \a density :\n
 *    Represents memory density in megabits.
 *
 * \a size :\n
 *    Represents memory size in megabytes.
 *
 * \a speed :\n
 *    Represents memory speed in MT/s.
 *
 * \a freq :\n
 *    Represents memory frequency in MHz.
 *
 * \a volt :\n
 *    Represents memory voltage in milliVolts.
 *    This entry is deprecated and always returns 0.
 *
 * \a revision :\n
 *    Represents hardware revision of the memory.
 *
 * \a ecc :\n
 *    Indicates whether the ECC mode is enabled or disabled. A non-zero value is returned if it is enabled.
 *
 * \a vendor_name :\n
 *    Represents the memory device vendor’s name.
 *
 * \a memory_type :\n
 *    Represents the memory type. (e.g. GDDR)
 *
 * \a memory_tech :\n
 *    Represents the memory technology.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_DEVICE_NOT_ONLINE
 * - MICSDKERR_INTERNAL_ERROR
 * - MICSDKERR_DEVICE_IO_ERROR
 *
 */
uint32_t mic_get_memory_info(MIC *devHandle, MIC_MEM_INFO *mem_info)
{
    if ((NULL == devHandle) || (NULL == mem_info))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);
    if (NULL == device)
        return errCode;

    MicMemoryInfo memInfo;
    errCode = device->getMemoryInfo(&memInfo);

    if (false == MicDeviceError::isError(errCode)) {
        // Fill up MicMemory Info.
        string vendorStr = memInfo.vendorName();
        strncpy(mem_info->vendor_name, vendorStr.c_str(), NAME_MAX);
        mem_info->vendor_name[NAME_MAX - 1] = '\0';

        string memTypeStr = memInfo.memoryType();
        strncpy(mem_info->memory_type, memTypeStr.c_str(), NAME_MAX);
        mem_info->memory_type[NAME_MAX - 1] = '\0';

        string memTechStr = memInfo.technology();
        strncpy(mem_info->memory_tech, memTechStr.c_str(), NAME_MAX);
        mem_info->memory_tech[NAME_MAX - 1] = '\0';

        mem_info->revision = memInfo.revision();
        mem_info->density  = static_cast<uint32_t>(memInfo.density().scaledValue(MicMemory::eMega)); // in Mega-bits
        mem_info->size     = static_cast<uint32_t>(memInfo.size().scaledValue(MicMemory::eMega)); // in Mega bytes
        mem_info->speed    = static_cast<uint32_t>(memInfo.speed().scaledValue(MicSpeed::eMega)); // in MT/s
        mem_info->freq     = static_cast<uint32_t>(memInfo.frequency().scaledValue(MicFrequency::eMega)); // in Mega hz
        mem_info->volt     = 0;
        mem_info->ecc      = memInfo.isEccEnabled() ? ENABLED : DISABLED;
    }
    return errCode;
}

/*! @} */

/* Processor info */

/*! \addtogroup Processor_Information Processor Information.
 * NONE.
 * @{
 * @anchor Group8
 */

/**
 * @fn     uint32_t mic_get_processor_info(MIC *device, MIC_PROCESSOR_INFO *processor)
 *
 * @brief  Retrieves processor information of the specified coprocessor.
 *
 * @param  device    A valid handle to an open device.
 * @param  processor A pointer to the MIC_PROCESSOR_INFO structure that receives the processor information.
 *
 * If the function succeeds, the processor information data is contained in the buffer pointed to
 * by \b MIC_PROCESSOR_INFO \b *processor parameter. The processor information is available in any device state as long as
 * the device driver is loaded. \b MIC_PROCESSOR_INFO structure contains the following members:\n\n
 * \a stepping_data :\n
 *    Represents stepping Id.\n
 *    On Linux, the stepping Id is read from \b /sys/class/mic/mic\<n\>/stepping_data sysfs entry, and
 *    corresponds to bits 0-3 of the result of CPUID instruction with EAX=1 run on the coprocessor.
 *    On Microsoft Windows, the Id is read from \b stepping_data WMI entry and corresponds to bits 0-3 of
 *    the result of CPUID instruction with EAX=1 run on the coprocessor.
 *
 * \a substepping_data :\n
 *    Represents sub-stepping Id.\n
 *    This is not supported in Intel® Xeon Phi™ coprocessor x200.\n
 *    On Linux, the sub-stepping Id is read from \b /sys/class/mic/mic\<n\>substepping_data.
 *    On Microsoft Windows, the Id is read from \b substepping_data WMI entry.
 *
 * \a model :\n
 *    Represents the coprocessor model.\n
 *    On Linux, the model is read from \b /sys/class/mic/mic\<n\>/model sysfs entry (for Intel® Xeon Phi™ coprocessor x100) and
 *    \b /sys/class/mic/mic\<n\>/device/dmi/cpu_sig (for Intel® Xeon Phi™ coprocessor x200), and corresponds
 *    to bits 4-7 of the result of CPUID instruction with EAX=1 when run on the coprocessor.
 *    On Microsoft Windows, these values are read from \b model WMI entry and correspond to bits 4-7
 *    of the result of CPUID instruction with EAX=1 when run on the coprocessor.
 *
 * \a model_ext :\n
 *    Represents the coprocessor extended model.\n
 *    This is not supported in Intel® Xeon Phi™ coprocessor x200.\n
 *    On Linux, the model is read from \b /sys/class/mic/mic\<n\>/model_ext sysfs entry, and corresponds
 *    to bits 20-27 of the result of CPUID instruction with EAX=1 when run on the coprocessor.
 *    On Microsoft Windows, these values are read from \b extended_model WMI entry and correspond to bits 20-27
 *    of the result of CPUID instruction with EAX=1 when run on the coprocessor.
 *
 * \a family :\n
 *    Represents the coprocessor family.\n
 *    This is not supported in Intel® Xeon Phi™ coprocessor x200.\n
 *    On Linux, the family is read from \b /sys/class/mic/mic\<n\>/family_data sysfs entry (for Intel® Xeon Phi™ coprocessor x100), and corresponds
 *    to bits 8-11 of the result of CPUID instruction with EAX=1 when run on the coprocessor.
 *    On Microsoft Windows, these values are read from \b family_data WMI entry and correspond to bits 8-11
 *    of the result of CPUID instruction with EAX=1 when run on the coprocessor.
 *
 * \a family_ext :\n
 *    Represents the coprocessor extended family.\n
 *    This is not supported in Intel® Xeon Phi™ coprocessor x200.\n
 *    On Linux, the family is read from \b /sys/class/mic/mic\<n\>/extended_family sysfs entry (for Intel® Xeon Phi™ coprocessor x100), and corresponds
 *    to bits 16-19 of the result of CPUID instruction with EAX=1 when run on the coprocessor.
 *    On Microsoft Windows, these values are read from \b extended_family WMI entry and correspond to bits 16-19
 *    of the result of CPUID instruction with EAX=1 when run on the coprocessor.
 *
 * \a type :\n
 *    Represents the coprocessor type.\n
 *    On Linux, the type is read from \b /sys/class/mic/mic\<n\>/processor sysfs entry (for Intel® Xeon Phi™ coprocessor x100) and
 *    \b /sys/class/mic/mic\<n\>/device/dmi/cpu_sig (for Intel® Xeon Phi™ coprocessor x200), and corresponds
 *    to bits 12-13 of the result of CPUID instruction with EAX=1 when run on the coprocessor.
 *    On Microsoft Windows, these values are read from \b processor(for Intel® Xeon Phi™ coprocessor x100) and
 *    \b DeviceType (for Intel® Xeon Phi™ coprocessor x200)  WMI entry and correspond to bits 12-13
 *    of the result of CPUID instruction with EAX=1 when run on the coprocessor.
 *
 * \a stepping :\n
 *    Represents the coprocessor stepping info. (e.g. "B0")
 *
 * \a sku :\n
 *    Represents coprocessor device SKU.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_INTERNAL_ERROR
 *
 */
uint32_t mic_get_processor_info(MIC *devHandle, MIC_PROCESSOR_INFO *processor)
{
    if ((NULL == devHandle) || (NULL == processor))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);
    if (NULL == device)
        return errCode;

    MicProcessorInfo procInfo;
    errCode = device->getProcessorInfo(&procInfo);

    if (false == MicDeviceError::isError(errCode)) {
       // Fill up the processor Info.
        processor->type             = procInfo.type().value();
        processor->model            = procInfo.model().value();
        processor->model_ext        = procInfo.extendedModel().value();
        processor->family           = procInfo.family().value();
        processor->family_ext       = procInfo.extendedFamily().value();
        processor->stepping_data    = procInfo.steppingId().value();
        processor->substepping_data = procInfo.substeppingId().value();

        string stepStr = procInfo.stepping().value();
        strncpy(processor->stepping, stepStr.c_str(), NAME_MAX);
        processor->stepping[NAME_MAX - 1] = '\0';

        string skuStr = procInfo.sku().value();
        strncpy(processor->sku, skuStr.c_str(), NAME_MAX);
        processor->sku[NAME_MAX - 1] = '\0';
    }
    return errCode;
}

/*! @} */

/* Coprocessor OS core info */

/*! \addtogroup Coprocessor_core_Information Coprocessor core Information.
 * NONE.
 * @{
 * @anchor Group9
 */

/**
 * @fn     uint32_t mic_get_cores_count(MIC *device, uint32_t *count)
 *
 * @brief  Retrieves the number of active cores in the coprocessor.
 *
 * @param  device A valid handle to an open device.
 * @param  count  A pointer to the variable to receive the cores count.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_DEVICE_NOT_ONLINE
 * - MICSDKERR_DEVICE_IO_ERROR
 *
 */
uint32_t mic_get_cores_count(MIC *devHandle, uint32_t *count)
{
    if ((NULL == devHandle) || (NULL == count))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);
    if (NULL == device)
        return errCode;

    MicCoreInfo coreInfo;
    errCode = device->getCoreInfo(&coreInfo);

    if (false == MicDeviceError::isError(errCode))
        *count = (uint32_t)coreInfo.coreCount().value();

    return errCode;
}

/**
 * @fn     uint32_t mic_get_cores_voltage(MIC *device, uint32_t *voltage)
 *
 * @brief  Retrieves the cores voltage (in millivolts) of the specified coprocessor.
 *
 * @param  device   A valid handle to an open device.
 * @param  voltage  A pointer to the variable to receive the cores voltage.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_DEVICE_NOT_ONLINE
 * - MICSDKERR_DEVICE_IO_ERROR
 *
 */
uint32_t mic_get_cores_voltage(MIC *devHandle, uint32_t *voltage)
{
    if ((NULL == devHandle) || (NULL == voltage))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);
    if (NULL == device)
        return errCode;

    MicCoreInfo coreInfo;
    errCode = device->getCoreInfo(&coreInfo);

    if (false == MicDeviceError::isError(errCode))
        *voltage = static_cast<uint32_t>(coreInfo.voltage().scaledValue(MicVoltage::eMilli)); // in mV.

    return errCode;
}

/**
 * @fn     uint32_t mic_get_cores_frequency(MIC *device, uint32_t *frequency)
 *
 * @brief  Retrieves the cores frequency (in MHz) of the specified coprocessor.
 *
 * @param  device     A valid handle to an open device.
 * @param  frequency  A pointer to the variable to receive the cores frequency.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_DEVICE_NOT_ONLINE
 * - MICSDKERR_DEVICE_IO_ERROR
 *
 */
uint32_t mic_get_cores_frequency(MIC *devHandle, uint32_t *frequency)
{
    if ((NULL == devHandle) || (NULL == frequency))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);
    if (NULL == device)
        return errCode;

    MicCoreInfo coreInfo;
    errCode = device->getCoreInfo(&coreInfo);

    if (false == MicDeviceError::isError(errCode))
        *frequency = static_cast<uint32_t>(coreInfo.frequency().scaledValue(MicFrequency::eMega)); // in MHz

    return errCode;
}
/*! @} */

/*! \addtogroup Version_Information Version Information.
 * NONE.
 * @{
 * @anchor Group10
 */

/**
 * @fn     uint32_t mic_get_coprocessor_os_version(MIC *device, char *value, size_t *size)
 *
 * @brief  Retrieves the version of the Linux Operating System running on the coprocessor.
 *
 * @param  device  A valid handle to an open device.
 * @param  value   A pointer to the buffer to receive the version info.
 * @param  size    A pointer to receive the size of the data copied/required.
 *
 * This functions returns the current version of Linux OS running on the coprocessor.
 * The \b size_t \b *size argument is an input/output argument. Its input
 * value is used to determine the maximum number of characters (including trailing
 * nul) that may be transferred to the buffer. The appropriate \b size value
 * can be queried by setting it to 0. The behavior is undetermined
 * if the input size is greater than the actual buffer size, or if the buffer
 * references a NULL or invalid memory location.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_DEVICE_NOT_ONLINE
 * - MICSDKERR_DEVICE_IO_ERROR
 * - MICSDKERR_INTERNAL_ERROR
 *
 */
uint32_t mic_get_coprocessor_os_version(MIC *devHandle, char *value, size_t *size)
{
    if ((NULL == devHandle) || (NULL == value) || (NULL == size))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);
    if (NULL == device)
        return errCode;

    MicPlatformInfo platformInfo;
    errCode = device->getPlatformInfo(&platformInfo);

    if (true == MicDeviceError::isError(errCode))
        return errCode;

    string versionStr = platformInfo.coprocessorOsVersion().value();
    if (0 < *size) {
        strncpy(value, versionStr.c_str(), *size);
        value[*size - 1] = '\0'; // Safe null termination
    }

    if (*size < versionStr.length() + 1)
        *size = versionStr.length() + 1;

    return errCode;
}

/**
 * @fn     uint32_t mic_get_part_number(MIC *device, char *value, size_t *size)
 *
 * @brief  Retrieves the part number of the coprocessor.
 *
 * @param  device  A valid handle to an open device.
 * @param  value   A pointer to the buffer to receive the part number.
 * @param  size    A pointer to receive the size of the data copied/required.
 *
 * This function is supported only in Intel® Xeon Phi™ x200 product family.
 * For Intel® Xeon Phi™ x100 product family the function returns a 'Operation not supported' error.
 *
 * This functions returns the part number of the coprocessor to value buffer.
 * The \b size_t \b *size argument is an input/output argument. Its input
 * value is used to determine the maximum number of characters (include trailing
 * nul) that may be transferred to the buffer. The appropriate \b size value
 * can be queried by setting its value to 0. The behavior is undetermined
 * if the input size is greater than the actual buffer size, or if the buffer
 * references a NULL or invalid memory location.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_DEVICE_NOT_ONLINE
 * - MICSDKERR_DEVICE_IO_ERROR
 * - MICSDKERR_INTERNAL_ERROR
 * - MICSDKERR_NOT_SUPPORTED
 *
 */
uint32_t mic_get_part_number(MIC *devHandle, char *value, size_t *size)
{
    if ((NULL == devHandle) || (NULL == value) || (NULL == size))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);
    if (NULL == device)
        return errCode;

    if(device->isReady())
        return MICSDKERR_DEVICE_NOT_ONLINE;

    MicPlatformInfo platformInfo;
    errCode = device->getPlatformInfo(&platformInfo);

    if (true == MicDeviceError::isError(errCode))
        return errCode;

    string partNumStr = platformInfo.partNumber().value();
    if (0 < *size) {
        strncpy(value, partNumStr.c_str(), *size);
        value[*size - 1] = '\0'; // Safe null termination
    }

    if (*size < partNumStr.length() + 1)
        *size = partNumStr.length() + 1;

    return errCode;
}

/**
 * @fn     uint32_t mic_get_fsc_strap(MIC *device, char *value, size_t *size)
 *
 * @brief  Retrieves the fan speed controller(fsc) strap information of the coprocessor.
 *         Note: This is not supported in Intel® Xeon Phi™ coprocessor x200.
 *
 * @param  device  A valid handle to an open device.
 * @param  value   A pointer to the buffer to receive the strap info.
 * @param  size    A pointer to receive the size of the data copied/required.
 *
 * The \b size_t \b *size argument is an input/output argument. Its input
 * value is used to determine the maximum number of characters (including trailing
 * nul) that may be transferred to the buffer. The appropriate \b size value
 * can be queried by setting it to 0. The behavior is undetermined
 * if the input size is greater than the actual buffer size, or if the buffer
 * references a NULL or invalid memory location.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_DEVICE_NOT_ONLINE
 * - MICSDKERR_DEVICE_IO_ERROR
 * - MICSDKERR_INTERNAL_ERROR
 * - MICSDKERR_NOT_SUPPORTED
 *
 */
uint32_t mic_get_fsc_strap(MIC *devHandle, char *value, size_t *size)
{
    if ((NULL == devHandle) || (NULL == value) || (NULL == size))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);
    if (NULL == device)
        return errCode;

    if(device->isReady())
        return MICSDKERR_DEVICE_NOT_ONLINE;

    MicPlatformInfo platformInfo;
    errCode = device->getPlatformInfo(&platformInfo);

    if (true == MicDeviceError::isError(errCode))
        return errCode;

    if(device->deviceType() == X200_TYPE_STR)
        return MICSDKERR_NOT_SUPPORTED;

    string strapStr = platformInfo.strapInfo().value();
    if (0 < *size) {
        strncpy(value, strapStr.c_str(), *size);
        value[*size - 1] = '\0'; // Safe null termination
    }

    if (*size < strapStr.length() + 1)
        *size = strapStr.length() + 1;

    return errCode;
}

/**
 * @fn     uint32_t mic_get_flash_version(MIC *device, char *value, size_t *size)
 *
 * @brief  Retrieves the flash image version of the coprocessor.
 *
 * @param  device  A valid handle to an open device.
 * @param  value   A pointer to the buffer to receive the version info.
 * @param  size    A pointer to receive the size of the data copied/required.
 *
 * This function returns the loaded and active flash image version to the \b value
 * buffer. The \b size_t \b *size argument is an input/output argument. Its input value
 * is used to determine the maximum number of characters (including trailing
 * nul) that may be transferred to the buffer. The appropriate \b size value
 * can be queried by setting it to 0. The behavior is undetermined
 * if the input size is greater than the actual buffer size, or if the buffer
 * references a NULL or invalid memory location.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_DEVICE_IO_ERROR
 * - MICSDKERR_INTERNAL_ERROR
 *
 */
uint32_t mic_get_flash_version(MIC *devHandle, char *value, size_t *size)
{
    if ((NULL == devHandle) || (NULL == value) || (NULL == size))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);
    if (NULL == device)
        return errCode;

    MicVersionInfo versionInfo;
    errCode = device->getVersionInfo(&versionInfo);

    if (true == MicDeviceError::isError(errCode))
        return errCode;

    string flashStr = versionInfo.flashVersion().value();
    if (0 < *size) {
        strncpy(value, flashStr.c_str(), *size);
        value[*size - 1] = '\0'; // Safe null termination
    }

    if (*size < flashStr.length() + 1)
        *size = flashStr.length() + 1;

    return errCode;
}

/**
 * @fn     uint32_t mic_get_me_version(MIC *device, char *value, size_t *size)
 *
 * @brief  Retrieves the management engine version of the coprocessor.
 *
 * @param  device  A valid handle to an open device.
 * @param  value   A pointer to the buffer to receive the version string.
 * @param  size    A pointer to receive the size of the data copied/required.
 *
 * This function is supported only in Intel® Xeon Phi™ x200 product family.
 * For Intel® Xeon Phi™ x100 product family the function returns a 'Operation not supported' error.
 *
 * The \b size_t \b *size argument is an input/output argument. Its input value
 * is used to determine the maximum number of characters (including trailing
 * nul) that may be transferred to the buffer. The appropriate \b size value
 * can be queried by setting it to 0. The behavior is undetermined
 * if the input size is greater than the actual buffer size, or if the buffer
 * references a NULL or invalid memory location.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_DEVICE_IO_ERROR
 * - MICSDKERR_INTERNAL_ERROR
 * - MICSDKERR_NOT_SUPPORTED
 *
 */
uint32_t mic_get_me_version(MIC *devHandle, char *value, size_t *size)
{
    if ((NULL == devHandle) || (NULL == value) || (NULL == size))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);
    if (NULL == device)
        return errCode;

    if(device->isReady())
        return MICSDKERR_DEVICE_NOT_ONLINE;

    MicVersionInfo versionInfo;
    errCode = device->getVersionInfo(&versionInfo);

    if (true == MicDeviceError::isError(errCode))
        return errCode;

    string meVerStr = versionInfo.meVersion().value();
    if (0 < *size) {
        strncpy(value, meVerStr.c_str(), *size);
        value[*size - 1] = '\0'; // Safe null termination
    }

    if (*size < meVerStr.length() + 1)
        *size = meVerStr.length() + 1;

    return errCode;
}

/**
 * @fn     uint32_t mic_get_smc_fwversion(MIC *device, char *value, size_t *size)
 *
 * @brief  Retrieves the SMC firmware version of the coprocessor.
 *
 * @param  device  A valid handle to an open device.
 * @param  value   A pointer to the buffer to receive the firmware version string.
 * @param  size    A pointer to receive the size of the data copied/required.
 *
 * The \b size_t \b *size argument is an input/output argument. Its input value
 * is used to determine the maximum number of characters (including trailing
 * nul) that may be transferred to the buffer. The appropriate \b size value
 * can be queried by setting it to 0. The behavior is undetermined
 * if the input size is greater than the actual buffer size, or if the buffer
 * references a NULL or invalid memory location.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_DEVICE_IO_ERROR
 * - MICSDKERR_INTERNAL_ERROR
 *
 */
uint32_t mic_get_smc_fwversion(MIC *devHandle, char *value, size_t *size)
{
    if ((NULL == devHandle) || (NULL == value) || (NULL == size))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);
    if (NULL == device)
        return errCode;

    if(device->isReady())
        return MICSDKERR_DEVICE_NOT_ONLINE;

    MicVersionInfo versionInfo;
    errCode = device->getVersionInfo(&versionInfo);

    if (true == MicDeviceError::isError(errCode))
        return errCode;

    string smcVerStr = versionInfo.smcVersion().value();
    if (0 < *size) {
        strncpy(value, smcVerStr.c_str(), *size);
        value[*size - 1] = '\0'; // Safe null termination
    }

    if (*size < smcVerStr.length() + 1)
        *size = smcVerStr.length() + 1;

    return errCode;
}

/**
 * @fn     uint32_t mic_get_smc_hwrevision(MIC *device, char *value, size_t *size)
 *
 * @brief  Retrieves the SMC hardware version of the coprocessor.
 *
 * @param  device  A valid handle to an open device.
 * @param  value   A pointer to the buffer to receive the hardware version string.
 * @param  size    A pointer to receive the size of the data copied/required.
 *
 * The \b size_t *size argument is an input/output argument. Its input value
 * is used to determine the maximum number of characters (including trailing
 * nul) that may be transferred to the buffer. The appropriate \b size value
 * can be queried by setting it to 0. The behavior is undetermined
 * if the input size is greater than the actual buffer size, or if the buffer
 * references a NULL or invalid memory location.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_DEVICE_NOT_ONLINE
 * - MICSDKERR_DEVICE_IO_ERROR
 * - MICSDKERR_INTERNAL_ERROR
 *
 */
uint32_t mic_get_smc_hwrevision(MIC *devHandle, char *value, size_t *size)
{
    if ((NULL == devHandle) || (NULL == value) || (NULL == size))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);
    if (NULL == device)
        return errCode;

    if(device->isReady())
        return MICSDKERR_DEVICE_NOT_ONLINE;

    MicPlatformInfo platInfo;
    errCode = device->getPlatformInfo(&platInfo);

    if (true == MicDeviceError::isError(errCode))
        return errCode;

    string hwVerStr = platInfo.featureSet().value();
    if (0 < *size) {
        strncpy(value, hwVerStr.c_str(), *size);
        value[*size - 1] = '\0'; // Safe null termination
    }

    if (*size < hwVerStr.length() + 1)
        *size = hwVerStr.length() + 1;

    return errCode;
}

/**
 * @fn     uint32_t mic_get_smc_boot_loader_ver(MIC *device, char *value, size_t *size)
 *
 * @brief  Retrieves the SMC boot loader version of the coprocessor.
 *
 * @param  device  A valid handle to an open device.
 * @param  value   A pointer to the buffer to receive the version string.
 * @param  size    A pointer to receive the size of the data copied/required.
 *
 * This function is supported only in Intel® Xeon Phi™ x100 product family.
 * For Intel® Xeon Phi™ x200 product family the function returns an
 * empty string for boot loader version.
 *
 * The \b size_t \b *size argument is an input/output argument. Its input value
 * is used to determine the maximum number of characters (including trailing
 * nul) that may be transferred to the buffer. The appropriate \b size value
 * can be queried by setting it to 0. The behavior is undetermined
 * if the input size is greater than the actual buffer size, or if the buffer
 * references a NULL or invalid memory location.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_DEVICE_IO_ERROR
 * - MICSDKERR_INTERNAL_ERROR
 *
 */
uint32_t mic_get_smc_boot_loader_ver(MIC *devHandle, char *value, size_t *size)
{
    if ((NULL == devHandle) || (NULL == value) || (NULL == size))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);
    if (NULL == device)
        return errCode;

    if(device->isReady())
        return MICSDKERR_DEVICE_NOT_ONLINE;

    MicVersionInfo versionInfo;
    errCode = device->getVersionInfo(&versionInfo);

    if (true == MicDeviceError::isError(errCode))
        return errCode;

    string bootStr = versionInfo.smcBootLoaderVersion().value();
    if (0 < *size) {
        strncpy(value, bootStr.c_str(), *size);
        value[*size - 1] = '\0'; // Safe null termination
    }

    if (*size < bootStr.length() + 1)
        *size = bootStr.length() + 1;

    return errCode;
}

/**
 * @fn     uint32_t mic_is_smc_boot_loader_ver_supported(MIC *device, int *supported)
 *
 * @brief  Determine if the Boot-loader version is supported by the coprocessor.
 *
 * @param  device      A valid handle to an open device.
 * @param  supported   A pointer to the variable to receive the status.
 *
 * This function returns a non-zero value in the \b int \b *supported argument if the Boot-loader
 * version is supported by the current SMC firmware on the specified coprocessor, otherwise
 * it is set to 0.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_DEVICE_IO_ERROR
 * - MICSDKERR_INTERNAL_ERROR
 *
 */
uint32_t mic_is_smc_boot_loader_ver_supported(MIC *devHandle, int *supported)
{
    if ((NULL == devHandle) || (NULL == supported))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);
    if (NULL == device)
        return errCode;

    if(device->isReady())
        return MICSDKERR_DEVICE_NOT_ONLINE;

    MicVersionInfo versionInfo;
    errCode = device->getVersionInfo(&versionInfo);

    if (true == MicDeviceError::isError(errCode))
        return errCode;

    string bootStr = versionInfo.smcBootLoaderVersion().value();

    *supported = DISABLED;
    if ((true == device->isOnline()) && (true != bootStr.empty())) {
        // Device online and boot loader version available.
        *supported = ENABLED;
    }
    return errCode;
}

/*! @} */

/* Led mode */

/*! \addtogroup LED_Mode_Information LED Mode Information.
 * NONE.
 * @{
 * @anchor Group11
 */

/**
 * @fn     uint32_t mic_get_led_alert(MIC *device, uint32_t *led_alert)
 *
 * @brief  Determine if the LED alert is set in the coprocessor.
 *
 * @param  device     A valid handle to an open device.
 * @param  led_alert  A pointer to the variable to receive the LED state.
 *
 * This function returns a non-zero value in the \b uint32_t \b *led_alert argument if
 * the LED alert is set in the specified coprocessor, otherwise it is set to 0.
 * The LED mode is only available when the device is online.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_DEVICE_NOT_ONLINE
 * - MICSDKERR_DEVICE_IO_ERROR
 *
 */
uint32_t mic_get_led_alert(MIC *devHandle, uint32_t *led_alert)
{
    if ((NULL == devHandle) || (NULL == led_alert))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);
    if (NULL != device)
        errCode = device->getLedMode(led_alert);

    return errCode;
}

/**
 * @fn     uint32_t mic_set_led_alert(MIC *device, uint32_t led_alert)
 *
 * @brief  Activate specified LED mode.
 *
 * @param  device     A valid handle to an open device.
 * @param  led_alert  The LED mode to set for the coprocessor.
 *
 * The device has to be online to be able to set the LED mode.
 * Valid values for the \b led_alert argument are 0 and 1. A value of
 * 1 indicates the 'Identify mode' and 0 indicates the 'Normal mode'.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_DEVICE_NOT_ONLINE
 * - MICSDKERR_DEVICE_IO_ERROR
 *
 */
uint32_t mic_set_led_alert(MIC *devHandle, uint32_t led_alert)
{
    if (NULL == devHandle || (led_alert != 0 && led_alert != 1))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);
    if (NULL != device)
        errCode = device->setLedMode(led_alert);

    return errCode;
}

/*! @} */

/* Turbo info */
/*! \addtogroup Turbo_State_Information Turbo State Information.
 * NONE.
 * @{
 * @anchor Group12
 */

/**
 * @fn     uint32_t mic_get_turbo_mode(MIC *device, int *mode)
 *
 * @brief  Determine if the turbo mode is enabled or disabled.
 *
 * @param  device  A valid handle to an open device.
 * @param  mode    A pointer to the variable to receive the turbo mode enabled state.
 *
 * This function returns the current turbo mode of the specified coprocessor.
 * A non-zero value returned in the \b mode indicates that the turbo mode is enabled; a zero value
 * indicates the mode is disabled.
 * The turbo mode information is only available when the coprocessor is online.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_DEVICE_NOT_ONLINE
 * - MICSDKERR_DEVICE_IO_ERROR
 *
 */
uint32_t mic_get_turbo_mode(MIC *devHandle, int *mode)
{
    if ((NULL == devHandle) || (NULL == mode))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);
    if (!device) {
        return errCode;
    }

    bool enabled = false;
    if((errCode = device->getTurboMode(&enabled)) == MICSDKERR_SUCCESS)
        *mode = enabled ? ENABLED : DISABLED;

    return errCode;
}

/**
 * @fn     uint32_t mic_get_turbo_state_valid(MIC *device, int *valid)
 *
 * @brief  Determine if the turbo mode is available on the coprocessor.
 *
 * @param  device  A valid handle to an open device.
 * @param  valid   A pointer to the variable to receive the turbo mode available state.
 *
 * This function returns the current turbo mode availability state on the specified coprocessor.
 * A non-zero value returned in the \b valid indicates that the turbo mode is supported; a zero value
 * indicates that it is not supported.
 * The turbo mode information is only available when the coprocessor is online.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_DEVICE_NOT_ONLINE
 * - MICSDKERR_DEVICE_IO_ERROR
 *
 */
uint32_t mic_get_turbo_state_valid(MIC *devHandle, int *valid)
{
    if ((NULL == devHandle) || (NULL == valid))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);
    if (!device) {
        return errCode;
    }

    bool enabled   = false;
    bool available = false;
    if((errCode = device->getTurboMode(&enabled, &available)) == MICSDKERR_SUCCESS)
        *valid = available ? ENABLED : DISABLED;

    return errCode;
}

/**
 * @fn     uint32_t mic_get_turbo_state(MIC *device, int *active)
 *
 * @brief  Determine if the turbo mode is active on the coprocessor.
 *
 * @param  device  A valid handle to an open device.
 * @param  active  A pointer to the variable to receive the turbo mode active state.
 *
 * This function returns the current turbo state of the specified coprocessor.
 * A non-zero value returned in the \b active indicates that the turbo mode is active; a zero value
 * indicates the mode is inactive.
 * The turbo mode information is only available when the coprocessor is online.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_DEVICE_NOT_ONLINE
 * - MICSDKERR_DEVICE_IO_ERROR
 *
 */
uint32_t mic_get_turbo_state(MIC *devHandle, int *active)
{
    if ((NULL == devHandle) || (NULL == active))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);
    if (!device) {
        return errCode;
    }

    bool enabled   = false;
    bool available = false;
    bool state     = false;
    if((errCode = device->getTurboMode(&enabled, &available, &state)) == MICSDKERR_SUCCESS)
        *active = state ? ENABLED : DISABLED;

    return errCode;
}

/**
 * @fn     uint32_t mic_set_turbo_mode(MIC *device, int mode)
 *
 * @brief  Enable or disable the turbo mode.
 *
 * @param  device  A valid handle to an open device.
 * @param  mode    The turbo mode state to set on the coprocessor.
 *
 * This function is supported only in Intel® Xeon Phi™ x100 product family.
 * For Intel® Xeon Phi™ x200 product family the function returns a 'Operation not supported' error.
 *
 * The device has to be online to be able to set the turbo mode state.
 * Valid values for the \b mode argument are 1 and 0. A value of
 * 1 enables turbo mode and 0 will disable the turbo mode.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_DEVICE_NOT_ONLINE
 * - MICSDKERR_DEVICE_IO_ERROR
 * - MICSDKERR_NOT_SUPPORTED
 *
 */
uint32_t mic_set_turbo_mode(MIC *devHandle, int mode)
{
    if (NULL == devHandle || (mode != 0 && mode != 1))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);
    if (NULL != device)
        errCode = device->setTurboModeEnabled(!!mode);

    return errCode;
}

/*! @} */

/* SMC config */

/*! \addtogroup SMC_Configuration_Information SMC Configuration Information.
 * NONE.
 * @{
 * @anchor Group13
 */

/**
 * @fn     uint32_t mic_get_smc_persistence_flag(MIC *device, int *persist_flag)
 *
 * @brief  Retrieves the current SMC persistence state.
 *
 * @param  device        A valid handle to an open device.
 * @param  persist_flag  A pointer to the variable to receive the SMC persistence state.
 *
 * This function is supported only in Intel® Xeon Phi™ x100 product family.
 * For Intel® Xeon Phi™ x200 product family the function returns a 'Operation not supported' error.
 *
 * The SMC persistence state is only available when the device is online.
 * If the \b persist_flag return value is 0, it indicates persistence is turned OFF, while a non-zero value
 * indicates that persistence is turned ON. Once this flag is turned ON, the SMC will retain certain
 * parameters, like Power Limits and Time Windows, even after a power cycle.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_DEVICE_NOT_ONLINE
 * - MICSDKERR_INTERNAL_ERROR
 * - MICSDKERR_NOT_SUPPORTED
 *
 */
uint32_t mic_get_smc_persistence_flag(MIC *devHandle, int *persist_flag)
{
    if ((NULL == devHandle) || (NULL == persist_flag))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);
    if (!device) {
        return errCode;
    }

    bool enabled = false;
    errCode = device->getSmcPersistenceState(&enabled);
    if((errCode = device->getSmcPersistenceState(&enabled)) == MICSDKERR_SUCCESS)
        *persist_flag = enabled ? ENABLED : DISABLED;

    return errCode;
}

/**
 * @fn     uint32_t mic_set_smc_persistence_flag(MIC *device, int persist_flag)
 *
 * @brief  Enable or disable SMC persistence.
 *
 * @param  device        A valid handle to an open device.
 * @param  persist_flag  The SMC persistence state to set on the coprocessor.
 *
 * This function is supported only in Intel® Xeon Phi™ x100 product family.
 * For Intel® Xeon Phi™ x200 product family the function returns a 'Operation not supported' error.
 *
 * The device has to be online to be able to set the persistence state.
 * Valid values for the \b persist_flag argument are 1 and 0. A value of
 * 1 turns the persistence 'ON', and 0 turns it 'OFF'.
 * When 'ON', the SMC settings will persist over a boot sequence.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_DEVICE_NOT_ONLINE
 * - MICSDKERR_INTERNAL_ERROR
 * - MICSDKERR_NOT_SUPPORTED
 *
 */
uint32_t mic_set_smc_persistence_flag(MIC *devHandle, int persist_flag)
{
    if (NULL == devHandle || (persist_flag != 0 && persist_flag != 1))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);
    if (NULL != device)
        errCode = device->setSmcPersistenceEnabled(!!persist_flag);

    return errCode;
}

/*! @} */

/* Throttle state info */

/*! \addtogroup Throttle_State_Information Throttle State Information.
 * NONE.
 * @{
 * @anchor Group14
 */

/**
 * @fn     uint32_t mic_get_thermal_throttle_info(MIC *device, MIC_THROTTLE_INFO *info)
 *
 * @brief  Retrieves the thermal throttle information of the specified coprocessor.
 *
 * @param  device A valid handle to an open device.
 * @param  info   A pointer to the MIC_THROTTLE_INFO structure that receives the throttle information.
 *
 * If the function succeeds, the thermal throttle information data is contained in the buffer
 * pointed to by the \b MIC_THROTTLE_INFO \b *info parameter. The throttle information is only
 * available when the device is online. \b MIC_THROTTLE_INFO structure contains the following
 * members:\n\n
 * \a duration :\n
 *    Represents the current thermal throttling duration in milliseconds.
 *
 * \a totalTime :\n
 *    Represents the total thermal throttling time since device boot in milliseconds.
 *
 * \a eventCount :\n
 *    Represents the total count of thermal throttling events since device boot.
 *
 * \a active :\n
 *    A flag to indicate if the device is currently thermal throttling. A non-zero
 *    is set for the \b active variable if the device is throttling, otherwise it is zero.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_DEVICE_NOT_ONLINE
 * - MICSDKERR_DEVICE_IO_ERROR
 *
 */
uint32_t mic_get_thermal_throttle_info(MIC *devHandle, MIC_THROTTLE_INFO *info)
{
    if ((NULL == devHandle) || (NULL == info))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);
    if (NULL == device)
        return errCode;

    MicThermalInfo thermal;
    errCode = device->getThermalInfo(&thermal);

    if (true == MicDeviceError::isError(errCode))
        return errCode;

    if(device->deviceType() == X200_TYPE_STR)
        return MICSDKERR_NOT_SUPPORTED;

    // Fill up the Throttle Info.
    ThrottleInfo throttleInfo = thermal.throttleInfo();
    info->duration   = throttleInfo.duration();
    info->totalTime  = throttleInfo.totalTime();
    info->eventCount = throttleInfo.eventCount();
    info->active     = throttleInfo.isActive() ? ENABLED : DISABLED;

    return errCode;
}

/**
 * @fn     uint32_t mic_get_power_throttle_info(MIC *device, MIC_THROTTLE_INFO *info)
 *
 * @brief  Retrieves the power throttling information of the specified coprocessor.
 *
 * @param  device A valid handle to an open device.
 * @param  info   A pointer to the MIC_THROTTLE_INFO structure that receives the throttle information.
 *
 * If the function succeeds, the power throttle information data is contained in the buffer
 * pointed to by the \b MIC_THROTTLE_INFO \b *info parameter. The throttle information is only
 * available when the device is online. \b MIC_THROTTLE_INFO structure contains the following
 * members:\n\n
 * \a duration :\n
 *    Represents the current power throttling duration in milliseconds.
 *
 * \a totalTime :\n
 *    Represents the total time spent in power throttling events in milliseconds.
 *
 * \a eventCount :\n
 *    Represents the total count of power throttling events since device boot.
 *
 * \a active :\n
 *    A flag to indicate if the device is currently power throttling. A non-zero
 *    is set for the \b active variable if the device is throttling, otherwise it is zero.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_DEVICE_NOT_ONLINE
 * - MICSDKERR_DEVICE_IO_ERROR
 *
 */
uint32_t mic_get_power_throttle_info(MIC *devHandle, MIC_THROTTLE_INFO *info)
{
    if ((NULL == devHandle) || (NULL == info))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);
    if (NULL == device)
        return errCode;

    MicPowerUsageInfo powerUsage;
    errCode = device->getPowerUsageInfo(&powerUsage);

    if (true == MicDeviceError::isError(errCode))
        return errCode;

    if(device->deviceType() == X200_TYPE_STR)
        return MICSDKERR_NOT_SUPPORTED;

    // Fill up the power throttle info.
    ThrottleInfo throttleInfo = powerUsage.throttleInfo();
    info->duration   = throttleInfo.duration();
    info->totalTime  = throttleInfo.totalTime();
    info->eventCount = throttleInfo.eventCount();
    info->active     = throttleInfo.isActive() ? ENABLED : DISABLED;

    return errCode;
}

/*! @} */

/* coprocessor OS power management config */

/*! \addtogroup Coprocessor_OS_Power_Management_Configuration Coprocessor OS Power Management Config.
 * NONE.
 * @{
 * @anchor Group15
 */

/**
 * @fn     uint32_t mic_get_cpufreq_mode(MIC *device, int *mode)
 *
 * @brief  Retrieves the current "cpufreq" (P state) power state of the coprocessor.
 *
 * @param  device  A valid handle to an open device.
 *
 * @param  mode    A pointer to the variable to receive the CPU freq power state.
 *
 * This function is supported only in Intel® Xeon Phi™ x100 product family.
 * For Intel® Xeon Phi™ x200 product family the function returns a 'Operation not supported' error.
 *
 * The power states are only available when the device is online.
 * A non-zero value returned in the \b mode argument indicates that the "cpufreq" power management feature
 * is enabled; a zero value indicates that it is disabled.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_DEVICE_NOT_ONLINE
 * - MICSDKERR_INTERNAL_ERROR
 * - MICSDKERR_NOT_SUPPORTED
 *
 */
uint32_t mic_get_cpufreq_mode(MIC *devHandle, int *mode)
{
    if ((NULL == devHandle) || (NULL == mode))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);
    if (NULL == device)
        return errCode;

    list<MicPowerState> states;
    errCode = device->getPowerStates(&states);

    if (true == MicDeviceError::isError(errCode))
        return errCode;

    list<MicPowerState>::const_iterator itr;
    for(itr=states.begin(); itr!=states.end(); ++itr) {
        if (MicPowerState::eCpuFreq == itr->state()) {
            *mode = itr->isEnabled() ? ENABLED : DISABLED;
            break;
        }
    }
    return errCode;
}

/**
 * @fn     uint32_t mic_get_corec6_mode(MIC *device, int *mode)
 *
 * @brief  Retrieves the current "corec6" (Core C6 state) power state of the coprocessor.
 *
 * @param  device  A valid handle to an open device.
 *
 * @param  mode    A pointer to the variable to receive the "corec6" power state.
 *
 * This function is supported only in Intel® Xeon Phi™ x100 product family.
 * For Intel® Xeon Phi™ x200 product family the function returns a 'Operation not supported' error.
 *
 * The power states are only available when the device is online.
 * A non-zero value returned in the \b mode argument indicates that the "corec6" power management feature
 * is enabled; a zero value indicates that it is disabled.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_DEVICE_NOT_ONLINE
 * - MICSDKERR_INTERNAL_ERROR
 * - MICSDKERR_NOT_SUPPORTED
 *
 */
uint32_t mic_get_corec6_mode(MIC *devHandle, int *mode)
{
    if ((NULL == devHandle) || (NULL == mode))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);
    if (NULL == device)
        return errCode;

    list<MicPowerState> states;
    errCode = device->getPowerStates(&states);

    if (true == MicDeviceError::isError(errCode))
        return errCode;

    list<MicPowerState>::const_iterator itr;
    for(itr=states.begin(); itr!=states.end(); ++itr) {
        if (MicPowerState::eCoreC6 == itr->state()) {
            *mode = itr->isEnabled() ? ENABLED : DISABLED;
            break;
        }
    }
    return errCode;
}

/**
 * @fn     uint32_t mic_get_pc3_mode(MIC *device, int *mode)
 *
 * @brief  Retrieves the current "pc3" (Package C3 state) power state of the coprocessor.
 *
 * @param  device  A valid handle to an open device.
 *
 * @param  mode    A pointer to the variable to receive the "pc3" power state.
 *
 * This function is supported only in Intel® Xeon Phi™ x100 product family.
 * For Intel® Xeon Phi™ x200 product family the function returns a 'Operation not supported' error.
 *
 * The power states are only available when the device is online.
 * A non-zero value returned in the \b mode argument indicates that the "pc3" power management feature is enabled; a zero value
 * indicates disabled state.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_DEVICE_NOT_ONLINE
 * - MICSDKERR_INTERNAL_ERROR
 *
 */
uint32_t mic_get_pc3_mode(MIC *devHandle, int *mode)
{
    if ((NULL == devHandle) || (NULL == mode))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);
    if (NULL == device)
        return errCode;

    list<MicPowerState> states;
    errCode = device->getPowerStates(&states);

    if (true == MicDeviceError::isError(errCode))
        return errCode;

    list<MicPowerState>::const_iterator itr;
    for(itr=states.begin(); itr!=states.end(); ++itr) {
        if (MicPowerState::ePc3 == itr->state()) {
            *mode = itr->isEnabled() ? ENABLED : DISABLED;
            break;
        }
    }
    return errCode;
}

/**
 * @fn     uint32_t mic_get_pc6_mode(MIC *device, int *mode)
 *
 * @brief  Retrieves the current "pc6" (Package C6 state) power state of the coprocessor.
 *
 * @param  device  A valid handle to an open device.
 *
 * @param  mode    A pointer to the variable to receive the "pc6" power state.
 *
 * This function is supported only in Intel® Xeon Phi™ x100 product family.
 * For Intel® Xeon Phi™ x200 product family the function returns a 'Operation not supported' error.
 *
 * The power states are only available when the device is online.
 * A non-zero value returned in the \b mode argument indicates that the "pc6" power management feature
 * is enabled; a zero value indicates that it is disabled.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_DEVICE_NOT_ONLINE
 * - MICSDKERR_INTERNAL_ERROR
 * - MICSDKERR_NOT_SUPPORTED
 *
 */
uint32_t mic_get_pc6_mode(MIC *devHandle, int *mode)
{
    if ((NULL == devHandle) || (NULL == mode))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);
    if (NULL == device)
        return errCode;

    list<MicPowerState> states;
    errCode = device->getPowerStates(&states);

    if (true == MicDeviceError::isError(errCode))
        return errCode;

    list<MicPowerState>::const_iterator itr;
    for(itr=states.begin(); itr!=states.end(); ++itr) {
        if (MicPowerState::ePc6 == itr->state()) {
            *mode = itr->isEnabled() ? ENABLED : DISABLED;
            break;
        }
    }
    return errCode;
}

/*! @} */

/* Thermal info */

/*! \addtogroup Thermal_Information Thermal Information.
 * NONE.
 * @{
 * @anchor Group16
 */

/**
 * @fn     uint32_t mic_get_thermal_sensor_list(MIC *device, char ***sensor_names, size_t *num_sensors)
 *
 * @brief  Retrieves a list of thermal sensor names and their count present on the coprocessor board.
 *
 * @param  device        A valid handle to an open device.
 * @param  sensor_names  A pointer to an array of strings to receive the temperature sensor names.
 * @param  num_sensors   A pointer to the variable to receive the sensors count.
 *
 * If the function succeeds, the \b sensor_names argument will contain the list of temperature sensors present
 * on the coprocessor board. Note that this list is dynamically allocated and, once no longer needed
 * should be freed (including the memory allocated for the individual sensor names in the list) by calling
 * the \b mic_free_sensor_list() function.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_DEVICE_NOT_ONLINE
 * - MICSDKERR_DEVICE_IO_ERROR
 *
 */
uint32_t mic_get_thermal_sensor_list(MIC *devHandle, char ***sensor_names, size_t *num_sensors)
{
    if ((NULL == devHandle) || (NULL == sensor_names) || (NULL == num_sensors))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);
    if (NULL == device)
        return errCode;

    MicThermalInfo thermalInfo;
    errCode = device->getThermalInfo(&thermalInfo);

    if (true == MicDeviceError::isError(errCode))
        return errCode;

    *num_sensors = thermalInfo.sensorCount();

    char **names = (char**)malloc((*num_sensors) * sizeof(char*));

    if (NULL == names) {
        errCode = MicDeviceError::errorCode( MICSDKERR_NO_MEMORY );
        return errCode;
    }

    for (size_t idx=0; idx < *num_sensors; ++idx) {
        string nameStr = thermalInfo.sensorValueAt(idx).name();
        names[idx] = NULL; // Initialize heap.

        if (false == nameStr.empty()) {
            char *name = (char*)malloc(nameStr.length() + 1); // Caller has to free this memory.
            if (NULL == name) {
               errCode = MicDeviceError::errorCode( MICSDKERR_NO_MEMORY );
               break;
            }
            strcpy(name, nameStr.c_str()); // Guaranteed null terminated copy
            names[idx] = name;
        }
    }
    // Free any allocated memory if insufficient memory condition is encountered.
    if (errCode == MicDeviceError::errorCode( MICSDKERR_NO_MEMORY )) {
        for (size_t idx=0; idx < *num_sensors; ++idx) {
            if (NULL != names[idx])
                free(names[idx]);
            names[idx] = NULL;
        }
        free(names);
        names = NULL;
    }
    *sensor_names = names; // assign and return
    return errCode;
}

/**
 * @fn     uint32_t mic_get_thermal_version_by_name(MIC *device, const char *sensor_name, uint32_t *sensor_value)
 *
 * @brief  Retrieves the temperature value in Celsius for the sensor with the specified name.
 *
 * @param  device        A valid handle to an open device.
 * @param  sensor_name   The name of the temperature sensor to retrieve.
 * @param  sensor_value  A pointer to the variable to receive the sensor value.
 *
 * A list of the available thermal sensor names can be obtained by calling \b mic_get_thermal_sensor_list()
 * function.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_DEVICE_NOT_ONLINE
 * - MICSDKERR_DEVICE_IO_ERROR
 *
 */
uint32_t mic_get_thermal_version_by_name(MIC *devHandle, const char *sensor_name, uint32_t *sensor_value)
{
    if ((NULL == devHandle) || (NULL == sensor_name) || (NULL == sensor_value))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);
    if (NULL == device)
        return errCode;

    MicThermalInfo thermalInfo;
    errCode = device->getThermalInfo(&thermalInfo);

    if (true == MicDeviceError::isError(errCode))
        return errCode;

    std::vector<MicTemperature> sensors = thermalInfo.sensors();
    auto sensor = sensors.begin(); // iterator needed outside of for loop scope
    for ( ; sensor != sensors.end(); ++sensor){
        string nameStr = sensor->name();
        if (0 == nameStr.compare(sensor_name)) // sensor name match
        {
            *sensor_value = static_cast<uint32_t>(sensor->celsius());
            break;
        }
    }

    if (sensor == sensors.end())
        // couldn't find the requested sensor
        errCode = MICSDKERR_INVALID_ARG;

    return errCode;
}

/**
 * @fn     uint32_t mic_get_thermal_version_by_index(MIC *device, int version_index, uint32_t *sensor_value)
 *
 * @brief  Retrieves the temperature value in Celsius for the sensor at the specified index.
 *
 * @param  device         A valid handle to an open device.
 * @param  version_index  A valid index to the thermal sensor list.
 * @param  sensor_value   A pointer to the variable to receive the sensor value.
 *
 * A list of the available thermal sensors can be obtained by calling \b mic_get_thermal_sensor_list()
 * function.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_DEVICE_NOT_ONLINE
 * - MICSDKERR_DEVICE_IO_ERROR
 *
 */
uint32_t mic_get_thermal_version_by_index(MIC *devHandle, int version_index, uint32_t *sensor_value)
{
    if ((NULL == devHandle) || (NULL == sensor_value))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);
    if (NULL == device)
        return errCode;

    MicThermalInfo thermalInfo;
    errCode = device->getThermalInfo(&thermalInfo);

    if (true == MicDeviceError::isError(errCode))
        return errCode;

    int count = (int)thermalInfo.sensorCount();

    if (version_index < 0 || version_index >= count)
        return MICSDKERR_INVALID_ARG;

    for (int idx=0; idx < count; ++idx) {
        if (version_index == idx) // sensor index match
            *sensor_value = static_cast<uint32_t>(thermalInfo.sensorValueAt(idx).celsius());
    }
    return errCode;
}

/*! @} */

/* Power utilization info */

/*! \addtogroup Power_Utilization_Information Power Utilization Information.
 * NONE.
 * @{
 * @anchor Group17
 */

/**
 * @fn     uint32_t mic_get_power_sensor_list(MIC *device, char ***sensor_names, size_t *num_sensors)
 *
 * @brief  Retrieves a list of power sensor names and their count present on the coprocessor board.
 *
 * @param  device        A valid handle to an open device.
 * @param  sensor_names  A pointer to an array of strings to receive the power sensor names.
 * @param  num_sensors   A pointer to the variable to receive the sensors count.
 *
 * If the function succeeds, the \b sensor_names argument will contain the list of power sensors present
 * on the coprocessor board. Note that this list is dynamically allocated and, once no longer needed
 * should be freed (including the memory allocated for the individual sensor name in the list) by calling
 * the \b mic_free_sensor_list() function.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_DEVICE_NOT_ONLINE
 * - MICSDKERR_DEVICE_IO_ERROR
 *
 */
uint32_t mic_get_power_sensor_list(MIC *devHandle, char ***sensor_names, size_t *num_sensors)
{
    if ((NULL == devHandle) || (NULL == sensor_names) || (NULL == num_sensors))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);
    if (NULL == device)
        return errCode;

    MicPowerUsageInfo powerInfo;;
    errCode = device->getPowerUsageInfo(&powerInfo);

    if (true == MicDeviceError::isError(errCode))
        return errCode;

    *num_sensors = powerInfo.sensorCount();

    char **names = (char**)malloc((*num_sensors) * sizeof(char*));

    if (NULL == names) {
        errCode = MicDeviceError::errorCode( MICSDKERR_NO_MEMORY );
        return errCode;
    }

    for (size_t idx=0; idx < *num_sensors; ++idx) {
        string nameStr = powerInfo.sensorValueAt(idx).name();
        names[idx] = NULL; // Initialize heap.

        if (false == nameStr.empty()) {
            char *name = (char*)malloc(nameStr.length() + 1);
            if (NULL == name) {
               errCode = MicDeviceError::errorCode( MICSDKERR_NO_MEMORY );
               break;
            }
            strcpy(name, nameStr.c_str());
            names[idx] = name;
        }
    }

    // Free any allocated memory if insufficient memory condition is encountered.
    if (errCode == MicDeviceError::errorCode( MICSDKERR_NO_MEMORY )) {
        for (size_t idx=0; idx < *num_sensors; ++idx) {
            if (NULL != names[idx])
                free(names[idx]);
            names[idx] = NULL;
        }
        free(names);
        names = NULL;
    }
    *sensor_names = names; //assign and return
    return errCode;
}

/**
 * @fn     uint32_t mic_get_power_sensor_value_by_name(MIC *device,  const char *sensor_name, double *value, int exponent)
 *
 * @brief  Retrieves the value for the power sensor at the specified index.
 *
 * @param  device        A valid handle to an open device.
 * @param  sensor_name   The name of the power sensor to retrieve.
 * @param  value         A pointer to the variable to receive the sensor value.
 * @param  exponent      A unit that can be used to scale the sensor value.
 *
 * Valid values for the \b exponent argument are:\n
 * 9  - for Giga Units.\n
 * 6  - for Mega Units.\n
 * 3  - for Kilo Units.\n
 * 0  - for Base Units.\n
 * -3 - for Milli Units.\n
 * -6 - for Micro Units.\n
 * -9 - for Nano Units.\n\n
 *
 * A list of the available thermal sensors can be obtained by calling \b mic_get_power_sensor_list()
 * function.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_DEVICE_NOT_ONLINE
 * - MICSDKERR_DEVICE_IO_ERROR
 *
 */
uint32_t mic_get_power_sensor_value_by_name(MIC *devHandle, const char *sensor_name, double *value, int exponent)
{
    if ((NULL == devHandle) || (NULL == sensor_name) || (NULL == value) || (!isExpValid(exponent)))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);
    if (NULL == device)
        return errCode;

    MicPowerUsageInfo powerInfo;
    errCode = device->getPowerUsageInfo(&powerInfo);

    if (true == MicDeviceError::isError(errCode))
        return errCode;

    std::vector<MicPower> sensors = powerInfo.sensors();
    auto sensor = sensors.begin(); // iterator required outside of for loop scope

    for ( ; sensor != sensors.end(); sensor++){
        string nameStr = sensor->name();
        if (0 == nameStr.compare(sensor_name))
        {
            *value = sensor->scaledValue(exponent);
            break;
        }
    }

    if (sensor == sensors.end())
        errCode = MICSDKERR_INVALID_ARG;

    return errCode;
}

/**
 * @fn     uint32_t mic_get_power_sensor_value_by_index(MIC *device, int sensor_index, double *value, int exponent)
 *
 * @brief  Retrieves the value for the power sensor at the specified index.
 *
 * @param  device        A valid handle to an open device.
 * @param  sensor_index  A valid index to the power sensor list.
 * @param  value         A pointer to the variable to receive the sensor value.
 * @param  exponent      A unit that can be used to scale the sensor value.
 *
 * Valid values for the \b exponent argument are:\n
 * 9  - for Giga Units.\n
 * 6  - for Mega Units.\n
 * 3  - for Kilo Units.\n
 * 0  - for Base Units.\n
 * -3 - for Milli Units.\n
 * -6 - for Micro Units.\n
 * -9 - for Nano Units.\n\n
 *
 * A list of the available thermal sensors can be obtained by calling \b mic_get_power_sensor_list()
 * function.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_DEVICE_NOT_ONLINE
 * - MICSDKERR_DEVICE_IO_ERROR
 *
 */
uint32_t mic_get_power_sensor_value_by_index(MIC *devHandle, int sensor_index, double *value, int exponent)
{
    if ((NULL == devHandle) || (NULL == value) || (!isExpValid(exponent)))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);
    if (NULL == device)
        return errCode;

    MicPowerUsageInfo powerInfo;
    errCode = device->getPowerUsageInfo(&powerInfo);

    if (true == MicDeviceError::isError(errCode))
        return errCode;

    int count = (int)powerInfo.sensorCount();

    if (sensor_index < 0 || sensor_index >= count)
        return MICSDKERR_INVALID_ARG;

    for (int idx=0; idx < count; ++idx) {
        if (sensor_index == idx) // sensor index match
            *value = powerInfo.sensorValueAt(idx).scaledValue(exponent);
    }

    return errCode;
}

/*! @} */

/* Memory utilization */

/*! \addtogroup Memory_Utilization_Information Memory Utilization Information.
 * NONE.
 * @{
 * @anchor Group18
 */

/**
 * @fn     uint32_t mic_get_memory_utilization_info(MIC *device, MIC_MEMORY_UTIL_INFO *mem_util)
 *
 * @brief  Retrieves the Memory utilization information of the specified coprocessor.
 *
 * @param  device    A valid handle to an open device.
 * @param  mem_util  A pointer to the MIC_MEMORY_UTIL_INFO structure that receives the memory usage information.
 *
 * If the function succeeds, the memory utilization information data is contained in the buffer
 * pointed to by the \b MIC_MEMORY_UTIL_INFO \b *mem_util parameter. The memory usage information is only
 * available when the device is online. \b MIC_MEMORY_UTIL_INFO structure contains the following
 * members:\n\n
 * \a memory_free :\n
 *    Represents the total amount of free memory in kilobytes.
 *
 * \a memory_inuse :\n
 *    Represents the total amount of memory that is in active use in kilobytes.
 *
 * \a memory_buffers :\n
 *    Represents the amount of memory available for kernel buffers in kilobytes.
 *
 * \a memory_available :\n
 *    Represents the total memory size in kilobytes.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_DEVICE_NOT_ONLINE
 * - MICSDKERR_DEVICE_IO_ERROR
 *
 */
uint32_t mic_get_memory_utilization_info(MIC *devHandle, MIC_MEMORY_UTIL_INFO *mem_util)
{
    if ((NULL == devHandle) || (NULL == mem_util))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);
    if (NULL == device)
        return errCode;

    MicMemoryUsageInfo memUse;
    errCode = device->getMemoryUsageInfo(&memUse);

    if (false == MicDeviceError::isError(errCode)) {
        mem_util->memory_free      = static_cast<uint32_t>(memUse.free().scaledValue(MicMemory::eKilo));
        mem_util->memory_inuse     = static_cast<uint32_t>(memUse.used().scaledValue(MicMemory::eKilo));
        mem_util->memory_buffers   = static_cast<uint32_t>(memUse.buffers().scaledValue(MicMemory::eKilo));
        mem_util->memory_available = static_cast<uint32_t>(memUse.total().scaledValue(MicMemory::eKilo));
    }
    return errCode;
}

/*! @} */

/* Power limits */

/*! \addtogroup Power_Limits Power Limits.
 * NONE.
 * @{
 * @anchor Group19
 */

/**
 * @fn     uint32_t mic_get_power_phys_limit(MIC *device, uint32_t *phys_lim)
 *
 * @brief  Retrieves the physical power limit of the coprocessor in microwatts.
 *         Note: This is not supported in Intel® Xeon Phi™ coprocessor x200.
 *
 * @param  device    A valid handle to an open device.
 * @param  phys_lim  A pointer to the variable to receive the physical power limit in microwatts.
 *
 * The power threshold information is only available when the device is online.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_DEVICE_NOT_ONLINE
 * - MICSDKERR_DEVICE_IO_ERROR
 *
 */
uint32_t mic_get_power_phys_limit(MIC *devHandle, uint32_t *phys_lim)
{
    if ((NULL == devHandle) || (NULL == phys_lim))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);
    if (NULL == device)
        return errCode;

    MicPowerThresholdInfo powerThreshold;
    errCode = device->getPowerThresholdInfo(&powerThreshold);

    if (true == MicDeviceError::isError(errCode))
        return errCode;

    if(device->deviceType() == X200_TYPE_STR)
        return MICSDKERR_NOT_SUPPORTED;

    *phys_lim = static_cast<uint32_t>(powerThreshold.maximum().scaledValue(MicPower::eMicro));

    return errCode;
}

/**
 * @fn     uint32_t mic_get_power_hmrk(MIC *device, uint32_t *hmrk)
 *
 * @brief  Retrieves the highest instantaneous power threshold setting of the coprocessor in microwatts.
 *
 * @param  device  A valid handle to an open device.
 * @param  hmrk    A pointer to the variable to receive the high power threshold setting in microwatts.
 *
 * The power threshold information is only available when the device is online.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_DEVICE_NOT_ONLINE
 * - MICSDKERR_DEVICE_IO_ERROR
 *
 */
uint32_t mic_get_power_hmrk(MIC *devHandle, uint32_t *hmrk)
{
    if ((NULL == devHandle) || (NULL == hmrk))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);
    if (NULL == device)
        return errCode;

    MicPowerThresholdInfo powerThreshold;
    errCode = device->getPowerThresholdInfo(&powerThreshold);

    if (false == MicDeviceError::isError(errCode))
        *hmrk = static_cast<uint32_t>(powerThreshold.hiThreshold().scaledValue(MicPower::eMicro));

    return errCode;
}

/**
 * @fn     uint32_t mic_get_power_lmrk(MIC *device, uint32_t *lmrk)
 *
 * @brief  Retrieves the lowest instantaneous power threshold setting of the coprocessor in microwatts.
 *
 * @param  device  A valid handle to an open device.
 * @param  lmrk    A pointer to the variable to receive the low power threshold setting in microwatts.
 *
 * The power threshold information is only available when the device is online.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_DEVICE_NOT_ONLINE
 * - MICSDKERR_DEVICE_IO_ERROR
 *
 */
uint32_t mic_get_power_lmrk(MIC *devHandle, uint32_t *lmrk)
{
    if ((NULL == devHandle) || (NULL == lmrk))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);
    if (NULL == device)
        return errCode;

    MicPowerThresholdInfo powerThreshold;
    errCode = device->getPowerThresholdInfo(&powerThreshold);

    if (false == MicDeviceError::isError(errCode))
        *lmrk = static_cast<uint32_t>(powerThreshold.loThreshold().scaledValue(MicPower::eMicro));

    return errCode;
}

/**
 * @fn     uint32_t mic_get_time_window0(MIC *device, uint32_t *time_window)
 *
 * @brief  Retrieves the time window level 0 in microseconds.
 *
 * @param  device       A valid handle to an open device.
 * @param  time_window  A pointer to the variable to receive the window 0 time in microseconds.
 *
 * The time window duration is only available when the device is online.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_DEVICE_NOT_ONLINE
 * - MICSDKERR_DEVICE_IO_ERROR
 *
 */
uint32_t mic_get_time_window0(MIC *devHandle, uint32_t *time_window)
{
    if ((NULL == devHandle) || (NULL == time_window))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);
    if (NULL == device)
        return errCode;

    MicPowerThresholdInfo powerThreshold;
    errCode = device->getPowerThresholdInfo(&powerThreshold);

    if (false == MicDeviceError::isError(errCode))
        *time_window = powerThreshold.window0Time();

    return errCode;
}

/**
 * @fn     uint32_t mic_get_time_window1(MIC *device, uint32_t *time_window)
 *
 * @brief  Retrieves the time window level 1 in microseconds.
 *
 * @param  device       A valid handle to an open device.
 * @param  time_window  A pointer to the variable to receive the window 1 time in microseconds.
 *
 * The time window duration is only available when the device is online.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_DEVICE_NOT_ONLINE
 * - MICSDKERR_DEVICE_IO_ERROR
 *
 */
uint32_t mic_get_time_window1(MIC *devHandle, uint32_t *time_window)
{
    if ((NULL == devHandle) || (NULL == time_window))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);
    if (NULL == device)
        return errCode;

    MicPowerThresholdInfo powerThreshold;
    errCode = device->getPowerThresholdInfo(&powerThreshold);

    if (false == MicDeviceError::isError(errCode))
        *time_window = powerThreshold.window1Time();

    return errCode;
}

/**
 * @fn     uint32_t mic_set_power_limit0(MIC *device, uint32_t power, uint32_t time_window)
 *
 * @brief  Set power limit 0 for the coprocessor.
 *
 * @param  device       A valid handle to an open device.
 * @param  power        The power limit 0 (hmrk) to set for the coprocessor in microwatts.
 * @param  time_window  The time window 0 to set for the coprocessor in microseconds.
 *
 * It is recommended that power limit 0 be less than power limit 1, and time
 * window 0 be greater than time window 1.
 *
 * Note: Please refer to document "Intel® 64 and IA-32 Architectures Software Developer Manual"
 *       for valid values for the power limits.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_DEVICE_NOT_ONLINE
 * - MICSDKERR_INTERNAL_ERROR
 *
 */
uint32_t mic_set_power_limit0(MIC *devHandle, uint32_t power, uint32_t time_window)
{
    if (NULL == devHandle)
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);

    if (NULL != device)
        errCode = device->setPowerThreshold(MicDevice::PowerWindow::eWindow0, power, time_window);

    return errCode;
}

/**
 * @fn     uint32_t mic_set_power_limit1(MIC *device, uint32_t power, uint32_t time_window)
 *
 * @brief  Set power limit 1 for the coprocessor.
 *
 * @param  device       A valid handle to an open device.
 * @param  power        The power limit 1 (lmrk) to set for the coprocessor in microwatts.
 * @param  time_window  The time window 1 to set for the coprocessor im microseconds.
 *
 * It is recommended that power limit 1 be greater than power limit 0, and time
 * window 1 be less than time window 0.
 *
 * Note: Please refer to document "Intel® 64 and IA-32 Architectures Software Developer Manual"
 *       for valid values for the power limits.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_DEVICE_NOT_ONLINE
 * - MICSDKERR_INTERNAL_ERROR
 *
 */
uint32_t mic_set_power_limit1(MIC *devHandle, uint32_t power, uint32_t time_window)
{
    if (NULL == devHandle)
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);

    if (NULL != device)
        errCode = device->setPowerThreshold(MicDevice::PowerWindow::eWindow1, power, time_window);

    return errCode;
}

/*! @} */

/* Core utilization */

/*! \addtogroup Core_Utilization_Information Core Utilization Information.
 * NONE.
 * @{
 * @anchor Group20
 */

/**
 * @fn     uint32_t mic_update_core_util(MIC *device, MIC_CORE_UTIL_INFO *core_util)
 *
 * @brief  Retrieves the core utilization information of the specified coprocessor.
 *
 * @param  device     A valid handle to an open device.
 * @param  core_util  A pointer to the MIC_CORE_UTIL_INFO structure that receives the core usage information.
 *
 * If the function succeeds, the core utilization information data is contained in the buffer
 * pointed to by the \b MIC_CORE_UTIL_INFO \b *core_util parameter. The core usage information is only
 * available when the device is online. \b MIC_CORE_UTIL_INFO structure contains the following
 * members:\n\n
 * \a tick_count :\n
 *    Represents the ticks per second of the coprocessor. It is for measuring internal system time.
 *
 * \a jiffy_count :\n
 *    Represents the usage value in number of counter ticks.
 *
 * \a idle_sum :\n
 *    Represents the total sum of idle time on all threads.
 *
 * \a nice_sum :\n
 *    Represents the total sum of time spent by processes running at reduced priority on all threads.
 *
 * \a sys_sum :\n
 *    Represents the aggregate sum of time spent in system (kernel) mode on all threads.
 *
 * \a user_sum :\n
 *    Represents the aggregate sum of time spent in user mode on all threads.
 *
 * \a counters :\n
 *    Represents a counters array where every element, representing a separate counter, will point to
 *    an integer array of size equal to the number of cores in the coprocessor. The cores array will store
 *    the usage counter values for each active core.
 *    The counters array indices 1, 2, 3, and 4 represent User, System, Nice and Idle counters.
 *    NOTE: The integer array is dynamically allocated by the library and, once no longer needed, should be freed calling Linux/Windows free() function.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_DEVICE_NOT_ONLINE
 * - MICSDKERR_DEVICE_IO_ERROR
 *
 */
uint32_t mic_update_core_util(MIC *devHandle, MIC_CORE_UTIL_INFO *core_util)
{
    if ((NULL == devHandle) || (NULL == core_util))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);

    if (NULL == device)
        return errCode;

    MicCoreUsageInfo coreUsage;
    errCode = device->getCoreUsageInfo(&coreUsage);

    if (true == MicDeviceError::isError(errCode))
        return errCode;

    core_util->core_count = coreUsage.coreCount();
    core_util->core_threadcount = coreUsage.coreThreadCount();
    size_t nthreads = core_util->core_count * core_util->core_threadcount;

    // Construct array of size #threads for each counter.
    for (size_t idx = MicCoreUsageInfo::eUser; idx <= MicCoreUsageInfo::eIdle; ++idx) {
        uint64_t *threads = (uint64_t*) malloc(nthreads * sizeof(uint64_t));
        if (NULL == threads) {
            errCode = MicDeviceError::errorCode( MICSDKERR_NO_MEMORY );
            break;
        }
        core_util->counters[idx] = threads;
    }

    // In case of an error,free up any allocated memory.
    if (errCode == MicDeviceError::errorCode( MICSDKERR_NO_MEMORY )) {
        for (size_t idx = MicCoreUsageInfo::eUser; idx <= MicCoreUsageInfo::eIdle; ++idx) {
            if (NULL != core_util->counters[idx])
                free(core_util->counters[idx]);
            core_util->counters[idx] = NULL;
        }
        return errCode;
    }
    // Fill up the usage count.
    for (size_t type  = MicCoreUsageInfo::eUser; type <= MicCoreUsageInfo::eIdle; ++type) {
        for (size_t threads_idx=0; threads_idx < nthreads; ++threads_idx)
            core_util->counters[type][threads_idx] = coreUsage.counterValue(static_cast<MicCoreUsageInfo::Counter>(type), threads_idx);
    }
    core_util->idle_sum         = coreUsage.counterTotal(MicCoreUsageInfo::eIdle);
    core_util->nice_sum         = coreUsage.counterTotal(MicCoreUsageInfo::eNice);
    core_util->sys_sum          = coreUsage.counterTotal(MicCoreUsageInfo::eSystem);
    core_util->user_sum         = coreUsage.counterTotal(MicCoreUsageInfo::eUser);
    core_util->jiffy_count      = coreUsage.tickCount();
    core_util->tick_count       = coreUsage.ticksPerSecond();

    return errCode;
}

/*! @} */

/**
 * @ingroup Coprocessor_core_Information
 * @fn     uint32_t mic_get_tick_count(MIC *device, uint64_t *tick_count)
 *
 * @brief  Retrieves the ticks per second of the coprocessor.
 *
 * @param  device       A valid handle to an open device.
 * @param  tick_count   A pointer to the variable to receive the tick count.
 *
 * The \b tick value is used to measure the internal system time.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_DEVICE_NOT_ONLINE
 * - MICSDKERR_DEVICE_IO_ERROR
 *
 */
uint32_t mic_get_tick_count(MIC *devHandle, uint64_t *tick_count)
{
    if ((NULL == devHandle) || (NULL == tick_count))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);

    if (NULL == device)
        return errCode;

    MicCoreUsageInfo coreUsage;
    errCode = device->getCoreUsageInfo(&coreUsage);

    if (false == MicDeviceError::isError(errCode))
        *tick_count = coreUsage.ticksPerSecond();

    return errCode;
}
/**
 * @ingroup Coprocessor_core_Information
 * @fn     uint32_t mic_get_jiffy_counter(MIC *device, uint64_t *jiffy)
 *
 * @brief  Retrieves the jiffy count of the coprocessor.
 *
 * @param  device  A valid handle to an open device.
 * @param  jiffy   A pointer to the variable to receive the jiffy count.
 *
 * The \b jiffy is the usage value in number of counter ticks.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_DEVICE_NOT_ONLINE
 * - MICSDKERR_DEVICE_IO_ERROR
 *
 */
uint32_t mic_get_jiffy_counter(MIC *devHandle, uint64_t *jiffy)
{
    if ((NULL == devHandle) || (NULL == jiffy))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    *jiffy = 0;
    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);

    if (NULL == device)
        return errCode;

    MicCoreUsageInfo coreUsage;
    errCode = device->getCoreUsageInfo(&coreUsage);

    if (false == MicDeviceError::isError(errCode))
        *jiffy = coreUsage.tickCount();

    return errCode;
}

/**
 * @ingroup Coprocessor_core_Information
 * @fn     uint32_t mic_get_threads_core(MIC *device, size_t *threads_core)
 *
 * @brief  Retrieves the number of threads per core.
 *
 * @param  device		A valid handle to an open device.
 * @param  threads_core		A pointer to the variable to receive the core thread count.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_DEVICE_NOT_ONLINE
 * - MICSDKERR_DEVICE_IO_ERROR
 *
 */
uint32_t mic_get_threads_core(MIC *devHandle, size_t *threads_core)
{
    if ((NULL == devHandle) || (NULL == threads_core))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);

    if (NULL == device)
        return errCode;

    MicCoreUsageInfo coreUsage;
    errCode = device->getCoreUsageInfo(&coreUsage);

    if (false == MicDeviceError::isError(errCode))
        *threads_core = coreUsage.coreThreadCount();

    return errCode;
}

/**
 * @ingroup Coprocessor_core_Information
 * @fn     uint32_t mic_get_num_cores(MIC *device, size_t *num_cores)
 *
 * @brief  Retrieves the number of available cores.
 *
 * @param  device      A valid handle to an open device.
 * @param  num_cores   A pointer to the variable to receive the cores count.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 * - MICSDKERR_NO_MPSS_STACK
 * - MICSDKERR_DRIVER_NOT_LOADED
 * - MICSDKERR_DRIVER_NOT_INITIALIZED
 * - MICSDKERR_NO_DEVICES
 * - MICSDKERR_NO_MEMORY
 * - MICSDKERR_DEVICE_NOT_OPEN
 * - MICSDKERR_DEVICE_NOT_ONLINE
 * - MICSDKERR_DEVICE_IO_ERROR
 *
 */
uint32_t mic_get_num_cores(MIC *devHandle, size_t *num_cores)
{
    if ((NULL == devHandle) || (NULL == num_cores))
        return MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    uint32_t errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    MicDevice *device = getOpenMicDev(devHandle, &errCode);

    if (NULL == device)
        return errCode;

    MicCoreUsageInfo coreUsage;
    errCode = device->getCoreUsageInfo(&coreUsage);

    if (false == MicDeviceError::isError(errCode))
        *num_cores = coreUsage.coreCount();

    return errCode;
}


/* Free Memory Utilities */

/*! \addtogroup Free_Memory_Utilities Free Memory Utilities.
 * NONE.
 * @{
 * @anchor Group21
 */

/**
 * @fn     uint32_t mic_free_core_util(MIC_CORE_UTIL_INFO *core_util)
 *
 * @brief  Frees memory allocated by calls \b mic_update_core_util().
 *
 * @param  core_util  A pointer to the MIC_CORE_UTIL_INFO structure that receives the core usage information.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 *
 */
uint32_t mic_free_core_util(MIC_CORE_UTIL_INFO *core_util)
{
    if(NULL == core_util)
        return MICSDKERR_INVALID_ARG;

    for (size_t idx = MicCoreUsageInfo::eUser; idx <= MicCoreUsageInfo::eIdle; ++idx) {
        uint64_t *cores = core_util->counters[idx];
        if(cores)
            free(cores);
    }
    return MICSDKERR_SUCCESS;
}

/**
 * @fn     uint32_t mic_free_sensor_list(char **sensor_names, size_t num_sensors)
 *
 * @brief  Frees memory allocated by calls to \b mic_get_power_sensor_list(), \b mic_get_thermal_sensor_list().
 *
 * @param  sensor_names  A pointer to the array of strings to be freed.
 * @param  num_sensors   The number of elements in the array.
 *
 * @return error code
 *
 * On success, MICSDKERR_SUCCESS is returned.\n
 * On failure, one of the following error codes may be returned:
 * - MICSDKERR_INVALID_ARG
 *
 */
uint32_t mic_free_sensor_list(char **sensor_names, size_t num_sensors)
{
    if(NULL == sensor_names || NULL == *sensor_names)
        return MICSDKERR_INVALID_ARG;

    for(size_t sensor = 0; sensor < num_sensors; ++sensor){
		char *p = sensor_names[sensor];
		if(p)
            free(p);
    }

    free(sensor_names);
    return MICSDKERR_SUCCESS;
}


/*! @} */

/*
 * Common private function.
 */
MicDevice *getOpenMicDev(MIC *devHandle, uint32_t *err)
{
    MicDevice *micDev = NULL;

    if ((NULL == devHandle) || (NULL == err))
        return micDev;

    *err = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    if (false == initLib->isInitialized())
        *err = initLib->getLastErrCode();
    else
        micDev = initLib->getOpenDevice(devHandle);

    if ((false == MicDeviceError::isError(*err) && (NULL == micDev)))
        *err = MicDeviceError::errorCode( MICSDKERR_DEVICE_NOT_OPEN );

    return micDev;
}
