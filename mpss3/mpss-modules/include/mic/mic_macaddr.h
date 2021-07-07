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

#ifndef __MIC_MACADDR_H__
#define __MIC_MACADDR_H__

#define MAC_RUN_SHIFT	1
#define MAC_DATE_SHIFT	16

/**
 * mic_get_mac_from_serial	- Create MAC address from serial number string
 *      \param serial		string containing serial number
 *      \param mac		data space to place MAC address
 *      \param host		if true set least significant bit for hosts MAC
 *
 * mic_get_mac_from_serial() creates a MAC address from a MIC host's serial number.
 *
 * A MAC address contains 6 bytes of which the first 3 are either assigned by IEEE
 * or bit 2 of the first byte is set to indicate locally created.  While awaiting
 * our assigned values, the first the bytes have been set to 'MIC' with the local
 * bit also being set and multicast not.  The result is actually seeing "NIC".
 *
 * The last 3 bytes, or 24 bits are set in the pattern:
 *   o 8 bits are created by subtracting 1 from the cards year character mulitplied
 *     by the work week field.  By subtracting 1 the year starts at 2012 and there
 *     is enough room to accout for MIC cards build through 2017
 *   o 15 bits are the work week running number from the serail number.  This allows
 *     space for 32k of boards to be build in any one week.
 *   o 1 bit is used to indicated whether it is the host or card end of the virtual
 *     network connection.  The bit being set is the card MAC address.
 *
 * Upon successful completion, mic_get_mac_from_serial returns zero.  If the serial
 * number does not have "KC" (for Knights Corner) as the 3rd and 4th characters
 * then the serial number is invalid and a non zero value is returned.
 */

static int
mic_get_mac_from_serial(char *serial, unsigned char *mac, int host)
{
	unsigned long final;
	int y;
	int ww;

	if ((serial == NULL) || (serial[2] != 'K') || (serial[3] != 'C'))
		return 1;

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,39)
	y = kstrtoul(&serial[7], 10, &final);	// y is to shutup Suse build
#else
	final = simple_strtoul(&serial[7], NULL, 10);
#endif

	final = final << MAC_RUN_SHIFT;	/* Card side will add one */

	y = (serial[4] - '1');		/* start year 2012 end year 2016 */
	ww = ((serial[5] - '0') * 10) + (serial[6] - '0');

	final += (y * ww) << MAC_DATE_SHIFT;

	if (host)			/* least bit indicates host MAC */
		final++;

	mac[0] = 0x4c;
	mac[1] = 0x79;
	mac[2] = 0xba;
	mac[3] = (final >> 16) & 0xff;
	mac[4] = (final >> 8) & 0xff;
	mac[5] = final & 0xff;
	return 0;
}

#endif /* __MIC_MACADDR_H__ */
