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
 * RAS EEPROM log driver
 *
 * Contains code to handle creation of MC event records in
 * the designated EEPROM hanging off the 'OverClocking' I2C bus.
 *
 * Since it is not clear for the moment for how long the serial
 * port on the POST card needs to (or will) be supported, it is
 * not safe to assume we just can tap into the Linux I2C frame
 * work to access the 'OverClocking' I2C bus.
 *
 * Furthermore, we need access from exception context, and cannot
 * run a driver that has spinlocks, mutexes and sleeps in it's path
 * like the current PXA-derived driver has.
 *
 * Therefore, a local exception safe driver is included here.
 */

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/ctype.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/serial_reg.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <asm/uaccess.h>
#include <asm/mic/mic_knc/autobaseaddress.h>
#include <asm/mic/mic_knc/micsboxdefine.h>
#include "micras.h"

#ifdef MIC_IS_EMULATION
/*
 * Emulation does not handle I2C busses.
 * Therefore all code that deals with I2C needs to be
 * replaced with harmless substitutes in emulation.
 * The following stubs are for emulation only.
 */

#if 0
/*
 * Probably don't need exclusive locks in emulation
 */
atomic_t pxa_block = ATOMIC_INIT(0);

static void
ee_lock(void)
{
  while(atomic_xchg(&pxa_block, 1))
    myDELAY(50);
}

static void
ee_unlock(void)
{
  atomic_xchg(&pxa_block, 0);
}
#endif

char		ee_buf[EE_BUF_COUNT * EE_BUF_LINELEN];
atomic_t	ee_msg = ATOMIC_INIT(-1);
atomic_t	ee_seen = ATOMIC_INIT(0);
int		ee_rdy;

char *
ee_fmt(char * fmt, va_list args)
{
  char	      * buf;
  int		msg_id, msg_btm;

  msg_btm = atomic_read(&ee_seen);
  msg_id = atomic_inc_return(&ee_msg);
  if ((msg_id - msg_btm) < (EE_BUF_COUNT - 1)) {
    buf = ee_buf + (msg_id % EE_BUF_COUNT) * EE_BUF_LINELEN;
    vsnprintf(buf, EE_BUF_LINELEN - 1, fmt, args);
    return buf;
  }
  return 0;
}

int
ee_printk(char * fmt, ...)
{
  va_list       args;
  char	      * buf;

  va_start(args, fmt);
  buf = ee_fmt(fmt, args);
  va_end(args);

  return buf ? strlen(buf) : 0;
}

int
ee_print(char * fmt, ...)
{
  va_list       args;
  char	      * buf;

  va_start(args, fmt);
  buf = ee_fmt(fmt, args);
  va_end(args);

  return buf ? strlen(buf) : 0;
}
EXPORT_SYMBOL_GPL(ee_print);


int
ee_init(void)
{
  ee_rdy = 1;

  if (mce_disabled)
    printk("RAS.elog (EMU): disabled\n");
  else
    printk("RAS.elog (EMU): init complete\n");
  return 0;
}

int
ee_exit(void)
{
  ee_rdy = 0;

  printk("RAS.elog (EMU): exit complete\n");
  return 0;
}

void
micras_mc_log(struct mce_info * event)
{
  if (mce_disabled)
    return;

  /*
   * Print entry on serial console (copy in kernel log)
   */
  ee_printk("RAS.elog (EMU): bnk %d, id %d, ctl %llx, stat %llx, addr %llx, misc %llx\n",
	event->org, event->id, event->ctl, event->status, event->addr, event->misc);
}

#else

/*
**
** Exception safe I2C driver for the 'OverClocking' bus.
** The driver is a derivative of the FreeBSD driver that
** Ben W wrote. I.e. it is safe to re-use here because we
** wrote it in the first place, copyright is ours.
**
** NOTE: This I2C bus is usually run by the PXA driver,
**       which means that the activities of this driver
**	 may interrupt the PXA driver's activity, i.e.
**	 interrupt the serial console.
**	 This is by design, the alternative was major
**	 hacking of the PXA driver to support use in
**	 exception context.
**
** NOTE: This code is currently exclusively designed to
**	 run on a KnF or KnC device, i.e. we know what
**	 hardware is present and we know the location
**	 of the CSRs. This code does very little for
**	 niceties like device discovery and registration.
**
** NOTE: Timing is altered slightly from the FreeBSD code.
**	 The I2C bus should run in 400 kHz mode, which at
**	 optimal conditions can transmit a byte in about
**	 25 uSec (8 bits + ack/nak + a little overhead).
**	 Therefore it does not make much sense to poll
** 	 much faster than 1 uSec anywhere in this driver.
**	 However, experiments show that timing is far
**	 from optimal, though it is not clear whether
**	 it is the UART or the controller that's slow.
**       Update: In fact some of the boards cannot run
**       reliably at 400 kHz, so we switched to 100 kHz.
*/

#define	REG_DBG		0	/* Debug I2C Layer 1 */
#define	I2C_DBG		0	/* Debug I2C Layer 2 */
#define	XFR_DBG		0	/* Debug I2C Layer 3 */
#define	CON_DBG		0	/* Debug I2C UART */
#define	EPR_DBG		0	/* Debug EEPROM log */

#if REG_DBG
#define REG_REG			reg_dmp
#else
#define REG_REG(s);		/* As nothing */
#endif

#if I2C_DBG
#define	I2C_PRT			ee_printk
#else
#define	I2C_PRT(s,...);		/* As nothing */
#endif

#if XFR_DBG
#define	XFR_PRT			ee_printk
#else
#define	XFR_PRT(s,...);		/* As nothing */
#endif

#if CON_DBG
#define	CON_PRT			ee_printk
#else
#define	CON_PRT(s,...);		/* As nothing */
#endif

#if EPR_DBG
#define	EPR_PRT			ee_printk
#else
#define	EPR_PRT(s,...);		/* As nothing */
#endif


#include <mic/micsboxdefine.h>
#include "monahan.h"


/*
 *TBD: Get rid of Pascal relics!
 */

#ifndef FALSE
#define FALSE	false
#endif
#ifndef TRUE
#define TRUE	true
#endif


/*
 * Local timer routine.
 * Similar to the udelay function, just simpler.
 *
 * The delay instruction can only go upto 1023 clocks,
 * and larger delay needs to be split into two or more
 * delay instructions.
 * According to Kn{F|C} errata, delay disables interrupts.
 * Want to play nice and allow interrupts every 250 clocks.
 * For now the overhead of the loop is ignored.
 */

#define MAX_DELAY	250

void
myDELAY(uint64_t usec)
{
  uint64_t	num_cpu_clks, tick;

  /*
   * Convert usec count into CPU clock cycles.
   * Similar to set_cyc2ns_scale() we have:
   *              us = cycles / (freq / us_per_sec)
   *              us = cycles * (us_per_sec / freq)
   *              us = cycles * (10^6 / (cpu_khz * 10^3))
   *              us = cycles * (10^3 / cpu_khz)
   *          cycles = us / ((10^3 / cpu_khz))
   *          cycles = (us * cpu_khz) / 10^3
   */
  num_cpu_clks = (usec * tsc_khz) / 1000;

  if (num_cpu_clks <= MAX_DELAY) {
    __asm__ __volatile__("delay %0"::"r"(num_cpu_clks):"memory");
  } else {
    for(tick = MAX_DELAY; num_cpu_clks > tick; num_cpu_clks -= tick)
      __asm__ __volatile__("delay %0"::"r"(tick):"memory");
    __asm__ __volatile__("delay %0"::"r"(num_cpu_clks):"memory");
  }
}


/*
 * Layer 1 abstraction: device bus (controller register access)
 *
 * Access API to provide read/write to the I2C controller.
 * Simply use a local copy of the SBOX MMIO routines, where the
 * 'OverClocking' I2C controller CSRs starts at offset 0x1000.
 * We use a local copy in order to not mix I2C register traces
 * with those of the SBOX MMIO routines in micras_main.c.
 *
 *TBD: Shall debug features stay in the code?
 */

#if REG_DBG

/*
 * I2C controller register dump utilities.
 * Traces go to the kernel log.
 */

struct bits {
  uint32_t    mask;
  char *set;
  char *unset;
};

#define PXA_BIT(m, s, u)        { .mask = m, .set = s, .unset = u }

static struct bits icr_bits[] = {
  PXA_BIT(ICR_START,  "START",    0),
  PXA_BIT(ICR_STOP,   "STOP",     0),
  PXA_BIT(ICR_ACKNAK, "NAK",     "ACK"),
  PXA_BIT(ICR_TB,     "TB",       0),
  PXA_BIT(ICR_MA,     "MA",       0),
  PXA_BIT(ICR_SCLE,   "SCLE",     0),
  PXA_BIT(ICR_IUE,    "IUE",      0),
  PXA_BIT(ICR_GCD,    "GCD",      0),
  PXA_BIT(ICR_ITEIE,  "ITEIE",    0),
  PXA_BIT(ICR_DRFIE,  "DRFIE",    0),
  PXA_BIT(ICR_BEIE,   "BEIE",     0),
  PXA_BIT(ICR_SSDIE,  "SSDIE",    0),
  PXA_BIT(ICR_ALDIE,  "ALDIE",    0),
  PXA_BIT(ICR_SADIE,  "SADIE",    0),
  PXA_BIT(ICR_UR,     "UR",       0),
};

static struct bits isr_bits[] = {
  PXA_BIT(ISR_RWM,    "RX",     "TX"),
  PXA_BIT(ISR_ACKNAK, "NAK",    "ACK"),
  PXA_BIT(ISR_UB,     "UB",     0),
  PXA_BIT(ISR_IBB,    "IBB",    0),
  PXA_BIT(ISR_SSD,    "SSD",    0),
  PXA_BIT(ISR_ALD,    "ALD",    0),
  PXA_BIT(ISR_ITE,    "ITE",    0),
  PXA_BIT(ISR_IRF,    "IRF",    0),
  PXA_BIT(ISR_GCAD,   "GCAD",   0),
  PXA_BIT(ISR_SAD,    "SAD",    0),
  PXA_BIT(ISR_BED,    "BED",    0),
};


static void
decode_bits(char *prefix, struct bits *bits, int num, uint32_t val)
{
  char	       * str;

  printk("  %s: ", prefix);
  while (num--) {
    str = (val & bits->mask) ? bits->set : bits->unset;
    if (str)
      printk("%s ", str);
    bits++;
  }
}

static void reg_ICR(uint32_t val)
{
  decode_bits("ICR", icr_bits, ARRAY_SIZE(icr_bits), val);
  printk("\n");
}

static void reg_ISR(uint32_t val)
{
  decode_bits("ISR", isr_bits, ARRAY_SIZE(isr_bits), val);
  printk("\n");
}


static void
reg_dmp(char * str)
{
  printk("%s: ICR %08x, ISR %08x, ISAR %08x, IDBR %08x, IBMR %08x\n", str,
  	mr_sbox_rl(0, SBOX_OC_I2C_ICR + ICR_OFFSET),
  	mr_sbox_rl(0, SBOX_OC_I2C_ICR + ISR_OFFSET),
  	mr_sbox_rl(0, SBOX_OC_I2C_ICR + ISAR_OFFSET),
  	mr_sbox_rl(0, SBOX_OC_I2C_ICR + IDBR_OFFSET),
  	mr_sbox_rl(0, SBOX_OC_I2C_ICR + IBMR_OFFSET));
}

#endif /* REG_DBG */


/*
 * Local versions of SBOX access routines, that
 * does not leave trace messages in kernel log.
 */

uint32_t
lmr_sbox_rl(int dummy, uint32_t roff)
{
  uint32_t	val;

  val = * (volatile uint32_t *)(micras_sbox + roff);
  return val;
}

void
lmr_sbox_wl(int dummy, uint32_t roff, uint32_t val)
{
  * (volatile uint32_t *)(micras_sbox + roff) = val;
}

static uint32_t
reg_read(uint32_t reg)
{
  uint32_t	val;

  val = lmr_sbox_rl(0, SBOX_OC_I2C_ICR + reg);

#if REG_DBG
  printk("%s: %4x -> %08x", "rd", SBOX_OC_I2C_ICR + reg, val);
  switch(reg) {
    case ICR_OFFSET:	reg_ICR(val);	break;
    case ISR_OFFSET:	reg_ISR(val);	break;
    default:
      printk("\n");
  }
#endif

  return val;
}

static void
reg_write(uint32_t reg, uint32_t val)
{
#if REG_DBG
  printk("%s: %4x <- %08x", "wr", SBOX_OC_I2C_ICR + reg, val);
  switch(reg) {
    case ICR_OFFSET:	reg_ICR(val);	break;
    default:
      printk("\n");
  }
#endif

  lmr_sbox_wl(0, SBOX_OC_I2C_ICR + reg, val);
}


/*
 * Layer 2 abstraction: I2C bus driver (byte access to I2C bus)
 *
 * Mostly a re-implementation of Ben W's low level FreeBSD driver.
 * Provides an API to control what goes onto the I2C bus on a
 * per individual byte basis.
 *
 *  i2c_reset		Reset bus controller
 *  i2c_init		Setup trasaction parameters (speed & mode)
 *  i2c_start		Send slave address + R/W bit
 *  i2c_rd_byte		Read data byte
 *  i2c_wr_byte		Send data byte
 *  i2c_stop		Stop current transaction
 *
 * NOTE: It seems that the controller lacks means to reset the
 *       I2C bus (i.e. other devices on it). The controller
 *       resets fine, but at least the UART has been seen
 *       locking up and blocking the bus entirely.
 */

static uint8_t		hnd_addr = 0;			/* Target address */
static int		hnd_freq = FREQ_100K;		/* Target speed */

static uint8_t		bus_slave_addr = ISAR_SLADDR;	/* Our I2C slave address */
static int		bus_start_op = I2C_NOP;		/* Bus command: R or W */
static int		bus_freq = 0;			/* Bus speed (actual) */
static int		bus_inited = 0;			/* Bus initialized */


/*
 * Master abort.
 * Flip the ICR:MA bit long enough for current
 * byte transfer to clock in/out on the wire.
 */

static int
i2c_master_abort(void) {
  I2C_PRT("i2c_master_abort: entry\n");

  reg_write(ICR_OFFSET, reg_read(ICR_OFFSET) | ICR_MA);
  myDELAY(25);
  reg_write(ICR_OFFSET, reg_read(ICR_OFFSET) & ~ICR_MA);

  I2C_PRT("i2c_master_abort: exit\n");
  return 0;
}


/*
 * Receive completion helper.
 * Transmission ended (we got IRF), check if it was OK.
 * We get ISR and whether a stop condition was expected.
 */

static int
check_rx_isr(uint32_t isr, bool stop)
{
  I2C_PRT("check_rx_isr: entry, isr %02x, stop %d\n", isr, stop);
  REG_REG("+check_rx_isr");

  if (stop) {
    /*
     * Last byte read, controller is expected to give a
     * NAK to slave. Verify that indeed is set in ISR.
     */
    if (!(isr & ISR_ACKNAK)) {
      REG_REG("-check_rx_isr");
      I2C_PRT("check_rx_isr: !ISR_ACKNAK, rtn %d\n", RX_SEVERE_ERROR);
      return RX_SEVERE_ERROR;
    }

    /*
     * The controller is expected to set the STOP condition.
     * Once completed the controller clears the RWM bit of the ISR.
     * Wait for this to happen in max 200 uSec.
     */
    if (isr & ISR_RWM) {
      int counter;

      I2C_PRT("check_rx_isr: RWM\n");
      counter = 100;
      while((reg_read(ISR_OFFSET) & ISR_RWM) && --counter)
        myDELAY(2);
      if(! counter) {
        REG_REG("-check_rx_isr");
        I2C_PRT("check_rx_isr: timeout, RWM wait %d uSec, rtn %d\n", 2 * 100, RX_BIZARRE_ERROR);
        return RX_BIZARRE_ERROR;
      }
      I2C_PRT("check_rx_isr: RWM clear, waited %d uSec\n", 2 * (100 - counter));
    }
  } else {
    /*
     * Mid-message, verify that unit is still busy, received
     * no NAK and that message operation is still 'read'.
     */
    if (!(isr & ISR_UB)) {
      REG_REG("-check_rx_isr");
      I2C_PRT("check_rx_isr: !UB, rtn %d\n", RX_SEVERE_ERROR);
      return RX_SEVERE_ERROR;
    }

    if (isr & ISR_ACKNAK) {
      REG_REG("-check_rx_isr");
      I2C_PRT("check_rx_isr: ISR_ACKNAK, rtn %d\n", RX_SEVERE_ERROR);
      return RX_SEVERE_ERROR;
    }

    if (!(isr & ISR_RWM)) {
      REG_REG("-check_rx_isr");
      I2C_PRT("check_rx_isr: !ISR_RWM, rtn %d\n", RX_BIZARRE_ERROR);
      return RX_BIZARRE_ERROR;
    }
  }

  REG_REG("-check_rx_isr");
  I2C_PRT("check_rx_isr: done, rtn %d\n", XFER_SUCCESS);
  return XFER_SUCCESS;
}

/*
 * Wait for receive completion.
 * We get if stop condition expected.
 */

static int
i2c_wait_rx_full(bool stop)
{
  int		uwt, counter, err;
  uint32_t	temp;

  I2C_PRT("i2c_wait_rx_full: entry, stop %d\n", stop);
  REG_REG("+i2c_wait_rx_full");

  /*
   * Guess on how long one I2C clock cycle is (in uSec)
   */
  uwt = (bus_freq == FREQ_400K) ? 3 : 10;

  /*
   * Wait for receive to end (IRF set).
   * Since slave can hold the SCL to reduce the speed
   * we wait longer than we expect the receive to last.
   */
  counter = 100;
  err = INCOMPLETE_XFER;
  while(counter) {
    temp = reg_read(ISR_OFFSET);
    if (temp & ISR_IRF) {
      I2C_PRT("i2c_wait_rx_full: IRF, ISR %02x\n", temp);
      err = check_rx_isr(temp, stop);
      reg_write(ISR_OFFSET, reg_read(ISR_OFFSET) | ISR_IRF);
      switch(err) {
        case XFER_SUCCESS:
          break;
	case RX_SEVERE_ERROR:
          break;
        case RX_END_WITHOUT_STOP:
          i2c_master_abort();
          break;
        default:
	  /*
	   * This is odd/unexpected, but not
	   * something we can do anything about.
	   */
          err = XFER_SUCCESS;
      }
      break;
    }
    myDELAY(uwt);
    counter--;
  }

  REG_REG("-i2c_wait_rx_full");
  I2C_PRT("i2c_wait_rx_full: done, IRF wait %d uSec, err %d\n", uwt * (100 - counter), err);
  return err;
}


/*
 * Transmit completion helper.
 * Transmission ended (we got ITE), check if it was OK.
 * We get ISR, the current operation and whether a stop
 * condition was expected (last byte of transmission).
 */

static int
check_tx_isr(uint32_t isr, bool stop, int op)
{
  I2C_PRT("check_tx_isr: entry, isr %02x, stop %d, op %d\n", isr, stop, op);
  REG_REG("+check_tx_isr");

  if (isr & ISR_BED) {	/* Bus error */
    REG_REG("-check_tx_isr");
    I2C_PRT("check_tx_isr: BED, rtn %d\n", TX_NAK);
    return TX_NAK;
  }

  if(stop) {
    /*
     * Last byte write, controller expected to
     * set the stop condition. This may take a
     * while to complete, controller holds the
     * UB flag of ISR until finished.
     */
    if(isr & ISR_UB) {
      int counter;

      I2C_PRT("check_rx_isr: UB\n");
      counter = 100;
      while((reg_read(ISR_OFFSET) & ISR_UB) && --counter)
        myDELAY(2);
      if (! counter) {
        REG_REG("-check_tx_isr");
        I2C_PRT("check_tx_isr: UB, timeout %d uSec, rtn %d\n", 2 * 100, TX_CONTROLLER_ERROR);
        return TX_CONTROLLER_ERROR;
      }
      I2C_PRT("check_tx_isr: !UB, waited %d uSec\n", 2 * (100 - counter));
    }
  } else {
    /*
     * Mid-message, the bus is expected to be busy.
     */
    if(!(isr & ISR_UB)) {
      REG_REG("-check_tx_isr");
      I2C_PRT("check_tx_isr: !UB, rtn %d\n", TX_CONTROLLER_ERROR);
      return TX_CONTROLLER_ERROR;
    }
  }

  /*
   * Assert that message operation hasn't changed
   */
  if ((isr & 0x1) != op) {
    REG_REG("-check_tx_isr");
    I2C_PRT("check_tx_isr: ISR %d != %d, rtn %d\n", isr & 0x1, op, TX_CONTROLLER_ERROR);
    return TX_CONTROLLER_ERROR;
  }

  REG_REG("-check_tx_isr");
  I2C_PRT("check_tx_isr: done, rtn %d\n", XFER_SUCCESS);
  return XFER_SUCCESS;
}

/*
 * Wait for transmit completion
 * We get the current operation and if a stop
 * condition was expected (last byte of transmission).
 */

static int
i2c_wait_tx_empty(bool stop, int op)
{
  int		counter, uwt, err;
  uint32_t	temp;

  I2C_PRT("i2c_wait_tx_empty: entry, stop %d, op %d\n", stop, op);
  REG_REG("+i2c_wait_tx_empty");

  /*
   * Guess on how long one I2C clock cycle is (in uSec)
   */
  uwt = (bus_freq == FREQ_400K) ? 3 : 10;

  /*
   * Wait for transmission to end (ITE set)
   * Since slave can hold the SCL to lower the speed
   * we wait longer than we expect the transmission
   * to last.
   */
  counter = 100;
  err = INCOMPLETE_XFER;
  while(counter) {
    temp = reg_read(ISR_OFFSET);
    if (temp & ISR_ITE) {
      I2C_PRT("i2c_wait_tx_empty: ITE, ISR %02x\n", temp);
      myDELAY(uwt);
      temp = reg_read(ISR_OFFSET);
      err = check_tx_isr(temp, stop, op);
      reg_write(ISR_OFFSET, reg_read(ISR_OFFSET) | ISR_ITE);
      break;
    }
    myDELAY(uwt);
    counter--;
  }

  REG_REG("-i2c_wait_tx_empty");
  I2C_PRT("i2c_wait_tx_empty: done, ITE wait %d uSec, err %d\n", uwt * (100 - counter), err);
  return err;
}


/*
 * Setup for a transaction.
 * Determine transmission speed and program ICR accordingly.
 * Also sets ISAR, though we probably don't neeed that.
 */

static int
i2c_init(uint8_t slave_addr)
{
  uint32_t	speed;

  I2C_PRT("i2c_init: entry, slave_addr %02x, hnd_speed %d\n", slave_addr, hnd_freq);
  REG_REG("+i2c_init");

  switch(hnd_freq) {
    case FREQ_MAX:
      speed = I2C_HS_FAST;
      break;
    case FREQ_400K:
      speed = I2C_FAST;
      break;
    case FREQ_100K:
      speed = I2C_STANDARD;
      break;
    case FREQ_AUTO:
#if I2C_SLOW
      hnd_freq = FREQ_100K;
      speed = I2C_STANDARD;
#else
      hnd_freq = FREQ_400K;
      speed = I2C_FAST;
#endif
      break;
    default:
      return -EINVAL;
  }
  if (bus_inited && hnd_freq == bus_freq) {
    REG_REG("-i2c_init");
    I2C_PRT("i2c_init: exit, bus_inited %d, hnd_freq %d\n", bus_inited, hnd_freq);
    return 0;
  }
  I2C_PRT("i2c_init: speed %d, hnd_freq %d\n", bus_inited, hnd_freq);

  bus_slave_addr = ISAR_SLADDR;
  reg_write(ISAR_OFFSET, bus_slave_addr);
  reg_write(ICR_OFFSET, (reg_read(ICR_OFFSET) & ~ICR_MODE) | ICR_ON | speed);
  bus_freq = hnd_freq;
  bus_inited = 1;

  REG_REG("-i2c_init");
  I2C_PRT("i2c_init: done, bus_inited %d, bus_freq %d\n", bus_inited, bus_freq);
  return 0;
}


/*
 * Stop current transaction.
 * If transmitting then do a master abort, otherwise
 * just ensure that no new transmission starts.
 */

static int
i2c_stop(void)
{
  I2C_PRT("i2c_stop: entry, bus_inited %d, bus_start_op %d\n", bus_inited, bus_start_op);
  REG_REG("+i2c_stop");

  if (reg_read(ISR_OFFSET) & ISR_UB) {
    I2C_PRT("i2c_stop: Unit busy\n");
    i2c_master_abort();
  }

  switch(bus_start_op) {
    case I2C_WRITE:
      I2C_PRT("i2c_stop: Stop Write\n");
      reg_write(ICR_OFFSET, reg_read(ICR_OFFSET) & ~(ICR_STOP | ICR_TB));
      break;
    case I2C_READ:
      I2C_PRT("i2c_stop: Stop Read\n");
      reg_write(ICR_OFFSET, reg_read(ICR_OFFSET) & ~(ICR_STOP | ICR_TB | ICR_ACKNAK));
      break;
  }
  bus_start_op = I2C_NOP;

  REG_REG("-i2c_stop");
  I2C_PRT("i2c_stop: bus_start_op %d\n", bus_start_op);
  return 0;
}


/*
 * Reset I2C controller.
 * Try to be nice and wait for current transaction to finish
 */

static int
i2c_reset(void)
{
  I2C_PRT("i2c_reset: entry, bus_inited %d\n", bus_inited);
  REG_REG("+i2c_reset");

  i2c_stop();

  reg_write(ICR_OFFSET, ICR_UR);
  myDELAY(1);
  reg_write(ISR_OFFSET, ~ISR_RESERVED);
  myDELAY(1);
  reg_write(ICR_OFFSET, 0);
  myDELAY(1);
  reg_write(ISAR_OFFSET, 0);
  myDELAY(1);
  reg_write(ICR_OFFSET, ICR_INIT_BITS);
  bus_inited = 0;

  REG_REG("-i2c_reset");
  I2C_PRT("i2c_reset: exit, bus_inited %d\n", bus_inited);
  return 0;
}


/*
 * Start transaction using current setup.
 * This is always a send of the target id and the R/W bit.
 */

static int
i2c_start(int rw)
{
  int		err;
  uint32_t	temp;

  I2C_PRT("i2c_start: entry, rw %d, bus_slave_addr %02x, bus_start_op %d\n", rw, bus_slave_addr, bus_start_op);
  REG_REG("+i2c_start");

  if (hnd_addr == bus_slave_addr) {
    bus_slave_addr = bus_slave_addr - 1;
    I2C_PRT("i2c_start: reset slave %02x\n", bus_slave_addr);
    reg_write(ISAR_OFFSET, bus_slave_addr);
  }

  reg_write(IDBR_OFFSET, (hnd_addr << 1) | rw);
  temp = reg_read(ICR_OFFSET);
  temp |= ICR_START | ICR_TB;
  temp &= ~(ICR_STOP | ICR_ALDIE);
  reg_write(ISR_OFFSET, ~ISR_RESERVED);
  reg_write(ICR_OFFSET, temp);

  err = i2c_wait_tx_empty(FALSE, rw);
  if (err) {
    i2c_reset();
    I2C_PRT("i2c_start: exit, err %d\n", err);
    REG_REG("-i2c_start");
    return err;
  }
  bus_start_op = rw;

  REG_REG("-i2c_start");
  I2C_PRT("i2c_start: done, bus_start_op %d\n", bus_start_op);
  return 0;
}


/*
 * Read next byte of transaction
 * Must follow a 'start' in READ mode.
 */

static int
i2c_rd_byte(bool sendStop, uint8_t *data)
{
  int		retval;
  uint32_t	temp;

  I2C_PRT("i2c_rd_byte: entry, stop %d\n", sendStop);

  if (bus_start_op != I2C_READ) {
    I2C_PRT("i2c_rd_byte: exit, called during WR\n");
    return -EINVAL;
  }

  REG_REG("+i2c_rd_byte");

  temp = reg_read(ICR_OFFSET);
  temp |= (ICR_ALDIE | ICR_TB);
  temp &= ~(ICR_START | ICR_STOP | ICR_ACKNAK);
  if (sendStop)
    temp |= ICR_STOP | ICR_ACKNAK;

  reg_write(ISR_OFFSET, ~ISR_RESERVED);
  reg_write(ICR_OFFSET, temp);
  retval = i2c_wait_rx_full(sendStop);
  if (retval) {
    REG_REG("-i2c_rd_byte");
    I2C_PRT("i2c_rd_byte: exit, err %d\n", retval);
    return retval;
  }

  temp = reg_read(IDBR_OFFSET);
  if (data)
    *data = temp;

  if (sendStop)
    i2c_stop();

  REG_REG("-i2c_rd_byte");
  I2C_PRT("i2c_rd_byte: done, data %02x\n", temp);
  return 0;
}

/*
 * Write next byte of transaction
 * Must follow a 'start' in WRITE mode.
 */

static int
i2c_wr_byte(bool sendStop, uint8_t data)
{
  int		retval;
  uint32_t	temp;

  I2C_PRT("i2c_wr_byte: entry, stop %d, data %02x\n", sendStop, data);

  if (bus_start_op != I2C_WRITE) {
    I2C_PRT("i2c_wr_byte: exit, called during RD\n");
    return EINVAL;
  }
 
  REG_REG("+i2c_wr_byte");

  reg_write(IDBR_OFFSET, data);

  temp = reg_read(ICR_OFFSET);
  temp |= (ICR_ALDIE | ICR_TB);
  temp &= ~(ICR_START | ICR_STOP);
  if (sendStop)
    temp |= ICR_STOP;

  reg_write(ISR_OFFSET, ~ISR_RESERVED);
  reg_write(ICR_OFFSET, temp);
  retval = i2c_wait_tx_empty(sendStop, I2C_WRITE);
  if (retval) {
    REG_REG("-i2c_wr_byte");
    I2C_PRT("i2c_wr_byte: exit, err %d\n", retval);
    return retval;
  }

  if (sendStop)
    i2c_stop();

  REG_REG("-i2c_wr_byte");
  I2C_PRT("i2c_wr_byte: done\n");
  return 0;
}


/*
 * Get exclusive access to the I2C bus at _any_ given time.
 *
 * If a transaction is in progress then try to complete it
 * in a non-destructive way.  We know that the interupted
 * activity was from the console access to the UART, which
 * boils down to just two possible sequences, read UART
 * register or write UART register. The acting code paths is
 *  sc16is_serial_in()
 *  -> i2c_smbus_read_byte_data
 *     -> i2c_smbus_xfer
 *        -> i2c_smbus_xfer_emulated
 *           -> i2c_transfer
 *              -> i2c_pxa_pio_xfer
 *		   -> i2c_pxa_do_pio_xfer
 *		      -> i2c_pxa_set_master
 *		      -> i2c_pxa_start_message
 *		      -> i2c_pxa_handler (repeat for all bytes)
 *		         -> i2c_pxa_irq_txempty (on writes)
 *                       -> i2c_pxa_irq_rxfull (on reads)
 *		      -> i2c_pxa_stop_message
 *		    
 * Function i2c_pxa_handler (designed as an interrupt handler)
 * is polled every 10 uSec, which is pretty fast for a line that
 * clocks at 400 kHz (minimum 20 uSec to send one byte).
 *
 * The two sequences on the I2C bus for the UART are:
 *
 *  Write:  S <addr | W> A <reg> A <data byte> A P
 *  Read:   S <addr | W> A <reg> A Sr <addr | R> A <data byte> A P
 *
 * where
 *   S	Start sequence
 *   P  Stop sequence
 *   Sr	Repeated start
 *   W  Write flag
 *   R  Read flag
 *   A	Ack (send or recv)
 *
 * We need the abilitity to 'borrow' the I2C bus from the PXA driver
 * both when it is running (say on another CPU) or when it has been
 * interrupted (NMI and Exception context).
 *
 * From trackers in the PXA driver we get to know the current state
 * of the I2C transaction with the following granularity:
 *
 *  '-' Idle
 *  'B' Waiting for bus free
 *  'I'	Initiating transfer (i.e. send addr & direction flag)
 *  'S' Sending byte
 *  'R' Receving byte
 *
 * Last byte of the transaction can be identified by the STOP flag.
 *
 * The take-over sequence starts by setting an atomic variable which
 * tells the PXA driver to wait (and retry the I2C transaction when
 * the variable gets cleared). Then we look at the controller status
 * and command registers to determine whether it is active or not.
 *
 * Simple cases:
 * -------------
 *  state = '-'
 *	Controller is not in use by PXA driver.
 *
 *  state 'B'
 *	Controller not actively in use yet.
 *	At worst the SCLE bit will be set, which won't affect
 *      anything in this driver since we always run as master.
 *
 *  STOP bit set
 *	This is last byte of a transaction, we have two cases:
 *      a) Last part of a write UART register transaction.
 *     	   - Wait for the byte to clock out
 *      b) Last part of a read UART register transaction.
 *	   - Wait for the byte to clock in, then preserve IDBR.
 *
 * Other cases:
 * ------------
 *  state 'I'
 *      Starting an I2C command (Start or Start-Repeat),
 *      we have 3 sub-cases of this:
 *	a) Starting a write UART register transaction:
 *	   - Wait for the byte to clock out, then transmit a
 *	     0 byte with STOP bit set. This selects RX/TX
 *	     UART register without accessing it.
 *      b) Starting a read UART register transaction:
 *	   - Same as case a), turn it into a NOP.
 *	c) Reversing direction during read UART register,
 *	   probably need to finish the read operation:
 *	   - Wait for the byte to clock out, send STOP + ACK
 *	     and wait for the receive to clock in.
 *
 *  state 'S'
 *	Since STOP bit is not set, then this is the <reg>
 *      index being transfered, two sub-cases:
 *      a) Sending <reg> of a write UART register.
 *	   - Wait for the byte to clock out, then transmit a
 *	     0 byte with the STOP bit set. This inadvertantly
 *	     and temporarily clears a random UART register,
 *	     which may result in a null byte transmitted
 *	     Since there is a retry associated, the intended
 *	     register value will be written later.
 *      b) Sending <reg> of a read UART register.
 *	   - Same as state 'I' case c).
 *    
 *  state 'R'
 *	Should not occur, because communications with the
 *	UART only have single byte reads, which always is
 *	accompanied by a STOP bit, and thus is covered by
 *      the simple case above. If multi-byte reads were to
 *	be used then we'd have to terminate it:
 *	- Wait for the byte to clock in, send STOP + ACK
 *	  and wait for the 2nd byte to clock in.
 *	  Both bytes received can be discarded, as there
 *	  is no easy way to pass them to the PXA driver.
 *
 * Warning:
 *  Beyond this being an ugly hack, it is also not re-entrant.
 *  It can reliably interrupt the console and return it without
 *  causing too much breakage, but it cannot grab the I2C bus
 *  from itself due to the use of global variables.
 *
 * Warning:
 *  The synchronization between i2c_grap/i2c_release and the
 *  PXA driver can still wreck the I2C controller. Cause not
 *  known, but when it happens the PXA driver ends up repeating
 *  these log messages:
 *    i2c: error: pxa_pio_set_master: timeout
 *    i2c: msg_num: 0 msg_idx: 1 msg_ptr: 0
 *    i2c: ICR: 000017e0 ISR: 00000044
 *    i2c: log: [000000c6:000017e0:00:9a] 
 *    i2c i2c-0: i2c_pxa: timeout waiting for bus free
 *    pxa_do_pio_xfer: timeout to become master
 *    pxa_pio_set_master 'B': ISR 00044, ICR 7e0, IDBR 28, IBMR 1
 *  Looks like the I2C controller gets stuck, ISR: IRF + IBB,
 *  The code failing is i2c_pxa_pio_set_master(), which points
 *  to the I2C UART as the culprit. One such case was during
 *  module load on KnF, where the only activity in the module
 *  was one ee_lock/ee_release pair, which in state 'B' should
 *  be straight forward to handle. 
 */

#ifdef CONFIG_I2C_PXA
#define PXA_SYNC	1
#else
#define PXA_SYNC	0
#endif

#if PXA_SYNC
static uint32_t		sv_icr, sv_isr, sv_isar, sv_idbr, ee_term;
extern char		pxa_state;
extern atomic_t		pxa_block;
#endif

static void
i2c_grab(void)
{
  int		uwt, n;
  uint32_t	icr, isr;
  char	      * w;

  I2C_PRT("i2c_grab: entry\n");
  REG_REG("+i2c_grab");

#if PXA_SYNC
        sv_isar = reg_read(ISAR_OFFSET);
	sv_idbr = reg_read(IDBR_OFFSET);
        sv_icr  = reg_read(ICR_OFFSET);
  isr = sv_isr  = reg_read(ISR_OFFSET);
  if ((pxa_state == '-' || pxa_state == 'B') && !(isr & ISR_UB)) {
    REG_REG("-i2c_grab");
    I2C_PRT("i2c_grab: controller idle, isr %08x\n", isr);
    return;
  }
  ee_term = 1;
  I2C_PRT("i2c_grab: controller active, pxa %c\n", pxa_state);
#else
  isr = reg_read(ISR_OFFSET);
  if (!(isr & ISR_UB)) {
    REG_REG("-i2c_grab");
    I2C_PRT("i2c_grab: controller idle, isr %08x\n", isr);
    return;
  }
  I2C_PRT("i2c_grab: controller active\n");
  w = "-";
#endif

  /*
   * Guess on how long one I2C clock cycle is (in uSec)
   * Note: ignore High-Speed modes, they are not used.
   */
  icr = reg_read(ICR_OFFSET);
  uwt = (icr & ICR_FAST_MODE) ? 3 : 10;

  /*
   * Wait here long enough that current byte transaction
   * on the I2C controller must have clocked all on its bus.
   * Imperically, we've determined that length of this wait
   * can to be in range up to a dozen I2C clocks.
   * We probe state once per I2C clock cycle.
   */
  for(n = 0; n < 100 && (isr & ISR_UB); n++) {
    /*
     * Controller busy doing something. Whatever it is
     * doing, it should set either ITE or IRF when done.
     * Need to check for this independently because UB
     * is asserted all the way from START thru STOP.
     */
    if (isr & (ISR_ITE | ISR_IRF))
      break;
    myDELAY(uwt);
    isr = reg_read(ISR_OFFSET);
  }
  I2C_PRT("i2c_grab: ITE/IRF wait %d uSec, isr %02x, UB %d\n",
  		n * uwt, isr, (isr & ISR_UB) == ISR_UB);

  /*
   * Controller should have finished current byte transfer by now.
   * If it was last byte of a transaction, we are done.
   * In read mode we preserve the received data.
   */
  if (icr & ICR_STOP) {
#if PXA_SYNC
    if (isr & ISR_RWM)
      sv_idbr = reg_read(IDBR_OFFSET);
#endif
    for(n = 0; n < 100 && (isr & ISR_UB); n++) {
      myDELAY(uwt);
      isr = reg_read(ISR_OFFSET);
    }
    
    REG_REG("-i2c_grab");
    I2C_PRT("i2c_grab: easy case, UB wait %d uSec, bus %sclear, icr %08x, isr %08x\n",
    		n * uwt, (isr & ISR_UB) ? "NOT " : "", icr, isr);
    return;
  }

#if PXA_SYNC
  w = "?";

  if (pxa_state == 'I') {
    isr &= ~ISR_INTS;
    reg_write(ISR_OFFSET, isr);

    if (isr & ISR_RWM) {
      /*
       * Sub-case c)
       * Start byte read and send nak+stop when received.
       */
      I2C_PRT("i2c_grab: state 'I', sub-case c\n");
      icr = (icr & ~ICR_START) | (ICR_STOP | ICR_ACKNAK | ICR_TB);
      reg_write(ICR_OFFSET, icr);
      w = "c";
    }
    else {
      /*
       * Sub-case a) and b)
       * Send a null byte and stop the transaction.
       */
      I2C_PRT("i2c_grab: state 'I', sub-case a & b\n");
      icr = (icr & ~ICR_START) | (ICR_STOP | ICR_TB);
      reg_write(IDBR_OFFSET, 0);
      reg_write(ICR_OFFSET, icr);
      w = "a & b";
    }

    myDELAY(8 * uwt);
    isr = reg_read(ISR_OFFSET);
    for(n = 0; n < 100 && (isr & ISR_UB); n++) {
      myDELAY(uwt);
      isr = reg_read(ISR_OFFSET);
    }
    if (*w == 'c')
      sv_idbr = reg_read(IDBR_OFFSET);
  }

  if (pxa_state == 'S') {
    isr &= ~ISR_INTS;
    reg_write(ISR_OFFSET, isr);

    if (isr & ISR_RWM) {
      I2C_PRT("i2c_grab: state 'S', sub-case b\n");
      icr = (icr & ~ICR_START) | (ICR_STOP | ICR_ACKNAK | ICR_TB);
      reg_write(ICR_OFFSET, icr);
      w = "b";
    }
    else {
      I2C_PRT("i2c_grab: state 'S', sub-case a\n");
      icr = (icr & ~ICR_START) | (ICR_STOP | ICR_TB);
      reg_write(IDBR_OFFSET, 0);
      reg_write(ICR_OFFSET, icr);
      w = "a";
    }

    myDELAY(8 * uwt);
    isr = reg_read(ISR_OFFSET);
    for(n = 0; n < 100 && (isr & ISR_UB); n++) {
      myDELAY(uwt);
      isr = reg_read(ISR_OFFSET);
    }
    if (*w == 'b')
      sv_idbr = reg_read(IDBR_OFFSET);
  }
#endif	/* PXA_SYNC */

  REG_REG("-i2c_grab");
  I2C_PRT("i2c_grab: controller %sclear, icr %08x, isr %08x, w %s\n",
  		(isr & ISR_UB) ? "NOT " : "", icr, isr, w);
}

static void
i2c_release(void)
{
  I2C_PRT("i2c_release: entry\n");
  REG_REG("+i2c_release");

#if PXA_SYNC
#if 0
  /*
   * Reset I2C controller before returning it to PXA driver
   *TBD: Usually not necessary, remove?
   */
  if (ee_term) {
    I2C_PRT("i2c_release: resetting bus\n");
    reg_write(ICR_OFFSET, ICR_UR);
    myDELAY(2);
    reg_write(ICR_OFFSET, 0);
  }
#endif

  I2C_PRT("i2c_release: restore controller state\n");
  reg_write(ISR_OFFSET, sv_isr);
  reg_write(ICR_OFFSET, sv_icr & ~ICR_TB);
  reg_write(ISAR_OFFSET, sv_isar);
  reg_write(IDBR_OFFSET, sv_idbr);

  if (ee_term)
    ee_term = 0;
#endif	/* PXA_SYNC */

  if (reg_read(IBMR_OFFSET) != 3)
    I2C_PRT("i2c_release: WARNING: bus active!!!\n");

  REG_REG("-i2c_release");
  I2C_PRT("i2c_release: exit\n");
}


/*
 * Layer 3 abstraction: I2C driver API (message passing).
 *
 * Controls data transfers to/from devices on the I2C bus.
 * This is what device drivers should use.
 *
 *   xfr_configure	Set target address and speed
 *   xfr_start		Start R/W operation
 *   xfr_write		Write buffer to target
 *   xfr_read		Read buffer from target
 *   xfr_rept_start	Repeat-start new R/W operation
 *   xfr_reset		Reset driver
 */

static int
xfr_configure(uint8_t addr, int freq)
{
  XFR_PRT("xfr_configure: entry, addr %02x, freq %d\n", addr, freq);

  if (freq > FREQ_AUTO || freq <= FREQ_MAX) {
    XFR_PRT("xfr_configure: exit, invalid freq\n");
    return -EINVAL;
  }

  if (addr & 0x80) {
    XFR_PRT("xfr_configure: exit, invalid addr\n");
    return -EINVAL;
  }

  hnd_addr = addr;
  hnd_freq = freq;
  XFR_PRT("xfr_configure: done, hnd_addr %02x, hnd_freq %d\n", hnd_addr, hnd_freq);
  return 0;
}


static int
xfr_start(int rw)
{
  int		err;

  XFR_PRT("xfr_start: entry, rw %d, hnd_addr %02x\n", rw, hnd_addr);

  if (rw != I2C_WRITE && rw != I2C_READ) {
    XFR_PRT("xfr_start: exit, op invalid\n");
    return -EINVAL;
  }

  if (hnd_addr & 0x80) {
    XFR_PRT("xfr_start: exit, hnd_addr %02x invalid\n", hnd_addr);
    return -EINVAL;
  }

  err = i2c_init(hnd_addr);
  if (err) {
    XFR_PRT("xfr_start: i2c_init failed, err %d\n", err);
    i2c_reset();
    return -EIO;
  }

  err = i2c_start(rw);
  if (err)
    XFR_PRT("xfr_start: i2c_start failed, err %d\n", err);
  switch(err) {
    case INCOMPLETE_XFER:
      i2c_stop();
      err = -EBUSY;
      break;
    case TX_CONTROLLER_ERROR:
      i2c_reset();
      err = -ENODEV;
      break;
    case TX_NAK:
      i2c_stop();
      err = -ENXIO;
      break;
  }

  XFR_PRT("xfr_start: done, err %d\n", err);
  return err;
}


static int
xfr_rept_start(int rw)
{
  int		err;

  XFR_PRT("xfr_rept_start: entry, rw %d, bus_start_op %d\n", rw, bus_start_op);

  if (bus_start_op != I2C_READ && bus_start_op != I2C_WRITE) {
    XFR_PRT("xfr_rept_start: exit, mode change %d\n", -ENXIO);
    return -ENXIO;
  }

  err = i2c_start(rw);
  if (err)
    XFR_PRT("xfr_rept_start: i2c_start err %d\n", err);
  switch(err) {
    case INCOMPLETE_XFER:
      i2c_stop();
      err = -EBUSY;
      break;
    case TX_CONTROLLER_ERROR:
      i2c_reset();
      err = -ENODEV;
      break;
    case TX_NAK:
      i2c_stop();
      err = -ENXIO;
      break;
  }

  XFR_PRT("xfr_rept_start: done, err %d\n", err);
  return err;
}


static int
xfr_write(bool sendStop, int cnt, uint8_t *data)
{
  int		retval, i;

  XFR_PRT("xfr_write: entry, sendStop %d, cnt %d\n", sendStop, cnt);

  if (cnt < 0) {
    XFR_PRT("xfr_write: exit, bad count %d\n", cnt);
    return -EINVAL;
  }

  if (! cnt) {
    XFR_PRT("xfr_write: null write\n");
    retval = i2c_stop();
    goto out;
  }

  if (cnt == 1) {
    XFR_PRT("xfr_write: 1-byte write, '%02x'\n", *data);
    retval = i2c_wr_byte(sendStop, *data);
    goto out;
  }

  for (i = 0; i < cnt - 1; i++) {
    XFR_PRT("xfr_write: multi-byte write %d, '%02x'\n", i, data[i]);
    retval = i2c_wr_byte(FALSE, data[i]);
    if (retval)
      goto out;
  }

  XFR_PRT("xfr_write: last of multi-byte write %d, '%02x'\n", cnt - 1, data[cnt - 1]);
  retval = i2c_wr_byte(sendStop, data[cnt - 1]);

out:
  if (retval)
    XFR_PRT("xfr_write: post val %d\n", retval);
  switch(retval) {
    case INCOMPLETE_XFER:
    i2c_stop();
    retval = -EBUSY;
    break;
  case TX_CONTROLLER_ERROR:
    i2c_reset();
    retval = -ENODEV;
    break;
  case TX_NAK:
    i2c_stop();
    retval = -ENXIO;
    break;
  }

  XFR_PRT("xfr_write: done, val %d\n", retval);
  return retval;
}


static int
xfr_read(bool sendStop, int cnt, uint8_t *data)
{
  int		retval, i;

  XFR_PRT("xfr_read: entry, stop %d, cnt %d\n", sendStop, cnt);

  if (cnt < 0) {
    XFR_PRT("xfr_read: exit, bad count %d\n", cnt);
    return -EINVAL;
  }

  if (! cnt) {
    XFR_PRT("xfr_read: null read\n");
    retval = i2c_stop();
    goto out;
  }

  if (cnt == 1) {
    XFR_PRT("xfr_read: 1-byte read\n");
    retval = i2c_rd_byte(sendStop, data);
    goto out;
  }
 
  for (i = 0; i < cnt - 1; i++) {
    XFR_PRT("xfr_read: multi-byte read %d\n", i);
    retval = i2c_rd_byte(FALSE, data ? &data[i] : data);
    if (retval)
      goto out;
  }

  XFR_PRT("xfr_read: last of multi-byte read %d\n", cnt - 1);
  retval = i2c_rd_byte(sendStop, data ? &data[cnt - 1] : data);

out:
  if (retval) {
    XFR_PRT("xfr_read: post val %d\n", retval);
    i2c_reset();
    retval = -ENXIO;
  }

  XFR_PRT("xfr_read: done, err %d\n", retval);
  return retval;
}


#if NOT_YET
static void
xfr_reset(void)
{
  i2c_reset();
}
#endif



/*
**
** UART support for printing from exception context.
** A somewhat crude implementation of two low level
** routines that write/read CSRs on the I2C UART.
** On top of these two functions, a set of mid-layer
** routines adds init/exit and character based I/O.
** We try not to alter the UART's transmission setup
** in order lower the risk of corrupting normal use.
**
** All UART support routines assume I2C controller
** to be initialized by xfr_configure() and expects
** exclusive access to the device
**
*/


/*
 * Weird way to say that the I2C UART has slave address
 * 0x4D (or 0x48) and the UART registers are in bits
 * [6:3] of the register address byte.
 * KnF has both I2C UART address pins wired to Vss.
 * KnC MPI has the address pins wired to Vdd instead.
 *TBD: That's according to the schematics, in reality
 *     on A0 CRBs the address of the onboard UART is
 *     0x4D, which matches address pins wired to Vss.
 *     Not sure why that changed.
 */

#ifdef CONFIG_ML1OM
#define SC16IS_ADDR_0	1
#define SC16IS_ADDR_1	1
#endif
#ifdef CONFIG_MK1OM	/* KAA: MPI specific or KnC specific ? */
#define SC16IS_ADDR_0	1
#define SC16IS_ADDR_1	1
#endif
#define SC16IS_ADDR(a1, a0) \
		(0x40 | (((a1 + 8) + (a1 * 3))  | a0))
#define SC16IS_SUBADDR(addr, ch) \
		((addr & 0xf) << 3) | ((ch  & 3) << 1)


static uint8_t
cons_getreg(int reg)
{
  uint8_t	sub, val;
  int		err;

  CON_PRT("cons_getreg: reg %02x\n", reg);

  /*
   * The SC16IS740 device reads 8-bit UART registers
   * by first writing the register index and then in
   * an subsequent read operation gets the register
   * value. The two operations can (and probably
   * should) be joined by a repeated start to save
   * the intermediate stop signaling.
   */
  val = 0;
  sub = (uint8_t) SC16IS_SUBADDR(reg, 0);
  err = xfr_start(I2C_WRITE);
  if (err) {
    CON_PRT("cons_getreg: xfr_start (WR) err %d\n", err);
    return 0;
  }
  err = xfr_write(FALSE, 1, &sub);
  if (err) {
    CON_PRT("cons_getreg: xfr_write (%02x) err %d\n", sub, err);
    return 0;
  }
  err = xfr_rept_start(I2C_READ);
  if (err) {
    CON_PRT("cons_getreg: xfr_rept_start (RD) err %d\n", err);
    return 0;
  }
  err = xfr_read(TRUE, 1, &val);
  if (err) {
    CON_PRT("cons_getreg: xfr_read err %d\n", err);
    return 0;
  }

  CON_PRT("cons_getreg: reg %02x, val %02x\n", reg, val);
  return val;
}


static void
cons_setreg(int reg, int val)
{
  uint8_t	payload[2];
  int		err;

  CON_PRT("cons_setreg: reg %02x, val %02x\n", reg, val);

  payload[0] = (uint8_t) SC16IS_SUBADDR(reg, 0);
  payload[1] = (uint8_t) val;
  CON_PRT("cons_setreg: I2C payload %02x, %02x\n", payload[0], payload[1]);
  err = xfr_start(I2C_WRITE);
  if (err) {
    CON_PRT("cons_setreg: xfr_start (WR) err %d\n", err);
    return;
  }
  err = xfr_write(TRUE, 2, payload);
  if (err)
    CON_PRT("cons_getreg: xfr_write (%02x, %02x) err %d\n", payload[0], payload[1], err);
}


static void
cons_init(void)
{
  /*
   * For now assume that the kernel LXA driver or the
   * bootstrap code has setup the I2C uart properly, i.e.
   * we don't need to alter speed/databits/stopbits/parity
   * or any other serial properties.
   *
   *WARNING: Since the switch of console from the I2C uart to
   *	     the virtual console, the uart is left with default
   *	     serial port speed of 9600 baud. Bootstrap blasts
   *         it's messages at 115200 baud, so now the choice
   *         of getting garbage from this routine or from the
   *         bootstrap. Using program stty from userspace may
   *	     set any baudrate, we cannot override it here!
   *	      # stty 115200 < /dev/ttyS0
   *TBD:     make 115200 baud default on I2C uart!
   */
  CON_PRT("cons_init: pass\n");
}


static void
cons_exit(void)
{
  CON_PRT("cons_exit: pass\n");
}


#if NOT_YET
static int
cons_rxrdy(void)
{
  int		val;

  CON_PRT("cons_rxrdy: check console RxRdy\n");

  val = (cons_getreg(UART_LSR) & UART_LSR_DR) ? 1 : 0;

  CON_PRT("cons_rxrdy: RxRdy %d\n", val);
  return val;
}


static int
cons_getc(void)
{
  int		c;

  CON_PRT("cons_getc: rd from console\n");

  while((cons_getreg(UART_LSR) & UART_LSR_DR) == 0)
    myDELAY(1000);
  c = cons_getreg(UART_RX);

  CON_PRT("cons_getc: read '%02x'\n", c);
  return c;
}
#endif


static void
cons_putc(int c)
{
  int		limit;

  CON_PRT("cons_putc: wr '%02x' to console\n", c);

  limit = 10;
  while((cons_getreg(UART_LSR) & UART_LSR_THRE) == 0 && --limit) ;
  CON_PRT("cons_putc: THRE ready, limit %d\n", limit);
  cons_setreg(UART_TX, c);

#if 0
  /*
   * No reason to wait for it to clock out
   */
  limit = 10;
  while((cons_getreg(UART_LSR) & UART_LSR_TEMT) == 0 && --limit) ;
  CON_PRT("cons_putc: TEMT ready, limit %d\n", limit);
#endif

  CON_PRT("cons_putc: done printing '%02x'\n", c);
}


/*
 * Simple exclusive access method for the 'OverClock' I2C bus.
 * The POST-card UART is the only known other party using this
 * bus under normal circumstances (because it is the console).
 * If the POST-card UART is built into the kernel, the lock is
 * in file 'drivers/serial/8250_sc16is7xx.c'. Otherwise the lock
 * is local to the RAS module.
 *
 * Warning:
 *  This locking works perfectly in standard contexts and in
 *  the MCA handling contexts. However, they do not mix safely.
 *  If the ee_lock is taken from standard context, then an
 *  MCA event may hang because it cannot get the lock, ever!
 *  This can happen when/if ee_print() is used.
 */

#ifdef CONFIG_I2C_PXA
extern atomic_t pxa_block;
extern char	pxa_state;
#else
atomic_t pxa_block = ATOMIC_INIT(0);
char	 pxa_state = '-';
#endif

static void
ee_lock(void)
{
  /*
   * Wait here until lock ackquired
   */
  while(atomic_xchg(&pxa_block, 1))
    myDELAY(50);

  /*
   * Lock taken, I2C transaction could be underway.
   * Wait for it to end or forcefully terminate it.
   */
  i2c_grab();
}

static void
ee_unlock(void)
{
  i2c_release();
  atomic_xchg(&pxa_block, 0);
}


/*
 * Printf to the POST card UART.
 *
 * Function ee_printk() and ee_print() both creates
 * a message into a local buffer from where the RAS
 * timer will synch them into the kernel log about
 * once a second. ee_printk() is thread safe.
 *
 * Function ee_print() will also attempt to write to
 * the POST card serial port, which may be useful
 * from exception context where OS services are out
 * of the question.
 *
 * WARNING: ee_print() takes the same lock as
 * the machine checks does, so if a machine check
 * happens while a standard context thread are in
 * this code we'll have an instant kernel hang.
 */

char ee_buf[EE_BUF_COUNT * EE_BUF_LINELEN];
atomic_t ee_msg = ATOMIC_INIT(-1);
atomic_t ee_seen = ATOMIC_INIT(-1);
int  ee_rdy;

#define EE_TSC		0	/* 1 to get rdtsc() included */

char *
ee_fmt(char * fmt, va_list args)
{
  char	      * buf;
  int		msg_id, tsl;
#if EE_TSC
  uint64_t	ts = rdtsc();
#endif

  msg_id = atomic_inc_return(&ee_msg);
  buf = ee_buf + (msg_id % EE_BUF_COUNT) * EE_BUF_LINELEN;
  if (! *buf) {
#if EE_TSC
    tsl = snprintf(buf, EE_BUF_LINELEN - 1, "[%lld] ", ts);
#else
    tsl = 0;
#endif
    vsnprintf(buf + tsl, EE_BUF_LINELEN - 1 - tsl, fmt, args);
    return buf;
  }
  return 0;
}

int
ee_printk(char * fmt, ...)
{
  va_list       args;
  char	      * buf;

  va_start(args, fmt);
  buf = ee_fmt(fmt, args);
  va_end(args);

  return buf ? strlen(buf) : 0;
}

int
ee_print(char * fmt, ...)
{
  char 		ch, * buf;
  va_list	args;
  int		len;

  va_start(args, fmt);
  buf = ee_fmt(fmt, args);
  va_end(args);

  len = 0;
  if (ee_rdy && buf) {
    /*
     * Get I2C bus exclusive access,
     * setup for targeting the UART and
     * send string one byte at a time
     * with lf -> lr/cr translation.
     */
    ee_lock();
    xfr_configure(SC16IS_ADDR(SC16IS_ADDR_1, SC16IS_ADDR_0), FREQ_AUTO);
    while((ch = *(buf++))) {
      if (ch == '\n') {
        cons_putc('\r');
        len++;
      }
      cons_putc(ch);
      len++;
    }
    ee_unlock();
  }

  return len;
}
EXPORT_SYMBOL_GPL(ee_print);



/*
**
** EEPROM support routines
**
** The device is a 1 Mbit Atmel AT24C1024 which has 128
** KByte addressable storage over 2 slave addresses.
** Lower 64 KB is at slave address 0x54 and upper
** 64KB is at slave address 0x55, i.e. it uses LSB of
** the slave address as bit 16 of the byte address.
**
** All EEPROM support routines assume I2C controller
** to be initialized by xfr_configure() and expects
** exclusive access to the device
**
** Only KnC has this storage
*/

#ifdef CONFIG_MK1OM

#define MR_ELOG_SIZE		(128 * 1024)	/* 1 Mbit */
#define MR_ELOG_ADDR_LO		0x54   		/* Lo 64K slave */
#define MR_ELOG_ADDR_HI		0x55   		/* Hi 64K slave */
#define EE_PG_SIZ		256		/* Device page size */


/*
 * Layout of the EEPROM is roughly like this:
 *
 *  Bytes	Content
 *   0 - 15	Fixed log header
 *  16 - 17	Log head index (last written)
 *  18 - 19	Log tail index (last read)
 *  20 - end	Log entries
 *
 * By definition, the log is fully read when head and
 * tail pointer are equal (initial value: last entry).
 * The effective log size is
 *  (device_size - sizeof(McaHeader))/sizeof(McaRecord).
 *
 * Fields of interest in the log entry 'id' are
 *  bits  7:0		Source index, 8 bit
 *  bits 18:16		Source type, 3 bit
 *  bits 22:22		Injected error flag
 *  bits 23:23		Repaired flag
 *  bits 24:24		Filtered flag
 *  bits 31:31		Valid flag
 *
 * Enumeration details are in file micras_mca.h
 *
 * Time stamps in the MCA header and event records are supposed to be
 * standard 32-bit Unix format, i.e.  seconds since 00:00 Jan 1 1979 GMT.
 * This will wrap some time Jan 19th 2038, which is about 25 years from
 * the release of KnC. Given the use of 386's (introduced 1985) in the
 * modern data center anno '12, 32 bit will last for all practical purposes.
 */

typedef struct _mca_header {
  uint8_t	signature[8];	/* Magic */
  uint8_t	header_ver;	/* Format revision */
  uint8_t	rec_start;	/* Offset of 1st record */
  uint16_t	rec_size;	/* Size of an MCA record */
  uint16_t	entries;	/* Log size */
  uint8_t	logfull;	/* Log has wrapped (reserved) */
  uint8_t	hwtype;		/* Board type (reserved) */
  uint16_t	rec_head;	/* Head index */
  uint16_t	rec_tail;	/* Tail index */
} McaHeader;

typedef struct _mca_record {
  uint32_t	id;		/* Event origin & flags */
  uint32_t	stamp;		/* Low 32 bit of system time */
  uint64_t	ctl;            /* MCA bank register 'CTL' */
  uint64_t	status;         /* MCA bank register 'STATUS' */
  uint64_t	addr;           /* MCA bank register 'ADDR' */
  uint64_t	misc;           /* MCA bank register 'MISC' */
} McaRecord;


/*
 * Header to drop onto un-initalized EEPROM
 * By definition, the EEPROM is uninitialised
 * if the magic signature is wrong.
 */

#define MR_ELOG_NUM	(MR_ELOG_SIZE - sizeof(McaHeader))/sizeof(McaRecord)

static McaHeader elog_preset = {
  .signature = {"MCA_LOG"},
  .header_ver = 1,
  .rec_start = sizeof(McaHeader),
  .rec_size = sizeof(McaRecord),
  .entries = MR_ELOG_NUM,
  .logfull = -1,
  .hwtype = 0,
  .rec_head = MR_ELOG_NUM - 1,
  .rec_tail = MR_ELOG_NUM - 1,
};

static uint16_t	ee_num, ee_head, ee_tail;	/* Cached log state */


#if EPR_DBG || EE_VERIFY
/*
 * Printk from EEPROM code.
 * We have the lock, and the I2C target address is
 * set for the Atmel device, we must reset I2C for
 * the UART on every entry, and reset it back to the
 * EEPROM in order to keep this function transparent.
 *
 * Warning: this call is highly risky, particularly
 * in error conditions where the I2C bus is involved.
 * Do not call it during an EEPROM I2C transaction!!
 * Use for internal debug _ONLY_ and at own risk.
 */

int
elog_print(char * fmt, ...)
{
  char	      * buf, ch;
  va_list	args;
  int		len;

  va_start(args, fmt);
  buf = ee_fmt(fmt, args);
  va_end(args);

  if (! buf)
    return 0;

  xfr_configure(SC16IS_ADDR(SC16IS_ADDR_1, SC16IS_ADDR_0), FREQ_AUTO);

  len = 0;
  while((ch = *(buf++))) {
    if (ch == '\n') {
      cons_putc('\r');
      len++;
    }
    cons_putc(ch);
    len++;
  }

  return len;
}
#endif /* EPR_DBG */


/*
 * Write block of data to EEPROM
 * The Atmel device does not allow writes to cross the
 * internal page size, which is 256 bytes on the 1 Mbit part.
 * Given the size of an McaRecord this is likely to occur, but
 * cannot happen more than once per call.
 * Must preset slave address on every call.
 */

static void
ee_wr(uint8_t addr, uint16_t ofs, uint8_t *buf, uint8_t len)
{
  uint16_t 	pix, swp;
  uint8_t	wl;
  int		err;

  if (mce_disabled)
    return;

  if ((ofs + len) < ofs) {
    EPR_PRT("ee_wr: address overrun\n");
    return;
  }

  xfr_configure(addr, FREQ_AUTO);

  pix = ofs & (EE_PG_SIZ - 1);
  while(len) {
    wl = (uint8_t) min((uint16_t)len, (uint16_t)(EE_PG_SIZ - pix));

    err = xfr_start(I2C_WRITE);
    if (err) {
      EPR_PRT("ee_wr: xfr_start (WR) err %d\n", err);
      return;
    }
  
    /*
     * Byte swap, send Most significant byte first
     */
    swp = (ofs >> 8) | (ofs << 8);
    err = xfr_write(FALSE, 2, (uint8_t *) &swp);
    if (err) {
      EPR_PRT("ee_wr: xfr_write offset (%02x, %02x) err %d\n", ofs >> 8, ofs & 0xff, err);
      return;
    }

    /*
     * Write payload to device
     */
    err = xfr_write(TRUE, wl, buf);
    if (err) {
      EPR_PRT("ee_wr: xfr_write %d bytes (%02x, %02x ..) err %d\n", wl, buf[0], buf[1], err);
      return;
    }
    ofs += wl;
    buf += wl;
    len -= wl;
    pix = 0;

    /*
     * Data sheet says wait 5 mSec before next
     * transaction to the device after a write.
     */
    myDELAY(5000);
  }
}


/*
 * Read block of data from EEPROM
 * Must preset slave address on every call.
 */

static void
ee_rd(uint8_t addr, uint16_t ofs, uint8_t *buf, uint8_t len)
{
  uint16_t	swp;
  int		err;

  if ((ofs + len) < ofs) {
    EPR_PRT("ee_rd: address overrun\n");
    return;
  }

  xfr_configure(addr, FREQ_AUTO);

  err = xfr_start(I2C_WRITE);
  if (err) {
    EPR_PRT("ee_rd: xfr_start (WR) err %d\n", err);
    return;
  }

  /*
   * Byte swap, send Most significant byte first
   */
  swp = (ofs >> 8) | (ofs << 8);
  err = xfr_write(FALSE, 2, (uint8_t *) &swp);
  if (err) {
    EPR_PRT("ee_rd: xfr_write (%02x, %02x) err %d\n", ofs >> 8, ofs & 0xff, err);
    return;
  }

  /*
   * Change bus direction and read payload
   */
  err = xfr_rept_start(I2C_READ);
  if (err) {
    EPR_PRT("ee_rd: xfr_rept_start (RD) err %d\n", err);
    return;
  }
  err = xfr_read(TRUE, len, buf);
  if (err) {
    EPR_PRT("ee_rd: xfr_read err %d\n", err);
    return;
  }
}


/*
 * Read one MCA event record from EEPROM
 * Handles crossing device addresses.
 */

static void
ee_get(McaRecord * rec, int no)
{
  uint32_t	pos, mid, low;

  mid = MR_ELOG_SIZE / 2;
  memset(rec, '\0', sizeof(*rec));
  pos = sizeof(McaHeader) + no * sizeof(McaRecord);
  if (pos < (mid - sizeof(McaRecord))) {
    /*
     * Record fit entirely in lower half of EEPROM
     */
    ee_rd(MR_ELOG_ADDR_LO, pos, (uint8_t *) rec, sizeof(*rec));
  }
  else
  if (pos > mid) {
    /*
     * Record fit entirely in upper half of EEPROM
     */
    ee_rd(MR_ELOG_ADDR_HI, pos - mid, (uint8_t *) rec, sizeof(*rec));
  }
  else {
    /*
     * Record spans both halves, need 2 reads.
     */
    low = mid - pos;
    ee_rd(MR_ELOG_ADDR_LO, pos, (uint8_t *) rec, low);
    ee_rd(MR_ELOG_ADDR_HI, 0, ((uint8_t *) rec) + low, sizeof(*rec) - low);
  }
}


/*
 * Write one MCA event record to EEPROM
 * Handles crossing device addresses.
 */

static void
ee_put(McaRecord * rec, int no)
{
  uint32_t	loc, mid, low;

  mid = MR_ELOG_SIZE / 2;
  loc = sizeof(McaHeader) + no * sizeof(McaRecord);
  if (loc < (mid - sizeof(McaRecord))) {
    /*
     * Record fit entirely in lower half of EEPROM
     */
    ee_wr(MR_ELOG_ADDR_LO, loc, (uint8_t *) rec, sizeof(*rec));
  }
  else
  if (loc > mid) {
    /*
     * Record fit entirely in upper half of EEPROM
     */
    ee_wr(MR_ELOG_ADDR_HI, loc - mid, (uint8_t *) rec, sizeof(*rec));
  }
  else {
    /*
     * Record spans both halves, need 2 writes.
     */
    low = mid - loc;
    ee_wr(MR_ELOG_ADDR_LO, loc, (uint8_t *) rec, low);
    ee_wr(MR_ELOG_ADDR_HI, 0, ((uint8_t *) rec) + low, sizeof(*rec) - low);
  }
}


/*
 * Add one MCA event to the EEPROM
 * Store the passed event info in the EEPROM, and update write
 * position to next entry, just in case if there are more than
 * one MC event detected that needs checking in maintenance mode.
 *
 * This can be called in exception context, and therefore must
 * work without any kernel support whatsoever. We must assume
 * kernel services are not reliable at this point.
 */

void
micras_mc_log(struct mce_info * event)
{
  McaRecord	mr;
  uint16_t	nxt, id;

  if (mce_disabled)
    return;

  /*
   * Print entry on serial console (copy in kernel log)
   */
#if MC_VERBOSE
  ee_printk("RAS.elog: bnk %d, id %d, ctl %llx, stat %llx, addr %llx, misc %llx\n",
	event->org, event->id, event->ctl, event->status, event->addr, event->misc);
#endif

  /*
   * Bail if EEPROM not in order (I2C lock-up or faulty device)
   */
  if (! ee_num)
    return;

  /*
   * Prepare MCA error log record.
   * We use the pysical CPU ID in the EEPROM records.
   */
  id = (event->org <= 2) ? event->pid : event->id;
  mr.id = PUT_BITS( 7,  0, id) |
          PUT_BITS(18, 16, event->org) |
          PUT_BIT(22, (event->flags & MC_FLG_FALSE) != 0) |
          PUT_BIT(24, (event->flags & MC_FLG_FILTER) != 0) |
	  PUT_BIT(31, 1);
  mr.stamp  = (uint32_t) event->stamp;
  mr.ctl    = event->ctl;
  mr.status = event->status;
  mr.addr   = event->addr;
  mr.misc   = event->misc;

#if ADD_DIE_TEMP
  {
    uint32_t	tmp;
    tmp = mr_sbox_rl(0, SBOX_THERMAL_STATUS_2);
    mr.id |= PUT_BITS(15, 8, GET_BITS(19, 10, tmp));
  }
#endif

  /*
   * Get I2C bus exclusive access
   */
  ee_lock();

#if EE_VERIFY
  {
    /*
     * Check for header corruption.
     * Time sink, only enable for debugging
     */
    extern int in_sync;
    McaHeader	hdr;

    ee_rd(MR_ELOG_ADDR_LO, 0, (uint8_t *) &hdr, sizeof(hdr));
    if (memcmp(hdr.signature, elog_preset.signature,
			sizeof(elog_preset.signature))) {
      if (in_sync) {
        printk("mc_log: Header corruption detected\n");
        dmp_hex(&hdr, sizeof(hdr), "mc_log: EEPROM header (entry)");
      }
      else {
	elog_print("mc_log: Header corruption detected (entry)\n");
        elog_print("EEPROM header: signature bad, ver %d, type %d\n",
		 hdr.header_ver, hdr.hwtype);
        elog_print("EEPROM capacity: %d events, size %d, start %d\n",
		hdr.entries, hdr.rec_size, hdr.rec_start);
        elog_print("EEPROM state: head %d, tail %d, full %d\n",
		hdr.rec_head, hdr.rec_tail, hdr.logfull);
      }
    }
  }
#endif

  nxt = (ee_head + 1) % ee_num;
  if (nxt == ee_tail) {
    ee_printk("RAS.elog: EEPROM full, dropping event\n");
    ee_unlock();
    return;
  }
  ee_put(&mr, nxt);

#if EE_VERIFY
  {
    /*
     * Read back and verify with memory buffer
     * Note: only works on 1st half of device.
     * Time sink, only enable for debugging
     */
    McaRecord  tst;

    ee_rd(MR_ELOG_ADDR_LO, loc, (uint8_t *) &tst, sizeof(tst));
    if (memcmp(&mr, &tst, sizeof(tst)))
      elog_print("Write event verify failed\n");
    else
      elog_print("Write event verify OK\n");
  }
#endif

  /*
   * Update head pointer in EEPROM header
   */
  ee_wr(MR_ELOG_ADDR_LO, offsetof(McaHeader, rec_head), (uint8_t *) &nxt, sizeof(nxt));
  ee_head = nxt;

#if EE_VERIFY
  {
    /*
     * Read back and verify with memory buffer
     * Time sink, only enable for debugging
     */
    uint16_t tst;

    ee_rd(MR_ELOG_ADDR_LO, 16, (uint8_t *) &tst, 2);
    if (tst != nxt)
      elog_print("Write index verify failed\n");
    else
      elog_print("Write index verify OK\n");
  }

  {
    /*
     * Check again for header corruption
     * Time sink, only enable for debugging
     */
    extern int in_sync;
    McaHeader	hdr;

    ee_rd(MR_ELOG_ADDR_LO, 0, (uint8_t *) &hdr, sizeof(hdr));
    if (memcmp(hdr.signature, elog_preset.signature,
			sizeof(elog_preset.signature))) {
      if (in_sync) {
        printk("mc_log: Header corruption detected (exit)\n");
        dmp_hex(&hdr, sizeof(hdr), "mc_log: EEPROM header");
      }
      else {
	elog_print("mc_log: Header corruption detected (exit)\n");
        elog_print("EEPROM header: signature bad, ver %d, type %d\n",
		 hdr.header_ver, hdr.hwtype);
        elog_print("EEPROM capacity: %d events, size %d, start %d\n",
		hdr.entries, hdr.rec_size, hdr.rec_start);
        elog_print("EEPROM state: head %d, tail %d, full %d\n",
		hdr.rec_head, hdr.rec_tail, hdr.logfull);
      }
    }
  }
#endif

  /*
   * Release I2C bus exclusive lock
   */
  ee_unlock();
}


/*
 * Reset the EEPROM to mint condition
 */

#define BSIZ	0xf0

static void
ee_mint(void)
{
  uint8_t	buf[EE_PG_SIZ];
  McaHeader	hdr;
  uint32_t	loc, mid;
  uint16_t	ofs;
  uint8_t	addr;


  if (ee_rdy && ! mce_disabled) {
    printk("EEPROM erase started ..\n");
    memset(buf, 0xff, sizeof(buf));

    ee_lock();

    /*
     * Several cheats in this loop.
     * - Despite  maximum transfer per write command is 255 (8 bit count),
     *   we send only half a 'page', i.e. 128 byte, per call to ee_wr().
     * - Picking exactly half a page, starting page aligned, ensures there
     *   will be no writes across a page boundary, i.e. ee_wr() will always
     *   result in exactly one I2C write command per call.
     * - We know that MR_ELOG_SIZE / (EE_PG_SIZ / 2) is a clean integer,
     *   and therefore will be no end condition to special case.
     * - Same will be true for the 'mid-chip' limit where the target
     *   address is bumped by one.
     */
    mid = MR_ELOG_SIZE / 2;
    for(loc = 0; loc < MR_ELOG_SIZE; loc += (EE_PG_SIZ / 2)) {
      addr = (loc < mid) ? MR_ELOG_ADDR_LO : MR_ELOG_ADDR_HI;
      ofs = loc & 0xffff;
      // printk(" -- loc %5x: addr %2x, offs %4x, len %4x\n", loc, addr, ofs, EE_PG_SIZ / 2);
      ee_wr(addr, ofs, buf, EE_PG_SIZ / 2);
    }

    /*
     * Put in a fresh header
     */
    ee_wr(MR_ELOG_ADDR_LO, 0, (uint8_t *) &elog_preset, sizeof(elog_preset));
    ee_rd(MR_ELOG_ADDR_LO, 0, (uint8_t *) &hdr, sizeof(hdr));
    printk("EEPROM erase complete\n");

    ee_unlock();

    /*
     * Verify that the header stuck.
     * If not, then complain to kernel log and set event capacity to 0
     */
    if (memcmp(hdr.signature, elog_preset.signature, sizeof(elog_preset.signature)) ||
        hdr.header_ver != elog_preset.header_ver ||
	hdr.rec_start != elog_preset.rec_start ||
	hdr.rec_size != elog_preset.rec_size ||
	hdr.hwtype != elog_preset.hwtype) {
      /*
       * Write EEPROM header failed.
       * Leave a message in the kernel log about it.
       */ 
      printk("Error: EEPROM initialization failed!\n");
      printk("MCA events cannot be logged to EEPROM\n");
      ee_num = 0;
    }
    else {
      ee_num  = hdr.entries;
      ee_head = hdr.rec_head;
      ee_tail = hdr.rec_tail;
      printk("EEPROM ready!\n");
    }


  }
}


#if EE_PROC
/*
 * Support for user space access to the EEPROM event log.
 * Implemented as a 'proc' file named elog, who returns
 * MCE events on read and on writes of 6 hex values
 * per line creates new event(s) to be entered.
 *
 * Compile time configurable for disabling writes and
 * choice of whether to dump new events or everything.
 */

static struct proc_dir_entry * elog_pe;

/*
 * Write is just a simple file operation.
 * We do not care about file offset since the specified event is to
 * be added to the EEPROM at head+1, not at any arbitrary location.
 */

static ssize_t
elog_write(struct file * file, const char __user * buff, size_t len, loff_t * off)
{
  char 	      * buf;
  uint16_t	nxt;
  McaRecord	mr;
  uint64_t	ull[6];
  char	      * ep, * cp;
  int		i, err;

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
   * Special case EEPROM reset option,
   * first 5 letters form the word 'reset'
   */
  if (!strncmp(buf, "reset", 5)) {
    ee_mint();
    goto wr_one;
  }

  /*
   * Need 6 numbers for an event record
   */
  for(i = 0; i < 6; i++) {
    while(isspace(*cp))
      cp++;
    ull[i] = simple_strtoull(cp, &ep, 16);
    if (ep == cp || (*ep != '\0' && !isspace(*ep))) {
      err = -EINVAL;
      goto wr_out;
    }
    cp = ep;
  }

#if 0
  /*
   * If we were to screen this the we should ensure that
   *   id[7:0]    < CPU_MAX on org 0, 1, 2
   *              < DBOX_NUM on org 3
   *		  == 0 on org 4
   *		  < GBOX_NUM on org 5
   *		  < TBOX_NUM on org 6
   *   id[18:16]  <= 6
   *   id[23]     == 0
   *   id[31]     == 1
   */
#endif

  if (ee_num) {
    mr.id     = (uint32_t) ull[0];
    mr.stamp  = (uint32_t) ull[1];
    mr.ctl    = ull[2];
    mr.status = ull[3];
    mr.addr   = ull[4];
    mr.misc   = ull[5];
   
    /*
     * Add event record under I2C bus exclusive access
     */
    ee_lock();
    nxt = (ee_head + 1) % ee_num;
    ee_put(&mr, nxt);
    ee_wr(MR_ELOG_ADDR_LO, offsetof(McaHeader, rec_head), (uint8_t *) &nxt, sizeof(nxt));
    ee_head = nxt;
    ee_unlock();
  }

  /*
   * Swallow any trailing junk up to next newline
   */
wr_one:
  ep = strchr(buf, '\n');
  if (ep)
   cp = ep + 1;
  err = cp - buf;

wr_out:
  kfree(buf);
  return err;
}


/*
 * Use the sequencer to read one event at a time,
 * in order of occurrence in the EEPROM. Sequence
 * position is event index in range 0 .. ee_num,
 * which will be offset by (ee_tail + 1) modulo
 * ee_num if EE_PROC_NEW flag is set.
 */

static int elog_eof;		/* Elog end-of-file marker */

static int
elog_seq_show(struct seq_file * f, void * v)
{
  McaRecord	mr;
  int		pos, nxt;
  static int	inv;

  pos = *(loff_t *) v;

  /*
   * Print nice header on 1st read from /proc/elog
   */
  if (! pos) {
    extern struct mr_rsp_hwinf hwinf;
    struct mr_rsp_hwinf * r = &hwinf;

    inv = 0;
    seq_printf(f, "Card %c%c%c%c%c%c%c%c%c%c%c%c: "
    		  "brd %d, fab %d, sku %d, rev %d, stp %d, sub %d\n",
	r->serial[0],  r->serial[1],  r->serial[2],  r->serial[3],
	r->serial[4],  r->serial[5],  r->serial[6],  r->serial[7],
	r->serial[8],  r->serial[9],  r->serial[10], r->serial[11],
	r->board, r->fab, r->sku, r->rev, r->step, r->substep);
    if (ee_num) {
      seq_printf(f, "Head %d, tail %d, cap %d\n", ee_head, ee_tail, ee_num);
      seq_printf(f, "%5s %8s %12s %8s %16s %16s %16s %16s\n",
		   "index", "id", "id decode", "time", "ctrl", "status", "addr", "misc");
    }
    else
      seq_printf(f, "Error: EEPROM not initialized\n");
  }

  /*
   * Set EOF and quit if EEPROM not accessible
   */
  if (! ee_num) {
    elog_eof = 1;
    return 0;
  }

  /*
   * Get event under I2C bus exclusive access
   */
#if EE_PROC_NEW
  nxt = (pos + ee_tail + 1) % ee_num;
#else
  nxt = pos;
#endif
  ee_lock();
  ee_get(&mr, nxt);
  ee_unlock();

#if ! EE_PROC_NEW
  /*
   * We refuse to print invalid entries. 
   * However, a freshly reset EEPROM contains all 1s and
   * therefore we won't rely on the valid-bit alone.
   * Instead rely on the unused areas of 'id' to be 0s.
   * Probably need to stop sequencer once a bad entry is
   * seen because in all likelihood we've reached the
   * log end and reading the remainder of the EEPROM will
   * just be waste of time.
   */
  if (GET_BITS(30, 25, mr.id) == 0x3f &&
      GET_BITS(21, 19, mr.id) == 0x07 &&
      GET_BITS(15,  8, mr.id) == 0xff) {
    if (inv++ > 10)
      elog_eof = 1;
    return 0;
  }
#endif

  seq_printf(f, "%5d %08x [%d %3d %c%c%c%c] %08x %016llx %016llx %016llx %016llx\n",
    		nxt, mr.id,
		GET_BITS(18,16,mr.id),
	 	GET_BITS(7,0,mr.id),
		GET_BIT(22,mr.id) ? 'I' : ' ',
		GET_BIT(23,mr.id) ? 'R' : ' ',
		GET_BIT(24,mr.id) ? 'F' : ' ',
		GET_BIT(31,mr.id) ? 'V' : ' ',
		mr.stamp, mr.ctl, mr.status, mr.addr, mr.misc);

  return 0;
}

static void *
elog_seq_start(struct seq_file * f, loff_t * pos)
{
  if (ee_num) {
    if (*pos >= ee_num)
      return NULL;
#if EE_PROC_NEW
    /*
     * Skip checks if we are dumping full log
     */
    if (ee_head == ee_tail)
      return NULL;
    if (*pos && ((*pos + ee_tail) % ee_num) == ee_head)
      return NULL;
#endif
  }

  elog_eof = 0;

  return pos;
}

static void *
elog_seq_next(struct seq_file * f, void * v, loff_t * pos)
{
  if (elog_eof)
    return NULL;

  (*pos)++;
  if (*pos >= ee_num)
    return NULL;

#if EE_PROC_NEW
  /*
   * No wrap checks if we are dumping full log
   */
  {
    int		nxt;

    nxt = ((*pos) + ee_tail) % ee_num;
    if (nxt == ee_head)
      return NULL;
  }
#endif

  return pos;
}

static void
elog_seq_stop(struct seq_file * f, void * v)
{
}

static const struct seq_operations elog_seq_ops = {
  .start = elog_seq_start,
  .next  = elog_seq_next,
  .stop  = elog_seq_stop,
  .show  = elog_seq_show,
};

static int
elog_open(struct inode *inode, struct file *filp)
{
  return seq_open(filp, &elog_seq_ops);
}

static struct file_operations proc_elog_operations = {
  .open           = elog_open,
  .read           = seq_read,
  .llseek         = seq_lseek,
  .release        = seq_release,
  .write	  = elog_write,
};

#endif /* EE_PROC */



/*
**
** Validation hooks.
**
** ee_list	List EEPROM contents to kernel log
** ee_wipe	Clear EEPROM (after RAS testing)
**
** Used by validation, exported entry point
** Do not enable this in production code.
**
*/

void
ee_list(void)
{
  McaHeader	hdr;
  McaRecord	rec;
  int		pos, i;

  /*
   * Get I2C bus exclusive access
   */
  ee_lock();

  /*
   * Read header
   */
  ee_rd(MR_ELOG_ADDR_LO, 0, (uint8_t *) &hdr, sizeof(hdr));
  if (! strncmp(hdr.signature, "MCA_LOG", sizeof(hdr.signature))) {
    printk("MCE log header: signature OK, ver %d, type %d\n",
		 hdr.header_ver, hdr.hwtype);
    printk("MCE log capacity: %d events, size %d, start %d\n",
		hdr.entries, hdr.rec_size, hdr.rec_start);
    printk("MCE log state: head %d, tail %d, full %d\n",
		hdr.rec_head, hdr.rec_tail, hdr.logfull);
    if (hdr.entries != MR_ELOG_NUM) {
      printk("MCE log check: invalid capacity, expected %ld\n", MR_ELOG_NUM);
      goto ee_bad;
    }
    if (hdr.rec_size != sizeof(McaRecord)) {
      printk("MCE log check: invalid rec size, expected %ld\n", sizeof(McaRecord));
      goto ee_bad;
    }
    if (hdr.rec_tail != ee_tail ||
	hdr.rec_head != ee_head) {
      printk("MCE log check: cached h/t mismatch %d/%d\n", ee_head, ee_tail);
      goto ee_bad;
    }
    if (hdr.entries != ee_num) {
      printk("MCE log check: cached capacity mismatch %d\n", ee_num);
      goto ee_bad;
    }

    /*
     * Header looks OK,
     * Dump all valid entries in eeprom
     */
    for(i = 0; i < hdr.entries; i++) {
      ee_get(&rec, i);

      /*
       * Uninitialized parts have all FFs in them,
       * need to screen those before testing the valid bit
       */
      if (rec.id != 0xffffffff && GET_BIT(31, rec.id)) {
#if EE_VERIFY
        dmp_hex(&rec, sizeof(rec), "ee_list: Entry[%d]", i);
#endif
        pos = hdr.rec_start + i * hdr.rec_size;
 	printk("Log %4d (pos %06x): id %08x, "
	       "ctrl %016llx, stat %016llx, addr %016llx, misc %016llx, time %d\n",
		i, pos, rec.id, rec.ctl, rec.status,
		rec.addr, rec.misc, rec.stamp);
      }
    }
  }
  else {
    printk("MCE log header: bad signature %02x%02x%02x%02x%02x%02x%02x%02x\n",
	hdr.signature[0], hdr.signature[1], hdr.signature[2], hdr.signature[3],
	hdr.signature[4], hdr.signature[5], hdr.signature[6], hdr.signature[7]);
  }

ee_bad:
  /*
   * Release I2C bus exclusive lock
   */
  ee_unlock();
}
EXPORT_SYMBOL_GPL(ee_list);

void
ee_wipe(void)
{
#if 1
  printk("Wiping EEPROM disabled, call ignored\n");
#else
  ee_mint();
#endif
}
EXPORT_SYMBOL_GPL(ee_wipe);
#endif /* CONFIG_MK1OM */


/*
**
** Setup access to the EEPROM on KnC
** This include initializing the local I2C driver and
** locating the next write position in the EEPROM.
** We want to limit the exception time activity to
** a minimum and thus make preparations up front.
** This is expected to happen before enabling the
** MC event intercepts.
**
*/

int
ee_init(void)
{
#if 0
  /*
   * Clocking the delay loop.
   * Average results over 3 runs:
   * uSec     % off
   *   1	12.46
   *   2	6.22
   *   4	4.34
   *   8	3.41
   *   16	2.90
   *   32	2.65
   *   64	2.52
   *   128	2.46
   *   256	2.43
   *   512	2.41
   *   1024	2.41
   *   2048	6.30
   *   4096	2.43
   *   8192	3.28
   *   16384	3.30
   *   32768	3.42
   * , which is fine for the purposes in this driver.
   */
  {
    uint64_t	t1, t2;
    uint64_t	usec, pwr;

    printk("RAS.test: tsc_khz %d\n", tsc_khz);
    for(pwr = 0; pwr < 16; pwr++) {
      usec = 1UL << pwr;
      t1 = rdtsc();
      myDELAY(usec);
      t2 = rdtsc();
      printk("RAS.test: myDelay(%lld) => %lld clocks\n", usec, t2 - t1);
    }
  }
#endif
    
#ifdef CONFIG_MK1OM
  if (! mce_disabled) {
    McaHeader	hdr;

#ifndef CONFIG_I2C_PXA
    /*
     * Reset I2C controller if PXA driver is not included in the kernel.
     */
    i2c_reset();
#endif

    /*
     * Get I2C bus exclusive access
     */
    ee_lock();

    /*
     * Paranoia!!
     * At this point the I2C controller should be inactive and
     * the I2C bus should be idle. Verify this to be true.
     * Note: This check is only applied on this very first
     *       access to the I2C controller. If it passed the
     *       two criterias we _assume_ we have good hardware.
     * TBD: should we assume that the I2C subsystem can go bad
     *      at runtime and add more checking?
     */
    ee_num = 0;
    if ((reg_read(ISR_OFFSET) & ISR_UB) || (reg_read(IBMR_OFFSET) != 3)) {
      printk("RAS.elog: I2C unit out of control, cannot access EEPROM\n");
    }
    else {
      /*
       * Get EEPROM header and cache log state.
       */
      ee_rd(MR_ELOG_ADDR_LO, 0, (uint8_t *) &hdr, sizeof(hdr));
      if (memcmp(hdr.signature, elog_preset.signature, sizeof(elog_preset.signature)) ||
	  hdr.header_ver != elog_preset.header_ver ||
	  hdr.rec_start != elog_preset.rec_start ||
	  hdr.rec_size != elog_preset.rec_size ||
	  hdr.hwtype != elog_preset.hwtype) {
	printk("RAS.elog: Found un-initialized EEPROM, initializing ..\n");
	ee_wr(MR_ELOG_ADDR_LO, 0, (uint8_t *) &elog_preset, sizeof(elog_preset));
	ee_rd(MR_ELOG_ADDR_LO, 0, (uint8_t *) &hdr, sizeof(hdr));
      }

      if (memcmp(hdr.signature, elog_preset.signature, sizeof(elog_preset.signature)) ||
	  hdr.header_ver != elog_preset.header_ver ||
	  hdr.rec_start != elog_preset.rec_start ||
	  hdr.rec_size != elog_preset.rec_size ||
	  hdr.hwtype != elog_preset.hwtype) {
	/*
	 * Write to EEPROM header failed.
	 * Leave a message in the kernel log about it and set capacity to 0.
	 */ 
	printk("RAS.elog: Error: EEPROM initialization failed!\n");
      }
      else {
	ee_num  = hdr.entries;
	ee_head = hdr.rec_head;
	ee_tail = hdr.rec_tail;
	printk("RAS.elog: rev %d, size %d, head %d, tail %d\n",
		    hdr.header_ver, ee_num, ee_head, ee_tail);
	if (ee_head != ee_tail) {
	  /*
	   *TBD: should we be aggressive and replay these events to the host
	   *     when it opens the MC SCIF channel to force the issue?
	   */
	  printk("RAS.elog: Warning: MCA log has unprocessed entries\n");
	}
      }
    }
    if (!ee_num)
      printk("RAS.elog: MCA events cannot be logged to EEPROM\n");

    /*
     * Release I2C bus exclusive lock
     */
    ee_unlock();
  }
#endif /* CONFIG_MK1OM */

  /*
   * Reset I2C bus & UART (sort of, internal reset only)
   */
  xfr_configure(SC16IS_ADDR(SC16IS_ADDR_1, SC16IS_ADDR_0), FREQ_AUTO);
  cons_init();
  ee_rdy = 1;

#if defined(CONFIG_MK1OM) && EE_PROC
  /*
   * Create proc file
   * We allow writes if EE_INJECT is defined or during manufacturing.
   */
  {
    int		mode;
#if EE_INJECT
    mode = 0644;
#else
    uint32_t	smc_err, smc_val, smc_fwv;

    /*
     * HSD 4846538
     * Needs SMC FW 1.8 or later to be safe to use.
     * Read FW version; if failed then not at manufacturing.
     * If FW version 1.8 or later go read Zombie register.
     * If zombie register responded we're at manufacturing,
     */
    mode = 0444;
    smc_err = gmbus_i2c_read(2, 0x28, 0x11, (uint8_t *) &smc_fwv, sizeof(smc_fwv));
    if (smc_err == sizeof(smc_fwv) && GET_BITS(31, 16, smc_fwv) >= 0x0108) {
      smc_err = gmbus_i2c_read(2, 0x28, 0x1b, (uint8_t *) &smc_val, sizeof(smc_val));
      if (smc_err == sizeof(uint32_t))
        mode = 0644;
    }
    if (mode == 0444)
      proc_elog_operations.write = 0;
#endif
    elog_pe = proc_create("elog", mode, 0, &proc_elog_operations);
  }
#endif

#if 0
  /*
   * Say hello on the console
   */
  ee_printk("RAS: ee_print ready, uart adr %02x\n",
		SC16IS_ADDR(SC16IS_ADDR_1, SC16IS_ADDR_0));
#endif

  if (mce_disabled)
    printk("RAS.elog: disabled\n");
  else
    printk("RAS.elog: init complete\n");
  return 0;
}


/*
 * Cleanup for module unload.
 * Free any resources held by this driver
 */

int
ee_exit(void)
{
#if defined(CONFIG_MK1OM) && EE_PROC
  if (elog_pe) {
    remove_proc_entry("elog", 0);
    elog_pe = 0;
  }
#endif


  /*
   * Reset I2C bus & UART (sort of, internal reset only)
   */
  ee_rdy = 0;
  xfr_configure(SC16IS_ADDR(SC16IS_ADDR_1, SC16IS_ADDR_0), FREQ_AUTO);
  cons_exit();

  printk("RAS.elog: exit complete\n");
  return 0;
}

#endif /* EMULATION */
