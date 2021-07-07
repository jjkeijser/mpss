/*
 * Copyright 2010-2017 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * Disclaimer: The codes contained in these modules may be specific to
 * the Intel Software Development Platform codenamed Knights Ferry,
 * and the Intel product codenamed Knights Corner, and are not backward
 * compatible with other Intel products. Additionally, Intel will NOT
 * support the codes or instruction set in future products.
 *
 * Intel offers no warranty of any kind regarding the code. This code is
 * licensed on an "AS IS" basis and Intel is not obligated to provide
 * any support, assistance, installation, training, or other services
 * of any kind. Intel is also not obligated to provide any updates,
 * enhancements or extensions. Intel specifically disclaims any warranty
 * of merchantability, non-infringement, fitness for any particular
 * purpose, and any other warranty.
 *
 * Further, Intel disclaims all liability of any kind, including but
 * not limited to liability for infringement of any proprietary rights,
 * relating to the use of the code, even if Intel is notified of the
 * possibility of such liability. Except as expressly stated in an Intel
 * license agreement provided with this code and agreed upon with Intel,
 * no license, express or implied, by estoppel or otherwise, to any
 * intellectual property rights is granted herein.
 */

/* common power management specific header defines for host and card */

#include "io_interface.h"

#if !defined(__MIC_PM_H)
#define __MIC_PM_H

#define PC6_TIMER 10

#define IOCTL_PM_SendIoctl _IOC(_IOC_READ|_IOC_WRITE, 'l', 2, 0)

#define MAX_HW_IDLE_WAIT_COUNT 100
#define PC3_EXIT_WAIT_COUNT 1000
#define PM_SEND_MODE SCIF_SEND_BLOCK
#define PM_RECV_MODE SCIF_RECV_BLOCK
#define SET_VID_RETRY_COUNT 3

#define PM_NODE_MAGIC_BIT 31
#define PM_NODE_IDLE (1 << PM_NODE_MAGIC_BIT)

#define PM_PRINT(fmt, ...) printk("[ %s : %d ]:"fmt, \
		__func__, __LINE__, ##__VA_ARGS__)

#define PM_DEBUG(fmt, ...) pr_debug("[ %s : %d ]:"fmt, \
		__func__, __LINE__, ##__VA_ARGS__)

#define PM_ENTRY PM_DEBUG("==> %s\n", __func__)
#define PM_EXIT PM_DEBUG("<== %s\n", __func__)
#define PM_MAJOR_VERSION 1
#define PM_MINOR_VERSION 0


typedef enum _PM_MESSAGE {
	PM_MESSAGE_PC3READY,
	PM_MESSAGE_OPEN,
	PM_MESSAGE_OPEN_ACK,
	PM_MESSAGE_CLOSE,
	PM_MESSAGE_CLOSE_ACK,
	PM_MESSAGE_TEST,
	PM_MESSAGE_MAX,
} PM_MESSAGE;

typedef enum _PM_IDLE_STATE {
	PM_IDLE_STATE_PC0,
	PM_IDLE_STATE_PC3_READY,
	PM_IDLE_STATE_PC3,
	PM_IDLE_STATE_PC6,
	PM_IDLE_STATE_LOST,
	PM_IDLE_STATE_MAX,
} PM_IDLE_STATE;

#ifndef _MIC_SCIF_
typedef enum {
	IOCTL_pm_send,
	IOCTL_pm_recv,
	IOCTL_pm_send_check,
	IOCTL_pm_get_idle_state,
	IOCTL_pm_exit_idle_state,
	// For emulator testing
	IOCTL_pmemu_pc3_entry,
	IOCTL_pmemu_pc3_exit,
	IOCTL_pmemu_pc6_entry,
	IOCTL_pmemu_pc6_exit,
	IOCTL_pmemu_dpc3_entry,
	IOCTL_pmemu_dpc3_exit,
	IOCTL_get_dependency_graph,
    IOCTL_get_dependency_set,
	IOCTL_pm_toggle_connection,
	IOCTL_pm_idlestate_exit,
	IOCTL_pm_enable_dpc3_testing,
	IOCTL_pm_device_restart,
} PM_IOCTL_TYPE;

struct pm_ioctl_header {
	uint32_t node;
	PM_IOCTL_TYPE opcode;
	uint64_t arglen;
};
#define PM_TEST_MSG_BODY "PM Test Message"
#endif

//Generic PM Header. Has message type and length of message.
typedef struct _pm_msg_header {
	PM_MESSAGE opcode;
	uint32_t len;
} pm_msg_header;

typedef struct _pm_msg_unit_test
{
	pm_msg_header header;
	void * buf;
} pm_msg_unit_test;

typedef struct _pm_version
{
	uint16_t major_version;
	uint16_t minor_version;

} pm_version;

typedef struct _pm_msg_pm_options
{
	uint8_t pc3_enabled;
	uint8_t pc6_enabled;
	pm_version version;
} pm_msg_pm_options;

#ifndef _MIC_SCIF_
// PM IOCTLs
struct pm_scif_send {
	struct pm_ioctl_header header;
	uint32_t length;
	void *buf;
};

struct pm_scif_recv {
	struct pm_ioctl_header header;
	uint32_t length;
	void *buf;
};

struct pm_scif_send_check {
	struct pm_ioctl_header header;
	uint32_t length;
	void *buf;
};

typedef struct pm_get_idle_state {
	struct pm_ioctl_header header;
	PM_IDLE_STATE *idle_state;
} pm_get_idle_state_t;

typedef struct pm_exit_idle_state {
	struct pm_ioctl_header header;
	PM_IDLE_STATE idle_state;
}pm_exit_idlestate_t;

typedef struct dependency_graph {
	struct pm_ioctl_header header;
	uint32_t** depmtrx;
} dependency_graph_t;

struct io_dependency_set {
    struct pm_ioctl_header header;
    int is_active_set;
    uint64_t dep_set;
};

struct io_enable_dpc3_test {
	struct pm_ioctl_header header;
	uint32_t enable_test;
	uint32_t state;
};

typedef struct _pm_status {
	uint32_t hoststate_reg;
	uint32_t cardstate_reg;
	uint32_t c3waketimer_reg;
	uint32_t pcucontrol_reg;
	uint32_t uos_pcucontrol_reg;
	uint32_t corevolt_reg;
	uint32_t gpmctrl_reg;
	uint32_t idle_state;
	uint32_t board_id;
} pm_status_t;

typedef struct _test_msg_ctrl {
	uint32_t action;
} test_msg_ctrl_t;

typedef struct _connection_info {
	int32_t conn_state;
	int32_t local_port;
	int32_t local_node;
	int32_t remote_port;
	int32_t remote_node;
	int32_t num_messages_queued;
} connection_info_t;

#endif //_MIC_SCIF_

#if defined(CONFIG_MK1OM)

#define SBOX_SVID_CONTROL 0x00004110
#define SBOX_PCU_CONTROL 0x00004114
#define SBOX_HOST_PMSTATE 0x00004118
#define SBOX_UOS_PMSTATE 0x0000411c
#define SBOX_C3WAKEUP_TIMER 0x00004120
#define GBOX_PM_CTRL 0x0000413C
#define SBOX_UOS_PCUCONTROL 0x0000412C

#elif defined(CONFIG_ML1OM) || defined(WINDOWS)

#define DBOX_SWFOX1 0x00002414
#define DBOX_SWFOX2 0x00002418
#define DBOX_SWFOX3 0x0000241C
#define DBOX_SWFOX4 0x00002420
#define DBOX_SWFOX5 0x00002424
#define DBOX_SWFOX6 0x00002428
#define DBOX_SWFOX7 0x0000242C
#define DBOX_SWF0X8 0x00002430

#define SBOX_SVID_CONTROL DBOX_SWFOX1
#define SBOX_PCU_CONTROL DBOX_SWFOX2
#define SBOX_HOST_PMSTATE DBOX_SWFOX3
#define SBOX_UOS_PMSTATE DBOX_SWFOX4
#define SBOX_C3WAKEUP_TIMER DBOX_SWFOX5
#define GBOX_PM_CTRL DBOX_SWFOX6
#define SBOX_UOS_PCUCONTROL DBOX_SWFOX7

#else
#error Neither CONFIG_ML1OM nor CONFIG_MK1OM defined
#endif

#define SBOX_SVIDCTRL_SVID_DOUT(x)                        ((x) & 0x1ff)
#define SBOX_SVIDCTRL_SVID_DOUT_BITS(x)                   ((x) & 0x1ff)
#define SBOX_SVIDCTRL_SVID_CMD(x)                         (((x) >> 9) & 0x1ff)
#define SBOX_SVIDCTRL_SVID_CMD_BITS(x)                    (((x) & 0x1ff) << 9)
#define SBOX_SVIDCTRL_SVID_DIN(x)                         (((x) >> 18) & 0x3ff)
#define SBOX_SVIDCTRL_SVID_ERROR(x)                       (((x) >> 29) & 0x1)
#define SBOX_SVIDCTRL_SVID_IDLE(x)                        (((x) >> 30) & 0x1)
#define SBOX_SVIDCTRL_CMD_START(x)                        (((x) >> 31) & 0x1)
#define SBOX_SVIDCTRL_CMD_START_BITS(x)                   (((x) & 0x1) << 31)
// This is not a register field, but we need to check these bits to determine parity error
#define SBOX_SVIDCTRL_ACK1ACK0(x)                         (((x) >> 27) & 0x11)

#define SBOX_PCUCTRL_ENABLE_MCLK_SHUTDWN(x)               ((x) & 0x1)
#define SBOX_PCUCTRL_ENABLE_MCLK_SHUTDWN_BITS(x)          ((x) & 0x1)
#define SBOX_PCUCTRL_RING_ACTIVE(x)                       (((x) >> 2) & 0x1)
#define SBOX_PCUCTRL_RING_ACTIVE_BITS(x)                  (((x) & 0x1) << 2)
#define SBOX_PCUCTRL_PREVENT_AUTOC3_EXIT(x)               (((x) >> 3) & 0x1)
#define SBOX_PCUCTRL_PREVENT_AUTOC3_EXIT_BITS(x)          (((x) & 0x1) << 3)
#define SBOX_PCUCTRL_PWRGOOD_MASK(x)                      (((x) >> 17) & 0x1)
#define SBOX_PCUCTRL_PWRGOOD_MASK_BITS(x)                 (((x) & 0x1) << 17)
#define SBOX_PCUCTRL_MCLK_PLL_LCK(x)			 		  (((x) >> 16) & 0x1)
#define SBOX_THERMAL_STS_ALERT_LOG(x)					  (((x) >> 3) & 0x1)
#define SBOX_THERMAL_STS_ALERT_LOG_BITS(x)				  (((x) & 0x1) << 3)

// used by host to communicate card idle state to uos
#define SBOX_HPMSTATE_STATUS(x)                           ((x) & 0xff)
#define SBOX_HPMSTATE_STATUS_BITS(x)                      ((x) & 0xff)
#define SBOX_HPMSTATE_MINVID(x)                           (((x) >> 8) & 0xff)
#define SBOX_HPMSTATE_TDPVID(x)                           (((x) >> 16) & 0xff)
// used by uos to communicate card idle state to host
#define SBOX_UPMSTATE_STATUS(x)                           ((x) & 0xff)
#define SBOX_UPMSTATE_STATUS_BITS(x)			  ((x) & 0xff)

#define SBOX_C3WAKEUP_TIME(x)                             ((x) & 0xffff)
#define SBOX_C3WAKEUP_TIME_BITS(x)                        ((x) & 0xffff)

#define IN_PCKGC6_BITS(x)								  (((x) & 0x1) << 1)
#define KNC_SVID_ADDR	0
#define	KNC_SETVID_FAST	1
#define	KNC_SETVID_SLOW	2
#define KNC_SETVID_ATTEMPTS 50


typedef union _sbox_pcu_ctrl {
	uint32_t value;
	struct {
		uint32_t enable_mclk_pl_shutdown	:1;
		uint32_t mclk_enabled				:1;
		uint32_t ring_active				:1;
		uint32_t prevent_auto_c3_exit		:1;
		uint32_t ghost_active				:1;
		uint32_t tcu_active					:1;
		uint32_t itp_scllk_gate_disable		:1;
		uint32_t itp_pkg_c3_disable			:1;
		uint32_t scratch					:1;
		uint32_t unallocated_1				:1;
		uint32_t sysint_active				:1;
		uint32_t sclk_grid_off_disable		:1;
		uint32_t icc_dvo_ssc_cg_enable		:1;
		uint32_t icc_core_ref_clk_cg_enable :1;
		uint32_t icc_gddr_ssc_cg_enable		:1;
		uint32_t icc_pll_disable			:1;
		uint32_t mclk_pll_lock				:1;
		uint32_t grpB_pwrgood_mask			:1;
		uint32_t unallocated_2				:14;
	} bits;

} sbox_pcu_ctrl_t;

typedef union _sbox_host_pm_state {
	uint32_t value;
	struct {
		uint32_t host_pm_state			:7;
		uint32_t abort_not_processed		:1;
		uint32_t min_vid			:8;
		uint32_t tdp_vid			:8;
		uint32_t unallocated			:8;
	} bits;

} sbox_host_pm_state_t;

typedef union _sbox_uos_pm_state {
	uint32_t value;
	struct {
		uint32_t uos_pm_state				:8;
		uint32_t unallocated				:24;
	}bits;

} sbox_uos_pm_state_t;

typedef union _c3_wakeup_timer {
	uint32_t value;
	struct {
		uint32_t c3_wake_time				:16;
		uint32_t unallocated_1				:1;
		uint32_t c3_wake_timeout			:1;
		uint32_t unallocated_2				:14;
	} bits;

} c3_wakeup_timer_t;

typedef union _sbox_svid_control {
	uint32_t value;
	struct {
		uint32_t svid_dout					:9;
		uint32_t svid_cmd					:9;
		uint32_t svid_din					:11;
		uint32_t svid_error					:1;
		uint32_t svid_idle					:1;
		uint32_t cmd_start					:1;
	} bits;

} sbox_svid_control;

typedef union _gbox_pm_control {
	uint32_t value;
	struct {
		uint32_t c6_disable					:1;
		uint32_t in_pckgc6					:1;
		uint32_t gbox_inM3					:2;
		uint32_t unallocated				:28;
	} bits;

} gbox_pm_control;

typedef union _sbox_thermal_sts_interrupt {
	uint32_t value;
	struct {
		uint32_t mclk_ratio_status			:1;
		uint32_t mclk_ratio_log				:1;
		uint32_t alert_status				:1;
		uint32_t alert_log					:1;
		uint32_t gpu_hot_status				:1;
		uint32_t gpu_hot_log				:1;
		uint32_t pwr_alert_status			:1;
		uint32_t pwr_alert_log				:1;
		uint32_t pmu_status					:1;
		uint32_t pmu_log					:1;
		uint32_t etc_freeze					:1;
		uint32_t unallocated				:21;
	}bits;

} sbox_thermal_sts_interrupt;

typedef union _sboxUosPcucontrolReg
{
	uint32_t value;
	struct
	{
		uint32_t c3_wakeuptimer_enable		:1;
		uint32_t enable_mclk_pll_shutdown	:1;
		uint32_t spi_clk_disable			:1;
		uint32_t unallocated				:29;
	} bits;

} sbox_uos_pcu_ctrl_t;

typedef union _sboxCorefreqReg
{
	uint32_t value;
	struct
	{
		uint32_t ratio					   :12; // bit 0-11 Ratio
		uint32_t rsvd0					   : 3; // bit 12-14
		uint32_t fuseratio				   : 1; // bit 15 If overclocking is enabled, setting this bit will default the goal ratio to the fuse value.
		uint32_t asyncmode				   : 1; // bit 16 Async Mode Bit 16, Reserved Bits 20:17 used to be ExtClkFreq,
		uint32_t rsvd1					   : 9; // bit 17-25
		uint32_t ratiostep				   : 4; // bit 26-29 Power throttle ratio-step
		uint32_t jumpratio				   : 1; // bit 30 Power throttle jump at once
		uint32_t booted					   : 1; // bit 31 Booted: This bit selects between the default MCLK Ratio (600MHz) and the programmable MCLK ratio. 0=default 1=programmable.
	} bits;

} sbox_core_freq_t;

typedef union _sboxCoreVoltReg
{
	uint32_t value;
	struct
	{
		uint32_t vid        				:8;
		uint32_t unallocated				:24;
	} bits;

} sbox_core_volt_t;

typedef enum _PM_CONNECTION_STATE {
	PM_CONNECTING,
	PM_CONNECTED,
	PM_DISCONNECTING,
	PM_DISCONNECTED
} PM_CONNECTION_STATE;

#endif //__MIC_PM_H
