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
 * RAS module common internal declarations
 *
 * Configuration flags, constants and function prototypes
 * for the RAS sysfs, MT and MC module.
 */

#ifndef _MICRAS_H_
#define _MICRAS_H_	1


/*
 * Public APIs first.
 * Must be self-contained and independent of local tunables.
 */

#include "micras_api.h"
#include "micmca_api.h"
#include "micpm_api.h"


/*
 * Local configurables & tunables
 */

#define	USE_PM		1	/* Support power management */

#define RAS_HALT	1	/* Panic on uncorrectable MCAs */

#define I2C_SLOW        1       /* Default to lowest speed on I2C */

#define USE_FSC         1       /* Allow using FSC MGBR/MGBSR protocol */
#define USE_SVID        0       /* Allow using SVID for VR info */
#define USE_SMC         1       /* Prefer SMC over SBOX (telemetry) */

#define MT_TIMER        1       /* Enable periodic wakeup */
#define MT_PERIOD       999     /* Period sleep (mS) */

#define MCU_NMI         1       /* Use NMI in SBOX redirection table */

#define EE_VERIFY	0	/* Verify all EEPROM writes */
#define EE_PROC		1	/* Enable access to EEPROM from /proc/elog */
#define EE_PROC_NEW	0	/* Only display events between head & tail */
#define EE_INJECT	0	/* Enable writes to EEPROM via /proc/elog */

#define BEAM_TEST	0	/* Neuter MC handling for beam test */

#define MT_VERBOSE	0	/* Track MT activity in kernel log */ 
#define MC_VERBOSE	0	/* Track MC activity in kernel log */ 
#define PM_VERBOSE	0	/* Track PM activity in kernel log */ 

#define GBOX_WORKING	0	/* Set to one when GBOX writes are stable */

#define WA_4845465	0	/* Use HSD #4845465 workaround */

#define ADD_DIE_TEMP	1	/* Embed die temperature in event reports */

#define NOT_YET		0	/* 'Hide' code that's not currently in use */


/*
 * Useful macros
 *TBD: Cast everything to 64 bit (ULL)?
 *     For now all is 32 bit (U)
 */

#define GET_BITS(l,r,v)	(((v) >> (r)) & ((1U << ((l) - (r) +1)) -1))
#define PUT_BITS(l,r,v)	(((v) & ((1U << ((l) - (r) +1)) -1)) << (r))

#define GET_BIT(n,v)	GET_BITS((n), (n), (v))
#define PUT_BIT(n,v)	PUT_BITS((n), (n), (v))


/*
 * Init/Exit functions
 */

extern void	mr_mt_init(void);
extern void	mr_mt_exit(void);
extern void	mr_mt_card_init(void);
extern void 	mr_mt_card_exit(void);


/*
 * Command line options (exported from generic MCE handler)
 */

extern int	mce_disabled;


/*
 * MT opcode/function table.
 * Resides in micras_main() and gates access though sysctls and SCIF.
 */

struct fnc_tab {
  uint16_t	cmd;
  uint8_t	simple;
  uint8_t	privileged;
  int		(*fnc)(void *);
};

extern int	micras_priv;
extern int	micras_mt_call(uint16_t, void *);


/*
 * MT get functions
 * Spread over micras_{common,knf,knc}.c
 */
extern int	mr_get_hwinf(void *);
extern int	mr_get_vers(void *);
extern int	mr_get_pver(void *);
extern int	mr_get_freq(void *);
extern int	mr_get_volt(void *);
extern int	mr_get_power(void *);
extern int	mr_get_plim(void *);
extern int	mr_get_clst(void *);
extern int	mr_get_gddr(void *);
extern int	mr_get_gfreq(void *);
extern int	mr_get_gvolt(void *);
extern int	mr_get_temp(void *);
extern int	mr_get_fan(void *);
extern int	mr_get_ecc(void *);
extern int	mr_get_trc(void *);
extern int	mr_get_trbo(void *);
extern int	mr_get_oclk(void *);
extern int	mr_get_cutl(void *);
extern int	mr_get_mem(void *);
extern int	mr_get_os(void *);
extern int	mr_get_proc(void *);
extern int	mr_get_pmcfg(void *);

/*
 * MT set functions
 * Spread over micras_{common,knf,knc}.c
 */
extern int	mr_set_freq(void *);
extern int	mr_set_volt(void *);
extern int	mr_set_plim(void *);
extern int	mr_set_gfreq(void *);
extern int	mr_set_gvolt(void *);
extern int	mr_set_fan(void *);
extern int	mr_set_trc(void *);
extern int	mr_set_trbo(void *);
extern int	mr_set_oclk(void *);


/*
 * MT cmd functions
 */
extern int	mr_cmd_pkill(void *);
extern int	mr_cmd_ukill(void *);


#if defined(CONFIG_ML1OM) && USE_FSC
/*
 * MT FSC access functions
 * KnF specific, located in micras_knf.c
 */
extern int	mr_get_fsc(void *);
extern int	mr_set_fsc(void *);
#endif

#if defined(CONFIG_MK1OM)
/*
 * MT SMC access functions
 * KnC specific, located in micras_knc.c
 */
extern int	mr_get_smc(void *);
extern int	mr_get_led(void *);
extern int	mr_get_prochot(void *);
extern int	mr_get_pwralt(void *);
extern int	mr_get_perst(void *);
extern int	mr_get_ttl(void *);

extern int	mr_set_smc(void *);
extern int	mr_set_led(void *);
extern int	mr_set_prochot(void *);
extern int	mr_set_pwralt(void *);
extern int	mr_set_perst(void *);
#endif


#if defined(CONFIG_MK1OM) && USE_PM
/*
 * PM get functions
 */
extern int	pm_get_pl0(void *);
extern int	pm_get_pl1(void *);
extern int	pm_get_pavg(void *);
extern int	pm_get_pttl(void *);
extern int	pm_get_volt(void *);
extern int	pm_get_temp(void *);
extern int	pm_get_tach(void *);
extern int	pm_get_tttl(void *);
extern int	pm_get_fttl(void *);

/*
 * PM set functions
 */
extern int	pm_set_pl0(void *);
extern int	pm_set_pl1(void *);
extern int	pm_set_fttl(void *);
#endif


/*
 * MC & TTL event distribution functions
 * Spread over micras_{main,elog,core}.c
 */

#ifdef MR_MCE_PORT
extern int	micras_mc_send(struct mce_info *, int);
extern void	micras_mc_ipmi(struct mce_info *, int);
extern void	micras_mc_log(struct mce_info *);
extern uint32_t	micras_mc_filter(struct mce_info *, uint64_t, int);
#endif
#ifdef MR_TTL_PORT
extern void	micras_ttl_send(struct ttl_info *);
#endif


/*
 * BOX constants (card variations).
 */

#ifdef  CONFIG_ML1OM
#define DBOX_NUM	1
#define GBOX_NUM	4
#endif

#ifdef  CONFIG_MK1OM
#define DBOX_NUM	2
#define GBOX_NUM	8		/* Max count, SKU dependent */
#define TBOX_NUM	8		/* Max count, SKU dependent */
#endif

#ifndef COMMON_MMIO_BOX_SIZE
#define COMMON_MMIO_BOX_SIZE	(1<<16)
#endif


/*
 * BOX utility functions
 * Most located in micras_main.c
 */

extern char    *mr_sku(void);
extern int	mr_mch(void);
extern int	mr_txs(void);

extern uint8_t *micras_sbox;
extern uint8_t *micras_dbox[DBOX_NUM];
extern uint8_t *micras_gbox[GBOX_NUM];
#ifdef CONFIG_MK1OM
extern uint8_t *micras_tbox[TBOX_NUM];
#endif

extern uint8_t *mr_sbox_base(int);
extern uint32_t	mr_sbox_rl(int, uint32_t);
extern void	mr_sbox_wl(int, uint32_t, uint32_t);
extern uint64_t	mr_sbox_rq(int, uint32_t);
extern void	mr_sbox_wq(int, uint32_t, uint64_t);

extern uint8_t *mr_dbox_base(int);
extern uint32_t	mr_dbox_rl(int, uint32_t);
extern void	mr_dbox_wl(int, uint32_t, uint32_t);
extern uint64_t	mr_dbox_rq(int, uint32_t);
extern void	mr_dbox_wq(int, uint32_t, uint64_t);

extern uint8_t *mr_gbox_base(int);
extern uint32_t	mr_gbox_rl(int, uint32_t);
extern void	mr_gbox_wl(int, uint32_t, uint32_t);
extern uint64_t	mr_gbox_rq(int, uint32_t);
extern void	mr_gbox_wq(int, uint32_t, uint64_t);

#ifdef CONFIG_MK1OM
extern uint8_t *mr_tbox_base(int);
extern uint32_t	mr_tbox_rl(int, uint32_t);
extern void	mr_tbox_wl(int, uint32_t, uint32_t);
extern uint64_t	mr_tbox_rq(int, uint32_t);
extern void	mr_tbox_wq(int, uint32_t, uint64_t);
#endif


/*
 * Un-core MCA register offsets
 * Some #defines stolen from FreeBSD uOS.
 *
 *TBD: check again when we get real register include files
 */

#define SBOX_MCX_CTL_LO                 0x00003090
#define SBOX_MCX_STATUS_LO              0x00003098
#define SBOX_MCX_STATUS_HI              0x0000309C
#define SBOX_MCX_ADDR_LO                0x000030A0
#define SBOX_MCX_ADDR_HI                0x000030A4
#define SBOX_MCX_MISC                   0x000030A8
#define SBOX_MCX_MISC2                  0x000030AC
#define SBOX_MCA_INT_STAT               0x0000AB00
#define SBOX_MCA_INT_EN                 0x0000AB04
#define SBOX_COMPONENT_ID		0x00004134

#define DBOX_MC2_CTL                    0x00000340
#define DBOX_MC2_STATUS                 0x00000348
#define DBOX_MC2_ADDR                   0x00000350

#define GBOX_FBOX_MCA_CTL_LO            0x0000005C
#define GBOX_FBOX_MCA_CTL_HI            0x00000060
#define GBOX_FBOX_MCA_STATUS_LO         0x00000064
#define GBOX_FBOX_MCA_STATUS_HI         0x00000068
#define GBOX_FBOX_MCA_ADDR_LO           0x0000006C
#define GBOX_FBOX_MCA_ADDR_HI           0x00000070
#define GBOX_FBOX_MCA_MISC              0x00000074

#ifdef CONFIG_MK1OM
#define TXS_MCX_CONTROL                 0x00003700
#define TXS_MCX_STATUS                  0x00003740
#define TXS_MCX_ADDRESS                 0x00003780
#endif


/*
 * Thermal register offsets
 */

#if defined(CONFIG_MK1OM) && WA_4845465
#ifndef SBOX_MICROCONTROLLER_FAN_STATUS
#define SBOX_MICROCONTROLLER_FAN_STATUS	0x1020
#endif
#endif
#if defined(CONFIG_MK1OM) && (WA_4845465 || ADD_DIE_TEMP || USE_PM)
#ifndef SBOX_THERMAL_STATUS_2
#define SBOX_THERMAL_STATUS_2		0x1080
#endif
#endif


/*
 * SMP utilities
 * Located in micras_main.c
 */

extern uint32_t	rd_cr4_on_cpu(int);
extern void	set_in_cr4_on_cpu(int, uint32_t);
extern void	clear_in_cr4_on_cpu(int, uint32_t);
extern uint64_t	rdtsc(void);


/*
 * General EEPROM and POST card UART access
 * Located in micras_elog.c
 */

#define EE_BUF_COUNT   		100
#define EE_BUF_LINELEN 		256
extern char	ee_buf[];
extern atomic_t ee_msg;
extern atomic_t	ee_seen;

extern char *	ee_fmt(char *, va_list);
extern int	ee_printk(char *, ...);
extern int	ee_print(char *, ...);
#ifdef CONFIG_MK1OM
extern void	ee_list(void);
extern void	ee_wipe(void);
#endif
extern int	ee_init(void);
extern int	ee_exit(void);

extern void	myDELAY(uint64_t);


/*
 * SMC access API
 * Provided by the kernel
 */

extern int gmbus_i2c_read(uint8_t, uint8_t, uint8_t, uint8_t *, uint16_t);
extern int gmbus_i2c_write(uint8_t, uint8_t, uint8_t, uint8_t *, uint16_t);


/*
 * RAS core MCA handling
 * Located in micras_core.c
 */

extern uint8_t	xlat_cpu[NR_CPUS];
extern void	mcc_sync(void);
extern int	mcc_init(void);
extern int	mcc_exit(void);
extern void	mcc_flt_parm(uint8_t *);


/*
 * RAS un-core MCA handling
 * Located in micras_uncore.c
 */

extern void	box_reset(int);
extern int	mcu_init(void);
extern int	mcu_exit(void);


#if defined(CONFIG_MK1OM) && USE_PM
/*
 * RAS PM handling
 * Located in micras_pm.c
 *
 * Power management registration exchange records:
 * The RAS module populates a 'params' record and pass it to
 * the PM module through the micpm_ras_register() function.
 * In return the PM module populate the passed 'callbacks' record.
 * The PM module is responsible for populating the lists of
 * supported core frequencies and core voltages. In contrast to
 * KnF, where the lists reflect the hardware capabilities, these
 * reflect the actual frequencies and voltages that core-freq
 * module can use to lower power consumption.
 */  

struct micpm_params {
  uint32_t    * freq_lst;		/* Core frequency list */
  uint32_t    * freq_len;		/* Core freq count */
  uint32_t	freq_siz;		/* Space in core freq list */
  uint32_t    * volt_lst;		/* Core voltage list */
  uint32_t    * volt_len;		/* Core voltage count */
  uint32_t	volt_siz;		/* Space in core volt list */ 
  int	     (* mt_call)(uint16_t, void *); /* Access MT function */
  void	     (* mt_ttl)(int, int);	    /* Throttle notifier */
};

struct micpm_callbacks {
  int  (*micpm_get_turbo)(void);	/* Get PM turbo setting */
  void (*micpm_set_turbo)(int);		/* Notify PM of new turbo setting */
  void (*micpm_vf_refresh)(void);	/* Refresh core V/F lists */
  int  (*micpm_get_pmcfg)(void);	/* Get PM operating mode */
};

extern struct micpm_params	pm_reg;
extern struct micpm_callbacks	pm_cb;


/*
 * Args for mt_ttl() function
 */

#define TTL_OFF		0
#define TTL_ON		1

#define TTL_POWER	0
#define TTL_THERMAL	1


/*
 * Bit locations for micpm_get_turbo() and micpm_set_turbo()
 */

#define MR_PM_MODE	(1 << 0)	/* Turbo mode */
#define MR_PM_STATE	(1 << 1)	/* Current turbo state */
#define MR_PM_AVAIL	(1 << 2)	/* Turbo mode available */


/*
 * Bit positions for the different features turned on/off
 * in the uOS PM configuration, for micpm_get_pmcfg().
 */

#define PMCFG_PSTATES_BIT	0
#define PMCFG_COREC6_BIT	1
#define PMCFG_PC3_BIT		2
#define PMCFG_PC6_BIT		3


/*
 * Register/Unregister functions in micpm driver that RAS calls
 * during module init/exit.  Pointers to the exchanged data
 * structures are passed during registration.
 * The RAS module guarantee that the pointers are valid until
 * the unregister function is called. That way the PM module can
 * modify the core frequency/voltage lists if they gets changed.
 * The callbacks must always either be a valid function pointer
 * or a null pointer. 
 */

extern int	micpm_ras_register(struct micpm_callbacks *, struct micpm_params *);
extern void	micpm_ras_unregister(void);

extern int	mr_pm_ttl(struct mr_rsp_ttl *);
extern int	pm_init(void);
extern void	pm_exit(void);
#endif

		 	
/*
 * Debug tools
 */

extern void dmp_hex(void *, int, const char *, ...);

#endif	/* Recursion block */
