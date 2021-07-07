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

#include "mic_common.h"

/* vnet/mic_shutdown/hvc/virtio */
#define VNET_SBOX_INT_IDX	0
#define MIC_SHT_SBOX_INT_IDX	1
#define HVC_SBOX_INT_IDX	2
#define VIRTIO_SBOX_INT_IDX	3
#define PM_SBOX_INT_IDX		4

#define MIC_BSP_INTERRUPT_VECTOR 229	// Host->Card(bootstrap) Interrupt Vector#
/*
 * Current usage of MIC interrupts:
 * APICICR1 - mic shutdown interrupt
 * APCICR0 - rest
 *
 * Planned Usage:
 * SCIF - rdmasrs
 * vnet/hvc/virtio - APICICR0
 * mic shutdown interrupt - APICICR1
 */
static void __mic_send_intr(mic_ctx_t *mic_ctx, int i)
{
	uint32_t apicicr_low;
	uint64_t apic_icr_offset = SBOX_APICICR0 + i * 8;

	apicicr_low = SBOX_READ(mic_ctx->mmio.va, apic_icr_offset);
	/* for KNC we need to make sure we "hit" the send_icr bit (13) */
	if (mic_ctx->bi_family == FAMILY_KNC)
		apicicr_low = (apicicr_low | (1 << 13));

	/* MIC card only triggers when we write the lower part of the
	 * address (upper bits)
	 */
	SBOX_WRITE(apicicr_low, mic_ctx->mmio.va, apic_icr_offset);
}

static inline void mic_send_vnet_intr(mic_ctx_t *mic_ctx)
{
	__mic_send_intr(mic_ctx, VNET_SBOX_INT_IDX);
}

static inline void mic_send_hvc_intr(mic_ctx_t *mic_ctx)
{
	__mic_send_intr(mic_ctx, HVC_SBOX_INT_IDX);
}

static inline void mic_send_scif_intr(mic_ctx_t *mic_ctx)
{
	__mic_send_intr(mic_ctx, 0);
}

static inline void mic_send_virtio_intr(mic_ctx_t *mic_ctx)
{
	__mic_send_intr(mic_ctx, VIRTIO_SBOX_INT_IDX);
}

static inline void mic_send_sht_intr(mic_ctx_t *mic_ctx)
{
	__mic_send_intr(mic_ctx, 1);
}

static inline void mic_send_pm_intr(mic_ctx_t *mic_ctx)
{
	__mic_send_intr(mic_ctx, PM_SBOX_INT_IDX);
}

static inline void mic_send_bootstrap_intr(mic_ctx_t *mic_ctx)
{
	uint32_t apicicr_low;
	uint64_t apic_icr_offset = SBOX_APICICR7;
	int vector = MIC_BSP_INTERRUPT_VECTOR;

	if (mic_ctx->bi_family == FAMILY_ABR){
		apicicr_low = vector;
	} else {
		/* for KNC we need to make sure we "hit" the send_icr bit (13) */
		apicicr_low = (vector | (1 << 13));
	}

	SBOX_WRITE(mic_ctx->apic_id, mic_ctx->mmio.va, apic_icr_offset + 4);
	// MIC card only triggers when we write the lower part of the address (upper bits)
	SBOX_WRITE(apicicr_low, mic_ctx->mmio.va, apic_icr_offset);
}
