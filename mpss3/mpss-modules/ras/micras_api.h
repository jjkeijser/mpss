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
 * Definition of the public RAS Monitoring Thread interface.
 * Access to RAS features are expected from SCIF and through
 * nodes under '/sys/class/micras'. Both interfaces ends up
 * in the same code and thus present the exact same data.
 *
 * Some information that are available elsewhere through standard
 * Linux mechanism are included in this API, though things like
 * process status (/proc/<pid>/stat), cpu status (/proc/stat),
 * and memory status (/proc/vmstat) are better from the source.
 */

#ifndef _MICRAS_API_H_
#define _MICRAS_API_H_		1

#ifdef __cplusplus
extern "C" {	/* C++ guard */
#endif

/*
**
** Configuration manifests
**
*/

#pragma pack(push, 4)		/* Windos requirement */


/*
 * RAS module version info: M.NP
 */

#define	RAS_MAJOR	"1"
#define	RAS_MINOR	"0"
#define	RAS_PATCH	" "
#define RAS_VER		RAS_MAJOR "." RAS_MINOR  RAS_PATCH


/*
 * RAS services in uOS kernel listens on this port for incoming queries.
 * Consumers may establish multiple connections to this port, though no
 * guarantee on connection processing order will be given. Transactions
 * on a connection will be processed and replied to in order recieved.
 */

#define	MR_MON_PORT	SCIF_RAS_PORT_0
#define	MR_SCIF_MAX	32


/*
 * Some array max sizes.
 * These may be replaced by system wide constants
 * if they become available in the source tree.
 */

#define MR_VERS_LEN	120	/* Version string lengths */
#define MR_GUID_LEN	16	/* Global unique ID length (bytes) */
#define MR_SENO_LEN	12	/* Serial number length (bytes) */
#define MR_PVER_LEN	8	/* API version string length */
#define MR_PTAB_LEN	64	/* PM freq/volt pairs */
#define MR_DIES_LEN	9	/* Die temperatures */
#define MR_BRDS_LEN	4	/* Board temp sensors */
#define MR_GVND_LEN	16	/* GDDR vendor string length */
#define MR_CORE_LEN	62	/* Max number of CPU cores */


/*
** Transaction header for requests and responses is a fixed size
** record followed by an optional variable length data block.
**
** Fields usage:
**      cmd	[15]	data field is error record
**	cmd	[14]	response to opcode
**      cmd	[13:0]	opcode
**	len		length of payload
**	parm		command parameter
**	stamp		host side cookie, performance monitoring
**	spent		processing time, performance monitoring
**
** Command codes:
** Codes that directly relate to cores may set the 'parm' field to a
** non-zero value to address one core (base 1) instead of them all.
**
*/

typedef struct mr_hdr {
	uint16_t	cmd;	/* Command field */
	uint16_t	len;	/* Size of data payload */
	uint32_t	parm;	/* Parameter field */	
	uint64_t	stamp;	/* Time stamp of 'send' (set by host) */
	uint64_t	spent;	/* Time used on response (rdtsc delta) */
} MrHdr;

#define MR_RESP		(1 << 14)	/* Response bit */
#define MR_ERROR	(1 << 15)	/* Error bit */
#define MR_OP_MASK	(MR_RESP - 1)	/* Opcode mask */

#define MR_REQ_HWINF	1		/* Get hardware info */
#define	MR_REQ_VERS	2		/* Get version strings */
#define	MR_REQ_CFREQ	3		/* Get core frequencies */
#define	MR_SET_CFREQ	4		/* Set core frequency */
#define MR_REQ_CVOLT	5		/* Get core voltages */
#define MR_SET_CVOLT	6		/* Set core voltage */
#define MR_REQ_PWR	7		/* Get power metrics */
#define MR_REQ_PLIM	8		/* Get power limit */
#define MR_SET_PLIM	9		/* Set power limit */
#define MR_REQ_CLST	10		/* Get core list */
#define MR_ENB_CORE	11		/* Enable core */
#define MR_DIS_CORE	12		/* Disable core */
#define MR_REQ_GDDR	13		/* Get GDDR device info */
#define MR_REQ_GFREQ	14		/* Get GDDR frequencies */
#define MR_SET_GFREQ	15		/* Set GDDR frequency */
#define MR_REQ_GVOLT	16		/* Get GDDR voltages */
#define MR_SET_GVOLT	17		/* Set GDDR voltage */
#define MR_REQ_TEMP	18		/* Get board temperatures */
#define MR_REQ_FAN	19		/* Get fan status */
#define MR_SET_FAN	20		/* Set fan power */
#define MR_REQ_ECC	21		/* Get ECC mode */
#define MR_SET_ECC	22		/* Set ECC mode */
#define MR_REQ_TRC	23		/* Get debug trace level */
#define MR_SET_TRC	24		/* Set debug trace level */
#define MR_REQ_TRBO	25		/* Get turbo mode status */
#define MR_SET_TRBO	26		/* Set turbo mode status */
#define MR_REQ_OCLK	27		/* Get overclocking status */
#define MR_SET_OCLK	28		/* Set overclocking status */
#define MR_REQ_CUTL	29		/* Get core utilization */
#define MR_REQ_MEM	30		/* Get memory utilization */
#define MR_REQ_OS	31		/* Get OS status & process list */
#define MR_REQ_PROC	32		/* Get process details */
#define MR_REQ_THRD	33		/* Get thread details */
#define MR_REQ_PVER	34		/* Get API version */
#define MR_CMD_PKILL	35		/* Kill process */
#define MR_CMD_UKILL	36		/* Kill processes owned by user */
#define MR_GET_SMC	37		/* Get SMC register */
#define MR_SET_SMC	38		/* Write SMC register */
#define MR_REQ_PMCFG	39		/* Get PM config mode */
#define MR_REQ_LED	40		/* Get LED mode */
#define MR_SET_LED	41		/* Set LED mode */
#define MR_REQ_PROCHOT	42		/* Get PROC hot trigger */
#define MR_SET_PROCHOT	43		/* Set PROC hot trigger */
#define MR_REQ_GPUHOT	42		/* Get GPU hot trigger */
#define MR_SET_GPUHOT	43		/* Set GPU hot trigger */
#define MR_REQ_PWRALT	44		/* Get power alert trigger */
#define MR_SET_PWRALT	45		/* Set power alert trigger */
#define MR_REQ_PERST	46		/* Get persistent triggers flag */
#define MR_SET_PERST	47		/* Set persistent triggers flag */
#define MR_REQ_TTL	48		/* Get Throttle state */
#define MR_REQ_MAX	48		/* Max command code */


/*
**
** Transaction error record:
** If an error occurs during the handling of a request, an
** error record is returned, possibly with supplemental info.
**
** Fields usage:
**	err		code indication error condition
**	len		size of additional data
**
** For now there is no definition on what supplemental info
** should look like, but the idea is to open for a possibility
** of giving very precise specification on what the error was.
** Consider it a place holder for future use.
**
** Error codes:
** Code 'NOMEM' means that space for response generation was unavailable.
** Code 'NOVAL' is used to indicate that a valid request (i.e. a query
** on something temporarily unavailable, like processor utilization on
** a core in a sleep state) has no valid response.
**
*/

typedef struct mr_err {
	uint16_t	err;	/* Error code field */
	uint16_t	len;	/* Length of additional error info */
} MrErr;

#define	MR_ERR_INVOP	1	/* Dofus, command/opcode invalid */
#define MR_ERR_INVLEN	2	/* Dofus, length not valid for opcode */
#define MR_ERR_INVAUX	3	/* Dofus, parm field not valid for opcode */
#define MR_ERR_INVDATA	4	/* Dofus, content of data block invalid */
#define MR_ERR_PERM	5	/* Failure, privileged command */
#define MR_ERR_NOMEM	6	/* Failure, out of memory */
#define MR_ERR_SMC	7	/* Failure, SMC communication */
#define MR_ERR_NOVAL	8	/* Failure, no valid value to report */
#define MR_ERR_UNSUP	9	/* Failure, not implemented (temporary) */
#define MR_ERR_RANGE	10	/* Failure, parameter out of range */
#define MR_ERR_PEND	11	/* Pending, internal use only */


/*
**
** Response container structures below.
**
** Strings are returned in Pascal format (why?), i.e. pre-fixed
** with a 1 byte length field and post-fixed with a 0 byte.
**
*/


/*
 * MIC Hardware Info
 * REQ_HWINF Notes:
 *  - no idea how to determine PCI-E slot, it's a host side thing.
 *  - assume revision is same as model ID in the component ID register
 *  - unique ID not available in all flash versions
 *  - Hardware version codes are reported as-is, anticipating
 *    recipient to know what the codes means.
 */

typedef struct mr_rsp_hwinf {
  uint8_t	guid[MR_GUID_LEN];	/* Unique ID, from SMC */
  uint8_t	board;			/* Board type, SMC HW 17:16 */
  uint8_t	fab;			/* Fab version, SMC HW 10:8 */
  uint8_t	sku;			/* SKU #, SMC HW 2:0 */
  uint8_t	slot;			/* PCI-E slot, get from where ? */
  uint8_t	rev;			/* Revision, component ID 16:19 */
  uint8_t	step;			/* Stepping, component ID 12:15 */
  uint8_t	substep;		/* Sub-stepping, component ID 8:11 */
  uint8_t	serial[MR_SENO_LEN];	/* Serial number, from SMC */
} MrRspHwInf;



/*
 * MIC API version
 * REQ_PVER Notes:
 *  - returns RAS_VER string the module was built with.
 */

typedef struct mr_rsp_pver {
  char		api[MR_PVER_LEN];	/* Ras module version */
} MrRspPver;



/*
 * MIC uOS/Flash version
 * REQ_VERS Notes:
 *  - unclear at this point what the lengths of these strings are.
 *    The limit of 128 bytes is a 'best safe guess' and may change.
 *  - KnF: My card has 3 flash strings, for now that's the count.
 *  - KnC: Has fewer defined version strings, currently only fboot0
 *         string has been defined.
 */

typedef struct mr_rsp_vers {
  char		fboot0[MR_VERS_LEN];	/* Fboot 0 version */
  char		fboot1[MR_VERS_LEN];	/* Fboot 1 version */
  char		flash[3][MR_VERS_LEN];	/* Flash block versions */
  char		uos[MR_VERS_LEN];	/* uOS kernel version */
  char		fsc[MR_VERS_LEN];	/* Fan controller version */
} MrRspVers;



/*
 * Core frequency
 * REQ_CFREQ Notes:
 *  - current is clock read from CURRENTRATIO register.
 *  - default/requested clock is read from COREFREQ register.
 *    In KnF, the CURRENTRATIO is not used and therefore
 *    COREFREQ s reported as current speed and the default
 *    is simply the first value registered (at module load).
 *  - supported speeds are part of freq/voltage pairs maintained
 *    by the cpu_freq driver as part of PM (cpu_freq driver).
 *  - unclear if we should allow manual control (writes).
 */

typedef struct mr_rsp_freq {
  uint32_t	cur;			/* Actual core speed in kHz */
  uint32_t	def;			/* Set core speed in kHz */
  uint32_t	slen;			/* Supported count */
  uint32_t	supt[MR_PTAB_LEN];	/* Supported speed list in kHz */
} MrRspFreq;

/*
 * Set core frequency
 * New frequency (in kHz) passed in MrHdr.parm
 * SET_CFREQ Notes:
 *  - need to turn off PM for this to stick
 */



/*
 * Core voltage
 * REQ_CVOLT Notes:
 *  - KnF: Two core voltages; current voltage set from COREVOLT
 *         register and sense1 read in the BOARD_VOLTAGE_SENSE register.
 *  - KnC: 3 potential sources; SVID, SMC, and SBOX registers.
 *         SBOX regs require SMC telemetry which is uncertain.
 *         SVID does not work in A0, B0 is TBD.
 *         SMC will eventually relay VR data.
 *         Only SVID gives both set and actual values.
 *         Only SMC sets c_val field, zero is good.
 *  - Supported voltages are either determined from what the VRs
 *    can support or if PM is active it is part of the freq/voltage pairs
 *    maintained by the cpu_freq driver as part of PM (cpu_freq driver).
 */

typedef struct mr_rsp_volt {
  uint32_t	cur;			/* Core voltage read in uV */
  uint32_t	set;			/* Core voltage set in uV */
  uint8_t	c_val;			/* Valid bits, volt read */
  uint32_t	slen;			/* Supported count */
  uint32_t	supt[MR_PTAB_LEN];	/* Supported voltage list in uV */
} MrRspVolt;

/*
 * Set core voltage
 * New voltage passed in MrHdr.parm
 * SET_CVOLT Notes:
 *  - need to turn off PM for this to stick
 *  - Unclear if we should allow manual control through this API.
 */



/*
 * Card power
 * REQ_PWR Notes
 *  - Power status only avalable on KnC via SMC query
 *  - VR status on KnC may come from VRs directly or from SMC query
 *  - VR status on KnF comes from SBOX registers (telemtry)
 *  - If available, status bits from query is provided, zero is good.
 */

typedef struct mr_rsp_pws {		/* Power sensor status */
  uint32_t	prr;			/* Current reading, in uW */
  uint8_t	p_val;			/* Valid bits, power */
} MrRspPws;

typedef struct mr_rsp_vrr {		/* Voltage regulator status */
  uint32_t	pwr;			/* Power reading, in uW */
  uint32_t	cur;			/* Current, in uA */
  uint32_t	volt;			/* Voltage, in uV */
  uint8_t	p_val;			/* Valid bits, power */
  uint8_t	c_val;			/* Valid bits, current */
  uint8_t	v_val;			/* Valid bits, voltage */
} MrRspVrr;

typedef struct mr_rsp_power {
  MrRspPws	tot0;			/* Total power, win 0 */
  MrRspPws	tot1;			/* Total power, win 1 */
  MrRspPws	inst;			/* Instantaneous power */
  MrRspPws	imax;			/* Max instantaneous power */
  MrRspPws	pcie;			/* PCI-E connector power */
  MrRspPws	c2x3;			/* 2x3 connector power */
  MrRspPws	c2x4;			/* 2x4 connector power */
  MrRspVrr	vccp;			/* Core rail */
  MrRspVrr	vddg;			/* Uncore rail */
  MrRspVrr	vddq;			/* Memory subsystem rail */
} MrRspPower;



/*
 * Power envelope
 * REQ_PLIM Notes:
 *  - power envelope is a PM property. A physical limit
 *    is given to PM, which then calculate derivative high
 *    and low water mark figures.
 *  - values are retrieved from PM module
 */

typedef struct mr_rsp_plim {
  uint32_t	phys;			/* Physical limit, in W */
  uint32_t	hmrk;			/* High water mark, in W */
  uint32_t	lmrk;			/* Low water mark, in W */
} MrRspPlim;

/*TBD
 * Set power envelope
 * New value passed in MrHdr.parm
 * SET_PLIM Notes:
 *  - not sure if setting this should be allowed at all.
 */



/*
 * Core information
 * REQ_CLST Notes:
 *  - for the average user a core count is all required, since
 *    logically the cores are _always_ enumerated 0 .. <n>-1.
 *    Physical enumeration, such as ring stop, are not useful.
 *  - perhaps this request should return the CPU bitfields from
 *    the uOS of offline, online, possible, and present masks.
 *    Would allow watching of PM activity.
 */

typedef struct mr_rsp_clst {
  uint16_t	count;			/* Cores present */
  uint16_t	thr;			/* Threads per core */
} MrRspClst;


/*
 * Set core enable/disable
 * Core id & set/reset value passed in MrHdr.parm
 * ENB_CORE/DIS_CORE Notes:
 *  - uOS Linux does not have write access to HW config in SPI flash.
 *    No way to enable/disable cores
 *  - only listed here since if compatibility with FreeBSD is needed.
 */



/*
 * Memory device info
 * REQ_GDDR Notes:
 *  - This is read from scratch9, i.e. provided by bootstrap.
 */

typedef struct mr_rsp_gddr {
  char		dev[MR_GVND_LEN];	/* Device vendor */
  uint16_t	rev;			/* Device revision */
  uint32_t	size;			/* Device size, in Mbit/device */
  uint32_t	speed;			/* Transactions speed, kT/sec */
} MrRspGddr;



/*
 * GDDR frequencies
 * REQ_GFREQ Notes:
 *  - current clock can be read from MEMORYFREQ register
 *  - the GDDR nominal frequency is reported
 *  - the supported frequency list contains values that PLLs
 *    are capable of producing.  Info is of limited use, since
 *    there is no way to control the GDDR frequency (locked by fuses).
 */

typedef struct mr_rsp_gfreq {
  uint32_t	cur;			/* Current GDDR speed in kHz */
  uint32_t	def;			/* Default GDDR speed in kHz */
  uint32_t	slen;			/* Supported count */
  uint32_t	supt[MR_PTAB_LEN];	/* Supported speeds list in kHz */
} MrRspGfreq;

/*
 * Set GDDR frequency
 * New frequency passed in MrHdr.parm
 * SET_GFREQ Notes:
 *  - uOS cannot alter the PLLs because it requires retraining, which
 *    causes loss of memory content.
 *  - KnF: uOS does not have write access to SPI flash, which is required
 *         to modify the GDDR frequency at next reboot.
 *  - KnC: GDDR frequency is hard locked by fuses, cannot change, ever!!! 
 */



/*
 * GDDR voltages
 * REQ_GVOLT Notes:
 *  - KnF: Two GDDR voltages; current voltage set from MEMVOLT
 *         register and sense2 from BOARD_VOLTAGE_SENSE register.
 *         MEMVOLT register always returns zero, only sense2
 *         actually returns something useful in current Si.
 *  - KnC: 3 potential sources; SVID, SMC, and SBOX registers.
 *         SBOX regs require SMC telemetry which is uncertain.
 *         SVID does not work in A0, B0 is TBD.
 *         SMC will eventually relay VR data
 *         Only SVID gives both set and actual values.
 *         Only SMC sets c_val field, zero is good.
 *  - Supported voltages reported are voltages the VRs can be programmed
 *    to supply. Info is of limited use, since there is no way to control
 *    the GDDR voltage (locked by fuses).
 */

typedef struct mr_rsp_gvolt {
  uint32_t	cur;			/* GDDR voltage read in uV */
  uint32_t	set;			/* GDDR voltage set in uV */
  uint8_t	c_val;			/* Valid bits, volt read */
  uint32_t	slen;			/* Supported count */
  uint32_t	supt[MR_PTAB_LEN];	/* Supported voltage list in uV */
} MrRspGvolt;

/*
 * Set GDDR voltage
 * New voltage passed in MrHdr.parm
 * SET_GVOLT Notes:
 *  - uOS cannot alter the VR settings at all. Even if it could
 *    then it still clash with the need to retrain and memory loss.
 *  - KnF: uOS does not have write access to SPI flash, which is required
 *         to modify the GDDR voltage at next reboot.
 *  - KnC: GDDR voltage is hard locked by fuses, cannot change, ever!!! 
 */



/*
 * Board temperatures
 * REQ_TEMP Notes:
 *  - CPU die temps can be read from THERMAL_STATUS (highest
 *    of several sensors) and CURRENT_DIE_TEMP registers.
 *    The die sensors values do not match the status
 *    value, so the conversion formula or calibration
 *    needs a re-visit.
 *  - If we could get at them, we could provide readings
 *    from the following devices, but are they all useful?
 *      Fan inlet sensor
 *      Fan exhaust sensor
 *      GDDR temp (one chip is measured) sensor
 *      Vccp VR
 *      Vddg VR
 *      Vddq VR
 *  - most devices report current and maximum temperatures in
 *    degrees Celcius as a signed integer, 9 bits for die temp
 *    and 8 bits for voltage regulators, 12 bit for sensors.
 */

typedef struct mr_rsp_tsns {
  int16_t	cur;			/* Current temperature, in C */
  int8_t	c_val;			/* Valid bits, if available */
} MrRspTsns;

typedef struct mr_rsp_tdie {
  int16_t	cur;			/* Current temperature, in C */
  int16_t	max;			/* Maximum temperature, in C */
} MrRspTdie;

typedef struct mr_rsp_temp {
  MrRspTsns	die;			/* Highest on-die measure */
  MrRspTdie	dies[MR_DIES_LEN];	/* All on-die measures */
  MrRspTsns	brd;			/* Highest board measure */
  MrRspTsns	fin;			/* Fan inlet */
  MrRspTsns	fout;			/* Fan outlet */
  MrRspTsns	gddr;			/* Gddr device */
  MrRspTsns	vccp;			/* Vccp VR */
  MrRspTsns	vddg;			/* Vddg VR */
  MrRspTsns	vddq;			/* Vddq VR */
} MrRspTemp;



/*
 * Fan speed
 * REQ_FAN Notes:
 *  - fan status is reported in RPM and it's control is
 *    a pulse with modulation ratio to 255, i.e. 0 is min,
 *    127 is ~50% and 255 is max.
 *  - the card has logic for controlling two fans.
 *    Only one is used and we only report status for one.
 */

typedef struct mr_rsp_fan {
  uint16_t	rpm;			/* Fan speed, rpm */
  uint8_t	pwm;			/* Active PWM ratio, 0..255 */
  uint8_t	override;		/* Override flag */
  uint8_t	r_val;			/* Valid bits, speed */
  uint8_t	p_val;			/* Valid bits, PWM */
} MrRspFan;

/*
 * Set fan speed
 * Control is passed in MrHdr.parm (struct fits into 32 bit)
 * SET_FAN Notes:
 *  - this may collide with OOB methods (such as IPMI)
 *    that has priority, no guarantee this will stick.
 *  - changing fan speed parameters may interfere
 *    with PM in undefined ways.
 */

typedef struct mr_set_fan {
  uint8_t	override;		/* Override enable flag */
  uint8_t	pwm;			/* Force PWM ratio, 0..255 */
} MrSetFan;



/*
 * Error correction mode
 * REQ_ECC Notes:
 *  - retrieve this info from one (any) of the gboxes.
 */

typedef struct mr_rsp_ecc {
  uint32_t	enable;			/* ECC mode: 1 enabled, 0 disabled */
} MrRspEcc;

/*
 * Set error correction mode
 * New mode passed in MrHdr.parm
 * SET_ECC Notes:
 *  - ECC cannot be changed on the fly by uOS, requires retraining
 *    of GDDR which causes loss of memory content.
 *  - uOS Linux does not have write access to HW config in SPI flash.
 *    No way to change ECC enable/disable setting.
 */



/*
 * Trace level
 * REQ_TRC Notes:
 *  - No idea what support this has in uOS Linux.
 */

typedef struct mr_rsp_trc {
  uint32_t	lvl;			/* Debug trace level */
} MrRspTrc;

/*
 * Set trace level
 * New level passed in MrHdr.parm
 * SET_TRC Notes:
 *  - No idea what this does in uOS Linux (nothing yet).
 */



/*
 * Turbo setting
 * REQ_TRBO Notes:
 *  - Retrieve current actual turbo mode and state
 *  - 'set' value: 1 if enabled, 0 otherwise
 *  - 'state' value: 1 if active, 0 otherwise
 *  - 'avail' value: 1 if TRBO supported, 0 otherwise
 */

typedef struct mr_rsp_trbo {
  uint8_t	set;			/* Turbo mode */
  uint8_t	state;			/* Turbo state */
  uint8_t	avail;			/* Turbo mode available */
  uint8_t	pad;			/* Pad to 32 bit */
} MrRspTrbo;

/*
 * Set turbo mode
 * New mode passed in MrHdr.parm
 * SET_TRB Notes:
 *  - Set always allowed, but silently ignored is not available.
 */



/*
 * LED override
 * REQ_LED Notes:
 * - KnC: Retrieve current LED mode setting, 0=normal, 1=identify
 * - KnF: not implemented (error MR_ERR_UNSUP)
 */

typedef struct mr_rsp_led {
  uint32_t	led;			/* LED mode setting */
} MrRspLed;

/*
 * Set LED mode
 * New mode passed in MrHdr.parm
 * SET_LED Notes:
 * - KnC: Mode values
 *     0 is normal SMC control (fast blink)
 *     1 is identify mode (2 blinks every 2 seconds)
 * - KnF: not implemented (error MR_ERR_UNSUP)
 */



/*
 * Overclocking
 * REQ_OCLK Notes:
 *  - Curently no idea how to represent overclocking state 
 *  - Overclocking not supported, return MR_RSP_NOVAL
 */

typedef struct mr_rsp_oclk {
  uint32_t	freq;			/* Over clocking setting */
} MrRspOclk;

/*
 * Set overclocking mode
 * New mode passed in MrHdr.parm
 * SET_OCLK Notes:
 *  - Overclocking not supported, return MR_RSP_NOVAL
 */



/*
 * Processor utilization (OS status)
 * REQ_CUTL Notes:
 *  - returned info is a simple sum of 4 logical CPUs
 *  - the counter units returned are Linux kernel jiffies,
 *    typically in range 1 - 10 ms, based on continous
 *    counters maintained by the kernel. The number of
 *    jiffies per second is reported for scaling purposes.
 *    In order to get a current 'utilization' figure, the
 *    host needs to query the counters at regular intervals
 *    and use this formula to achieve a percentage:
 *       u = ((c2 - c1) / (t2 - t1)) * 100
 *    or
 *       u = ((c2 - c1) * 100) / (t2 - t1)
 *    where t2 - t1 = elapsed jiffies between samples
 *          c2 - c1 = usage jiffy counts between samples
 *  - the listed counters does not add up to cover the
 *    wall clock time exactly, sampling errors do occur.
 *  - counters for iowait, irq, and softirq are not included.
 *  - jiffy counters are updated by the timer tick interrupt
 *    handler. It's accuracy is known to be limited, see
 *    Documentation/cpu-load.txt for details.
 *  - counters are reported regardless of core sleep states
 */

typedef struct mr_rsp_ccnt {
  uint64_t	user;			/* Normal user mode jiffies */
  uint64_t	nice;			/* 'Nice' user mode jiffies */
  uint64_t	sys;			/* System mode jiffies */
  uint64_t	idle;			/* Idle time jiffies */
} MrRspCcnt;

typedef struct mr_rsp_cutl {
  uint32_t	tck;			/* Actual jiffs/sec (scaled by 256) */
  uint16_t	core;			/* Cores reported on */
  uint16_t	thr;			/* Threads per core */
  uint64_t	jif;			/* Jiffy counter at query time */
  MrRspCcnt	sum;			/* System wide counters */
  MrRspCcnt	cpu[MR_CORE_LEN];	/* Counters per core */
} MrRspCutl;



/*
 * Memory utilization (OS status)
 * REQ_MEM Notes:
 *  - memory snapshot is obtained from kernel structs.
 *    No walk of page descriptors is performed.
 *  - Not all memory stats are visible (exported to) modules.
 *
 *TBD:
 * - Need clarification on what memory utilization means.
 *   For now the total, free and buffer memory is reported.
 */

typedef struct mr_rsp_mem {
  uint32_t	total;			/* Total usable RAM in kB */
  uint32_t	free;			/* Free memory in kB */
  uint32_t	bufs;			/* Buffer storage in kB */
} MrRspMem;



/*
 * Process management (OS status)
 * REQ_OS/REQ_PROC/REQ_THRD Notes:
 * - split in 3 levels of detail:
 *     1) Get set of applications (exclude kernel processes and threads)
 *     2) Get details on specified application (pid in MrHdr.parm),
 *        which includes a thread pid list (up to 256 threads).
 *     3) Get details on specific thread (thread id in MrHdr.parm)
 *   Opcodes 2 and 3 will, apart from thread list, mostly report the same
 *   set of details. What needs monitoring (see 'man proc', section on
 *   /proc/<pid>/stat and /proc/<pid>/status for what's available)?
 * - process time counters are continuous, so if any ratio between
 *   the time a process/thread spends and actual wall clock time is
 *   to be calculated, the same logic for dynamic display applies as
 *   for the CUTL counters. I.e. a jiffy stamp is needed in the reply.
 *TBD:
 * - Introduce some sanity in time measurements.
 * - Level 3 (thread details) is not implemented (is it needed ?).
 * - Add ppid & credentials in MrRspProc? Needed to make a "top" display.
 */

typedef struct mr_rsp_os {
  uint64_t	uptime;			/* Seconds since OS boot */
  uint64_t	loads[3];		/* 1, 5, 15 minute load average */
  uint32_t	alen;			/* Application count */
  uint32_t	apid[256];		/* Application PIDs */
} MrRspOs;

typedef struct mr_rsp_proc {
  uint32_t	pid;			/* Process ID */
  char		name[16];		/* Program name (less path) */
  uint64_t	utime;			/* User time in uS */
  uint64_t	stime;			/* System time in uS */
  uint64_t	etime;			/* Elapsed time in uS */
  uint32_t	rss;			/* Resident set, in kB */
  uint32_t	vm;			/* VM size, in kB */
  uint32_t	tlen;			/* Thread count */
  uint32_t	tpid[256];		/* Process threads */
} MrRspProc;



/*
 * Terminate process
 * Signal passed in MrHdr.parm bits 31:24 (see 'kill -l')
 * Process ID passed in MrHdr.parm bits 23:0 (see /proc/sys/kernel/pid_max)
 * CMD_PKILL Notes:
 * - This is specifically for MPI style cluster managers
 *   who wants to rid the card of a specific process.
 * - Processes owned by users ID's less than 500 are immune to this.
 */



/*
 * Terminate user
 * Signal passed in MrHdr.parm bits 31:24 (see 'kill -l')
 * User ID passed in MrHdr.parm bits 23:0 (see /etc/login.defs).
 * CMD_UKILL Notes:
 * - This is specifically for MPI style cluster managers to
 *   rid the card of processes owned by a specific user ID.
 * - User ID's below 500 will silently be ignored.
 */



/*
 * Read SMC register
 * MR_GET_SMC Notes:
 * - Both SMC and FSC devices are accessed through I2C busses, which
 *   means that retrieval will be slow (order of milli seconds).
 * - KnC: allows direct access to the SMC CSRs, which can be read
 *        or written in any random order.
 *        SMC CSR definitions are not within the scope of this API.
 *        Register number passed in MrHdr.parm bits 7:0 (8 bits).
 *        SMC registers are 32 bit, except one (UUID) that is 16 byte.
 * - KnF: allows direct access to the fan speed controller (FSC)
 *        status registers on board temp and power sensors.
 *        The FSC execute command register every 50 mSec, which means
 *        that register needs 'SET' and hold for 50 mSec before any
 *        value can be returned. For telemetry data the SET is done
 *        implicitly, all other has to execute a 'SET' before running
 *        a 'GET' command.
 *        
          FSC register definitions are not within the scope of this API.
 *        All sensor data returns are 8 bit wide.
 */

typedef struct mr_rsp_smc {
  uint8_t	reg;			/* Register number */
  uint16_t	width;			/* Valid return bytes (4 or 16) */
  union {
    uint32_t	val;			/* Requested register value */
    uint8_t	uuid[16];		/* Unique identifier */
    uint8_t	serial[12];		/* Card serial number */
  } rtn;
} MrRspSmc;

/*
 * Write SMC register
 * Register number passed in MrHdr.parm bits 31:24 (8-bit address decode).
 * Register value passed in MrHdr.parm bits 23:0 (24 bit data).
 * MR_SET_SMC Notes:
 * - Improper use of this command can cause thermal shutdown of the card.
 * - Improper use can interfere with power management.
 * - KnC: For security reasons only the following registers are writeable:
 *        20, 22				IPMI <not documented>
 *        2b, 2c, 2d, 2f, 30, 31, 32, 33	PM control parameters
 *        4b					Fan Adder
 *        60					LED control
 *        No SMC registers of interest are more than 16 bits wide.
 * - KnF: For security reasons only the followingregisters are writable:
 *        0		 Fan 1 Speed Override
 *        1		 Power Management and Control Config
 *        11		 General Status command
 *        Selector is 8 bits wide and only valid values are
 *        20, 21, 22, 23			Power sensors, 1s avg.
 *        30, 31, 32, 33			Power sensors, 1 sample
 *        a1, a2, a3, a4, a5			Max temps
 */



/*
 * Get PM config mode
 * REQ_PMCFG notes:
 *  - Return value is reported 'as-is' from the PM module.
 */

typedef struct mr_rsp_pmcfg {
  uint32_t	mode;			/* Current PM operation mode */
} MrRspPmcfg;



/*
 * Read Power triggers
 * Consist of two trigger points (power,time), which can be calculated
 * from SKU at card power-on or be persistent across reboots.
 * At trigger (PROCHOT), GPU Hot gets asserted
 * At trigger (PWRALT), Power Alert gets asserted
 *
 * MR_REQ_PROCHOT, MR_REQ_PWRALT Notes:
 * - KnC: Read SMC registers for trigger 0 and 1 respectively.
 *        GPUHOT: registers 0x2c and 0x2d
 *        PWRALT: registers 0x2f and 0x30
 * - KnF: not implemented (error MR_ERR_UNSUP)
 */

typedef struct mr_rsp_ptrig {
  uint16_t	power;			/* Power limit, Watt */
  uint16_t	time;			/* Time windows, mSec */
} MrRspPtrig;

/*
 * Write Power triggers
 * MR_SET_PROCHOT, MR_SET_PWRALT Notes
 * Structure MrRspPtrig passed in MrHdr.parm
 * Trigger PROCHOT.power must be higher than trigger PWRALT.power.
 * - KnC: Write SMC registers for trigger 0 and 1 respectively.
 *        GPUHOT: registers 0x2c and 0x2d
 *        PWRALT: registers 0x2f and 0x30
 * - KnF: not implemented (error MR_ERR_UNSUP)
 * Warning: MT does not check for GPUHOT.power >= PWRALT.power.
 *TBD: Should it?
 *     It is anticipated that changes follows reads, i.e. checking
 *     can be checked in application software. 
 */



/*
 * Read Persistent Power triggers flag
 * If set, changes to Power Triggers will be permanent
 * MR_REQ_PERST Notes:
 * - KnC: Reads bit 0 of SMC register 0x32
 * - KnF: not implemented (error MR_ERR_UNSUP)
 */

typedef struct mr_rsp_perst {
  uint32_t	perst;			/* Persistent power triggers */
} MrRspPerst;

/*
 * Write Persistent Power triggers flag
 * New value passed in MrHdr.parm
 * MR_SET_PERST Notes:
 * - KnC: Writes bit 0 of SMC register 0x32
 * - KnF: not implemented (error MR_ERR_UNSUP)
 */


/*
 * Read Throttle states
 * Returns status of current and previous throttle state
 * retrieved from the card side PM module.
 * MR_REQ_TTL Notes:
 *  - KnC: Calls PM for latest information.
 *         Note that the 'active' flags can toggle very often,
 *         which may make it less informative for display.
 *         Time tracked in jiffies, not true mSec resolution.
 *  - KnF: not implemented (error MR_ERR_UNSUP)
 */

typedef struct mr_rsp_tstat {
  uint8_t	active;		/* Currently active */
  uint32_t	since;		/* Length of current throttle, mSec */
  uint32_t	count;		/* Number of throttles */
  uint32_t	time;		/* Total time throttled, mSec */
} MrRspTstat; 

typedef struct mr_rsp_ttl {
  MrRspTstat	thermal;	/* Thermal throttle state */
  MrRspTstat	power;		/* Power throttle state */
  MrRspTstat	alert;		/* Power alert state */
} MrRspTtl;
  

#pragma pack(pop)		/* Restore to entry conditions */

#ifdef __cplusplus
}	/* C++ guard */
#endif

#endif	/* Recursion block */
