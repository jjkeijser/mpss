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

/* "Raw" register offsets & bit specifications for Intel MIC (KNF) */
#ifndef _MIC_SBOXDEFINE_REGISTERS_H_
#define _MIC_SBOXDEFINE_REGISTERS_H_


#define SBOX_OC_I2C_ICR						0x00001000
#define SBOX_THERMAL_STATUS					0x00001018
#define SBOX_THERMAL_INTERRUPT_ENABLE				0x0000101C
#define SBOX_STATUS_FAN1					0x00001024
#define SBOX_STATUS_FAN2					0x00001028
#define SBOX_SPEED_OVERRIDE_FAN					0x0000102C
#define SBOX_BOARD_TEMP1					0x00001030
#define SBOX_BOARD_TEMP2					0x00001034
#define SBOX_BOARD_VOLTAGE_SENSE				0x00001038
#define SBOX_CURRENT_DIE_TEMP0					0x0000103C
#define SBOX_CURRENT_DIE_TEMP1					0x00001040
#define SBOX_CURRENT_DIE_TEMP2					0x00001044
#define SBOX_MAX_DIE_TEMP0					0x00001048
#define SBOX_MAX_DIE_TEMP1					0x0000104C
#define SBOX_MAX_DIE_TEMP2					0x00001050
#define SBOX_ELAPSED_TIME_LOW                                   0x00001074
#define SBOX_ELAPSED_TIME_HIGH                                  0x00001078
#define SBOX_FAIL_SAFE_OFFSET					0x00002004
#define SBOX_CURRENT_CLK_RATIO					0x00003004
#define SBOX_SMPT00						0x00003100
#define SBOX_SMPT02						0x00003108
#define SBOX_RGCR						0x00004010
#define SBOX_DSTAT						0x00004014
#define SBOX_PCIE_PCI_REVISION_ID_AND_C_0X8			0x00005808
#define SBOX_PCIE_BAR_ENABLE					0x00005CD4
#define SBOX_SICR0						0x00009004
#define SBOX_SICE0						0x0000900C
#define SBOX_SICC0						0x00009010
#define SBOX_SICR1						0x0000901C
#define SBOX_SICC1						0x00009028
#ifdef CONFIG_MK1OM
#define SBOX_PMU_PERIOD_SEL             0x00001070
#define SBOX_THERMAL_STATUS_INTERRUPT   0x0000107C
#define SBOX_THERMAL_STATUS_2           0x00001080
#define SBOX_THERMAL_TEST_2             0x00001084
#define SBOX_COREFREQ					0x00004100
#define SBOX_COREVOLT					0x00004104
#define SBOX_MEMORYFREQ					0x00004108
#define SBOX_MEMVOLT					0x0000410C
//add defines used by drivers that are the same as DOORBELL_INTX
#define SBOX_SDBIC0                     0x0000CC90
#define SBOX_SDBIC1                     0x0000CC94
#define SBOX_SDBIC2                     0x0000CC98
#define SBOX_SDBIC3                     0x0000CC9C
#else
#define SBOX_SDBIC0						0x00009030
#define SBOX_SDBIC1						0x00009034
#define SBOX_SDBIC2						0x00009038
#define SBOX_SDBIC3						0x0000903C
#define SBOX_COREFREQ					0x00004040
#define SBOX_COREVOLT					0x00004044
#define SBOX_MEMORYFREQ					0x00004048
#define SBOX_MEMVOLT					0x0000404C
#define SBOX_RSC0                       0x0000CC10
#define SBOX_RSC1                       0x0000CC14

#endif
#define SBOX_MXAR0						0x00009040
#define	SBOX_MXAR0_K1OM						0x00009044
#define SBOX_MXAR1						0x00009044
#define SBOX_MXAR2						0x00009048
#define SBOX_MXAR3						0x0000904C
#define SBOX_MXAR4						0x00009050
#define SBOX_MXAR5						0x00009054
#define SBOX_MXAR6						0x00009058
#define SBOX_MXAR7						0x0000905C
#define SBOX_MXAR8						0x00009060
#define SBOX_MXAR9						0x00009064
#define SBOX_MXAR10						0x00009068
#define SBOX_MXAR11						0x0000906C
#define SBOX_MXAR12						0x00009070
#define SBOX_MXAR13						0x00009074
#define SBOX_MXAR14						0x00009078
#define SBOX_MXAR15						0x0000907C
#define SBOX_MSIXPBACR						0x00009080
#define	SBOX_MSIXPBACR_K1OM					0x00009084
#define SBOX_DCAR_0						0x0000A000
#define SBOX_DHPR_0						0x0000A004
#define SBOX_DTPR_0						0x0000A008
#define SBOX_DAUX_LO_0						0x0000A00C
#define SBOX_DAUX_HI_0						0x0000A010
#define SBOX_DRAR_LO_0						0x0000A014
#define SBOX_DRAR_HI_0						0x0000A018
#define SBOX_DITR_0						0x0000A01C
#define SBOX_DSTAT_0						0x0000A020
#define	SBOX_DSTATWB_LO_0					0x0000A024
#define	SBOX_DSTATWB_HI_0					0x0000A028
#define	SBOX_DCHERR_0						0x0000A02C
#define	SBOX_DCHERRMSK_0					0x0000A030
#define SBOX_DCAR_1						0x0000A040
#define SBOX_DHPR_1						0x0000A044
#define SBOX_DTPR_1						0x0000A048
#define SBOX_DAUX_LO_1						0x0000A04C
#define SBOX_DAUX_HI_1						0x0000A050
#define SBOX_DRAR_LO_1						0x0000A054
#define SBOX_DRAR_HI_1						0x0000A058
#define SBOX_DITR_1						0x0000A05C
#define SBOX_DSTAT_1						0x0000A060
#define	SBOX_DSTATWB_LO_1					0x0000A064
#define	SBOX_DSTATWB_HI_1					0x0000A068
#define	SBOX_DCHERR_1						0x0000A06C
#define	SBOX_DCHERRMSK_1					0x0000A070
#define SBOX_DCAR_2						0x0000A080
#define SBOX_DHPR_2						0x0000A084
#define SBOX_DTPR_2						0x0000A088
#define SBOX_DAUX_LO_2						0x0000A08C
#define SBOX_DAUX_HI_2						0x0000A090
#define SBOX_DRAR_LO_2						0x0000A094
#define SBOX_DRAR_HI_2						0x0000A098
#define SBOX_DITR_2						0x0000A09C
#define SBOX_DSTAT_2						0x0000A0A0
#define	SBOX_DSTATWB_LO_2					0x0000A0A4
#define	SBOX_DSTATWB_HI_2					0x0000A0A8
#define	SBOX_DCHERR_2						0x0000A0AC
#define	SBOX_DCHERRMSK_2					0x0000A0B0
#define SBOX_DCAR_3						0x0000A0C0
#define SBOX_DHPR_3						0x0000A0C4
#define SBOX_DTPR_3						0x0000A0C8
#define SBOX_DAUX_LO_3						0x0000A0CC
#define SBOX_DAUX_HI_3						0x0000A0D0
#define SBOX_DRAR_LO_3						0x0000A0D4
#define SBOX_DRAR_HI_3						0x0000A0D8
#define SBOX_DITR_3						0x0000A0DC
#define SBOX_DSTAT_3						0x0000A0E0
#define	SBOX_DSTATWB_LO_3					0x0000A0E4
#define	SBOX_DSTATWB_HI_3					0x0000A0E8
#define	SBOX_DCHERR_3						0x0000A0EC
#define	SBOX_DCHERRMSK_3					0x0000A0F0
#define SBOX_DCAR_4						0x0000A100
#define SBOX_DHPR_4						0x0000A104
#define SBOX_DTPR_4						0x0000A108
#define SBOX_DAUX_LO_4						0x0000A10C
#define SBOX_DAUX_HI_4						0x0000A110
#define SBOX_DRAR_LO_4						0x0000A114
#define SBOX_DRAR_HI_4						0x0000A118
#define SBOX_DITR_4						0x0000A11C
#define SBOX_DSTAT_4						0x0000A120
#define	SBOX_DSTATWB_LO_4					0x0000A124
#define	SBOX_DSTATWB_HI_4					0x0000A128
#define	SBOX_DCHERR_4						0x0000A12C
#define	SBOX_DCHERRMSK_4					0x0000A130
#define SBOX_DCAR_5						0x0000A140
#define SBOX_DHPR_5						0x0000A144
#define SBOX_DTPR_5						0x0000A148
#define SBOX_DAUX_LO_5						0x0000A14C
#define SBOX_DAUX_HI_5						0x0000A150
#define SBOX_DRAR_LO_5						0x0000A154
#define SBOX_DRAR_HI_5						0x0000A158
#define SBOX_DITR_5						0x0000A15C
#define SBOX_DSTAT_5						0x0000A160
#define	SBOX_DSTATWB_LO_5					0x0000A164
#define	SBOX_DSTATWB_HI_5					0x0000A168
#define	SBOX_DCHERR_5						0x0000A16C
#define	SBOX_DCHERRMSK_5					0x0000A170
#define SBOX_DCAR_6						0x0000A180
#define SBOX_DHPR_6						0x0000A184
#define SBOX_DTPR_6						0x0000A188
#define SBOX_DAUX_LO_6						0x0000A18C
#define SBOX_DAUX_HI_6						0x0000A190
#define SBOX_DRAR_LO_6						0x0000A194
#define SBOX_DRAR_HI_6						0x0000A198
#define SBOX_DITR_6						0x0000A19C
#define SBOX_DSTAT_6						0x0000A1A0
#define	SBOX_DSTATWB_LO_6					0x0000A1A4
#define	SBOX_DSTATWB_HI_6					0x0000A1A8
#define	SBOX_DCHERR_6						0x0000A1AC
#define	SBOX_DCHERRMSK_6					0x0000A1B0
#define SBOX_DCAR_7						0x0000A1C0
#define SBOX_DHPR_7						0x0000A1C4
#define SBOX_DTPR_7						0x0000A1C8
#define SBOX_DAUX_LO_7						0x0000A1CC
#define SBOX_DAUX_HI_7						0x0000A1D0
#define SBOX_DRAR_LO_7						0x0000A1D4
#define SBOX_DRAR_HI_7						0x0000A1D8
#define SBOX_DITR_7						0x0000A1DC
#define SBOX_DSTAT_7						0x0000A1E0
#define	SBOX_DSTATWB_LO_7					0x0000A1E4
#define	SBOX_DSTATWB_HI_7					0x0000A1E8
#define	SBOX_DCHERR_7						0x0000A1EC
#define	SBOX_DCHERRMSK_7					0x0000A1F0
#define SBOX_DCR						0x0000A280
#define SBOX_APICICR0						0x0000A9D0
#define SBOX_APICICR1						0x0000A9D8
#define SBOX_APICICR2						0x0000A9E0
#define SBOX_APICICR3						0x0000A9E8
#define SBOX_APICICR4						0x0000A9F0
#define SBOX_APICICR5						0x0000A9F8
#define SBOX_APICICR6						0x0000AA00
#define SBOX_APICICR7						0x0000AA08
#define SBOX_SCRATCH0						0x0000AB20
#define SBOX_SCRATCH1						0x0000AB24
#define SBOX_SCRATCH2						0x0000AB28
#define SBOX_SCRATCH3						0x0000AB2C
#define SBOX_SCRATCH4						0x0000AB30
#define SBOX_SCRATCH5						0x0000AB34
#define SBOX_SCRATCH6						0x0000AB38
#define SBOX_SCRATCH7						0x0000AB3C
#define SBOX_SCRATCH8						0x0000AB40
#define SBOX_SCRATCH9						0x0000AB44
#define SBOX_SCRATCH10						0x0000AB48
#define SBOX_SCRATCH11						0x0000AB4C
#define SBOX_SCRATCH12						0x0000AB50
#define SBOX_SCRATCH13						0x0000AB54
#define SBOX_SCRATCH14						0x0000AB58
#define SBOX_SCRATCH15						0x0000AB5C
#define SBOX_RDMASR0						0x0000B180
#define	SBOX_SBQ_FLUSH				                0x0000B1A0 // Pseudo-register, not autogen, must add manually
#define SBOX_TLB_FLUSH						0x0000B1A4
#define SBOX_GTT_PHY_BASE					0x0000C118
#define SBOX_EMON_CNT0						0x0000CC28
#define SBOX_EMON_CNT1						0x0000CC2C
#define SBOX_EMON_CNT2						0x0000CC30
#define SBOX_EMON_CNT3						0x0000CC34

#endif
