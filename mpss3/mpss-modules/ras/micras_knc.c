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
 * Code and data structures to handle get/set tasks for KnC.
 * Parties accessing the data structures are supposed to use the
 * micras_mt_tsk() routines to ensure integrity and consistency.
 * Particularly important when handling sysfs nodes and actions
 * requested from SCIF connections must use that method in order
 * to guarantee serialized access.
 *
 * Even if read-only access to latest valid data is required,
 * it should go through micras_mt_tsk() using dedicated handlers
 * in this module.
 *
 * Apologies for the messy code, but hardware support to report
 * board properties at this time (Power-On of A0) is so erratic
 * that odd ways of obtaining the info had to replace the POR
 * methods. The SMC support is sporadic, A0 has issues with SVID
 * and some SBOX registers are invalid because they depend on
 * TMU telemetry transmissions from the SMC which some reason
 * has been forgotten/missed/defeatured (does not happen).
 *
 * TBD: Once the dust settles there will be code to remove.
 *      But until then, lots of #ifdef's remains.
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
#include <asm/mic/mic_knc/autobaseaddress.h>
#include <asm/mic/mic_knc/micsboxdefine.h>
#include "micras.h"


/*
 * Persistent data accessible through the CP api.
 * Some functions just read/modify hardware CSRs
 * and thus need no storage between invocations.
 */

extern struct mr_rsp_hwinf	hwinf;
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

#if USE_SVID
static uint8_t	vccp_cap,  vddq_cap,  vddg_cap;
static uint8_t	vccp_imax, vddq_imax, vddg_imax;
#endif

uint8_t	xlat_cpu[NR_CPUS];

#define FIX_DBOX	1

#if FIX_DBOX
/*
 * Pre-emptive restoring DBOX-0 register access.
 * A glitch during clock speed changes (PM or GPU_HOT)
 * may under some rare circumstances break access to DBOX
 * registers. It is very rare, requires hours of tailored
 * simulation to reproduce, never seen in the wild (yet).
 * The gmbus controller sits in the DBOX and is affected.
 * Calling this routine prior to every gmbus read/write
 * reduces risk of hitting this bug to a single SMC register,
 * which has been deemed acceptable for B-step KnCs.
 * Only alternative is to perform repeated transaction(s)
 * until a stable result is obtained, which will be costly
 * in performance.
 */
static void
mr_smc_deglitch(void)
{
  mr_dbox_rl(0, 0x600);
  mr_dbox_rl(0, 0x2440);
}
#else
#define mr_smc_deglitch();	/* As nothing */
#endif


/*
**
** Conversion between CP formats (uV, MHz, etc.)
** and hardware register formats (SMC and VRs mostly).
**
*/


/*
 * PLL tables used to map between hw scale register
 * value and actual frequencies given a fixed base.
 *
 * The core frequency (MCLK) formula is
 *    freq = Icc * (Feedback / Feedforward)
 * where
 *    Icc = Frequency generated from ICC, nominal 200 MHz
 *    FeedBack = ratio bits 8:1 (valid range: 8 .. 16)
 *    FeedForward = ratio bits 10:9 (01 -> 4, 10 -> 2, 11 -> 1)
 *
 * The gddr frequency (PGCLK) formula is
 *    freq = (X / 2) * Feedback / Feedforward
 * where
 *    X = SBPLL (ICC)      Table 1, FB range 10..22
 *    X = LCVCO (ICC/2)    Table 2, FB range 44..65
 *    X = Bypass (ICC/2)   Table 3, FB range 20..44
 * which is why there's three gddr tables. The divide by 2 of
 * 'X' is represented as doubling the FF dividers in the tables.
 *
 * Overlapping ranges over feedback and feedforward values are
 * handled by range table(s) below such that lower frequencies
 * can be selected at a finer granularity. The tables themselves
 * do not allow overlaps, i.e. two ways to specify the same
 * PLL output frequency.
 *
 * Note that ICC clocks have their own PLL built in which uses
 * the PCI-E 100 MHz clock, adds SSC and scale it by a pair of
 * dividers. One divider is (I'm told) fixed at 40, the other
 * is fused, and none of them can be read from uOS at runtime.
 * The fused dividers are nominally 20, which is what the
 * tables below is based on. Some SKUs tweak the core ICC PLL
 * by fuses, so to counter it that divider is reported in scr #4.
 * No means to know if gddr ICC PLL gets tweaked too.
 *
 *WARNING: there are overlabs on the divider codes for GDDR PLLs,
 *         which theoretically can cause false reporting of GDDR
 *	   device speeds (example: FB dividers 20, 21, and 22 are
 *	   defined both in gddr_tab1 and gddr_tab3). Currently
 *         there is no way to determine which table is used.
 */

struct pll_tab {
  uint8_t       clk_div;                /* Feed forward */
  uint8_t       min_mul;                /* Lower feedback */
  uint8_t       max_mul;                /* Upper feedback */
  uint16_t      min_clk;                /* Lower frequency */
  uint16_t      max_clk;                /* Upper frequency */
  uint8_t       step_size;              /* Granularity */
}  cpu_tab[] = {                        /* CPU PLL, ICC @ ~200 MHz */
  {1,  8, 16, 1600, 3200, 200},
  {2,  8, 15,  800, 1500, 100},
  {4,  8, 15,  400,  750,  50},
}, gddr_tab1[] = {                      /* GDDR PLL, ICC @ 200 MHz */
  {2, 10, 22, 1000, 2200, 100},
  {4, 10, 22,  500, 1100,  50},
  {8, 10, 22,  250,  550,  25},
}, gddr_tab2[] = {                      /* GDDR PLL, LCVCO @ 100 MHz */
  {2, 44, 65, 2200, 3250,  50},
}, gddr_tab3[] = {                      /* GDDR PLL, ICC bypass @ 100 MHz */
  {2, 20, 44, 1000, 2200, 100},
  {4, 20, 44,  500, 1100,  50},
  {8, 20, 44,  250,  550,  25},
};

#define ICC_NOM		20		/* Nominal ICC feed back divider */

static uint16_t
ratio2freq(uint16_t ratio, struct pll_tab * tab, int tablen, uint16_t base)
{ 
  uint16_t      fwd, bck;
  
  fwd = GET_BITS(10, 9, ~ratio);
  bck = GET_BITS(8, 1, ratio);

  if (tab == gddr_tab3 && (bck & 1))
    return 0;

  if (fwd < tablen && bck >= tab[fwd].min_mul && bck <= tab[fwd].max_mul)
    return (base * bck) / tab[fwd].clk_div;

  return 0;
}

static uint16_t
freq2ratio(uint16_t freq, struct pll_tab * tab, int tablen, uint16_t base)
{
  int		fwd;

  for(fwd = tablen - 1; fwd >= 0; fwd--) {
    if (freq >= tab[fwd].min_clk && freq <= tab[fwd].max_clk) {
      /*
       * Why bother check for accurate input?
       * Ignoring it just rounds down to nearest supported!
       */
      if (freq % tab[fwd].step_size)
        break;

      return PUT_BITS(10, 9, ~fwd) |
             PUT_BITS( 8, 1, (freq * tab[fwd].clk_div) / base);
    }
  }

  return 0;
}

static uint32_t
icc_fwd(void)
{
  uint32_t	scr4, div;

  scr4 = mr_sbox_rl(0, SBOX_SCRATCH4);
  div = GET_BITS(29, 25, scr4);

  return div ? div : ICC_NOM;
}

static uint32_t
mr_mt_gf_r2f(uint16_t pll)
{
  uint64_t	freq;

  /*
   * As per HSD 4118175, ICC clock at 200 MHz is currently not
   * used on any SKUs, and is unlikely to be used in the future.
   * Therefore, the 100 MHz tables are searched first.
   */
  freq = ratio2freq(pll, gddr_tab3, ARRAY_SIZE(gddr_tab3), 100);
  if (! freq)
    freq = ratio2freq(pll, gddr_tab2, ARRAY_SIZE(gddr_tab2), 100);
  if (! freq)
    freq = ratio2freq(pll, gddr_tab1, ARRAY_SIZE(gddr_tab1), 200);

  return 1000 * freq;
}

static uint32_t
mr_mt_cf_r2f(uint16_t pll)
{
  uint64_t	freq;

  freq = ratio2freq(pll, cpu_tab, ARRAY_SIZE(cpu_tab), 200);

  return (1000 * freq * ICC_NOM) / icc_fwd();
}


#if USE_SVID
/*
 * VRM12 voltage converters
 * Only bits 7:0 are being used as follows:
 *   Volt = Min + Res * (Bits -1)
 *   Bits = 1 + (Volt - Min) / Res
 * Bits value of 0 reserved for turning VR off.
 */

#define VRM12_MAX	1520000		/* 1.52 V */
#define VRM12_MIN	 250000		/* 250 mV */
#define VRM12_RES	   5000		/* 5.0 mV */

static uint32_t
svid2volt(uint8_t svid)
{
  uint32_t	bits;

  bits = GET_BITS(7, 0, svid);
  if (bits)
    return VRM12_MIN + VRM12_RES * (bits - 1);
  else
    return 0;
}

static uint8_t
volt2svid(uint32_t uv)
{
  uint32_t	delta, bits;

  bits = 0;
  if (uv >= VRM12_MIN && uv <= VRM12_MAX) {
    delta = uv - VRM12_MIN;
    /*
     * Why bother check for accurate input?
     * Ignoring it just rounds up to nearest!
     */
    if (! (delta % VRM12_RES))
      bits = 1 + delta / VRM12_RES;
  }
  return PUT_BITS(7, 0, bits);
}


/*
 * SVID register scaling:
 *
 *  Vin  = SVID_REG(0x1A) <unknown>
 *  Iin  = SVID_REG(0x19) 1:1 A
 *  Pin  = SVID_REG(0x1B) 1:1 W
 *  Vout = SVID_REG(0x16) / 128 V
 *  Iout = SVID_REG(0x15) 1:1 A
 *  Pout = SVID_REG(0x18) 1:1 W
 *  Iout = (SVID_REG(0x15) / ADCmax) * (SVID_REG(0x21) A
 *  Temp = SVID_REG(0x17) 1:1 C
 *
 *  Note: SVID_REG(0x06) bit 7 tells Iout formula.
 *	 Assuming 8-bit ADC => ADCmax to be 0xff.
 *
 * Inputs are SVID register values, outputs are u{V|A|W}.
 */

static uint32_t
vout2volt(uint8_t vout)
{
  /*
   * Linear range from 0 to 2 volt
   */
  return (((uint32_t) vout) * 1000000) / 128;
}

static uint32_t
vin2volt(uint8_t vin)
{
  /*
   * Formula not known.
   */
  return (((uint32_t) vin) * 1000000) / 128;
}

static uint32_t
one2one(uint8_t in)
{
  return ((uint32_t) in) * 1000000;
}

static uint32_t
iout2amp(uint8_t iout, uint8_t cap, uint8_t imax)
{
  if (GET_BITS(7, 7, cap))
    return (((uint32_t) iout) * ((uint32_t) imax) * 1000000) / 256;
  else
    return one2one(iout);
}

#define iin2amp(iin)	one2one(iin)
#define pin2watt(pin)	one2one(pin)
#define pout2watt(pout)	one2one(pout)



/*
**
** Simple SVIDCONTROL interface.
**
**    0		Parity bit out
**  8:1		SVID data out
** 13:9		SVID command
** 17:14	SVID address
**    18	Parity bit in (if any)
** 26:19	SVID data in (if any)
**    27	ACK #0
**    28	ACK #1
**    29	SVID Error
**    30	CTL Idle
**    31	CMD Start
**
** See SBOX HAS for more details.
** One transaction is expected to finish
** in less than 2 uSec (15.625 MHz clock)
** and busy waiting here should be OK.
**
** Return values:
**  0	OK
** 1-7	Controller bits 29:27
**  8	Parameter error (invalid device or opcode)
**
*/

/*
 * SVID command set
 * Source: SVID Protocol rev 1.5
 */
#define VR12Cmd_Extend		0x00    /* Req */
#define VR12Cmd_SetVID_Fast	0x01    /* Req */
#define VR12Cmd_SetVID_Slow	0x02    /* Req */
#define VR12Cmd_SetVID_Decay	0x03    /* Req */
#define VR12Cmd_SetPS		0x04    /* Req */
#define VR12Cmd_SetRegADR	0x05    /* Req */
#define VR12Cmd_SetRegDAT	0x06    /* Req */
#define VR12Cmd_GetReg		0x07    /* Req */
#define VR12Cmd_TestMode	0x08    /* Req */

/*
 * SVID registers of interest
 * Source: SVID Protocol rev 1.5
 *
 * Notes on the capability register:
 *  bit  0 Iout (0x15)
 *  bit  1 Vout (0x16)
 *  bit  2 Pout (0x18)
 *  bit  3 Iin  (0x19)
 *  bit  4 Vin  (0x1a)
 *  bit  5 Pin  (0x1b)
 *  bit  6 Temp (0x17)
 *  bit  7 Iout format of register 0x15
 *	0 -> value in Amps
 *	1 -> value scaled to Icc_Max
 */

#define VR12Reg_VendorID	0x00    /* Req */
#define VR12Reg_ProductID	0x01    /* Req */
#define VR12Reg_ProductRev	0x02    /* Req */
#define VR12Reg_ProductDate	0x03    /* Opt */
#define VR12Reg_LotCode		0x04    /* Opt */
#define VR12Reg_ProtocolID	0x05    /* Req */
#define VR12Reg_Capability	0x06    /* Req */
#define VR12Reg_Iout		0x15    /* Req */
#define VR12Reg_Vout		0x16    /* Opt */
#define VR12Reg_Temp		0x17    /* Opt */
#define VR12Reg_Pout		0x18    /* Opt */
#define VR12Reg_Iin		0x19    /* Opt */
#define VR12Reg_Vin		0x1a    /* Opt */
#define VR12Reg_Pin		0x1b    /* Opt */
#define VR12Reg_Icc_Max		0x21    /* Req */
#define VR12Reg_Temp_Max	0x22    /* Req */
#define VR12Reg_Vout_Max	0x30    /* Req */
#define VR12Reg_VID_Set		0x31    /* Req */

/*
 * SVID addresses on KnC
 */
#define SVID_VCCP	0x0	/* Core rail */
#define SVID_VDDQ	0x2	/* Memory rail (1st loop) */
#define SVID_VDDG	0x3	/* Uncore rail (2nd loop) */

static DEFINE_SPINLOCK(svidcontrol_lock);

static int
SvidCmd(uint8_t dev, uint8_t op, uint8_t in)
{
  uint32_t	cmd, ret, err;

  /*
   * The SVID Controller does not work in A0 (HSD 3498464)
   * Pretend success, but return 0 always
   */
  return 0;

  /*
   * For now just check that command can be contructed.
   *
   *TBD: Add stricter parameter check?
   */
  if (dev > GET_BITS(17, 14, ~0) ||
      op  > GET_BITS(13,  9, ~0))
    return -MR_ERR_SMC;

  /*
   * Craft 18 bit command with even parity
   */
  cmd = PUT_BITS( 8,  1, in) |
  	PUT_BITS(13,  9, op) |
	PUT_BITS(17, 14, dev);
  if (bitmap_weight((unsigned long *) &cmd, 18) & 1)
    cmd |= 1;

  /*
   * Wait until controller in idle state,
   * write command + start bit and then
   * wait for controller to be idle again.
   */
  spin_lock(&svidcontrol_lock);
  for( ;; ) {
    ret = mr_sbox_rl(0, SBOX_SVIDCONTROL);
    if (GET_BITS(31, 30, ret) == 0x1)
      break;
  }
  mr_sbox_wl(0, SBOX_SVIDCONTROL, cmd | PUT_BIT(31, 1));
  for( ;; ) {
    ret = mr_sbox_rl(0, SBOX_SVIDCONTROL);
    if (GET_BITS(31, 30, ret) == 0x1)
      break;
  }
  spin_lock(&svidcontrol_lock);

  /*
   * Report command status
   * Only if SVID_Error = 0, Ack #1 = 1, and Ack #0 = 0
   * did we have a successful transfer, and have data
   * to return (SBOX HAS table 9).
   */
  err = GET_BITS(29, 27, ret);
  return (err == 0x2) ? GET_BITS(26, 19, ret) : -MR_ERR_SMC;
}
#endif



/*
**
** SMC API
**
** See "Knights Corner System Managment Architecture Specification"
** for details on the SMC internals and supported APIs.
**
** This module is based on rev 0.31
**
*/

#define MR_SMC_ADDR		0x28	/* SMC DVO-B Slave address */

#define MR_SMC_PCI_VID		0x00	/* PCI Vendor ID, 4 */
#define MR_SMC_PCI_DID		0x02	/* PCI Device ID, 4 */
#define MR_SMC_PCI_BCC		0x04	/* PCI Base Class Code, 4 */
#define MR_SMC_PCI_SCC		0x05	/* PCI Sub Class Code, 4 */
#define MR_SMC_PCI_PI		0x06	/* PCI Programming Interface, 4 */
#define MR_SMC_PCI_SMBA		0x07	/* PCI MBus Manageability Address, 4 */
#define MR_SMC_UUID		0x10	/* Universally Unique Identification, 16 */
#define MR_SMC_FW_VERSION	0x11	/* SMC Firmware Version, 4 */
#define MR_SMC_EXE_DOMAIN	0x12	/* SMC Execution Domain, 4 */
#define MR_SMC_STS_SELFTEST	0x13	/* SMC Self-Test Results, 4 */
#define MR_SMC_HW_REVISION	0x14	/* SMC Hardware Revision, 4 */
#define MR_SMC_SERIAL		0x15	/* Card serial number, 12 */
#define MR_SMC_SMB_RESTRT	0x17	/* Restart SMBus addr negotiation, 4 */

#define MR_SMC_CPU_POST		0x1a	/* POST Register, 4 */
#define MR_SMC_ZOMBIE		0x1b	/* Zombie Mode Enable, 4 */
#define MR_SMC_CPU_ID		0x1c	/* CPU Identifier, 4 */

#define MR_SMC_SEL_ENTRY_SEL	0x20	/* SEL Entry Selection Register, 4 */
#define MR_SMC_SEL_DATA		0x21	/* SEL Data register, <N> */
#define MR_SMC_SDR_ENTRY_SEL	0x22	/* SDR Entry Selection Register, 4 */
#define MR_SMC_SDR_DATA		0x23	/* SDR Data register, <N> */

#define MR_SMC_PWR_PCIE		0x28	/* PCIe Power Reading, 4 */
#define MR_SMC_PWR_2X3		0x29	/* 2x3 Power Reading, 4 */
#define MR_SMC_PWR_2X4		0x2a	/* 2x4 Power Reading, 4 */
#define MR_SMC_FORCE_TTL	0x2b	/* Forced Throttle, 4 */
#define MR_SMC_PWR_LIM_0	0x2c	/* Power Limit 0, 4 */
#define MR_SMC_TIME_WIN_0	0x2d	/* Time Window 0, 4 */
#define MR_SMC_PWR_LIM0_GRD	0x2e	/* Power Limit 0 Guardband, 4 */
#define MR_SMC_PWR_LIM_1	0x2f	/* Power Limit 1, 4 */
#define MR_SMC_TIME_WIN_1	0x30	/* Time Window 1, 4 */
#define MR_SMC_INCL_3V3		0x31	/* Include 3.3 V, 4 */
#define MR_SMC_PWR_LIM_PERS	0x32	/* Power Limit Persistence, 4 */
#define MR_SMC_CLAMP_MODE	0x33	/* Clamp Mode, 4 */
#define MR_SMC_ENERGY_STS_0	0x34	/* Energy Status 0, 4 */
#define MR_SMC_AVG_PWR_0	0x35	/* Average Power 0, 4 */
#define MR_SMC_AVG_PWR_1	0x36	/* Average Power 1, 4 */
#define MR_SMC_MIN_PWR		0x37	/* Min Power, 4 */
#define MR_SMC_PWR_TTL_DUR	0x38	/* Power Throttle Duration, 4 */
#define MR_SMC_PWR_TTL		0x39	/* Power Throttling, 4 */
#define MR_SMC_PWR_INST		0x3a	/* Instantaneous Power Reading, 4 */
#define MR_SMC_PWR_IMAX		0x3b	/* Maximum Power Reading, 4 */
#define MR_SMC_VOLT_VCCP	0x3c	/* VCCP VR Output Voltage, 4 */
#define MR_SMC_VOLT_VDDQ	0x3d	/* VDDQ VR Output Voltage, 4 */
#define MR_SMC_VOLT_VDDG	0x3e	/* VDDG VR Output Voltage, 4 */

#define MR_SMC_TEMP_CPU		0x40	/* CPU DIE Temperature, 4 */
#define MR_SMC_TEMP_EXHAUST	0x41	/* Card Exhaust Temperature, 4 */
#define MR_SMC_TEMP_INLET	0x42	/* Card Inlet Temperature, 4 */
#define MR_SMC_TEMP_VCCP	0x43	/* VCCP VR Temperature, 4 */
#define MR_SMC_TEMP_VDDG	0x44	/* VDDG VR Temperature, 4 */
#define MR_SMC_TEMP_VDDQ	0x45	/* VDDQ VR Temperature, 4 */
#define MR_SMC_TEMP_GDDR	0x46	/* GDDR Temperature, 4 */
#define MR_SMC_TEMP_EAST	0x47	/* East Temperature, 4 */
#define MR_SMC_TEMP_WEST	0x48	/* West Temperature, 4 */
#define MR_SMC_FAN_TACH		0x49	/* Fan RPM, 4 */
#define MR_SMC_FAN_PWM		0x4a	/* Fan PWM Percent, 4 */
#define MR_SMC_FAN_PWM_ADD	0x4b	/* Fan PWM Adder, 4 */
#define MR_SMC_TCRITICAL	0x4c	/* KNC Tcritical temperature, 4 */
#define MR_SMC_TCONTROL		0x4d	/* KNC Tcontrol temperature, 4 */
#define MR_SMC_TRM_TTL_DUR	0x4e	/* Thermal Throttle Duration, 4 */
#define MR_SMC_TRM_TTL		0x4f	/* Thermal Throttling, 4 */
#define MR_SMC_TRM_PUSH		0x50	/* Target for die temp push, 4 */

#define MR_SMC_PWR_VCCP		0x58	/* VCCP VR Output Power, 4 */
#define MR_SMC_PWR_VDDQ		0x59	/* VDDQ VR Output Power, 4 */
#define MR_SMC_PWR_VDDG		0x5a	/* VDDG VR Output Power, 4 */

#define MR_SMC_LED_CODE		0x60	/* LED blink code, 4 */


/*
 * Simple I/O access routines for most SMC registers.
 * All but UUID & SERIAL are 4 bytes in size.
 */
#define SMC_TRACK	0

#if SMC_TRACK
#define RL	printk("%s: %2x -> %08x, rtn %d\n",    __FUNCTION__, reg, *val, rl)
#define WL	printk("%s: %2x <- %08x, rtn %d\n",    __FUNCTION__, reg, *val, rl)
#else
#define RL	/* As nothing */
#define WL	/* As nothing */
#endif

#ifdef MIC_IS_EMULATION
/*
 * Emulation does not handle I2C busses.
 * Therefore all code that deals with I2C needs to be
 * replaced with harmless substitutes in emulation.
 * The following stubs are for emulation only.
 */
int
gmbus_i2c_read(uint8_t d, uint8_t a, uint8_t r, uint8_t * v, uint16_t l)
{
  if (v && l)
    memset(v, 0, l);
  return l;
}

int
gmbus_i2c_write(uint8_t d, uint8_t a, uint8_t r, uint8_t * v, uint16_t l)
{
  return l;
}
#endif /* EMULATION */

static char *
gm_err(int err)
{
  char * str = "unknown";

  switch(err) {
    case -1:  str = "timeout"; break;
    case -2:  str = "ack timeout"; break;
    case -3:  str = "interrupted"; break;
    case -4:  str = "invalid command"; break;
  }

  return str;
}


int
mr_smc_rd(uint8_t reg, uint32_t * val)
{
  int		rl;

  mr_smc_deglitch();
  rl = gmbus_i2c_read(2, MR_SMC_ADDR, reg, (uint8_t *) val, sizeof(*val));
  RL;
  if (rl == sizeof(uint32_t))
    return 0;

  /*
   * Something failed, do a dummy read to get I2C bus in a known good state.
   *TBD: Do retries, and if so how many?
   */
  printk("smc_rd: error %d (%s), reg %02x\n", rl, gm_err(rl), reg);
  mr_smc_deglitch();
  gmbus_i2c_read(2, MR_SMC_ADDR, MR_SMC_FW_VERSION, (uint8_t *) &rl, sizeof(rl));
  *val = 0;
  return 1;
}

int
mr_smc_wr(uint8_t reg, uint32_t * val)
{
  int		rl;

  WL;
  mr_smc_deglitch();
  rl = gmbus_i2c_write(2, MR_SMC_ADDR, reg, (uint8_t *) val, sizeof(*val));
  if (rl == sizeof(uint32_t))
    return 0;

  /*
   * Something failed, do a dummy read to get I2C bus in a known good state.
   *TBD: Do retries, and if so how many?
   */
  printk("smc_wr: error %d (%s), reg %02x\n", rl, gm_err(rl), reg);
  mr_smc_deglitch();
  gmbus_i2c_read(2, MR_SMC_ADDR, MR_SMC_FW_VERSION, (uint8_t *) &rl, sizeof(rl));
  return 0;
}
#undef RL
#undef WL


/*
 * Bypass for SMC access.
 * Kind of a backdoor really as it allows for raw access to the SMC which
 * may be device dependent and vary significantly between SMC firmware
 * revisions. This is intended for host side tools that (hopefully) know
 * what they are receiving through this interface. There is a 'set' command
 * too, which we screen heavily since the SMC controls board cooling and
 * therefore is critical for the cards safe operation envolope.
 */

int
mr_get_smc(void * p)
{
  int		rtn;
  uint32_t	parm;
  struct mr_rsp_smc * r;

  parm = * (uint32_t *) p;
  if (GET_BITS(31, 8, parm))
    return -MR_ERR_RANGE;
  r = (struct mr_rsp_smc *) p;

  r->reg = GET_BITS(7, 0, parm);

  /*
   * These cannot be read by anybody
   */
  if (r->reg > MR_SMC_LED_CODE ||
      r->reg == MR_SMC_ZOMBIE)
    return -MR_ERR_PERM;

  /*
   * These can only be read by root
   */
  if (! micras_priv)
    switch(r->reg) {
      case MR_SMC_SEL_ENTRY_SEL:
      case MR_SMC_SEL_DATA:
      case MR_SMC_SDR_ENTRY_SEL:
      case MR_SMC_SDR_DATA:
        return -MR_ERR_PERM;
    }
     
  /*
   * Determine how wide the SMC register is
   */
  switch(r->reg) {
    case MR_SMC_UUID:
      r->width = 16;
      break;
    case MR_SMC_SERIAL:
      r->width = 12;
      break;
    default:
      r->width = 4;
  }

  mr_smc_deglitch();
  rtn = gmbus_i2c_read(2, MR_SMC_ADDR, r->reg, (uint8_t *) &r->rtn, r->width);
#if SMC_TRACK
  printk("%s: %2x -> %08x, rtn %d\n",    __FUNCTION__, r->reg, r->rtn.val, rtn);
#endif
  if (rtn != r->width) {
    /*
     * Failed once, try one more time
     *TBD: insert a known good read before the actual retry?
     */
    mr_smc_deglitch();
    rtn = gmbus_i2c_read(2, MR_SMC_ADDR, r->reg, (uint8_t *) &r->rtn, r->width);
#if SMC_TRACK
    printk("%s: %2x -> %08x, rtn %d\n",    __FUNCTION__, r->reg, r->rtn.val, rtn);
#endif

    if (r->reg == MR_SMC_SERIAL) {
      memcpy((uint8_t *) &r->rtn, hwinf.serial, r->width);
      rtn = r->width;
    }
  }

  if (rtn != r->width)
    return -MR_ERR_SMC;
 
  return sizeof(*r);
}


int
mr_set_smc(void * p)
{
  uint8_t	reg;
  uint16_t	width;
  int		rtn;
  uint32_t	val, parm;

  parm = * (uint32_t *) p;
  reg = GET_BITS(31, 24, parm);

  /*
   * Screen for registers we allow setting.
   * POST register is accessible to everyone,
   * only root can 'SET' anything beyond that.
   */
  if (micras_priv) {
    switch (reg) {
      case MR_SMC_CPU_POST:
      case MR_SMC_SEL_ENTRY_SEL:
      case MR_SMC_SDR_ENTRY_SEL:
      case MR_SMC_SMB_RESTRT:
      case MR_SMC_FORCE_TTL:
      case MR_SMC_PWR_LIM_0:
      case MR_SMC_TIME_WIN_0:
      case MR_SMC_PWR_LIM_1:
      case MR_SMC_TIME_WIN_1:
      case MR_SMC_INCL_3V3:
      case MR_SMC_PWR_LIM_PERS:
      case MR_SMC_CLAMP_MODE:
      case MR_SMC_FAN_PWM_ADD:
      case MR_SMC_LED_CODE:
	break;
      default:
	return -MR_ERR_PERM;
    }
  }
  else {
    switch (reg) {
      case MR_SMC_CPU_POST:
	break;
      default:
	return -MR_ERR_PERM;
    }
  }

  /*
   * Screen against known SMC register widths.
   * We insist that unused upper bits are zeros
   */
  switch (reg) {
    case MR_SMC_SEL_ENTRY_SEL:
    case MR_SMC_SDR_ENTRY_SEL:
    case MR_SMC_FAN_PWM_ADD:
      val = GET_BITS(7, 0, parm);	/* 8-bit registers */
      break;
    case MR_SMC_PWR_LIM_0:
    case MR_SMC_TIME_WIN_0:
    case MR_SMC_PWR_LIM_1:
    case MR_SMC_TIME_WIN_1:
      val = GET_BITS(15, 0, parm);	/* 16 bit registers */
      break;
    case MR_SMC_CPU_POST:
      val = GET_BITS(23, 0, parm);	/* 24 bit registers */
      break;
    default:
      val = GET_BIT(0, parm);		/* Booleans */
  }
  if (val != GET_BITS(23, 0, parm))
    return -MR_ERR_INVAUX;

  width = 4;
  mr_smc_deglitch();
  rtn = gmbus_i2c_write(2, MR_SMC_ADDR, reg, (uint8_t *) & val, width);
#if SMC_TRACK
  printk("%s: %2x <- %08x, rtn %d\n",    __FUNCTION__, reg, val, rtn);
#endif
  if (rtn != width)
    return -MR_ERR_SMC;
 
  return 0;
}


/*
 * IPMI interface.
 * The SMC has a connection to the host's board management software, which
 * usually resides in a dedicated Board Management Controller, of which the
 * SMC is supposed to be a registered satellite controller (aka. additional
 * management controller). As such the SMC can receive controls originating
 * from any valid IPMI session on things like power limits, but it can also
 * add events to the non-volatile IPMI System Events Log for things like
 * reporting catastrophic failures that otherwise might be lost because the
 * main processors might be disabled (section 1.7.6 in IPMI spec 2.0 E5).
 * In RAS context we'd want to let the SM know if fatal MC events occur
 * and possibly also if the uOS crashes, such that remote management can
 * be alerted via standard IPMI mechanisms.
 *
 * Input to this routine is an MceInfo record and an 'in-exception context'
 * flag. It is still TBD what exactly to tell the SMC, but it is expected
 * that all relevant info is in the MceInfo record.
 */

void
micras_mc_ipmi(struct mce_info * mc, int ctx)
{
}


#if !(USE_SVID || USE_SMC)
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
#endif



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


/*
 *TBD: substitute with official defines when availble.
 */
#define	KNC_FLASH_TAB	0x0FFF76000	/* Yes, it's below 4GB */
#define KNC_FLASH_FILT	0x400		/* Correctable MC event filter */
#define	KNC_FLASH_BASE	0x0FFFA8000	/* Yes, it's below 4GB */
#define KNC_FLASH_SIZE	0x2000		/* 8 KB according to Scott */
#define KNC_FLASH_BOOT1	0x1274 		/* Fboot1 version string */
#define KNC_FLASH_BOOTB	0x02b8 		/* Fboot1 backup version string */
#define KNC_MP_PHYS	0x9e000		/* Location of MP table */
#define KNC_MPF_SIG	0xa0afb2a0	/* String "_PM_" inverted */
#define KNC_MPC_SIG	0x504d4350	/* String "PCMP" */

static void
get_cpu_table(void)
{
  struct mpf_intel * mpf;
  struct mpc_table * mpc;
  struct mpc_cpu   * mpp;
  uint8_t	     * ptr, * ep;

  mpf = phys_to_virt((phys_addr_t) KNC_MP_PHYS);
  if (mpf) {
    if (*((uint32_t *) mpf->signature) != KNC_MPF_SIG) {
      printk("MP FP signature not found, %02x %02x %02x %02x\n",
	      mpf->signature[0], mpf->signature[1],
	      mpf->signature[2], mpf->signature[3]);
      return;
    }
    mpc = phys_to_virt((phys_addr_t) mpf->physptr);
    if (mpc) {
      if (*((uint32_t *) mpc->signature) != KNC_MPC_SIG) {
	printk("MP header signature not found, %02x %02x %02x %02x\n",
		mpc->signature[0], mpc->signature[1],
		mpc->signature[2], mpc->signature[3]);
	return;
      }
      ptr = (uint8_t *)(mpc + 1);
      ep = ptr + mpc->length;
      while(ptr < ep) {
	switch(*ptr) {
	  case 0x00:	/* CPU */
	    mpp = (struct mpc_cpu *) ptr;
	    if (GET_BIT(0, mpp->cpuflag) && mpp->apicid < nr_cpu_ids)
	      xlat_cpu[mpp->apicid] = GET_BITS(7, 0, mpp->reserved[1]);
	    ptr += 20;
	    break;
	  case 0x01:	/* BUS */
	    ptr += 8;
	    break;
	  case 0x02:	/* I/O-APIC */
	    ptr += 8;
	    break;
	  case 0x03:	/* INT source */
	    ptr += 8;
	    break;
	  case 0x04:	/* LINT source */
	    ptr += 8;
	    break;
	  default:	/* Table out of spec */
	    ptr = ep;
	}
      }
    }
#if 0
    {
      uint32_t  eax, ebx, ecx, edx;
      uint32_t	hwt, i;

      cpuid(1, &eax, &ebx, &ecx, &edx);
      hwt = GET_BITS(23, 16, ebx);
      if (hwt > nr_cpu_ids)
        hwt = nr_cpu_ids;
      printk("RAS.card: CPU thread table:\n");
      for(i=0; i < hwt; i++)
        printk("  cpu %d -> thr %d\n", i, xlat_cpu[i]); 
    }
#endif
  }
}


static void __init
mr_mk_cf_lst(void)
{
  int		i, n;
  uint16_t	f;

  /*
   * If PM module interface is in place, then the
   * core voltage list may already be populated.
   */
  if (freq.supt[0] && freq.slen)
    return;

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
  for(i = ARRAY_SIZE(gddr_tab1) -1; i >= 0; i--) {
    for(f = gddr_tab1[i].min_clk;
	f <= gddr_tab1[i].max_clk;
	f += gddr_tab1[i].step_size) {
      gfreq.supt[n] = 1000 * f;
      gfreq.slen = ++n;
      if (n == MR_PTAB_LEN)
	return;
    }
  }
  for(i = ARRAY_SIZE(gddr_tab2) -1; i >= 0; i--) {
    for(f = gddr_tab2[i].min_clk;
	f <= gddr_tab2[i].max_clk;
	f += gddr_tab2[i].step_size) {
      gfreq.supt[n] = 1000 * f;
      gfreq.slen = ++n;
      if (n == MR_PTAB_LEN)
	return;
    }
  }
}

/*
 * We can only list 64 values in this list, but on
 * a VRM12 device there is 256 values to chose from.
 * For now we'll list values from 0.7 to 1.3 volt
 * in 10 mV increments (61 values).
 */

#define VRM_MIN	 600000
#define VRM_MAX 1300000
#define VRM_RES   10000

static void __init
mr_mk_cv_lst(void)
{
  int		n;
  uint32_t      cv;

  /*
   * If PM module interface is in place, then the
   * core voltage list may already be populated.
   */
  if (volt.supt[0] && volt.slen)
    return;

  n = 0;
  for(cv = VRM_MIN; cv <= VRM_MAX; cv += VRM_RES) {
    volt.supt[n] = cv;
    volt.slen = ++n;
    if (n >= MR_PTAB_LEN)
      return;
  }
}


void __init
mr_mt_card_init(void)
{
  uint32_t	scr7, scr9, cf;
  uint32_t	smc, ci;
  int		rtn;
#ifndef MIC_IS_EMULATION
  uint8_t     * parm;
#endif
#if ! USE_SMC
  uint32_t	gv;
#endif
#if USE_SVID
  int		svid;
  uint8_t	vr;
#else
#if ! USE_SMC
  uint32_t	cv;
#endif
#endif
#if USE_PM
  int	     (* fnc)(void);
#endif

  /*
   * Make CPU->phys ID translation table
   */
  get_cpu_table();

  /*
   * Build numbers for fboot0 and fboot 1 repectively
   */
  scr7 = mr_sbox_rl(0, SBOX_SCRATCH7);

  /*
   * VERS:
   * Map flash and look for version strings.
   */
#ifdef MIC_IS_EMULATION
  vers.fboot1[0] = scnprintf(vers.fboot1 + 1, MR_VERS_LEN -2,
    		"No emulation flash version string (build %d)",
			GET_BITS(31, 16, scr7));
#else
  parm = ioremap(KNC_FLASH_BASE, KNC_FLASH_SIZE);
  if (!parm) {
    printk("mr_mt_card_init: ioremap failure: parm %x\n", KNC_FLASH_BASE);
    goto fail_iomap;
  }

  /*
   * The fboot0 version (hardwired in the chip) is placed in flash
   * by bootstrap at a fixed location, and is less than 16 byte long.
   */
  if (strnlen(parm + KNC_FLASH_BOOT1, 16) < 16)
    vers.fboot1[0] = scnprintf(vers.fboot1 + 1, MR_VERS_LEN -2,
    		"fboot1 version: %s (build %d)",
		parm + KNC_FLASH_BOOT1, GET_BITS(31, 16, scr7));
  else
    vers.fboot1[0] =scnprintf(vers.fboot1 + 1, MR_VERS_LEN -2,
    		"No valid version string found");
  iounmap(parm);

  /*
   * While at it, check if there is a MC filter list in flash
   */
  parm = ioremap(KNC_FLASH_TAB, KNC_FLASH_SIZE);
  if (!parm) {
    printk("mr_mt_card_init: ioremap failure: parm %x\n", KNC_FLASH_TAB);
    goto fail_iomap;
  }
  mcc_flt_parm(parm + KNC_FLASH_FILT);
  iounmap(parm);

fail_iomap:
#endif

  /*
   * Retrieve ID details from the SMC
   *   UUID, 16 byte
   *   serial, 12 byte
   *   FW version,
   *	15:0	Build number
   *    23:16	Minor version
   *	31:24	Major version
   * Note: Ancient systems, like Berta, runs on cards with an older
   *       version on the SMC firmware that does not support serial.
   */
  mr_smc_deglitch();
  rtn = gmbus_i2c_read(2, MR_SMC_ADDR, MR_SMC_UUID, hwinf.guid, 16);
#if SMC_TRACK
  printk("%s: %2x -> %08x, rtn %d\n",  __FUNCTION__, MR_SMC_UUID, *(uint32_t *) hwinf.guid, rtn);
#endif
  if (rtn != 16)
    memset(hwinf.guid, '\0', 16);
  mr_smc_deglitch();
  rtn = gmbus_i2c_read(2, MR_SMC_ADDR, MR_SMC_SERIAL, hwinf.serial, 12);
#if SMC_TRACK
  printk("%s: %2x -> %08x, rtn %d\n",  __FUNCTION__, MR_SMC_SERIAL, *(uint32_t *) hwinf.serial, rtn);
#endif
  if (rtn != 12)
    memcpy(hwinf.serial, "Update_SMC!!", sizeof(hwinf.serial));
  if (! mr_smc_rd(MR_SMC_FW_VERSION, &smc))
    vers.fsc[0] = scnprintf(vers.fsc + 1, MR_VERS_LEN -2,
  		"SMC firmware rev. %d.%d (build %d)",
		  GET_BITS(31, 24, smc),
		  GET_BITS(23, 16, smc),
		  GET_BITS(15,  0, smc));

  /*
   * HWINF:
   * Get processor details from SBOX componentID.
   *   19:16	Model ID => aka revision
   *   15:12	Stepping ID => stepping
   *   11:8	Substepping ID => substep
   *
   * Get Card Revision details from the SMC.
   *   17:16	board (0=MPI, CRB, SFF, Product)
   *   10:8	fab version (0='A' .. 7='H')
   *    2:0	PBA SKU # (need name table here?)
   */
  ci = mr_sbox_rl(0, SBOX_COMPONENT_ID);
  hwinf.rev     = GET_BITS(19, 16, ci);
  hwinf.step    = GET_BITS(15, 12, ci);
  hwinf.substep = GET_BITS(11,  8, ci);
  if (! mr_smc_rd(MR_SMC_HW_REVISION, &smc)) {
    hwinf.board = GET_BITS(17, 16, smc);
    hwinf.fab   = GET_BITS(10,  8, smc);
    hwinf.sku   = GET_BITS( 2,  0, smc);
  }
  
  /*
   * VOLT:
   * By definition, reference voltage is 1st value seen.
   * Order of preference is SVID, then SMC and lastly SBOX.
   * SMC register bits 15:0 is voltage in mV.
   * SBOX_COREVOLT should be in SVID voltage format.
   */
#if USE_SVID
  svid = SvidCmd(SVID_VCCP, VR12Cmd_GetReg, VR12Reg_VID_Set);
  if (svid >= 0)
    volt.set = svid2volt(svid);
#else
#if USE_SMC
  if (!mr_smc_rd(MR_SMC_VOLT_VCCP, &smc) && GET_BITS(31, 30, smc) != 0x3)
    volt.set = GET_BITS(15, 0, smc) * 1000;
#else
  cv = mr_sbox_rl(0, SBOX_COREVOLT);
  volt.set = svid2volt(GET_BITS(7, 0, cv));
#endif
#endif
  mr_mk_cv_lst();

  /*
   * FREQ
   * By definition, reference frequency is 1st value seen.
   */
  cf = mr_sbox_rl(0, SBOX_COREFREQ);
  freq.def = mr_mt_cf_r2f(GET_BITS(11, 0, cf));
  mr_mk_cf_lst();

  /*
   * GDDR:
   * See layout of scratch #9 in 'common'.
   * 26:16	Clock ratio encoding
   *    27	ClamShell
   */
  scr9 = mr_sbox_rl(0, SBOX_SCRATCH9);
  gddr.speed = 2 * mr_mt_gf_r2f(GET_BITS(26, 16, scr9));

  /*
   * GVOLT:
   * Report all values the hardware can set, kind
   * of silly as these cannot be changed from uOS.
   * Order of preference is SVID, then SMC and lastly SBOX.
   * SMC register bits 15:0 is voltage in mV.
   *
   *TBD: Seriously suspect SBOX register to be wrong.
   */
#if USE_SVID
  svid = SvidCmd(SVID_VDDQ, VR12Cmd_GetReg, VR12Reg_VID_Set);
  if (svid >= 0)
    gvolt.set = svid2volt(svid);
#else
#if USE_SMC
  if (!mr_smc_rd(MR_SMC_VOLT_VDDQ, &smc) && GET_BITS(31, 30, smc) != 0x3)
    gvolt.set = GET_BITS(15, 0, smc) * 1000;
#else
  gv = mr_sbox_rl(0, SBOX_MEMVOLT);
  gvolt.set = svid2volt(GET_BITS(7, 0, gv));
#endif
#endif
 
  /*
   * GFREQ:
   * Report all values the hardware can set, kind
   * of silly as these cannot be changed from uOS.
   */
  gfreq.def = mr_mt_gf_r2f(GET_BITS(26, 16, scr9));
  mr_mk_gf_lst();

  /*
   * PWR:
   * If we are going to use SVID registers we'd need
   * to know the VRs capabilities and ICC_MAX setting.
   */
#if USE_SVID
  vr = SvidCmd(SVID_VCCP, VR12Cmd_GetReg, VR12Reg_Capability);
  if (vr >= 0)
    vccp_cap = vr;
  vr = SvidCmd(SVID_VDDQ, VR12Cmd_GetReg, VR12Reg_Capability);
  if (vr >= 0)
    vddq_cap = vr;
  vr = SvidCmd(SVID_VDDG, VR12Cmd_GetReg, VR12Reg_Capability);
  if (vr >= 0)
    vddg_cap = vr;
  vr = SvidCmd(SVID_VCCP, VR12Cmd_GetReg, VR12Reg_Icc_Max);
  if (vr >= 0)
    vccp_imax = vr;
  vr = SvidCmd(SVID_VDDQ, VR12Cmd_GetReg, VR12Reg_Icc_Max);
  if (vr >= 0)
    vddq_imax = vr;
  vr = SvidCmd(SVID_VDDG, VR12Cmd_GetReg, VR12Reg_Icc_Max);
  if (vr >= 0)
    vddg_imax = vr;
#endif

  /*
   * ECC:
   *
   *TBD: Where to find ECC setting?
   *     There are several GBOX registers that has something
   *	 named ECC in them. Scott to tell once PO is done.
   */
  ecc.enable = GET_BIT(29, scr9);

  /*
   * TRBO
   * The PM module have the inital turbo mode setting.
   * Get it now, so we don't need to call PM to report it.
   */
#if USE_PM
  fnc = pm_cb.micpm_get_turbo;
  if (fnc)
    trbo.set = fnc();
#endif

  /*
   *TBD: Save registers this module may change
   */
}

void __exit
mr_mt_card_exit(void)
{
  /*
   *TBD: Restore registers this module may change
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
#if USE_PM
  void		     (* fnc)(void);
#endif

  /*
   * Preference is VR out.
   * Not sure if board sensors work in KnC
   */
#if USE_SVID
  {
    int			vout;	

    vout = SvidCmd(SVID_VCCP, VR12Cmd_GetReg, VR12Reg_VID_Set);
    if (vout < 0)
      return vout;
    volt.set = svid2volt(vout);

    vout = SvidCmd(SVID_VCCP, VR12Cmd_GetReg, VR12Reg_Vout);
    if (vout < 0)
      return vout;
    volt.cur = vout2volt(vout);
  }
#else
#if USE_SMC
  {
    uint32_t	smc;

    volt.cur = 0;
    volt.c_val = 3;
    if (! mr_smc_rd(MR_SMC_VOLT_VCCP, &smc)) {
      volt.c_val = GET_BITS(31, 30, smc);
      if (volt.c_val != 0x3)
        volt.cur = GET_BITS(15, 0, smc) * 1000;
    }

    /*
     *TBD: override 'set' value ?
     */
  }
#else
  {
    uint32_t		fsc, cv;

    cv = mr_sbox_rl(0, SBOX_COREVOLT);
    volt.set = svid2volt(GET_BITS(7, 0, cv));

    fsc = mr_sbox_rl(0, SBOX_BOARD_VOLTAGE_SENSE);
    volt.cur = bvs2volt(GET_BITS(15, 0, fsc));
  }
#endif
#endif

#if USE_PM
  /*
   * Ask PM for table refresh
   */
  fnc = pm_cb.micpm_vf_refresh;
  if (fnc)
    fnc();
#endif

  r = (struct mr_rsp_volt *) p;
  *r = volt;
  return sizeof(*r);
}


int
mr_get_freq(void * p)
{
  struct mr_rsp_freq  * r;
  uint32_t		cf, cr;
#if USE_PM
  void		     (* fnc)(void);
#endif

  /*
   * Current Ratio:
   *  11:0	Current core ratio
   *     15	Enable 600 MHz
   *  27:16	Goal ratio
   *     31	OC disable
   * Goal ratio is a product of base ratio and fuse overrides
   * Current ration is a product of goal, fuse limits and themal throttle
   *
   * Core Frequency:
   *  11:0	Base ratio
   *	 15	Fuse override
   *     31	Select ratio
   * Base ratio accepted only if (bit 15 | bit 31 | OC disble) == 010
   *
   *TBD: How to detect clock bypasses?
   *     ICC bypass cuts the core and reference base in half.
   */
  cr = mr_sbox_rl(0, SBOX_CURRENTRATIO);
  cf = mr_sbox_rl(0, SBOX_COREFREQ);
  freq.cur = mr_mt_cf_r2f(GET_BITS(11, 0, cr));
  freq.def = mr_mt_cf_r2f(GET_BITS(11, 0, cf));
  if (GET_BITS(11,  0, cf) != GET_BITS(11,  0, cr))
    printk("RAS.get_freq: core not running at expected frequency\n");

#if USE_PM
  /*
   * Ask PM for table refresh
   */
  fnc = pm_cb.micpm_vf_refresh;
  if (fnc)
    fnc();
#endif

  r = (struct mr_rsp_freq *) p;
  *r = freq;
  return sizeof(*r);
}


#if USE_SVID
int
mr_get_svid(uint8_t vr, uint8_t cap, uint8_t imax, struct mr_rsp_vrr * vrr)
{
  int			v, a, p;

  p = SvidCmd(vr, VR12Cmd_GetReg, VR12Reg_Pout);
  a = SvidCmd(vr, VR12Cmd_GetReg, VR12Reg_Iout);
  v = SvidCmd(vr, VR12Cmd_GetReg, VR12Reg_Vout);

  if (p < 0 || a < 0 || v < 0)
    return -MR_ERR_SMC;

  vrr->pwr  = pout2watt(p);
  vrr->cur  = iout2amp(a, cap, imax);
  vrr->volt = vout2volt(v);

  return 0;
}
#endif

#define KNC_DFF_BOARD   2       /*  DFF/SFF board */

int
mr_get_power(void * p)
{
  struct mr_rsp_power * r;
#if USE_SMC
  static struct mr_rsp_vrr vnil = { 0, 0, 0, 3, 3, 3 };
  static struct mr_rsp_pws pnil = { 0, 3 };
  uint32_t		vccp, vddg, vddq;
  uint32_t		prd0, prd1, pcie, p2x3, p2x4;
#endif

#if USE_SVID
  /*
   * Get VR status over SVID.
   */
  if (mr_get_svid(SVID_VCCP, vccp_cap, vccp_imax, &power.vccp) < 0 ||
      mr_get_svid(SVID_VDDQ, vddq_cap, vddq_imax, &power.vddq) < 0 ||
      mr_get_svid(SVID_VDDG, vddg_cap, vddg_imax, &power.vddq) < 0)
    return -MR_ERR_SMC;
#else
#if USE_SMC
  /*
   * Get VR status from SMC.
   * Only voltages are available currently.
   * Still need to screen for good data.
   * Top 2 bits decode as
   *  00	Data OK
   *  01	Upper threshold reached
   *  10	Lower threshold reached
   *  11	Data unavailable
   * Assume data is valid even if a threshold reached
   */
  power.vccp = power.vddg = power.vddq = vnil;
  if (! mr_smc_rd(MR_SMC_VOLT_VCCP, &vccp)) {
    power.vccp.v_val = GET_BITS(31, 30, vccp);
    if (power.vccp.v_val != 0x3)
      power.vccp.volt = 1000 * GET_BITS(15, 0, vccp);
  }
  if (! mr_smc_rd(MR_SMC_VOLT_VDDG, &vddg)) {
    power.vddg.v_val = GET_BITS(31, 30, vddg);
    if (power.vddg.v_val != 0x3)
      power.vddg.volt = 1000 * GET_BITS(15, 0, vddg);
  }
  if (! mr_smc_rd(MR_SMC_VOLT_VDDQ, &vddq)) {
    power.vddq.v_val = GET_BITS(31, 30, vddq);
    if (power.vddq.v_val != 0x3)
      power.vddq.volt = 1000 * GET_BITS(15, 0, vddq);
  }
  if (! mr_smc_rd(MR_SMC_PWR_VCCP, &vccp)) {
    power.vccp.p_val = GET_BITS(31, 30, vccp);
    if (power.vccp.p_val != 0x3)
      power.vccp.pwr = 1000000 * GET_BITS(15, 0, vccp);
  }
  if (! mr_smc_rd(MR_SMC_PWR_VDDG, &vddg)) {
    power.vddg.p_val = GET_BITS(31, 30, vddg);
    if (power.vddg.p_val != 0x3)
      power.vddg.pwr = 1000000 * GET_BITS(15, 0, vddg);
  }
  if (! mr_smc_rd(MR_SMC_PWR_VDDQ, &vddq)) {
    power.vddq.p_val = GET_BITS(31, 30, vddq);
    if (power.vddq.p_val != 0x3)
      power.vddq.pwr = 1000000 * GET_BITS(15, 0, vddq);
  }
#endif
#endif
     
#if USE_SMC
  /*
   * Get reads on VRs and power sensors from SMC.
   * This is a mess:
   *  - total power may or may not include 3.3 V rail.
   *    If it is then it's not measured, just "guessed".
   *  - there are two averaging windows for total power,
   *	though it is not clear who controls these windows.
   *    For now we assume window 0 is shorter than window 1
   *    and thus power 0 is 'current' reading and power 1
   *    is the '20 sec' reading.
   *    TBD: Who controls the time windows and is is true
   *         that Window 0 is shorter than Window 1?
   *  - No specifics on how power sensors are averaged,
   *    i.e. is Window 0/1 used or is is a third window.
   *    Need to know, otherwise Ptot may not be sum(sources).
   *  - There still is no 'max' value from SMC
   *
   * Still need to screen for good data.
   * Top 2 bits decode as
   *  00	Data OK
   *  01	Upper threshold reached
   *  10	Lower threshold reached
   *  11	Data unavailable
   * Assume data is valid even if a threshold reached
   */
  power.tot0 = power.tot1 =
  power.inst = power.imax =
  power.pcie = power.c2x3 = power.c2x4 = pnil;

  if (! mr_smc_rd(MR_SMC_AVG_PWR_0, &prd0)) {
    power.tot0.p_val = GET_BITS(31, 30, prd0);
    if (power.tot0.p_val != 0x3)
      power.tot0.prr = 1000000 * GET_BITS(29, 0, prd0);
  }
  if (! mr_smc_rd(MR_SMC_AVG_PWR_1, &prd1)) {
    power.tot1.p_val = GET_BITS(31, 30, prd1);
    if (power.tot1.p_val != 0x3)
      power.tot1.prr = 1000000 * GET_BITS(29, 0, prd1);
  }
  power.inst = power.imax = pnil;
  if (! mr_smc_rd(MR_SMC_PWR_INST, &prd0)) {
    power.inst.p_val = GET_BITS(31, 30, prd0);
    if (power.inst.p_val != 0x3)
      power.inst.prr = 1000000 * GET_BITS(29, 0, prd0);
  }
  if (! mr_smc_rd(MR_SMC_PWR_IMAX, &prd1)) {
    power.imax.p_val = GET_BITS(31, 30, prd1);
    if (power.imax.p_val != 0x3)
      power.imax.prr = 1000000 * GET_BITS(29, 0, prd1);
  }
  if (! mr_smc_rd(MR_SMC_PWR_PCIE, &pcie)) {
    power.pcie.p_val = GET_BITS(31, 30, pcie);
    if (power.pcie.p_val != 0x3)
      power.pcie.prr  = 1000000 * GET_BITS(15, 0, pcie);
  }
  if (hwinf.board != KNC_DFF_BOARD) {
    if (! mr_smc_rd(MR_SMC_PWR_2X3,  &p2x3)) {
      power.c2x3.p_val = GET_BITS(31, 30, p2x3);
      if (power.c2x3.p_val != 0x3)
        power.c2x3.prr  = 1000000 * GET_BITS(15, 0, p2x3);
    }
    if (! mr_smc_rd(MR_SMC_PWR_2X4,  &p2x4)) {
      power.c2x4.p_val = GET_BITS(31, 30, p2x4);
      if (power.c2x4.p_val != 0x3)
        power.c2x4.prr  = 1000000 * GET_BITS(15, 0, p2x4);
    }
  }
#endif

  r = (struct mr_rsp_power *) p;
  *r = power;
  return sizeof(*r);
}


int
mr_get_plim(void * p)
{
  uint32_t	pl0, pl1, grd;
  struct mr_rsp_plim  * r;

  /*
   * Get values from PM
   */
  if (! mr_smc_rd(MR_SMC_PWR_LIM_0, &pl0))
    plim.hmrk = GET_BITS(15, 0, pl0);

  if (! mr_smc_rd(MR_SMC_PWR_LIM_1, &pl1))
    plim.lmrk = GET_BITS(15, 0, pl1);

  if (! mr_smc_rd(MR_SMC_PWR_LIM0_GRD, &grd))
    plim.phys = plim.hmrk + GET_BITS(15, 0, grd);

  r = (struct mr_rsp_plim *) p;
  *r = plim;
  return sizeof(*r);
}


int
mr_get_gfreq(void * p)
{
  struct mr_rsp_gfreq * r;
  uint32_t              gbr;

  /*
   * SBOX register MEMFREQ bits 7:0 now holds 10 x rate in GTps.
   */
  gbr = mr_sbox_rl(0, SBOX_MEMORYFREQ);
  gfreq.cur = GET_BITS(7, 0, gbr) * 100000 / 2;

  r = (struct mr_rsp_gfreq *) p;
  *r = gfreq;
  return sizeof(*r);
}


int
mr_get_gvolt(void * p)
{
  struct mr_rsp_gvolt * r;

  /*
   * Preference is VR out.
   * Not sure if board sensors work in KnC
   */
#if USE_SVID
  {
    int			vout;	

    vout = SvidCmd(SVID_VDDQ, VR12Cmd_GetReg, VR12Reg_VID_Set);
    if (vout < 0)
      return vout;
    gvolt.set = svid2volt(vout);

    vout = SvidCmd(SVID_VDDQ, VR12Cmd_GetReg, VR12Reg_Vout);
    if (vout < 0)
      return vout;
    gvolt.cur = vout2volt(vout);
  }
#else
#if USE_SMC
  {
    uint32_t	smc;

    gvolt.cur = 0;
    gvolt.c_val = 3;
    if (! mr_smc_rd(MR_SMC_VOLT_VDDQ, &smc)) {
      gvolt.c_val = GET_BITS(31, 30, smc);
      if (gvolt.c_val != 0x3)
        gvolt.cur = GET_BITS(15, 0, smc) * 1000;
    }
    if (!gvolt.set)
      gvolt.set = gvolt.cur;
  }
#else
  {
    uint32_t		bvs;

    bvs = mr_sbox_rl(0, SBOX_BOARD_VOLTAGE_SENSE);
    gvolt.cur = bvs2volt(GET_BITS(31, 16, bvs));
  }
#endif
#endif

  r = (struct mr_rsp_gvolt *) p;
  *r = gvolt;
  return sizeof(*r);
}


/*
 * Card has 3 dedicated temp sensors (read from SMC):
 *   0	Air Inlet (aka West)
 *   1	Air exhaust (aka East)
 *   2	GDDR memory (not sure which chip)
 *
 * VRs can measure temperature too, which may be read
 * from SMC (via I2C bus) or the VRs directly (via SVID).
 *   3	Vccp VR (IR3538) temp
 *   4	Vddq VR (IR3541, loop 1) temp
 *   5	Vddg VR (IR3541, loop 2) temp
 * Note: Vddg and Vddq are measured on the same VR,
 * likely will be the same reading (or very close).
 *
 * SBOX board temperature sensors are not connected
 * in KnC (SBOX HAS vol 1, section 1.40.1). Instead it
 * relies on SMC to 'broadcast' sensor telemetry into
 * the KnC's TMU unit via it's I2C bus.
 * Currently it doesn't, though a DCR has been filed.
 */

int
mr_get_temp(void * p)
{
  struct mr_rsp_temp  * r;
  uint32_t		die1, die2, die3;	/* Die temps */
  uint32_t		dmx1, dmx2, dmx3;	/* Max die temps */
#if USE_SVID
  int			tvccp, tvddq, tvddg;	/* VR temps */
#endif
#if USE_SMC
  static struct mr_rsp_tsns tnil = { 0, 3 };
#endif

#if USE_SVID
  /*
   * Get VR temperatures over SVID.
   * These are _all_ positive numbers.
   */
  tvccp = SvidCmd(SVID_VCCP, VR12Cmd_GetReg, VR12Reg_Temp);
  tvddq = SvidCmd(SVID_VDDQ, VR12Cmd_GetReg, VR12Reg_Temp);
  tvddg = SvidCmd(SVID_VDDG, VR12Cmd_GetReg, VR12Reg_Temp);
  if (tvccp < 0 || tvddq < 0 || tvddg < 0)
    return -MR_ERR_SMC;
  temp.vccp.cur = GET_BITS(7, 0, tvccp);
  temp.vddq.cur = GET_BITS(7, 0, tvddq);
  temp.vddg.cur = GET_BITS(7, 0, tvddg);
#endif

#if USE_SMC
  /*
   * Get temp sensor readings from SMC.
   * According to MAS 0.30 it presents
   *  - CPU die temp (just one value)
   *  - Fan exhaust temp
   *  - Fan inlet temp
   *  - Vccp VR temp
   *  - Vddg VR temp
   *  - Vddq VR temp
   *  - GDDR temp
   *
   * Still need to screen for good data.
   * Top 2 bits decode as
   *  00	Data OK
   *  01	Upper threshold reached
   *  10	Lower threshold reached
   *  11	Data unavailable
   * Assume data is valid even if a threshold reached
   */
  {
    uint32_t		fin, fout, gddr;	/* Sensor temps */
    uint32_t		vccp, vddg, vddq;	/* VR temps */
    uint32_t		die;			/* Die summary */

    temp.die = temp.fin = temp.fout =
    temp.vccp = temp.vddg = temp.vddq = tnil;
    if (! mr_smc_rd(MR_SMC_TEMP_CPU, &die)) {
      temp.die.c_val = GET_BITS(31, 30, die);
      if (temp.die.c_val != 0x3)
	temp.die.cur  = GET_BITS(15, 0, die);
    }
    if (! mr_smc_rd(MR_SMC_TEMP_EXHAUST, &fout)) {
      temp.fout.c_val = GET_BITS(31, 30, fout);
      if (temp.fout.c_val != 0x3)
        temp.fout.cur = GET_BITS(15, 0, fout);
    }
    if (! mr_smc_rd(MR_SMC_TEMP_INLET, &fin)) {
      temp.fin.c_val = GET_BITS(31, 30, fin);
      if (temp.fin.c_val != 0x3)
        temp.fin.cur  = GET_BITS(15, 0, fin);
    }
    if (! mr_smc_rd(MR_SMC_TEMP_VCCP, &vccp)) {
      temp.vccp.c_val = GET_BITS(31, 30, vccp);
      if (temp.vccp.c_val != 0x3)
        temp.vccp.cur = GET_BITS(15, 0, vccp);
    }
    if (! mr_smc_rd(MR_SMC_TEMP_VDDG, &vddg)) {
      temp.vddg.c_val = GET_BITS(31, 30, vddg);
      if (temp.vddg.c_val != 0x3)
        temp.vddg.cur = GET_BITS(15, 0, vddg);
    }
    if (! mr_smc_rd(MR_SMC_TEMP_VDDQ, &vddq)) {
      temp.vddq.c_val = GET_BITS(31, 30, vddq);
      if (temp.vddq.c_val != 0x3)
        temp.vddq.cur = GET_BITS(15, 0, vddq);
    }
    if (! mr_smc_rd(MR_SMC_TEMP_GDDR, &gddr)) {
      temp.gddr.c_val = GET_BITS(31, 30, gddr);
      if (temp.gddr.c_val != 0x3)
        temp.gddr.cur = GET_BITS(15, 0, gddr);
    }
  }
#else
  /*
   * The TMU registers relies on telemetry broadcasts from
   * the SMC in order to report current data, early SMC
   * firmware does not provide telemetry at all.
   * Mapping of 'board temps' to physical sensors isn't
   * really defined anywhere. Based on FreeBSD comments
   * they map is:
   *   0	Air Inlet
   *   1	VCCP VR
   *   2	GDDR (not sure which chip)
   *   3	GDDR VR
   *
   *TBD: verify map on actual CRB
   */
  {
    uint32_t		btr1, btr2;		/* Board temps */
    uint32_t		tsta;			/* Thermal status */
    uint32_t		fsc;			/* Fan controller status */

    fsc  = mr_sbox_rl(0, SBOX_STATUS_FAN2);
    btr1 = mr_sbox_rl(0, SBOX_BOARD_TEMP1);
    btr2 = mr_sbox_rl(0, SBOX_BOARD_TEMP2);
    tsta = mr_sbox_rl(0, SBOX_THERMAL_STATUS);
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
    if (tsta & (1 << 31))
      temp.die.cur = GET_BITS(30, 22, tsta);
  }
#endif

  /*
   * Raw SBOX data for die temperatures.
   *
   *TBD: do these depend on SMC telemetry?
   *     If so they probably won't work until DCR in place.
   */
  die1 = mr_sbox_rl(0, SBOX_CURRENT_DIE_TEMP0);
  die2 = mr_sbox_rl(0, SBOX_CURRENT_DIE_TEMP1);
  die3 = mr_sbox_rl(0, SBOX_CURRENT_DIE_TEMP2);
  dmx1 = mr_sbox_rl(0, SBOX_MAX_DIE_TEMP0);
  dmx2 = mr_sbox_rl(0, SBOX_MAX_DIE_TEMP1);
  dmx3 = mr_sbox_rl(0, SBOX_MAX_DIE_TEMP2);

  /*
   * Die temperatures.
   * Always positive numbers (or zero for unfused parts)
   */
  temp.dies[0].cur = GET_BITS( 9,  0, die1);
  temp.dies[1].cur = GET_BITS(19, 10, die1);
  temp.dies[2].cur = GET_BITS(29, 20, die1);
  temp.dies[3].cur = GET_BITS( 9,  0, die2);
  temp.dies[4].cur = GET_BITS(19, 10, die2);
  temp.dies[5].cur = GET_BITS(29, 20, die2);
  temp.dies[6].cur = GET_BITS( 9,  0, die3);
  temp.dies[7].cur = GET_BITS(19, 10, die3);
  temp.dies[8].cur = GET_BITS(29, 20, die3);

  /*
   * Die max temp (probably 0 for unfused parts)
   */
  temp.dies[0].max = GET_BITS( 9,  0, dmx1);
  temp.dies[1].max = GET_BITS(19, 10, dmx1);
  temp.dies[2].max = GET_BITS(29, 20, dmx1);
  temp.dies[3].max = GET_BITS( 9,  0, dmx2);
  temp.dies[4].max = GET_BITS(19, 10, dmx2);
  temp.dies[5].max = GET_BITS(29, 20, dmx2);
  temp.dies[6].max = GET_BITS( 9,  0, dmx3);
  temp.dies[7].max = GET_BITS(19, 10, dmx3);
  temp.dies[8].max = GET_BITS(29, 20, dmx3);

  r = (struct mr_rsp_temp *) p;
  *r = temp;
  return sizeof(*r);
}


int
mr_get_fan(void * p)
{
  struct mr_rsp_fan   * r;
  uint32_t		fs, fp;
#if USE_SMC
  uint32_t		fa;
#endif

  r = (struct mr_rsp_fan *) p;

  /*
   * Preference is SMC data.
   * Not sure if SBOX registers work sensors work in KnC
   */
#if USE_SMC
  /*
   * Read fan state from SMC.
   * No info on override available.
   */
  r->override = 0;
  r->r_val = r->p_val = 3;
  if (mr_smc_rd(MR_SMC_FAN_TACH, &fs))
    fs = PUT_BITS(31, 30, 3);
  if (mr_smc_rd(MR_SMC_FAN_PWM, &fp))
    fp = PUT_BITS(31, 30, 3);
  if (mr_smc_rd(MR_SMC_FAN_PWM_ADD, &fa))
    fa = PUT_BITS(31, 30, 3);
 
  /*
   * Still need to screen for good data.
   * Top 2 bits decode as
   *  00	Data OK
   *  01	Reserved
   *  10	Lower threshold reached (or reserved)
   *  11	Data unavailable
   * Assume data is still valid if a threshold reached
   */
  if (GET_BITS(31, 30, fs) != 0x3) {
    /*
     * The override concept from KnF (and SBOX registers)
     * seems to have been replaced with a PWM adder.
     * Propose to set override flag if adder is non-zero.
     */
    r->r_val = 0;
    r->rpm = GET_BITS(15,  0, fs);
    if (GET_BITS(31, 30, fp) != 0x3) {
      r->p_val = 0;
      r->pwm = GET_BITS(7, 0, fp);
      if (GET_BITS(31, 30, fa) != 0x3) {
	fa = GET_BITS(7, 0, fa);
	if (fa) {
	  r->override = 1;
	  r->pwm += fa;
	  if (r->pwm > 100)
	    r->pwm = 100;
	}
      }
    }
  }
#else
  /*
   * Read fan state from SBOX registers
   * Require SMC telemetry to work.
   */
  fs = mr_sbox_rl(0, SBOX_STATUS_FAN1);
  fp = mr_sbox_rl(0, SBOX_SPEED_OVERRIDE_FAN);

  r->override = GET_BIT(15, fp);
  r->rpm      = GET_BITS(15,  0, fs);
  if (r->override)
    r->pwm    = GET_BITS( 7,  0, fp);
  else
    r->pwm    = GET_BITS(23, 16, fs);
#endif

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
  struct mr_rsp_trbo  * r;

  /*
   * Get current value from PM
   */
#if USE_PM
  int		     (* fnc)(void);

  fnc = pm_cb.micpm_get_turbo;
  if (fnc) {
    uint32_t pm;

    pm = fnc();
    trbo.state = GET_BIT(1, pm);
    trbo.avail = GET_BIT(2, pm);
    if (! trbo.avail)
      trbo.set = 0;
  }
#endif

  r = (struct mr_rsp_trbo *) p;
  *r = trbo;
  return sizeof(*r);
}


int
mr_get_pmcfg(void * p)
{
  struct mr_rsp_pmcfg * r;

#if USE_PM
  int		     (* fnc)(void);

  fnc = pm_cb.micpm_get_pmcfg;
  if (fnc)
    pmcfg.mode = fnc();
#endif

  r = (struct mr_rsp_pmcfg *) p;
  *r = pmcfg;
  return sizeof(*r);
}


int
mr_get_led(void * p)
{
  struct mr_rsp_led   * r;
  uint32_t		led;

  if (mr_smc_rd(MR_SMC_LED_CODE, &led))
    return -MR_ERR_SMC;

  r = (struct mr_rsp_led *) p;
  r->led = GET_BIT(0, led);
  return sizeof(*r);
}


int
mr_get_prochot(void * p)
{
  struct mr_rsp_ptrig * r;
  uint32_t		pwr0;
  uint32_t		time0;

  if (mr_smc_rd(MR_SMC_PWR_LIM_0, &pwr0) ||
      mr_smc_rd(MR_SMC_TIME_WIN_0, &time0))
    return -MR_ERR_SMC;

  r = (struct mr_rsp_ptrig *) p;
  r->power = GET_BITS(15, 0, pwr0);
  r->time = GET_BITS(15, 0, time0);
  return sizeof(*r);
}


int
mr_get_pwralt(void * p)
{
  struct mr_rsp_ptrig * r;
  uint32_t		pwr1;
  uint32_t		time1;

  if (mr_smc_rd(MR_SMC_PWR_LIM_1, &pwr1) ||
      mr_smc_rd(MR_SMC_TIME_WIN_1, &time1))
    return -MR_ERR_SMC;

  r = (struct mr_rsp_ptrig *) p;
  r->power = GET_BITS(15, 0, pwr1);
  r->time = GET_BITS(15, 0, time1);
  return sizeof(*r);
}


int
mr_get_perst(void * p)
{
  struct mr_rsp_perst * r;
  uint32_t		perst;

  if (mr_smc_rd(MR_SMC_PWR_LIM_PERS, &perst))
    return -MR_ERR_SMC;

  r = (struct mr_rsp_perst *) p;
  r->perst = GET_BIT(0, perst);
  return sizeof(*r);
}


int
mr_get_ttl(void * p)
{
  struct mr_rsp_ttl   * r;

  r = (struct mr_rsp_ttl *) p;

#if USE_PM
  mr_pm_ttl(r);
#endif

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
#if USE_SVID
  uint32_t	err, val;
  uint8_t	svid;

  /*
   * Ensure it's a supported value
   * Which limits to use, physical or PM list?
   */
  val = *(uint32_t *) p;
  svid = volt2svid(val);
#if 1
  {
    if (!svid)
      return -MR_ERR_RANGE;
  }
#else
  {
    int		i;

    for(i = 0; i < MR_PTAB_LEN; i++)
      if (volt.supt[i] == val)
	break;
    if (i == MR_PTAB_LEN)
      return -MR_ERR_RANGE;
    }
#endif

  /*
   * Read-modify-write the core voltage VID register
   */
  err = SvidCmd(SVID_VCCP, VR12Cmd_SetVID_Slow, svid);
  printk("SetVolt: %d -> %08x (err %08x)\n", val, svid, err);
 
  return err ? -MR_ERR_SMC : 0;
#else
  return -MR_ERR_INVOP;
#endif
}


int
mr_set_freq(void * p)
{
  uint32_t	cf, msk, new, val;
  uint16_t	rat;
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
   * Core Frequency:
   *  11:0	Base ratio
   *	 15	Fuse override
   *     31	Select ratio
   * Base ratio accepted only if (bit 15 | bit 31 | OC disble) == 010
   * Pre-scale frequency to counter for any ICC trickery.
   * Not nice, makes exact table matches difficult!!
   */
  val = (val * icc_fwd()) / ICC_NOM;
  rat = freq2ratio(val/1000, cpu_tab, ARRAY_SIZE(cpu_tab), 200);
  cf = mr_sbox_rl(0, SBOX_COREFREQ);
  msk = ~(PUT_BITS(11, 0, ~0) | PUT_BIT(15, 1) | PUT_BIT(31, 1));
  new = (cf & msk) | PUT_BITS(11,  0, rat) | PUT_BIT(31, 1);
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

  /*
   * Notify PM of change
   *TBD: not supported, remove?
   */
  return 0;
}


int
mr_set_fan(void * p)
{
  struct mr_set_fan * fc;

  /*
   * Ensure operation is valid, i.e. no garbage
   * in override flag (only 1 and 0 allowed) and
   * that pwm in in range 0 through 99.
   */
  fc = (struct mr_set_fan *) p;
  if (GET_BITS(7, 1, fc->override) || fc->pwm >= 100)
    return -MR_ERR_RANGE;

#if USE_SMC
  {
    uint32_t	dat;

    /*
     * Determine the PWM-adder value, and send it to the SMC. 
     * Subsequent 'GET' fan will add the calculated PWM and
     * this adder to report current PWM percentage.
     * Only way to retrieve the adder is via GET_SMC(0x4b).
     */
    if (fc->override)
      dat = fc->pwm;
    else
      dat = 0;

    if (mr_smc_wr(MR_SMC_FAN_PWM_ADD, &dat))
      return -MR_ERR_SMC;
  }
#else
  /*
   * Read-modify-write the fan override register
   * Control of fan #1 only, don't touch #2
   * Note: require SMC to support SBOX registers
   *       which is not on the radar right now.
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
  uint32_t		tmp;
#if USE_PM
  void		     (* fnc)(int);
#endif

  /*
   * Only values 0 and 1 allowed
   */
  tmp = *(uint32_t *) p;
  if (GET_BITS(31, 1, tmp))
    return -MR_ERR_RANGE;
  trbo.set = tmp;

#if USE_PM
  /*
   * Notify PM of new value
   */
  fnc = pm_cb.micpm_set_turbo;
  if (fnc)
    fnc(trbo.set);
#endif

  return 0;
}


int
mr_set_led(void * p)
{
  uint32_t		led;

  /*
   * Only values 0 and 1 allowed
   */
  led = *(uint32_t *) p;
  if (GET_BITS(31, 1, led))
    return -MR_ERR_RANGE;

  if (mr_smc_wr(MR_SMC_LED_CODE, &led))
    return -MR_ERR_SMC;
 
  return 0;
}


int
mr_set_prochot(void * p)
{
  struct mr_rsp_ptrig * trig;
  uint32_t		pwr0;
  uint32_t		time0;

  trig = (struct mr_rsp_ptrig *) p;
  pwr0 = trig->power;
  time0 = trig->time;

  /*
   * Check for sane values
   *TBD: check pwr0 higher than current pwr1?
   */
  if (pwr0 < 50 || pwr0 > 400)
    return -MR_ERR_RANGE;
  if (time0 < 50 || time0 > 1000)
    return -MR_ERR_RANGE;

  if (mr_smc_wr(MR_SMC_PWR_LIM_0, &pwr0) ||
      mr_smc_wr(MR_SMC_TIME_WIN_0, &time0))
    return -MR_ERR_SMC;
 
  return 0;
}


int
mr_set_pwralt(void * p)
{
  struct mr_rsp_ptrig * trig;
  uint32_t		pwr1;
  uint32_t		time1;

  trig = (struct mr_rsp_ptrig *) p;
  pwr1 = trig->power;
  time1 = trig->time;

  /*
   * Check for sane values
   *TBD: check pwr1 lower than current pwr0?
   */
  if (pwr1 < 50 || pwr1 > 400)
    return -MR_ERR_RANGE;
  if (time1 < 50 || time1 > 1000)
    return -MR_ERR_RANGE;

  if (mr_smc_wr(MR_SMC_PWR_LIM_1, &pwr1) ||
      mr_smc_wr(MR_SMC_TIME_WIN_1, &time1))
    return -MR_ERR_SMC;
 
  return 0;
}


int
mr_set_perst(void * p)
{
  uint32_t		perst;

  /*
   * Only values 0 and 1 allowed
   */
  perst = *(uint32_t *) p;
  if (GET_BITS(31, 1, perst))
    return -MR_ERR_RANGE;

  if (mr_smc_wr(MR_SMC_PWR_LIM_PERS, &perst))
    return -MR_ERR_SMC;
 
  return 0;
}
  

#if USE_PM
/*
**
** API functions dedicated for PM support
**
** These functions are embedded within the MT callout table
** and thus needs to follow the calling convention, which
** for 'get' functions is to pass an opague pointer to a buffer
** to hold retrieved data and on return get a staus code (positive
** on success, negative on failures) and for 'put' functions is
** to pass an opague pointer to a buffer holding input data.
**
** Function list as per PM needs:
**
**   pm_get_pl0		reads 0x2c, 0x2d and 0x2e
**   pm_set_pl0		writes 0x2c and 0x2d
**
**   pm_get_pl1		reads 0x2f and 0x30
**   pm_set_pl1		writes 0x2f and 0x30
**
**   pm_get_pavg	reads 0x35 and 0x36
**
**   pm_get_pttl	reads 0x38 and 0x39
**
**   pm_get_volt	reads 0x3c, 0x3d and 0x3e
**
**   pm_get_temp	reads 0x40, 0x43, 0x44 and 0x45
**
**   pm_get_tach	reads 0x49 and 0x4a
**
**   pm_get_tttl	reads 0x4e and 0x4f
**
**   pm_get_fttl	reads 0x2b
**   pm_set_fttl	writes 0x2b
**
*/

#include "micpm_api.h"

int
pm_get_pl0(void * p)
{
  struct pm_rsp_plim  * r;
  uint32_t		lim, win, grd;

  lim = 0;
  win = 0;
  grd = 0;
  mr_smc_rd(MR_SMC_PWR_LIM_0, &lim);
  mr_smc_rd(MR_SMC_TIME_WIN_0, &win);
  mr_smc_rd(MR_SMC_PWR_LIM0_GRD, &grd);

  r = (struct pm_rsp_plim *) p;
  r->pwr_lim = GET_BITS(15,  0, lim);
  r->time_win = GET_BITS(15,  0, win);
  r->guard_band = GET_BITS(15,  0, grd);

  return sizeof(*r);
}

int
pm_set_pl0(void * p)
{
  struct pm_cmd_plim  * r;

  /*
   * Only lower 16 bit used
   */
  r = (struct pm_cmd_plim *) p;
  if (GET_BITS(31, 16, r->pwr_lim))
    return -MR_ERR_RANGE;
  if (GET_BITS(31, 16, r->time_win))
    return -MR_ERR_RANGE;

  /*
   * This does not allow caller to tell which failed.
   *TBD: do we care?
   */
  if (mr_smc_wr(MR_SMC_PWR_LIM_0, &r->pwr_lim))
    return -MR_ERR_SMC;
  if (mr_smc_wr(MR_SMC_TIME_WIN_0, &r->time_win))
    return -MR_ERR_SMC;

  return 0;
}

int
pm_get_pl1(void * p)
{
  struct pm_rsp_plim  * r;
  uint32_t		lim, win;

  lim = 0;
  win = 0;
  mr_smc_rd(MR_SMC_PWR_LIM_1, &lim);
  mr_smc_rd(MR_SMC_TIME_WIN_1, &win);

  r = (struct pm_rsp_plim *) p;
  r->pwr_lim = GET_BITS(15,  0, lim);
  r->time_win = GET_BITS(15,  0, win);
  r->guard_band = 0;

  return sizeof(*r);
}

int
pm_set_pl1(void * p)
{
  struct pm_cmd_plim  * r;

  /*
   * Only lower 16 bit used
   */
  r = (struct pm_cmd_plim *) p;
  if (GET_BITS(31, 16, r->pwr_lim))
    return -MR_ERR_RANGE;
  if (GET_BITS(31, 16, r->time_win))
    return -MR_ERR_RANGE;

  /*
   * This does not allow caller to tell which failed.
   *TBD: do we care?
   */
  if (mr_smc_wr(MR_SMC_PWR_LIM_1, &r->pwr_lim))
    return -MR_ERR_SMC;
  if (mr_smc_wr(MR_SMC_TIME_WIN_1, &r->time_win))
    return -MR_ERR_SMC;

  return 0;
}

int
pm_get_pavg(void * p)
{
  struct pm_rsp_pavg  * r;
  uint32_t		pwr0, pwr1;

  pwr0 = PUT_BITS(31, 30, 3);
  pwr1 = PUT_BITS(31, 30, 3);
  mr_smc_rd(MR_SMC_AVG_PWR_0, &pwr0);
  mr_smc_rd(MR_SMC_AVG_PWR_1, &pwr1);

  r = (struct pm_rsp_pavg *) p;
  r->stat_0 = GET_BITS(31, 30, pwr0);
  r->stat_1 = GET_BITS(31, 30, pwr1);
  r->pwr_0  = GET_BITS(29,  0, pwr0);
  r->pwr_1  = GET_BITS(29,  0, pwr1);

  return sizeof(*r);
}

int
pm_get_pttl(void * p)
{
  struct pm_rsp_pttl  * r;
  uint32_t		dur, ttl;

  if (mr_smc_rd(MR_SMC_PWR_TTL, &ttl))
    return -MR_ERR_SMC;

  r = (struct pm_rsp_pttl *) p;
  r->pwr_ttl = GET_BIT(0, ttl);
  dur = PUT_BITS(31, 30, 3);
  if (r->pwr_ttl)
    mr_smc_rd(MR_SMC_PWR_TTL_DUR, &dur);
  r->stat_dur = GET_BITS(31, 30, dur);
  r->duration = GET_BITS(15,  0, dur);

  return sizeof(*r);
}

int
pm_get_volt(void * p)
{
  struct pm_rsp_volt  * r;
  uint32_t		vccp, vddg, vddq;

  vccp = PUT_BITS(31, 30, 3);
  vddg = PUT_BITS(31, 30, 3);
  vddq = PUT_BITS(31, 30, 3);
  mr_smc_rd(MR_SMC_VOLT_VCCP, &vccp);
  mr_smc_rd(MR_SMC_VOLT_VDDG, &vddg);
  mr_smc_rd(MR_SMC_VOLT_VDDQ, &vddq);

  r = (struct pm_rsp_volt *) p;
  r->stat_vccp = GET_BITS(31, 30, vccp);
  r->stat_vddg = GET_BITS(31, 30, vddg);
  r->stat_vddq = GET_BITS(31, 30, vddq);
  r->vccp  = GET_BITS(15,  0, vccp);
  r->vddg  = GET_BITS(15,  0, vddg);
  r->vddq  = GET_BITS(15,  0, vddq);

  return sizeof(*r);
}

int
pm_get_temp(void * p)
{
  struct pm_rsp_temp  * r;
  uint32_t		cpu, vccp, vddg, vddq;

  cpu =  PUT_BITS(31, 30, 3);
  vccp = PUT_BITS(31, 30, 3);
  vddg = PUT_BITS(31, 30, 3);
  vddq = PUT_BITS(31, 30, 3);
  mr_smc_rd(MR_SMC_TEMP_CPU,  &cpu);
  mr_smc_rd(MR_SMC_TEMP_VCCP, &vccp);
  mr_smc_rd(MR_SMC_TEMP_VDDG, &vddg);
  mr_smc_rd(MR_SMC_TEMP_VDDQ, &vddq);

  r = (struct pm_rsp_temp *) p;
  r->stat_cpu  = GET_BITS(31, 30, cpu);
  r->stat_vccp = GET_BITS(31, 30, vccp);
  r->stat_vddg = GET_BITS(31, 30, vddg);
  r->stat_vddq = GET_BITS(31, 30, vddq);
  r->cpu   = GET_BITS(15,  0, cpu);
  r->vccp  = GET_BITS(15,  0, vccp);
  r->vddg  = GET_BITS(15,  0, vddg);
  r->vddq  = GET_BITS(15,  0, vddq);

  return sizeof(*r);
}

int
pm_get_tach(void * p)
{
  struct pm_rsp_tach  * r;
  uint32_t		pwm, tach;

  pwm = PUT_BITS(31, 30, 3);
  tach = PUT_BITS(31, 30, 3);
  mr_smc_rd(MR_SMC_FAN_PWM, &pwm);
  mr_smc_rd(MR_SMC_FAN_TACH, &tach);

  r = (struct pm_rsp_tach *) p;
  r->stat_pwm  = GET_BITS(31, 30, pwm);
  r->stat_tach = GET_BITS(31, 30, tach);
  r->fan_pwm   = GET_BITS( 7,  0, pwm);
  r->fan_tach  = GET_BITS(15,  0, tach);

  return sizeof(*r);
}

int
pm_get_tttl(void * p)
{
  struct pm_rsp_tttl  * r;
  uint32_t		dur, ttl;

  if (mr_smc_rd(MR_SMC_TRM_TTL, &ttl))
    return -MR_ERR_SMC;

  r = (struct pm_rsp_tttl *) p;
  r->thrm_ttl = GET_BIT(0, ttl);
  dur = PUT_BITS(31, 30, 3);
  if (r->thrm_ttl)
    mr_smc_rd(MR_SMC_TRM_TTL_DUR, &dur);
  r->stat_dur = GET_BITS(31, 30, dur);
  r->duration = GET_BITS(15,  0, dur);

  return sizeof(*r);
}

int
pm_get_fttl(void * p)
{
  struct pm_rsp_fttl  * r;
  uint32_t		ttl;

  if (mr_smc_rd(MR_SMC_FORCE_TTL, &ttl))
    return MR_ERR_SMC;

  r = (struct pm_rsp_fttl *) p;
  r->forced = GET_BIT(0, ttl);

  return sizeof(*r);
}

int
pm_set_fttl(void * p)
{
  uint32_t		ttl;

  /*
   * Only values 0 and 1 allowed
   */
  ttl = ((struct pm_rsp_fttl *) p)->forced;
  if (GET_BITS(31, 1, ttl))
    return -MR_ERR_RANGE;

  if (mr_smc_wr(MR_SMC_FORCE_TTL, &ttl))
    return -MR_ERR_SMC;

  return 0;
}

#endif
