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
 * RAS PM interface
 *
 * Contains code to handle interaction with the PM driver.
 * This includes the initial upload of core voltages and 
 * frequencies, handling of 'turbo' mode, and accounting
 * for and reporting of card throttles.
 * This really is for KnC only.
 */


#include <linux/types.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/bitmap.h>
#include <linux/cpumask.h>
#include <linux/io.h>
#include <linux/cred.h>
#include <asm/msr.h>
#include <asm/mce.h>
#include <asm/apic.h>
#include <asm/mic/mic_common.h>
#include <asm/mic/mic_knc/micsboxdefine.h>
#include <scif.h>
#include "micras.h"
#include "monahan.h"
#include <asm/mic/micpm_device.h>

#if USE_PM

static atomic_t	pm_entry;	/* Active calls from PM */


/*
 * Local variables to keep track of throttle states
 *
 *   onoff	Set to 1 if throttling is in effect, otherwise 0
 *   count	Count of complete throttles (not counting current).
 *   time	Time spent in complete throttles
 *   start	Time when current throttle started (or 0)
 * 
 * Units of time is measured in jiffies and converted to mSecs
 * at the end of a throttle period. Jiffies are lower resolution
 * than mSec. If a throttle starts and ends within same jiffy,
 * a standard penalty of 1/2 jiffy gets added.
 *
 *TBD: perhaps it's better simply to add 1/2 jiffy to every throttle
 *     period to compensate for rounding down errors. Would be fair
 *     if average throttle period is more than 1 jiffy long.
 *
 *TBD: Using atomics may be overkill. Calls from the RAS MT thread
 *     will be serialized (guaranteed), i.e. the report routine needs
 *     not to care about re-entrancy.
 */

static atomic_t tmp_onoff;
static atomic_t tmp_count;
static atomic_long_t tmp_time;
static atomic_long_t tmp_start;

static atomic_t pwr_onoff;
static atomic_t pwr_count;
static atomic_long_t pwr_time;
static atomic_long_t pwr_start;

static atomic_t alrt_onoff;
static atomic_t alrt_count;
static atomic_long_t alrt_time;
static atomic_long_t alrt_start;


static void
mr_pwr_enter(void)
{
  if (atomic_xchg(&pwr_onoff, 1))
    return;

  atomic_long_set(&pwr_start, jiffies);
}

static void
mr_pwr_leave(void) {
  unsigned long	then;

  if (! atomic_xchg(&pwr_onoff, 0))
    return;

  then = atomic_long_xchg(&pwr_start, 0);
  atomic_inc(&pwr_count);

  if (jiffies == then)
    atomic_long_add(jiffies_to_msecs(1) / 2, &pwr_time);
  else
    atomic_long_add(jiffies_to_msecs(jiffies - then), &pwr_time);
}


static void
mr_tmp_enter(void)
{
  if (atomic_xchg(&tmp_onoff, 1))
    return;

  atomic_long_set(&tmp_start, jiffies);
}

static void
mr_tmp_leave(void)
{
  unsigned long	then;

  if (! atomic_xchg(&tmp_onoff, 0))
    return;

  then = atomic_long_xchg(&tmp_start, 0);
  atomic_inc(&tmp_count);
  if (jiffies == then)
    atomic_long_add(jiffies_to_msecs(1) / 2, &tmp_time);
  else
    atomic_long_add(jiffies_to_msecs(jiffies - then), &tmp_time);
}


static void
mr_alrt_enter(void)
{
  if (atomic_xchg(&alrt_onoff, 1))
    return;

  atomic_long_set(&alrt_start, jiffies);
}

static void
mr_alrt_leave(void)
{
  unsigned long	then;

  if (! atomic_xchg(&alrt_onoff, 0))
    return;

  then = atomic_long_xchg(&alrt_start, 0);
  atomic_inc(&alrt_count);
  if (jiffies == then)
    atomic_long_add(jiffies_to_msecs(1) / 2, &alrt_time);
  else
    atomic_long_add(jiffies_to_msecs(jiffies - then), &alrt_time);
}



/*
 * Report current throttle state(s) to MT.
 * Simple copy of local variables, except for the time
 * measurement, where current throttle (if any) is included.
 * Don't want a lock to gate access to the local variables,
 * so the atomics needs to be read in the correct order.
 * First throttle state, then adder if throttle is in
 * progress, then counters. If PM enters or leave throttle
 * while reading stats, the worst is that time for the
 * current trottle is not included until next read.
 */

int
mr_pm_ttl(struct mr_rsp_ttl * rsp)
{
  unsigned long	then;

  rsp->power.since = 0;
  rsp->power.active = (uint8_t) atomic_read(&pwr_onoff);
  if (rsp->power.active) {
    then = atomic_long_read(&pwr_start);
    if (then)
      rsp->power.since = jiffies_to_msecs(jiffies - then);
  }
  rsp->power.count = atomic_read(&pwr_count);
  rsp->power.time = atomic_long_read(&pwr_time);
  
  rsp->thermal.since = 0;
  rsp->thermal.active = (uint8_t) atomic_read(&tmp_onoff);
  if (rsp->thermal.active) {
    then = atomic_long_read(&tmp_start);
    if (then)
      rsp->thermal.since = jiffies_to_msecs(jiffies - then);
  }
  rsp->thermal.count = atomic_read(&tmp_count);
  rsp->thermal.time = atomic_long_read(&tmp_time);
  
  rsp->alert.since = 0;
  rsp->alert.active = (uint8_t) atomic_read(&alrt_onoff);
  if (rsp->alert.active) {
    then = atomic_long_read(&alrt_start);
    if (then)
      rsp->alert.since = jiffies_to_msecs(jiffies - then);
  }
  rsp->alert.count = atomic_read(&alrt_count);
  rsp->alert.time = atomic_long_read(&alrt_time);
  
  return 0;
}


/*
 * Throttle signaling function (call from PM)
 */

static int	ttl_tcrit;

void
mr_throttle(int which, int state)
{
  struct ttl_info ttl;
  uint32_t	   tmp;

  atomic_inc(&pm_entry);

  tmp = mr_sbox_rl(0, SBOX_THERMAL_STATUS_2);
  ttl.die = GET_BITS(19, 10, tmp);

  /*
   * PM is weird in the destinction of thermal and power throttle.
   * Power below PLIM should be quiet. Power between PLim1 and PLim0
   * results in TTL_POWER events.  Power above PLim0 results in both
   * TTL_POWER and TTL_THERMAL events, _even_ if temperature is well
   * below Tcrit. We handle this by maintaining 3 throttle related
   * event types: thermal throttles, power throttles and power alert.
   * The power alert is flaggend on entry as TTL_POWER, no problems.
   * The two throttles both come in as TTL_THERMAL, so we use current
   * die temperature to determine whether it was a thermal threshold
   * or the power limit that was exceeded. Point is power throttles
   * arriving while temperature is above Tcrit _will_ be counted as 
   * thermal throttles, period. 
   */
  ttl.upd = 0;
  switch(which) {
    case TTL_POWER:
      (state == TTL_OFF) ? mr_alrt_leave() : mr_alrt_enter();
      ttl.upd |= PM_ALRT_TTL_CHG;
      ttl.upd |= atomic_read(&alrt_onoff) ? PM_ALRT_TTL : 0;
      break;

    case TTL_THERMAL:
#if 1
       /*
        * Careful here: may get throttle ON while die > tcrit
	* and select thermal throttle correctly and then get
	* the corresponding throttle OFF when die has fallen
	* below tcrit in which case we must de-assert thermal
	* trottle.
	* As a shortcut, we deassert both throttles if the
	* GPU_HOT signal gets de-asserted (which is correct).
	*/
      if (state == TTL_OFF) {
        if (atomic_read(&pwr_onoff))
	  ttl.upd |= PM_PWR_TTL_CHG;
        if (atomic_read(&tmp_onoff))
	  ttl.upd |= PM_TRM_TTL_CHG;
	mr_pwr_leave();
	mr_tmp_leave();
      }
      else {
	if (ttl_tcrit && ttl.die < ttl_tcrit) {
	  if (! atomic_read(&pwr_onoff))
	    ttl.upd |= (PM_PWR_TTL_CHG | PM_PWR_TTL);
	  mr_pwr_enter();
	}
	else {
	  if (! atomic_read(&tmp_onoff))
	    ttl.upd |= (PM_TRM_TTL_CHG | PM_TRM_TTL);
	  mr_tmp_enter();
	}
      }
#else
      if (ttl_tcrit && ttl.die < ttl_tcrit) { 
        (state == TTL_OFF) ? mr_pwr_leave() : mr_pwr_enter();
        ttl.upd |= PM_PWR_TTL_CHG;
        ttl.upd |= atomic_read(&pwr_onoff) ? PM_PWR_TTL : 0;
      }
      else {
        (state == TTL_OFF) ? mr_tmp_leave() : mr_tmp_enter();
        ttl.upd |= PM_TRM_TTL_CHG;
        ttl.upd |= atomic_read(&tmp_onoff) ? PM_TRM_TTL : 0;
      }
#endif
     break;
  }

  micras_ttl_send(&ttl);
   
#if 0
   printk("ttl - args: which %d, state %d\n", which, state);

   printk("ttl - therm: on %d, count %d, time %ld, start %ld\n",
	atomic_read(&tmp_onoff), atomic_read(&tmp_count),
	atomic_long_read(&tmp_time), atomic_long_read(&tmp_start));

   printk("ttl - power: on %d, count %d, time %ld, start %ld\n",
	atomic_read(&pwr_onoff), atomic_read(&pwr_count),
	atomic_long_read(&pwr_time), atomic_long_read(&pwr_start));

   printk("ttl - alert: on %d, count %d, time %ld, start %ld\n",
	atomic_read(&alrt_onoff), atomic_read(&alrt_count),
	atomic_long_read(&alrt_time), atomic_long_read(&alrt_start));
#endif

   atomic_dec(&pm_entry);
}


/*
 * Throttle signaling function (call from notifier chain)
 *
 * TBD: should we test for odd state transitions and recursions?
 */

static int
mr_pm_throttle_callback(struct notifier_block *nb, unsigned long event, void *msg)
{
  atomic_inc(&pm_entry);

  switch(event) {
  
    case EVENT_PROCHOT_ON:	
      mr_throttle(TTL_THERMAL, TTL_ON);
      break;

    case EVENT_PROCHOT_OFF:
      mr_throttle(TTL_THERMAL, TTL_OFF);
      break;

    case EVENT_PWR_ALERT_ON:
      mr_throttle(TTL_POWER, TTL_ON);
      break;
   
    case EVENT_PWR_ALERT_OFF:
      mr_throttle(TTL_POWER, TTL_OFF);
      break;

    default:
      /*
       * Ignore whatever else is sent this way
       */
      break;
  }

  atomic_dec(&pm_entry);
  return 0;
}




/*
**
** Power management routines
**
**   one_mmio_rd	Read one MMIO register into memory safe
**   one_mmio_wr	Write one MMIO register from memory safe
**
**   one_msr_rd		Read one MSR register into memory safe
**   one_msr_wr		Write one MSR register from memory safe
**
**   mc_suspend		Prepare for suspend, preserve CSRs to safe
**   mc_suspend_cancel	Suspend canceled, restore operating mode
**   mc_resume		Recover from suspend, restore CSRs from safe
**
** For now this stores all registers that are used by this module.
** In reality, only those registers on power planes turned off in
** deep sleep states needs to be stored, but at this point it is
** not known which registers are in that group. This is a table
** driven mechanism that _only_ handles RAS related registers.
** 
**TBD: Turn off MC handlers while in suspend?
**     Both pro's and con's on this one, such as
**	 + Disabling uncore is easy, just clear INT_EN
**       + prevents MC to interfere with PM state transitions
**       - can hide corruption due to UC errors
**       - requires a lot of IPIs to shut down core MC handling
**       + there's nobody to handle MCs when cores are asleep.
**	 ? can events hide in *BOX banks during suspend/resume
**         and fire when restoring the INT_EN register?
**	 - Disabling core is not that easy (from a module).
**	   Enabling core MCEs requires setting flag X86_CR4_MCE
**         in CR4 on every core _and_ writing ~0 to MSR IA32_MCG_CAP
**         on every CPU. Probably better to let per-CPU routines
**	   like mce_suspend() and mce_resume() handle it, with
**         some care because we'd want to save all CTLs before
**         mce_suspend() runs and restore them after mce_resume().
**	   Problem is how to get at these functions; they are not
**	   exported and seems not to be hooked into the kernel's PM
**         call chains.  Perhaps sysclass abstraction ties into PM.
**         Even so, who's to invoke it and how?
*/

#define SAVE_BLOCK_MCA		1	/* Disable MC handling in suspend */
#define RAS_SAVE_MSR		1	/* Include global MSRs in suspend */
#define RAS_SAVE_CPU_MSR	0	/* Include per-CPU MSRs in suspend */

#define SBOX	1		/* SBOX register (index 0) */
#define DBOX	2		/* DBOX register (index 0..1) */
#define GBOX	3		/* GBOX register (index 0..7) */
#define TBOX	4		/* TBOX register (index 0..7) */
#define GMSR	5		/* Global MSR (index 0) */
#define LMSR	6		/* Per-CPU MSR (index 0..CONFIG_NR_CPUS-1) */

#define W64	(1 << 6)	/* 64 bit MMIO register (32 bit default) */
#define VLD	(1 << 7)	/* Register value valid, can be restored */

typedef struct _regrec {
  uint8_t	box;	/* Box type + width bit + valid bit */
  uint8_t	num;	/* Box index (or 0) */
  uint16_t	ofs;	/* MMIO byte offset / MSR number */
  uint64_t	reg;	/* Register value */
} RegRec;


/*
 * Rumor has it that SBOX CSRs below 0x7000 will survive deep sleep
 * Think it's safer to save/restore CSRs that RAS writes to anyways.
 * We'll leave out a bunch of RO CSRs, most of which are HW status.
 * SCRATCH<n> CSRs are above 0x7000 and needs to be preserved.
 *
 *TBD: Somebody else to preserve scratch CSRs not used by RAS?
 *     For now I'll save and restore all of them.
 */

static RegRec susp_mmio[] = {				/* Used in file */
  { SBOX, 0, SBOX_MCA_INT_EN, 0 },			/* Uncore, must be 1st */
  { SBOX, 0, SBOX_SCRATCH0, 0 },			/* - */
  { SBOX, 0, SBOX_SCRATCH1, 0 },			/* - */
  { SBOX, 0, SBOX_SCRATCH2, 0 },			/* - */
  { SBOX, 0, SBOX_SCRATCH3, 0 },			/* - */
  { SBOX, 0, SBOX_SCRATCH4, 0 },			/* Common, knc, */
  { SBOX, 0, SBOX_SCRATCH5, 0 },			/* - */
  { SBOX, 0, SBOX_SCRATCH6, 0 },			/* - */
  { SBOX, 0, SBOX_SCRATCH7, 0 },			/* Knc, knf */
  { SBOX, 0, SBOX_SCRATCH8, 0 },			/* - */
  { SBOX, 0, SBOX_SCRATCH9, 0 },			/* Common, knc, knf */
  { SBOX, 0, SBOX_SCRATCH10, 0 },			/* - */
  { SBOX, 0, SBOX_SCRATCH11, 0 },			/* - */
  { SBOX, 0, SBOX_SCRATCH12, 0 },			/* - */
  { SBOX, 0, SBOX_SCRATCH13, 0 },			/* Common */
  { SBOX, 0, SBOX_SCRATCH14, 0 },			/* - */
  { SBOX, 0, SBOX_SCRATCH15, 0 },			/* - */
//  { SBOX, 0, SBOX_COMPONENT_ID, 0 },			/* Knc  */
//  { SBOX, 0, SBOX_SVIDCONTROL, 0 },			/* Knc  */
//  { SBOX, 0, SBOX_PCIE_PCI_SUBSYSTEM, 0 },		/* Common */
//  { SBOX, 0, SBOX_PCIE_VENDOR_ID_DEVICE_ID, 0 },	/* Common */
//  { SBOX, 0, SBOX_PCIE_PCI_REVISION_ID_AND_C_0X8, 0 },/* Common */
  { SBOX, 0, SBOX_OC_I2C_ICR + ICR_OFFSET, 0 },		/* Elog */
  { SBOX, 0, SBOX_OC_I2C_ICR + ISR_OFFSET, 0 },		/* Elog */
  { SBOX, 0, SBOX_OC_I2C_ICR + ISAR_OFFSET, 0 },	/* Elog */
  { SBOX, 0, SBOX_OC_I2C_ICR + IDBR_OFFSET, 0 },	/* Elog */
//  { SBOX, 0, SBOX_OC_I2C_ICR + IBMR_OFFSET, 0 },	/* Elog */
//  { SBOX, 0, SBOX_COREVOLT, 0 },			/* Knc, knf */
//  { SBOX, 0, SBOX_COREFREQ, 0 },			/* Knc, knf */
//  { SBOX, 0, SBOX_MEMVOLT, 0 },			/* Knc, knf */
//  { SBOX, 0, SBOX_MEMORYFREQ, 0 },			/* Knc, knf */
//  { SBOX, 0, SBOX_CURRENTRATIO, 0 },			/* Knc */
//  { SBOX, 0, SBOX_BOARD_VOLTAGE_SENSE, 0 },		/* Knc, knf */
//  { SBOX, 0, SBOX_THERMAL_STATUS, 0 },		/* Knc, knf */
//  { SBOX, 0, SBOX_BOARD_TEMP1, 0 },			/* Knc, knf */
//  { SBOX, 0, SBOX_BOARD_TEMP2, 0 },			/* Knc, knf */
//  { SBOX, 0, SBOX_CURRENT_DIE_TEMP0, 0 },		/* Knc, knf */
//  { SBOX, 0, SBOX_CURRENT_DIE_TEMP1, 0 },		/* Knc, knf */
//  { SBOX, 0, SBOX_CURRENT_DIE_TEMP2, 0 },		/* Knc, knf */
//  { SBOX, 0, SBOX_MAX_DIE_TEMP0, 0 },			/* Knc, knf */
//  { SBOX, 0, SBOX_MAX_DIE_TEMP1, 0 },			/* Knc, knf */
//  { SBOX, 0, SBOX_MAX_DIE_TEMP2, 0 },			/* Knc, knf */
//  { SBOX, 0, SBOX_STATUS_FAN1, 0 },			/* Knc, knf */
//  { SBOX, 0, SBOX_STATUS_FAN2, 0 },			/* Knc, knf */
//  { SBOX, 0, SBOX_SPEED_OVERRIDE_FAN, 0 },		/* Knc, knf */
  { SBOX, 0, SBOX_MCA_INT_STAT, 0 },			/* Uncore */
//  { SBOX, 0, SBOX_APICRT16, 0 },			/* Uncore */
  { SBOX, 0, SBOX_MCX_CTL_LO, 0 },			/* Uncore */
  { DBOX, 0, DBOX_MC2_CTL, 0 },				/* Uncore */
#ifdef CONFIG_MK1OM
  { DBOX, 1, DBOX_MC2_CTL, 0 },				/* Uncore */
#endif
  { GBOX | W64, 0, GBOX_FBOX_MCA_CTL_LO, 0 },		/* Uncore */
  { GBOX | W64, 1, GBOX_FBOX_MCA_CTL_LO, 0 },		/* Uncore */
  { GBOX | W64, 2, GBOX_FBOX_MCA_CTL_LO, 0 },		/* Uncore */
  { GBOX | W64, 3, GBOX_FBOX_MCA_CTL_LO, 0 },		/* Uncore */
#ifdef CONFIG_MK1OM
  { GBOX | W64, 4, GBOX_FBOX_MCA_CTL_LO, 0 },		/* Uncore */
  { GBOX | W64, 5, GBOX_FBOX_MCA_CTL_LO, 0 },		/* Uncore */
  { GBOX | W64, 6, GBOX_FBOX_MCA_CTL_LO, 0 },		/* Uncore */
  { GBOX | W64, 7, GBOX_FBOX_MCA_CTL_LO, 0 },		/* Uncore */
#endif
#ifdef CONFIG_MK1OM
  { TBOX | W64, 0, TXS_MCX_CONTROL, 0 },		/* Uncore */
  { TBOX | W64, 1, TXS_MCX_CONTROL, 0 },		/* Uncore */
  { TBOX | W64, 2, TXS_MCX_CONTROL, 0 },		/* Uncore */
  { TBOX | W64, 3, TXS_MCX_CONTROL, 0 },		/* Uncore */
  { TBOX | W64, 4, TXS_MCX_CONTROL, 0 },		/* Uncore */
  { TBOX | W64, 5, TXS_MCX_CONTROL, 0 },		/* Uncore */
  { TBOX | W64, 6, TXS_MCX_CONTROL, 0 },		/* Uncore */
  { TBOX | W64, 7, TXS_MCX_CONTROL, 0 },		/* Uncore */
#endif
};

#if RAS_SAVE_MSR
static RegRec susp_msr[] = {				/* Used in file */
  { GMSR, 0, MSR_IA32_MCG_STATUS, 0 },			/* Uncore, kernel */
};

#if RAS_SAVE_CPU_MSR
static RegRec susp_lcl_msr[4 * CONFIG_NR_CPUS] = {	/* Used in file */
  { LMSR, 0, MSR_IA32_MCx_CTL(0), 0 },			/* Core, kernel */
  { LMSR, 0, MSR_IA32_MCx_CTL(1), 0 },			/* Core, kernel */
  { LMSR, 0, MSR_IA32_MCx_CTL(2), 0 },			/* Core, kernel */
  { LMSR, 0, MSR_IA32_MCG_CTL, 0 },			/* kernel */
  /*
   * The remaining entries is setup/replicated by pm_init()
   */
};
#endif
#endif


static void
one_mmio_rd(RegRec * r)
{
  switch(r->box & 0xf) {
    case SBOX:
	if (r->box & W64)
	   r->reg = mr_sbox_rq(0, r->ofs);
	else
	   r->reg = (uint64_t) mr_sbox_rl(0, r->ofs);
	break;
    case DBOX:
        if (r->box & W64)
	  r->reg = mr_dbox_rq(r->num, r->ofs);
	else
	  r->reg = (uint64_t) mr_dbox_rl(r->num, r->ofs);
	break;
    case GBOX:
        if (r->box & W64)
	  r->reg = mr_gbox_rq(r->num, r->ofs);
	else
	  r->reg = (uint64_t) mr_gbox_rl(r->num, r->ofs);
	break;
    case TBOX:
	if (mr_txs()) {
	  if (r->box & W64)
	    r->reg = mr_tbox_rq(r->num, r->ofs);
	  else
	    r->reg = (uint64_t) mr_tbox_rl(r->num, r->ofs);
	}
	break;
    default:
    	r->box &= ~VLD;
	return;
  }
  r->box |= VLD;

#if PM_VERBOSE
  printk("mmio_rd: box %d, idx %3d, ofs %04x -> %llx\n",
  	r->box & 0xf, r->num, r->ofs, r->reg);
#endif
}

static void
one_mmio_wr(RegRec * r)
{
  if (! (r->box & VLD))
    return;

  switch(r->box & 0xf) {
    case SBOX:
	if (r->box & W64)
	  mr_sbox_wq(0, r->ofs, r->reg);
	else
	  mr_sbox_wl(0, r->ofs, (uint32_t) r->reg);
	break;
    case DBOX:
	if (r->box & W64)
	  mr_dbox_wq(r->num, r->ofs, r->reg);
	else
	  mr_dbox_wl(r->num, r->ofs, (uint32_t) r->reg);
	break;
    case GBOX:
	if (r->box & W64)
	  mr_gbox_wq(r->num, r->ofs, r->reg);
	else
	  mr_gbox_wl(r->num, r->ofs, (uint32_t) r->reg);
	break;
    case TBOX:
	if (mr_txs()) {
	  if (r->box & W64)
	    mr_tbox_wq(r->num, r->ofs, r->reg);
	  else
	    mr_tbox_wl(r->num, r->ofs, (uint32_t) r->reg);
	}
	break;
  }
  r->box &= ~VLD;

#if PM_VERBOSE
  printk("mmio_wr: box %d, idx %3d, ofs %04x <- %llx\n",
  	r->box & 0xf, r->num, r->ofs, r->reg);
#endif
}


#if RAS_SAVE_MSR
static void
one_msr_rd(RegRec * r)
{
  uint32_t	hi, lo;

  switch(r->box & 0xf) {
    case GMSR:
    	rdmsr(r->ofs, lo, hi);
	break;
#if RAS_SAVE_CPU_MSR
    case LMSR:
    	rdmsr_on_cpu(r->num, r->ofs, &lo, &hi);
	break;
#endif
    default:
	r->box &= ~VLD;
	return;
  }
  r->reg = ((uint64_t) hi) << 32 | (uint64_t) lo;
  r->box |= VLD;

#if PM_VERBOSE
  printk("msr_rd: box %d, idx %3d, ofs %04x -> %llx\n",
  	r->box & 0xf, r->num, r->ofs, r->reg);
#endif
}

static void
one_msr_wr(RegRec * r)
{
  uint32_t	hi, lo;

  if (! (r->box & VLD))
    return;

  hi = r->reg >> 32;
  lo = r->reg & 0xffffffff;
  switch(r->box & 0xf) {
    case GMSR:
    	wrmsr(r->ofs, lo, hi);
	break;
#if RAS_SAVE_CPU_MSR
    case LMSR:
    	wrmsr_on_cpu(r->num, r->ofs, lo, hi);
	break;
#endif
  }
  r->box &= ~VLD;

#if PM_VERBOSE
  printk("msr_wr: box %d, idx %3d, ofs %04x <- %llx\n",
  	r->box & 0xf, r->num, r->ofs, r->reg);
#endif
}
#endif /* RAS_SAVE_MSR */


/*
 * Preserve all HW registers that will be lost in
 * deep sleep states. This will be SBOX registers
 * above offset 0x7000 and all other BOX registers.
 */

static void
mr_suspend(void)
{
  int		i;

  atomic_inc(&pm_entry);

  /*
   * Save SBOX_MCA_INT_EN first and clear it.
   * No more uncore MCAs will get through.
   */
  one_mmio_rd(susp_mmio + 0);
#if SAVE_BLOCK_MCA
  mr_sbox_wl(0, SBOX_MCA_INT_EN, 0);
#endif

  /*
   * Save remaining BOX MMIOs
   */
  for(i = 1; i < ARRAY_SIZE(susp_mmio); i++)
    one_mmio_rd(susp_mmio + i);

#if RAS_SAVE_MSR
  /*
   * Save global MSRs and set MCIP
   * No new exceptions will be asserted
   */
  for(i = 0; i < ARRAY_SIZE(susp_msr); i++)
    one_msr_rd(susp_msr + i);
#if SAVE_BLOCK_MCA
  wrmsr(MSR_IA32_MCG_STATUS, MCG_STATUS_MCIP, 0);
#endif

#if RAS_SAVE_CPU_MSR
  /*
   * Save per-CPU MSRs
   */
  for(i = 0; i < ARRAY_SIZE(susp_lcl_msr); i++)
    one_msr_rd(susp_lcl_msr + i);
#endif
#endif

  atomic_dec(&pm_entry);
}


/*
 * Undo side effects of a suspend call.
 * Nothing to do unless we turned MC handlers off.
 */

static void
mr_cancel(void)
{
  int i;

  atomic_inc(&pm_entry);

  /*
   * Restore SBOX_MCA_INT_EN to unblock uncore MCs
   * Invalidate all other saved MMIO registers.
   */
  one_mmio_wr(susp_mmio + 0);
  for(i = 1; i < ARRAY_SIZE(susp_mmio); i++)
    susp_mmio[i].box &= ~VLD;

#if RAS_SAVE_MSR
  /*
   * Restore IA32_MCG_STATUS to unblock core MCs
   * Invalidate all other saved MSR registers.
   */
  one_msr_wr(susp_msr + 0);
  for(i = 1; i < ARRAY_SIZE(susp_msr); i++)
    susp_msr[i].box &= ~VLD;

#if RAS_SAVE_CPU_MSR
  for(i = 0; i < ARRAY_SIZE(susp_lcl_msr); i++)
    susp_lcl_msr[i].box &= ~VLD;
#endif
#endif

  atomic_dec(&pm_entry);
}


/*
 * Restore all HW registers that we use.
 */

static void
mr_resume(void)
{
  int		i;

  atomic_inc(&pm_entry);

  /*
   * Clear uncore MCA banks (just in case)
   */
  if (susp_mmio[0].box & VLD) 
    box_reset(0);

  /*
   * Restore all BOX MMIOs but SBOX_MCA_INT_EN
   */
  for(i = 1; i < ARRAY_SIZE(susp_mmio); i++)
    one_mmio_wr(susp_mmio + i);

  /*
   * Then restore SBOX_MCA_INT_EN to enable uncore MCAs
   */
  one_mmio_wr(susp_mmio + 0);

#if RAS_SAVE_MSR
  /*
   * Restore all global MSRs but IA32_MCG_STATUS
   */
  for(i = 1; i < ARRAY_SIZE(susp_msr); i++)
    one_msr_wr(susp_msr + i);

  /*
   * Then restore IA32_MCG_STATUS to allow core MCAs
   */
  one_msr_wr(susp_msr + 0);

#if RAS_SAVE_CPU_MSR
  /*
   * Restore all per-cpu MSRs
   */
  for(i = 0; i < ARRAY_SIZE(susp_lcl_msr); i++)
    one_msr_wr(susp_lcl_msr + i);
#endif
#endif

  atomic_dec(&pm_entry);
}


/*
 * Callback from PM notifier chain.
 * TBD: should we test for odd state transitions and recursions?
 */

static int
mr_pm_callback(struct notifier_block *nb, unsigned long event, void *msg)
{

  switch(event) {
    case MICPM_DEVEVENT_SUSPEND:
      mr_suspend();
      break;

    case MICPM_DEVEVENT_RESUME:
      mr_resume();
      break;

    case MICPM_DEVEVENT_FAIL_SUSPEND:
      mr_cancel();
      break;

    default:
      /*
       * Ignore whatever else is sent this way
       */
      break;
  }

  return 0;
}



/*
**
** The PM module loads before RAS, so we must setup
** the API to support power management, i.e register.
** PM needs:
**  - Notification when MT changes certain variables.
**    Provided by a call-out list that the PM sets
**    at registration time.
**  - Access to MT calls. 
**    The PM module can use micras_mt_call() for access.
**    Since PM loads first, this function needs to
**    be passed at registration time.
** RAS needs:
**  - list of core voltages (for CVOLT query).
**    We pass a pointer to the voltage list and the
**    voltage list counter to PM module, who will
**    fill in the actual values (not available until
**    core-freq driver loads).
**  - list of core frequencies (for CFREQ query).
**    Same solution as for CVOLT.
**  - Notifications for throttle state changes.
**  - Power management notifications for suspend/resume.
**
** Note: can one notifier block be inserted in multiple
**       chains? Its assume not, which require two blocks
**	 both pointing to the same local function.
*/

extern struct mr_rsp_freq       freq;
extern struct mr_rsp_volt       volt;

struct micpm_params    pm_reg;		/* Our data for PM */
struct micpm_callbacks pm_cb;		/* PM data for us */

extern void micpm_device_register(struct notifier_block *n);
extern void micpm_device_unregister(struct notifier_block *n);
extern void micpm_atomic_notifier_register(struct notifier_block *n);
extern void micpm_atomic_notifier_unregister(struct notifier_block *n);

static struct notifier_block ras_deviceevent = {
  .notifier_call = mr_pm_callback,
};

static struct notifier_block ras_throttle_event_ns = {
  .notifier_call = mr_pm_throttle_callback,
};

static struct notifier_block ras_throttle_event = {
  .notifier_call = mr_pm_throttle_callback,
};


/*
 * Setup PM callbacks and SCIF handler.
 */

static int
pm_mt_call(uint16_t cmd, void * buf)
{
  int	err;

  atomic_inc(&pm_entry);
  err = micras_mt_call(cmd, buf);
  atomic_dec(&pm_entry);

  return err;
}


int __init
pm_init(void)
{
  extern int mr_smc_rd(uint8_t, uint32_t *);

#if RAS_SAVE_CPU_MSR
  /*
   * Preset MCA bank MSR register descriptions
   *
   *TBD: We have to use IPIs to read MSRs, which will wake
   *     up cores at sleep when this function is called. 
   *	 PM module may not like this at all.
   */
  int i, j;
  for(i = 1; i < nr_cpu_ids; i++) {
    j = 4 * i;
    susp_lcl_msr[j]     = susp_lcl_msr[0];
    susp_lcl_msr[j + 1] = susp_lcl_msr[1];
    susp_lcl_msr[j + 2] = susp_lcl_msr[2];
    susp_lcl_msr[j + 3] = susp_lcl_msr[3];
    susp_lcl_msr[j].num = i;
    susp_lcl_msr[j + 1].num = i;
    susp_lcl_msr[j + 2].num = i;
    susp_lcl_msr[j + 3].num = i;
  }
#endif

  /*
   * Get temperature where power throttle becomes thermal throttle
   */
  mr_smc_rd(0x4c, &ttl_tcrit);

  /*
   * Register with the MIC Power Management driver.
   */
  pm_reg.volt_lst = volt.supt;
  pm_reg.volt_len = &volt.slen;
  pm_reg.volt_siz = ARRAY_SIZE(volt.supt);
  pm_reg.freq_lst = freq.supt;
  pm_reg.freq_len = &freq.slen;
  pm_reg.freq_siz = ARRAY_SIZE(freq.supt);
  pm_reg.mt_call	   = pm_mt_call;
  pm_reg.mt_ttl		   = mr_throttle;
  if (micpm_ras_register(&pm_cb, &pm_reg))
    goto fail_pm;

  /*
   * Get into the PM notifier lists
   * MicPm reports events in 2 chains, one atomic and one
   * blocking. Our callback will not block!
   */ 
  micpm_atomic_notifier_register(&ras_throttle_event_ns);
  micpm_notifier_register(&ras_throttle_event);

  if (boot_cpu_data.x86_mask == KNC_C_STEP)
    micpm_device_register(&ras_deviceevent);

  printk("RAS.pm: init complete\n");
  return 0;

fail_pm:
  printk("RAS.pm: init failed\n");
  return 1;
}


/*
 * Cleanup for module unload.
 * Clear/restore hooks in the native MCA handler.
 */

void __exit
pm_exit(void)
{
  /*
   * Get off the PM notifier list
   */
  micpm_atomic_notifier_unregister(&ras_throttle_event_ns);
  micpm_notifier_unregister(&ras_throttle_event);

  if (boot_cpu_data.x86_mask == KNC_C_STEP)
    micpm_device_unregister(&ras_deviceevent);

  /*
   * De-register with the PM module.
   */
  micpm_ras_unregister();

  /*
   * Wait for an calls to module to finish.
   */
  while(atomic_read(&pm_entry))
    cpu_relax();

  printk("RAS.pm: exit complete\n");
}

#endif	/* USE_PM */
