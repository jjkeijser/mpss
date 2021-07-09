/*
 * Copyright 2012-2017 Intel Corporation.
 * 
 * This file is subject to the Intel Sample Source Code License. A copy
 * of the Intel Sample Source Code License is included.
*/

/*============================================================================
 *
 *  This example is a simple version of MicInfo to demonstrate the libystools
 *  C API usage.
 *
 *  The example:
 *  Iterate over all the devices and print the following Information.
 *  1. General device information.
 *  2. Version Information.
 *  3. Board Information.
 *  4. UUID Information.
 *  5. Memory Information.
 *  6. Memory Utilization Information.
 *  7. Thermal Information.
 *  8. Core Information.
 *  9. Core Utilization Information.
 *  10. Power Information.
 *  11. Power Utilization Information.
 *  12. Turbo Information.
 *  13. LED Information.
 *  14. Set LED Alert
 *  15. Set Power Threshold W0
 *  16. Set Power Threshold W1
 *============================================================================*/

/* INCLUDES */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "libsystools.h"
#include "micsdkerrno.h"

#ifdef _WIN32
    #include <windows.h>

    void sleep_p(uint32_t secs)
    {
        Sleep(secs * 1000);
    }
#else
    #include <unistd.h>

    void sleep_p(uint32_t secs)
    {
        sleep(secs);
    }
#endif

/* FUNCTIONS PROTO TYPE */
void show_device_info(MIC *handle, size_t dev_num);
void show_version_info(MIC *handle, size_t dev_num);
void show_board_info(MIC *handle, size_t dev_num);
void show_uuid_info(MIC *handle, size_t dev_num);
void show_memory_info(MIC *handle, size_t dev_num);
void show_memory_util_info(MIC *handle, size_t dev_num);
void show_thermal_info(MIC *handle, size_t dev_num);
void show_core_info(MIC *handle, size_t dev_num);
void show_core_util_info(MIC *handle, size_t dev_num);
void show_power_info(MIC *handle, size_t dev_num);
void show_power_util_info(MIC *handle, size_t dev_num);
void show_turbo_info(MIC *handle, size_t dev_num);
void show_led_info(MIC *handle, size_t dev_num);
void set_led_alert(MIC *handle, size_t dev_num);
void set_pthresh_w0(MIC *handle, size_t dev_num);
void set_pthresh_w1(MIC *handle, size_t dev_num);
void cleanup_array(char **array, size_t num_ele);
void print_line(const char *msn, const char *str, ...);
#ifdef _WIN32
    __inline int c99_vsnprintf(char *outBuf, size_t size, const char *format, va_list ap);
    __inline int c99_snprintf(char *outBuf, size_t size, const char *format, ...);
    #define snprintf c99_snprintf
    #define vsnprintf c99_vsnprintf
#endif

#define STR_MAX_SIZE 50
#define UUID_MAX_SIZE 50
#define REPORT_ERROR(func_name, err_code) \
    if(MICSDKERR_SUCCESS != err_code) { \
        fprintf(stderr, "ERROR: %s returned \"%s\" \n", func_name, mic_get_error_string(err_code)); \
    }
#define USAGE() \
    fprintf(stdout, "USAGE: examples [set]\n");

/* GLOBAL CONSTANTS */
const int ENABLED  = 1;
const int DISABLED = 0;

/* GLOBAL VARIABLES */
char      type[10]   = "";
uint32_t  led_alert   = 0;
uint32_t  w0_time     = 0;
uint32_t  w1_time     = 0;
uint32_t  w0_power    = 0;
uint32_t  w1_power    = 0;
uint8_t   get_error   = 0;

/*----------------------------------------------------------------------------*/

int main(int argc, char** argv)
{
    uint32_t  status = 0;
    uint8_t   set_functions_enabled = 0;
    size_t    num_devices = 0;
    size_t    dev_num;
    MIC       *device_handle;

    if (argc > 2) {
        USAGE();
        return 1;
    }

    if (argc == 2) {
        if(strcmp(argv[1], "set")) {
           fprintf(stderr, "Error! Invalid argument %s\n", argv[1]);
           USAGE();
           return 1;
        }
        set_functions_enabled = 1;
    }

    status = mic_get_ndevices(&num_devices);
    if(MICSDKERR_SUCCESS != status) {
        fprintf(stderr, "ERROR: mic_get_ndevices returned \"%s\" \n", mic_get_error_string(status));
        return 1;
    }

    if (0 == num_devices)
        fprintf(stdout, "No devices found\n");

    /* Loop over all devices and print device info */
    for (dev_num=0; dev_num < num_devices; dev_num++)
    {
        device_handle = mic_open_device((int)dev_num, &status);
        if (NULL == device_handle) {
            fprintf(stderr, "ERROR: mic_open_device returned \"%s\" \n", mic_get_error_string(status));
            continue;   /* Skip device*/
        }
        /* Show general device info */
        show_device_info(device_handle, dev_num);

        /* Show version info */
        show_version_info(device_handle, dev_num);

        /* Show board info */
        show_board_info(device_handle, dev_num);

        /* Show board info */
        show_uuid_info(device_handle, dev_num);

        /* Show memory info */
        show_memory_info(device_handle, dev_num);

        /* Show memory utilization info */
        show_memory_util_info(device_handle, dev_num);

        /* Show thermal info */
        show_thermal_info(device_handle, dev_num);

        /* Show core info */
        show_core_info(device_handle, dev_num);

        /* Show core utilization info*/
        show_core_util_info(device_handle, dev_num);

        /* Show power info */
        show_power_info(device_handle, dev_num);

        /* Show power utilization info */
        show_power_util_info(device_handle, dev_num);

        /* Show turbo info */
        show_turbo_info(device_handle, dev_num);

        /* Show led info */
        show_led_info(device_handle, dev_num);

        /* Set functions (Only Call them if enabled)*/
        if (set_functions_enabled && !get_error) {
            fprintf(stdout, "\n\n***** Starting Set Functions Examples! *****");
            /* Set LED Alert*/
            set_led_alert(device_handle, dev_num);

            /* Set Power Threshold W0 */
            set_pthresh_w0(device_handle, dev_num);

            /* Set Power Threshold W1 */
            set_pthresh_w1(device_handle, dev_num);
            fprintf(stdout, "\n\n***** Ending Set Functions Examples! *****");
        }

        /* We're done with this device.*/
        status = mic_close_device(device_handle);
        if (MICSDKERR_SUCCESS != status) {
            fprintf(stderr, "ERROR: mic_close_device returned \"%s\" \n", mic_get_error_string(status));
        }
        device_handle = NULL;
        fprintf(stdout, "\n\n");
    }
    return  0;
}

void print_line(const char *msn, const char *str, ...){
    va_list ap;
    va_start(ap,str);
    switch(*msn){
        case '!': printf("\n\n%s:  ",msn+1);  break; // Headers
        case '-': printf("\n    %s:",msn+1);  break; // Sub header
        case '#': printf("\n\t");             break; // Table
        default : printf("\n\t%-27s : ",msn); break; // Normal
    }
    while(*str!='\0'){
        if(*str=='%'){
            switch(*(++str)){
                case 'd': printf("%d",va_arg(ap,int));        break;
                case 'D': printf("%04d",va_arg(ap,int));      break;
                case 'f': printf("%2.3f",va_arg(ap,double));  break;
                case 'x': printf("0x%02x",va_arg(ap,int));    break;
                case 'X': printf("0x%04x",va_arg(ap,int));    break;
                case 's': printf("%s",va_arg(ap,char *));     break;
                case 'u': printf("mic%lu",va_arg(ap,size_t)); break;
                case 'U': printf("%lu",(va_arg(ap,uint64_t)));break;
                case '#': printf("%-30s",va_arg(ap,char *));  break;
                case 'F': printf("%.3lf",va_arg(ap,double));  break;
                default: str--;                               break;
            }
        }
        else{
            putchar(*str);
        }
        str++;
    }
}

/*
    snprintf is not supported by earlier versions of VS2015
*/
#ifdef _WIN32
__inline int c99_vsnprintf(char *outBuf, size_t size, const char *format, va_list ap)
{
    int count = -1;

    if (size != 0)
        count = _vsnprintf_s(outBuf, size, _TRUNCATE, format, ap);
    if (count == -1)
        count = _vscprintf(format, ap);

    return count;
}

__inline int c99_snprintf(char *outBuf, size_t size, const char *format, ...)
{
    int count;
    va_list ap;

    va_start(ap, format);
    count = c99_vsnprintf(outBuf, size, format, ap);
    va_end(ap);

    return count;
}
#endif

void show_device_info(MIC *handle, size_t dev_num)
{
    uint32_t err_code;
    size_t data_size = 0;
    char name[15]   = "";

    data_size = 15;
    err_code = mic_get_device_name(handle, name, &data_size);
    REPORT_ERROR("mic_get_device_name", err_code);

    data_size = 10;
    err_code = mic_get_device_type(handle, type, &data_size);
    REPORT_ERROR("mic_get_device_type", err_code);

    print_line("!GENERAL INFORMATION","%u",dev_num);
    print_line("Device Name", "%s", name);
    print_line("Device Type", "%s", type);
}

void show_version_info(MIC *handle, size_t dev_num)
{
    uint32_t err_code;
    size_t data_size   = 0;
    int online         = -1;
    char serial[25]    = "";
    char smc_fw[25]    = "";
    char os_ver[25]    = "";
    char smc_boot[25]  = "";
    char flash_ver[25] = "";
    char me_ver[25]    = "";

    err_code = mic_is_device_avail(handle, &online);
    REPORT_ERROR("mic_is_device_avail", err_code);

    data_size = 10;
    print_line("!VERSION INFORMATION","%u",dev_num);
    if (ENABLED == online) {
        data_size = 25;
        err_code = mic_get_serial_number(handle, serial, &data_size);
        REPORT_ERROR("mic_get_serial_number", err_code);
        print_line("Serial Number", "%s", serial);

        data_size = 25;
        err_code = mic_get_smc_fwversion(handle, smc_fw, &data_size);
        REPORT_ERROR("mic_get_smc_fwversion", err_code);
        print_line("SMC Firmware Version", "%s", smc_fw);

        data_size = 25;
        err_code = mic_get_coprocessor_os_version(handle, os_ver, &data_size);
        REPORT_ERROR("mic_get_coprocessor_os_version", err_code);
        print_line("Coprocessor OS Ver", "%s", os_ver);

        if (0 == strcmp(type,"x100")) {
            data_size = 25;
            err_code = mic_get_flash_version(handle, flash_ver, &data_size);
            REPORT_ERROR("mic_get_flash_version", err_code);
            print_line("Flash Version", "%s", flash_ver);

            data_size = 25;
            err_code = mic_get_smc_boot_loader_ver(handle, smc_boot, &data_size);
            REPORT_ERROR("mic_get_smc_boot_loader_ver", err_code);
            print_line("SMC Boot version", "%s", smc_boot);
        }
        if (0 == strcmp(type,"x200")) {
            data_size = 25;
            err_code = mic_get_me_version(handle, me_ver, &data_size);
            REPORT_ERROR("mic_get_me_version", err_code);
            print_line("ME Version", "%s", me_ver);
        }
    }
    else if (DISABLED == online) {
        print_line("Serial Number",       "Not Available");
        print_line("SMC Firmware Version","Not Available");
        print_line("Coprocessor OS Ver",  "Not Available");
        if (0 == strcmp(type,"x100")) {
            print_line("Flash Version",   "Not Available");
            print_line("SMC Boot version","Not Available");
        }
        if (0 == strcmp(type,"x200")) {
            print_line("ME Version",      "Not Available");
        }
    }
}

void show_board_info(MIC *handle, size_t dev_num)
{
    uint32_t err_code;
    MIC_PCI_CONFIG pci_config;
    MIC_PROCESSOR_INFO proc_config;

    err_code = mic_get_pci_config(handle, &pci_config);
    REPORT_ERROR("mic_get_pci_config", err_code);

    if (MICSDKERR_SUCCESS != err_code)
        return;

    err_code = mic_get_processor_info(handle, &proc_config);
    REPORT_ERROR("mic_get_processor_info", err_code);

    if (MICSDKERR_SUCCESS != err_code)
        return;

    print_line("!BOARD INFORMATION","%u",dev_num);

    print_line("-PCI INFORMATION","");
    print_line("Vendor id",   "%X", pci_config.vendor_id);
    print_line("Device id",   "%X", pci_config.device_id);
    print_line("Revision id", "%x", pci_config.revision_id);
    print_line("Subsystem id","%X", pci_config.subsystem_id);

#ifdef _WIN32
    print_line("Bus id", "%D", pci_config.bus_no); // Windows OS reports PCI BUS ID in Decimal
#else //_WIN32
    print_line("Bus id", "%X", pci_config.bus_no); // Linux OS reports PCI BUS ID in Hexadecimal
#endif //_WIN32

    if ((uint32_t)DISABLED == pci_config.access_violation) {
        print_line("Width",            "x%d",     pci_config.link_width);
        print_line("Max payload size", "%d Bytes",pci_config.payload_size);
        print_line("Max read req size","%d Bytes",pci_config.read_req_size);
        print_line("Link speed",       "%d MT/s", pci_config.link_speed);
    }
    else {
        print_line("Width", "Insufficient Privileges");
        print_line("Max payload size",  "Insufficient Privileges");
        print_line("Max read req size", "Insufficient Privileges");
        print_line("Link speed",        "Insufficient Privileges");
    }

    print_line("-Coprocessor INFORMATION",   "");
    print_line("Coprocessor model",          "%d",proc_config.model);
    print_line("Coprocessor model Extension","%d",proc_config.model_ext);
    print_line("Coprocessor type",           "%d",proc_config.type);
    print_line("Coprocessor stepping id",    "%d",proc_config.stepping_data);
    print_line("Coprocessor stepping",       "%s",proc_config.stepping);
    print_line("Coprocessor SKU",            "%s",proc_config.sku);
}

void show_uuid_info(MIC *handle, size_t dev_num)
{
    uint32_t err_code;
    char uuid[UUID_MAX_SIZE];
    size_t uuid_size = UUID_MAX_SIZE;

    err_code = mic_get_uuid(handle, uuid, &uuid_size);
    REPORT_ERROR("mic_get_uuid", err_code);

    if (MICSDKERR_SUCCESS != err_code) {
        print_line("!UUID INFORMATION","%u \"Not Available\"",dev_num);
        return;
    }

    print_line("!UUID INFORMATION","%u",dev_num);
    if (0 == strcmp(type,"x100")) {
        int i = 0;
         print_line("UUID", "");
        for (i = 0; i < 16; i++)
            fprintf(stdout, "0x%02x ", ((uint8_t)uuid[i]) & 0xff);
    }
    else
        print_line("UUID", "%s", uuid);
}

void show_memory_info(MIC *handle, size_t dev_num)
{
    uint32_t err_code;
    MIC_MEM_INFO mem_info;;

    err_code = mic_get_memory_info(handle, &mem_info);
    REPORT_ERROR("mic_get_memory_info", err_code);

    if (MICSDKERR_SUCCESS != err_code) {
        print_line("!MEMORY INFORMATION","%u \"Not Available\"",dev_num);
        return;
    }

    print_line("!MEMORY INFORMATION","%u",dev_num);

    print_line("Memory vendor",   "%s",                  mem_info.vendor_name);
    print_line("Memory revision", "%d",                  mem_info.revision);
    print_line("Memory density",  "%d Mega-bits/device", mem_info.density);
    print_line("Memory size",     "%d MBytes",           mem_info.size);
    print_line("Memory type",     "%s",                  mem_info.memory_type);
    print_line("Memory tech",     "%s",                  mem_info.memory_tech);
    print_line("Memory frequency","%d MHz",              mem_info.freq);
    print_line("Memory speed",    "%d MT/s",             mem_info.speed);
    print_line("ECC mode",        "%d",                  mem_info.ecc);
}

void show_thermal_info(MIC *handle, size_t dev_num)
{
    uint32_t err_code;
    char **sensor_names   = NULL;
    size_t sensor_size    = 0;
    uint32_t sensor_value = 0;
    size_t idx;
    char buffer[STR_MAX_SIZE];

    err_code = mic_get_thermal_sensor_list(handle, &sensor_names, &sensor_size);
    REPORT_ERROR("mic_get_thermal_sensor_list", err_code);

    if (MICSDKERR_SUCCESS != err_code) {
        print_line("!THERMAL INFORMATION","%u \"Not Available\"",dev_num);
        return;
    }

    print_line("!THERMAL INFORMATION","%u",dev_num);
    for (idx = 0; idx < sensor_size; ++idx)
    {
        snprintf(buffer,STR_MAX_SIZE,"Sensor %s",sensor_names[idx]);
        err_code = mic_get_thermal_version_by_index(handle, (int)idx, &sensor_value);
        REPORT_ERROR("mic_get_thermal_version_by_index", err_code);
        print_line(buffer,"%U Celsius",sensor_value);
     }

    /* Free allocated memory */
    mic_free_sensor_list(sensor_names, sensor_size);
}

void show_core_info(MIC *handle, size_t dev_num)
{
    uint32_t err_code;
    uint32_t voltage = 0;
    uint32_t frequency = 0;
    MIC_CORE_UTIL_INFO core_util;

    err_code = mic_update_core_util(handle, &core_util);
    REPORT_ERROR("mic_update_core_util", err_code);

    if (MICSDKERR_SUCCESS != err_code) {
        print_line("!CORE INFORMATION","%u \"Not Available\"",dev_num);
        return;
    }

    print_line("!CORE INFORMATION",        "%u",dev_num);
    print_line("Total No. of Active Cores","%U",core_util.core_count);
    print_line("Threads per Core",         "%U",core_util.core_threadcount);

    /* Clean up memory allocated for the counters */
    mic_free_core_util(&core_util);

    mic_get_cores_voltage(handle, &voltage);
    REPORT_ERROR("mic_get_cores_voltage", err_code);

    mic_get_cores_frequency(handle, &frequency);
    REPORT_ERROR("mic_get_cores_frequency", err_code);

    print_line("Core Voltage",  "%U mV" ,voltage);
    print_line("Core Frequency","%U MHz",frequency);
}

void show_core_util_info(MIC *handle, size_t dev_num)
{
    uint32_t err_code;
    uint64_t jiffy = 0;
    uint64_t tick = 0;
    size_t   idx = 0;
    char A[STR_MAX_SIZE];
    char B[STR_MAX_SIZE];
    char C[STR_MAX_SIZE];
    char D[STR_MAX_SIZE];
    MIC_CORE_UTIL_INFO core_util;

    print_line("!CORE UTIL INFORMATION","%u",dev_num);

    err_code = mic_get_jiffy_counter(handle, &jiffy);
    REPORT_ERROR("mic_get_jiffy_counter", err_code);

    if (MICSDKERR_SUCCESS != err_code)
        print_line("Jiffy counter","Not Available");
    else
        print_line("Jiffy counter","%U",jiffy);

    err_code = mic_get_tick_count(handle, &tick);
    REPORT_ERROR("mic_get_tick_count", err_code);

    if (MICSDKERR_SUCCESS != err_code)
        print_line("Tick counter","Not Available");
    else
        print_line("Tick counter","%U",tick);

    err_code = mic_update_core_util(handle, &core_util);
    REPORT_ERROR("mic_update_core_util", err_code);

    if (MICSDKERR_SUCCESS != err_code) {
        print_line("Core utilization","Not Available");
        return;
    }

    /* Print counters information per core*/
    for (idx = 0; idx < core_util.core_count * core_util.core_threadcount; ++idx) {
        snprintf(A,STR_MAX_SIZE,"idle_counter[%lu]: %lu",  idx,   core_util.counters[e_idle][idx]);
        snprintf(B,STR_MAX_SIZE,"nice_counter[%lu]: %lu",  idx,   core_util.counters[e_nice][idx]);
        snprintf(C,STR_MAX_SIZE,"system_counter[%lu]: %lu",idx,   core_util.counters[e_system][idx]);
        snprintf(D,STR_MAX_SIZE,"user_counter[%lu]: %lu",  idx,   core_util.counters[e_user][idx]);
        print_line("#","%#%#%#%#",A,B,C,D);
    }

    print_line("Idle   counter Sum","%U",core_util.idle_sum);
    print_line("Nice   counter Sum","%U",core_util.nice_sum);
    print_line("System counter Sum","%U",core_util.sys_sum);
    print_line("User   counter Sum","%U",core_util.user_sum);

    /* Clean up memory allocated for the counters */
    mic_free_core_util(&core_util);
}

void show_memory_util_info(MIC *handle, size_t dev_num)
{
    uint32_t err_code;
    MIC_MEMORY_UTIL_INFO mem_util;

    err_code = mic_get_memory_utilization_info(handle, &mem_util);
    REPORT_ERROR("mic_get_memory_utilization_info", err_code);

    if (MICSDKERR_SUCCESS != err_code) {
        print_line("!MEMORY UTIL INFORMATION","%u",dev_num);
        return;
    }

    print_line("!MEMORY UTIL INFORMATION","%u",dev_num);
    print_line("Total",  "%U KB",mem_util.memory_available);
    print_line("Buffers","%U KB",mem_util.memory_buffers);
    print_line("Inuse",  "%U KB",mem_util.memory_inuse);
    print_line("Free",   "%U KB",mem_util.memory_free);
}

void show_power_info(MIC *handle, size_t dev_num)
{
    uint32_t err_code;

    print_line("!POWER INFORMATION","%u",dev_num);

    err_code = mic_get_power_hmrk(handle, &w1_power);
    REPORT_ERROR("mic_get_power_hmrk", err_code);

    if (MICSDKERR_SUCCESS != err_code) {
        print_line("hmrk","Not Available");
        w1_power = 0;
        get_error = 1;
    }
    else
        print_line("hmrk",   "%U uW",w1_power);

    err_code = mic_get_power_lmrk(handle, &w0_power);
    REPORT_ERROR("mic_get_power_lmrk", err_code);

    if (MICSDKERR_SUCCESS != err_code) {
        print_line("lmrk",   "Not Available");
        w0_power = 0;
        get_error = 1;
    }
    else
        print_line("lmrk",   "%U uW",w0_power);

    err_code = mic_get_time_window0(handle, &w0_time);
    REPORT_ERROR("mic_get_time_window0", err_code);

    if (MICSDKERR_SUCCESS != err_code) {
        print_line("time win0",   "Not Available");
        w0_time = 0;
        get_error = 1;
    }
    else
        print_line("time win0",   "%U us",w0_time);

    err_code = mic_get_time_window1(handle, &w1_time);
    REPORT_ERROR("mic_get_time_window1", err_code);

    if (MICSDKERR_SUCCESS != err_code) {
        print_line("time win1",   "Not Available");
        w1_time = 0;
        get_error = 1;
    }
    else
        print_line("time win1",   "%U us",w1_time);
}

void show_power_util_info(MIC *handle, size_t dev_num)
{
    uint32_t     err_code;
    char **      sensor_list = NULL;
    size_t       num_sensors = 0;
    double       value = 0;
    const int    exponent = 0;
    unsigned int sensor = 0;
    char buffer[STR_MAX_SIZE];

    print_line("!POWER UTIL INFORMATION","%u",dev_num);

    err_code = mic_get_power_sensor_list(handle, &sensor_list, &num_sensors);
    REPORT_ERROR("mic_get_power_sensor_list", err_code);

    if (MICSDKERR_SUCCESS != err_code) {
        print_line("Power Sensor List","Not Available");
        cleanup_array(sensor_list, num_sensors);
        return;
    }

    for (sensor = 0; sensor < num_sensors; sensor++) {
        if (sensor_list[sensor] != NULL) {
            snprintf(buffer,STR_MAX_SIZE,"Sensor %s",sensor_list[sensor]);
            err_code = mic_get_power_sensor_value_by_name(handle, sensor_list[sensor], &value, exponent);
            REPORT_ERROR("mic_get_power_sensor_value_by_name", err_code);

            if (MICSDKERR_SUCCESS == err_code)
                print_line(buffer,"%F W",value);
            else
                print_line(buffer,"Not Available");
        }
    }

    cleanup_array(sensor_list, num_sensors);
}

void show_turbo_info(MIC *handle, size_t dev_num)
{
    uint32_t err_code;
    int      active;
    int      valid;
    int      mode;

    print_line("!TURBO INFO","%u",dev_num);

    err_code = mic_get_turbo_state_valid(handle, &valid);
    REPORT_ERROR("mic_get_turbo_state_Valid", err_code);

    if (MICSDKERR_SUCCESS != err_code) {
        print_line("Turbo Mode Supported","Not supported");
        return;
    }
    else
        print_line("Turbo Mode Supported",valid? "True": "False");

    err_code = mic_get_turbo_mode(handle, &mode);
    REPORT_ERROR("mic_get_turbo_mode", err_code);

    if (MICSDKERR_SUCCESS != err_code) {
        print_line("Turbo Mode Enabled", "Not Available");
        return;
    }
    else
        print_line("Turbo Mode Enabled",mode? "True": "False");

    err_code = mic_get_turbo_state(handle, &active);
    REPORT_ERROR("mic_get_turbo_state", err_code);

    if (MICSDKERR_SUCCESS != err_code) {
        print_line("Turbo Mode Active", "Not Available");
        return;
    }
    else
        print_line("Turbo Mode Active",active? "True": "False");

}

void show_led_info(MIC *handle, size_t dev_num)
{
    uint32_t err_code;

    err_code = mic_get_led_alert(handle, &led_alert);
    REPORT_ERROR("mic_get_led_alert", err_code);

    if (MICSDKERR_SUCCESS != err_code) {
        print_line("!LED INFORMATION","%u Not Available",dev_num);
        get_error = 1;
        return;
    }

    print_line("!LED INFORMATION","%u ",dev_num);
    print_line("LED Alert Set","%s",led_alert? "True": "False");
}

void set_led_alert(MIC *handle, size_t dev_num)
{
    uint32_t err_code;

    //show_led_info(handle, dev_num);
    print_line("!LED INFORMATION BEFORE UPDATE","%u Current Value: %U;  Updating to: %U",dev_num,led_alert, led_alert? 0: 1);

    err_code = mic_set_led_alert(handle, led_alert? 0: 1);
    REPORT_ERROR("mic_set_led_alert", err_code);

    if (MICSDKERR_SUCCESS != err_code) {
         print_line("Set LED Alert","Failed!");
        return;
    }

    /* Wait bit until the value refreshes */
    sleep_p(1);

    print_line("!LED INFORMATION AFTER UPDATE","%u",dev_num);
    show_led_info(handle, dev_num);

    err_code = mic_set_led_alert(handle, led_alert? 0: 1);
    REPORT_ERROR("mic_set_led_alert", err_code);

    if (MICSDKERR_SUCCESS != err_code) {
        print_line("Set Back LED Alert","Failed!");
        return;
    }
}

void set_pthresh_w0(MIC *handle, size_t dev_num)
{
    uint32_t err_code;
    uint32_t power_change = w0_power + 5000000; // current lmrk + 5W (expresed in uW)
    uint32_t time_change  = w0_time  +  500000; // current w0 time + 500 milliseconds (expressed in ms)

    print_line("!POWER INFORMATION BEFORE UPDATE","%u Current values: P0: %U; T0: %U. Updating to: P0: %U; T0: %U",
            dev_num, w0_power, w0_time, power_change, time_change);

    err_code =  mic_set_power_limit0(handle, power_change, time_change);
    REPORT_ERROR("mic_set_power_limit0", err_code);

    if (MICSDKERR_SUCCESS != err_code) {
        print_line("Set Power Limit 0","Failed!");
        return;
    }

    /* Wait bit until the value refreshes */
    sleep_p(1);

    /* Save previous values */
    power_change = w0_power;
    time_change  = w0_time;

    print_line("!POWER INFORMATION AFTER UPDATE","%u",dev_num);
    show_power_info(handle, dev_num);

    err_code =  mic_set_power_limit0(handle, power_change, time_change);
    REPORT_ERROR("mic_set_power_limit0", err_code);

    if (MICSDKERR_SUCCESS != err_code) {
        print_line("Set Back Power Limit 0","Failed!");
        return;
    }
}

void set_pthresh_w1(MIC *handle, size_t dev_num)
{
    uint32_t err_code;
    uint32_t power_change = w1_power - 5000000; // current hmrk - 5W (expresed in uW)
    uint32_t time_change  = w1_time  -    1000; // current w1 time - 1 milliseconds (expressed in ms)

    print_line("!POWER INFORMATION BEFORE UPDATE","%u Current values: P1: %U; T1: %U. Updating to: P1: %U; T1: %U",
            dev_num, w1_power, w1_time, power_change, time_change);

    err_code =  mic_set_power_limit1(handle, power_change, time_change);
    REPORT_ERROR("mic_set_power_limit1", err_code);

    if (MICSDKERR_SUCCESS != err_code) {
        print_line("Set Power Limit 1","Failed!");
        return;
    }

    /* Wait bit until the value refreshes */
    sleep_p(1);

    /* Save previous values */
    power_change = w1_power;
    time_change  = w1_time;

    print_line("!POWER INFORMATION AFTER UPDATE","%u",dev_num);
    show_power_info(handle, dev_num);

    err_code =  mic_set_power_limit1(handle, power_change, time_change);
    REPORT_ERROR("mic_set_power_limit1", err_code);

    if (MICSDKERR_SUCCESS != err_code) {
        print_line("Set Back Power Limit 1","Failed!");
        return;
    }
}

void cleanup_array(char **array, size_t num_ele)
{
    mic_free_sensor_list(array, num_ele);
}
