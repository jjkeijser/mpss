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
 * Definition of the public MC interface.
 * Access to MC event features provided through SCIF only.
 */

#ifndef _MICMCA_API_H_
#define _MICMCA_API_H_		1

#ifdef __cplusplus
extern "C" {	/* C++ guard */
#endif

/*
 * Configuration manifests
 */

#pragma pack(push, 4)		/* Windows requirement */


/*
 * Machine check info is reported on this port. Only one consumer can
 * (and must) connect in order to be notified about MC events.
 */

#define MR_MCE_PORT	SCIF_RAS_PORT_1


/*
 * MC events are provide in raw form, i.e. as close to the
 * contents of MCA register banks as possible. It is not
 * the responsibility of the MCA event handler to perform
 * analysis and interpretation of these registers, beyond
 * determining whether the event was deadly to the uOS.
 *
 * Any data or context corruption _IS_ deadly by definition!
 *
 * Source identifiers:
 *  org	 id
 *    0	 Bank 0 CPU #, core event, range 0..CPU_MAX
 *    1	 Bank 1 CPU #, core event, range 0..CPU_MAX
 *    2	 Bank 2 CPU #, core event, range 0..CPU_MAX
 *    3  DBOX #, uncore event, range 0..DBOX_MAX
 *    4  SBOX,   uncore event, range 0
 *    5	 GBOX #, uncore event, range 0..GBOX_MAX
 *    6	 TBOX #, uncore event, range 0..TBOX_MAX
 *
 * Report flags bits (when set) representing:
 *   [31:5]	Unused (and reserved)
 *	[4]	Filter event, uOS side disabled this event
 *	[3]	Status event, no failure (just MCA bank dump)
 *	[2]	Injected or artificially generated event
 *      [1]	This event has been recorded in EEPROM
 *	[0]	Fatal, the uOS is toast (card needs reset)
 *
 * MCA bank register sizes are not the same on all banks:
 *
 *	    CTL STATUS   ADDR   MISC    Notes
 * CPU 0:    32     64     -      - 	A,M not implemented, always 0
 * CPU 1:    32     64     64     32
 * CPU 2:    32     64     64     - 	M not implemented, always 0
 * DBOX:     32     64     64     - 	M not implemented, always 0
 * SBOX:     32     64     64     64	
 * GBOX:     64     64     64     32	
 * TBOX:     64     64     32     -	M not implemented, not there
 */

#define	MC_ORG_BNK0	0
#define	MC_ORG_BNK1	1
#define	MC_ORG_BNK2	2
#define MC_ORG_DBOX	3
#define MC_ORG_SBOX	4
#define MC_ORG_GBOX	5
#define MC_ORG_TBOX	6

#define MC_FLG_FATAL	(1 << 0)
#define MC_FLG_LOG	(1 << 1)
#define MC_FLG_FALSE	(1 << 2)
#define MC_FLG_STATUS	(1 << 3)
#define MC_FLG_FILTER	(1 << 4)

typedef struct mce_info {
  uint16_t	org;		/* Source of event */
  uint16_t	id;		/* Identifier of source */
  uint16_t	flags;		/* Report flags */
  uint16_t	pid;		/* Alternate source ID */
  uint64_t	stamp;		/* Time stamp of event */
  uint64_t	ctl;		/* MCA bank register 'CTL' */
  uint64_t	status;		/* MCA bank register 'STATUS' */
  uint64_t	addr;		/* MCA bank register 'ADDR' */
  uint64_t	misc;		/* MCA bank register 'MISC' */
} MceInfo;


#pragma pack(pop)		/* Restore to entry conditions */

#ifdef __cplusplus
}	/* C++ guard */
#endif

#endif	/* Recursion block */
