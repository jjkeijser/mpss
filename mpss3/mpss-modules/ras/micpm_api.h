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

/*
 * Definition of the PM interface to the RAS module.
 *
 * Throttle event interface is similar to the MC interface.
 * If a connection is made to MR_TTL_PORT then event records
 * will be sent to the host. Events are sent non-blocking,
 * so if the SCIF buffer runs full, events are dropped until
 * the block disappear (or the session is closed).
 * 
 * Queries are technically implemented as an extension to the 
 * MT interface, and thus are accessible from the host.
 * Except for the risk of conflicting commands written to the
 * two power limit registers, there are no side effects from
 * host side access via SCIF.
 *
 * Currently there are no plans to expose this in SysFs nodes.
 * These routines are just wrappers for read/write access to
 * SMC registers. No precious IP here.
 */

#ifndef _MICPM_API_H_
#define _MICPM_API_H_		1

#ifdef __cplusplus
extern "C" {	/* C++ guard */
#endif


/*
**
** Configuration manifests
**
*/

#pragma pack(push, 4)		/* Weird Windos requirement */


/*
 * Throttle events are reported on this port. Only one consumer can
 * connect in order to be notified about PM throttling events.
 */

#define	MR_TTL_PORT	SCIF_RAS_PORT_2


/*
 * Throttle events are provided in raw form, i.e. with as
 * little processing on the card side as possible.
 * For nicer throttle state display, use MT command MR_REQ_TTL.
 *
 * To compensate for the chance of lost events, the full
 * throttle state is transfered in one byte on every message:
 *
 *  Bit#	Content
 *    0		Power trottle state changed
 *    1		New/Current power throttle state
 *    2		Thermal throttle state changed
 *    3		New/Current thermal throttle state
 *    4		Power alert state changed
 *    5		New/Current power alert state
 *
 * By definition, when power and thermal throttle are in effect
 * the KnC is forced to run at reduced speed (600 MHz or so) and
 * with lower operating voltages, i.e. software is not in control.
 * During power alerts the KnC is consuming more power than PLim1
 * and the PM module can reduce speed and/or voltages to reduce
 * power consumption. If power consumption goes beyond PLim0, the
 * hardware (SMC really) will start real power throttles.
 * In effect time spent in power throttle, will also be counted
 * as being in the power alert state. See MT request MR_REQ_TTL. 
 */

#define	PM_PWR_TTL_CHG	(1 << 0)	/* Power throttle change */
#define	PM_PWR_TTL	(1 << 1)	/* Power Trottle state */
#define	PM_TRM_TTL_CHG	(1 << 2)	/* Thermal throttle change */
#define	PM_TRM_TTL	(1 << 3)	/* Thermal Trottle state */
#define	PM_ALRT_TTL_CHG	(1 << 4)	/* Power alert change */
#define	PM_ALRT_TTL	(1 << 5)	/* Power alert state */

typedef struct ttl_info {
  uint8_t	upd;			/* Throttle state update */
  uint8_t	die;			/* Die temperature (as per SBOX) */
} TtlInfo;



/*
 * PM specific MT opcodes
 * Leave one empty slot in callout table between
 * this and the official MT API entries.
 */

#define PM_REQ_PL0	(MR_REQ_MAX + 2)   /* Get power limit 0 */
#define PM_SET_PL0	(MR_REQ_MAX + 3)   /* Set power limit 0 */
#define PM_REQ_PL1	(MR_REQ_MAX + 4)   /* Get power limit 1 */
#define PM_SET_PL1	(MR_REQ_MAX + 5)   /* Set power limit 1 */
#define PM_REQ_PAVG	(MR_REQ_MAX + 6)   /* Get average power */
#define PM_REQ_PTTL	(MR_REQ_MAX + 7)   /* Get power throttle */
#define PM_REQ_VOLT	(MR_REQ_MAX + 8)   /* Get voltage */
#define PM_REQ_TEMP	(MR_REQ_MAX + 9)   /* Get temperatures */
#define PM_REQ_TACH	(MR_REQ_MAX + 10)  /* Get fan tachometer */
#define PM_REQ_TTTL	(MR_REQ_MAX + 11)  /* Get thermal throttle */
#define PM_REQ_FTTL	(MR_REQ_MAX + 12)  /* Get force throttle */
#define PM_SET_FTTL	(MR_REQ_MAX + 13)  /* Set force throttle */
#define PM_REQ_MAX	PM_SET_FTTL	   /* Last PM command */


/*
**
** Response container structures below.
**
*/


/*
 * Get power limit
 * REQ_PL{0/1} notes:
 *  - Only power limit 0 have a guard band defined.
 */
typedef struct pm_rsp_plim {
  uint32_t	pwr_lim;		/* Power limit, in Watt */
  uint32_t	time_win;		/* Time Window, in mSec */
  uint32_t	guard_band;		/* Guard band, in Watt */
} PmRspPlim;


/*
 * Set power limit
 */
typedef struct pm_cmd_plim {
  uint32_t	pwr_lim;		/* Power limit, in Watt */
  uint32_t	time_win;		/* Time Window, in mSec */
} PmCmdPlim;


/*
 * Get average power
 * REQ_PAVG notes:
 *  - Both values are subject to availability in the SMC.
 *    The top two status bit of each SMC register is provided
 *    separately (and stripped from the read value). Decode as
 *	00        Data OK
 *	01        Lower threshold reached
 *	10        Upper threshold reached 
 *	11        Data unavailable
 *    It is unclear if data is good if outside thresholds.
 */
typedef struct pm_rsp_pavg {
  uint8_t	stat_0;			/* Status bits for window 0 */
  uint8_t	stat_1;			/* Status bits for window 1 */
  uint32_t	pwr_0;			/* Average over window 0, in Watt */
  uint32_t	pwr_1;			/* Average over window 1, in Watt */
} PmRspPavg;


/*
 * Get Power throttle status
 * REQ_PTTL notes:
 *  - Duration value is subject to availability in the SMC.
 *    The top two status bit of this SMC register is provided
 *    separately (and stripped from the read value). Decode as
 *	00        Data OK
 *	01        Reserved
 *	10        Reserved
 *	11        Data unavailable
 */
typedef struct pm_rsp_pttl {
  uint8_t	pwr_ttl;		/* Power throttle asserted */
  uint8_t	stat_dur;		/* Status bits duration */
  uint32_t	duration;		/* Power throttle duration, in mSec */
} PmRspPttl;


/*
 * Get voltages
 * REQ_VOLT notes:
 *  - VR values are subject to availability in the SMC.
 *    The top two status bit of each SMC register is provided
 *    separately (and stripped from the read value). Decode as
 *	00        Data OK
 *	01        Lower threshold reached
 *	10        Upper threshold reached 
 *	11        Data unavailable
 *    It is unclear if data is good if outside thresholds.
 */
typedef struct pm_rsp_volt {
  uint8_t	stat_vccp;		/* Status bits for Vddc */
  uint8_t	stat_vddg;		/* Status bits for Vddg */
  uint8_t	stat_vddq;		/* Status bits for Vddq */
  uint32_t	vccp;			/* Vccp, in mV */
  uint32_t	vddg;			/* Vddg, in mV */
  uint32_t	vddq;			/* Vddq, in mV */
} PmRspVolt;


/*
 * Get temperatures
 * REQ_TEMP notes:
 *  - These values are subject to availability in the SMC.
 *    The top two status bit of each SMC register is provided
 *    separately (and stripped from the read value). Decode as
 *	00        Data OK
 *	01        Lower threshold reached
 *	10        Upper threshold reached 
 *	11        Data unavailable
 *    It is unclear if data is good if outside thresholds.
 */
typedef struct pm_rsp_temp {
  uint8_t	stat_cpu;		/* Status bits for Tcpu */
  uint8_t	stat_vccp;		/* Status bits for Tvddc */
  uint8_t	stat_vddg;		/* Status bits for Tvddg */
  uint8_t	stat_vddq;		/* Status bits for Tvddq */
  uint32_t	cpu;			/* CPU temp, in C */
  uint32_t	vccp;			/* Vccp VR temp, in C */
  uint32_t	vddg;			/* Vddg VR temp, in C */
  uint32_t	vddq;			/* Vddq VR temp, in C */
} PmRspTemp;


/*
 * Get fan tachometer
 * REQ_TACH notes:
 *  - These values are subject to availability in the SMC.
 *    The top two status bit of each SMC register is provided
 *    separately (and stripped from the read value). Decode as
 *	00        Data OK
 *	01        Lower threshold reached (tach only)
 *	10        Reserved
 *	11        Data unavailable
 *    It is unclear if data is good if outside thresholds.
 */
typedef struct pm_rsp_tach {
  uint8_t	stat_pwm;		/* Status bits for PWM */
  uint8_t	stat_tach;		/* Status bits for TACH */
  uint32_t	fan_pwm;		/* Fan power, in % */
  uint32_t	fan_tach;		/* Fan speed, in RPM */
} PmRspTach;


/*
 * Get thermal throttle status
 * REQ_THRM notes:
 *  - Duration value is subject to availability in the SMC.
 *    The top two status bit of this SMC register is provided
 *    separately (and stripped from the read value). Decode as
 *	00        Data OK
 *	01        Reserved
 *	10        Reserved
 *	11        Data unavailable
 */
typedef struct pm_rsp_tttl {
  uint8_t	thrm_ttl;		/* Power throttle asserted */
  uint8_t	stat_dur;		/* Status bits duration */
  uint32_t	duration;		/* Thermal throttle duration, in mSec */
} PmRspTttl;


/*
 * Get/Set force trottle control
 */
typedef struct pm_rsp_fttl {
  uint8_t	forced;			/* Forced power throttle asserted */
} PmRspFttl;


#pragma pack(pop)		/* Restore to sane conditions */

#ifdef __cplusplus
}	/* C++ guard */
#endif

#endif	/* Recursion block */
