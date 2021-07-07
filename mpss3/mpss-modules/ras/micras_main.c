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
 * RAS module driver
 *
 * Contains code to handle module install/deinstall
 * and handling proper registration(s) to SCIF, sysfs
 * pseudo file system, timer ticks, I2C driver and
 * other one-time tasks.
 */

#include <linux/types.h>
#include <linux/ctype.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/proc_fs.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>
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
#include <asm/mic/mic_knc/autobaseaddress.h>
#include <asm/mic/mic_knc/micsboxdefine.h>
#include <scif.h>
#include "micras.h"

#if MT_VERBOSE || MC_VERBOSE || PM_VERBOSE
/*
 * For making scif_epd_t non-opague
 */
#define _MIC_MICBASEDEFINE_REGISTERS_H_	1
#include <mic/micscif.h>
#endif

/*
** Lookup table to map API opcode into MT function.
**
** As we have to deal with both KnF and KnC, functions to
** retrieve information may be generic, in micras_common.c,
** or platform specific, in micras_kn{cf}.c.
** Code location is transparent to this table.
**
** Some MT functions can safely be called without
** serialization, e.g. if they are read-only or use
** atomics to get/set variables. The 'simple' flag tells
** which functions are safe to call without serialization.
** Other functions should be called thru micras_mt_call().
**
** See micras_api.h and micpm_api.h for function details.
*/

static struct fnc_tab fnc_map[] = {
  { 0,              0, 0, 0 },
  { MR_REQ_HWINF,   1, 0, mr_get_hwinf },
  { MR_REQ_VERS,    1, 0, mr_get_vers },
  { MR_REQ_CFREQ,   0, 0, mr_get_freq },
  { MR_SET_CFREQ,   0, 1, mr_set_freq },
  { MR_REQ_CVOLT,   0, 0, mr_get_volt },
  { MR_SET_CVOLT,   0, 1, mr_set_volt },
  { MR_REQ_PWR,     0, 0, mr_get_power },
  { MR_REQ_PLIM,    0, 0, mr_get_plim },
  { MR_SET_PLIM,    0, 1, mr_set_plim },
  { MR_REQ_CLST,    0, 0, mr_get_clst },
  { MR_ENB_CORE,    0, 1, 0 },
  { MR_DIS_CORE,    0, 1, 0 },
  { MR_REQ_GDDR,    1, 0, mr_get_gddr },
  { MR_REQ_GFREQ,   1, 0, mr_get_gfreq },
  { MR_SET_GFREQ,   1, 1, 0 },
  { MR_REQ_GVOLT,   1, 0, mr_get_gvolt },
  { MR_SET_GVOLT,   1, 1, 0 },
  { MR_REQ_TEMP,    0, 0, mr_get_temp },
  { MR_REQ_FAN,     0, 0, mr_get_fan },
  { MR_SET_FAN,     0, 1, mr_set_fan },
  { MR_REQ_ECC,     1, 0, mr_get_ecc },
  { MR_SET_ECC,     0, 1, 0 },
  { MR_REQ_TRC,     1, 0, mr_get_trc },
  { MR_SET_TRC,     1, 1, mr_set_trc },
  { MR_REQ_TRBO,    0, 0, mr_get_trbo },
  { MR_SET_TRBO,    0, 1, mr_set_trbo },
  { MR_REQ_OCLK,    0, 0, 0 },
  { MR_SET_OCLK,    0, 1, 0 },
  { MR_REQ_CUTL,    0, 0, mr_get_cutl },
  { MR_REQ_MEM,     0, 0, mr_get_mem },
  { MR_REQ_OS,      0, 0, mr_get_os },
  { MR_REQ_PROC,    0, 0, mr_get_proc },
  { MR_REQ_THRD,    0, 0, 0 },
  { MR_REQ_PVER,    1, 0, mr_get_pver },
  { MR_CMD_PKILL,   0, 1, mr_cmd_pkill },
  { MR_CMD_UKILL,   0, 1, mr_cmd_ukill },
#if defined(CONFIG_MK1OM)
  { MR_GET_SMC,     0, 0, mr_get_smc },
  { MR_SET_SMC,     0, 0, mr_set_smc },
#else
#if defined(CONFIG_ML1OM) && USE_FSC
  { MR_GET_SMC,     0, 0, mr_get_fsc },
  { MR_SET_SMC,     0, 1, mr_set_fsc },
#else
  { 0,              0, 0, 0 },
  { 0,              0, 0, 0 },
#endif
#endif
  { MR_REQ_PMCFG,   0, 0, mr_get_pmcfg },
#if defined(CONFIG_MK1OM)
  { MR_REQ_LED,     0, 0, mr_get_led },
  { MR_SET_LED,     0, 1, mr_set_led },
  { MR_REQ_PROCHOT, 0, 0, mr_get_prochot },
  { MR_SET_PROCHOT, 0, 1, mr_set_prochot },
  { MR_REQ_PWRALT,  0, 0, mr_get_pwralt },
  { MR_SET_PWRALT,  0, 1, mr_set_pwralt },
  { MR_REQ_PERST,   0, 0, mr_get_perst },
  { MR_SET_PERST,   0, 1, mr_set_perst },
  { MR_REQ_TTL,     0, 0, mr_get_ttl },
#else
  { 0,              0, 0, 0 },
  { 0,              0, 0, 0 },
  { 0,              0, 0, 0 },
  { 0,              0, 0, 0 },
  { 0,              0, 0, 0 },
  { 0,              0, 0, 0 },
  { 0,              0, 0, 0 },
  { 0,              0, 0, 0 },
  { 0,              0, 0, 0 },
#endif
#if defined(CONFIG_MK1OM) && USE_PM
  { 0,              0, 0, 0 },
  { PM_REQ_PL0,     1, 0, pm_get_pl0 },
  { PM_SET_PL0,     1, 1, pm_set_pl0 },
  { PM_REQ_PL1,     1, 0, pm_get_pl1 },
  { PM_SET_PL1,     1, 1, pm_set_pl1 },
  { PM_REQ_PAVG,    1, 0, pm_get_pavg },
  { PM_REQ_PTTL,    1, 0, pm_get_pttl },
  { PM_REQ_VOLT,    1, 0, pm_get_volt },
  { PM_REQ_TEMP,    1, 0, pm_get_temp },
  { PM_REQ_TACH,    1, 0, pm_get_tach },
  { PM_REQ_TTTL,    1, 0, pm_get_tttl },
  { PM_REQ_FTTL,    1, 0, pm_get_fttl },
  { PM_SET_FTTL,    1, 1, pm_set_fttl },
#endif
};



/*
**
** The monitoring thread.
** In fact this is a work_queue, that receive work items
** from several independent parties, such as SCIF, sysfs,
** out of band telemetry, PM and possibly timers.
**
** These parties pass a structure with information necessary
** for the call-out function called by the MT thread to operate.
** These structures must include the work item structure, such
** that the container_of() mechanism can be used to locate it.
**
** The MT thread does not by itself provide any feed-back on
** when a task was executed nor the results from it. Therefore
** if a feedback is requred, then the callout needs to provide
** their own methods, such as the wait queue used by function
** micras_mt_data() below.  Experiments has shown that it is not
** safe to place work item or the wait queue on a stack (no
** idea why, could be a bug).
**
*/

static int			 micras_stop;		/* Module shutdown */
static struct delayed_work	 micras_wq_init;	/* Setup work item */
static struct delayed_work	 micras_wq_tick;	/* Timer tick token */
static struct workqueue_struct * micras_wq;		/* Monitor thread */
       int			 micras_priv;		/* Call-out privileged */


typedef struct wq_task {
  int			req;		/* Request opcode */
  int			rtn;		/* Return value */
  int			priv;		/* Privileged */
  void		      * ptr;		/* Response buffer */
  int		     (* fnc)(void *);	/* Call out */
  struct work_struct    wrk;		/* Work item */
  wait_queue_head_t	wqh;		/* Wait queue header */
} WqTask;


#if defined(CONFIG_MK1OM) && WA_4845465
/*
 * SMC die temp update job.
 *
 * As per HSD #4845465 we push the die temperature
 * to the SMC instead of the usual reverse direction.
 * This has to happen at around 50 mSec intervals, which should
 * be possible with a work queue implementation. If that turns out
 * not to be reliable enough we may need a more direct approach.
 * During the experiment, we want to override the pushed temp.
 */

#define DIE_PROC	1		/* Enable die temp override */
#define SMC_PERIOD	50		/* SMC update interval, mSec */
#define JITTER_STATS	1		/* Enable jitter measurements */

static struct delayed_work	micras_wq_smc;	/* SMC update token */
static int			smc_4845465;	/* SMC push capable */
#if DIE_PROC
static int			die_override;	/* Temperature override */
#endif

static void
micras_mt_smc(struct work_struct *work)
{
  extern int		mr_smc_wr(uint8_t, uint32_t *);
  static uint64_t	n;
  uint32_t		tmp;
  uint32_t		ts2, mfs;

  if (! micras_stop) {
    /*
     * Re-arm for a callback in about 1 second.
     * There is no guarantee this will be more than approximate.
     */
    queue_delayed_work(micras_wq, &micras_wq_smc, msecs_to_jiffies(SMC_PERIOD));
  }

#if JITTER_STATS
  /*
   * Time the interval in order to get some
   * measurement on what jitter to expect.
   * Leave a log message once every minute.
   */
  {
    static uint64_t	d, t1, t2, s, hi, lo = ~0;

    t2 = rdtsc();
    if (n) {
      d = t2 - t1;
      s += d;
      if (d > hi)
	hi = d;
      if (d < lo)
	lo = d;
#if 1
      {
	/*
	 * Show jitter in buckets representing 5 mSec.
	 * The center (#20) represent +- 2.5 mSec from reference.
	 * It is assumed TSC running at 1.1 GHz here, if PM kicks
	 * in the mesurements may be way off because it manipulate
	 * the system clock and indirectly the jiffy counter.
	 * It is assumed TSC running at 1.1 GHz here.
	 */
	static uint64_t buckets[41];
	int		bkt;
	int64_t		err;

	err = ((d * 10) / 11) - (50 * 1000 * 1000);
	if (err < -(25 * 100 * 1000))
	  bkt = 19 + (err + (25 * 100 * 1000)) / (5 * 1000 * 1000);
	else
	if (err > (25 * 100 * 1000))
	  bkt = 21 + (err - (25 * 100 * 1000)) / (5 * 1000 * 1000);
	else
	  bkt = 20;
	if (bkt < 0)
	  bkt = 0;
	if (bkt > 40)
	  bkt = 40;
	buckets[bkt]++;
	if ((n % ((10 * 1000)/SMC_PERIOD)) == ((10 * 1000)/SMC_PERIOD) - 1) {
	  printk("smc_upd: dist");
	  for(bkt = 0; bkt < 41; bkt++) {
	    if (bkt == 20)
	      printk(" | %lld |", buckets[bkt]);
	    else
	      printk(" %lld", buckets[bkt]);
	  }
	  printk("\n");
	}
      }
#endif
      if ((n % ((60 * 1000)/SMC_PERIOD)) == ((60 * 1000)/SMC_PERIOD) - 1)
	printk("smc_upd: %lld, min %lld, max %lld, avg %lld\n", n, lo, hi, s / n);
    }
    t1 = t2;
  }
#endif	/* JITTER_STATS */

  /*
   * Send update to SMC to register 0x50.
   * The value to push at the SMC must have following content
   *
   *  Bits  9:0	Device Temperature
   *  		-> THERMAL_STATUS_2 bits 19:10
   *  Bit     10	Valid bit
   *			-> THERMAL_STATUS_2 bit 31
   *  Bits 20:11	Thermal Monitor Control value
   *			-> THERMAL_STATUS_2 bits 9:0
   *  Bits 30:21	Fan Thermal Control value
   *			-> MICROCONTROLLER_FAN_STATUS bits 17:8
   */

  n++;
  ts2 = mr_sbox_rl(0, SBOX_THERMAL_STATUS_2);
  mfs = mr_sbox_rl(0, SBOX_MICROCONTROLLER_FAN_STATUS);

#if DIE_PROC
  if (die_override)
    tmp = GET_BITS(9, 0, die_override);
  else
#endif
    tmp = PUT_BITS(9, 0, GET_BITS(19, 10, ts2));
  tmp |= PUT_BIT(10, GET_BIT(31, ts2)) |
	 PUT_BITS(20, 11, GET_BITS(9, 0, ts2)) |
	 PUT_BITS(30, 21, GET_BITS(17, 8, mfs));
  
  if (mr_smc_wr(0x50, &tmp))
    printk("smc_upd: %lld, tmp %d, SMC write failed\n", n, tmp);
}


#if DIE_PROC
/*
 * Test proc file to override die temperature push.
 * A value of 0 means no override, any other value is
 * pushed as if it was a 'device temperature'.
 */

static struct proc_dir_entry * die_pe;

/*
 * On writes: scan input line for single number.
 */

static ssize_t
die_write(struct file * file, const char __user * buff, size_t len, loff_t * off)
{
  char 	      * buf;
  char	      * ep, * cp;
  unsigned long	ull;
  int		err;

  /*
   * Get input line into kernel space
   */
  if (len > PAGE_SIZE -1)
    len = PAGE_SIZE -1;
  buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
  if (! buf)
    return -ENOMEM;
  if (copy_from_user(buf, buff, len)) {
    err = -EFAULT;
    goto wr_out;
  }
  buf[len] = '\0';
  cp = ep = (char *) buf;

  /*
   * Read a number in strtoul format 0.
   */
  while(isspace(*cp))
    cp++;
  ull = simple_strtoull(cp, &ep, 0);
  if (ep == cp || (*ep != '\0' && !isspace(*ep))) {
    printk("Invalid die temp given\n");
    err = -EINVAL;
    goto wr_out;
  }

  die_override = GET_BITS(9, 0, ull);
  printk("Die temp override set to %d C\n", die_override);

  /*
   * Swallow any trailing junk up to next newline
   */
  ep = strchr(buf, '\n');
  if (ep)
   cp = ep + 1;
  err = cp - buf;

wr_out:
  kfree(buf);
  return err;
}


/*
 * On reads: return string of current override temp.
 */

static ssize_t
die_read(struct file * file, char __user * buff, size_t count, loff_t *ppos)
{
  char buf[32];
  size_t len;

  len = snprintf(buf, sizeof(buf), "%d\n", die_override);
  return simple_read_from_buffer(buff, count, ppos, buf, len);
}


static const struct file_operations proc_die_operations = {
  .read           = die_read,
  .write	  = die_write,
  .llseek         = no_llseek,
};
#endif	/* DIE_PROC */
#endif	/* WA_4845465 */


/*
 * Timer tick job
 *
 * This is for periodic updates from the SMC,
 * which (with a little luck) can be avoided
 * at the cost of I2C communications during
 * actual CP queries.
 */

static void
micras_mt_tick(struct work_struct *work)
{
#if MT_TIMER
  static int	n;

  n++;
  if (! micras_stop) {
    /*
     * Re-arm for a callback in about 1 second.
     * There is no guarantee this will be more than approximate.
     */
    queue_delayed_work(micras_wq, &micras_wq_tick, msecs_to_jiffies(MT_PERIOD));
  }

  /*
   * Dump elog prints into the kernel log
   *TBD: debug tool, time-shifts messages, remove eventually.
   */
  {
    int msg_top, msg_id;
    char * buf;

    msg_id = atomic_read(&ee_seen);
    msg_top = atomic_read(&ee_msg);
    while(++msg_id <= msg_top) {
      buf = ee_buf + (msg_id % EE_BUF_COUNT) * EE_BUF_LINELEN;
      if (! *buf)
        break;
      printk("%s", buf);
      *buf = '\0';
      atomic_inc(&ee_seen);
    }
  }
#endif
}


/*
 * Handle SCIF & sysfs show/store requests
 *
 * By convention we know that the work item is member of
 * a larger struct, which can readily be found using the
 * container_of mechanism.
 *
 * Otherwise this routine just calls the function stored
 * in the larger struct's mt_data element, and on its
 * return wake up whoever is waiting for it's completion.
 */

static void
micras_mt_data(struct work_struct * work)
{
  struct wq_task      * wq;

  wq = container_of(work, struct wq_task, wrk);
  micras_priv = wq->priv;
  wq->rtn = wq->fnc(wq->ptr);
  micras_priv = 0;
  wake_up_all(& wq->wqh);
}


/*
 * Helper to pass jobs (work items) to the monitoring thread.
 *
 * As input it receives details on function to be called, one
 * argument to pass to that function, the opcode associated
 * with the function and a function return value. The latter
 * will be set to -MR_ERR_PEND, and we'll expect the callout
 * function to change it.
 *
 * The work item is the only piece of information passed to
 * the work queue callout, so we'll wrap it into a larger
 * structure along with the received details such that the
 * work queue can perform a function call on our behalf.
 */

static int
micras_mt_tsk(struct wq_task * wq)
{
  int			err;

#if MT_VERBOSE
  uint64_t		start, stop;
  start = rdtsc();
#endif

  /*
   * Create a work item for the RAS thread,
   * enqueue and wait for it's completion.
   *
   *TBD: Timeout length to be revisited
   */
  wq->rtn = -MR_ERR_PEND;
  INIT_WORK_ONSTACK(&wq->wrk, micras_mt_data);
  init_waitqueue_head(&wq->wqh);
  queue_work(micras_wq, &wq->wrk);
  err = wait_event_interruptible_timeout(wq->wqh,
  		wq->rtn != -MR_ERR_PEND, msecs_to_jiffies(1000));

  /*
   * Check for potential errors, which for now can only be
   * "interrupted" or "timeout". In both cases try cancel the work
   * item from MT thread. If cancel succeds (returns true) then
   * the work item was still "pending" and is now removed from the
   * work queue, i.e. it is safe to continue (with error).
   * Otherwise, the cancel operation will wait for the work item's
   * call-out function to finish, which kind of defies the purpose
   * of "interruptable". However, we cannot leave until it is certain
   * that it will not be accessed by the RAS thread.
   */
  if (err == -ERESTARTSYS || err == 0) {
    printk("MT tsk: interrupted or failure, err %d\n", err);
    printk("MT tsk: FAILED: cmd %d, rtn %d, fnc %p, ptr %p\n",
		wq->req, wq->rtn, wq->fnc, wq->ptr);

    err = cancel_work_sync(&wq->wrk);
    printk("MT tsk: work canceled (%d)\n", err);
  }

  /*
   * Completed, turn interrupts and timeouts into MR errors.
   */
  err = wq->rtn;
  if (err == -MR_ERR_PEND)
    err = -MR_ERR_NOVAL;

#if MT_VERBOSE
  stop = rdtsc();
  printk("MT tsk: cmd %d, err %d, time %llu\n", wq->req, err, stop - start);
#endif
  return err;
}


/*
 * Public interface to the MT functions
 * Caller responsible for passing a buffer large enough
 * to hold data for reads or writes (1 page will do,
 * but structs matching the commands are recommended).
 * Returned data are structs defined in micras.h
 */

int
micras_mt_call(uint16_t cmd, void * buf)
{
  struct wq_task	* wq;
  int			  err;

  if (micras_stop)
    return -MR_ERR_UNSUP;

  if (cmd > MR_REQ_MAX)
    return -MR_ERR_INVOP;

  err = -MR_ERR_UNSUP;
  if (fnc_map[cmd].fnc) {
    if (fnc_map[cmd].simple) {
      /*
       * Fast access, just call function
       */
      err = fnc_map[cmd].fnc(buf);
    }
    else {
      /*
       * Slow access, go through serializer.
       * We allocate a work queue task for the MT thread,
       * stuff arguments in it, run task, and then free
       * work queue task.
       */
      wq = kmalloc(sizeof(* wq), GFP_KERNEL);
      if (! wq) {
	printk("Scif: CP work task alloc failed\n");
	return -MR_ERR_NOMEM;
      }

      memset(wq, '\0', sizeof(*wq));
      wq->req = cmd;
      wq->priv = 1;
      wq->fnc = (int (*)(void *)) fnc_map[cmd].fnc;
      wq->ptr = buf;
      err = micras_mt_tsk(wq);

      kfree(wq);
    }
  }

  return err;
}
EXPORT_SYMBOL_GPL(micras_mt_call);



/*
**
** The sysfs nodes provided by this module is not really associated
** with a 'struct device', since we don't create device entries for
** access through '/dev'. Instead we register a 'struct class'
** with nodes defined with the CLASS_ATTR macro.
** Reasons for this choice are:
**   - we don't want a device node created
**   - we don't need (at least now) to create udev events
**   - we don't act on suspend/resume transitions
**   - we don't want to have our files unnecessarily deep
**     in the sysfs file system.
**
** The sysfs layout is intended to look like:
**
** /sys/class/micras/		Root of this driver
**		    /clst	Core information
**		    /cutl	Core utilization
**		    /ecc	Error correction mode
**		    /fan	Fan controller
**		    /freq	Core frequency
**		    /gddr	GDDR devices
**		    /gfreq	GDDR speed
**		    /gvolt	GDDR voltage
**		    /hwinf	Hardware Info
**		    /mem	Memory utilization
**		    /os		OS status
**		    /plim	Power envelope
**		    /power	Card power
**		    /temp	Board tempearatures
**		    /trbo	Turbo mode
**		    /trc	Trace level
**		    /vers	uOS/Flash version
**		    /volt	Core voltage
**
** The following should be removed as there are better tools
** available in /proc/<pid>/{stat|status|smap}, /proc/meminfo,
** /proc/stat, /proc/uptime, /proc/loadavg, and /proc/cpuinfo:
**    clst, cutl, mem, os
**
** Below we hand-craft a 'micras' class to sit under '/sys/class'
** with attribute nodes directly under it. Each attribute may
** have a 'show' and a 'store' handler, both called with a reference
** to its class (ras_class, may hold private data), it's class_attribute,
** a buffer reference, and for 'store's a string length. The buffer
** passed to 'show' is one page (PAGE_SIZE, 4096) which sets the
** upper limit on the return string(s). Return value of 'store'
** has to be either an error code (negative) or the count of bytes
** consumed. If consumed less than what's passed in, the store routine
** will be called again until all input data has been consumed.
**
** Function pointers are hardwired by the macros below since it
** is easy and simpler than using the fnc_map table. This may
** change if the command set expands uncontrolled.
** We have local helper funtions to handle array prints.
** Any locking required is handled in called routines, not here.
**
** Note: This is not coded for maximum performance, since the
**       use of the MT thread to serialize access to card data
**	 has a cost of two task switches attached, both which
**       may cause delays due to other system activity.
**
*/


/*
 * Hack alert!
 * Formatting routines for arrays of 16/32/64 bit unsigned ints.
 * This reduces the printf argument list in _SHOW() macros below
 * considerably, though perhaps at a cost in code efficiency.
 * They need a scratch buffer in order to construct long lines.
 * A quick swag at the largest possible response tells that we'll
 * never exceed half if the page we are given to scribble into.
 * So, instead of allocating print space, we'll simply use 2nd
 * half of the page as scratch buffer.
 */

#define BP	(buf + (PAGE_SIZE/2))		/* Scratch pad location */
#define BL	(PAGE_SIZE/2 - 1)		/* Scratch size */


static char *
arr16(int16_t * arr, int len, char * buf, int siz)
{
  int		n, bs;

  bs = 0;
  for(n = 0; n < len && bs < siz; n++)
    bs += scnprintf(buf + bs, siz - bs, "%s%u", n ? " " : "", arr[n]);
  buf[bs] = '\0';

  return buf;
}


static char *
arr32(uint32_t * arr, int len, char * buf, int siz)
{
  int		n, bs;

  bs = 0;
  for(n = 0; n < len && bs < siz; n++)
    bs += scnprintf(buf + bs, siz - bs, "%s%u", n ? " " : "", arr[n]);
  buf[bs] = '\0';

  return buf;
}


static char *
arr64(uint64_t * arr, int len, char * buf, int siz)
{
  int		n, bs;

  bs = 0;
  for(n = 0; n < len && bs < siz; n++)
    bs += scnprintf(buf + bs, siz - bs, "%s%llu", n ? " " : "", arr[n]);
  buf[bs] = '\0';

  return buf;
}


#define _SHOW(op,rec,nam,str...) \
  static ssize_t \
  micras_show_##nam(struct class *class, \
  		    struct class_attribute *attr, \
		    char *buf) \
  { \
    struct mr_rsp_##rec	* r; \
    struct wq_task	* wq; \
    int			  len; \
    int			  err; \
\
    wq = kmalloc(sizeof(* wq) + sizeof(* r), GFP_KERNEL); \
    if (! wq) \
      return -ENOMEM; \
\
    memset(wq, '\0', sizeof(* wq)); \
    r = (struct mr_rsp_##rec *)(wq + 1); \
    wq->req = MR_REQ_##op; \
    wq->fnc = (int (*)(void *)) mr_get_##nam; \
    wq->ptr = r; \
    err = micras_mt_tsk(wq); \
\
    if (err < 0) { \
      len = 0; \
      *buf = '\0'; \
    } \
    else { \
      len = scnprintf(buf, PAGE_SIZE, ##str); \
    } \
\
    kfree(wq); \
    return len; \
  }

_SHOW(HWINF, hwinf, hwinf, "%u %u %u %u %u %u "
		"%c%c%c%c%c%c%c%c%c%c%c%c "
		"%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
		r->rev, r->step, r->substep, r->board, r->fab, r->sku,
		r->serial[0], r->serial[1], r->serial[2], r->serial[3],
		r->serial[4], r->serial[5], r->serial[6], r->serial[7],
		r->serial[8], r->serial[9], r->serial[10], r->serial[11],
		r->guid[0], r->guid[1], r->guid[2], r->guid[3],
		r->guid[4], r->guid[5], r->guid[6], r->guid[7],
		r->guid[8], r->guid[9], r->guid[10], r->guid[11],
		r->guid[12], r->guid[13], r->guid[14], r->guid[15]);

_SHOW(VERS, vers, vers, "\"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\"\n",
		r->fboot0 +1, r->fboot1 +1, r->flash[0] +1,
		r->flash[1] +1, r->flash[2] +1, r->fsc +1, r->uos +1)

_SHOW(CFREQ, freq, freq, "%u %u %s\n",
		r->cur, r->def, arr32(r->supt, r->slen, BP, BL))

_SHOW(CVOLT, volt, volt, "%u %u %s\n",
		r->cur, r->set, arr32(r->supt, r->slen, BP, BL))

#if defined(CONFIG_MK1OM) || (defined(CONFIG_ML1OM) && USE_FSC)
_SHOW(PWR, power, power, "%d\n%d\n%d\n%d\n%d\n%d\n%d\n%s\n%s\n%s\n",
		r->tot0.prr,
		r->tot1.prr,
		r->inst.prr,
		r->imax.prr,
		r->pcie.prr,
		r->c2x3.prr,
		r->c2x4.prr,
		arr32(&r->vccp.pwr,  3, BP, 32),
		arr32(&r->vddg.pwr,  3, BP + 32, 32),
		arr32(&r->vddq.pwr,  3, BP + 64, 32))

_SHOW(PLIM, plim, plim, "%u %u %u\n",
		r->phys, r->hmrk, r->lmrk)
#endif

_SHOW(CLST, clst, clst, "%u %u\n",
		r->count, r->thr)

_SHOW(GDDR, gddr, gddr, "\"%s\" %u %u %u\n",
		r->dev +1, r->rev, r->size, r->speed)

_SHOW(GFREQ, gfreq, gfreq, "%u %u\n",
		r->cur, r->def)

_SHOW(GVOLT, gvolt, gvolt, "%u %u\n",
		r->cur, r->set)

_SHOW(TEMP, temp, temp, "%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n",
		arr16(&r->die.cur,   2, BP, 32),
		arr16(&r->brd.cur,   2, BP + 32, 32),
		arr16(&r->fin.cur,   2, BP + 64, 32),
		arr16(&r->fout.cur,  2, BP + 96, 32),
		arr16(&r->gddr.cur,  2, BP + 128, 32),
		arr16(&r->vccp.cur,  2, BP + 160, 32),
		arr16(&r->vddg.cur,  2, BP + 224, 32),
		arr16(&r->vddq.cur,  2, BP + 256, 32))

_SHOW(FAN, fan, fan, "%u %u %u\n",
		r->override, r->pwm, r->rpm)

#ifdef CONFIG_MK1OM
_SHOW(ECC, ecc, ecc, "%d\n",
		r->enable)
#endif

_SHOW(TRC, trc, trc, "%d\n",
		r->lvl)

_SHOW(TRBO, trbo, trbo, "%d %d %d\n",
		r->set, r->state, r->avail)

#ifdef CONFIG_MK1OM
_SHOW(LED, led, led, "%d\n",
		r->led)

_SHOW(PROCHOT, ptrig, prochot, "%d %d\n",
  		r->power, r->time);

_SHOW(PWRALT, ptrig, pwralt, "%d %d\n",
  		r->power, r->time);

_SHOW(PERST, perst, perst, "%d\n",
  		r->perst);

_SHOW(TTL, ttl, ttl, "%u %u %u %u\n%u %u %u %u\n%u %u %u %u\n",
  		r->thermal.active, r->thermal.since, r->thermal.count, r->thermal.time,
  		r->power.active,   r->power.since,   r->power.count,   r->power.time,
  		r->alert.active,   r->alert.since,   r->alert.count,   r->alert.time);
#endif

_SHOW(CUTL, cutl, cutl, "%u %u %u %llu\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n...\n",
		r->tck, r->core, r->thr, r->jif,
		arr64(&r->sum.user,    4, BP, 80),
		arr64(&r->cpu[0].user, 4, BP + 80, 80),
		arr64(&r->cpu[1].user, 4, BP + 160, 80),
		arr64(&r->cpu[2].user, 4, BP + 240, 80),
		arr64(&r->cpu[3].user, 4, BP + 320, 80),
		arr64(&r->cpu[4].user, 4, BP + 400, 80),
		arr64(&r->cpu[5].user, 4, BP + 480, 80),
		arr64(&r->cpu[6].user, 4, BP + 560, 80),
		arr64(&r->cpu[7].user, 4, BP + 640, 80))

_SHOW(MEM, mem, mem, "%u %u %u\n",
		r->total, r->free, r->bufs)

_SHOW(OS, os, os, "%llu %llu %llu %llu %u [%s]\n",
		r->uptime, r->loads[0], r->loads[1], r->loads[2],
		r->alen, arr32(r->apid, r->alen, BP, BL))


/*
 * Ensure caller's creditials is root on all 'set' files.
 * Even though file creation mode should prevent writes?
 *
 *TBD:
 * - How many of the 'store's are to be permitted?
 */

#define _STORE(op, nam) \
  static ssize_t \
  micras_store_##nam (struct class *class, \
  		     struct class_attribute *attr, \
		     const char *buf, \
		     size_t count) \
  { \
    struct wq_task    * wq; \
    size_t		ocount; \
    uint32_t		val; \
    int			err; \
    char	      * ep; \
\
    if (current_euid() != 0) \
      return -EPERM; \
\
    ocount = count; \
    if (count && buf[count - 1] == '\n') \
      ((char *) buf)[--count] = '\0'; \
\
    err = -EINVAL; \
    if (count && *buf) { \
      val = simple_strtoul(buf, &ep, 0); \
      if (ep != buf && !*ep) { \
        wq = kmalloc(sizeof(* wq), GFP_KERNEL); \
        if (! wq) \
          return -ENOMEM; \
\
        wq->req = MR_SET_##op; \
        wq->fnc = (int (*)(void *)) mr_set_##nam; \
        wq->ptr = (void *) &val; \
        if (! micras_mt_tsk(wq)) \
	  err = ocount; \
        kfree(wq); \
      } \
    } \
\
    return err; \
  }

_STORE(CFREQ, freq)
_STORE(CVOLT, volt)

#if defined(CONFIG_MK1OM) || (defined(CONFIG_ML1OM) && USE_FSC)
_STORE(PLIM, plim)
#endif

_STORE(FAN, fan)
_STORE(TRC, trc)
_STORE(TRBO, trbo)

#ifdef CONFIG_MK1OM
_STORE(LED, led)
_STORE(PERST, perst)
#endif


/*
 *TBD:
 * - Remove entries clst, cutl, mem, and os.
 *   Only included here for comparison with what cp/micinfo displays.
 *   They really need to go.
 */

static struct class_attribute micras_attr[] = {
  __ATTR(hwinf,   0444, micras_show_hwinf,  0),
  __ATTR(vers,    0444, micras_show_vers,   0),
  __ATTR(freq,    0644, micras_show_freq,   micras_store_freq),
  __ATTR(volt,    0644, micras_show_volt,   micras_store_volt),
#if defined(CONFIG_MK1OM) || (defined(CONFIG_ML1OM) && USE_FSC)
  __ATTR(power,   0444, micras_show_power,  0),
  __ATTR(plim,    0644, micras_show_plim,   micras_store_plim),
#endif
  __ATTR(clst,    0444, micras_show_clst,   0),
  __ATTR(gddr,    0444, micras_show_gddr,   0),
  __ATTR(gfreq,   0444, micras_show_gfreq,  0),
  __ATTR(gvolt,   0444, micras_show_gvolt,  0),
  __ATTR(fan,     0644, micras_show_fan,    micras_store_fan),
  __ATTR(temp,    0444, micras_show_temp,   0),
#ifdef CONFIG_MK1OM
  __ATTR(ecc,     0444, micras_show_ecc,    0),
#endif
  __ATTR(trc,     0644, micras_show_trc,    micras_store_trc),
  __ATTR(trbo,    0644, micras_show_trbo,   micras_store_trbo),
#ifdef CONFIG_MK1OM
  __ATTR(led,	  0644, micras_show_led,    micras_store_led),
  __ATTR(prochot, 0444, micras_show_prochot, 0),
  __ATTR(pwralt,  0444, micras_show_pwralt, 0),
  __ATTR(perst,	  0644, micras_show_perst,  micras_store_perst),
  __ATTR(ttl,	  0444, micras_show_ttl,    0),
#endif
  __ATTR(cutl,    0444, micras_show_cutl,   0),
  __ATTR(mem,     0444, micras_show_mem,    0),
  __ATTR(os,      0444, micras_show_os,     0),
  __ATTR_NULL,
};


static struct class ras_class = {
  .name           = "micras",
  .owner	  = THIS_MODULE,
  .class_attrs    = micras_attr,
};



/*
**
** SCIF interface & services are mostly handled here, including
** all aspects of setting up and tearing down SCIF channels.
** We create three listening SCIF sockets and create a workqueue
** with the initial task of waiting for 'accept's to happen.
**
** When TTL or MC accept incoming connections, their workqueue
** task spawns one thread just to detect if/when peer closes
** the session and will block any further connects until thes
** service thread terminates (peer closes session).
** The TTL or MC event handler, executing in interrupt context,
** will check for an open session and if one is present, deliver
** their event record(s) on it by using scif_send().
**
** When CP accept incoming connections, its workqueue task spawns
** a new thread to run a session with the peer and then proceeds
** to accepting a new connection. Thus, there are no strict
** bounds on number of incoming connections, but for internal
** house-keeping sessions are limited to MR_SCIF_MAX (32).
** Accepted requests from the peer are fulfilled through the
** MT thread in a similar fashion as the sysctl interface, i.e.
** though function micras_mt_tsk(), who guarantee synchronized
** (serialized) access to MT core data and handle waits as needed.
** Function pointers corresponding to request opcodes are found
** by lookup in the fnc_map table.
**
** Note: This is not coded for maximum performance, since the
**       use of the MT thread to serialize access to card data
**	 has a cost of two task switches attached, both which
**       may cause delays due to other system activity.
*/


static scif_epd_t		     micras_cp_lstn;	/* CP listener handle */
static struct workqueue_struct *     micras_cp_wq;	/* CP listener thread */
static atomic_t			     micras_cp_rst;	/* CP listener restart */
static struct delayed_work	     micras_cp_tkn;	/* CP accept token */
static DECLARE_BITMAP(micras_cp_fd, MR_SCIF_MAX);	/* CP free slots */
static volatile struct scif_portID   micras_cp_si[MR_SCIF_MAX];	/* CP sessions */
static volatile struct task_struct * micras_cp_kt[MR_SCIF_MAX];	/* CP threads */
static volatile scif_epd_t 	     micras_cp_ep[MR_SCIF_MAX];	/* CP handles */

static scif_epd_t		     micras_mc_lstn;	/* MC listener handle */
static struct workqueue_struct     * micras_mc_wq;	/* MC listener thread */
static struct delayed_work	     micras_mc_tkn;	/* MC accept token */
static volatile struct task_struct * micras_mc_kt;	/* MC session */
static volatile	scif_epd_t 	     micras_mc_ep;	/* MC handle */

static scif_epd_t		     micras_ttl_lstn;	/* TTL listener handle */
static struct workqueue_struct     * micras_ttl_wq;	/* TTL listener thread */
static struct delayed_work	     micras_ttl_tkn;	/* TTL accept token */
static volatile struct task_struct * micras_ttl_kt;	/* TTL session */
static volatile	scif_epd_t 	     micras_ttl_ep;	/* TTL handle */


/*
 * SCIF CP session thread
 */

static int
micras_cp_sess(void * _slot)
{
  struct wq_task      * wq;
  struct mr_hdr		q, a;
  scif_epd_t		ep;
  uint32_t		slot;
  void		      * buf;
  uint64_t		start, stop;
  int			blen, len, priv;

  slot = (uint32_t)((uint64_t) _slot);
  priv = (micras_cp_si[slot].port < 1024) ? 1 : 0;
#if MT_VERBOSE
  printk("Scif: CP session %d running%s\n", slot, priv ? " privileged" : "");
#endif

  /*
   * Allocate local buffer from kernel
   * Since the I/O buffers in SCIF is just one page,
   * we'd never expect to need larger buffers here.
   */
  buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
  if (! buf) {
    printk("Scif: CP scratch pad alloc failed\n");
    return 0;
  }

  /*
   * Allocate a work queue task for the MT thread
   */
  wq = kmalloc(sizeof(* wq), GFP_KERNEL);
  if (! wq) {
    printk("Scif: CP work task alloc failed\n");
    goto cp_sess_end;
  }

  /*
   * Start servicing MT protocol
   */
  ep = micras_cp_ep[slot];
  for( ;; ) {

    /*
     * Get a message header
     */
    len = scif_recv(ep, &q, sizeof(q), SCIF_RECV_BLOCK);
    start = rdtsc();
    if (len < 0) {
      if (len != -ECONNRESET)
        printk("Scif: CP recv error %d\n", len);
      goto cp_sess_end;
    }
    if (len != sizeof(q)) {
      printk("Scif: CP short recv (%d), discarding\n", len);
      continue;
    }

    /*
     * Validate the query:
     *  - known good opcode,
     *  - expected length (zero)
     *  - have callout in jump table
     *  - check requestor's port ID on privileged opcodes.
     *
     *TBD: opcodes above MR_REQ_MAX is really only meant for
     *     use by the PM module. Should it be host accessible?
     */
    blen = 0;
    if (q.cmd < MR_REQ_HWINF ||
#if defined(CONFIG_MK1OM) && USE_PM
	q.cmd > PM_REQ_MAX
#else
	q.cmd > MR_REQ_MAX
#endif
       ) {
      printk("Scif: CP opcode %d invalid\n", q.cmd);
      blen = -MR_ERR_INVOP;
    }
    else
    if (q.len != 0) {
      printk("Scif: CP command length %d invalid\n", q.len);
      blen = -MR_ERR_INVLEN;
    }
    else
    if (! fnc_map[q.cmd].fnc) {
      printk("Scif: CP opcode %d un-implemented\n", q.cmd);
      blen = -MR_ERR_UNSUP;
    }
    else
    if (fnc_map[q.cmd].privileged && !priv) {
      printk("Scif: CP opcode %d privileged, remote %d:%d\n",
		q.cmd, micras_cp_si[slot].node, micras_cp_si[slot].port);
      blen = -MR_ERR_PERM;
    }

    /*
     *TBD: If there is an error at this point, it might
     *     be a good idea to drain the SCIF channel.
     *     If garbage has entered the channel somehow,
     *     then how else can we get in sync such that
     *     next recv really is a command header?
     *     More radical solution is closing this session.
     */

    /*
     * If header is OK (blen still zero) then pass
     * a work queue item to MT and wait for response.
     * The result will end up in buf (payload for response)
     * or an error code that can be sent back to requestor.
     * Since we don't want to care about whether it is a
     * get or set command here, the 'parm' value is copied
     * into buf prior to passing the work item to MT.
     * Thus, functions expecting an 'uint32_t *' to
     * point to a new value will be satisfied.
     */
    if (blen == 0) {
      if (fnc_map[q.cmd].simple) {
        *((uint32_t *) buf) = q.parm;
        blen = fnc_map[q.cmd].fnc(buf);
      }
      else {
        memset(wq, '\0', sizeof(*wq));
        wq->req = q.cmd;
	wq->priv = priv;
        wq->fnc = (int (*)(void *)) fnc_map[q.cmd].fnc;
        wq->ptr = buf;
        *((uint32_t *) buf) = q.parm;
        blen = micras_mt_tsk(wq);
      }
    }
    stop = rdtsc();

    /*
     * Craft response header
     */
    a.cmd = q.cmd | MR_RESP;
    if (blen < 0) {
      /*
       * MT thread reported a failure.
       * Set error bit and make error record in buf
       */
      a.cmd |= MR_ERROR;
      ((struct mr_err *) buf)->err = -blen;
      ((struct mr_err *) buf)->len = 0;
      a.len = sizeof(struct mr_err);
    }
    else {
      /*
       * Payload size is set by call-out
       */
      a.len = blen;
    }
    a.stamp = q.stamp;
    a.spent = stop - start;

    /*
     * Send response header (always)
     */
    len = scif_send(ep, &a, sizeof(a), SCIF_SEND_BLOCK);
    if (len < 0) {
      printk("Scif: header send error %d\n", len);
      goto cp_sess_end;
    }
    if (len != sizeof(a)) {
      printk("Scif: CP short header send (%d of %lu)\n", len, sizeof(a));
      goto cp_sess_end;
    }

    /*
     * Send payload (if any, defined by a.len)
     */
    if (a.len > 0) {
      len = scif_send(ep, buf, a.len, SCIF_SEND_BLOCK);
      if (len < 0) {
        printk("Scif: CP payload send error %d\n", len);
        goto cp_sess_end;
      }
      if (len != a.len) {
        printk("Scif: CP short payload send (%d of %d)\n", len, a.len);
        goto cp_sess_end;
      }
    }

  }

cp_sess_end:
  if (wq)
    kfree(wq);
  if (buf)
    kfree(buf);
  ep = (scif_epd_t) atomic64_xchg((atomic64_t *)(micras_cp_ep + slot), 0);
  if (ep)
    scif_close(ep);
  micras_cp_kt[slot] = 0;
  set_bit(slot, micras_cp_fd);
#if MT_VERBOSE
  printk("Scif: CP session %d terminated, sess mask %lx\n", slot, micras_cp_fd[0]);
#endif

  if (atomic_xchg(&micras_cp_rst, 0)) {
    printk("Scif: resume listener\n");
    queue_delayed_work(micras_cp_wq, &micras_cp_tkn, 0);
  }

  return 0;
}


/*
 * SCIF CP session launcher
 */

static void
micras_cp(struct work_struct * work)
{
  struct task_struct  * thr;
  scif_epd_t		sess_ep;
  struct scif_portID	sess_id;
  int			slot;
  int			err;

  /*
   * Wait for somebody to connect to us
   * We stop listening on any error whatsoever
   */
  err = scif_accept(micras_cp_lstn, &sess_id, &sess_ep, SCIF_ACCEPT_SYNC);
  if (err == -EINTR) {
    printk("Scif: CP accept interrupted, error %d\n", err);
    return;
  }
  if (err < 0) {
    printk("Scif: CP accept failed, error %d\n", err);
    return;
  }
#if MT_VERBOSE
  printk("Scif: CP accept: remote %d:%d, local %d:%d\n",
  		sess_id.node, sess_id.port,
		micras_cp_lstn->port.node, micras_cp_lstn->port.port);
#endif

  /*
   * Spawn a new thread to run session with connecting peer
   * We support only a limited number of connections, so first
   * get a free "slot" for this session.
   * The use of non-atomic ffs() below is safe as long as this
   * function is never run by more than one thread at a time
   * and all other manipulations of micras_cp_fd are atomic.
   */
  slot = find_first_bit(micras_cp_fd, MR_SCIF_MAX);
  if (slot < MR_SCIF_MAX) {
    if (micras_cp_kt[slot] || micras_cp_ep[slot]) {
      printk("Scif: CP slot %d busy (bug)\n", slot);
      return;
    }

    clear_bit(slot, micras_cp_fd);
    micras_cp_ep[slot] = sess_ep;
    micras_cp_si[slot] = sess_id;
    thr = kthread_create(micras_cp_sess, (void *)(uint64_t) slot, "RAS CP svc %d", slot);
    if (IS_ERR(thr)) {
      printk("Scif: CP service thread creation failed\n");
      scif_close(sess_ep);
      micras_cp_ep[slot] = 0;
      set_bit(slot, micras_cp_fd);
      return;
    }
    micras_cp_kt[slot] = thr;
#if MT_VERBOSE
    printk("Scif: CP session %d launched, pid %d\n", slot, thr->pid);
#endif
    wake_up_process(thr);
  }
  else {
    printk("Scif: No open session slots, closing session\n");
    scif_close(sess_ep);
  }

  /*
   * Keep listening until session limit reached.
   */
  if (bitmap_weight(micras_cp_fd, MR_SCIF_MAX))
    queue_delayed_work(micras_cp_wq, &micras_cp_tkn, 0);
  else {
    printk("Scif: CP connection limit reached\n");
    atomic_xchg(&micras_cp_rst, 1);
  }
}


/*
 * SCIF MC session thread
 */

static int
micras_mc_sess(void * dummy)
{
  scif_epd_t		ep;
  char			buf[8];
  int			len;

#if MC_VERBOSE
  printk("Scif: MC session running\n");
#endif

  /*
   * Start servicing.
   * This is just to get indication if peer closes connection
   */
  for( ;; ) {
    /*
     * Sync with kernel MC event log.
     */
    mcc_sync();

    /*
     * Try read 1 byte from host (turns into a wait-point
     * keeping the connection open till host closes it)
     */
    len = scif_recv(micras_mc_ep, buf, 1, SCIF_RECV_BLOCK);
    if (len < 0) {
      if (len != -ECONNRESET)
        printk("Scif: MC recv error %d\n", len);
      goto mc_sess_end;
    }

    /*
     * Ignore any received content.
     */
  }

mc_sess_end:
  ep = (scif_epd_t) atomic64_xchg((atomic64_t *) &micras_mc_ep, 0);
  if (ep)
    scif_close(ep);
  micras_mc_kt = 0;
#if MC_VERBOSE
  printk("Scif: MC session terminated\n");
#endif
  return 0;
}


/*
 * SCIF MC session launcher
 */

static void
micras_mc(struct work_struct * work)
{
  struct task_struct  * thr;
  scif_epd_t		sess_ep;
  struct scif_portID	sess_id;
  int			err;

  /*
   * Wait for somebody to connect to us
   * We stop listening on any error whatsoever
   */
  err = scif_accept(micras_mc_lstn, &sess_id, &sess_ep, SCIF_ACCEPT_SYNC);
  if (err == -EINTR) {
    printk("Scif: MC accept interrupted, error %d\n", err);
    return;
  }
  if (err < 0) {
    printk("Scif: MC accept failed, error %d\n", err);
    return;
  }
#if MC_VERBOSE
  printk("Scif: MC accept: remote %d:%d, local %d:%d\n",
  		sess_ep->peer.node, sess_ep->peer.port,
		sess_ep->port.node, sess_ep->port.port);
#endif

  /*
   * Spawn a new thread to run session with connecting peer
   * We support only one connection, so if one already is
   * running this one will be rejected.
   */
  if (! micras_mc_ep) {
    micras_mc_ep = sess_ep;
    thr = kthread_create(micras_mc_sess, 0, "RAS MC svc");
    if (IS_ERR(thr)) {
      printk("Scif: MC service thread creation failed\n");
      scif_close(sess_ep);
      micras_mc_ep = 0;
      return;
    }
    micras_mc_kt = thr;
    wake_up_process(thr);
  }
  else {
    printk("Scif: MC connection limit reached\n");
    scif_close(sess_ep);
  }

  /*
   * Keep listening
   */
  queue_delayed_work(micras_mc_wq, &micras_mc_tkn, 0);
}


/*
 * Ship a pre-packaged machine check event record to host
 */

#ifndef SCIF_BLAST
#define SCIF_BLAST	2
#endif

int
micras_mc_send(struct mce_info * mce, int exc)
{
  if (micras_mc_ep) {
    int		err;

#if ADD_DIE_TEMP
    err = mr_sbox_rl(0, SBOX_THERMAL_STATUS_2);
    mce->flags |= PUT_BITS(15, 8, GET_BITS(19, 10, err));
#endif

    if (exc) {
      /*
       * Exception context SCIF access, can't sleep and can't
       * wait on spinlocks either. May be detrimental to
       * other scif communications, but this _is_ an emergency
       * and we _do_ need to ship this message to the host.
       */
      err = scif_send(micras_mc_ep, mce, sizeof(*mce), SCIF_BLAST);
      if (err != sizeof(*mce))
        ee_printk("micras_mc_send: scif_send failed, err %d\n", err);
    }
    else {
      /*
       * Thread context SCIF access.
       * Just send message.
       */
      err = scif_send(micras_mc_ep, mce, sizeof(*mce), SCIF_SEND_BLOCK);
      if (err != sizeof(*mce))
        printk("micras_mc_send: scif_send failed, err %d\n", err);
    }
    return err == sizeof(*mce);
  }
  return 0;
}


/*
 * SCIF TTL session thread
 */

static int
micras_ttl_sess(void * dummy)
{
  scif_epd_t		ep;
  char			buf[8];
  int			len;

#if PM_VERBOSE
  printk("Scif: TTL session running\n");
#endif

  /*
   * Start servicing.
   * This is just to get indication if peer closes connection
   */
  for( ;; ) {
    /*
     * Try read 1 byte from host (turns into a wait-point
     * keeping the connection open till host closes it)
     */
    len = scif_recv(micras_ttl_ep, buf, 1, SCIF_RECV_BLOCK);
    if (len < 0) {
      if (len != -ECONNRESET)
        printk("Scif: TTL recv error %d\n", len);
      goto ttl_sess_end;
    }

    /*
     * Ignore any received content.
     */
  }

ttl_sess_end:
  ep = (scif_epd_t) atomic64_xchg((atomic64_t *) &micras_ttl_ep, 0);
  if (ep)
    scif_close(ep);
  micras_ttl_kt = 0;
#if PM_VERBOSE
  printk("Scif: TTL session terminated\n");
#endif
  return 0;
}


/*
 * SCIF TTL session launcher
 */

static void
micras_ttl(struct work_struct * work)
{
  struct task_struct  * thr;
  scif_epd_t		sess_ep;
  struct scif_portID	sess_id;
  int			err;

  /*
   * Wait for somebody to connect to us
   * We stop listening on any error whatsoever
   */
  err = scif_accept(micras_ttl_lstn, &sess_id, &sess_ep, SCIF_ACCEPT_SYNC);
  if (err == -EINTR) {
    printk("Scif: TTL accept interrupted, error %d\n", err);
    return;
  }
  if (err < 0) {
    printk("Scif: TTL accept failed, error %d\n", err);
    return;
  }
#if PM_VERBOSE
  printk("Scif: TTL accept: remote %d:%d, local %d:%d\n",
  		sess_ep->peer.node, sess_ep->peer.port,
		sess_ep->port.node, sess_ep->port.port);
#endif

  /*
   * Spawn a new thread to run session with connecting peer
   * We support only one connection, so if one already is
   * running this one will be rejected.
   */
  if (! micras_ttl_ep) {
    micras_ttl_ep = sess_ep;
    thr = kthread_create(micras_ttl_sess, 0, "RAS TTL svc");
    if (IS_ERR(thr)) {
      printk("Scif: TTL service thread creation failed\n");
      scif_close(sess_ep);
      micras_ttl_ep = 0;
      return;
    }
    micras_ttl_kt = thr;
    wake_up_process(thr);
  }
  else {
    printk("Scif: TTL connection limit reached\n");
    scif_close(sess_ep);
  }

  /*
   * Keep listening
   */
  queue_delayed_work(micras_ttl_wq, &micras_ttl_tkn, 0);
}


/*
 * Ship a pre-packaged throttle event record to host
 */

void
micras_ttl_send(struct ttl_info * ttl)
{
  static struct ttl_info split_rec;
  static int	 	 split_rem;
  int			err;
  char		      * cp;

  if (micras_ttl_ep) {

    if (split_rem) {
      cp = ((char *) &split_rec) + (sizeof(*ttl) - split_rem);
      err = scif_send(micras_ttl_ep, cp, split_rem, 0);
      if (err == split_rem) {
	/*
	 * Tx of pendig buffer complete
	 */
        split_rem = 0;
      }
      else {
        if (err < 0) {
          /*
	   * SCIF failed squarely, just drop the message.
	   * TBD: close end point?
	   */
        }
        else {
          /*
	   * Another partial send
	   */
	  split_rem -= err;
        }
      }
    }

    if (! split_rem) {
      /*
       * Send message
       */
      err = scif_send(micras_ttl_ep, ttl, sizeof(*ttl), 0);
      if (err != sizeof(*ttl)) {
        /*
	 * Did not send all the message
	 */
	if (err < 0) {
	  /*
	   * SCIF failed squarely, drop the message.
	   * TBD: close end point?
	   */
	}
	else {
	  split_rec = *ttl;
	  split_rem = sizeof(*ttl) - err;
        }
      }
    }
  }
}



/*
**
** MMIO regions used by RAS module
** Until some common strategy on access to BOXes and other CSRs
** we'll map them ourselves. All MMIO accesses are performed
** through 32 bit unsigned integers, but a 64 bit abstraction
** is provided for convenience (low 32 bit done first).
**
** We need access to the SBOX, all GBOXs, TBOXs and DBOXs.
**
** Note: I2C driver code for exception context in micras_elog.c
**       has its own set of I/O routines in order to allow
**       separate debugging.
**      
*/

uint8_t       * micras_sbox;		/* SBOX mmio region */
uint8_t       * micras_dbox[DBOX_NUM];	/* DBOX mmio region */
uint8_t       * micras_gbox[GBOX_NUM];	/* GBOX mmio regions */
#ifdef CONFIG_MK1OM
uint8_t       * micras_tbox[TBOX_NUM];	/* TBOX mmio regions */
#endif

/*
 * Specials: some defines are currently missing
 */

#ifdef CONFIG_MK1OM
#define	DBOX1_BASE		0x0800620000ULL

#define	GBOX4_BASE		0x08006D0000ULL
#define	GBOX5_BASE		0x08006C0000ULL
#define	GBOX6_BASE		0x08006B0000ULL
#define	GBOX7_BASE		0x08006A0000ULL
#endif


/*
 * MMIO I/O dumpers (for debug)
 * Exception mode code needs to use the ee_print dumpers
 * because printk is not safe to use (works most of the time
 * though, but may hang the system eventually).
 */
#if 0
#if 0
extern atomic_t	pxa_block;
#define RL	if (! atomic_read(&pxa_block)) ee_print("%s: %4x -> %08x\n",    __FUNCTION__, roff, val)
#define RQ	if (! atomic_read(&pxa_block)) ee_print("%s: %4x -> %016llx\n", __FUNCTION__, roff, val)
#define WL	if (! atomic_read(&pxa_block)) ee_print("%s: %4x <- %08x\n",    __FUNCTION__, roff, val)
#define WQ	if (! atomic_read(&pxa_block)) ee_print("%s: %4x <- %016llx\n", __FUNCTION__, roff, val)
#else
#define RL	printk("%s: %4x -> %08x\n",    __FUNCTION__, roff, val)
#define RQ	printk("%s: %4x -> %016llx\n", __FUNCTION__, roff, val)
#define WL	printk("%s: %4x <- %08x\n",    __FUNCTION__, roff, val)
#define WQ	printk("%s: %4x <- %016llx\n", __FUNCTION__, roff, val)
#endif
#else
#define RL	/* As nothing */
#define RQ	/* As nothing */
#define WL	/* As nothing */
#define WQ	/* As nothing */
#endif


/*
 * SBOX MMIO I/O routines
 *  mr_sbox_base	Return SBOX MMIO region
 *  mr_sbox_rl		Read 32-bit register
 *  mr_sbox_rq		Read 64-bit register (really two 32-bit reads)
 *  mr_sbox_wl		Write 32-bit register
 *  mr_sbox_wq		Write 64-bit register (really two 32-bit writes)
 */

#if NOT_YET
uint8_t *
mr_sbox_base(int dummy)
{
  return micras_sbox;
}
#endif

uint32_t
mr_sbox_rl(int dummy, uint32_t roff)
{
  uint32_t	val;

  val = * (volatile uint32_t *)(micras_sbox + roff);
  RL;
  return val;
}

uint64_t
mr_sbox_rq(int dummy, uint32_t roff)
{
  uint32_t	hi, lo;
  uint64_t	val;

  lo = * (volatile uint32_t *)(micras_sbox + roff);
  hi = * (volatile uint32_t *)(micras_sbox + roff + 4);
  val = ((uint64_t) hi << 32) | (uint64_t) lo;
  RQ;
  return val;
}

void
mr_sbox_wl(int dummy, uint32_t roff, uint32_t val)
{
  WL;
  * (volatile uint32_t *)(micras_sbox + roff) = val;
}

void
mr_sbox_wq(int dummy, uint32_t roff, uint64_t val)
{
  uint32_t	hi, lo;

  WQ;
  lo = val;
  hi = val >> 32;

  * (volatile uint32_t *)(micras_sbox + roff) = lo;
  * (volatile uint32_t *)(micras_sbox + roff + 4) = hi;
}


/*
 * DBOX MMIO I/O routines
 *  mr_dbox_base	Return DBOX MMIO region
 *  mr_dbox_rl		Read 32-bit register
 *  mr_dbox_rq		Read 64-bit register (really two 32-bit reads)
 *  mr_dbox_wl		Write 32-bit register
 *  mr_dbox_wq		Write 64-bit register (really two 32-bit writes)
 */

#if NOT_YET
uint8_t *
mr_dbox_base(int unit)
{
  return micras_dbox[unit];
}
#endif

uint32_t
mr_dbox_rl(int unit, uint32_t roff)
{
  uint32_t	val;

  val = * (volatile uint32_t *)(micras_dbox[unit] + roff);
  RL;
  return val;
}

uint64_t
mr_dbox_rq(int unit, uint32_t roff)
{
  uint32_t	hi, lo;
  uint64_t	val;

  lo = * (volatile uint32_t *)(micras_dbox[unit] + roff);
  hi = * (volatile uint32_t *)(micras_dbox[unit] + roff + 4);
  val = ((uint64_t) hi << 32) | (uint64_t) lo;
  RQ;
  return val;
}

void
mr_dbox_wl(int unit, uint32_t roff, uint32_t val)
{
  WL;
  * (volatile uint32_t *)(micras_dbox[unit] + roff) = val;
}

void
mr_dbox_wq(int unit, uint32_t roff, uint64_t val)
{
  uint32_t	hi, lo;

  WQ;
  lo = val;
  hi = val >> 32;

  * (volatile uint32_t *)(micras_dbox[unit] + roff) = lo;
  * (volatile uint32_t *)(micras_dbox[unit] + roff + 4) = hi;
}


/*
 * GBOX MMIO I/O routines
 *  mr_gbox_base	Return GBOX MMIO region
 *  mr_gbox_rl		Read 32-bit register
 *  mr_gbox_rq		Read 64-bit register (really two 32-bit reads)
 *  mr_gbox_wl		Write 32-bit register
 *  mr_gbox_wq		Write 64-bit register (really two 32-bit writes)
 *
 * Due to a Si bug, MMIO writes can be dropped by the GBOXs
 * during heavy DMA activity (HSD #4844222). The risk of it
 * happening is low enough that a 'repeat until it sticks'
 * workaround is sufficient. No 'read' issues so far.
 *
 *TBD: Ramesh asked that GBOX MMIOs check for sleep states.
 *     Not sure how to do that, but here is a good spot to
 *     add such check, as all GBOX access comes thru here.
 */

#if NOT_YET
uint8_t *
mr_gbox_base(int unit)
{
  return micras_gbox[unit];
}
#endif

uint32_t
mr_gbox_rl(int unit, uint32_t roff)
{
  uint32_t	val;

  val = * (volatile uint32_t *)(micras_gbox[unit] + roff);
  RL;
  return val;
}

uint64_t
mr_gbox_rq(int unit, uint32_t roff)
{
  uint32_t	hi, lo;
  uint64_t	val;

  lo = * (volatile uint32_t *)(micras_gbox[unit] + roff);
  if (roff == 0x5c) {
    /*
     * Instead of placing HI part of MCA_STATUS
     * at 0x60 to form a natural 64-bit register,
     * it located at 0xac, against all conventions.
     */
    hi = * (volatile uint32_t *)(micras_gbox[unit] + 0xac);
  }
  else
    hi = * (volatile uint32_t *)(micras_gbox[unit] + roff + 4);
  val = ((uint64_t) hi << 32) | (uint64_t) lo;
  RQ;
  return val;
}

void
mr_gbox_wl(int unit, uint32_t roff, uint32_t val)
{
#if !GBOX_WORKING
  {
    int	rpt;
    uint32_t rb;

    /*
     * Due to bug HSD 4844222 loop until value sticks
     */
    for(rpt = 10; rpt-- ; ) {
#endif

      WL;
      * (volatile uint32_t *)(micras_gbox[unit] + roff) = val;

#if !GBOX_WORKING
      rb = mr_gbox_rl(unit, roff);
      if (rb == val)
        break;
    }
  }
#endif
}

void
mr_gbox_wq(int unit, uint32_t roff, uint64_t val)
{
  uint32_t	hi, lo;

  lo = val;
  hi = val >> 32;

#if !GBOX_WORKING
  {
    int	rpt;
    uint64_t rb;

    /*
     * Due to bug HSD 4844222 loop until value sticks
     * Note: this may result in bad things happening if
     *       wrinting to a MMIO MCA STATUS register
     *	     since there is a non-zero chance that the
     *       NMI handler can fire and change the register
     *       inside this loop. Require that the caller
     *       is on same CPU as the NMI handler (#0).
     */
    for(rpt = 10; rpt-- ; ) {
#endif

      WQ;
      * (volatile uint32_t *)(micras_gbox[unit] + roff) = lo;
      if (roff == 0x5c) {
        /*
         * Instead of placing HI part of MCA_STATUS
         * at 0x60 to form a natural 64-bit register,
         * it located at 0xac, against all conventions.
         */
        * (volatile uint32_t *)(micras_gbox[unit] + 0xac) = hi;
      }
      else
        * (volatile uint32_t *)(micras_gbox[unit] + roff + 4) = hi;

#if !GBOX_WORKING
      rb = mr_gbox_rq(unit, roff);
      if (rb == val)
        break;
    }
  }
#endif
}


#ifdef CONFIG_MK1OM
/*
 * TBOX MMIO I/O routines
 *  mr_tbox_base	Return TBOX MMIO region
 *  mr_tbox_rl		Read 32-bit register
 *  mr_tbox_rq		Read 64-bit register (really two 32-bit reads)
 *  mr_tbox_wl		Write 32-bit register
 *  mr_tbox_wq		Write 64-bit register (really two 32-bit writes)
 *
 * Some SKUs don't have TBOXs, in which case the
 * micras_tbox array will contain null pointers.
 * We do not test for this here, but expect that
 * caller either know what he's doing or consult
 * the mr_tbox_base() function first.
 */

#if NOT_YET
uint8_t *
mr_tbox_base(int unit)
{
  return micras_tbox[unit];
}
#endif

uint32_t
mr_tbox_rl(int unit, uint32_t roff)
{
  uint32_t	val;

  val = * (volatile uint32_t *)(micras_tbox[unit] + roff);
  RL;
  return val;
}

uint64_t
mr_tbox_rq(int unit, uint32_t roff)
{
  uint32_t	hi, lo;
  uint64_t	val;

  lo = * (volatile uint32_t *)(micras_tbox[unit] + roff);
  hi = * (volatile uint32_t *)(micras_tbox[unit] + roff + 4);
  val = ((uint64_t) hi << 32) | (uint64_t) lo;
  RQ;
  return val;
}

void
mr_tbox_wl(int unit, uint32_t roff, uint32_t val)
{
  WL;
  * (volatile uint32_t *)(micras_tbox[unit] + roff) = val;
}

void
mr_tbox_wq(int unit, uint32_t roff, uint64_t val)
{
  uint32_t	hi, lo;

  WQ;
  lo = val;
  hi = val >> 32;

  * (volatile uint32_t *)(micras_tbox[unit] + roff) = lo;
  * (volatile uint32_t *)(micras_tbox[unit] + roff + 4) = hi;
}
#endif



/*
**
** SMP utilities for CP and MC.
** The kernel offers routines for MSRs, but as far
** as I could find then there isn't any for some
** CPU registers we need, like CR4.
**
**  rd_cr4_on_cpu        Read a CR4 value on CPU
**  set_in_cr4_on_cpu    Set bits in CR4 on a CPU
**  clear_in_cr4_on_cpu  Guess...
**  rdtsc		 Read time stamp counter
**
**TBD: Special case when CPU happens to be current?
*/

#if NOT_YET
static void
_rd_cr4_on_cpu(void * p)
{
  *((uint32_t *) p) = read_cr4();
}

uint32_t
rd_cr4_on_cpu(int cpu)
{
  uint32_t	cr4;

  smp_call_function_single(cpu, _rd_cr4_on_cpu, &cr4, 1);
  return cr4;
}

static void
_set_in_cr4_on_cpu(void * p)
{
  uint32_t	cr4;

  cr4 = read_cr4();
  cr4 |= * (uint32_t *) p;
  write_cr4(cr4);
}

void
set_in_cr4_on_cpu(int cpu, uint32_t m)
{
  smp_call_function_single(cpu, _set_in_cr4_on_cpu, &m, 1);
}

static void
_clear_in_cr4_on_cpu(void * p)
{
  uint32_t	cr4;

  cr4 = read_cr4();
  cr4 &= ~ *(uint32_t *) p;
  write_cr4(cr4);
}

void
clear_in_cr4_on_cpu(int cpu, uint32_t m)
{
  smp_call_function_single(cpu, _clear_in_cr4_on_cpu, &m, 1);
}
#endif

uint64_t
rdtsc(void) {
  uint32_t lo, hi;
  __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
  return ((uint64_t) hi) << 32 | lo;
}



/*
**
** Module load/unload logic
**
*/


/*
 * Startup job (run by MT thread)
 * Intended to handle tasks that cannot impact
 * module load status, such as kicking off service
 * work queues, etc.
 */

static void
micras_init2(struct work_struct * work)
{
  /*
   * Make MT one-time setup and kick
   * off 1 sec timer and SCIF listeners
   */
  if (! micras_stop) {

    INIT_DELAYED_WORK(&micras_wq_tick, micras_mt_tick);
    queue_delayed_work(micras_wq, &micras_wq_tick, msecs_to_jiffies(5000));

    bitmap_fill(micras_cp_fd, MR_SCIF_MAX);
    INIT_DELAYED_WORK(&micras_cp_tkn, micras_cp);
    queue_delayed_work(micras_cp_wq, &micras_cp_tkn, 0);

    INIT_DELAYED_WORK(&micras_mc_tkn, micras_mc);
    queue_delayed_work(micras_mc_wq, &micras_mc_tkn, 0);

    INIT_DELAYED_WORK(&micras_ttl_tkn, micras_ttl);
    queue_delayed_work(micras_ttl_wq, &micras_ttl_tkn, 0);

#if defined(CONFIG_MK1OM) && WA_4845465 && DIE_PROC
    if (smc_4845465)
      die_pe = proc_create("die", 0644, 0, &proc_die_operations);
#endif
    
    printk("RAS.init: module operational\n");
    module_put(THIS_MODULE);
  }
}


static int __init
micras_init(void)
{
  int		i;
  int		err;

  printk("Loading RAS module ver %s. Build date: %s\n", RAS_VER, __DATE__);

  /*
   * Create work queue for the monitoring thread
   * and pass it some initial work to start with.
   */
#if defined(CONFIG_MK1OM) && WA_4845465
  micras_wq = alloc_workqueue("RAS MT", WQ_HIGHPRI | WQ_UNBOUND | WQ_MEM_RECLAIM, 1);
#else
  micras_wq = create_singlethread_workqueue("RAS MT");
#endif
  if (! micras_wq) {
    err = -ESRCH;
    printk("RAS.init: cannot start work queue, error %d\n", err);
    goto fail_wq;
  }

  /*
   * Register top sysfs class (directory) and attach attributes (files)
   * beneath it. No 'device's involved.
   */
  err = class_register(&ras_class);
  if (err) {
    printk("RAS.init: cannot register class 'micras', error %d\n", err);
    goto fail_class;
  }

  /*
   * Setup CP SCIF port in listening mode
   */
  micras_cp_lstn = scif_open();
  if (! micras_cp_lstn) {
    printk("RAS.init: cannot get SCIF CP endpoint\n");
    goto fail_cp;
  }
  err = scif_bind(micras_cp_lstn, MR_MON_PORT);
  if (err < 0) {
    printk("RAS.init: cannot bind SCIF CP endpoint, error %d\n", err);
    goto fail_cp_ep;
  }
  err = scif_listen(micras_cp_lstn, MR_SCIF_MAX);
  if (err < 0) {
    printk("RAS.init: cannot make SCIF CP listen, error %d\n", err);
    goto fail_cp_ep;
  }
  micras_cp_wq = create_singlethread_workqueue("RAS CP listen");
  if (! micras_cp_wq) {
    err = -ESRCH;
    printk("RAS.init: cannot start CP listener work queue, error %d\n", err);
    goto fail_cp_ep;
  }

  /*
   * Setup MC SCIF port in listening mode
   */
  micras_mc_lstn = scif_open();
  if (! micras_mc_lstn) {
    printk("RAS.init: cannot get SCIF MC endpoint\n");
    goto fail_mc;
  }
  err = scif_bind(micras_mc_lstn, MR_MCE_PORT);
  if (err < 0) {
    printk("RAS.init: cannot bind SCIF MC endpoint, error %d\n", err);
    goto fail_mc_ep;
  }
  err = scif_listen(micras_mc_lstn, MR_SCIF_MAX);
  if (err < 0) {
    printk("RAS.init: cannot make SCIF MC listen, error %d\n", err);
    goto fail_mc_ep;
  }
  micras_mc_wq = create_singlethread_workqueue("RAS MC listen");
  if (! micras_mc_wq) {
    err = -ESRCH;
    printk("RAS.init: cannot start listener work queue, error %d\n", err);
    goto fail_mc_ep;
  }

  /*
   * Setup TTL SCIF port in listening mode
   */
  micras_ttl_lstn = scif_open();
  if (! micras_ttl_lstn) {
    printk("RAS.init: cannot get SCIF TTL endpoint\n");
    goto fail_ttl;
  }
  err = scif_bind(micras_ttl_lstn, MR_TTL_PORT);
  if (err < 0) {
    printk("RAS.init: cannot bind SCIF TTL endpoint, error %d\n", err);
    goto fail_ttl_ep;
  }
  err = scif_listen(micras_ttl_lstn, MR_SCIF_MAX);
  if (err < 0) {
    printk("RAS.init: cannot make SCIF TTL listen, error %d\n", err);
    goto fail_ttl_ep;
  }
  micras_ttl_wq = create_singlethread_workqueue("RAS TTL listen");
  if (! micras_ttl_wq) {
    err = -ESRCH;
    printk("RAS.init: cannot start listener work queue, error %d\n", err);
    goto fail_ttl_ep;
  }

  /*
   * Make the MMIO maps we need.
   */
  micras_sbox = ioremap(SBOX_BASE, COMMON_MMIO_BOX_SIZE);
  if (! micras_sbox)
    goto fail_iomap;

  micras_dbox[0] = ioremap(DBOX0_BASE, COMMON_MMIO_BOX_SIZE);
  if (! micras_dbox[0])
    goto fail_iomap;

#ifdef CONFIG_MK1OM
  micras_dbox[1] = ioremap(DBOX1_BASE, COMMON_MMIO_BOX_SIZE);
  if (! micras_dbox[1])
    goto fail_iomap;
#endif

  micras_gbox[0] = ioremap(GBOX0_BASE, COMMON_MMIO_BOX_SIZE);
  micras_gbox[1] = ioremap(GBOX1_BASE, COMMON_MMIO_BOX_SIZE);
  micras_gbox[2] = ioremap(GBOX2_BASE, COMMON_MMIO_BOX_SIZE);
  micras_gbox[3] = ioremap(GBOX3_BASE, COMMON_MMIO_BOX_SIZE);
  if (!micras_gbox[0] || !micras_gbox[1] ||
      !micras_gbox[2] || !micras_gbox[3])
    goto fail_iomap;

#ifdef CONFIG_MK1OM
  micras_gbox[4] = ioremap(GBOX4_BASE, COMMON_MMIO_BOX_SIZE);
  micras_gbox[5] = ioremap(GBOX5_BASE, COMMON_MMIO_BOX_SIZE);
  micras_gbox[6] = ioremap(GBOX6_BASE, COMMON_MMIO_BOX_SIZE);
  micras_gbox[7] = ioremap(GBOX7_BASE, COMMON_MMIO_BOX_SIZE);
  if (!micras_gbox[4] || !micras_gbox[5] ||
      !micras_gbox[6] || !micras_gbox[7])
    goto fail_iomap;
#endif

#ifdef CONFIG_MK1OM
  /*
   * Most SKUs don't have TBOXes.
   * If not, then don't map to their MMIO space
   */
  if (mr_txs()) {
    micras_tbox[0] = ioremap(TXS0_BASE, COMMON_MMIO_BOX_SIZE);
    micras_tbox[1] = ioremap(TXS1_BASE, COMMON_MMIO_BOX_SIZE);
    micras_tbox[2] = ioremap(TXS2_BASE, COMMON_MMIO_BOX_SIZE);
    micras_tbox[3] = ioremap(TXS3_BASE, COMMON_MMIO_BOX_SIZE);
    micras_tbox[4] = ioremap(TXS4_BASE, COMMON_MMIO_BOX_SIZE);
    micras_tbox[5] = ioremap(TXS5_BASE, COMMON_MMIO_BOX_SIZE);
    micras_tbox[6] = ioremap(TXS6_BASE, COMMON_MMIO_BOX_SIZE);
    micras_tbox[7] = ioremap(TXS7_BASE, COMMON_MMIO_BOX_SIZE);
    if (!micras_tbox[0] || !micras_tbox[1] ||
        !micras_tbox[2] || !micras_tbox[3] ||
        !micras_tbox[4] || !micras_tbox[5] ||
        !micras_tbox[6] || !micras_tbox[7])
      goto fail_iomap;
  }
#endif

  /*
   * Setup non-volatile MC error logging device.
   */
  if (ee_init())
    goto fail_iomap;

  /*
   * Setup core MC event handler.
   * If this can't fail, move into micras_wq_init instead.
   */
  if (mcc_init())
    goto fail_ee;

  /*
   * Setup un-core MC event handler.
   * If this can't fail, move into micras_wq_init instead.
   */
  if (mcu_init())
    goto fail_core;

  /*
   * Prepare MT drivers
   */
  mr_mt_init();

#if defined(CONFIG_MK1OM) && USE_PM
  /*
   * Setup PM interface
   */
  if (pm_init())
    goto fail_uncore;
#endif

#if defined(CONFIG_MK1OM) && WA_4845465
  /*
   * Launch SMC temperature push work.
   * Supported by SMC firmware later than 121.11 (build 4511).
   */
  {
    extern int	mr_smc_rd(uint8_t, uint32_t *);
    int		rev, ref;

    mr_smc_rd(0x11, &rev);
    if (rev) {
      ref = PUT_BITS(31, 24, 121) |
	    PUT_BITS(23, 16, 11) |
	    PUT_BITS(15,  0, 4511);

      if (rev >= ref)
	smc_4845465 = rev;
    }

    if (smc_4845465) {
      INIT_DELAYED_WORK(&micras_wq_smc, micras_mt_smc);
      queue_delayed_work(micras_wq, &micras_wq_smc, 0);
      printk("RAS.init: HSD 4845465 workaround active, fw %x\n", rev);
    }
    else
      printk("RAS.init: SMC too old for HSD 4845465 workaround, fw %x\n", rev);
  }
#endif

  /*
   * Launch deferable setup work
   */
  try_module_get(THIS_MODULE);
  INIT_DELAYED_WORK(&micras_wq_init, micras_init2);
  queue_delayed_work(micras_wq, &micras_wq_init, msecs_to_jiffies(500));
  printk("RAS module load completed\n");
  return err;

  /*
   * Error exits: unwind all setup done so far and return failure
   *
   *TBD: consider calling exit function. Requires that it can tell
   *     with certainty what has been setup and what hasn't.
   */
#if defined(CONFIG_MK1OM) && USE_PM
fail_uncore:
  mr_mt_exit();
  mcu_exit();
#endif
fail_core:
  mcc_exit();
fail_ee:
#ifdef CONFIG_MK1OM
  ee_exit();
#endif
fail_iomap:
  if (micras_sbox)
    iounmap(micras_sbox);
  for(i = 0; i < ARRAY_SIZE(micras_dbox); i++)
    if (micras_dbox[i])
      iounmap(micras_dbox[i]);
  for(i = 0; i < ARRAY_SIZE(micras_gbox); i++)
    if (micras_gbox[i])
      iounmap(micras_gbox[i]);
#ifdef CONFIG_MK1OM
  for(i = 0; i < ARRAY_SIZE(micras_tbox); i++)
    if (micras_tbox[i])
      iounmap(micras_tbox[i]); 
#endif

  destroy_workqueue(micras_ttl_wq);

fail_ttl_ep:
  scif_close(micras_ttl_lstn);

fail_ttl:
  destroy_workqueue(micras_mc_wq);

fail_mc_ep:
  scif_close(micras_mc_lstn);

fail_mc:
  destroy_workqueue(micras_cp_wq);

fail_cp_ep:
  scif_close(micras_cp_lstn);

fail_cp:
  class_unregister(&ras_class);

fail_class:
  micras_stop = 1;
  flush_workqueue(micras_wq);
  destroy_workqueue(micras_wq);

fail_wq:
  printk("RAS module load failed\n");
  return err;
}


static void __exit
micras_exit(void)
{
  int		i;
  scif_epd_t	ep;

  printk("Unloading RAS module\n");
  micras_stop = 1;

  /*
   * Disconnect MC event handlers and
   * close the I2C eeprom interfaces.
   */
  mcu_exit();
  mcc_exit();
  ee_exit();
 
  /*
   * Close SCIF listeners (no more connects).
   */
  scif_close(micras_cp_lstn);
  scif_close(micras_mc_lstn);
  scif_close(micras_ttl_lstn);
  msleep(10);
  destroy_workqueue(micras_cp_wq);
  destroy_workqueue(micras_mc_wq);
  destroy_workqueue(micras_ttl_wq);

  /*
   * Terminate active sessions by closing their end points.
   * Session threads then should clean up after themselves.
   */
  for(i = 0; i < MR_SCIF_MAX; i++) {
    if (micras_cp_kt[i]) {
      printk("RAS.exit: force closing CP session %d\n", i);
      ep = (scif_epd_t) atomic64_xchg((atomic64_t *)(micras_cp_ep + i), 0);
      if (ep)
        scif_close(ep);
    }
  }
  for(i = 0; i < 1000; i++) {
    if (bitmap_weight(micras_cp_fd, MR_SCIF_MAX) == MR_SCIF_MAX)
      break;
    msleep(1);
  }
  if (micras_mc_kt) {
    printk("RAS.exit: force closing MC session\n");
    ep = (scif_epd_t) atomic64_xchg((atomic64_t *) &micras_mc_ep, 0);
    if (ep)
      scif_close(ep);
    for(i = 0; (i < 1000) && micras_mc_kt; i++)
      msleep(1);
  }
  if (micras_ttl_kt) {
    printk("RAS.exit: force closing TTL session\n");
    ep = (scif_epd_t) atomic64_xchg((atomic64_t *) &micras_ttl_ep, 0);
    if (ep)
      scif_close(ep);
    for(i = 0; (i < 1000) && micras_ttl_kt; i++)
      msleep(1);
  }

  /*
   * Tear down sysfs class and its nodes
   */
  class_unregister(&ras_class);

#if defined(CONFIG_MK1OM) && USE_PM
  /*
   * De-register with the PM module.
   */
  pm_exit();
#endif

  /*
   * Shut down the work queues
   */
#if defined(CONFIG_MK1OM) && WA_4845465
  if (smc_4845465)
    cancel_delayed_work(&micras_wq_smc);
#endif
  cancel_delayed_work(&micras_wq_tick);
  cancel_delayed_work(&micras_wq_init);
  flush_workqueue(micras_wq);
  destroy_workqueue(micras_wq);

  /*
   * Restore MT state
   */
  mr_mt_exit();

  /*
   * Remove MMIO region maps
   */
  iounmap(micras_sbox);
  for(i = 0; i < ARRAY_SIZE(micras_dbox); i++)
    if (micras_dbox[i])
      iounmap(micras_dbox[i]);
  for(i = 0; i < ARRAY_SIZE(micras_gbox); i++)
    if (micras_gbox[i])
      iounmap(micras_gbox[i]);
#ifdef CONFIG_MK1OM
  for(i = 0; i < ARRAY_SIZE(micras_tbox); i++)
    if (micras_tbox[i])
      iounmap(micras_tbox[i]);
#endif

#if defined(CONFIG_MK1OM) && WA_4845465 && DIE_PROC
  if (smc_4845465 && die_pe) {
    remove_proc_entry("die", 0);
    die_pe = 0;
  }
#endif

  printk("RAS module unload completed\n");
}

module_init(micras_init);
module_exit(micras_exit);

MODULE_AUTHOR("Intel Corp. 2013 (" __DATE__ ") ver " RAS_VER);
MODULE_DESCRIPTION("RAS and HW monitoring module for MIC");
MODULE_LICENSE("GPL");

