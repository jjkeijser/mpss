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
 * The Monahan GX processor implementation of the I2C unit does not support
 * the hardware general call, 10-bit slave addressing or CBUS compatibility.
 * Otherwise it is compliant with I2C spec version 2.1.
 *
 * This is the SBOX 'OverClock' bus controller, which for reference is
 * mostly like the I2C controller on PXA270 with the above limitations.
 */

#ifndef _MONAHAN_H_
#define _MONAHAN_H_	1

/*
**
** Layer 1 stuff
**
** Offsets and bit definitions for the Monahans I2C controller.
** This is equivalent to defines in 'i2c-pxa.c', but kept separate.
*/

/*
 * Register locations (base SBOX register SBOX_OC_I2C_ICR)
 */
#define ICR_OFFSET		0x00
#define ISR_OFFSET		0x04
#define ISAR_OFFSET		0x08
#define IDBR_OFFSET		0x0c
#define IBMR_OFFSET		0x10

/*
 * I2C Control Register bits
 */
#define ICR_START		0x00000001	/* Start bit */
#define ICR_STOP		0x00000002	/* Stop bit */
#define ICR_ACKNAK		0x00000004	/* Send ACK(0) or NAK(1) */
#define ICR_TB			0x00000008	/* Transfer byte bit */
#define ICR_MA			0x00000010	/* Master abort */
#define ICR_SCLE		0x00000020	/* Master clock enable */
#define ICR_IUE			0x00000040	/* Unit enable */
#define ICR_GCD			0x00000080	/* General call disable */
#define ICR_ITEIE		0x00000100	/* Enable tx interrupts */
#define ICR_DRFIE		0x00000200	/* Enable rx interrupts */
#define ICR_BEIE		0x00000400	/* Enable bus error ints */
#define ICR_SSDIE		0x00000800	/* Slave STOP detected int enable */
#define ICR_ALDIE		0x00001000	/* Enable arbitration interrupt */
#define ICR_SADIE		0x00002000	/* Slave address detected int enable */
#define ICR_UR			0x00004000	/* Unit reset */
#define ICR_MODE		0x00018000	/* Bus speed mode */
#define ICR_RESERVED		0xfffe0000	/* Unused */

/*
 * Bus speed control values
 * High speed modes are not supported by controller.
 */
#define ICR_STANDARD_MODE       0x00000000	/* 100k operation */
#define ICR_FAST_MODE           0x00008000	/* 400k operation */
#define ICR_HS_STANDARD_MODE	0x00010000	/* 3.4M/100k operation */
#define ICR_HS_FAST_MODE        0x00018000	/* 3.4M/400k operation */

/*
 * Shorthands
 */
#define ICR_ON		(ICR_IUE | ICR_SCLE)	/* Turn unit on */
#define ICR_INIT_BITS	(ICR_ITEIE | \
			 ICR_DRFIE | \
			 ICR_BEIE | \
			 ICR_SADIE | \
			 ICR_FAST_MODE | \
			 ICR_ON)		/* Init flags */

/*
 * I2C Status Register bits
 */
#define ISR_RWM			0x00000001	/* Read(1)/write(0) mode */
#define ISR_ACKNAK		0x00000002	/* Ack(0)/nak(1) sent or received */
#define ISR_UB			0x00000004	/* Unit busy */
#define ISR_IBB			0x00000008	/* Bus busy */
#define ISR_SSD			0x00000010	/* Slave stop detected */
#define ISR_ALD			0x00000020	/* Arbitration loss detected */
#define ISR_ITE			0x00000040	/* Tx buffer empty */
#define ISR_IRF			0x00000080	/* Rx buffer full */
#define ISR_GCAD		0x00000100	/* General call address detected */
#define ISR_SAD			0x00000200	/* Slave address detected */
#define ISR_BED			0x00000400	/* Bus error no ACK/NAK */
#define ISR_RESERVED		0xfffff800	/* Unused */

#define ISR_INTS	(ISR_SSD | \
			 ISR_ALD | \
			 ISR_ITE | \
			 ISR_IRF | \
			 ISR_SAD | \
			 ISR_BED)		/* Interrupt flags */
/*
 * I2C Slave Address Register bits
 */
#define ISAR_SLADDR		0x0000007f	/* 7-bit address for slave-receive mode */
#define ISAR_RESERVED		0xffffff80	/* Unused */

/*
 * I2C Data Buffer Register bits
 */
#define IDBR_DATA		0x000000ff	/* 8-bit data buffer */
#define IDBR_RESERVED		0xffffff00	/* Unused */

/*
 * I2C Bus Monitor Register bits
 */
#define IBMR_SDA		0x00000001	/* State of SDA pin */
#define IBMR_SCL		0x00000002	/* State of SCL pin */
#define IBMR_RESERVED		0xfffffffc	/* Unused */


/*
**
** Layer 2 stuff
**
*/

/*
 * Bus speed selections
 */
#define	I2C_STANDARD		ICR_STANDARD_MODE
#define	I2C_FAST		ICR_FAST_MODE
#define	I2C_HS_STANDARD		ICR_HS_STANDARD_MODE
#define	I2C_HS_FAST		ICR_HS_FAST_MODE

/*
 * Command types
 */
#define	I2C_INVALID		-1	/* Internal, not to be used */
#define	I2C_WRITE		 0	/* Next transfer will be outgoing */
#define	I2C_READ		 1	/* Next transfer will be incoming */
#define	I2C_NOP			 2	/* Idle state */

/*
 * Return codes
 */
#define XFER_SUCCESS		  0	/* All OK */
#define INCOMPLETE_XFER		 -1	/* Basic timeout */
#define TX_CONTROLLER_ERROR	 -2	/* Requires reset */
#define TX_NAK			 -3	/* NAK, master to send a stop */
#define RX_SEVERE_ERROR		 -4	/* Requires reset */
#define RX_END_WITHOUT_STOP	 -5	/* Deprecated */
#define RX_BIZARRE_ERROR	 -6	/* Doesn't require reset */


/*
**
** Layer 3 stuff
**
*/

/*
 * Frequency selections
 */
#define	FREQ_MAX		-3	/* As fast as possible */
#define	FREQ_400K		-2	/* 400 kHz */
#define	FREQ_100K		-1	/* 100 kHz */
#define	FREQ_AUTO		 0	/* Default speed */

/*
 * Return codes: standard kernel codes used
 *  EBUSY, ENODEV, ENXIO, EINVAL, EIO
 */

#endif /* Recursion block */
