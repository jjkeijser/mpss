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

#ifndef MICLIB_INCLUDE_MICLIB_H_
#define MICLIB_INCLUDE_MICLIB_H_

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <errno.h>
#include <limits.h>

#ifdef _WIN32
#include  <stdio.h>
#  ifndef  NAME_MAX
#    define  NAME_MAX  FILENAME_MAX
#  endif
#endif

#ifdef __cplusplus
extern "C" {
#endif
typedef uintptr_t MIC;

enum  DEVICE_STATE  { e_offline=0,
                      e_online,
                      e_ready,
                      e_reset,
                      e_reboot,
                      e_lost,
                      e_booting_fw,
                      e_online_fw,
                      e_error,
                      e_unknown};

typedef struct {
    uint32_t stepping_data;
    uint32_t substepping_data;
    uint16_t model;
    uint16_t model_ext;
    uint16_t family;
    uint16_t family_ext;
    uint16_t type;
    char     stepping[NAME_MAX];
    char     sku[NAME_MAX];
} MIC_PROCESSOR_INFO;

typedef struct {
    uint32_t domain;
    uint32_t bus_no;
    uint32_t link_speed; /*In MT/s*/
    uint32_t link_width;
    uint32_t payload_size;
    uint32_t read_req_size;
    uint32_t access_violation;
    uint32_t class_code;
    uint16_t device_no;
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t subsystem_id;
    uint8_t  revision_id;
} MIC_PCI_CONFIG;

typedef struct {
    uint32_t  duration;
    uint32_t  totalTime;
    uint32_t  eventCount;
    uint32_t  active;
} MIC_THROTTLE_INFO;

typedef struct {
    uint32_t density;
    uint32_t size;
    uint32_t speed;
    uint32_t freq;
    uint32_t volt;
    uint16_t revision;
    uint16_t ecc;
    char     vendor_name[NAME_MAX];
    char     memory_type[NAME_MAX];
    char     memory_tech[NAME_MAX];
} MIC_MEM_INFO;

typedef struct {
    uint32_t memory_free;
    uint32_t memory_inuse;
    uint32_t memory_buffers;
    uint32_t memory_available;
} MIC_MEMORY_UTIL_INFO;

/* Core counters */
enum  COUNTER { e_user=0, e_system=1, e_nice=2, e_idle=3 };

typedef struct {
    uint64_t tick_count;
    uint64_t jiffy_count;
    uint64_t idle_sum;
    uint64_t nice_sum;
    uint64_t sys_sum;
    uint64_t user_sum;
    uint64_t *counters[4]; /* Array of pointers to int array storing counter values for each core.
                              The int array is allocated dynamically whose size is equal to #cores */
    size_t   core_count;
    size_t   core_threadcount;
} MIC_CORE_UTIL_INFO;

#ifdef __cplusplus
}
#endif


#ifdef __cplusplus
extern "C" {
#endif

/* Error handling */
const char *mic_get_error_string(uint32_t mic_error_num);

/* Device mode switch/start/stop */
uint32_t mic_enter_maint_mode(MIC *device);
uint32_t mic_leave_maint_mode(MIC *device);
uint32_t mic_in_maint_mode(MIC *device, int *mode);
uint32_t mic_in_ready_state(MIC *device, int *state);
uint32_t mic_get_state(MIC *device, int *state);

/* Initalization, general access to device */
uint32_t mic_get_ndevices(size_t *ndevices);
MIC *mic_open_device(int device_num, uint32_t *errcode);
MIC *mic_open_device_by_name(const char *device_name, uint32_t *errcode);
uint32_t mic_close_device(MIC *device);

/* General device information */
uint32_t mic_get_post_code(MIC *device, char *postcode, size_t *size);
uint32_t mic_get_device_type(MIC *device, char *device_type, size_t *size);
uint32_t mic_get_device_name(MIC *device, char *device_name, size_t *size);
uint32_t mic_get_silicon_sku(MIC *device, char *value, size_t *size);
uint32_t mic_get_serial_number(MIC *device, char *value, size_t *size);
uint32_t mic_get_uuid(MIC *device, char *uuid, size_t *size);
uint32_t mic_is_device_avail(MIC *device, int *device_avail);

/* Pci configuration */
uint32_t mic_get_pci_config(MIC *device, MIC_PCI_CONFIG *config);

/* Thermal info */
uint32_t mic_get_thermal_sensor_list(MIC *device, char ***sensor_names, size_t *num_sensors);
uint32_t mic_get_thermal_version_by_name(MIC *device, const char *sensor_name, uint32_t *sensor_value);
uint32_t mic_get_thermal_version_by_index(MIC *device, int version_index, uint32_t *sensor_value);

/* Device memory info */
uint32_t mic_get_ecc_mode(MIC *device, uint16_t *mode);
uint32_t mic_get_memory_info(MIC *device, MIC_MEM_INFO *mem_info);

/* Processor info */
uint32_t mic_get_processor_info(MIC *device, MIC_PROCESSOR_INFO *processor);

/* coprocessor OS core info */
uint32_t mic_get_cores_count(MIC *device, uint32_t *count);
uint32_t mic_get_cores_voltage(MIC *device, uint32_t *voltage);
uint32_t mic_get_cores_frequency(MIC *device, uint32_t *frequency);

/* Version info*/
uint32_t mic_get_coprocessor_os_version(MIC *device, char *value, size_t *size);
uint32_t mic_get_flash_version(MIC *device, char *value, size_t *size);
uint32_t mic_get_fsc_strap(MIC *device, char *value, size_t *size);
uint32_t mic_get_me_version(MIC *device, char *value, size_t *size);
uint32_t mic_get_part_number(MIC *device, char *value, size_t *size);
uint32_t mic_get_smc_hwrevision(MIC *device, char *value, size_t *size);
uint32_t mic_get_smc_fwversion(MIC *device, char *value, size_t *size);
uint32_t mic_get_smc_boot_loader_ver(MIC *device, char *value, size_t *size);
uint32_t mic_is_smc_boot_loader_ver_supported(MIC *device, int *supported);

/* Power utilization info */
uint32_t mic_get_power_sensor_list(MIC *device, char ***sensor_names, size_t *num_sensors);
uint32_t mic_get_power_sensor_value_by_name(MIC *device, const char *sensor_name, double *value, int exponent);
uint32_t mic_get_power_sensor_value_by_index(MIC *device, int sensor_index, double *value, int exponent);

/* Power limits */
uint32_t mic_get_power_phys_limit(MIC *device, uint32_t *phys_lim);
uint32_t mic_get_power_hmrk(MIC *device, uint32_t *hmrk);
uint32_t mic_get_power_lmrk(MIC *device, uint32_t *lmrk);
uint32_t mic_get_time_window0(MIC *device, uint32_t *time_window);
uint32_t mic_get_time_window1(MIC *device, uint32_t *time_window);
uint32_t mic_set_power_limit0(MIC *device, uint32_t power, uint32_t time_window);
uint32_t mic_set_power_limit1(MIC *device, uint32_t power, uint32_t time_window);

/* Memory utilization */
uint32_t mic_get_memory_utilization_info(MIC *devHandle, MIC_MEMORY_UTIL_INFO *mem_util);

/* Core utilization */
uint32_t mic_update_core_util(MIC *device, MIC_CORE_UTIL_INFO *core_util);
uint32_t mic_get_jiffy_counter(MIC *device, uint64_t *jiffy);
uint32_t mic_get_num_cores(MIC *device, size_t *num_cores);
uint32_t mic_get_threads_core(MIC *device, size_t *threads_core);
uint32_t mic_get_tick_count(MIC *device, uint64_t *tick_count);

/* Led mode */
uint32_t mic_get_led_alert(MIC *device, uint32_t *led_alert);
uint32_t mic_set_led_alert(MIC *device, uint32_t led_alert);

/* Turbo info */
uint32_t mic_get_turbo_state(MIC *device, int *active);
uint32_t mic_get_turbo_mode(MIC *device, int *mode);
uint32_t mic_get_turbo_state_valid(MIC *device, int *valid);
uint32_t mic_set_turbo_mode(MIC *device, int mode);

/* Throttle state info */
uint32_t mic_get_thermal_throttle_info(MIC *device, MIC_THROTTLE_INFO *info);
uint32_t mic_get_power_throttle_info(MIC *device, MIC_THROTTLE_INFO *info);

/* coprocessor OS power management config */
uint32_t mic_get_cpufreq_mode(MIC *device, int *mode);
uint32_t mic_get_corec6_mode(MIC *device, int *mode);
uint32_t mic_get_pc3_mode(MIC *device, int *mode);
uint32_t mic_get_pc6_mode(MIC *device, int *mode);

/* SMC config */
uint32_t mic_get_smc_persistence_flag(MIC *device, int *persist_flag);
uint32_t mic_set_smc_persistence_flag(MIC *device, int persist_flag);

/* Utility */
uint32_t mic_free_sensor_list(char **sensor_names, size_t num_sensors);
uint32_t mic_free_core_util(MIC_CORE_UTIL_INFO *core_util);

#ifdef __cplusplus
}
#endif
#endif /* MICLIB_INCLUDE_MICLIB_H_ */
