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
 * RAS MT module driver
 *
 * Code and data structures to handle get/set tasks for KnF.
 * Parties accessing the data structures are supposed to use the
 * micras_mt_tsk() routines to ensure integrity and consistency.
 * Particularly important when handling sysfs nodes and actions
 * requested from SCIF connections must use that method in order
 * to guarantee serialized access.
 *
 * Even if read-only access to latest valid data is required,
 * it should go through micras_mt_tsk() using dedicated handlers
 * in this module.
 */

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/io.h>
#include <linux/utsname.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include <linux/kernel_stat.h>
#include <linux/bitmap.h>
#include <generated/compile.h>
#include <generated/utsrelease.h>
#include <mic/micbaseaddressdefine.h>
#include <mic/micsboxdefine.h>
#include "micras_api.h"
#include "micmca_api.h"
#include "micras.h"


/*
 * Persistent data accessible through the CP api.
 * Some functions just read/modify hardware CSRs
 * and thus need no storage between invocations.
 */

extern struct mr_rsp_vers	vers;
extern struct mr_rsp_volt	volt;
extern struct mr_rsp_freq	freq;
extern struct mr_rsp_power	power;
extern struct mr_rsp_plim	plim;
extern struct mr_rsp_gddr	gddr;
extern struct mr_rsp_gvolt	gvolt;
extern struct mr_rsp_gfreq	gfreq;
extern struct mr_rsp_temp	temp;
extern struct mr_rsp_ecc	ecc;
extern struct mr_rsp_trbo	trbo;
extern struct mr_rsp_pmcfg	pmcfg;

#if USE_FSC
/*
**
** FSC API
**
** The FSC has a back-door communication channel, not documented
** anywhere in the register spec nor in any HAS or LLD that is
** available on recent KnF cards (later than rev ??). 
** Found a .35 proposal for it, so it better do. In short, this
** backdoor relies on fan #2 is not used on KnF and the fact that
** controls for fan #2 is transmitted over I2C to the fan speed
** controller (FSC) unaltered, such that it can chose an alternate
** interpretation of received data.
**
** The Fan Speed Override register (SBOX 0x8007d102c) has this
** definition in the register spec:
**
**  Bit(s)	Usage
**  ------	----------
**  7:0		Fan 1 override ratio
**  14		Fan 1 Set max speed
**  15		Fan 1 Enable override 
**  23:16	Fan 2 override ratio
**  30		Fan 2 Set max speed
**  31		Fan 2 Enable override 
**
** This register has been repurposed into a Message Gain Bit Bang Register
** (MGBR) with a 4 bit command and a 16 bit data field, layout is:
**
**  Bit(s)	Usage
**  ------	----------
**  7:0		MGBR data 7:0
**  21:14	MGBR data 15:8
**  23:22	MGBR command 1:0
**  31:30	MBGR command 3:2
**
** Command	Usage
**    0		Fan 1 Speed Override
**    1		Power Management and Control Config
**    7		PMC PCIe Alert Override
**    8		PMC 2x3 Alert Override
**    9         PMC 2x4 Alert Override
**   10		Temperature Override Command
**   11		General Status Command
**   12-15	PID Gain Command(s)
**
** Fan 1 control works as MGBR command 0, though the spec is unclear on
** whether the resulting FSO register format is same as the original spec.
** Specifically, old spec has Fan 1 override enable in FSO bit 15, whereas
** the MGBR spec has it in MGBR data bit 15 (corresponds to FSO bit 20).
** Test shows it has to be MGBR bit 9, i.e. compatible with register spec.
** 
** Fan #2 Status Register (SBOX 0x8007d1028) has been redefined into a
** Message Gain Bit Bang Status (MGBSR) used to hold return data from
** the MGBR General Status command in this layout:
**
**  Bit(s)	Usage
**  ------	----------
**  23:0	MGBSR data
**  31:28	MGBR Gen. Sts. selector (bits 23:0 source).
**
** To get access to KnF telemetry data, only MGBR command 11 is needed.
** Bits 7:0 of MGBR data for this command selects the sensor which FSC
** will report to MGBSR (not sure if one-time or repeatedly). The actual
** encoding is as follows:
**
**    0x00      Fan2Status
**    0x01      PMC Configuration Command Settings
**    0x07      Reads the 2x4 IR3275 Configuration Register
**    0x08      Reads the 2x3 IR3275 Configuration Register
**    0x09      Reads the PCIe IR3275 Configuration Register
**    0x0A      Reads the Temperature Command Settings
**    0x20      Maximum Total Card Power - 1s Moving Average (20 Samples)
**    0x21      Maximum 2x4 Connector Power - 1s Moving Average (20 Samples)
**    0x22      Maximum 2x3 Connector Power - 1s Moving Average (20 Samples)
**    0x23      Maximum PCIe Connector Power - 1s Moving Average (20 Samples)
**    0x30      Maximum Total Card Power - Single Sample
**    0x31      Maximum 2x4 Connector Power - Single Sample
**    0x32      Maximum 2x3 Connector Power - Single Sample
**    0x33      Maximum PCIe Connector Power - Single Sample
**    0xA0      Returns the current Fan Tcontrol setting for the GPU temperature
**    0xA1      Maximum Temperature for Temperature Sensor 1 - VCCP
**    0xA2      Maximum Temperature for Temperature Sensor 2 - Air Inlet
**    0xA3      Maximum Temperature for Temperature Sensor 3 - NW GDDR
**    0xA4      Maximum Temperature for Temperature Sensor 4 - V1P5 VDD VR
**    0xA5      Maximum Temperature for Temperature Sensor 5 - Display Transmitter
**    0xA6      Maximum Temperature for GPU
**
** The 'return' values in MGBSR are 16 bit only, power in Watts, Temp in C. 
**
** Implementation notes:
** > The MGBR API is timing sensitive. FSC reads the MGBR register
**   at ~50 mSec intervals over an I2C bus and performs the command
**   on every read, which in case of the General Status command will
**   result in wrinting FSC internal data to the MGBSR register.
**   A delay is required after every write to MGBR in order to
**   ensure the FSC actually sees it.
** 
** > I2C bus reads are 7 bytes, writes are 6 bytes, 1 clock at 100 kHz
**   is 10 uSec, 1 byte roughly translates to 10 bits, so minimum delay
**   on I2C from command written to return value is valid becomes
**     10 * (6 + 7) * 10 uSec  = 1.3 mSec
**   The I2C bus on KnF runs slower than 100 kHz, causing tranfers
**   to take more time than that to finish.
**   After the initial delay, we'll may need to wait on a result
**   to arrive in the MGBSR register.
**
** > It seems that fan 1 override is a dynamic act, i.e. for it to
**   be in effect the MBGR command needs to be set accordingly.
**   Therefore, when reading telemetry, the MGBR command is set
**   just for a period long enough for it to be seen by FSC and the
**   result to be latched into the MGBSR register. After that period
**   (when fan speed override is active) the MGBR is returned to
**   restore the fan 1 override.
**
*/

#define MR_FSC_MGBR_OVR_CMD	0	/* Fan 1 Speed Override */
#define MR_FSC_MGBR_GEN_CMD	11	/* General Status command */

#define MR_FSC_STATUS		0x00	/* FSC Status & version */
#define MR_FSC_PMC_CFG		0x01	/* PMC Configuration */

#define MR_FSC_PWR_TOT		0x20	/* Total Power (1 sec avg) */
#define MR_FSC_PWR_2X4		0x21	/* 2x4 Power (1 sec avg) */
#define MR_FSC_PWR_2X3		0x22	/* 2x3 Power (1 sec avg) */
#define MR_FSC_PWR_PCIE		0x23	/* PCIe Power (1 sec avg) */

#define MR_FSC_PWR1_TOT		0x30	/* Total Power (single sample) */
#define MR_FSC_PWR1_2X4		0x31	/* 2x4 Power (single sample) */
#define MR_FSC_PWR1_2X3		0x32	/* 2x3 Power (single sample) */
#define MR_FSC_PWR1_PCIE	0x33	/* PCIe Power (single sample) */

#define MR_FSC_TEMP_VCCP        0xA1    /* VCCP VR Temperature */
#define MR_FSC_TEMP_INLET       0xA2    /* Card Inlet Temperature */
#define MR_FSC_TEMP_GDDR        0xA3    /* GDDR Temperature */
#define MR_FSC_TEMP_VDD         0xA4    /* VDD VR Temperature */
#define MR_FSC_TEMP_DISP        0xA5    /* Display Transmitter */


/*
 * Simple I/O access routines for FSC registers
 */

#ifdef MIC_IS_EMULATION
/*
 * Emulation does not handle I2C busses in general.
 * Not sure if FSC is emulated, but won't rely on it.
 * The following stubs are for emulation only.
 */

int
fsc_mgbr_read(uint32_t * v)
{
  if (v)
    memset(v, 0, 4);

  return 0;
}

void
fsc_mgbr_write(uint8_t c, uint32_t v)
{
}

#else

#if 0
#define RL      printk("%s: %2x -> %08x\n",    __FUNCTION__, mgbr_cmd, *val)
#define WL      printk("%s: %2x <- %08x\n",    __FUNCTION__, mgbr_cmd, *val)
#else
#define RL      /* As nothing */
#define WL      /* As nothing */
#endif

static uint8_t	mgbr_cmd;	/* Last MGBR command */
static uint32_t	mgbr_dat;	/* Last MGBR data */
static uint32_t	fan1_ovr;	/* Current fan 1 override command */

/*
 * Read MGBSR from SBOX
 *
 * This function only support MGBR commands MR_FSC_MGBR_{OVR|GEN}_CMD.
 * The operation mode is that the command is written to MGBR and after
 * a while the response shows up in MGBSR, which has fields that tell
 * which command caused the response (bits 31:28), and for GEN command
 * also which sensor was read. This function checks both fields.
 * 
 * We'll poll at 1 mSec rate and allow up to 200 mSec for the
 * FSC to provide the measure in the SBOX register.
 */

int
fsc_mgbsr_read(uint32_t * val)
{
  uint32_t	mgbsr;
  int		n;
  
  for(n = 0; n < 200; n++) {
    mgbsr = mr_sbox_rl(0, SBOX_STATUS_FAN2);
    if ((GET_BITS(31, 28, mgbsr) == mgbr_cmd) ||
	mgbr_cmd != MR_FSC_MGBR_GEN_CMD || mgbr_dat == 0) {
      if (mgbr_cmd != MR_FSC_MGBR_GEN_CMD ||
          mgbr_dat <= 1) {
        *val = GET_BITS(23, 0, mgbsr);
        RL;
        return 0;
      }
      if (GET_BITS(23, 16, mgbsr) == mgbr_dat) {
        *val = GET_BITS(15, 0, mgbsr);
        RL;
        return 0;
      }
    }
    myDELAY(1000);
  }

  /*
   * Timeout
   */
  return 1;
}


/*
 * Write MGBR on SBOX
 *
 * This function only support MGBR commands MR_FSC_MGBR_{OVR|GEN}_CMD.
 * The OVR command only when fan 1 speed override is active.
 * The GEN command is meant to cause a new selectable telemetry to be
 * pushed into the MBGSR register by the FSC. Any necessary delays
 * are handled here. Not by the read function.
 */

void
fsc_mgbr_write(uint8_t c, uint32_t * val)
{
  uint32_t	prev_cmd, prev_dat;
  uint32_t	mgbr_reg, mgbr_sel;
  uint32_t	mgbsr, n;

  prev_cmd = mgbr_cmd;
  prev_dat = mgbr_dat;
  mgbr_cmd = GET_BITS(3, 0, c);
  mgbr_dat = GET_BITS(15, 0, *val);

  mgbr_reg = PUT_BITS(31, 30, (mgbr_cmd >> 2)) |
	     PUT_BITS(23, 22,  mgbr_cmd) |
	     PUT_BITS(21, 14, (mgbr_dat >> 8)) |
	     PUT_BITS( 7,  0,  mgbr_dat);
  WL;
  mr_sbox_wl(0, SBOX_SPEED_OVERRIDE_FAN, mgbr_reg);

  /*
   * Special for Set Fan Speed, we keep track of that one
   */
  if (mgbr_cmd == MR_FSC_MGBR_OVR_CMD) {
    if (GET_BIT(9, mgbr_dat))
      fan1_ovr = GET_BITS(9, 0, mgbr_dat);
    else
      fan1_ovr = 0;
  }

  /*
   * If the command issued is the same as the previous command,
   * there is no way to determine if the MGBSR register is result
   * of this or the previous command. It is not possible to clear
   * MGBSR (read-only register), so if it is the same register,
   * we'll just have to wait long enough for FSC to respond.
   * Not all MGBR commands are mirrored into top 4 bits of MGBSR,
   * those gets the simple delay treatment.
   */
  if ((mgbr_cmd == prev_cmd && mgbr_dat == prev_dat) ||
       mgbr_cmd != MR_FSC_MGBR_GEN_CMD || mgbr_dat <= 1) {
    myDELAY(100 * 1000);
    return;
  }
  mgbr_sel = GET_BITS(7, 0, mgbr_dat);
  for(n = 0; n < 200; n++) {
    mgbsr = mr_sbox_rl(0, SBOX_STATUS_FAN2);
    if (GET_BITS(31, 28, mgbsr) == mgbr_cmd) {
      if (mgbr_cmd != MR_FSC_MGBR_GEN_CMD)
	return;
      if (GET_BITS(23, 16, mgbsr) == mgbr_sel)
	return;
    }
    myDELAY(1000);
  }
}
#undef RL
#undef WL
#endif /* EMULATION */


/*
 * Bypass for FSC access.
 * Somewhat bizarre backdoor to the FSC's MGBR and MGBSR registers.
 * The FSC interface is asymmetrical by nature since only the General
 * Status MGBR command can cause data to be returned through MGBSR.
 * To make it appear as telemetry registers can be read directly
 * and without need for privileges, the Read operation is rigged to
 * issue the appropriate MGBR registers itself when necessary.
 *
 * To protect the FSC integrity, the SET command are restricted
 * to privileged users and is only accepting commands that cannot
 * harm the FSC integrity. For now the whitelist consists of
 *    0 Fan 1 Speed Override
 *    1 Power Management and Control Config
 *   11	General Status command
 *
 * To read back the response from a SET command the exact same value
 * of 'parm' must be passed to a subsequent GET, in which case the
 * the GET routine will not insert it's own MGBR command to select
 * contents of the MGBSR to return.
 *  
 * Notice that FSC read is equivalent of reading Fan #2 Status register
 * and FSC write is equivalent of writing Fan Speed Override register.
 *
 * This reuse the SMC interface structs, but the semantics are different.
 *
 * Return:
 *  r->reg	MGBSR sensor select (if applicable) or 0
 *  r->width	always 3 (24 bit wide field)
 *  r->rtn.val	MGBSR sensor data
 *
 * Input:
 *  parm 31:24	MGBR command (must be 0xb)
 *  parm 15:0	MGBR data (sensor select)
 */

int
mr_get_fsc(void * p)
{
  int           rtn;
  uint32_t	raw;
  struct mr_rsp_smc * r;
  uint8_t       cmd;
  uint32_t	dat, parm;

  /*
   * Extract MGBR command and dat
   */
  parm = * (uint32_t *) p;
  cmd = GET_BITS(31, 24, parm);
  dat = GET_BITS(15, 0, parm);

  /*
   * If the request is different from the last issued
   * 'SET' command in any way then 'GET' will issue the
   * corresponding MGBR command, if allowed.
   */
  if (mgbr_cmd != cmd || mgbr_dat != dat) {
    /*
     * Only allow 'General Status' command
     */
    if (cmd != MR_FSC_MGBR_GEN_CMD)
      return -MR_ERR_PERM;

    /*
     * Screen against known FSC register widths.
     * All commands seems to be 16 bit wide.
     * We insist that unused upper bits are zeros.
     */
    if (dat != GET_BITS(23, 0, parm))
      return -MR_ERR_INVAUX;

    /*
     * Better way to single out these numbers?
     *  0 1 20 21 22 23 30 31 32 33 a1 a2 a3 a4 a5
     */
    if (! ((dat <= 1) ||
           (dat >= 0x20 && dat <= 0x23) ||
           (dat >= 0x30 && dat <= 0x33) ||
           (dat >= 0xa1 && dat <= 0xa5)))
      return -MR_ERR_PERM;

    /*
     * Write MGBR command
     */
    fsc_mgbr_write(cmd, &dat);
  }

  /*
   * Read MGBSR result
   */
  rtn = fsc_mgbsr_read(&raw);
  if (rtn)
    return -MR_ERR_SMC;

  /*
   * Revert to normal if fan 1 speed override mode if needed.
   */
  if (fan1_ovr)
    fsc_mgbr_write(MR_FSC_MGBR_OVR_CMD, &fan1_ovr);

  r = (struct mr_rsp_smc *) p;
  if (cmd == MR_FSC_MGBR_GEN_CMD)
    r->reg = GET_BITS(7, 0, dat);
  r->width = 3;
  r->rtn.val = GET_BITS(23, 0, raw);

  return sizeof(*r);
}


int
mr_set_fsc(void * p)
{
  uint8_t       cmd;
  uint32_t      dat, parm;

  parm = * (uint32_t *) p;
  cmd = GET_BITS(31, 24, parm);
  dat = GET_BITS(15, 0, parm);

  /*
   * Screen against known FSC register widths.
   * All commands seems to be 16 bit wide.
   * We insist that unused upper bits are zeros.
   */
  if (dat != GET_BITS(23, 0, parm))
    return -MR_ERR_INVAUX;

  /*
   * 4-bit command code for FSC.
   * Mask of valid codes needs just 16 bits.
   * Max valid codes 0..1, 7..15, mask 0xff83.
   * Non-debug registers reduce mask to 0x0803.
   */
  if (! ((1 << cmd) & 0x0803))
    return -MR_ERR_PERM;

  /*
   * Write MGBR command and revert to fan 1 speed override mode
   * if needed (override in effect).  Side effect of reverting
   * is that any reponse in MGBSR must to be read before next
   * FSC sample happens, i.e. within 50 mSec.
   */
  fsc_mgbr_write(cmd, &dat);
  if (fan1_ovr)
    fsc_mgbr_write(MR_FSC_MGBR_OVR_CMD, &fan1_ovr);

  return 0;
}
#endif


/*
**
** Conversion between CP formats (uV, MHz, etc.)
** and hardware register formats (SBOX mostly).
**
*/


/*
 * VRM11 voltage converters
 * Only bits 6:1 are being used as follows:
 *   Volt = Max - Res * (Bits -1)
 *   Bits = 1 + (Max - Volt) / Res
 * The delta divided by resolution is 62.
 * Bits value of 0 reserved for turning VR off.
 */

#define VRM11_MAX	1600000		/* 1.60 V */
#define VRM11_MIN	 825000		/* 825 mV */
#define VRM11_RES	  12500		/* 12.5 mV */

uint32_t
vid2volt(uint8_t vid)
{
  uint32_t	bits;

  bits = GET_BITS(6, 1, vid);
  if (bits)
    return VRM11_MAX - VRM11_RES * (bits - 1);
  else
    return 0;
}

uint8_t
volt2vid(uint32_t uv)
{
  uint32_t	delta, bits;

  bits = 0;
  if (uv >= VRM11_MIN && uv <= VRM11_MAX) {
    delta = VRM11_MAX - uv;
    /*
     * Why bother check for accurate input?
     * Ignoring it just rounds up to nearest!
     */
    if (! (delta % VRM11_RES))
      bits = 1 + delta / VRM11_RES;
  }
  return PUT_BITS(6, 1, bits);
}


/*
 * PLL tables used to map between hw scale register
 * value and actual frequencies given a fixed base.
 * The formula is (probably KnF specific)
 *    freq = Base * Feedback / Feedforward
 * where
 *    Base = 100 MHz
 *    FeedBack = ratio bits 5:0
 *    FeedForward = ratio bits 7:6 (00 -> 8, 01 -> 4, 10 -> 2, 11 -> 1)
 *
 * Overlapping ranges over feedback and feedforward values are
 * handled by range table(s) below such that lower frequencies
 * can be selected at a finer granularity.
 */

struct pll_tab {
  uint8_t	clk_div;		/* Feed forward */
  uint8_t	min_mul;		/* Lower feedback */
  uint8_t	max_mul;		/* Upper feedback */
  uint16_t	min_clk;		/* Lower frequency */
  uint16_t	max_clk;		/* Upper frequency */
  uint8_t	step_size;		/* Granularity */
}  cpu_tab[] = {			/* CPU PLL */
  { 1, 20, 40, 2000, 4000, 100},
  { 2, 20, 39, 1000, 1950, 50},
  { 4, 20, 39,  500,  975, 25},
}, gddr_tab[] = {			/* GDDR PLL */
  {1, 14, 30, 1400, 3000, 100},
  {2, 12, 27,  600, 1350, 50},
};

#define B_CLK                   100	/* Base clock (MHz) */

static uint16_t
ratio2freq(uint8_t ratio, struct pll_tab * tab, int tablen)
{
  uint16_t	fwd, bck;

  fwd = GET_BITS(7, 6, ~ratio);
  bck = GET_BITS(5, 0, ratio);

  if (fwd < tablen && bck >= tab[fwd].min_mul && bck <= tab[fwd].max_mul)
    return (B_CLK * bck) / tab[fwd].clk_div;

  return 0;
}

static uint8_t
freq2ratio(uint16_t freq, struct pll_tab * tab, int tablen)
{
  int		fwd;

  for(fwd = tablen - 1; fwd >= 0; fwd--) {
    if (freq >= tab[fwd].min_clk && freq <= tab[fwd].max_clk) {
      /*
       * Why bother check for accurate input?
       * Ignoring just rounds down to nearest supported!
       */
      if (freq % tab[fwd].step_size)
        break;

      return PUT_BITS(7, 6, ~fwd) |
      	     PUT_BITS(5, 0, (freq * tab[fwd].clk_div) / B_CLK);
    }
  }

  return 0;
}

static uint32_t
mr_mt_gf_r2f(uint8_t pll)
{
  return 1000 * ratio2freq(pll, gddr_tab, ARRAY_SIZE(gddr_tab));
}

static uint32_t
mr_mt_cf_r2f(uint8_t pll)
{
  return 1000 * ratio2freq(pll, cpu_tab, ARRAY_SIZE(cpu_tab));
}


/*
 * Board voltage sense converter
 * Two 10 bit read-outs from SBOX register 0x1038.
 * The format is very poorly documented, so no
 * warranty on this conversion. Assumption is
 * the reading is a binary fixed point number.
 *  bit 15 	Valid reading if set
 *  bit 9:8	2 bit integer part
 *  bit 7:0	8 bit fraction part
 * Return value is 0 (invalid) or voltage i uV.
 */

uint32_t
bvs2volt(uint16_t sense)
{
  uint32_t	res, f, msk;

  if (! GET_BIT(15, sense))
    return 0;

  /*
   * First get integer contribution
   * Then accumulate fraction contributions.
   * Divide and add fraction if corresponding bit set.
   */
  res = 1000000 * GET_BITS(9, 8, sense);
  for(msk = (1 << 7), f = 1000000/2; msk && f; msk >>= 1, f >>= 1)
    if (sense & msk)
      res += f;

  return res;
}



/*
**
** Initializations
**
** This has two intended purposes:
**  - Do a on-time effort to collect info on properties that
**    are not going to change after the initial setup by
**    either bootstrap or kernel initialization.
**  - Collect initial values on things we can modify.
**    Intent is that unloading the ras module should reset
**    all state to that of the time the module was loaded.
**
*/

static void __init
mr_mk_cf_lst(void)
{
  int		i, n;
  uint16_t	f;

  n = 0;
  for(i = ARRAY_SIZE(cpu_tab) -1; i >= 0; i--) {
    for(f = cpu_tab[i].min_clk;
	f <= cpu_tab[i].max_clk;
	f += cpu_tab[i].step_size) {
      freq.supt[n] = 1000 * f;
      freq.slen = ++n;
      if (n >= MR_PTAB_LEN)
	return;
    }
  }
}

static void __init
mr_mk_gf_lst(void)
{
  int		i, n;
  uint16_t	f;

  n = 0;
  for(i = ARRAY_SIZE(gddr_tab) -1; i >= 0; i--) {
    for(f = gddr_tab[i].min_clk;
	f <= gddr_tab[i].max_clk;
	f += gddr_tab[i].step_size) {
      gfreq.supt[n] = 1000 * f;
      gfreq.slen = ++n;
      if (n == MR_PTAB_LEN)
	return;
    }
  }
}

static void __init
mr_mk_cv_lst(void)
{
  int		n;
  uint32_t      cv;

  n = 0;
  for(cv = VRM11_MIN; cv <= VRM11_MAX; cv += VRM11_RES) {
    volt.supt[n] = cv;
    volt.slen = ++n;
    if (n >= MR_PTAB_LEN)
      return;
  }
}


void __init
mr_mt_card_init(void)
{
  uint8_t	* boot, * stage2, * parm;
  uint32_t	scr7, scr9, fsc;
  uint32_t	cv, cf, gv;
  int		i, j;

  /*
   * VERS:
   * Map flash and scan for version strings.
   * Different methods for KnF and KnC.
   */
  boot   = ioremap(MIC_SPI_BOOTLOADER_BASE, MIC_SPI_BOOTLOADER_SIZE);
  stage2 = ioremap(MIC_SPI_2ND_STAGE_BASE, MIC_SPI_2ND_STAGE_SIZE);
  parm   = ioremap(MIC_SPI_PARAMETER_BASE, MIC_SPI_PARAMETER_SIZE);
  if (!boot || !stage2 || !parm) {
    printk("mr_mt_init: ioremap failure: boot %p, stage2 %p, par %p\n",
	      boot, stage2, parm);
    goto fail_iomap;
  }

  /*
   * Build numbers for fboot0 and fboot 1 repectively
   */
  scr7 = mr_sbox_rl(0, SBOX_SCRATCH7);

  /*
   * Boot block scan:
   * Scan for string 'fboot0 version:' or use a 16 bit offset af offset 0xfff8.
   * The latter points directly to the numeral, not to the string mentioned.
   */
  for(i = 0; i < MIC_SPI_BOOTLOADER_SIZE - 32; i++) {
    if (boot[i] != 'f')
      continue;

    if (! memcmp(boot + i, "fboot0 version:", 15)) {
      vers.fboot0[0] = scnprintf(vers.fboot0 + 1, MR_VERS_LEN -2,
		    "%s (build %d)", boot + i, GET_BITS(15, 0, scr7));
      break;
    }
  }

  /*
   * Stage 2 scan:
   * Scan for the magic string that locates the bootstrap version. This
   * area is formatted as '<txt> (<\0>, <vers>)', so the string we are
   * looking for is 23 bytes later.
   */
  for(i = 0; i < MIC_SPI_2ND_STAGE_SIZE - 32; i++) {
    if (stage2[i] != 'L')
      continue;

    if (! memcmp(stage2 + i, "Larrabee bootstrap", 18)) {
      vers.fboot1[0] = scnprintf(vers.fboot1 + 1, MR_VERS_LEN -2,
		    "fboot1 version: %s", stage2 + i + 23);
      vers.fboot1[0] = scnprintf(vers.fboot1 + vers.fboot1[0], MR_VERS_LEN -2,
		    " (build %d)", GET_BITS(31, 16, scr7));
      break;
    }
  }

  /*
   * Parameter block scan:
   * On 4 byte aligned locations, look for chars 'EOB_'.
   * Numerical values for that string is 0x5f424f45.
   */
  for(i = j = 0; i < MIC_SPI_PARAMETER_SIZE; i += sizeof(uint32_t))
    if (*(uint32_t *)(parm + i) == 0x5f424f45) {
      vers.flash[j][0] = scnprintf(vers.flash[j] + 1, MR_VERS_LEN -2,
	      "flash %c%c%c%c version: %s",
	      parm[i+4], parm[i+5], parm[i+6], parm[i+7], parm + i + 32);
      if (++j >= ARRAY_SIZE(vers.flash))
	break;
    }

fail_iomap:
  if (boot)
    iounmap(boot);
  if (stage2)
    iounmap(stage2);
  if (parm)
    iounmap(parm);

#if USE_FSC
  /*
   * Reset SMC registers to default (MGBR cmd 0, data 0).
   */
  mr_sbox_wl(0, SBOX_SPEED_OVERRIDE_FAN, 0);

  /*
   * The MGBR Status has this layout for (MGBR command 0).
   *   7:0	Firmware version
   *  10:8	Card straps
   *     11	Fan disable
   *  20:12	Temperatur sensor 5
   *  27:21	Reserved
   *  31:28 	Command (0)
   */
#else
  /*
   * Contrary to register spec, the fan speed controller
   * 2 status register has been redefined to hold version
   * information of the FSC firmware.
   *   7:0	Revision
   *  10:8	FSC straps
   *     11	Fan disable
   *  19:12	Temperatur sensor 5
   *  27:20	Reserved
   *     28	BIOS clear
   *  31:29 	Reserved
   * This is probably an early version of the MGBR hack.
   */
#endif

  /*
   * Retrieve FSC version and strap config
   */
  fsc = mr_sbox_rl(0, SBOX_STATUS_FAN2);
  vers.fsc[0] = scnprintf(vers.fsc + 1, MR_VERS_LEN -2,
      "FSC firmware revision: %02x, straps %x",
      GET_BITS(7, 0, fsc), GET_BITS(10, 8, fsc));

  /*
   * VOLT:
   * Report all voltages the hardware can set.
   */
  cv = mr_sbox_rl(0, SBOX_COREVOLT);
  volt.set = vid2volt(GET_BITS(7, 0, cv));
  mr_mk_cv_lst();

  /*
   * FREQ:
   * In FreeBSD uOS the reference (nominal) frequency
   * is simply the value read from the SBOX at boot time.
   * We'll do the same and set 'def' to the same as 'current'.
   * Report all voltages the hardware can set.
   */
  cf = mr_sbox_rl(0, SBOX_COREFREQ);
  freq.def = mr_mt_cf_r2f(GET_BITS(7, 0, cf));
  mr_mk_cf_lst();

  /*
   * GDDR:
   * See layout of scratch #9 in 'common'.
   * 23:16	Clock ratio encoding
   * 28:24	External clock frequency
   */
  scr9 = mr_sbox_rl(0, SBOX_SCRATCH9);
  gddr.speed = 2 * mr_mt_gf_r2f(GET_BITS(23, 16, scr9));

  /*
   * GVOLT:
   * Report all voltages the hardware can set.
   * Kind of silly as these cannot be changed from uOS.
   * Cheat and set 'def' to the same as 'current'.
   */
  gv = mr_sbox_rl(0, SBOX_MEMVOLT);
  gvolt.set = vid2volt(GET_BITS(7, 0, gv));

  /*
   * GFREQ:
   * Report all values the hardware can set.
   * Kind of silly as these cannot be changed from uOS.
   * Cheat and set 'ref' to the same as 'current'.
   */
  gfreq.def = mr_mt_gf_r2f(GET_BITS(23, 16, scr9));
  mr_mk_gf_lst();

  /*
   * POWER:
   * If case FSC not working or if not compiled in,
   * preset all power readings as invalid.
   */
  {
    struct mr_rsp_power tmp = {{0, 3}, {0, 3}, {0, 3},
  	   		       {0, 3}, {0, 3}, {0, 3}, {0, 3},
           		       {0, 0, 0, 3, 3, 3},
	   		       {0, 0, 0, 3, 3, 3},
	   		       {0, 0, 0, 3, 3, 3}};
    power = tmp;
  }

  /*
   *TBD: Save card registers this module may change
   */
}

void __exit
mr_mt_card_exit(void)
{
  /*
   *TBD: Restore card registers this module may change
   */
}



/*
**
** Card specific 'Get' functions
**
*/

int
mr_get_volt(void * p)
{
  struct mr_rsp_volt  * r;
  uint32_t		cv, fsc;


  cv = mr_sbox_rl(0, SBOX_COREVOLT);
  volt.set = vid2volt(GET_BITS(7, 0, cv));

  fsc = mr_sbox_rl(0, SBOX_BOARD_VOLTAGE_SENSE);
  volt.cur = bvs2volt(GET_BITS(15, 0, fsc));

  r = (struct mr_rsp_volt *) p;
  *r = volt;
  return sizeof(*r);
}

int
mr_get_freq(void * p)
{
  struct mr_rsp_freq  * r;
  uint32_t		cf;

  cf = mr_sbox_rl(0, SBOX_COREFREQ);
  freq.cur = mr_mt_cf_r2f(GET_BITS(7, 0, cf));

  r = (struct mr_rsp_freq *) p;
  *r = freq;
  return sizeof(*r);
}

#if USE_FSC
/*
 * Get Power stats from the FSC
 */
static void
get_fsc_pwr(uint32_t req, struct mr_rsp_pws * pws)
{
  uint32_t	fsc;
  
  /*
   * Read the FSC status
   */
  fsc_mgbr_write(MR_FSC_MGBR_GEN_CMD, &req);
  if (fsc_mgbsr_read(&fsc))
    pws->p_val = 3;
  else {
    pws->p_val = 0;
    pws->prr = 1000000 * GET_BITS(15, 0, fsc);
  }
}
#endif

int
mr_get_power(void * p)
{
  struct mr_rsp_power * r;

#if USE_FSC
  uint8_t	prev_cmd;
  uint32_t	prev_dat;

  /*
   * Backup current OVERRIDE register
   */
  prev_cmd = mgbr_cmd;
  prev_dat = mgbr_dat;
  
  /*
   * Get Power stats from the FSC
   */
  get_fsc_pwr(MR_FSC_PWR_TOT,  &power.tot0);
  get_fsc_pwr(MR_FSC_PWR1_TOT, &power.inst);
  get_fsc_pwr(MR_FSC_PWR_PCIE, &power.pcie);
  get_fsc_pwr(MR_FSC_PWR_2X3,  &power.c2x3);
  get_fsc_pwr(MR_FSC_PWR_2X4,  &power.c2x4);

  /*
   * Revert to normal or fan 1 speed override mode if needed.
   */
  if (fan1_ovr)
    fsc_mgbr_write(MR_FSC_MGBR_OVR_CMD, &fan1_ovr);
  else
    fsc_mgbr_write(prev_cmd, &prev_dat);
#endif
  
  r = (struct mr_rsp_power *) p;
  *r = power;
  return sizeof(*r);
}


int
mr_get_plim(void * p)
{
  struct mr_rsp_plim  * r;

#if USE_FSC
  uint32_t	fsc, req, ofs;
  
  /*
   * Read the FSC status
   */
  req = MR_FSC_PMC_CFG;
  fsc_mgbr_write(MR_FSC_MGBR_GEN_CMD, &req);
  if (! fsc_mgbsr_read(&fsc)) {
    ofs = 5 * GET_BITS(3, 0, fsc);
    if (GET_BIT(4, fsc))
      plim.phys = 300 - ofs;
    else 
      plim.phys = 300 + ofs;
    plim.hmrk = plim.lmrk = plim.phys;
  }
#endif

  r = (struct mr_rsp_plim *) p;
  *r = plim;
  return sizeof(*r);
}


int
mr_get_gfreq(void * p)
{
  struct mr_rsp_gfreq * r;
  uint32_t		gbr;

  gbr = mr_sbox_rl(0, SBOX_MEMORYFREQ);
  gfreq.cur = mr_mt_gf_r2f(GET_BITS(7, 0, gbr));

  r = (struct mr_rsp_gfreq *) p;
  *r = gfreq;
  return sizeof(*r);
}


int
mr_get_gvolt(void * p)
{
  struct mr_rsp_gvolt * r;
  uint32_t		gv, fsc;

  gv = mr_sbox_rl(0, SBOX_MEMVOLT);
  gvolt.set = vid2volt(GET_BITS(7, 0, gv));

  fsc = mr_sbox_rl(0, SBOX_BOARD_VOLTAGE_SENSE);
  gvolt.cur = bvs2volt(GET_BITS(31, 16, fsc));

  r = (struct mr_rsp_gvolt *) p;
  *r = gvolt;
  return sizeof(*r);
}

int
mr_get_temp(void * p)
{
  struct mr_rsp_temp  * r;
  uint32_t		btr1, btr2;		/* Board temps */
  uint32_t		die1, die2, die3;	/* Die temps */
  uint32_t		dmx1, dmx2, dmx3;	/* Max die temps */
  uint32_t		tsta, fsc;		/* Thermal status */

  btr1 = mr_sbox_rl(0, SBOX_BOARD_TEMP1);
  btr2 = mr_sbox_rl(0, SBOX_BOARD_TEMP2);
  die1 = mr_sbox_rl(0, SBOX_CURRENT_DIE_TEMP0);
  die2 = mr_sbox_rl(0, SBOX_CURRENT_DIE_TEMP1);
  die3 = mr_sbox_rl(0, SBOX_CURRENT_DIE_TEMP2);
  dmx1 = mr_sbox_rl(0, SBOX_MAX_DIE_TEMP0);
  dmx2 = mr_sbox_rl(0, SBOX_MAX_DIE_TEMP1);
  dmx3 = mr_sbox_rl(0, SBOX_MAX_DIE_TEMP2);
  tsta = mr_sbox_rl(0, SBOX_THERMAL_STATUS);
  fsc  = mr_sbox_rl(0, SBOX_STATUS_FAN2);

  /*
   * Board temperatures.
   * No idea of where on the board they are located, but
   * guessing from FreeBSD comments they are:
   *   0	Air Inlet
   *   1	VCCP VR
   *   2	GDDR (not sure which chip)
   *   3	GDDR VR
   * The temperature read from FSC #2 seems valid, but
   * there's no mention of where it's measured.
   * The readings does not make much sense.
   *       Sample readings are like this:
   *         fin   32
   *	     vccp  28	(vccp VR)
   *	     vddq  33	(gddr VR)
   *         vddg  28	(FSC 2)
   *	   So, at least 'fin' is wrong (or fan in reverse).
   */
  temp.fin.cur  = (btr1 & (1 << 15)) ? GET_BITS( 8,  0, btr1) : 0;
  temp.vccp.cur = (btr1 & (1 << 31)) ? GET_BITS(24, 16, btr1) : 0;
  temp.gddr.cur = (btr2 & (1 << 15)) ? GET_BITS( 8,  0, btr2) : 0;
  temp.vddq.cur = (btr2 & (1 << 31)) ? GET_BITS(24, 16, btr2) : 0;
  temp.vddg.cur = GET_BITS(19, 12, fsc);
  temp.brd.cur = 0;
  if (temp.fin.cur > temp.brd.cur)
    temp.brd.cur = temp.fin.cur;
  if (temp.vccp.cur > temp.brd.cur)
    temp.brd.cur = temp.vccp.cur;
  if (temp.gddr.cur > temp.brd.cur)
    temp.brd.cur = temp.gddr.cur;
  if (temp.vddq.cur > temp.brd.cur)
    temp.brd.cur = temp.vddq.cur;
  temp.fout.c_val = 3;
  temp.gddr.c_val = 3;

  /*
   * Die temperatures.
   */
  temp.die.cur = (tsta & (1 << 31)) ? GET_BITS(30, 22, tsta) : 0;
  temp.dies[0].cur = GET_BITS( 8,  0, die1);
  temp.dies[1].cur = GET_BITS(17,  9, die1);
  temp.dies[2].cur = GET_BITS(26, 18, die1);
  temp.dies[3].cur = GET_BITS( 8,  0, die2);
  temp.dies[4].cur = GET_BITS(17,  9, die2);
  temp.dies[5].cur = GET_BITS(26, 18, die2);
  temp.dies[6].cur = GET_BITS( 8,  0, die3);
  temp.dies[7].cur = GET_BITS(17,  9, die3);
  temp.dies[8].cur = GET_BITS(26, 18, die3);

  /*
   * Die max temp (min is not reported to CP).
   */
  temp.dies[0].max = GET_BITS( 8,  0, dmx1);
  temp.dies[1].max = GET_BITS(17,  9, dmx1);
  temp.dies[2].max = GET_BITS(26, 18, dmx1);
  temp.dies[3].max = GET_BITS( 8,  0, dmx2);
  temp.dies[4].max = GET_BITS(17,  9, dmx2);
  temp.dies[5].max = GET_BITS(26, 18, dmx2);
  temp.dies[6].max = GET_BITS( 8,  0, dmx3);
  temp.dies[7].max = GET_BITS(17,  9, dmx3);
  temp.dies[8].max = GET_BITS(26, 18, dmx3);

  r = (struct mr_rsp_temp *) p;
  *r = temp;
  return sizeof(*r);
}


int
mr_get_fan(void * p)
{
  struct mr_rsp_fan   * r;
  uint32_t		fan1, fovr;

  r = (struct mr_rsp_fan *) p;
  fan1 = mr_sbox_rl(0, SBOX_STATUS_FAN1);

#if USE_FSC
  fovr = fan1_ovr;
  r->override = GET_BIT(9, fovr);
#else
  fovr = mr_sbox_rl(0, SBOX_SPEED_OVERRIDE_FAN);
  r->override = GET_BIT(15, fovr);
#endif

  r->rpm      = GET_BITS(15,  0, fan1);
  if (r->override)
    r->pwm    = GET_BITS( 7,  0, fovr);
  else
    r->pwm    = GET_BITS(23, 16, fan1);

  return sizeof(*r);
}


int
mr_get_ecc(void * p)
{
  struct mr_rsp_ecc   * r;

  r = (struct mr_rsp_ecc *) p;
  *r = ecc;
  return sizeof(*r);
}


int
mr_get_trbo(void * p)
{
  struct mr_rsp_trbo   * r;

  r = (struct mr_rsp_trbo *) p;
  *r = trbo;
  return sizeof(*r);
}


int
mr_get_pmcfg(void * p)
{
  struct mr_rsp_pmcfg * r;

  r = (struct mr_rsp_pmcfg *) p;
  *r = pmcfg;
  return sizeof(*r);
}


/*
**
** Card specific 'Set' functions
** Input screening takes place here (to the extent possible).
**
*/


int
mr_set_volt(void * p)
{
  uint32_t	cv, msk, new, val;
  uint8_t	vid;
  int		i;

  /*
   * Ensure it's a supported value
   */
  val = *(uint32_t *) p;
  for(i = 0; i < MR_PTAB_LEN; i++)
    if (volt.supt[i] == val)
      break;
  if (i == MR_PTAB_LEN)
    return -MR_ERR_RANGE;

  /*
   * Read-modify-write the core voltage VID register
   */
  vid = volt2vid(val);
  cv = mr_sbox_rl(0, SBOX_COREVOLT);
  msk = ~PUT_BITS(7, 0, ~0);
  new = (cv & msk) | PUT_BITS(7, 0, vid);
  mr_sbox_wl(0, SBOX_COREVOLT, new);
  printk("SetVolt: %d -> %08x (%08x)\n", val, new, cv);

  return 0;
}


int
mr_set_freq(void * p)
{
  uint32_t	cf, msk, new, val;
  uint8_t	rat;
  int		i;

  /*
   * Ensure it's a supported value
   */
  val = *(uint32_t *) p;
  for(i = 0; i < MR_PTAB_LEN; i++)
    if (freq.supt[i] == val)
      break;
  if (i == MR_PTAB_LEN)
    return -MR_ERR_RANGE;

  /*
   * Read-modify-write the core frequency PLL register
   *
   *TBD: or should we just overwrite it?
   *     Register fields (of relevance):
   *       7:0	New PLL encoding
   *	     16	Async Operation
   *	     31	Override fuse setting
   */
  rat = freq2ratio(val/1000, cpu_tab, ARRAY_SIZE(cpu_tab));
  cf = mr_sbox_rl(0, SBOX_COREFREQ);
  msk = ~(PUT_BITS(7, 0, ~0) | PUT_BIT(16, 1) | PUT_BIT(31, 1));
  new = (cf & msk) | PUT_BITS(7, 0, rat) | PUT_BIT(31, 1);
  mr_sbox_wl(0, SBOX_COREFREQ, new);
  printk("SetFreq: %d -> %08x (%08x)\n", val, new, cf);

  /*
   *TBD:
   * We just changed the system's base clock without
   * re-calibrating the APIC timer tick counters.
   * There is probably a function call for the cpu-freq
   * driver to deal with this, so should we call it?
   */

  return 0;
}


int
mr_set_plim(void * p)
{
  plim.phys = *(uint32_t *) p;
  return 0;
}


int
mr_set_fan(void * p)
{
  struct mr_set_fan   * fc;

  /*
   * Ensure operation is valid, i.e. no garbage
   * in override flag (only 1 and 0 allowed) and
   * that pwm is not zero (or above lower limit?)
   */
  fc = (struct mr_set_fan *) p;
  if (GET_BITS(7, 1, fc->override) || !fc->pwm)
    return -MR_ERR_RANGE;

#if USE_FSC
  {
    uint32_t	dat;

    /*
     * Craft the default OVERRIDE command and write it to FSC
     * through the MGBR register (command 0).
     * This does not change the telemetry in MGBSR, so only way
     * to ensure it gets registered by FSC is to wait it out
     * (happens in fsc_mgbr_write function).
     */
    if (fc->override)
      dat = PUT_BIT(9, 1) | fc->pwm;
    else
      dat = 0;
    fsc_mgbr_write(MR_FSC_MGBR_OVR_CMD, &dat);
  }
#else
  /*
   * Read-modify-write the fan override register
   * Control of fan #1 only, don't touch #2
   */
  {
    uint32_t		fcor, fco1, fco2;

    fcor = mr_sbox_rl(0, SBOX_SPEED_OVERRIDE_FAN);
    fco2 = GET_BITS(31, 16, fcor);
    if (fc->override)
      fco1 = PUT_BIT(15, 1) | fc->pwm;
    else
      fco1 = 0;
    mr_sbox_wl(0, SBOX_SPEED_OVERRIDE_FAN,
  		PUT_BITS(31, 16, fco2) | PUT_BITS(15, 0, fco1));
  }
#endif

  return 0;
}


int
mr_set_trbo(void * p)
{
  return 0;
}

