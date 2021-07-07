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
 * RAS handler for core MC events
 *
 * Contains code to intercept MC events, collect information
 * from core MCA banks on originating core and possibly on
 * all active cores if necessary.
 *
 * In case of a severe event, defined by corrupted context,
 * the handler will add a record of the event in the designated
 * EEPROM hanging off the Over-Clocking I2C bus.  Next a message
 * will be sent to the SMC (enabling IPMI notifications) and at
 * last a message is sent to host via the MC SCIF connection
 * (if MC SCIF session has been established).
 *
 * Lesser events will also be sent to the host on a 'FYI' basis,
 * but no record will be stored in the event log, nor will the
 * SMC be notified.
 *
 * Special cases of high rate correctable errors may also cause
 * events to be recorded in EEPROM on the assumption that the
 * root cause will be detectable from maintenance mode.
 *
 * The handler cannot expect any support from the OS while in
 * exception (NMI) context. Therefore, NMI-safe routines has
 * been added to mimic some kernel services, e.g. ee_print().
 */

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/io.h>
#include <linux/cpumask.h>
#include <asm/mce.h>
#include <asm/apic.h>
#include "micras.h"


/*
**
** Brief design notes:
** There are two ways this code normally will be entered.
**
** 1) From standard interrupt context (bottom-half).
**    This is supporting MC events picked up by the
**    machine_check_poll(), i.e. events that aren't
**    causing state corrruption (UC bit not set).
**
** 2) From exception/NMI context.
**    This handles errors that _did_ flag processor
**    state corruption (UC bit set, or other condition
**    causing the kernel exception handler to pick it up).
**  
** Both cases can happen simultaneously on different CPU's,
** which require careful considerations about re-entrant code
** behaviour here. Particularly nasty is exception context where
** normal spinlocks won't work (FYI: x86 spinlocks assume interrupt
** disable can protect a critical region, an assumption that is
** false when an exception/NMI occur).
**
** Standard interrupt context entries occur when non-fatal and
** thus non-critical MC events are handled. In most cases just
** results in a regular SCIF send of McInfo structs to the host.
** Note that the call chain origin is a callout from the timer
** thread, not from an interrupt service routine, so to name
** it as standard interrupt context is somewhat misleading.
**
** Exception context messages are usuallly fatal and must be
** dealt with immediately, because otherwise the generic machine
** handler may panic() the system when exiting exception handler
** (default behavior, may be tweaked by altering 'threshold').
**
** In order to proceed we can either implement a locking mechanism
** at every API function entry, or we can let every function do it's
** thing independently. The latter is preferred, though it gets
** somewhat complicated because the API between the generic MC
** handling and RAS module is in fact composed of several calls.
**
** If state between API calls needs to be tracked then that can be
** done by means of pre-allocated arrays, similar to the generic
** handling in the Linux kernel. Currently the only state variable
** is the mask of CPUs that has been sent an IPI.
**
** Core MC events can be simulated by using the 'mce-inject' tool,
** consisting of a kernel module and a text mode application program.
** The 'mce-inject' module knows the difference between fatal and
** non-fatal events (defined by the UC bit) and acts differently
** in the two cases.  Non-fatal injections cause machine_check_poll()
** to be called on all CPUs, resulting in events being reported to
** function mce_poll().  Fatal injections cause do_machine_check()
** to be called on all CPUs, resulting in calls to the mcc_exc_*
** routines below.  Activities triggered by mce-inject are flagged
** as 'fake', and shall _NOT_ be logged in the EEPROM.
**
** Warning:
** Controls in the generic MC handling may cause the kernel to
** panic, _ALSO_ even if no event was found in any MCA banks!!
** Not sure exactly how to capture that sort of event.
**
** Warning:
** The 'mce-inject' module uses different methods of invoking error
** handling routines, depending on the mce record (inject_flags).
** Specifically, the 'mce-inject' module may use of broadcast NMIs
** to invoke machine_check_poll() or do_machine_check() on all CPUs,
** which will make these functions  execute in exception context.
** The NMI broadcast mechanism is based on registering a handler on
** the 'die' notifier chain and then doing an
**	apic->send_IPI_mask(.., NMI_VECTOR),
** knowing that do_nmi() will invoke this notifier chain when no
** genuine cause of NMI was found (i.e. if inb(61) returns 0xc0,
** [which is SERR + IOCHK on chipset register NSR]).
** Long story short; if 'mce-inject' is used we can not expect that
** polling is done in standard interrupt context, and need to set
** the 'in exception context' flag for SCIF access.
**
*/


/*
 * Hooks placed in the native machine check handler
 * See file arch/x86/kernel/cpu/mcheck/mce.c for placement.
 *
 *  poll	After entering a non-UC event into mce_log.
 *		This happens in normal thread context, which
 *		means that kernel services are avaialble.
 *  exc_flt	Filter on correctable errors. If events occur
 *		at a very high rate they can severely slow
 *		down the system and/or crash it entirely.
 *		Logic here will disable reporting of some
 *		events if they are seen too often.
 *  exc_entry	Entering MC exception handler.
 *		Called _after_ reading MCG_STATUS and the early
 *		severity assesment by mce_severity() has been
 *		performed on all banks, such that we get to
 *		know if the native MC handler will panic.
 *  exc_log	After entering a UC event into mce_log.
 *		The logged mce record has all available
 *		details on the event, and this point is the
 *		best place to perform our RAS activities.
 *  exc_panic	Right before the MC exception handler calls
 *		the panic function.
 *  exc_exit	Exit the MC exception handler
 *  print	Exception context safe printf to POST-card UART
 */

extern void (*mca_poll)(struct mce *, uint64_t, int);
extern void (*mca_exc_flt)(struct mce *, uint64_t, int);
extern void (*mca_exc_entry)(struct mce *, int, int, int, char *);
extern void (*mca_exc_log)(struct mce *, uint64_t, int, int, char *, int, int);
extern void (*mca_exc_panic)(struct mce *, char *, char *, int);
extern void (*mca_exc_exit)(struct mce *, int, int, int, int);
extern int  (*mca_print)(char *, ...);

extern struct mce_log   mcelog;		/* Export from kernel */
extern struct mutex     mce_read_mutex;	/* Export from kernel */
static unsigned		mcc_seen;	/* Last event in kernel log */
int			in_sync;	/* Flag when sync'ing */


/*
 * Convert a kernel mce record into a MC API format
 */

static void
mcc_conv(struct mce * mce, struct mce_info * mc)
{
  mc->org    = mce->bank;
  mc->id     = mce->extcpu;
#ifdef CONFIG_MK1OM
  mc->pid    = xlat_cpu[cpu_data(mc->id).apicid];
#endif
  mc->stamp  = mce->time;
  mc->status = mce->status;
  mc->addr   = mce->addr;
  mc->misc   = mce->misc;
  mc->flags  = (mc->status & MCI_STATUS_UC) ? MC_FLG_FATAL : 0;
}


/*
 * Filter for correctable errors, may modify CTL value.
 * The filter is pretty crude, we just want to protect
 * ourselves from being run over by fast recurring events.
 * We keep tabs of events seen in a static array.
 *
 * Algorithm is like this:
 *  - test if event is in filter list; if not exit filter.
 *  - search for instance of this event in history.
 *  - if not found, insert event in history (strike 1).
 *  - if found but time since last seen exceeds window,
 *    then treat event as new in history (new strike 1).
 *  - if found and within time window, bump strike counter.
 *  - if strike counter reach maximum, we're fed up and
 *    turn this event off by clearing the associated
 *    bit in the offending MCA bank's CTL register and
 *    send a 'filter' event notification to the host.
 *
 * Advantages of this design is:
 *  - individual parameters for every filtered event.
 *  - only one event history array.
 *  - no periodic aging of events in history array.
 *  - no averaging over time required.
 *  - no moving/reordering of event history entries.
 *  - new events do not replace older seen event
 *  - filter reacts immediately when max reached.
 *
 * Disadvantages are:
 *  - linear search through filter array.
 *  - linear search through history array.
 *  - time parameter not obvious, it's really a limit
 *    on how old events in history are allowed to be.
 *  - in pathological cases the filter's reaction time
 *    will be max * window (when events trickle in at
 *    a rate just below the window size).
 *  - data in ADDR and MISC registers are not used to
 *    match current event with history. Should they be?
 *
 * For now, both lists are short enough that introducing
 * more advanced searches probably are not going to help.
 *
 * On KnC the flash may have overrides of the mc_turnoff table.
 */

#define FT	((17 * 60) + 30) * 60	/* Default time window: 17.5 hours */

static struct mc_hist {
  uint32_t		count;		/* How many times seen */
  uint64_t		last;		/* TSC last time seen */
  struct mce_info	mc;		/* Local MC event record */
} mc_history[32];

static struct mc_disc {
  uint8_t	bank, ctl;		/* Bank selector and control bit # */
  uint16_t	win;			/* Time window (seconds) */
  uint16_t	max;			/* Max count */
  uint16_t	mca_code;		/* MCA code, status[15:0] */
  uint16_t	mdl_code;		/* Model code, status[31:16] */
} mc_turnoff[] = {
  { 0, 3, FT, 2, 0x0150, 0x0000 },	/* MC0: J-Cache error */
  { 1, 0, FT, 2, 0x010a, 0x0001 },	/* MC1: L2 Tag error */
  { 1, 4, FT, 2, 0x010a, 0x0010 },	/* MC1: L2 Data error */
  { 2, 2, FT, 2, 0x010d, 0x0100 },	/* MC2: Tag State, ext TD */
  { 2, 2, FT, 2, 0x010d, 0x0101 },	/* MC2: Tag State, int TD */
  { 2, 3, FT, 2, 0x012d, 0x0110 },	/* MC2: Core Valid, ext TD */
  { 2, 3, FT, 2, 0x012d, 0x0111 },	/* MC2: Core Valid, int TD */
  { 3, 2, FT, 2, 0x010d, 0x0100 },	/* DBOX: Tag State error, ext TD */
  { 3, 2, FT, 2, 0x010d, 0x0101 },	/* DBOX: Tag State error, int TD */
  { 3, 3, FT, 2, 0x012d, 0x0110 },	/* DBOX: Core Valid error, ext TD */
  { 3, 3, FT, 2, 0x012d, 0x0111 },	/* DBOX: Core Valid error, int TD */
  { 4, 4, FT, 2, 0x0e0b, 0x0030 },	/* SBOX: PCI-e */
  { 5, 0, FT, 2, 0x0001, 0x0000 },	/* GBOX: Ch-0 retraining */
  { 5, 1, FT, 2, 0x0001, 0x0001 },	/* GBOX: Ch-1 retraining */
  { 5, 2, FT, 2, 0x0001, 0x0002 },	/* GBOX: Ch-0 ECC error */
  { 5, 3, FT, 2, 0x0001, 0x0003 },	/* GBOX: Ch-1 ECC error */
  { 6, 3, FT, 2, 0x010e, 0x0008 },	/* TBOX: T2 CRC error */
};


#ifdef CONFIG_MK1OM

#define MC_FLT_SIG1	0x0e13c20f	/* Start signature */
#define MC_FLT_SIG2	0xf1ec3df0	/* End signature */
#define MC_FLT_SIZE	0x200		/* Filter block length */

void
mcc_flt_parm(uint8_t * p)
{
  uint16_t		fnum;

  /*
   * Check signatures
   */
  if (*((uint32_t *) p) != MC_FLT_SIG1 ||
      *((uint32_t *)(p + MC_FLT_SIZE - 4)) != MC_FLT_SIG2) {
    printk("mcc_flt_parm: signatures not found, (%08x, %08x)\n",
    		*((uint32_t *) p), *((uint32_t *)(p + MC_FLT_SIZE - 4)));
    return;
  }

  /*
   * After start signature comes filter count (uint16_t)
   * followed by 'count' filter descriptors (struct mc_disc).
   */
  fnum = *(uint16_t *)(p + 4);
  if (fnum > ARRAY_SIZE(mc_turnoff) ||
      fnum * sizeof(struct mc_disc) + 10 > MC_FLT_SIZE) {
    printk("mcc_flt_parm: filter count %d not valid\n", fnum);
    return;
  }

  /*
   * Seems the table is legit, copy it over defaults.
   */
  memset(mc_turnoff, '\0', sizeof(mc_turnoff));
  memcpy(mc_turnoff, p + 6, fnum * sizeof(struct mc_disc));
#if MC_VERBOSE
  {
    int i;

    for(i = 0; i < ARRAY_SIZE(mc_turnoff); i++) {
      printk("Filter %2d: bank %d, ctl %d, win %d, max %d, mca %04x, mdl %04x\n",
        	i, mc_turnoff[i].bank, mc_turnoff[i].ctl, mc_turnoff[i].win,
		mc_turnoff[i].max, mc_turnoff[i].mca_code, mc_turnoff[i].mdl_code);
    }
  }
#endif
}

#endif


/*
 * Frequency filter for core and un-core MC events
 */

uint32_t
micras_mc_filter(struct mce_info * mc, uint64_t tsc, int exc)
{
  struct mc_disc      * dsc;
  struct mc_hist      * hst;
  uint64_t		ostamp;
  int			i, oldest;

  if (mc->status & MCI_STATUS_UC)
    return 0;

  /*
   * Check if this event may be filtered
   */
  dsc = mc_turnoff;
  for(i = 0; i < ARRAY_SIZE(mc_turnoff); i++) {
    if (dsc->bank == mc->org &&
        dsc->mca_code == GET_BITS(15,  0, mc->status) &&
	dsc->mdl_code == GET_BITS(31, 16, mc->status))
      break;
    dsc++;
  }
  if (i == ARRAY_SIZE(mc_turnoff))
    return 0;

  /*
   * Have a candidate for filter.
   * Have we seen this one before?
   */
  oldest = 0;
  ostamp = tsc;
  hst = mc_history;
  for(i = 0; i < ARRAY_SIZE(mc_history); i++) {
    /*
     * While scanning, find the oldest event too
     */
    if (hst->last < ostamp) {
      ostamp = hst->last;
      oldest = i;
    }

    /*
     * Does this match event in filter history?
     * TBD: how much needs to match?
     *      For now: cpu (or box), bank, mca_code and model_code.
     */
    if (hst->last &&
        hst->mc.id == mc->id &&
        hst->mc.org == mc->org &&
	GET_BITS(15,  0, hst->mc.status) == GET_BITS(15,  0, mc->status) &&
	GET_BITS(31, 16, hst->mc.status) == GET_BITS(31, 16, mc->status))
      break;
    hst++;
  }
  if (i == ARRAY_SIZE(mc_history)) {
    /*
     * Not seen this event before.
     * 'oldest' is where to store this event.
     */
    hst = mc_history + oldest;
    hst->count = 1;
    hst->last = tsc;
    hst->mc = *mc;
    return 0;
  }

  /*
   * Already 'on file in history', test expiration date
   */
  if (hst->last + dsc->win * (cpu_khz * 1000LL) < tsc) {
    /*
     * Matching history element had expired, just overwrite it
     */
    hst->count = 1;
    hst->last = tsc;
    hst->mc = *mc;
    return 0;
  }

  /*
   * Filter element active, bump count and set last seen.
   * We do _NOT_ want injected events to enter the EEPROM,
   * so that flag is preserved over all event history
   */
  hst->count++;
  if (mc->flags & MC_FLG_FALSE)
    hst->mc.flags |= MC_FLG_FALSE;
  if (hst->count < dsc->max) {
    hst->last = tsc;
    return 0;
  }

  /*
   * Threshold reached, event source needs to be silenced.
   * Store a record of this in the EEPROM and send a
   * notification to host about it. Once duly reported, clear
   * event from the filter; it is not expected to show up again.
   * Note: we report the _first_ event seen, not the
   *       event at hand. We could save array space
   *       by sending latest event (less info to keep).
   */
  ee_printk("RAS: MCE filter #%d: bank %d, bit %d, limit %d, delta %d (mS)\n",
	    dsc - mc_turnoff, dsc->bank, dsc->ctl, dsc->max, (tsc - hst->last) / cpu_khz);
  hst->mc.flags |= MC_FLG_FILTER;
#ifdef CONFIG_MK1OM
  if (!(hst->mc.flags & MC_FLG_FALSE)) {
    micras_mc_log(&hst->mc);
    hst->mc.flags |= MC_FLG_LOG;
  }
#endif
  micras_mc_send(&hst->mc, exc);
  hst->last = 0;

  /*
   * MC events are disabled by caller when a
   * non-zero mask is returned by this routine.
   */
  return (1 << dsc->ctl);
}


/*
 * Remove/mask an 'enable-bit' from a core MCA bank.
 * Note: This applies to _current_ cpu only. It is not explicitly
 *       linked to the cpu that was ID'd in the incoming mce struct.
 *	 Happens to be OK for mcc_exc_flt() and mcc_poll() and mcc_exc_log().
 */

static void
mcc_ctl_mask(int bank, uint32_t msk)
{
  uint32_t		ctl_lo, ctl_hi;

  rdmsr(MSR_IA32_MCx_CTL(bank), ctl_lo, ctl_hi);
  ctl_lo &= ~msk;
  wrmsr(MSR_IA32_MCx_CTL(bank), ctl_lo, ctl_hi);

#if MC_VERBOSE
  ee_printk("RAS: ctl mask CPU %d, MC%d_CTL -> %x\n", smp_processor_id(), bank, ctl_lo);
#endif
}


/*
 * Filtering of correctable core MC events
 * Called from the exception handler.
 */

static void
mcc_exc_flt(struct mce * mce, uint64_t ctl, int fake)
{
  struct mce_info	mc;
  uint32_t		msk;

  if (!mce)
    return;

  if (mce->status & MCI_STATUS_UC)
    return;

  mcc_conv(mce, &mc);
  mc.ctl = ctl;
  mc.flags = fake ? MC_FLG_FALSE : 0;
  msk = micras_mc_filter(&mc, mce->tsc, 1);
  if (msk)
    mcc_ctl_mask(mce->bank, msk);
}


/*
 * Only action required for polled MC events is to
 * pass the event on to the SCIF channel (if connected).
 * The event should already have caused an excption (the
 * exception handler choses to ignore corrected errors)
 * which means it already has been filtered.
 * Injected corrected events do not cause MCE exceptions
 * and thus escaped filtering, so we'll filter them here.
 */

static void
mcc_poll(struct mce * mce, uint64_t ctl, int fake)
{
  struct mce_info	mc;

#if MC_VERBOSE
  ee_printk("RAS: poll %d, fake %d, status %llx\n", mce->extcpu, fake, mce->status);
#endif

  mcc_conv(mce, &mc);
  mc.ctl = ctl;
  mc.flags = fake ? MC_FLG_FALSE : 0;

#if BEAM_TEST
  /*
   * Under beam test we only want to send the SCIF message
   */
  micras_mc_send(&mc, fake);
  return;
#endif

  if (micras_mc_send(&mc, fake))
    mcc_seen = mcelog.next;

  /*
   * According to MCA HAS the MCI_STATUS_VAL will only
   * be set when an event's enable bit is set, in which
   * case it is difficult to imagine how events without
   * the MCI_STATUS_EN can appear here. The second clause
   * of the test may never actually happen on Kn{F,C}.
   * Note: MC polling does not capture TSCs
   */
  if (fake || !(mc.status & MCI_STATUS_EN)) {
    uint32_t		msk;

    msk = micras_mc_filter(&mc, rdtsc(), fake);
    if (msk)
      mcc_ctl_mask(mce->bank, msk);
  }
}


/*
 * One CPU entered do_machine_check().
 * We get the initial mce record (which has cpu ID), early
 * control variables and whether the event is injected.
 *
 * Since KnF and KnC deviate from the standard IA by not
 * having the core MCAs broadcast to all CPU's we'll try
 * to fake standard behavior in order to keep the generic
 * machine check code intact.
 * Therefore, if event is real (fake flag unset) and this
 * CPU is the first seeing it (mcc_exc_mask is empty),
 * then send IPI to all other CPU's listed in the online
 * cpumask for vector #18. Later CPUs will see themselves
 * marked in mcc_exc_mask and return quickly.
 */

struct cpumask	mcc_exc_mask;			/* CPU's in mce ctx */
static atomic_t ipi_lock = ATOMIC_INIT(0);	/* Lock on exc mask */

static void
mcc_exc_entry(struct mce * mce, int fake, int no_way_out, int entry, char * msg)
{
  unsigned int	cpu;

  /*
   *TBD: should we use 'extcpu' from the MCE record instead?
   */
  cpu = smp_processor_id();

  /*
   * Injected events invokes all CPUs automatically
   * by hooking into the NMI notify_die call_chain.
   * Nothing to do here.
   */
  if (fake)
    return;

#if 1
  /*
   * Avoid the IPI corralling circus on corrected errors,
   * based on assessment entirely done by mce_severity().
   * If the result (no_way_out) is MCE_NO_SEVERITY (=0), then
   * at worst we may have a correctable error, and that does
   * not warrant the system lockdown managed by mce_start()
   * and mce_end().
   * Note that MICs do not support newer status bits (MCG_SER_P)
   * which causes variable mce_ser always to be zero and thus
   * the test in the inner loop of do_machine_check() will be
   * reduced to just testing for the UC bit.
   */
  if (! no_way_out)
    return;
#endif

  /*
   * Test for entry from MT thread IPIs (testing)
   * or a 'soft' exception from a IPI issued from
   * the handler of the first exception.
   * No further action needed in both cases.
   */
  if (cpumask_test_cpu(cpu, &mcc_exc_mask))
    return;
 
  /*
   * Create mcc_exc_mask to flag which CPU's are
   * to be included in the IPI. This mask is later
   * used to determine who needs to EOI the local
   * APIC after MC event handling.
   */
  while(atomic_xchg(&ipi_lock, 1))
    cpu_relax();
  smp_rmb();
  if (cpumask_test_cpu(cpu, &mcc_exc_mask)) {
    /*
     * Another CPU got here first
     */
    atomic_xchg(&ipi_lock, 0);
    return;
  }
  cpumask_copy(&mcc_exc_mask, cpu_online_mask);
  cpumask_clear_cpu(cpu, &mcc_exc_mask);
  smp_wmb();
  atomic_xchg(&ipi_lock, 0);

  /*
   * Simulate a broadcast ny sending IPI to all
   * other CPUs.
   */
  // apic->send_IPI_mask(&mcc_exc_mask, MCE_VECTOR);
  apic->send_IPI_allbutself(MCE_VECTOR);
}


/*
 * In do_machine_check() bank scan loop.
 * Called from a lockdown, no synchronization needed.
 * MC bank scan is complete and the mce event has been
 * entered into the kernel MC log
 *
 *TBD: revise logic on HALT on UC events?
 *     From a state corruption point of view this
 *     _is_ a fatal error because UC bit was set.
 *     However, if the tolerance setting is set
 *     high enough, the generic MC handler may
 *     not chose to panic on this event.
 *     We currently do not have the tolerance value
 *     when recording this event, nor do we have 
 *     other factors that mce_reign() use to determine
 *     what to do after reporting event to the host.
 */

static void
mcc_exc_log(struct mce * mce, uint64_t ctl, int fake,
	    int no_way_out, char * msg, int severity, int worst)
{
  struct mce_info	mc;
  uint32_t		msk;

#if MC_VERBOSE
  ee_printk("RAS: log %d, wall %lld, nwo %d (%s), sev %d, wst %d\n",
	    mce->extcpu, mce->time, no_way_out, msg, severity, worst);
#endif

  /*
   * Create a message for the host.
   */
  mcc_conv(mce, &mc);
  mc.ctl = ctl;
  mc.flags |= fake ? MC_FLG_FALSE : 0;

#if BEAM_TEST
  /*
   * Under beam test we only want to send the SCIF message
   * This is guaranteed not to be called re-entrantly.
   */
  micras_mc_send(&mc, 1);
  return;
#endif

#ifdef CONFIG_MK1OM
  /*
   * If this is a true event then log it in the EEPROM and
   * notify SMC that we've had a serious machine check error.
   */
  if ((mc.flags & (MC_FLG_FALSE | MC_FLG_FATAL)) == MC_FLG_FATAL) {
    micras_mc_log(&mc);
    mc.flags |= MC_FLG_LOG;

    /*
     *TBD: Should this be deferred until the actual panic?
     *     The user can raise tolerance such that we in
     *     fact continue operating; in which case the SMC
     *     notification would be (somewhat) misleading.
     */
    micras_mc_ipmi(&mc, 1);
  }
#endif

  /*
   * Always notify host and sync to kernel log
   */
  if (micras_mc_send(&mc, 1))
    mcc_seen = mcelog.next;

#if RAS_HALT
  if ((mc.flags & MC_FLG_FATAL) && !fake)
    panic("FATAL core machine check event:\n"
      "bnk %d, id %d, ctl %llx, stat %llx, addr %llx, misc %llx\n",
      mc.org, mc.id, mc.ctl, mc.status, mc.addr, mc.misc);
#endif

  /*
   * Correctable events can in fact reach us here if
   * mce_no_way_out() tags them as critical (for other
   * reasons than the UC flag, e.g. MCIP missing).
   * If the tolerance setting is high enough to prevent
   * such events to panic, we'd still want filtering.
   */
  msk = micras_mc_filter(&mc, mce->tsc, 1);
  if (msk)
    mcc_ctl_mask(mce->bank, msk);
}


/*
 * In mce_panic().
 * Current event is about to make the kernel panic.
 * Sources of this call are
 *  do_machine_check(), when no_way_out set
 *  mce_timed_out(), CPU rendez-vous failed
 *  mce_reign(), when severety high, a CPU hung, or no events
 */

static void
mcc_exc_panic(struct mce * mce, char * msg, char * exp, int fake)
{
  /*
   * Should host be notified in this case?
   * And if so, how should be presented, we might not
   * even have a mce record to show when this happens!
   * If an mce is passed, it has already been seen and
   * reported to the host by a call to mcc_exc_log().
   * If mce is NULL, then this _is_ an MC relatedi panic,
   * but we have no data fitting for a host notification.
   * Create a pseudo event and ship that?
   */
  ee_printk("RAS: panic %d, wall %lld, msg %s, exp %s, fake %d\n",
		mce->extcpu, mce->time, msg, exp, fake);
}


/*
 * A CPU is leaving do_machine_check().
 * We get this after the monarch has 'reigned' and
 * the response to the event has been completed.
 */

static void
mcc_exc_exit(struct mce * mce, int no_way_out, int worst, int entry, int order)
{
  unsigned int	cpu;
  int		eoi;

  cpu = smp_processor_id();

  /*
   * Assuming test_and_clear_bit() is atomic.
   */
  smp_rmb();
  eoi = cpumask_test_and_clear_cpu(cpu, &mcc_exc_mask);
  smp_wmb();
  if (eoi)
    ack_APIC_irq();
}


/*
 * Routine to scan the kernel's MC log.
 * Called when SCIF MC session has been created, to bring the host
 * side up to date with prior unreported MC events, such as events
 * occurring when MC session was not active (no peer was listening
 * on the host) and events occurring before RAS module was loaded.
 *
 * Notes:
 *  - This is always called in thread context.
 *  - There are no injection flags in the kernel
 *    MC log, i.e. no guarantee events are genuine.
 *  - The MC kernel log has been exported explicitly for this.
 *
 * On synchronization (or the lack thereof):
 * Effectively the mcelog holds a static array of mce's where the
 * 'finished' flag says whether mce content is valid or not. The
 * 'next' field is the index of the first element in the array that
 * has not been assigned for an MC event. It is incremented when a
 * new event is entered, and reset to zero on reads to /dev/mcelog.
 * The kernel's event log does not wrap, so it is safe to use it as
 * an indicator of how many events (finished or not) are in it.
 * The mcelog's next field is protected by RCU style mechanisms
 * in the kernel MCA handler (see arch/x86/kernel/cpu/mcheck/mce.c).
 * For obvious reasons it is not genuine RCU, e.g. access to 'next'
 * isn't within rcu_read_lock()/rcu_read_unlock() pair, just a clever
 * masking use of a lock in an RCU macro definition.
 * There is no RCU moving data around, the mce array does not move,
 * and the 'finished' flag is set after a wmb() on the mce contents
 * which means this routine will not clash with the MCE handler.
 * Collisions with memset() on reads from /dev/mcelog are prevented
 * by locking of mce_read_mutex.
 */

void
mcc_sync(void)
{
  struct mce_info	mc;
  unsigned		seen;

  if (mce_disabled)
    return;

#if 0
  /*
   * Can't do this until bootstrap scrubs MC banks on all cards.
   * It has been observed that MCA banks may _not_ be reset on card
   * reboot which means events picked up by the kernel before loading
   * the RAS module may have occured in a previous uOS run.
   * Should be OK post early Jan '12 (flash ver 262, HSD 4115351).
   */
  return;
#endif

  /*
   * Lock out kernel log access through /dev/mcelog
   */
  mutex_lock(&mce_read_mutex);

  /*
   * Start over if the log has been cleared cleared
   */
  if (mcc_seen > mcelog.next)
    mcc_seen = 0;

  for(seen = mcc_seen; seen < mcelog.next; seen++) {
    /*
     * Basic checks. Index, CPU & bank must be reasonable.
     */
    if (mcelog.entry[seen].finished) {
      if (mcelog.entry[seen].cpu >= NR_CPUS ||
          mcelog.entry[seen].bank >= 3) {
	printk("mcc_sync: entry %d contains garbage, cpu %d, bank %d\n",
			seen, mcelog.entry[seen].cpu, mcelog.entry[seen].bank);
	continue;
      }

      /*
       * Have good entry, can be UC, but it is 'old'.
       */
      mcc_conv(&mcelog.entry[seen], &mc);
      mc.ctl = 0;

#ifdef CONFIG_MK1OM
      /*
       * Log this event in the eeprom and notify
       * that we've had a serious machine check error.
       */
      if (mc.flags & MC_FLG_FATAL) {
	in_sync = 1;
        micras_mc_log(&mc);
	in_sync = 0;
        mc.flags |= MC_FLG_LOG;
        micras_mc_ipmi(&mc, 0);
      }
#endif

      /*
       * Notify host about this too
       */
      if (! micras_mc_send(&mc, 0))
        break;
    }
  }
  mcc_seen = mcelog.next;

  /*
   * Done, release lock
   */
  mutex_unlock(&mce_read_mutex);
}


/*
 * Setup excetion handlers by hooking into the
 * kernel's native MCA handler.
 */

int __init
mcc_init(void)
{
  if (mce_disabled) {
    printk("RAS.core: disabled\n");
  }
  else {
    mca_poll      = mcc_poll;
    mca_exc_flt   = mcc_exc_flt;
    mca_exc_entry = mcc_exc_entry;
    mca_exc_log   = mcc_exc_log;
    mca_exc_panic = mcc_exc_panic;
    mca_exc_exit  = mcc_exc_exit;
    mca_print     = 0;		/* For debug: ee_printk; */
    printk("RAS.core: init complete\n");
  }

  return 0;
}


/*
 * Cleanup for module unload.
 * Clear/restore hooks in the native MCA handler.
 */

int __exit
mcc_exit(void)
{
  mca_poll      = 0;
  mca_exc_flt   = 0;
  mca_exc_entry = 0;
  mca_exc_log   = 0;
  mca_exc_panic = 0;
  mca_exc_exit  = 0;
  mca_print     = 0;

  /*
   * Links from kernel's MCE handler cut,
   * wait for everybody in handler to leave.
   */
  while(atomic_read(&mce_entry))
    cpu_relax();

  printk("RAS.core: exit complete\n");
  return 0;
}

