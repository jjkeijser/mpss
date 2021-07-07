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
 * Trace Capture module common declarations
 *
 * Contains configuration, constants and function prototypes
 * for the Trace Capture module.
 */

#ifndef _MICTC_H_
#define _MICTC_H_	1

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
//#include <linux/sched.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/smp.h>
#include <linux/nmi.h>
#include <linux/kdebug.h>
#include <linux/slab.h>
#include <linux/ratelimit.h>
#include <linux/fs.h>

#include <asm/uaccess.h>	// for get_user and put_user
#include <asm/processor.h>
#include <asm/debugreg.h>
#include <asm/apicdef.h>
#include <asm/system.h>
#include <asm/apic.h>
#include <asm/nmi.h>
#include <asm/irq_regs.h>
#include <asm/svm.h>
#include <asm/desc.h>
#include <linux/ioctl.h>

#ifndef __SCIF_H__
#include <scif.h>
#endif

/*
 * Version info: M.NP
 */

#define	TC_MAJOR	"0"
#define	TC_MINOR	"1"
#define	TC_PATCH	"a"
#define TC_VER		TC_MAJOR "." TC_MINOR  TC_PATCH

// These are common to the Host App
// and the MIC driver Trace Capture Feature
// COMMON DEFINES START HERE
enum TRACE_COMMAND
{
    TRACE_NOP = 100,
    TRACE_DATA,
    TRACE_HOST_READY,
    TRACE_DONE,
    TRACE_ERROR,
    TRACE_PRINT,
    TRACE_GET_FILE,
    TRACE_PAGE_READY,
    TRACE_REG_COMPLETE,
    TRACE_MEM_COMPLETE,
    TRACE_COMPLETE,
    TRACE_ABORTED
};

// IOCTL
#define MICTC_MAJOR_NUM		's'
#define MICTC_DEVICE_NAME	"trace_capture"
#define MICTC_FILE_NAME		"/dev/trace_capture"

#define MICTC_START_CAPTURE	_IOW(MICTC_MAJOR_NUM, 0xff, int)

// Use 2MB for KNF and 4MB for K1OM (auto-detected).
#define MICTC_XML_BUFFER_SIZE (2 * 1024UL * 1024UL)

#define MICTC_MEM_BUFFER_SIZE (1 * 1024UL * 1024UL * 1024UL)

// Shared memory constants
#define TRACE_STATUS_OFFSET   8
#define TRACE_SIZE_OFFSET     16

// Enable/Disable Memory Test.
// This MUST be enabled simultaneously on Host App as well.
#define MIC_TRACE_CAPTURE_MEMORY_TEST 0

#if MIC_TRACE_CAPTURE_MEMORY_TEST
#define TRACE_CHECKSUM_OFFSET 24
#endif

#define TRACE_TRIGGER_MAX	10
#define TRACE_TRIGGER_OFFSET	28
#define TRACE_DATA_OFFSET	4096

// Used to indicate the end of the list for trace triggers.
#define TRACE_EOL 0xffffffff
// Used for trace counts to indicate that the driver should ignore current trace.
// Only meaningful when it is first in the list of trace triggers -- the entries
// after it are ignored.  Trace counts supersede trace triggers.
#define TRACE_IGNORE 0xfffffffe

// Types of Triggers - Refer to uOS Trace Capture Wiki for Usage
// Generic counter
#define TRACE_HOST_GENERIC_COUNTER 0x1
// Async Flip counter
#define TRACE_HOST_FRAME_COUNTER   0x2
// COMMON DEFINES END HERE

// MSR's defined in the trace file sent during REQs
// Are these all valid for L1OM??
#define P6_CR_TSC                    0x10
#define X86_CR_APICBASE              0x1b
#define MIC_CR_SPUBASE               0x1c
#define IA32_CR_MISC                 0x1a0
#define WMT_CR_LASTBRANCH_0          0x1db
#define WMT_CR_LASTBRANCH_1          0x1dc
#define X86_CR_MTRRphysMask0         0x201
#define X86_CR_MTRRphysMask1         0x203
#define X86_CR_MTRRphysMask2         0x205
#define X86_CR_MTRRphysMask3         0x207
#define X86_CR_MTRRphysMask4         0x209
#define X86_CR_MTRRphysMask5         0x20b
#define X86_CR_MTRRphysMask6         0x20d
#define X86_CR_MTRRphysMask7         0x20f
#define IA32_CR_PAT                  0x277
#define IA32_MTRR_DEF_TYPE           0x2ff
#define VMX_MSR_BASE                 0x480
#define VMX_MSR_BASE_PLUS_1          0x481
#define VMX_MSR_BASE_PLUS_2          0x482
#define VMX_MSR_BASE_PLUS_3          0x483
#define VMX_MSR_BASE_PLUS_4          0x484
#define VMX_MSR_BASE_PLUS_5          0x485
#define VMX_MSR_BASE_PLUS_6          0x486
#define VMX_MSR_BASE_PLUS_7          0x487
#define VMX_MSR_BASE_PLUS_8          0x488
#define VMX_MSR_BASE_PLUS_9          0x489
#define TIME                         0x4711
#define PINFO                        0x4712
#define X86_CR_MTRRdefType           0x2ff
#define X86_CR_MTRRcap               0xfe
#define X86_CR_MTRRphysBase0         0x200
#define X86_CR_MTRRphysBase1         0x202
#define X86_CR_MTRRphysBase2         0x204
#define X86_CR_MTRRphysBase3         0x206
#define X86_CR_MTRRphysBase4         0x208
#define X86_CR_MTRRphysBase5         0x20a
#define X86_CR_MTRRphysBase6         0x20c
#define X86_CR_MTRRphysBase7         0x20e
#define X86_CR_MTRRfix64K_00000      0x250
#define X86_CR_MTRRfix16K_80000      0x258
#define X86_CR_MTRRfix16K_A0000      0x259
#define X86_CR_MTRRfix4K_C0000       0x268
#define X86_CR_MTRRfix4K_C8000       0x269
#define X86_CR_MTRRfix4K_D0000       0x26a
#define X86_CR_MTRRfix4K_D8000       0x26b
#define X86_CR_MTRRfix4K_E0000       0x26c
#define X86_CR_MTRRfix4K_E8000       0x26d
#define X86_CR_MTRRfix4K_F0000       0x26e
#define X86_CR_MTRRfix4K_F8000       0x26f
#define P5_MC_ADDR                   0x0
#define P5_MC_TYPE                   0x1
#define MSR_TR1                      0x2
#define MSR_TR2                      0x4
#define MSR_TR3                      0x5
#define MSR_TR4                      0x6
#define MSR_TR5                      0x7
#define MSR_TR6                      0x8
#define MSR_TR7                      0x9
#define MSR_TR9                      0xb
#define MSR_TR10                     0xc
#define MSR_TR11                     0xd
#define MSR_TR12                     0xe
#define IA32_APIC_BASE               0x1b
#define IA32_TIME_STAMP_COUNTER      0x10
#define IA32_PerfCntr0               0x20
#define IA32_PerfCntr1               0x21
#define IA32_PerfCntr2               0x22
#define IA32_PerfCntr3               0x23
#define PerfFilteredCntr0            0x24
#define PerfFilteredCntr1            0x25
#define PerfFilteredCntr2            0x26
#define PerfFilteredCntr3            0x27
#define IA32_PerfEvtSel0             0x28
#define IA32_PerfEvtSel1             0x29
#define IA32_PerfEvtSel2             0x2a
#define IA32_PerfEvtSel3             0x2b
#define PerfFilterMask               0x2c
#define IA32_PERF_GLOBAL_STATUS      0x2d
#define IA32_PERF_GLOBAL_OVF_CONTROL 0x2e
#define IA32_PERF_GLOBAL_CTRL        0x2f
#define IA32_MCG_CTL                 0x17b
#define IA32_MC0_CTRL                0x400
#define IA32_MC0_STAT                0x401
#define IA32_MC0_ADDR                0x402
#define IA32_MC0_MISC                0x403
#define IA32_MC1_CTRL                0x404
#define IA32_MC1_STAT                0x405
#define IA32_MC1_ADDR                0x406
#define IA32_MC1_MISC                0x407
#define STAR                         0xc0000081
#define LSTAR                        0xc0000082
#define SYSCALL_FLAG_MASK            0xc0000084
#define X86_PAT                      0x277
#define SPU_BASE                     0x1C


#endif	/* Recursion block */
