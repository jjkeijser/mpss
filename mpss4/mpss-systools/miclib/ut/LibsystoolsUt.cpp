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

#include <libsystools.h>
#include "../src/libsystools_p.hpp"
#include "MicDeviceError.hpp"
#include <gtest/gtest.h>
#ifdef _WIN32
#include <windows.h>
#endif

extern void reset_miclib_instance();

namespace micmgmt
{

    const uint32_t ERR_SUCCESS        = MicDeviceError::errorCode( MICSDKERR_SUCCESS );
    const uint32_t ERR_ALRDY_OPEN     = MicDeviceError::errorCode( MICSDKERR_DEVICE_ALREADY_OPEN );
    const uint32_t ERR_NO_SUCH_DEVICE = MicDeviceError::errorCode( MICSDKERR_NO_SUCH_DEVICE );
    const uint32_t ERR_NOT_SUPPORTED  = MicDeviceError::errorCode( MICSDKERR_NOT_SUPPORTED );
    const uint32_t ERR_NO_DEVICES     = MicDeviceError::errorCode( MICSDKERR_NO_DEVICES );
    const uint32_t ERR_INVALID_ARG    = MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );
    const uint32_t ERR_INTERNAL_ERROR = MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );

    using namespace std;
    TEST(libsystoolsP, TC_KNL_libsystoolsP)
    {
        miclib *libRef = miclib::resetInstance();
        uint32_t errcode = 0;

        string   mock    = "KNL_MOCK_CNT";
        string   mockCnt = "0";
    #ifdef _WIN32
        SetEnvironmentVariableA((LPCSTR)mock.c_str(), (LPCSTR)mockCnt.c_str());
    #else
        setenv( mock.c_str(), mockCnt.c_str(), 1 );
    #endif
        EXPECT_FALSE(libRef->isInitialized());
        errcode = libRef->getLastErrCode();
        ASSERT_EQ( ERR_NO_DEVICES, errcode );
    }

    TEST(libsystoolsP, TC_KNL_libsystoolsP_NegativeTest)
    {
        reset_miclib_instance();
        miclib *libRef = miclib::getInstance();
        string   mock    = "KNL_MOCK_CNT";
        string   mockCnt = "1";
    #ifdef _WIN32
        SetEnvironmentVariableA((LPCSTR)mock.c_str(), (LPCSTR)mockCnt.c_str());
    #else
        setenv( mock.c_str(), mockCnt.c_str(), 1 );
    #endif

        MIC *handle       = NULL;
        void *nullPtr     = NULL;
        uint32_t errcode  = 0;
        int devNum0       = 0;
        int invDevNum     = 3;
        MicDevice *dev    = NULL;

        handle = mic_open_device(devNum0, &errcode);
        ASSERT_EQ(ERR_SUCCESS, errcode);
        //EXPECT_NE(nullptr, handle);  nullptr is not supported on the compiler, "simulating" one to prevent error
        EXPECT_NE((MIC *)nullPtr, handle);

        errcode = libRef->isDeviceOpen(invDevNum);
        EXPECT_EQ(ERR_SUCCESS, errcode);

        errcode = libRef->updateDevContainer(0, NULL, NULL);
        EXPECT_EQ(ERR_INTERNAL_ERROR, errcode);

        dev = libRef->getOpenDevice(NULL, false);
        EXPECT_EQ(NULL, dev);
    }

    TEST(libsystools, TC_KNL_libsystools_NoDevices)
    {
        reset_miclib_instance();
        uint32_t errcode = 0;
        int      device  = 0;
        MIC     *handle  = NULL;
        MicDevice *dev   = NULL;

        string   mock    = "KNL_MOCK_CNT";
        string   mockCnt = "0";

    #ifdef _WIN32
        SetEnvironmentVariableA((LPCSTR)mock.c_str(), (LPCSTR)mockCnt.c_str());
    #else
        setenv( mock.c_str(), mockCnt.c_str(), 1 );
    #endif

        handle = mic_open_device(device, &errcode);
        EXPECT_EQ( ERR_NO_DEVICES, errcode );

        handle = (MIC*)&device;
        errcode = mic_close_device(handle);
        EXPECT_EQ(ERR_NO_DEVICES, errcode);

        dev = getOpenMicDev(handle, &errcode);
        EXPECT_EQ(ERR_NO_DEVICES, errcode);
        EXPECT_EQ(NULL, dev);

        handle = mic_open_device_by_name(mock.c_str(), &errcode);
        EXPECT_EQ(ERR_NO_DEVICES, errcode);
    }

    TEST(libsystools, TC_KNL_libsystools_NegativeTest_001)
    {
        reset_miclib_instance();
        string   mock    = "KNL_MOCK_CNT";
        string   mockCnt = "1";
    #ifdef _WIN32
        SetEnvironmentVariableA((LPCSTR)mock.c_str(), (LPCSTR)mockCnt.c_str());
    #else
        setenv( mock.c_str(), mockCnt.c_str(), 1 );
    #endif

        MIC *handle       = NULL;
        MIC *invHandle    = NULL;
        uint32_t errcode  = 0;
        int devNum0       = 0;
        char devName0[]   = {"mic0"};
        char invDevName[] = {"inv0"};
        char *strArr[]    = {devName0, invDevName};
        char **strPtr     = strArr;
        size_t size       = 0;
        char postCode[]   = {"7d"};
        MicDevice *dev    = NULL;

        handle = mic_open_device_by_name(NULL, NULL);
        EXPECT_EQ(NULL, handle);

        handle = mic_open_device_by_name(NULL, &errcode);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        handle = mic_open_device_by_name(invDevName, &errcode);
        EXPECT_EQ(ERR_NO_SUCH_DEVICE, errcode);

        handle = mic_open_device( devNum0, NULL );
        EXPECT_EQ(NULL, handle);

        handle = mic_open_device(devNum0, &errcode);
        ASSERT_EQ(ERR_SUCCESS, errcode);

        invHandle = mic_open_device_by_name(devName0, &errcode);
        EXPECT_EQ(ERR_ALRDY_OPEN, errcode);
        EXPECT_EQ(NULL, invHandle);

        errcode = mic_is_device_avail(handle, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_thermal_version_by_index(NULL, 0, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_thermal_version_by_index(handle, 0, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_power_sensor_value_by_index(NULL, 0, NULL, 0);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_power_sensor_value_by_index(handle, 0, NULL, 0);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_silicon_sku(NULL, NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_silicon_sku(handle, NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_silicon_sku(handle, postCode, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_post_code(NULL, NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_post_code(handle, NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_post_code(handle, postCode, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_post_code(handle, postCode, &size);
        EXPECT_EQ(ERR_SUCCESS, errcode);

        errcode = mic_get_device_type(NULL, NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_device_type(handle, NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_device_type(handle, postCode, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        size = 0;
        errcode = mic_get_device_type(handle, postCode, &size);
        EXPECT_EQ(ERR_SUCCESS, errcode);

        errcode = mic_get_device_name(NULL, NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_device_name(handle, NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_device_name(handle, postCode, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        size = 0;
        errcode = mic_get_device_name(handle, postCode, &size);
        EXPECT_EQ(ERR_SUCCESS, errcode);

        errcode = mic_get_thermal_version_by_name(NULL, NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_thermal_version_by_name(handle, NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_thermal_version_by_name(handle, postCode, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_power_sensor_value_by_name(NULL, NULL, NULL, 0);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_power_sensor_value_by_name(handle, NULL, NULL, 0);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_power_sensor_value_by_name(handle, postCode, NULL, 0);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_uuid(NULL, NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_uuid(handle, NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_uuid(handle, postCode, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        size = 0;
        errcode = mic_get_uuid(handle, postCode, &size);
        EXPECT_EQ(ERR_SUCCESS, errcode);

        errcode = mic_get_serial_number(NULL, NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_serial_number(handle, NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_serial_number(handle, postCode, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        size = 0;
        errcode = mic_get_serial_number(handle, postCode, &size);
        EXPECT_EQ(ERR_SUCCESS, errcode);

        errcode = mic_set_turbo_mode(NULL, 0);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_set_smc_persistence_flag(NULL, 0);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_set_power_limit1(NULL, 0, 0);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_set_power_limit0(NULL, 0, 0);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_set_led_alert(NULL, 0);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_leave_maint_mode(NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_ndevices(NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_enter_maint_mode(NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_close_device(NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_led_alert(NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_led_alert(handle, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_in_ready_state(NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_in_ready_state(handle, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_in_maint_mode(NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_in_maint_mode(handle, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_turbo_state_valid(NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_turbo_state_valid(handle, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_turbo_state(NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_turbo_state(handle, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_turbo_mode(NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_turbo_mode(handle, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_time_window1(NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_time_window1(handle, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_time_window0(NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_time_window0(handle, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_tick_count(NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_tick_count(handle, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_threads_core(NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_threads_core(handle, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_smc_persistence_flag(NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_smc_persistence_flag(handle, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_processor_info(NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_processor_info(handle, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_power_phys_limit(NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_power_phys_limit(handle, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_power_lmrk(NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_power_lmrk(handle, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_power_hmrk(NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_power_hmrk(handle, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_num_cores(NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_num_cores(handle, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_memory_utilization_info(NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_memory_utilization_info(handle, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_jiffy_counter(NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_jiffy_counter(handle, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_cores_voltage(NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_cores_voltage(handle, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_cores_frequency(NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_cores_frequency(handle, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_cores_count(NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_cores_count(handle, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_thermal_throttle_info(NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_thermal_throttle_info(handle, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_power_throttle_info(NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_power_throttle_info(handle, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_pci_config(NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_pci_config(handle, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_memory_info(NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_memory_info(handle, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_ecc_mode(NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_ecc_mode(handle, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        dev = getOpenMicDev(NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);
        EXPECT_EQ(NULL, dev);

        dev = getOpenMicDev(handle, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);
        EXPECT_EQ(NULL, dev);

        errcode = mic_is_smc_boot_loader_ver_supported(NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_is_smc_boot_loader_ver_supported(handle, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_smc_hwrevision(NULL, NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_smc_hwrevision(handle, NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_smc_hwrevision(handle, postCode, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        size = 0;
        errcode = mic_get_smc_hwrevision(handle, postCode, &size);
        EXPECT_EQ(ERR_SUCCESS, errcode);

        errcode = mic_get_smc_fwversion(NULL, NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_smc_fwversion(handle, NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_smc_fwversion(handle, postCode, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        size = 0;
        errcode = mic_get_smc_fwversion(handle, postCode, &size);
        EXPECT_EQ(ERR_SUCCESS, errcode);

        errcode = mic_get_smc_boot_loader_ver(NULL, NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_smc_boot_loader_ver(handle, NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_smc_boot_loader_ver(handle, postCode, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        size = 0;
        errcode = mic_get_smc_boot_loader_ver(handle, postCode, &size);
        EXPECT_EQ(ERR_SUCCESS, errcode);

        errcode = mic_get_part_number(NULL, NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_part_number(handle, NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_part_number(handle, postCode, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        size = 0;
        errcode = mic_get_part_number(handle, postCode, &size);
        EXPECT_EQ(ERR_SUCCESS, errcode);

        errcode = mic_get_me_version(NULL, NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_me_version(handle, NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_me_version(handle, postCode, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        size = 0;
        errcode = mic_get_me_version(handle, postCode, &size);
        EXPECT_EQ(ERR_SUCCESS, errcode);

        errcode = mic_get_fsc_strap(NULL, NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_fsc_strap(handle, NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_fsc_strap(handle, postCode, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        size = 0;
        errcode = mic_get_fsc_strap(handle, postCode, &size);
        EXPECT_EQ(ERR_NOT_SUPPORTED, errcode);

        errcode = mic_get_flash_version(NULL, NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_flash_version(handle, NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_flash_version(handle, postCode, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        size = 0;
        errcode = mic_get_flash_version(handle, postCode, &size);
        EXPECT_EQ(ERR_SUCCESS, errcode);

        errcode = mic_get_coprocessor_os_version(NULL, NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_coprocessor_os_version(handle, NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_coprocessor_os_version(handle, postCode, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        size = 0;
        errcode = mic_get_coprocessor_os_version(handle, postCode, &size);
        EXPECT_EQ(ERR_SUCCESS, errcode);

        errcode = mic_update_core_util(NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_update_core_util(handle, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_thermal_sensor_list(NULL, NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_thermal_sensor_list(handle, NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_thermal_sensor_list(handle, &strPtr, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_power_sensor_list(NULL, NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_power_sensor_list(handle, NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_power_sensor_list(handle, &strPtr, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_pc6_mode(NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_pc6_mode(handle, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_pc3_mode(NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_pc3_mode(handle, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_cpufreq_mode(NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_cpufreq_mode(handle, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_corec6_mode(NULL, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);

        errcode = mic_get_corec6_mode(handle, NULL);
        EXPECT_EQ(ERR_INVALID_ARG, errcode);
    }

    TEST(libsystools, TC_KNL_libsystools_KnlMockDevice_001)
    {
        reset_miclib_instance();
        int devNum0      = 0;
        int devNum1      = 1;
        size_t count     = 0;
        uint32_t errcode = 0;
        MIC *handle      = NULL;
        MIC *handle1     = NULL;
        const char *errstr  = NULL;

        string mock    = "KNL_MOCK_CNT";
        string mockCnt = "2";

    #ifdef _WIN32
        SetEnvironmentVariableA((LPCSTR)mock.c_str(), (LPCSTR)mockCnt.c_str());
    #else
        setenv ( mock.c_str(), mockCnt.c_str(), 1 );
    #endif
        {
            EXPECT_EQ( ERR_SUCCESS, mic_get_ndevices(&count));
            cout << " NUMBER OF DEVICES = " << count << endl;

            handle1 = mic_open_device(devNum0, &errcode);
            EXPECT_EQ( ERR_SUCCESS, errcode);

            errcode = mic_close_device(handle1);
            EXPECT_EQ( ERR_SUCCESS, errcode);

            handle1 = mic_open_device_by_name("mic0",&errcode);
            EXPECT_EQ( ERR_SUCCESS, errcode);

            errcode = mic_close_device(handle1);
            EXPECT_EQ( ERR_SUCCESS, errcode);

            handle = mic_open_device(devNum1, &errcode);
            EXPECT_EQ( ERR_SUCCESS, errcode);

            int devAvail = -1;
            errcode = mic_is_device_avail(handle, &devAvail);
            EXPECT_EQ( ERR_SUCCESS, errcode);
            cout << "DEV ONLINE : DEVICE = " << devNum1 << "  STATUS =  " << devAvail << endl;

            char name[5] = "";
            size_t size = 5;
            errcode = mic_get_device_name(handle, name, &size);
            EXPECT_EQ( ERR_SUCCESS, errcode);
            cout << "DEV NAME   : DEVICE = " << devNum1 << "  NAME   =  " << name << endl;

            char type[15] = "";
            size_t typeSize = 15;
            errcode = mic_get_device_type(handle, type, &typeSize);
            EXPECT_EQ( ERR_SUCCESS, errcode);
            cout << "DEV TYPE   : DEVICE = " << devNum1 << "  TYPE   =  " << type << endl;

            char slNum[25] = "";
            size_t slSize = 25;
            errcode = mic_get_serial_number(handle, slNum, &slSize);
            EXPECT_EQ( ERR_SUCCESS, errcode);
            cout << "DEV SLNUM  : DEVICE = " << devNum1 << "  SLNO  =  " << slNum << endl;

            char uuid[25] = "";
            size_t uuidSize = 25;
            errcode = mic_get_uuid(handle, uuid, &uuidSize);
            EXPECT_EQ( ERR_SUCCESS, errcode);
            cout << "DEV UUID   : DEVICE = " << devNum1 << "  UUID  =  " << uuid << endl;

            char sku[15] = "";
            size_t skuSize = 15;
            errcode = mic_get_silicon_sku(handle, sku, &skuSize);
            EXPECT_EQ( ERR_SUCCESS, errcode);
            cout << "DEV SKU    : DEVICE = " << devNum1 << "  SKU   =  " << sku << endl;

            char post[3]= "";
            size_t postSize = 3;
            errcode = mic_get_post_code(handle, post, &postSize);
            EXPECT_EQ( ERR_SUCCESS, errcode);
            cout << "DEV POST   : DEVICE = " << devNum1 << "  PCODE =  " << post << endl;

            handle1 = mic_open_device(devNum1, &errcode);
            EXPECT_EQ( ERR_ALRDY_OPEN , errcode);

            errstr = mic_get_error_string(errcode);
            ASSERT_STREQ( errstr, "Device already open");

            cout << "*** PCI CONFIG DATA FOR DEVICE = " << devNum1 << endl;
            MIC_PCI_CONFIG pciCfg;
            errcode = mic_get_pci_config(handle,&pciCfg);
            EXPECT_EQ( ERR_SUCCESS, errcode);
            if (ERR_SUCCESS == errcode) {
                cout << " DOMAIN      : " << pciCfg.domain << endl;
                cout << " BUS NO      : " << pciCfg.bus_no << endl;
                cout << " DEVICE NO   : " << pciCfg.device_no << endl;
                cout << " VENDOR ID   : " << pciCfg.vendor_id << endl;
                cout << " DEVICE ID   : " << pciCfg.device_id << endl;
                cout << " REVISION ID : " << pciCfg.revision_id << endl;
                cout << " SUBSYS ID   : " << pciCfg.subsystem_id << endl;
                cout << " LINK SPEED  : " << pciCfg.link_speed << endl;
                cout << " LINK WIDTH  : " << pciCfg.link_width << endl;
                cout << " PAYLOAD     : " << pciCfg.payload_size << endl;
                cout << " READ REQ    : " << pciCfg.read_req_size << endl;
                cout << " FULL ACCESS : " << pciCfg.access_violation << endl;
                cout << " CLASS CODE  : " << pciCfg.class_code << endl;
            }

            uint16_t mode ;
            errcode = mic_get_ecc_mode(handle, &mode);
            EXPECT_EQ( ERR_SUCCESS, errcode);
            cout << "DEV ECC MODE : DEVICE = " << devNum1 << "  MODE =  " << mode << endl;

            cout << "*** MEM INFO FOR DEVICE = " << devNum1 << endl;
            MIC_MEM_INFO memInfo;
            errcode = mic_get_memory_info(handle,&memInfo);
            EXPECT_EQ( ERR_SUCCESS, errcode);
            if (ERR_SUCCESS == errcode) {
                cout << " VENDOR NAME : " << memInfo.vendor_name << endl;
                cout << " MEM TYPE    : " << memInfo.memory_type << endl;
                cout << " MEM TECH    : " << memInfo.memory_tech << endl;
                cout << " REVISION    : " << memInfo.revision << endl;
                cout << " DENSITY     : " << memInfo.density << endl;
                cout << " SIZE        : " << memInfo.size << endl;
                cout << " SPEED       : " << memInfo.speed << endl;
                cout << " FREQUENCY   : " << memInfo.freq << endl;
                cout << " VOLT        : " << memInfo.volt << endl;
                cout << " ECC         : " << memInfo.ecc << endl;
            }

            cout << "*** PROCESSOR INFO FOR DEVICE = " << devNum1 << endl;
            MIC_PROCESSOR_INFO procInfo;
            errcode = mic_get_processor_info(handle,&procInfo);
            EXPECT_EQ( ERR_SUCCESS, errcode);
            if (ERR_SUCCESS == errcode) {
                cout << " MODEL        : " << procInfo.model << endl;
                cout << " MODEL EXT    : " << procInfo.model_ext << endl;
                cout << " FAMILY       : " << procInfo.family << endl;
                cout << " FAMILY EXT   : " << procInfo.family_ext << endl;
                cout << " STEPPING DATA: " << procInfo.stepping_data << endl;
                cout << " SUB STEPPING : " << procInfo.substepping_data << endl;
                cout << " TYPE         : " << procInfo.type << endl;
                cout << " STEPPING     : " << procInfo.stepping << endl;
                cout << " SKU          : " << procInfo.sku << endl;
            }

            cout << "*** COPROCESSOR OS CORE INFO FOR DEVICE = " << devNum1 << endl;
            uint32_t count = 0;
            errcode = mic_get_cores_count(handle, &count);
            EXPECT_EQ( ERR_SUCCESS, errcode);
            cout << " CORES COUNT : " << count << endl;

            uint32_t voltage = 0;
            errcode = mic_get_cores_voltage(handle, &voltage);
            EXPECT_EQ( ERR_SUCCESS, errcode);
            cout << " CORES VOLT  : " << voltage << endl;

            uint32_t freq = 0;
            errcode = mic_get_cores_frequency(handle, &freq);
            EXPECT_EQ( ERR_SUCCESS, errcode);
            cout << " CORES FREQ  : " << freq << endl;

            cout << "*** VERSION INFO FOR DEVICE = " << devNum1 << endl;
            char ver[25] = "";
            size_t verSize = 25;
            errcode = mic_get_coprocessor_os_version(handle, ver, &verSize);
            EXPECT_EQ( ERR_SUCCESS, errcode);
            cout << " COPROC OS VER : " << ver << endl;

            errcode = mic_get_part_number(handle, ver, &verSize);
            EXPECT_EQ( ERR_SUCCESS, errcode);
            cout << " PART NUMBER   : " << ver << endl;

            errcode = mic_get_fsc_strap(handle, ver, &verSize);
            EXPECT_EQ( ERR_NOT_SUPPORTED, errcode);
            cout << " STRAP INFO NOT SUPPORTED" << endl;

            errcode = mic_get_flash_version(handle, ver, &verSize);
            EXPECT_EQ( ERR_SUCCESS, errcode);
            cout << " FLASH VERSION: " << ver << endl;

            errcode = mic_get_me_version(handle, ver, &verSize);
            EXPECT_EQ( ERR_SUCCESS, errcode);
            cout << " ME VERSION   : " << ver << endl;

            errcode = mic_get_smc_fwversion(handle, ver, &verSize);
            EXPECT_EQ( ERR_SUCCESS, errcode);
            cout << " SMC FW VER   : " << ver << endl;

            errcode = mic_get_smc_hwrevision(handle, ver, &verSize);
            EXPECT_EQ( ERR_SUCCESS, errcode);
            cout << " SMC HW VER   : " << ver << endl;

            int supported = -1;
            errcode = mic_is_smc_boot_loader_ver_supported(handle, &supported);
            EXPECT_EQ( ERR_SUCCESS, errcode);
            cout << " SMC BOOT SUPP: " << supported << endl;

            errcode = mic_get_smc_boot_loader_ver(handle, ver, &verSize);
            EXPECT_EQ( ERR_SUCCESS, errcode);
            cout << " SMC BOOT VER : " << ver << endl;

            cout << "**** SET/GET LED DEVICE = "  << devNum1 << endl;
            uint32_t alert = 1;
            errcode = mic_set_led_alert(handle, alert);
            EXPECT_EQ( ERR_SUCCESS, errcode);

            uint32_t ledAlert = 0;
            errcode = mic_get_led_alert(handle, &ledAlert);
            EXPECT_EQ( ERR_SUCCESS, errcode);
            cout << " GET LED ALERT : " << ledAlert << endl;

            cout << "**** SET/GET TURBO MODE FOR DEVICE = "  << devNum1 << endl;
            int turboMode = 1;
            errcode = mic_set_turbo_mode(handle, turboMode);
            EXPECT_EQ( ERR_SUCCESS, errcode);

            turboMode = 0;
            errcode = mic_get_turbo_mode(handle, &turboMode);
            EXPECT_EQ( ERR_SUCCESS, errcode);
            cout << " GET TURBO MODE: " << turboMode << endl;

            int turboValid = 0;
            errcode = mic_get_turbo_state_valid(handle, &turboValid);
            EXPECT_EQ( ERR_SUCCESS, errcode);
            cout << " TURBO VALID   : " << turboValid << endl;

            int turboState = 0;
            errcode = mic_get_turbo_state(handle, &turboState);
            EXPECT_EQ( ERR_SUCCESS, errcode);
            cout << " TURBO ACTIVE  : " << turboState << endl;

            cout << "**** SET/GET SMC CONFIG FOR DEVICE = "  << devNum1 << endl;
            int persist = 1;
            errcode = mic_set_smc_persistence_flag(handle, persist);
            EXPECT_EQ( ERR_SUCCESS, errcode);

            persist = 0;
            errcode = mic_get_smc_persistence_flag(handle, &persist);
            EXPECT_EQ( ERR_SUCCESS, errcode);
            cout << " GET SMC FLAG: " << persist << endl;

            cout << "**** THERMAL THROTTLE INFO FOR DEVICE = " << devNum1 << endl;
            MIC_THROTTLE_INFO thermal;
            errcode = mic_get_thermal_throttle_info(handle, &thermal);
            EXPECT_EQ( ERR_NOT_SUPPORTED, errcode);
            if (ERR_NOT_SUPPORTED == errcode) {
                cout << " THERMAL THROTTLE INFO NOT SUPPORTED FOR X200 DEVICES!" << endl;
            }

            cout << "**** POWER THROTTLING INFO FOR DEVICE = " << devNum1 << endl;
            MIC_THROTTLE_INFO power;
            errcode = mic_get_power_throttle_info(handle, &power);
            EXPECT_EQ( ERR_NOT_SUPPORTED, errcode);
            if (ERR_NOT_SUPPORTED == errcode) {
                cout << " POWER THROTTLE INFO NOT SUPPORTED FOR X200 DEVICES!" << endl;
            }

            cout << "**** COPROCESSOR OS POWER MGMT CONFIG FOR DEVICE = " << devNum1 << endl;
            int powerMode = -1;
            errcode = mic_get_cpufreq_mode(handle, &powerMode);
            EXPECT_EQ( ERR_NOT_SUPPORTED, errcode);
            cout << " STATE PF1 : " << powerMode << endl;

            errcode = mic_get_corec6_mode(handle, &powerMode);
            EXPECT_EQ( ERR_NOT_SUPPORTED, errcode);
            cout << " STATE CO6 : " << powerMode << endl;

            errcode = mic_get_pc3_mode(handle, &powerMode);
            EXPECT_EQ( ERR_NOT_SUPPORTED, errcode);
            cout << " STATE PC3 : " << powerMode << endl;

            errcode = mic_get_pc6_mode(handle, &powerMode);
            EXPECT_EQ( ERR_NOT_SUPPORTED, errcode);
            cout << " STATE PC6 : " << powerMode << endl;

            cout << "**** MODE SWITCH/STOP/START FOR DEVICE = "  << devNum1 <<  endl;
            int state = -1;
            errcode = mic_in_ready_state(handle, &state);
            EXPECT_EQ( ERR_SUCCESS, errcode);
            cout << " READY STATE: " << state << endl;

            errcode = mic_enter_maint_mode(handle);
            EXPECT_EQ( ERR_NOT_SUPPORTED, errcode);     // Not supported for KNL

            int maintMode = -1;
            errcode = mic_in_maint_mode(handle, &maintMode);
            EXPECT_EQ( ERR_NOT_SUPPORTED, errcode);     // Not supported for KNL
            cout << " MAINT MODE: " << maintMode << endl;

            errcode = mic_leave_maint_mode(handle);
            EXPECT_EQ( ERR_NOT_SUPPORTED, errcode);     // Not supported for KNL

            cout << "**** THERMAL INFO FOR DEVICE = "  << devNum1 <<  endl;
            char **sensorNames = NULL;
            size_t numSensors = 0;
            errcode = mic_get_thermal_sensor_list(handle, &sensorNames, &numSensors);
            EXPECT_EQ( ERR_SUCCESS, errcode);
            for (size_t i=0; i < numSensors; ++i)
                cout << " SENSOR NAME[" <<  i  << "] = " << sensorNames[i] << endl;

            uint32_t dieVal = 0;
            errcode = mic_get_thermal_version_by_name(handle, "Die", &dieVal);
            EXPECT_EQ( ERR_SUCCESS, errcode);
            cout << " DIE TEMPERATURE = " <<  dieVal << endl;

            uint32_t fanoutTemp = 0;
            int fanoutIdx = 2;
            errcode = mic_get_thermal_version_by_index(handle, fanoutIdx, &fanoutTemp);
            EXPECT_EQ( ERR_SUCCESS, errcode);
            cout << " FANOUT TEMPERATURE =  " << fanoutTemp << endl;

            cout << "**** POWER UTIL INFO FOR DEVICE = "  << devNum1 <<  endl;
            char **powerSensors = NULL;
            size_t sensorsCnt = 0;
            errcode = mic_get_power_sensor_list(handle, &powerSensors, &sensorsCnt);
            EXPECT_EQ( ERR_SUCCESS, errcode);
            for (size_t i=0; i < sensorsCnt; ++i)
               cout << " POWER SENSOR NAME[" << i  << "] = " << powerSensors[i] << endl;

            double pcieVal = 0;
            errcode = mic_get_power_sensor_value_by_name(handle, "PCIe", &pcieVal, 0);
            EXPECT_EQ( ERR_SUCCESS, errcode);
            cout << " PCIe Power = " << pcieVal << endl;

            double e2x4powerVal = 0;
            uint32_t e2x4Index = 2;
            errcode = mic_get_power_sensor_value_by_index(handle, e2x4Index, &e2x4powerVal, 0);
            EXPECT_EQ( ERR_SUCCESS, errcode);
            cout << "e2x4Power SENSOR VALUE = " << e2x4powerVal << endl;

            cout << "**** POWER LIMITS FOR DEVICE = " << devNum1 <<  endl;
            uint32_t physLim = 0;
            errcode = mic_get_power_phys_limit(handle, &physLim);
            EXPECT_EQ( ERR_NOT_SUPPORTED, errcode);
            cout << "POWER THRESHOLD MAX NOT SUPPORTED" << endl;

            uint32_t hmrk = 0;
            errcode = mic_get_power_hmrk(handle, &hmrk);
            EXPECT_EQ( ERR_SUCCESS, errcode);
            cout << "POWER THRESHOLD HIGH MARK = " << hmrk << endl;

            uint32_t lmrk = 0;
            errcode = mic_get_power_lmrk(handle, &lmrk);
            EXPECT_EQ( ERR_SUCCESS, errcode);
            cout << "POWER THRESHOLD LOW MARK = " << lmrk << endl;

            uint32_t window0 = 0;
            errcode = mic_get_time_window0(handle, &window0);
            EXPECT_EQ( ERR_SUCCESS, errcode);
            cout << "TIME WINDOW0 = " << window0 << endl;

            uint32_t window1 = 0;
            errcode = mic_get_time_window1(handle, &window1);
            EXPECT_EQ( ERR_SUCCESS, errcode);
            cout << "TIME WINDOW1 = " << window1 << endl;

            const uint32_t PT_POWER = 1;
            const uint32_t PT_TIME  = 2;
            errcode = mic_set_power_limit0(handle, PT_POWER, PT_TIME);
            EXPECT_EQ( ERR_SUCCESS, errcode);

            errcode = mic_set_power_limit1(handle, PT_POWER, PT_TIME);
            EXPECT_EQ( ERR_SUCCESS, errcode);

            cout << "**** MEM UTIL INFO FOR DEVICE = "  << devNum1 << endl;
            MIC_MEMORY_UTIL_INFO memUtil;
            errcode = mic_get_memory_utilization_info(handle, &memUtil);
            EXPECT_EQ( ERR_SUCCESS, errcode);
            if (ERR_SUCCESS == errcode) {
                cout << "MEMORY FREE      = " << memUtil.memory_free << endl;
                cout << "MEMORY INUSE     = " << memUtil.memory_inuse << endl;
                cout << "MEMORY BUFFERS   = " << memUtil.memory_buffers << endl;
                cout << "MEMORY AVAILABLE = " << memUtil.memory_available << endl;
            }

            cout << "**** CORE UTIL INFO FOR DEVICE = "  << devNum1 << endl;
            MIC_CORE_UTIL_INFO coreUtil;
            errcode = mic_update_core_util(handle, &coreUtil);
            EXPECT_EQ( ERR_SUCCESS, errcode);
            if (ERR_SUCCESS == errcode) {
                cout << "CORE COUNT      = " << coreUtil.core_count << endl;
                cout << "THREAD COUNT    = " << coreUtil.core_threadcount << endl;
                cout << "TICK COUNT      = " << coreUtil.tick_count << endl;
                cout << "JIFFY COUNT     = " << coreUtil.jiffy_count << endl;
                cout << "IDLE SUM        = " << coreUtil.idle_sum << endl;
                cout << "NICE SUM        = " << coreUtil.nice_sum << endl;
                cout << "SYSTEM SUM      = " << coreUtil.sys_sum << endl;
                cout << "USER SUM        = " << coreUtil.user_sum << endl;

                for (unsigned int i= e_user; i <= e_idle; i++) {
                   cout << "COUNTER NUMBER = " << i << endl;
                   for (size_t core_idx = 0; core_idx < coreUtil.core_count; ++core_idx)
                       cout << "\tVALUES FOR CORE[" << core_idx << "] = " << coreUtil.counters[i][core_idx] << endl;
                }
            }
            size_t coreCnt = 0;
            errcode = mic_get_num_cores(handle, &coreCnt);
            EXPECT_EQ( ERR_SUCCESS, errcode);
            cout << "CORE COUNT = " << coreCnt << endl;

            size_t threadsCnt = 0;
            errcode = mic_get_threads_core(handle, &threadsCnt);
            EXPECT_EQ( ERR_SUCCESS, errcode);
            cout << "THREADS COUNT = " << threadsCnt << endl;

            uint64_t tickCnt = 0;
            errcode = mic_get_tick_count(handle, &tickCnt);
            EXPECT_EQ( ERR_SUCCESS, errcode);
            cout << "TICK COUNT = " <<  tickCnt << endl;

            uint64_t jiffyCnt = 0;
            errcode = mic_get_jiffy_counter(handle, &jiffyCnt);
            EXPECT_EQ( ERR_SUCCESS, errcode);
            cout << "JIFFY COUNT = " <<  jiffyCnt << endl;

            handle = mic_open_device(4, &errcode);
            EXPECT_EQ( ERR_NO_SUCH_DEVICE, errcode);
	}
    }
}
