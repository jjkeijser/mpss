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
 * RAS MT module, common code
 *
 * Code and data structures to handle get/set tasks for KnC and KnF.
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
#include <linux/tick.h>
#include <linux/kernel_stat.h>
#include <linux/bitmap.h>
#include <generated/compile.h>
#include <generated/utsrelease.h>
#include <asm/mic/mic_knc/autobaseaddress.h>
#include <asm/mic/mic_knc/micsboxdefine.h>
#include "micras.h"


/*
 * Persistent data accessible through the CP api.
 * Some functions just read/modify hardware CSRs
 * and thus need no storage between invocations.
 */

       struct mr_rsp_hwinf	hwinf;  /* Card specific */
       struct mr_rsp_vers	vers;	/* Card specific */
static struct mr_rsp_pver	pver;
       struct mr_rsp_freq	freq;	/* Card specific */
       struct mr_rsp_volt	volt;	/* Card specific */
       struct mr_rsp_power	power;	/* Card specific */
       struct mr_rsp_plim	plim;	/* Card specific */
static struct mr_rsp_clst	clst;
       struct mr_rsp_gddr	gddr;
       struct mr_rsp_gfreq	gfreq;	/* Card Specific */
       struct mr_rsp_gvolt	gvolt;	/* Card specific */
       struct mr_rsp_temp	temp;	/* Card specific */
       struct mr_rsp_ecc	ecc;	/* Card specific */
static struct mr_rsp_trc	trc;
       struct mr_rsp_trbo	trbo;	/* Card specific */
       struct mr_rsp_pmcfg	pmcfg;	/* Card specific */


/*
 * Map of SKUs for KnX cards (currently known, will change)
 * The SKU is identified solely from the PCIe ID and sub-ID.
 * A zero sub-ID is a don't care.
 *
 *TBD: core counts in KnF needs update, not all have 32.
 *
 * Notes:
 * - Unless the PCIe subID differs, there are two 2250 cards
 *   that can't be distinguished from each other, one has 8 TXs
 *   and the other has none. PO cards -> impact only internal.
 * - Not sure exactly what 2254 is, suspect MPI prototype.
 */

#define	VD(v, d)	(PUT_BITS(15,0,(v)) | PUT_BITS(31,16,(d)))

static struct sku {
  uint32_t	devID;		/* PCIe Vendor and device ID */
  uint32_t	subID;		/* PCIe Sub- Vendor and device ID */
  uint8_t	revNo;		/* PCIe Revision number */
  uint8_t	cr;		/* Core count */
  uint8_t	ch;		/* Memory channels */
  uint8_t	tx;		/* TX samplers (only in KnC) */
  char	      * name;		/* SKU name */
} skuList[] = {
  { VD(0x8086, 0x2240), 0, 0x00, 32, 8, 0, "E1" },		/* KnF */
  { VD(0x8086, 0x2241), 0, 0x00, 32, 8, 0, "E2" },		/* KnF */
  { VD(0x8086, 0x2242), 0, 0x00, 32, 8, 0, "E3" },		/* KnF */
  { VD(0x8086, 0x2243), 0, 0x00, 32, 8, 0, "E3" },		/* KnF */
  { VD(0x8086, 0x2249), VD(0x8086, 0xed08), 0, 32, 4, 0, "Ed" }, /* KnF */
  { VD(0x8086, 0x2249), VD(0x8086, 0xed0a), 0, 32, 4, 0, "Eb" }, /* KnF */
  { VD(0x8086, 0x224a), 0, 0x00, 32, 8, 0, "Eb" },		/* KnF */

  { VD(0x8086, 0x2250), 0, 0x00, 60, 16, 0, "SKU1/SKU2" },	/* KnC: ES1, ES1B */
  { VD(0x8086, 0x2250), 0, 0x10, 60, 16, 0, "SKU2" },		/* KnC: ES2 */
  { VD(0x8086, 0x2250), 0, 0x11, 60, 16, 0, "SKU2" },		/* KnC: Mkt2 */
  { VD(0x8086, 0x2250), 0, 0x20, 60, 16, 0, "SKU2" },
  { VD(0x8086, 0x2251), 0, 0x00, 48, 16, 8, "SKU2" },
  { VD(0x8086, 0x2252), 0, 0x00, 48, 16, 0, "SKU3" },
  { VD(0x8086, 0x2253), 0, 0x00, 40,  8, 0, "SKU4/SKU5" },	/* KnC: ES0, ES1 */
  { VD(0x8086, 0x2253), 0, 0x10, 40,  8, 0, "SKU5" },
  { VD(0x8086, 0x2254), 0, 0x00, 62, 16, 0, "??" },		/* KnC: ?? */
  { VD(0x8086, 0x2255), 0, 0x00, 62, 16, 8, "SKUX" },		/* KnC: A0-PO */
  { VD(0x8086, 0x2256), 0, 0x00, 48, 12, 7, "SKU5" },		/* KnC: A0-PO */
  { VD(0x8086, 0x2257), 0, 0x00,  4, 16, 0, "SKUZ" },
  { VD(0x8086, 0x2258), 0, 0x00, 62, 16, 0, "SKU1" },		/* KnC: ES1, ES1B */
  { VD(0x8086, 0x2258), 0, 0x10, 62, 16, 0, "SKU1" },
  { VD(0x8086, 0x2259), 0, 0x00, 52, 16, 0, "SKU3" },		/* KnC: ES1 */
  { VD(0x8086, 0x225a), 0, 0x00, 48, 12, 0, "SKU4" },		/* KnC: ES1, ES1B */
  { VD(0x8086, 0x225a), 0, 0x10, 48, 12, 0, "SKU4" },		/* KnC: ES2 */
  { VD(0x8086, 0x225a), 0, 0x11, 48, 12, 0, "SKU4" },		/* KnC: Int5 */
  { VD(0x8086, 0x225b), 0, 0x00, 52, 12, 0, "SKU3" },
  { VD(0x8086, 0x225b), 0, 0x10, 52, 12, 0, "SKU3" },
  { VD(0x8086, 0x225c), 0, 0x10, 61, 16, 0, "SKU1" },		/* KnC: Mkt1 */
  { VD(0x8086, 0x225c), 0, 0x11, 61, 16, 0, "SKU1" },		/* KnC: Mkt1 */
  { VD(0x8086, 0x225c), 0, 0x20, 61, 16, 0, "SKU1" },		/* KnC: Mkt1 */
  { VD(0x8086, 0x225d), 0, 0x10, 57, 12, 0, "SKU4" },		/* KnC: Mkt4 */
  { VD(0x8086, 0x225d), 0, 0x11, 57, 12, 0, "SKU4" },		/* KnC: Mkt3, Mkt4 */
  { VD(0x8086, 0x225d), 0, 0x20, 57, 12, 0, "SKU4" },
  { VD(0x8086, 0x225e), 0, 0x11, 57, 16, 0, "GZ" },
  { VD(0x8086, 0x225e), 0, 0x20, 57, 16, 0, "GZ" },
};


/*
 * Map of GDDR vendor ID vs company names
 */

static struct {
  int	 id;
  char * vendor;
} GddrVendors[] = {
  { 1, "Samsung" },
  { 2, "Quimonda" },
  { 3, "Elpida" },
  { 6, "Hynix" },
};



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

void __init
mr_mt_init(void)
{
  static int    only_once = 1;
  uint32_t	scr4, scr9, scr13;
  uint32_t	eax, ebx, ecx, edx;
  uint32_t	thr, hwt;
  uint32_t	id;
  int		i;

  if (! only_once)
    return;
  only_once = 0;

  /*
   * HWINF:
   * Scratch register 13 has more info than the hwinf record
   * currently can contain, may revisit.
   *  3:0	Substepping
   *  7:4	Stepping (0 A, 2&3 B, 4 C, 6 D)
   * 11:8	Model
   * 15:12	Family (11 KnF)
   * 17:16	Processor
   * 19:18	Platform (0 Silicon, 1 FSIM, 2 MCEMU)
   * 23:20	Extended model
   * 31:24	Extended family
   *
   * Valid KnF steppings (Step + Substep):
   * "A0" (0 + 0), "A1" (0 + 1), "A2" (0 + 2),
   * "B0" (2 + 0), "B1" (3 + 1), "C0" (4 + 0),
   * "D0" (6 + 0)
   * Valid KnC steppings (Step + Substep):
   * TBD:
   */
  scr13 = mr_sbox_rl(0, SBOX_SCRATCH13);
  hwinf.rev     = GET_BITS(11,  8, scr13);
  hwinf.step    = GET_BITS( 7,  4, scr13);
  hwinf.substep = GET_BITS( 3,  0, scr13);

  /*
   * VERS:
   * Add OS version
   */
  vers.uos[0] = scnprintf(vers.uos + 1, MR_VERS_LEN -2,
		"Linux version: %s (build %s)",
	      		init_uts_ns.name.release,
	      		init_uts_ns.name.version);

  /*
   * PVERS:
   * Make MicRas version available
   */
  pver.api[0] = scnprintf(pver.api + 1, MR_PVER_LEN -2,
  		"%s", RAS_VER);

  /*
   * CLST:
   * On regular CPU's this is read from CPUID 2 (htt cores)
   * and CPUID 4 (die cores), threads per cores is htt/die.
   * This does not work the same way in MIC, cores & threads
   * per core on various SKUs is not reflected by the CPUIDs.
   * All we have is the number of registered APIC IDs, which
   * happens to be the same as logical CPUs (htt cores).
   * The threads per core (die cores) is given by bootstrap in
   * scratch register #4 as a bit field.
   *   3:0	Threads per core (mask)
   *   5:4	Cache size (0,1,2: 512K, 3: 256K)
   *   9:6	GBOX channel count (0 based)
   *  29:25	ICC divider for MCLK
   *     30	Soft reset boot
   *     31	Internal flash build
   */
  cpuid(1, &eax, &ebx, &ecx, &edx);
  hwt = GET_BITS(23, 16, ebx);
  if (hwt > nr_cpu_ids)
    hwt = nr_cpu_ids;	
  scr4 = mr_sbox_rl(0, SBOX_SCRATCH4);
  thr = GET_BITS(3, 0, scr4);
  thr = bitmap_weight((const unsigned long *) &thr, 4);
  if (thr) {
    if (hwt % thr)
      printk("mr_mt_init: cpu/thr mismatch: hwt %d, thr %d, cor %d, (%d)\n",
      		hwt, thr, hwt / thr, hwt % thr);
    clst.thr = thr;
  }
  else {
    printk("Who trashed scratch #4? Val 0x%08x => 0 threads/core?\n", scr4);
    clst.thr = 4;	/* Best guess */
  }
  clst.count = hwt / 4;

  /*
   * GDDR:
   * Bootstrap leaves information in scratch register #9
   * about the GDDR devices. The layout is:
   *  3:0	Vendor ID, see table GddrVendors above
   *  7:4	Revision
   *  9:8	Density (00 = 512, 01 = 1024, 02 = 2048)
   * 11:10	FIFO depth
   * 15:12	DRAM info ??
   *    29	ECC enable
   */
  scr9 = mr_sbox_rl(0, SBOX_SCRATCH9);
  id = GET_BITS(3, 0, scr9);
  for(i = 0; i < ARRAY_SIZE(GddrVendors); i++)
    if (GddrVendors[i].id == id) {
      gddr.dev[0] = scnprintf(gddr.dev +1, MR_GVND_LEN -2,
			      "%s", GddrVendors[i].vendor);
      break;
    }
  if (i == ARRAY_SIZE(GddrVendors))
    gddr.dev[0] = scnprintf(gddr.dev +1, MR_GVND_LEN -2, "Vendor %d", id);
  gddr.rev = GET_BITS(7, 4, scr9);
  gddr.size = 512 * (1 << GET_BITS(9, 8, scr9));

  /*
   * Card specific initialization
   */
  mr_mt_card_init();

  /*
   *TBD: Save commmon registers this module may change
   */
}

void __exit
mr_mt_exit(void)
{
  /*
   * Card specific clean-up
   */
  mr_mt_card_exit();

  /*
   *TBD: Restore commmon registers this module may change
   */
}


/*
 * Return SKU properties for this card (as string)
 * Processor can be identified on it's own easily,
 * but the SKU reflects the impact of fuse changes
 * which don't alter the CPU id.
 *
 * SKU properties:
 *  - name	Name of sku (if known)
 *  - mch	Number of memory channels
 *  - txs	Number of texture samplers
 */

/*
 * Why are these not defined in the includes?
 */

#ifndef SBOX_PCIE_VENDOR_ID_DEVICE_ID
#define SBOX_PCIE_VENDOR_ID_DEVICE_ID           0x00005800
#endif
#ifndef SBOX_PCIE_PCI_SUBSYSTEM
#define SBOX_PCIE_PCI_SUBSYSTEM                 0x0000582c
#endif
#ifndef SBOX_PCIE_PCI_REVISION_ID_AND_C_0X8
#define SBOX_PCIE_PCI_REVISION_ID_AND_C_0X8	0x00005808
#endif

static struct sku *
get_sku(void)
{
  static struct sku * sku;
  uint32_t	dev, sub, rev, fuse;
  char	      * grp;
  int		i;
 
  if (sku)
    return sku;

  dev = mr_sbox_rl(0, SBOX_PCIE_VENDOR_ID_DEVICE_ID);
  rev = mr_sbox_rl(0, SBOX_PCIE_PCI_REVISION_ID_AND_C_0X8);
  sub = mr_sbox_rl(0, SBOX_PCIE_PCI_SUBSYSTEM);
  fuse = mr_sbox_rl(0, SBOX_SCRATCH7);
  rev = GET_BITS(7, 0, rev);
  fuse = GET_BITS(15, 0, fuse);
 
  /*
   * Usually the fuse revision define a group of SKUs.
   * Once that's determined we'll use the other details
   * to identify the SKU within that group.
   */
       if (fuse >= 0 && fuse <= 1)
         grp = "A0 PO";
  else if (fuse >= 2 && fuse <= 3)
         grp = "A0 ES1";
  else if (fuse >= 4 && fuse <= 50)
         grp = "A0 ES1B";
  else if (fuse >= 51 && fuse <= 100)
         grp = "B0 PO";
  else if (fuse >= 101 && fuse <= 150)
         grp = "B0 ES2";
  else if (fuse >= 151 && fuse <= 152)
         grp = "B1 PO";
  else if (fuse >= 153 && fuse <= 154)
         grp = "B1 PO";
  else if (fuse == 155)
         grp = "B1 QS";
  else if (fuse == 156)
         grp = "B1 PRQ";
  else if (fuse == 157)
         grp = "B1 PRQ/GZ";
  else if (fuse >= 158 && fuse <= 159)
         grp = "B1 PRQ";
  else if (fuse >= 201 && fuse <= 203)
         grp = "B2 PRQ/QS";
  else if (fuse == 253)
         grp = "C0 PO";
  else if (fuse == 254)
         grp = "C0 QS";
  else 
         grp = "???";

  /*
   * Now determine which member of the group.
   * Take hints from PCIe device ID and revision.
   * Device ID mappings is a mess, see table above.
   * Revision has a simple mapping (follows fuses):
   *   0x00 => A0 cards
   *   0x10 => B0 cards
   *   0x11 => B1 cards
   *   0x20 => C0 cards
   *   0x21 => C1 cards (if ever to be made)
   */
  for(i = 0; i < ARRAY_SIZE(skuList); i++) {
    if (dev == skuList[i].devID) {
      if (skuList[i].subID && sub != skuList[i].subID)
        continue;
      if (rev != skuList[i].revNo)
	continue;

      /*
       * Found one, this is the place to cross reference it
       *  - memory channels should match SCR4 bits 9:6
       */
      break;
    }
  }

  if (i < ARRAY_SIZE(skuList)) {
    sku = skuList + i;
    printk("RAS: card %x:%x:%x is a \"%s %s\" (%d cores, %d memch, %d txs)\n",
    			dev, sub, rev, grp, sku->name, sku->cr, sku->ch, sku->tx);
  }

  return sku;
}

#if NOT_YET
char *
mr_sku(void)
{
  struct sku  * sku;

  sku = get_sku();
  return sku ? sku->name : 0;
}
#endif

int
mr_mch(void)
{
  struct sku  * sku;

  sku = get_sku();
  return sku ? sku->ch : 0;
}

int
mr_txs(void)
{
  struct sku  * sku;

  sku = get_sku();
  return sku ? sku->tx : 0;
}


/*
**
** MT Get functions
**
** All works the same way; they get an opague pointer to
** a place where the return structure can be placed. The
** return value is either the amount (bytes) to be shipped
** back in response or one of the MR_* error codes.
**
*/

int
mr_get_hwinf(void * p)
{
  struct mr_rsp_hwinf * r;

  r = (struct mr_rsp_hwinf *) p;
  *r = hwinf;
  return sizeof(*r);
}


int
mr_get_vers(void * p)
{
  struct mr_rsp_vers  * r;

  r = (struct mr_rsp_vers *) p;
  *r = vers;
  return sizeof(*r);
}


int
mr_get_pver(void * p)
{
  struct mr_rsp_pver  * r;

  r = (struct mr_rsp_pver *) p;
  *r = pver;
  return sizeof(*r);
}


int
mr_get_clst(void * p)
{
  struct mr_rsp_clst  * r;

  r = (struct mr_rsp_clst *) p;
  *r = clst;
  return sizeof(*r);
}


int
mr_get_gddr(void * p)
{
  struct mr_rsp_gddr  * r;

  r = (struct mr_rsp_gddr *) p;
  *r = gddr;
  return sizeof(*r);
}


int
mr_get_trc(void * p)
{
  struct mr_rsp_trc   * r;

  r = (struct mr_rsp_trc *) p;
  *r = trc;
  return sizeof(*r);
}


int
mr_get_cutl(void * p)
{
  struct mr_rsp_cutl  * r;
  struct timespec	tp;
  struct cpu_usage_stat * u;
  uint64_t		user, nice, sys, idle;
  int			i, n;

  r = (struct mr_rsp_cutl *) p;
  memset(r, '\0', sizeof(*r));
  r->tck  = ACTHZ;
  r->core = clst.count;
  r->thr  = clst.thr;
  ktime_get_ts(&tp);
  monotonic_to_bootbased(&tp);
  r->jif = timespec_to_jiffies(&tp);

  for_each_possible_cpu(i) {
    u = & kstat_cpu(i).cpustat;

    user = u->user;
    nice = u->nice;
    sys  = u->system + u->irq + u->softirq;
    idle = u->idle + u->iowait;

    r->sum.user += user;
    r->sum.nice += nice;
    r->sum.sys  += sys;
    r->sum.idle += idle;

    /*
     * Currently the boot processor is thread 0 of the last
     * enabled core. Thus, on a 32 core machine, we get:
     *
     * cpu #	0    1  2  3  4  5 .. 124  125  126 127
     * core #	31   0  0  0  0  1 ..  30   31   31  31
     * apic ID	124  0  1  2  3  4 .. 123  125  126 127
     *
     * The core is included in the per-cpu CpuInfo struct,
     * and it should be safe to get it from there.
     */
    n = cpu_data(i).cpu_core_id;
    if (n < r->core) {
      r->cpu[n].user += user;
      r->cpu[n].nice += nice;
      r->cpu[n].sys  += sys;
      r->cpu[n].idle += idle;
    }
  }

  return sizeof(*r);
}


int
mr_get_mem(void * p)
{
  struct mr_rsp_mem   * r;
  struct sysinfo	si;

  si_meminfo(&si);

  r = (struct mr_rsp_mem *) p;
  memset(r, '\0', sizeof(*r));
  r->total = si.totalram  << (PAGE_SHIFT - 10);
  r->free  = si.freeram   << (PAGE_SHIFT - 10);
  r->bufs  = si.bufferram << (PAGE_SHIFT - 10);

  return sizeof(*r);
}


int
mr_get_os(void * p)
{
  struct mr_rsp_os    * r;
  uint16_t		i;
  struct timespec       tp;
  struct task_struct  * t;

  ktime_get_ts(&tp);
  monotonic_to_bootbased(&tp);

  r = (struct mr_rsp_os *) p;
  memset(r, '\0', sizeof(*r));
  r->uptime = tp.tv_sec + (tp.tv_nsec ? 1 : 0);
  r->loads[0] = avenrun[0] << (SI_LOAD_SHIFT - FSHIFT);
  r->loads[1] = avenrun[1] << (SI_LOAD_SHIFT - FSHIFT);
  r->loads[2] = avenrun[2] << (SI_LOAD_SHIFT - FSHIFT);

  /*
   * Walk process list and indentify processes that
   * are associated with user programs. For now we
   * exclude kernel threads and non-stable processes.
   *
   *TBD: Really just wanted to take the task_lock, but
   *     it is not exported to modules. It seems to be
   *     tied into the RCU logic, so locking the whole
   *     RCU should do the trick as long as it's just
   *     for a very short time.
   */
  i = 0;
  rcu_read_lock();
  for_each_process(t) {
    if ((t->flags & (PF_KTHREAD | PF_STARTING | PF_EXITING)) ||
        (t->group_leader && t->group_leader != t))
      continue;

    if (i < ARRAY_SIZE(r->apid))
      r->apid[i] = t->pid;
    i++;
  }
  rcu_read_unlock();
  r->alen = i;

  return sizeof(*r);
}


int
mr_get_proc(void * p)
{
  struct mr_rsp_proc  * r;
  struct task_struct  * t, * s;
  struct mm_struct    * mm;
  struct timespec	uptime, start, ts;
  cputime_t		utime, stime;
  pid_t			pid;
  int			err, i;

  err = -MR_ERR_NOVAL;
  pid = * (uint32_t *) p;
  if (! pid)
    return err;

  r = (struct mr_rsp_proc *) p;
  memset(r, '\0', sizeof(*r));
  do_posix_clock_monotonic_gettime(&uptime);

  rcu_read_lock();
  t = pid_task(find_pid_ns(pid, &init_pid_ns), PIDTYPE_PID);
  if (t) {
    /*
     * Found process, get base stats
     */
    r->pid = pid;
    strncpy(r->name +1, t->comm, sizeof(r->name) -1);
    start = t->start_time;
    utime = t->utime;
    stime = t->stime;
    mm = get_task_mm(t);
    if (mm) {
#ifdef SPLIT_RSS_COUNTING
      r->rss = atomic_long_read(& mm->rss_stat.count[MM_FILEPAGES]) +
	       atomic_long_read(& mm->rss_stat.count[MM_ANONPAGES]);
#else
      r->rss = mm->rss_stat.count[MM_FILEPAGES] +
	       mm->rss_stat.count[MM_ANONPAGES];
#endif
      r->vm = mm->total_vm;
      mmput(mm);
    }

    /*
     * Next try get list of threads (if any)
     */
    i = 0;
    if (!t->group_leader || t->group_leader == t) {
      s = t;
      do {
	if (s->pid != pid) {
	  if (i < ARRAY_SIZE(r->tpid))
	    r->tpid[i++] = s->pid;
	}
      } while_each_thread(t, s);
    }
    r->tlen = i;
    err = sizeof(*r);
  }
  rcu_read_unlock();

  /*
   * Convert values into API formats (uSec, kB).
   */
  if (err > 0) {
    r->name[0] = strlen(r->name +1);
    ts = timespec_sub(uptime, start);
    r->etime = timespec_to_ns(&ts) / NSEC_PER_USEC;
    r->utime = jiffies_to_usecs(utime);
    r->stime = jiffies_to_usecs(stime);
    r->vm  = r->vm  << (PAGE_SHIFT - 10);
    r->rss = r->rss << (PAGE_SHIFT - 10);
  }

  return err;
}



/*
**
** MT Set functions
**
** All works the same way; they get an opague pointer to
** a location where the 'set' parameter from the request is
** placed. Return code is one of the MR_* error codes.
**
** Input screening takes place here (to the extent possible).
**
*/


#if NOT_YET
int
mr_set_gvolt(void * p)
{
  /*
   * Cannot be set from uOS, pretend success
   */
  return 0;
}


int
mr_set_gfreq(void * p)
{
  /*
   * Cannot be set from uOS, pretend success
   */
  return 0;
}
#endif


int
mr_set_trc(void * p)
{
  /*
   * No idea on what to do with this
   */
  trc.lvl = *(uint32_t *) p;
  return 0;
}



/*
**
** MT Process controls
**
*/

int
mr_cmd_pkill(void * p)
{
  struct task_struct  * t;
  const struct cred   * cred;
  pid_t			pid;
  uint32_t		val;
  int			sig, ret;

  val = *(uint32_t *) p;
  pid = GET_BITS(23, 0, val);
  sig = GET_BITS(31, 24, val);

  ret = -MR_ERR_INVAUX;
  rcu_read_lock();
  t = pid_task(find_pid_ns(pid, &init_pid_ns), PIDTYPE_PID);
  if (t) {
    if (!(t->flags & (PF_KTHREAD | PF_STARTING | PF_EXITING)) &&
        !(t->group_leader && t->group_leader != t)) {

      cred = __task_cred(t);
      if (cred->euid >= 500) {
        if (!send_sig(sig, t, 1))
          ret = 0;
      }
      else
        ret = -MR_ERR_PERM;
    }
  }
  rcu_read_unlock();

  return ret;
}


int
mr_cmd_ukill(void * p)
{
  struct task_struct  * t;
  const struct cred   * cred;
  uid_t			uid;
  uint32_t		val;
  int			sig, ret;

  val = *(uint32_t *) p;
  uid = GET_BITS(23, 0, val);
  sig = GET_BITS(31, 24, val);

  if (uid < 500)
    return -MR_ERR_PERM;

  ret = 0;
  rcu_read_lock();
  for_each_process(t) {
    if ((t->flags & (PF_KTHREAD | PF_STARTING | PF_EXITING)) ||
        (t->group_leader && t->group_leader != t))
      continue;

    cred = __task_cred(t);
    if (cred->euid == uid) {
      ret = send_sig(sig, t, 1);
      if (ret)
        break;
    }
  }
  rcu_read_unlock();

  return ret ? -MR_ERR_INVAUX : 0;
}


/*
**
** Debug utilities.
** Remove or comment out when development complete!
**
*/

#if EE_VERIFY
/*
 * Hex dumper
 */

#include <linux/ctype.h>

#define ALEN	        9               /* Digits of address shown */

void
dmp_hex(void *ptr, int len, const char *msg, ...)
{
    unsigned char       * d;
    unsigned char       * prev;
    int                   n, m;
    int                   star;
    char		  asc[16 + 1];

    star = 0;
    prev = 0;

    /*
     * Print message (if any).
     * It is treated as a 'printf' format strings with arguments.
     */
    if (msg) {
        va_list               ap;

        va_start(ap, msg);
        vprintk(msg, ap);
        va_end(ap);
        printk("\n");
    }

    /*
     * Loop trying to dump 16 bytes at a time
     */
    for(d = (unsigned char *) ptr;; d += 16) {

        /*
         * Locate dump area from input buffer;
         */
        n = (len > 16) ? 16 : len;
        len -= n;

        /*
         * Skip repeated lines.
         * I want the last line shown on the output.
         */
        if (d != ptr && n == 16 && !memcmp(d, prev, 16)) {
            if (len) {
                if (!star) {
                    star = 1;
                    printk("%*s\n", ALEN + 3, "*");
                }
                continue;
            }
        }

        /*
         * Print one line of hex dump.
         */
        if (n) {
            printk("%*lx  ", ALEN, ((long) d) & ((1L << 4 * ALEN) - 1));
            for(m = 0; m < n; m++) {
                printk("%02x ", d[m]);
                if (m == 7)
                    printk(" ");
                asc[m] = (isascii(d[m]) && isprint(d[m])) ? d[m] : '.';
            }
            asc[m] = '\0';
            printk("%*s  %s\n", 3 * (16 - m) + (m < 8), "", asc);
        }

        /*
         * We are done when end of buffer reached
         */
        if (!len)
            break;

        /*
         * Reset repeat line suppression
         */
        star = 0;
        prev = d;
    }
}
#endif
