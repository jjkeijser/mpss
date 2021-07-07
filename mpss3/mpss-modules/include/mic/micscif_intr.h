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

#ifndef MICSCIF_INTR_H
#define MICSCIF_INTR_H
#define SBOX_SDBIC0_DBSTAT_BIT  0x40000000
#define SBOX_SDBIC0_DBREQ_BIT   0x80000000

/* RDMASR Info */
#define RDMASR_IRQ_BASE	17
#define get_rdmasr_irq(m)	((RDMASR_IRQ_BASE) + (m))
#define get_rdmasr_offset(m)	(((m) << 2) + (SBOX_RDMASR0))

#ifdef _MIC_SCIF_
int register_scif_intr_handler(struct micscif_dev *dev);
void deregister_scif_intr_handler(struct micscif_dev *dev);
#endif
int micscif_setup_interrupts(struct micscif_dev *dev);
void micscif_destroy_interrupts(struct micscif_dev *scifdev);
#endif /*  MICSCIF_INTR_H */
