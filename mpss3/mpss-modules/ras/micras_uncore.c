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
 * RAS handler for uncore MC events
 *
 * Contains code to intercept MC events, collect information
 * from uncore MCA banks and handle the situation.
 *
 * In case of a severe event, defined by corrupted context,
 * the handler will add a record of the event in the designated
 * EEPROM hanging off the Over Clocking I2C bus.  After that
 * a message will be sent to the SMC (enabling IPMI notifications)
 * and at last a message is sent to the host via the MC SCIF
 * connection.
 *
 * Lesser events will also be sent to the host on a 'FYI' basis,
 * but no rocord will be stored in the event log.
 *
 * This is in all aspects similar to the reaction to a severe
 * core MC event. Differences are in the MC bank access (mmio),
 * and that the event is delivered via an interrupt instead of
 * an exception. Still, the handler cannot expect any support
 * from the OS.
 */

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/nmi.h>
#include <asm/mce.h>
#include <asm/msr.h>
#include <asm/processor.h>
#include <asm/mic/mic_common.h>
#include <asm/mic/mic_knc/autobaseaddress.h>
#include <asm/mic/mic_knc/micsboxdefine.h>
#include "micras.h"


/*
 * Hooks placed in the native machine check handler
 * See file arch/x86/kernel/traps.c for placement
 *
 *  nmi		Entered NMI exception handler.
 *		Called before any other tests, which allow us
 *		to test for and handle un-core MCA events before
 *		the traditional NMI handling.
 *		Note that the mce-inject mechanism also uses
 *		NMI's to distribute calls to do_machine_check().
 */

extern int (*mca_nmi)(int);



/*
 * Table of un-core MCA banks.
 * Though there are differences in register count and sizes, un-core bank
 * registers are always spaced 8 bytes apart, so all we need to know is
 * the location of the first MCA bank register (CTL) to find them.
 * If bank is present, the bank register offsets for ctl, status, addr,
 * and misc are thus 0, 8, 16, and 24 respectively.
 * Default CTL masks pulled from the register documentation
 * Some SKUs don't have support for all BOXs but that will be handled
 * at runtime in the support code, not at compile time by this table.
 */


#ifdef CONFIG_ML1OM
#define SBOX_DEF	0x000e		/* All (7) */
#define	DBOX_DEF	0x0003		/* All (2) */
#define	GBOX_DEF	0x0003		/* All (2) */
#endif
#ifdef CONFIG_MK1OM
#define SBOX_DEF	0x03ce		/* All - PCIe errors (7) */
#define	DBOX_DEF	0x000f		/* All (4) */
#define	GBOX_DEF	0x3ffffffff	/* All (34) */
#define	TBOX_DEF	0x001f		/* All (5) */
#endif

#define MCU_CTL_64	(1 << 0)	/* Bank has 64 bit CTL register */
#define MCU_NO_ADDR	(1 << 1)	/* Bank has no ADDR register */
#define MCU_ADDR_32	(1 << 2)	/* Bank has 32 bit ADDR register */
#define MCU_NO_MISC	(1 << 3)	/* Bank has no MISC register */
#define MCU_MISC_64	(1 << 4)	/* Bank has 64 bit MISC register */

#define MCU_CTRL	0
#define MCU_STAT	8
#define MCU_ADDR	16
#define MCU_MISC	24

typedef struct _mcu_rec {
  uint8_t	num;				/* 'BOX' count */
  uint8_t	org;				/* Origin code */
  uint8_t	qflg;				/* Quirk flags */
  uint16_t	ofs;				/* MCA bank base offset */
  uint64_t	ctl;				/* Initial CTL mask */
  uint32_t	(*rl)(int, uint32_t);		/* 32-bit MMIO read */
  void		(*wl)(int, uint32_t, uint32_t);	/* 32-bit MMIO write */
  uint64_t	(*rq)(int, uint32_t);		/* 64-bit MMIO read */
  void		(*wq)(int, uint32_t, uint64_t);	/* 64-bit MMIO write */
} McuRec;


static McuRec	mcu_src[] = {
  { 1, MC_ORG_SBOX, MCU_MISC_64, SBOX_MCX_CTL_LO,
  	SBOX_DEF, mr_sbox_rl, mr_sbox_wl, mr_sbox_rq, mr_sbox_wq },
  { DBOX_NUM, MC_ORG_DBOX, MCU_NO_MISC, DBOX_MC2_CTL,
  	DBOX_DEF, mr_dbox_rl, mr_dbox_wl, mr_dbox_rq, mr_dbox_wq },
  { GBOX_NUM, MC_ORG_GBOX, MCU_CTL_64, GBOX_FBOX_MCA_CTL_LO,
  	GBOX_DEF, mr_gbox_rl, mr_gbox_wl, mr_gbox_rq, mr_gbox_wq },
#ifdef CONFIG_MK1OM
  { TBOX_NUM, MC_ORG_TBOX, MCU_CTL_64 | MCU_NO_MISC | MCU_ADDR_32, TXS_MCX_CONTROL,
  	TBOX_DEF, mr_tbox_rl, mr_tbox_wl, mr_tbox_rq, mr_tbox_wq },
#endif
};

#define GBOX_BROKEN	1		/* Set if GBOX MCA bank is borken */

#if GBOX_BROKEN
/*
 * Si design managed to break the GBOX MCA bank concept
 * by not filling useful data into ADDR and MISC registers.
 * Instead they use a bunch of registers in another part
 * of the GBOX (mbox to be specific) to hold this info.
 * In order to get at the right register it is necesary
 * to partially decode the STATUS register and from there
 * select an GBOX.MBOX register.
 * Since the new registers are all 32 bits wide, we'll stick
 * the value into MISC register if Misc_V bit of STATUS is
 * not set. The following table is used for register selection
 *
 * model code	base	width	Chan	Notes
 *    0		017c	32	0	26 bit address, CRC (retrain)
 *    1		097c	32	1	26 bit address, CRC (retrain)
 *    2		01e0	32	0	26 bit address, ECC
 *    3		09e0	32	1	26 bit address, ECC
 *    4		01dc	32	0	26 bit address, UC CAPE
 *    5		09dc	32	1	26 bit address, UC CAPE
 *   31		01a4    32	0	26 bit address, UC ECC
 *   32		09a4	32	1	26 bit address, UC ECC
 *
 * Note: model code is simply the enable bit number in CTL
 */ 

static struct liu {
  uint16_t	mcode;
  uint16_t	base;
} liu[] = {
  {  0, 0x17c },	/* Correctable CRC (retrain) ch 0 */
  {  1, 0x97c },	/* Correctable CRC (retrain) ch 1 */
  {  2, 0x1e0 },	/* Correctable ECC, ch 0 */
  {  3, 0x9e0 },	/* Correctable ECC, ch 1 */
  {  4, 0x1dc },	/* Uncorrectable CAPE, ch 0 */
  {  5, 0x9dc },	/* Uncorrectable CAPE, ch 1 */
  { 31, 0x1a4 },	/* Uncorrectable ECC, ch 0 */
  { 32, 0x9a4 }		/* Uncorrectable ECC, ch 1 */
};

static void
mcu_gbox_fixup(McuRec * mr, int num, MceInfo * mi)
{
  int i;
  uint16_t	mcode;

  /*
   * Skip if Status.Misc_v set
   */
  if (mi->status & (1ULL << 59))
    return;

  /*
   * Get model code and if it's in the array, then read
   * the addressed register into MISC. We don't set the
   * Status.Misc_v bit because we want to distinguish
   * this hack from the real MCA bank register.
   */
  mcode = GET_BITS(31, 16, mi->status);
  for(i = 0; i < ARRAY_SIZE(liu); i++)
    if (liu[i].mcode == mcode) {
      mi->misc = (uint64_t) mr->rl(num, liu[i].base);
      break;
    }
}
#endif 

/*
 * Read Ctrl, Addr and Misc registers from an un-core MCA bank.
 * The Status register is read/cleared in mcu_scan().
 */

static void
mcu_read(McuRec * mr, int num, MceInfo * mi)
{
  if (mr->qflg & MCU_CTL_64)
    mi->ctl = mr->rq(num, mr->ofs + MCU_CTRL);
  else
    mi->ctl = (uint64_t) mr->rl(num, mr->ofs + MCU_CTRL);

  if (mr->qflg & MCU_NO_ADDR)
    mi->addr = 0;
  else {
    if (mr->qflg & MCU_ADDR_32)
      mi->addr = (uint64_t) mr->rl(num, mr->ofs + MCU_ADDR);
    else
      mi->addr = mr->rq(num, mr->ofs + MCU_ADDR);
  }

  if (mr->qflg & MCU_NO_MISC)
    mi->misc = 0;
  else {
    if (mr->qflg & MCU_MISC_64)
      mi->misc = mr->rq(num, mr->ofs + MCU_MISC);
    else
      mi->misc = (uint64_t) mr->rl(num, mr->ofs + MCU_MISC);
  }

#if GBOX_BROKEN
  if (mr->org == MC_ORG_GBOX)
    mcu_gbox_fixup(mr, num, mi);
#endif 
}


/*
 * Reset one un-core MCA bank
 * Any quirks go here.
 */

static void
mcu_reset(McuRec * mr, int num, int arm)
{
  uint64_t	ctl;

  mr->wq(num, mr->ofs + MCU_STAT, 0);

  if (! (mr->qflg & MCU_NO_ADDR)) {
    if (mr->qflg & MCU_ADDR_32)
      mr->wl(num, mr->ofs + MCU_ADDR, 0);
    else
      mr->wq(num, mr->ofs + MCU_ADDR, 0);
  }

  if (! (mr->qflg & MCU_NO_MISC)) {
    if (mr->qflg & MCU_MISC_64)
      mr->wq(num, mr->ofs + MCU_MISC, 0);
    else
      mr->wl(num, mr->ofs + MCU_MISC, 0);
  }

  ctl = arm ? mr->ctl : 0;

#ifdef CONFIG_MK1OM
  if (ctl && mr->org == MC_ORG_SBOX && mic_hw_stepping(0) == KNC_A_STEP)
    ctl &= ~PUT_BIT(3, 1);	/* A0 SBOX 'unclaimed address' bug */

  if (ctl && mr->org == MC_ORG_GBOX && mr_mch() != 16)
    ctl &= ~(uint64_t) PUT_BIT(6, 1);	/* B0 GBOX 'Invalid Channel' (SKU 3 & 4) */
#endif

  if (mr->qflg & MCU_CTL_64)
    mr->wq(num, mr->ofs + MCU_CTRL, ctl);
  else
    mr->wl(num, mr->ofs + MCU_CTRL, ctl);
}


/*
 * Un-core MC bank pre-scan
 * Walk through all un-core MC sources to see if any events are pending.
 * Stops on 1st match where STATUS has both VAL bit set. On some BOXes,
 * like GBOX, interrupt may be signalled without the EN bit being set.
 * See HSD 4116374 for details.
 */

static int
mcu_prescan(void)
{
  int		i, j;
  uint64_t	status;
  struct _mcu_rec * mr;

  for(i = 0; i < ARRAY_SIZE(mcu_src); i++) {
    mr = mcu_src + i;

#ifdef CONFIG_MK1OM
    if (mr->org == MC_ORG_TBOX && !mr_txs())
      continue;
#endif

    for(j = 0; j < mr->num; j++) {
      status = mr->rq(j, mr->ofs + MCU_STAT);
      if (status & MCI_STATUS_VAL)
         return 1;
    }
  }

  return 0;
}


/*
 * Un-core MC bank scanner.
 * Walks through all un-core MC sources for new events.
 * If any found, then process them same way as core events.
 */

static int
mcu_scan(void)
{
  MceInfo	mc, uc;
  int		gone, seen;
  int		i, j;
  struct _mcu_rec * mr;

  /*
   * Walk list of known un-core MC sources
   */
  gone = seen = 0;
  memset(&uc, 0, sizeof(uc));
  for(i = 0; i < ARRAY_SIZE(mcu_src); i++) {
    mr = mcu_src + i;

#ifdef CONFIG_MK1OM
    if (mr->org == MC_ORG_TBOX && !mr_txs())
      continue;
#endif

    for(j = 0; j < mr->num; j++) {

      /*
       * Read status to see if we have something of interest.
       * As per HSD 4116374 the status register is cleared
       * after read, if it had valid content.
       *TBD: Clear unconditionally?
       */
      mc.status = mr->rq(j, mr->ofs + MCU_STAT);
      if (mc.status & MCI_STATUS_VAL)
        mr->wq(j, mr->ofs + MCU_STAT, 0);
      else
        continue;

      /*
       * Bank had valid content (VAL bit set).
       * Verify the event was subscribed to (EN bit set).
       * If not, the event is ignored.
       */
      if (! (mc.status & MCI_STATUS_EN))
	continue;

      /*
       * Valid and enabled event, read remaining bank registers.
       */
      seen++;
      mcu_read(mr, j, &mc);

      /*
       * Fill out blanks in the MceInfo record
       */
      mc.org = mr->org;
      mc.id = j;
      mc.stamp = get_seconds();
      mc.flags = (mc.status & MCI_STATUS_UC) ? MC_FLG_FATAL : 0;

      /*
       * If any way to detect injected errors then this is
       * the place to do so and indicate by MC_FLG_FALSE flag
       */

      if (mc.flags & MC_FLG_FATAL) {
#ifdef CONFIG_MK1OM
#if MC_VERBOSE
	ee_printk("Uncore fatal MC: org %d, id %d, status %lx\n", mc.org, mc.id, mc.status);
#endif

        /*
         * Log UC events in the eeprom.
         */
        micras_mc_log(&mc);
        mc.flags |= MC_FLG_LOG;
    
        /*
         * Notify SMC that we've had a serious machine check error.
         */
        micras_mc_ipmi(&mc, 1);
#endif
	/*
	 * Remember 1st fatal (UC) event
	 */
        if (! gone++)
	  uc = mc;
      }
    
      /*
       * Notify host
       */
      micras_mc_send(&mc, 1);

      /*
       * Filter corrected errors.
       */
      if (! (mc.flags & MC_FLG_FATAL)) {
	uint64_t	tsc, msk;

	tsc = rdtsc();
        msk = micras_mc_filter(&mc, tsc, 1);
	if (msk) {
#if MC_VERBOSE
	  ee_printk("Uncore filter: org %d, id %d, ctrl %lx, mask %lx\n", mc.org, mc.id, mc.ctl, msk);
#endif
	  if (mr->qflg & MCU_CTL_64)
	    mr->wq(j, mr->ofs + MCU_CTRL, mc.ctl & ~msk);
	  else
	    mr->wl(j, mr->ofs + MCU_CTRL, (uint32_t)(mc.ctl & ~msk));
	}
      }

      /*
       * Any event post processing goes here.
       * This would be things like cache line refresh and such.
       * Actual algorithms are TBD.
       */
    }
  }

#if RAS_HALT
  if (gone) {
    atomic_inc(&mce_entry);
    panic("FATAL un-core machine check event:\n"
          "bnk %d, id %d, ctl %llx, stat %llx, addr %llx, misc %llx\n",
	  uc.org, uc.id, uc.ctl, uc.status, uc.addr, uc.misc);
  }
#endif

  return seen;
}


/*
 * NMI handler.
 *
 * Once we get control in 1st interrupt (NMI or regular), we'll
 * use IPIs from the local APIC to force all active CPU's into
 * our RAS NMI handler, similar to the core MC handler.
 * After that, the same logic as for the generic MC handler is
 * applied to corral all CPU's through well defined rendez-vous
 * points where only one cpu gets to run the un-core MC event
 * scan while everybody else are sitting in a holding pen.
 * If containment wasn't an issue we could simply let the BP
 * run the scan without involving other CPUs at all.
 */

#define SPINUNIT	50
#define SERIAL_MCU	0

struct cpumask	mcu_exc_mask;		/* NMI recipients */
static int	mcu_cpu = -1;		/* SBOX target CPU */
#if MCU_NMI
static uint64_t	mcu_redir;		/* SBOX I/O-APIC redirection entry */
static uint64_t	mcu_old_redir;		/* Restore value for redirection entry */
#else
unsigned int	mcu_eoi;		/* 1st interrupt from local APIC */
#endif
static atomic_t mcu_callin;		/* Entry rendez-vous gate */
static atomic_t mcu_leavin;		/* Hold rendez-vous gate */


static int
mcu_timed_out(int64_t * timeout)
{
  if (*timeout < SPINUNIT)
    return 1;

  *timeout -= SPINUNIT;
  touch_nmi_watchdog();
  ndelay(SPINUNIT);

  return 0;
}


static int
mcu_wait(void)
{
  int		cpus, order;
  int64_t	timeout;

  cpus = num_online_cpus();
  timeout = 1 * NSEC_PER_SEC;		/* 1 Second */

  /*
   * Flush all caches
   */

  /*
   * 'Entry' rendez-vous point.
   * Wait here until all CPUs has entered.
   */
  order = atomic_inc_return(&mcu_callin);
  while(atomic_read(&mcu_callin) != cpus) {
    if (mcu_timed_out(&timeout)) {
      /*
       * Timout waiting for CPU enter rendez-vous
       */
      return -1;
    }
  }

  /*
   * 'Hold' rendez-vous point.
   * All CPUs drop by here 'simultaneously'.
   * The first CPU that 'enter'ed (order of 1) will
   * fall thru while the others wait until their
   * number number comes up in the 'leavin' counter
   * (or if a timeout happens). This also has a
   * serializing effect, where one CPU leaves this
   * loop at a time.
   */
  if (order == 1) {
#if SERIAL_MCU
    atomic_set(&mcu_leavin, 1);
#endif
  }
  else {
    while(atomic_read(&mcu_leavin) < order) {
      if (mcu_timed_out(&timeout)) {
        /*
         * Timout waiting in CPU hold rendez-vous
         */
        return -1;
      }
    }
  }

  return order;
}


static int
mcu_go(int order)
{
  int		ret;
  int64_t	timeout;

  ret = -1;
  if (order < 0)
    goto mcu_reset;

#if SERIAL_MCU
  /*
   * If any 'per-CPU' activity is needed in isolation
   * (one CPU at a time) then that code needs to go here.
   */

  atomic_inc(&mcu_leavin);		/* Next CPU out of hold */
#endif

  timeout = NSEC_PER_SEC;		/* 1 Second */
  if (order == 1) {
    int		cpus;

    /*
     * The first CPU that entered (order of 1) waits here
     * for the others to leave the 'hold' loop in mca_wait()
     * and enter the 'exit' rendez-vous loop below.
     * Once they are there, it will run the uncore MCA bank
     * scan while the others are parked in 'exit' loop below.
     */
    cpus = num_online_cpus();
#if SERIAL_MCU
    while(atomic_read(&mcu_leavin) <= cpus) {
      if (mcu_timed_out(&timeout)) {
        /*
         * Timout waiting for CPU exit rendez-vous
         */
        goto mcu_reset;
      }
    }
#else
    atomic_set(&mcu_leavin, cpus);
#endif
    mcu_scan();
    ret = 0;
  }
  else {
    /*
     * Exit rendez-vous point.
     */
    while(atomic_read(&mcu_leavin) != 0) {
      if (mcu_timed_out(&timeout)) {
        /*
         * Timout waiting in CPU exit rendez-vous
         */
        goto mcu_reset;
      }
    }
    return 0;
  }

  /*
   * Reset rendez-vous counters, letting all CPUs
   * leave this function 'simultaneously'.
   */
mcu_reset:
  atomic_set(&mcu_callin, 0);
  atomic_set(&mcu_leavin, 0);
  return ret;
}


/*
 * NMI exception handler
 * Uncertain if all cpumask_* functions implies barriers,
 * so erroring on the safe side explicit barriers is used.
 */

#if BEAM_TEST
static int
mcu_nmi(int cpu)
{
#ifdef CONFIG_MK1OM
  uint32_t	mcg_status_lo, mcg_status_hi;
#endif
  struct _mcu_rec * mr;
  MceInfo	mc;
  int		i, j;

  if (cpu != mcu_cpu)
    return 0;

  if (! mcu_prescan())
    return 0;

  wbinvd();

#ifdef CONFIG_MK1OM
  rdmsr(MSR_IA32_MCG_STATUS, mcg_status_lo, mcg_status_hi);
  wrmsr(MSR_IA32_MCG_STATUS, mcg_status_lo | MCG_STATUS_MCIP, mcg_status_hi);
#endif

  for(i = 0; i < ARRAY_SIZE(mcu_src); i++) {
    mr = mcu_src + i;

#ifdef CONFIG_MK1OM
    if (mr->org == MC_ORG_TBOX && !mr_txs())
      continue;
#endif

    for(j = 0; j < mr->num; j++) {
      mc.status = mr->rq(j, mr->ofs + MCU_STAT);

      if (! (mc.status & MCI_STATUS_VAL))
        continue;

      if (! (mc.status & MCI_STATUS_EN)) {
        mr->wq(j, mr->ofs + MCU_STAT, 0);
	continue;
      }

      mcu_read(mr, j, &mc);
      mr->wq(j, mr->ofs + MCU_STAT, 0);

      mc.org = mr->org;
      mc.id = j;
      mc.stamp = get_seconds();
      mc.flags = (mc.status & MCI_STATUS_UC) ? MC_FLG_FATAL : 0;
    
      micras_mc_send(&mc, 1);
    }
  }

#ifdef CONFIG_MK1OM
  wrmsr(MSR_IA32_MCG_STATUS, mcg_status_lo, mcg_status_hi);
#endif
  return 1;
  
  /*
   * Damn compiler options !!!!!!
   * Don't want more changes than this routine, so
   * added dummies to shut up gcc about unused code.
   */
  i = mcu_wait();
  mcu_go(i);
}
#else

static atomic_t mcu_entry;

static int
mcu_nmi(int cpu)
{
#ifdef CONFIG_MK1OM
  uint32_t	mcg_status_lo, mcg_status_hi;
#endif
  int		order, eoi;

  atomic_inc(&mcu_entry);

  /*
   * Get MCA status from SBOX.
   */
#if 0
  /*
   * If no source bits set, this was not an un-core MCA
   * This would work if the SBOX_MCA_INT_STAT actually worked
   * as described both in HAS and register specification.
   * Unfortunately, it doesn't, as per tribal knowledge errata.
   */
  uint32_t	int_stat, int_en;

  int_en = mr_sbox_rl(0, SBOX_MCA_INT_EN);
  int_stat = mr_sbox_rl(0, SBOX_MCA_INT_STAT);
  if (! (int_en & int_stat)) {
    atomic_dec(&mcu_entry);
    return 0;
  }
#else
  /*
   * Instead of having a single source of pending un-core MCA events,
   * we now have to walk all BOXes to check if there is a valid event
   * pending in one of them.  That is much more expensive as we have
   * to check this on all NMIs, including our own cascade NMIs used
   * to corrall all CPUs in their rendezvouz point(s). We try to avoid
   * this scan if there already is an un-core NMI in progress.
   * We know that:
   *  un-core MCA NMIs are sent to just one CPU, mcu_cpu
   *  CPUs targeted in the cascade are in mcu_exc_mask
   *  non-zero atomic variable 'mcu_callin' tells cascade is in progress
   */
  if (!cpumask_empty(&mcu_exc_mask))
    goto invited;
  if (cpu != mcu_cpu) {
    atomic_dec(&mcu_entry);
    return 0;
  }

  /*
   * On CPU 0 and no un-core handling in progress!
   * Then scan all BOXes for valid events pending,
   * If there wasn't any, this is a false alarm and
   * we'll re-connect MC lines and return.
   */
  if (! mcu_prescan()) {
    atomic_dec(&mcu_entry);
    return 0;
  }

invited:
#endif

  /*
   * Flush all caches.
   * This is uncore so it should not be necessary to
   * empty internal (L1) caches, doesn't harm either.
   */
  wbinvd();

  /*
   * We do not want to be interrupted by a core MC
   * exception while handling an NMI.  We can block
   * core MC events by setting the MCG_STATUS_MCIP.
   * This is a MSR, so it has to be done on all CPUs.
   * On KnC that is, KnF does not have that MSR.
   */
#ifdef CONFIG_MK1OM
  rdmsr(MSR_IA32_MCG_STATUS, mcg_status_lo, mcg_status_hi);
  wrmsr(MSR_IA32_MCG_STATUS, mcg_status_lo | MCG_STATUS_MCIP, mcg_status_hi);
#endif

  /*
   * Special for the SBOX NMI target CPU:
   *  - disconnect un-core MC lines from SBOX I/O-APIC, such
   *    that we don't get stacked NMIs in the Local APICs.
   *  - simulate a NMI broadcast by sending NMI to all _other_
   *    active CPUs via IPIs.  The SBOX could do a broadcast,
   *    but that will send NMIs to sleeping CPUs too, which
   *    we prefer to avoid if possible.
   *TBD: should creating the mcu_exc_mask be protected by
   *     lock, similar to core events? Who can interfere?
   */
  if (cpu == mcu_cpu) {
    mr_sbox_wl(0, SBOX_MCA_INT_EN, 0);
    cpumask_copy(&mcu_exc_mask, cpu_online_mask);
    cpumask_clear_cpu(cpu, &mcu_exc_mask);
    smp_wmb();
    // apic->send_IPI_mask(&mcu_exc_mask, NMI_VECTOR);
    apic->send_IPI_allbutself(NMI_VECTOR);
#if !MCU_NMI
    if (mcu_eoi) {
      smp_rmb();
      cpumask_set_cpu(cpu, &mcc_exc_mask);
      smp_wmb();
      mcu_eoi = 0;
    }
#endif
  }

  /*
   * Corral all CPUs through the rendez-vous point maze.
   * It guarantees that:
   *   - No CPU leaves mcu_wait() until all has entered.
   *   - One CPU leaves mcu_wait() at a time.
   *   - No CPU leaves mcu_go() until all has entered.
   *   - While one CPU is in transit between mcu_wait()
   *     and mcu_go(), all other CPUs are sitting in
   *     tight busy-wait loops in either function.
   *   - All CPUs leaves mcu_go() at the same time.
   * If there is any 'per-cpu' activity that needs to be
   * run in isolation, it must be placed between mcu_wait()
   * and mcu_go().
   */
  order = mcu_wait();
  if (mcu_go(order)) {
    /*
     * Timeout waiting at one of the rendez-vous points.
     * Scan the un-core MCA banks just in case.
     */
    mcu_scan();
  }

  /*
   * Special for the SBOX NMI target CPU:
   *  - reconnect un-core MC lines through to SBOX I/O-APIC.
   *    If new events already are pending, then this will
   *    result in a 'rising-edge' trigger to the I/O-APIC.
   */
  if (cpu == mcu_cpu)
    mr_sbox_wl(0, SBOX_MCA_INT_EN, mr_txs() ? 0x0fffff07 : 0xff07);

  /*
   * If this CPU got its NMI from an IPI, then it must
   * send an ACK to its local APIC (I think).
   */
  smp_rmb();
  eoi = cpumask_test_and_clear_cpu(cpu, &mcu_exc_mask);
  smp_wmb();
  if (eoi)
    ack_APIC_irq();

  /*
   * Restore core MCG status and return 1 indicating to the
   * kernel NMI handler we've handled it.
   *TBD: reduce to one write per core instead of one per thread?
   */
#ifdef CONFIG_MK1OM
  wrmsr(MSR_IA32_MCG_STATUS, mcg_status_lo, mcg_status_hi);
#endif
  atomic_dec(&mcu_entry);
  return 1;
}
#endif


#if !MCU_NMI
/*
 * MCA handler if using standard interrupts
 * It's just a trampoline to convert a regular interrupt
 * into an NMI, which is only needed if the I/O-APIC can't
 * generate and NMI.
 *
 *TBD: remove all this? It is not used on KnC, and the KnF's
 *     I've tested this on all have been OK sending NMIs.
 */

static irqreturn_t
sbox_handler(int irq, void * tag)
{
  /*
   * Convert this regular interrupt into an NMI.
   */
  mcu_cpu = smp_processor_id();
  mcu_eoi = 1;
  apic->send_IPI_self(NMI_VECTOR);
  return IRQ_HANDLED;
}
#endif


/*
 * Reset all uncore MCA banks to defaults
 */

void
box_reset(int arm)
{
  int		i, j;
  struct _mcu_rec * mr;

  for(i = 0; i < ARRAY_SIZE(mcu_src); i++) {
    mr = mcu_src + i;

#ifdef CONFIG_MK1OM
    if (mr->org == MC_ORG_TBOX && !mr_txs())
      continue;
#endif

    for(j = 0; j < mr->num; j++) {
      uint64_t	status;

      /*
       *TBD: Do we want to pick up existing MCA events or drop
       *     them because we don't know _when_ they occurred?
       *     Reporting them would require internal buffer because
       *     it's unlikely the SCIF MC session is up at this point.
       *     For now we just enter events into the system log.
       */
      status = mr->rq(j, mr->ofs + MCU_STAT);
      if (status & MCI_STATUS_VAL) {
	MceInfo		mc;

	mcu_read(mr, j, &mc);
	printk("RAS.uncore: discard MC event:\n"
	  "bnk %d, id %d, ctl %llx, stat %llx, addr %llx, misc %llx\n",
	  mr->org, j, mc.ctl, status, mc.addr, mc.misc);
      }

      /*
       * Reset MCA bank registers.
       */
      mcu_reset(mr, j, arm);
    }
  }
}


/*
 * Setup interrupt handlers by hooking into the SBOX's I/O-APIC.
 * For now, we send an NMI to single CPU, and let it process the
 * event. This may need to be expanded into a broadcast NMI similar
 * to what the generic core MC event handler does in order to keep
 * containment at high as we possibly can.
 *
 *TBD: code a dual rendez-vous mechanism on all active CPUs.
 */

int __init
mcu_init(void)
{
#if MC_VERBOSE
  int		i, j;
#endif

  if (mce_disabled) {
    printk("RAS.uncore: disabled\n");
  }
  else {
    /*
     * Clear rendez-vous counters
     */
    atomic_set(&mcu_callin, 0);
    atomic_set(&mcu_leavin, 0);

#if MC_VERBOSE
    /*
     * For debug only:
     * Record all SBOX I/O-APIC registers to kernel log
     */
    printk("SBOX_APICIDR: %lx\n", mr_sbox_rl(0, SBOX_APICIDR));
    printk("SBOX_APICVER: %lx\n", mr_sbox_rl(0, SBOX_APICVER));
    printk("SBOX_APICAPR: %lx\n", mr_sbox_rl(0, SBOX_APICAPR));
    for(i = 0; i < 26 ; i++)
      printk("APICCRT%d: %llx\n", i, mr_sbox_rq(0, SBOX_APICRT0 + (8 * i)));
    for(i = 0; i < 8 ; i++)
      printk("APICICR%d: %llx\n", i, mr_sbox_rq(0, SBOX_APICICR0 + (8 * i)));
    printk("SBOX_MCA_INT_EN: %lx\n", mr_sbox_rl(0, SBOX_MCA_INT_EN));
    printk("SBOX_MCA_INT_STAT: %lx\n", mr_sbox_rl(0, SBOX_MCA_INT_STAT));
#endif

    /*
     * Disconnect un-core MC lines from SBOX I/O-APIC, setup the
     * individual BOXes, and clear any un-core MC pending flags
     * from SBOX I/O-APIC
     */
    mr_sbox_wl(0, SBOX_MCA_INT_EN, 0);
    box_reset(1);
    mr_sbox_wl(0, SBOX_MCA_INT_STAT, 0);

    /*
     * Setup the SBOX I/O-APIC.
     * Un-core MC events are routed through a mask in register
     * SBOX_MCA_INT_EN into I/O APIC redirection table entry #16.
     * Ideally we want all uncore MC events to be handled similar
     * to core MCAs, which means we'd like an NMI on all CPUs.
     * On KnF the I/O-APIC may not trigger an NMI (PoC security)
     * and on KnC where NMI delivery is possible, it appears not
     * to be ideal to broadcast it to all CPUs because it could
     * wake up cores put to sleep bu power management rules.
     * See MCA HAS, SBOX HAS Vol 4, and A0 Vol 2 for details.
     *
     * The redirection table entry has the following format:
     *  47:32	Destination ID field
     *     17	Interrrupt set (testing: trigger an interrupt)
     *     16	Interrupt mask (0=enable, 1=disable)
     *     15	Trigger mode (0=edge, 1=level)
     *     14	Remote IRR (0=inactive, 1=accepted)
     *     13	Interrupt polarity (0=active_high, 1=active_low)
     *     12   Delivery status (0=idle, 1=send_pending)
     *     11	Destination mode (0=physical, 1=logical)
     *  10:8	Delivery mode (0=fixed, low, SMI, rsvd, NMI, INIT, rsvd, ext)
     *   7:0	Interrupt vector
     *
     * The I/O-APIC input is 'rising edge', so we'd need to select
     * it to be edge triggered, active high.
     */
#if MCU_NMI
    /*
     * If event delivery by NMI is preferred, we want it delivered on
     * the BP. There is already an NMI handler present, so we have to
     * tap into the existing NMI handler for the event notifications.
     *
     * The bit-fiddling below says:
     *   NMI delivery | Destination CPU APIC ID
     */
    mcu_cpu = 0;
    mcu_redir = PUT_BITS(10, 8, 4) | PUT_BITS(47, 32, (uint64_t) cpu_data(mcu_cpu).apicid);
    mcu_old_redir = mr_sbox_rq(0, SBOX_APICRT16);
    mr_sbox_wq(0, SBOX_APICRT16, mcu_redir | PUT_BITS(16, 16, 1));
    mr_sbox_wq(0, SBOX_APICRT16, mcu_redir);
#else
    /*
     * If event delivery by regular interrupt is preferred, then all
     * I/O-APIC setup will be handled by calling request_irq(16,..).
     * There is no guarantee that the event will be sent to the BP
     * (though it's more than likely) so we'll defer indentifying the
     * event handling CPU (mcu_cpu) till we receive the callback from
     * the interrupt handling sus-system.
     * The sbox_handler() function just converts the callback into an
     * NMI because the only way containment can be achieved is to be
     * able to lock down the system completely, which is not realistic
     * using regular interrupts.
     */
    mcu_eoi = 0;
    (void) request_irq(16, sbox_handler, IRQF_TRIGGER_HIGH, "un-core mce", (void *) 42);
#endif

    /*
     * Finally, place hook in NMI handler in case there's
     * an un-core event pending and connect un-core MC lines
     * through to SBOX I/O-APIC. From this point onwards we
     * can get uncore MC events at any time.
     */
    mca_nmi = mcu_nmi;
    mr_sbox_wl(0, SBOX_MCA_INT_EN, mr_txs() ? 0x0fffff07 : 0xff07);

#if MC_VERBOSE
    /*
     * For debug only
     * Record initial uncore MCA banks to kernel log.
     */
    printk("RAS.uncore: dumping all banks\n");

    /*
     * Dump all MCA registers we set to kernel log
     */
    for(i = 0; i < ARRAY_SIZE(mcu_src); i++) {
      char * boxname;
      struct _mcu_rec * mr;
      uint64_t  ctl, stat, addr, misc;

      mr = mcu_src + i;
#ifdef CONFIG_MK1OM
      if (mr->org == MC_ORG_TBOX && !mr_txs())
        continue;
#endif
      switch(mr->org) {
        case MC_ORG_SBOX: boxname = "SBOX";	break;
        case MC_ORG_DBOX: boxname = "DBOX";	break;
        case MC_ORG_GBOX: boxname = "GBOX";	break;
        case MC_ORG_TBOX: boxname = "TBOX";	break;
	default:	  boxname = "??";	/* Damn compiler */
      }

      for(j = 0; j < mr->num; j++) {

	if (mr->qflg & MCU_CTL_64)
	  ctl = mr->rq(j, mr->ofs + MCU_CTRL);
	else
	  ctl = (uint64_t) mr->rl(j, mr->ofs + MCU_CTRL);

        stat = mr->rq(j, mr->ofs + MCU_STAT);

	if (mr->qflg & MCU_NO_ADDR)
	  addr = 0;
	else {
	  if (mr->qflg & MCU_ADDR_32)
	    addr = (uint64_t) mr->rl(j, mr->ofs + MCU_ADDR);
	  else
	    addr = mr->rq(j, mr->ofs + MCU_ADDR);
	}

	if (mr->qflg & MCU_NO_MISC)
	  misc = 0;
	else {
	  if (mr->qflg & MCU_MISC_64)
	    misc = mr->rq(j, mr->ofs + MCU_MISC);
	  else
	    misc = (uint64_t) mr->rl(j, mr->ofs + MCU_MISC);
	}

        printk("RAS.uncore: %s[%d] = { %llx, %llx, %llx, %llx }\n",
		boxname, j, ctl, stat, addr, misc);
      }
    }
    printk("RAS.uncore: MCA_INT_EN = %x\n", mr_sbox_rl(0, SBOX_MCA_INT_EN));
    printk("RAS.uncore: APICRT16 = %llx\n", mr_sbox_rq(0, SBOX_APICRT16));
#endif

    printk("RAS.uncore: init complete\n");
  }

  return 0;
}


/*
 * Cleanup for module unload.
 * Clear/restore hooks in the SBOX's I/O-APIC.
 */

int __exit
mcu_exit(void)
{
  if (! mce_disabled) {

    /*
     * Disconnect uncore MC lines from SBOX I/O-APIC.
     * No new uncore MC interrupts will be made.
     */
    mr_sbox_wl(0, SBOX_MCA_INT_EN, 0);

    /*
     * Disconnect exception handler.
     */
#if MCU_NMI
    mcu_redir = 0;
    mr_sbox_wq(0, SBOX_APICRT16, mcu_old_redir);
#else
    mcu_eoi = 0;
    free_irq(16, (void *) 42);
#endif

    /*
     * Cut link from kernel's NMI handler and
     * wait for everybody in handler to leave.
     */
    mca_nmi = 0;
    while(atomic_read(&mcu_entry))
      cpu_relax();
    mcu_cpu = -1;

    /*
     * No more events will be received, clear
     * MC reporting in all BOXes (just in case)
     */
    box_reset(0);
  }

  printk("RAS.uncore: exit complete\n");
  return 0;
}

