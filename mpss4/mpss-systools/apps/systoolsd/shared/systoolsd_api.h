/*
 * Copyright (c) 2017, Intel Corporation.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
*/

#ifndef SYSTOOLS_SYSTOOLSD_SYSTOOLSDAPI_H_
#define SYSTOOLS_SYSTOOLSD_SYSTOOLSDAPI_H_

#include <stdint.h>

#include <scif.h>

#define SYSTOOLSD_MAJOR_VER 2
#define SYSTOOLSD_MINOR_VER 7

#define SYSTOOLSD_PORT (SCIF_BT_PORT_0)

#define SYSTOOLSDREQ_MAX_DATA_LENGTH 16
#define SMBA_RESTART_WAIT_MS 5000

#define SET_REQUEST_MASK (1 << 7)

enum SystoolsdRequest
{
    //Supported "get" requests
    GET_SYSTOOLSD_INFO = 0x01,
    GET_MEMORY_UTILIZATION,
    GET_DEVICE_INFO,
    GET_POWER_USAGE,
    GET_THERMAL_INFO,
    GET_VOLTAGE_INFO,
    GET_DIAGNOSTICS_INFO,
    GET_FWUPDATE_INFO,
    GET_MEMORY_INFO,
    GET_PROCESSOR_INFO,
    GET_CORES_INFO,
    GET_CORE_USAGE,
    GET_PTHRESH_INFO,
    GET_SMBA_INFO,
    GET_TURBO_INFO,
    READ_SMC_REG,
    MICBIOS_REQUEST,
    //supported "set" requests
    SET_FORCE_THROTTLE = (SET_REQUEST_MASK | 0x01), //deprecated
    SET_PWM_ADDER,
    SET_LED_BLINK,
    SET_PTHRESH_W0,
    SET_PTHRESH_W1,
    SET_TURBO,
    RESTART_SMBA,
    WRITE_SMC_REG
};


//systoolsd errors
enum SystoolsdError
{
    SYSTOOLSD_UNKOWN_ERROR = 0x01,
    SYSTOOLSD_UNSUPPORTED_REQ,
    SYSTOOLSD_INVAL_STRUCT,
    SYSTOOLSD_INVAL_ARGUMENT,
    SYSTOOLSD_TOO_BUSY,
    SYSTOOLSD_INSUFFICIENT_PRIVILEGES,
    SYSTOOLSD_DEVICE_BUSY,
    SYSTOOLSD_RESTART_IN_PROGRESS,
    SYSTOOLSD_SMC_ERROR,
    SYSTOOLSD_IO_ERROR,
    SYSTOOLSD_INTERNAL_ERROR,
    SYSTOOLSD_SCIF_ERROR
};

#pragma pack(push, 1)

struct SystoolsdReq
{
    uint16_t req_type;
    uint16_t length;
    uint16_t card_errno;
    uint32_t extra;
    char data[16];
};

struct MemoryUsageInfo
{
    uint32_t total;
    uint32_t used;
    uint32_t free;
    uint32_t buffers;
    uint32_t cached;
};

struct DeviceInfo
{
    uint32_t card_tdp;
    uint32_t fwu_cap;
    uint32_t cpu_id;
    uint32_t pci_smba;
    uint32_t fw_version;
    uint32_t exe_domain;
    uint32_t sts_selftest;
    uint32_t boot_fw_version;
    uint32_t hw_revision;
    char os_version[64];
    char bios_version[64];
    char bios_release_date[64];
    char uuid[16];
    char part_number[16];
    char manufacture_date[6];
    char serialno[12];
};

struct PowerUsageInfo
{
    uint32_t pwr_pcie;
    uint32_t pwr_2x3;
    uint32_t pwr_2x4;
    uint32_t force_throttle;
    uint32_t avg_power_0;
    uint32_t inst_power;
    uint32_t inst_power_max;
    uint32_t power_vccp;
    uint32_t power_vccu;
    uint32_t power_vccclr;
    uint32_t power_vccmlb;
    uint32_t power_vccd012; // Deprecated
    uint32_t power_vccd345; // Deprecated
    uint32_t power_vccmp;
    uint32_t power_ntb1;
};

struct ThermalInfo
{
    uint32_t temp_cpu;
    uint32_t temp_exhaust;
    uint32_t temp_inlet; // Deprecated
    uint32_t temp_vccp;
    uint32_t temp_vccclr;
    uint32_t temp_vccmp;
    uint32_t temp_mid; // Deprecated
    uint32_t temp_west;
    uint32_t temp_east;
    uint32_t fan_tach;
    uint32_t fan_pwm;
    uint32_t fan_pwm_adder;
    uint32_t tcritical;
    uint32_t tcontrol;
    uint32_t thermal_throttle_duration; // Deprecated
    uint32_t thermal_throttle; // Deprecated
};

struct VoltageInfo
{
    uint32_t voltage_vccp;
    uint32_t voltage_vccu;
    uint32_t voltage_vccclr;
    uint32_t voltage_vccmlb;
    uint32_t voltage_vccp012; // Deprecated
    uint32_t voltage_vccp345; // Deprecated
    uint32_t voltage_vccmp;
    uint32_t voltage_ntb1;
    uint32_t voltage_vccpio;
    uint32_t voltage_vccsfr;
    uint32_t voltage_pch;
    uint32_t voltage_vccmfuse;
    uint32_t voltage_ntb2;
    uint32_t voltage_vpp;
};

struct DiagnosticsInfo
{
    uint32_t led_blink;
};

struct FwUpdateInfo
{
    uint32_t fwu_sts;
    uint32_t fwu_cmd;
};

struct MemoryInfo
{
    uint32_t total_size;
    uint32_t speed;
    uint32_t frequency;
    uint32_t type;
    uint8_t ecc_enabled;
    char manufacturer[64];
    uint16_t voltage; //Deprecated
};

struct ProcessorInfo
{
    uint32_t stepping_id;
    uint16_t model;
    uint16_t family;
    uint16_t type;
    uint8_t threads_per_core;
    char stepping[16];
};

struct CoresInfo
{
    uint32_t num_cores;
    uint32_t cores_freq;
    uint32_t clocks_per_sec;
    uint32_t threads_per_core;
    uint8_t cores_voltage;
};

struct CoreCounters
{
    uint64_t user;
    uint64_t nice;
    uint64_t system;
    uint64_t idle;
    uint64_t total;
};

struct CoreUsageInfo
{
    uint64_t clocks_per_sec;
    uint64_t ticks;
    uint32_t num_cores;
    uint16_t threads_per_core;
    uint32_t frequency;
    CoreCounters sum;
};

struct PowerWindowInfo
{
    uint32_t threshold;
    uint32_t time_window;
};

struct PowerThresholdsInfo
{
    uint32_t max_phys_power;
    uint32_t low_threshold;
    uint32_t hi_threshold;
    struct PowerWindowInfo w0;
    struct PowerWindowInfo w1;
};

struct SmbaInfo
{
    uint8_t is_busy;
    uint32_t ms_remaining;
};

struct TurboInfo
{
    uint8_t enabled;
    uint8_t turbo_pct;
};

struct SystoolsdInfo
{
    uint8_t major_ver;
    uint8_t minor_ver;
};


enum MicBiosCmd
{
    micbios_read = 0,
    micbios_write,
    micbios_change_pass
};

enum MicBiosProperty
{
    mb_cluster = 0x01,
    mb_ecc = 0x02,
    mb_apei_supp = 0x04,
    mb_apei_ffm = 0x08,
    mb_apei_einj = 0x10,
    mb_apei_einjtable = 0x20,
    mb_fwlock = 0x40
};

enum Ecc
{
    ecc_disable = 0,
    ecc_enable,
    ecc_auto_mode,
    ecc_max
};

enum Cluster
{
    all2all = 0,
    snc2,
    snc4,
    hemisphere,
    quadrant,
    auto_mode,
    cluster_max
};

enum APEISupport
{
    supp_disable = 0,
    supp_enable,
    supp_max
};

enum APEIEinj
{
    einj_disable = 0,
    einj_enable,
    einj_max
};

enum APEIFfm
{
    ffm_disable = 0,
    ffm_enable,
    ffm_max
};

enum APEIEinjTable
{
    table_disable = 0,
    table_enable,
    table_max
};

enum Fwlock
{
    fwlock_disable = 0,
    fwlock_enable,
    fwlock_max
};

struct MicBiosRequest
{
    uint8_t cmd;
    uint8_t prop;
    union
    {
        uint64_t value;
        struct
        {
            unsigned char cluster : 4;
            unsigned char ecc : 3;
            unsigned char apei_supp : 2;
            unsigned char apei_einj : 2;
            unsigned char apei_ffm : 2;
            unsigned char appei_einjtable : 2;
            unsigned char fwlock : 2;
        } settings;
    };
};


#pragma pack(pop)

#endif
