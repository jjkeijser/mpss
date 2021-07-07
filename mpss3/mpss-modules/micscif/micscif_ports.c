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
 * Port reservation mechnism.
 * Since this goes with SCIF it must be available for any OS
 * and should not consume IP ports. Therefore, roll our own.
 * This is not required to be high performance, so a simple bit
 * array should do just fine.
 *
 * API specification (loosely):
 *
 *   uint16_t port
 *	Port number is a 16 bit unsigned integer
 *
 *   uint16_t rsrv_scif_port(uint16_t)
 *	reserve specified port #
 *	returns port #, or 0 if port unavailable.
 *
 *   uint16_t get_scif_port(void)
 *	reserve any available port #
 *	returns port #, or 0 if no ports available
 *
 *   void  put_scif_port(uint16_t)
 *	release port #
 *
 * Reserved ports comes from the lower end of the allocatable range,
 * and is reserved only in the sense that get_scif_port() won't use
 * them and there is only a predefined count of them available.
 */

#include <mic/micscif.h>

/*
 * Manifests
 * Port counts must be an integer multiple of 64
 */

#define	SCIF_PORT_BASE	0x0000	/* Start port (port reserved if 0) */
#define SCIF_PORT_COUNT	0x10000	/* Ports available */

#if SCIF_PORT_RSVD > (SCIF_PORT_COUNT/2)
#error	"No more than half of scif ports can be reserved !!"
#endif
#if (SCIF_PORT_BASE + SCIF_PORT_COUNT) > (2 << 16)
#error	"Scif ports cannot exceed 16 bit !!"
#endif

#include <linux/bitops.h>
#include <linux/spinlock_types.h>
static spinlock_t port_lock = __SPIN_LOCK_UNLOCKED(port_lock);

/*
 * Data structures
 *  init_array	Flag for initialize (mark as init_code?)
 *  port_bits	1 bit representing each possible port.
 *  first_free	Index into port_bits for free area
 *  port_lock	Lock for exclusive access
 *  port_rsvd	Total of successful "get/resv" calls.
 *  port_free	Total of successful "free" calls.
 *  port_err	Total of unsuccessfull calls.
 */

#define BITS_PR_PORT	 (8 * sizeof(uint64_t))
#define PORTS_ARRAY_SIZE ((SCIF_PORT_COUNT + (BITS_PR_PORT - 1)) / BITS_PR_PORT)


static int	init_array = 1;
static uint16_t	first_free;
static uint64_t	port_bits[PORTS_ARRAY_SIZE];
static uint64_t	port_rsvd;
static uint64_t	port_free;
static uint64_t	port_err;


/*
 * Bitfield handlers.
 *
 * Need 3 bit-fiddlers to operate on individual bits within
 * one 64 bit word in memory (always passing a pointer).
 * Individual bits are enumerated from 1, allowing for use
 * of value 0 to indicate an error condition.
 *
 * 1) __scif_ffsclr() returns index of first set bit in the
 *    64 bit word and clears it. A return value 0 means there
 *    were no set bits in the word.
 *
 * 2) __scif_clrbit() clears a specified bit in the 64 bit word
 *    The bit index is returned if bit was previously set and a
 *    value 0 is returned if it was previously clear.
 *
 * 3) __scif_setbit() sets a specified bit in the 64 bit word.
 *
 * Two versions, one should work for you.
 */

#if 1 && (defined(__GNUC__) || defined(ICC))
/*
 * Use GNU style inline assembly for bit operations.
 *
 * Gcc complains about uninitialized use of variables
 * big_bit in ffsclr and avl in clrbit. Generated code
 * is correct, just haven't figured out the correct
 * contraints yet.
 *
 * gcc -O2:
 *  __scif_ffsclr:  40 bytes
 *  __scif_clrbit:  34 bytes
 *  __scif_setbit:  17 bytes
 */

static int
__scif_ffsclr(uint64_t *word)
{
	uint64_t  big_bit = 0;
	uint64_t  field = *word;

	asm volatile (
		"bsfq %1,%0\n\t"
		"jnz 1f\n\t"
		"movq $-1,%0\n"
		"jmp 2f\n\t"
		"1:\n\t"
		"btrq %2,%1\n\t"
		"2:"
		: "=r" (big_bit), "=r" (field)
		: "0" (big_bit),  "1" (field)
	);

	if (big_bit == -1)
		return 0;

	*word = field;
	return big_bit + 1;
}

static int
__scif_clrbit(uint64_t *word, uint16_t bit)
{
	uint64_t  field = *word;
	uint64_t  big_bit = bit;
	int  avl = 0;

	big_bit--;
	asm volatile (
		"xorl %2,%2\n\t"
		"btrq %3,%1\n\t"
		"rcll $1,%2\n\t"
		: "=Ir" (big_bit), "=r" (field), "=r" (avl)
		: "0" (big_bit),   "1" (field),  "2" (avl)
	);

	*word = field;
	return avl ? bit : 0;
}

static void
__scif_setbit(uint64_t *word, uint16_t bit)
{
	uint64_t  field = *word;
	uint64_t  big_bit = bit;

	big_bit--;
	asm volatile (
		"btsq %2,%1"
		: "=r" (field)
		: "0" (field), "Jr" (big_bit)
	);

	*word = field;
}
#else
/*
 * C inliners for bit operations.
 *
 * gcc -O2:
 *  __scif_ffsclr:  50 bytes
 *  __scif_clrbit:  45 bytes
 *  __scif_setbit:  18 bytes
 *
 * WARNING:
 *  1) ffsll() may be glibc specific
 *  2) kernel ffs() use cmovz instruction that may not
 *     work in uOS kernel (see arch/x86/include/asm/bitops.h)
 *
 */


static int
__scif_ffsclr(uint64_t *word)
{
	int	bit;
/*
 *  ffsll()		Find 1st bit in 64 bit word
 */

	bit = ffsll(*word);
	if (bit)
		*word &= ~(1LL << (bit - 1));

	return bit;
}

static int
__scif_clrbit(uint64_t *word, uint16_t bit)
{
	uint64_t msk = (1LL << (bit - 1));

	if (*word & msk) {
		*word &= ~msk;
		return bit;
	}
	return 0;
}

static void
__scif_setbit(uint64_t *word, uint16_t bit)
{
	*word |= (1LL << (bit - 1));
}
#endif


static void
init_scif_array(void)
{
	spin_lock(&port_lock);
	if (init_array) {
		int i;
		for (i = 0; i < PORTS_ARRAY_SIZE; i++)
			port_bits[i] = ~0;
		first_free = SCIF_PORT_RSVD / BITS_PR_PORT;
		if (!SCIF_PORT_BASE)
			port_bits[0] ^= 1;
		port_rsvd = 0;
		port_free = 0;
		port_err = 0;
		init_array = 0;
	}
	spin_unlock(&port_lock);
	pr_debug("SCIF port array init:\n"
			"  %d ports available starting at %d, %d reserved\n"
			"  Array consists of %ld %ld-bit wide integers\n", 
			SCIF_PORT_BASE ? SCIF_PORT_COUNT : SCIF_PORT_COUNT - 1, 
			SCIF_PORT_BASE ? SCIF_PORT_BASE : 1, SCIF_PORT_RSVD, 
			PORTS_ARRAY_SIZE, BITS_PR_PORT);
}


/*
 * Reserve a specified port for SCIF
 * TBD: doxyfy this header
 */
uint16_t
rsrv_scif_port(uint16_t port)
{
	uint16_t port_ix;

	if (!port) {
		pr_debug("rsrv_scif_port: invalid port %d\n", port);
		port_err++;
		return 0;
	}

	if (init_array)
		init_scif_array();

	port -= SCIF_PORT_BASE;
	port_ix = port / BITS_PR_PORT;

	spin_lock(&port_lock);
	port = __scif_clrbit(port_bits + port_ix, 1 + (port % BITS_PR_PORT));
	if (port) {
		port = port - 1 + BITS_PR_PORT * port_ix + SCIF_PORT_BASE;
		port_rsvd++;
	} else {
		port_err++;
	}
	spin_unlock(&port_lock);

	return port;
}


/*
 * Get and reserve any port # for SCIF
 * TBD: doxyfy this header
 */
uint16_t
get_scif_port(void)
{
	uint16_t	port;

	if (init_array)
		init_scif_array();

	spin_lock(&port_lock);
	if (first_free >= PORTS_ARRAY_SIZE) {	/* Pool is empty */
		port = 0;
		port_err++;
		goto out;
	}
	port = __scif_ffsclr(port_bits + first_free);
	if (port) {
		port = port - 1 + BITS_PR_PORT * first_free + SCIF_PORT_BASE;
		while ((first_free < PORTS_ARRAY_SIZE) && !port_bits[first_free])
			first_free++;
		port_rsvd++;
	} else
		port_err++;
out:
	spin_unlock(&port_lock);
	return port;
}


/*
 * Release a reserved port # for SCIF
 * For now, just ignore release on unreserved port
 * TBD: doxyfy this header
 */

void
put_scif_port(uint16_t port)
{
	uint16_t port_ix;

	if (!port) {
		pr_debug("put_scif_port: invalid port %d\n", port);
		port_err++;
		return;
	}

	port -= SCIF_PORT_BASE;
	port_ix = port / BITS_PR_PORT;

	spin_lock(&port_lock);
	__scif_setbit(port_bits + port_ix, 1 + (port % BITS_PR_PORT));
	if (port >= SCIF_PORT_RSVD && port_ix < first_free)
		first_free = port_ix;
	port_free++;
	spin_unlock(&port_lock);
}

