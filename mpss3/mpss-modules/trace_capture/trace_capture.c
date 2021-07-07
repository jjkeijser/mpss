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
 * Trace Capture Driver
 *
 * Contains code to handle trace_capture syscall, stop all cpus
 * and dump their state, then dump all physical memeory.
 */

#include "trace_capture.h"

//#define DEBUG

int always_false = 0;

#define BARRIER(epd, string) { \
	printk("%s\n", string); \
	if ((err = scif_send(epd, &control_msg, sizeof(control_msg), 1)) <= 0) { \
	  pr_crit("%s:%s:%d scif_send failed with err %ld\n", __FILE__, __FUNCTION__, __LINE__, err); \
	  goto close;							\
	} \
	if ((err = scif_recv(epd, &control_msg, sizeof(control_msg), 1)) <= 0) { \
	  pr_crit("%s:%s:%d scif_recv failed with err %ld\n", __FILE__, __FUNCTION__, __LINE__, err); \
	  goto close;							\
	} \
}

/* SPU privileged gates (per specification) */
#define SPU_SPBA_OFFSET        0x1000 /* offset of Privileged gates in SPU MMIO */
#define SPU_XQ_SIZE            0x040
#define SPU_XQ_BASE            0x080
#define SPU_XQ_INDEX           0x0C0
#define SPU_CR                 0x100
#define SPU_CONTROL            0x100
#define SPU_SAMPLER_BASE       0x140
#define SPU_ABORT              0x180
#define SPU_ABORT_STATUS       0x1C0
#define SPU_FLUSH              0x200
#define SPU_FLUSH_STATUS       0x240
#define SPU_INVALPG_4K         0x280
#define SPU_INVALPG_64K        0x2C0
#define SPU_INVALPG_2M         0x300
#define SPU_EMPTY              0x340
#define SPU_ACTIVE             0x340
#define SPU_FULL               0x380
#define SPU_SOFT_RESET         0x3C0
#define SPU_PMU_EVENT_SEL      0x400
#define SPU_CONTROL2           0x440
#define SPU_CONTROL3           0x480

#define SPU_MEM_BW_LIMIT       0x4C0 // This is 64 bit register

#define SPU_TCU_CREDITS        0x700
#define SPU_FER                0x800
#define SPU_ALT_FER            0x840
#define SPU_MATCH_ACTION       0x880
#define SPU_INVAL              0xB00
#define SPU_COUNTER0_SET       0x500
#define SPU_COUNTER1_SET       0x540
#define SPU_COUNTER2_SET       0x580
#define SPU_COUNTER3_SET       0x5C0
#define SPU_COUNTER4_SET       0x600
#define SPU_COUNTER5_SET       0x640
#define SPU_COUNTER6_SET       0x680
#define SPU_COUNTER7_SET       0x6C0

#define CBOX_SPU_PA_MSR              0x0000017E
#define CBOX_SPU_SAMPLER_BIND_MSR    0x0000017F

#define	MSR_SF_MASK	0xc0000084	/* syscall flags mask */
#define	MSR_FSBASE	0xc0000100	/* base address of the %fs "segment" */
#define	MSR_GSBASE	0xc0000101	/* base address of the %gs "segment" */
#define	MSR_KGSBASE	0xc0000102	/* base address of the kernel %gs */

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

// Kernel virtual address to physical page at 0xfee03000
// This is created by an ioremap outside of interrupt context.
static uint8_t *spu_addr;

struct mictc_seg {
  struct desc_struct desc;
  char zero[8];
  u16 selector;
  uint64_t base;
};

struct mictc_tss {
  tss_desc desc;
  u16 selector;
  uint64_t base;
};

struct mictc_segment_reg
{
    struct mictc_seg cs;
    struct mictc_seg ds;
    struct mictc_seg es;
    struct mictc_seg ss;
    struct mictc_seg fs;
    struct mictc_seg gs;
    struct mictc_tss ldtr;
    struct mictc_tss tr;
};

#define MAX_SEG_REG 8

static char *SegRegNames[MAX_SEG_REG] = {"CS","DS","ES","SS", "FS","GS","LDTR","TR"};

//static struct i387_fxsave_struct fpu;

struct mictc_trace
{
  struct mictc_segment_reg segment;
  struct vpustate_struct vpustate;
  struct i387_fxsave_struct fpu;
};

struct mictc_trace *trace;

// fxsave definition copied from fpu.c
//#define mictc_fxsave(addr) __asm __volatile("fxsave %0" : "=m" (*(addr)))
#define mictc_fxsave(addr) __asm __volatile("fxsave (%0)" : "=a" (addr) : [fx] "a" (addr))


// Spinlock to serialize access in IPI handler
static DEFINE_SPINLOCK(mictc_lock);

// Used to count the cpus waiting
static atomic_t cpus_stopped = ATOMIC_INIT(0);

// Used to count the cpus released
static atomic_t cpus_released = ATOMIC_INIT(0);

// End points for SCIF
//static scif_epd_t mictc_endp_cmd;
static scif_epd_t mictc_endp_data;

// SCIF ports - temp hack; move to scif.h
#define MICTC_SCIF_PORT_DATA 300

// Used to prevent concurent access into the same device .
static int Device_Open = 0;

#define PS_BUF_SIZE 150
//static char print_string_buf[PS_BUF_SIZE] = "";

#define print_str(fmt, ...) \
{ \
  snprintf(print_string_buf, PS_BUF_SIZE, fmt, ##__VA_ARGS__); \
  print_string(print_string_buf); \
}

//#define printk(fmt, ...) print_str(fmt, ##__VA_ARGS__)
//#undef pr_crit
//#define pr_crit(fmt, ...) print_str(fmt, ##__VA_ARGS__)

// Interrupts off / on
#define cli __asm (" cli\n")
#define sti __asm (" sti\n")

// Debug code to display low 16 bits of eflags register.
#define print_eflags \
	{unsigned long kernel_eflags; \
	  raw_local_save_flags(kernel_eflags); \
	  printk("%s:%d eflags %lx\n", __FUNCTION__, __LINE__, kernel_eflags); \
	}


// Find another definition of this in some .h file
static __inline void
mictc_cpuid(u_int ax, u_int *p)
{
	__asm __volatile("cpuid"
			 : "=a" (p[0]), "=b" (p[1]), "=c" (p[2]), "=d" (p[3])
			 :  "0" (ax));
}

static inline
uint32_t get_dr(int regno)
{
	unsigned long val = 0;	/* Damn you, gcc! */

	switch (regno) {
	case 0:
		asm("mov %%db0, %0" :"=r" (val));
		break;
	case 1:
		asm("mov %%db1, %0" :"=r" (val));
		break;
	case 2:
		asm("mov %%db2, %0" :"=r" (val));
		break;
	case 3:
		asm("mov %%db3, %0" :"=r" (val));
		break;
	case 4:
		asm("mov %%db4, %0" :"=r" (val));
		break;
	case 5:
		asm("mov %%db5, %0" :"=r" (val));
		break;
	case 6:
		asm("mov %%db6, %0" :"=r" (val));
		break;
	case 7:
		asm("mov %%db7, %0" :"=r" (val));
		break;
	default:
		BUG();
	}
	return val;
}


static inline void mictc_store_ldt(u16 *dtr)
{
	asm volatile("sldt %0":"=m" (*dtr));
}


static inline void mictc_store_tr(u16 *dtr)
{
	asm volatile("str %0":"=m" (*dtr));
}


static inline void read_gdt_entry(struct desc_struct *gdt, int entry,
				  void *desc, int type)
{
	unsigned int size;
	switch (type) {
	case DESC_TSS:
		size = sizeof(tss_desc);
		break;
	case DESC_LDT:
		size = sizeof(ldt_desc);
		break;
	default:
		size = sizeof(struct desc_struct);
		break;
	}
	memcpy(desc, &gdt[entry], size);
#if 0 // Helpful for debug
	{ u64 *p = (u64 *)&gdt[entry];
	  printk("GDT[entry] = %p %llx %llx\n", &gdt[entry], p[0], p[1]);
	}
#endif
}


static inline void __get_tss_desc(unsigned cpu, unsigned int entry, void *dest)
{
	struct desc_struct *d = get_cpu_gdt_table(cpu);
	read_gdt_entry(d, entry, dest, DESC_TSS);
}

#define get_tss_desc(cpu, addr) __get_tss_desc(cpu, GDT_ENTRY_TSS, addr)


static inline void __get_seg_desc(unsigned cpu, unsigned int entry, void *dest)
{
  struct desc_struct *d = get_cpu_gdt_table(cpu);

  read_gdt_entry(d, entry, dest, 0);
}

#define get_seg_desc(cpu, seg, addr) __get_seg_desc(cpu, ((seg & 0xffff) >> 3), addr)

// Redefine rdmsr to work like BSD.

//#undef rdmsr
//#define rdmsr(msr) tc_msr((msr))

static inline
uint64_t tc_rdmsr(uint32_t msrid)
{
  uint32_t lower, upper;
  rdmsr(msrid, lower, upper);
  return (uint64_t)upper << 32 | lower;
}

// Number of Retries before it is assumed that the Host will not respond
#define TRACE_CAPTURE_TIMEOUT  50000000

static void *g_traceBufferAllocated;

// Global variable used by initiator to wait for everyone to complete trace captures
//static volatile u32 g_smpTraceCaptureWait;

// Global variable to keep track of how much data we are writing to the shared buffer
// with the Host.
static volatile u64 g_sizeXferred = 0;

static s64 g_triggerFound = -1;

static volatile u64 *g_traceBufferStatusOffset = NULL;
static volatile u64 *g_traceBufferSizeOffset = NULL;
static volatile u32 *g_traceBufferDataOffset = 0;
static volatile u32 *g_traceBufferTriggerOffset = NULL;

// This is an array of trigger numbers.  The value TRACE_EOL is ignored.
static u32 g_traceTriggers[TRACE_TRIGGER_MAX];
static u32 g_traceCurrentTrigger;

static long scif_offset_xml;
//static long scif_offset_xml_dst;
static long scif_offset_mem;
static long scif_offset_dst;

#if MIC_TRACE_CAPTURE_MEMORY_TEST
static volatile u64 *g_traceBufferChecksumOffset = NULL;

// The maximum size allowed for a DMA transfer is 1MB - 4K. The size of this array
// is 1MB to allow this to be used as the dst memory while dumping entire GDDR
// For Debug purposes only.
static u32 g_dstMemoryDump[4096/sizeof(u32)] __attribute__ ((aligned(4096)));
#endif

#define TRACE_SPRINTF(...) \
    (g_sizeXferred += sprintf(((char*)g_traceBufferDataOffset + g_sizeXferred), __VA_ARGS__))

#define ADD_SPU_REG_TO_HEADER(x) \
    TRACE_SPRINTF("\t\t\t\t<reg offset=\"0x%x\">\n\t\t\t\t\t<name>%s</name>\n\t\t\t\t</reg>\n", (x), #x)

#define ADD_MSR_TO_HEADER(x) \
    TRACE_SPRINTF("\t\t\t\t<reg addr=\"0x%x\"/>\n", (x))

#define TRACE_SPRINTF_MSR(x) \
    TRACE_SPRINTF("\t\t\t\t<reg addr=\"0x%x\">0x%llx</reg>\n", (x), tc_rdmsr((x)))

#define TRACE_SPRINTF_SPU(x) \
    TRACE_SPRINTF("\t\t\t\t<reg offset=\"0x%x\">0x%llx</reg>\n", (x), *(volatile u64*)((u8*)spu_addr + (x)))

#define TRACE_SPRINTF_VECTOR(x, vpu) \
    PrintVector((u8*)&(vpu), (x))


//------------------------------------------------------------------------------
//  FUNCTION: mictc_trace_capture_prep_SPU_header
//
//  DESCRIPTION:
//  Perform all the tasks related to preparing the SPU Trace Header
//
//  PARAMETERS: None
//
//  RETURNS: None
//
//  TODOS:
//
static void
mictc_trace_capture_prep_SPU_header(void)
{
    TRACE_SPRINTF("\t\t\t<spu>\n");
    ADD_SPU_REG_TO_HEADER(SPU_XQ_SIZE);
    ADD_SPU_REG_TO_HEADER(SPU_XQ_BASE);
    ADD_SPU_REG_TO_HEADER(SPU_XQ_INDEX);
    ADD_SPU_REG_TO_HEADER(SPU_CONTROL);
    ADD_SPU_REG_TO_HEADER(SPU_SAMPLER_BASE);
    ADD_SPU_REG_TO_HEADER(SPU_PMU_EVENT_SEL);
    ADD_SPU_REG_TO_HEADER(SPU_CONTROL2);
    ADD_SPU_REG_TO_HEADER(SPU_CONTROL3);
    TRACE_SPRINTF("\t\t\t</spu>\n");
}


//------------------------------------------------------------------------------
//  FUNCTION: mictc_trace_capture_prep_cpuid_header
//
//  DESCRIPTION:
//  Perform all the tasks related to preparing the CPUID Trace Header
//
//  PARAMETERS: None
//
//  RETURNS: None
//
//  TODOS:
//
static void
mictc_trace_capture_prep_cpuid_header(void)
{
    u_int regs[4];
    int i =0;
    TRACE_SPRINTF("\t\t\t<cpuid>\n");
    for (i = 0; i < 0x4; i++)
    {
        mictc_cpuid(i, regs);
        TRACE_SPRINTF("\t\t\t\t<reg eax=\"0x%x\">0x%x-0x%x-0x%x-0x%x</reg>\n",
                      i, regs[0], regs[1], regs[2], regs[3]);
    }
    TRACE_SPRINTF("\t\t\t</cpuid>\n");
}


//------------------------------------------------------------------------------
//  FUNCTION: mictc_trace_capture_prep_msr_header
//
//  DESCRIPTION:
//  Perform all the tasks related to preparing the MSR Trace Header
//
//  PARAMETERS: None
//
//  RETURNS: None
//
//  TODOS:
//
static void
mictc_trace_capture_prep_msr_header(void)
{
    TRACE_SPRINTF("\t\t\t<msr>\n");
    ADD_MSR_TO_HEADER(P6_CR_TSC);
    ADD_MSR_TO_HEADER(X86_CR_APICBASE);
    ADD_MSR_TO_HEADER(CBOX_SPU_PA_MSR);
    ADD_MSR_TO_HEADER(SPU_BASE);
    ADD_MSR_TO_HEADER(CBOX_SPU_SAMPLER_BIND_MSR);
    ADD_MSR_TO_HEADER(X86_CR_MTRRphysMask0);
    ADD_MSR_TO_HEADER(X86_CR_MTRRphysMask1);
    ADD_MSR_TO_HEADER(X86_CR_MTRRphysMask2);
    ADD_MSR_TO_HEADER(X86_CR_MTRRphysMask3);
    ADD_MSR_TO_HEADER(X86_CR_MTRRphysMask4);
    ADD_MSR_TO_HEADER(X86_CR_MTRRphysMask5);
    ADD_MSR_TO_HEADER(X86_CR_MTRRphysMask6);
    ADD_MSR_TO_HEADER(X86_CR_MTRRphysMask7);
    ADD_MSR_TO_HEADER(MSR_EFER);
    ADD_MSR_TO_HEADER(MSR_SF_MASK);
    ADD_MSR_TO_HEADER(MSR_FSBASE);
    ADD_MSR_TO_HEADER(MSR_GSBASE);
    ADD_MSR_TO_HEADER(X86_CR_MTRRdefType);
    ADD_MSR_TO_HEADER(X86_CR_MTRRcap);
    ADD_MSR_TO_HEADER(X86_CR_MTRRphysBase2);
    ADD_MSR_TO_HEADER(X86_CR_MTRRphysBase0);
    ADD_MSR_TO_HEADER(X86_CR_MTRRphysBase1);
    ADD_MSR_TO_HEADER(X86_CR_MTRRphysBase3);
    ADD_MSR_TO_HEADER(X86_CR_MTRRphysBase4);
    ADD_MSR_TO_HEADER(X86_CR_MTRRphysBase5);
    ADD_MSR_TO_HEADER(X86_CR_MTRRphysBase6);
    ADD_MSR_TO_HEADER(X86_CR_MTRRphysBase7);
    ADD_MSR_TO_HEADER(STAR);
    ADD_MSR_TO_HEADER(LSTAR);
    ADD_MSR_TO_HEADER(MSR_KGSBASE);
    
    // The following MSR's are currently ifdef'd out
    // because LarrySim barfs on these.
    // We might need these later.
#if 0
    ADD_MSR_TO_HEADER(X86_CR_MTRRfix64K_00000);
    ADD_MSR_TO_HEADER(X86_CR_MTRRfix16K_80000);
    ADD_MSR_TO_HEADER(X86_CR_MTRRfix16K_A0000);
    ADD_MSR_TO_HEADER(X86_CR_MTRRfix4K_C0000);
    ADD_MSR_TO_HEADER(X86_CR_MTRRfix4K_C8000);
    ADD_MSR_TO_HEADER(X86_CR_MTRRfix4K_D0000);
    ADD_MSR_TO_HEADER(X86_CR_MTRRfix4K_D8000);
    ADD_MSR_TO_HEADER(X86_CR_MTRRfix4K_E0000);
    ADD_MSR_TO_HEADER(X86_CR_MTRRfix4K_E8000);
    ADD_MSR_TO_HEADER(X86_CR_MTRRfix4K_F0000);
    ADD_MSR_TO_HEADER(X86_CR_MTRRfix4K_F8000);
    ADD_MSR_TO_HEADER(P5_MC_ADDR);
    ADD_MSR_TO_HEADER(P5_MC_TYPE);
    ADD_MSR_TO_HEADER(MSR_TR1);
    ADD_MSR_TO_HEADER(MSR_TR2);
    ADD_MSR_TO_HEADER(MSR_TR3);
    ADD_MSR_TO_HEADER(MSR_TR4);
    ADD_MSR_TO_HEADER(MSR_TR5);
    ADD_MSR_TO_HEADER(MSR_TR6);
    ADD_MSR_TO_HEADER(MSR_TR7);
    ADD_MSR_TO_HEADER(MSR_TR9);
    ADD_MSR_TO_HEADER(MSR_TR10);
    ADD_MSR_TO_HEADER(MSR_TR11);
    ADD_MSR_TO_HEADER(MSR_TR12);
    ADD_MSR_TO_HEADER(IA32_APIC_BASE); 
    ADD_MSR_TO_HEADER(IA32_TIME_STAMP_COUNTER); 
    ADD_MSR_TO_HEADER(IA32_PerfCntr0); 
    ADD_MSR_TO_HEADER(IA32_PerfCntr1); 
    ADD_MSR_TO_HEADER(IA32_PerfCntr2); 
    ADD_MSR_TO_HEADER(IA32_PerfCntr3); 
    ADD_MSR_TO_HEADER(PerfFilteredCntr0); 
    ADD_MSR_TO_HEADER(PerfFilteredCntr1); 
    ADD_MSR_TO_HEADER(PerfFilteredCntr2); 
    ADD_MSR_TO_HEADER(PerfFilteredCntr3); 
    ADD_MSR_TO_HEADER(IA32_PerfEvtSel0); 
    ADD_MSR_TO_HEADER(IA32_PerfEvtSel1); 
    ADD_MSR_TO_HEADER(IA32_PerfEvtSel2); 
    ADD_MSR_TO_HEADER(IA32_PerfEvtSel3); 
    ADD_MSR_TO_HEADER(PerfFilterMask); 
    ADD_MSR_TO_HEADER(IA32_PERF_GLOBAL_STATUS); 
    ADD_MSR_TO_HEADER(IA32_PERF_GLOBAL_OVF_CONTROL); 
    ADD_MSR_TO_HEADER(IA32_PERF_GLOBAL_CTRL);     
    ADD_MSR_TO_HEADER(IA32_MCG_CTL); 
    ADD_MSR_TO_HEADER(IA32_MC0_CTRL); 
    ADD_MSR_TO_HEADER(IA32_MC0_STAT); 
    ADD_MSR_TO_HEADER(IA32_MC0_ADDR); 
    ADD_MSR_TO_HEADER(IA32_MC0_MISC); 
    ADD_MSR_TO_HEADER(IA32_MC1_CTRL); 
    ADD_MSR_TO_HEADER(IA32_MC1_STAT); 
    ADD_MSR_TO_HEADER(IA32_MC1_ADDR); 
    ADD_MSR_TO_HEADER(IA32_MC1_MISC); 
    ADD_MSR_TO_HEADER(SYSCALL_FLAG_MASK);
    ADD_MSR_TO_HEADER(X86_PAT);
#endif
    TRACE_SPRINTF("\t\t\t</msr>\n");
}


//------------------------------------------------------------------------------
//  FUNCTION: mictc_prep_header
//
//  DESCRIPTION:
//  Perform all the tasks related to preparing the Trace Header
//
//  PARAMETERS: None
//
//  RETURNS: None
//
//  TODOS:
//
static void
mictc_prep_header(void)
{
    int i;

    TRACE_SPRINTF("<?xml version=\"1.0\" standalone=\"yes\"?>\n");
    TRACE_SPRINTF("<arch_data>\n");
    TRACE_SPRINTF("<!-- The format of this file is defined in https://cpu-sim.intel.com/twiki/bin/view/CpuSim/TraceFileFormats. -->\n");
    TRACE_SPRINTF("\t<header>\n");
    TRACE_SPRINTF("\t\t<format_version>1.0</format_version>\n");
    TRACE_SPRINTF("\t\t<creation_date>Nov 19 2009</creation_date>\n");
    TRACE_SPRINTF("\t\t<arch_xml_ver>1.1</arch_xml_ver>\n");
    TRACE_SPRINTF("\t\t<arch_xml_date>Oct 21 2009</arch_xml_date>\n");
    TRACE_SPRINTF("\t\t<created_by>archlib</created_by>\n");
    TRACE_SPRINTF("\t\t<comment>Warnings!  This is based on the state available in archlib.</comment>\n");
    TRACE_SPRINTF("\t\t<comment>  This state dump is primarily good for capturing frequently used architectural register state.</comment>\n");
    TRACE_SPRINTF("\t\t<comment>  Support for CPUId, MSRs, APIC, and x87 state is currently incomplete.</comment>\n");
    TRACE_SPRINTF("\t\t<comment>  There is no support for state not specifically modeled in archlib.</comment>\n");
    TRACE_SPRINTF("\t\t<comment>  Have also noticed inconsistencies in the final value of the RFLAGS reg.</comment>\n");
    if (g_triggerFound != -1)
    {
        TRACE_SPRINTF("\t\t<comment>  This capture is generated for HOST BASED TRIGGER # %lld.</comment>\n", g_triggerFound);
        g_triggerFound = -1;
    }
    TRACE_SPRINTF("\t</header>\n");
    TRACE_SPRINTF("\t<cpu_definition>\n");
    TRACE_SPRINTF("\t\t<num_cpus>%d</num_cpus>\n", num_online_cpus());
    TRACE_SPRINTF("<!-- the number of \"cpu\" definitions must correspond to the \"num_cpus\" data item -->\n");

    for (i = 0; i < num_online_cpus(); i++)
    {
        TRACE_SPRINTF("\t\t<cpu num=\"%d\">\n", i);
// SPU is not supported in Linux
	if (always_false) mictc_trace_capture_prep_SPU_header();
	mictc_trace_capture_prep_cpuid_header();
	mictc_trace_capture_prep_msr_header();
        TRACE_SPRINTF("\t\t</cpu>\n");
    }

    TRACE_SPRINTF("\t</cpu_definition>\n");
    TRACE_SPRINTF("\t<platform_definition>\n");
    TRACE_SPRINTF("\t\t<physical_memory/>\n");
    TRACE_SPRINTF("\t</platform_definition>\n");
    TRACE_SPRINTF("\t<cpu_state>\n");
    TRACE_SPRINTF("<!-- the number of \"cpu\" definitions must correspond to the \"num_cpus\" data item -->\n");
}


//------------------------------------------------------------------------------
//  FUNCTION: mictc_capture_general_purpose_reg
//
//  DESCRIPTION:
//  Capture all general purpose registers.
//
//  PARAMETERS: None
//
//  RETURNS: None
//
//  TODOS:
//
static void
mictc_capture_general_purpose_reg(struct pt_regs *regs)
{
  //  printk("starting reg dump regs=%llx\n", (uint64_t)regs);

  if (!regs) {
    printk("Null pointer found.  cpu %d %s\n", smp_processor_id(), current->comm);
    return;
  }

    TRACE_SPRINTF("\t\t\t<general>\n");
    TRACE_SPRINTF("\t\t\t\t<reg name=\"RAX\">0x%lx</reg>\n", regs->ax);
    TRACE_SPRINTF("\t\t\t\t<reg name=\"RBX\">0x%lx</reg>\n", regs->bx);
    TRACE_SPRINTF("\t\t\t\t<reg name=\"RCX\">0x%lx</reg>\n", regs->cx);
    TRACE_SPRINTF("\t\t\t\t<reg name=\"RDX\">0x%lx</reg>\n", regs->dx);
    TRACE_SPRINTF("\t\t\t\t<reg name=\"RBP\">0x%lx</reg>\n", regs->bp);
    TRACE_SPRINTF("\t\t\t\t<reg name=\"RSP\">0x%lx</reg>\n", regs->sp);
    TRACE_SPRINTF("\t\t\t\t<reg name=\"RSI\">0x%lx</reg>\n", regs->si);
    TRACE_SPRINTF("\t\t\t\t<reg name=\"RDI\">0x%lx</reg>\n", regs->di);
    TRACE_SPRINTF("\t\t\t\t<reg name=\"R8\">0x%lx</reg>\n",  regs->r8);
    TRACE_SPRINTF("\t\t\t\t<reg name=\"R9\">0x%lx</reg>\n",  regs->r9);
    TRACE_SPRINTF("\t\t\t\t<reg name=\"R10\">0x%lx</reg>\n", regs->r10);
    TRACE_SPRINTF("\t\t\t\t<reg name=\"R11\">0x%lx</reg>\n", regs->r11);
    TRACE_SPRINTF("\t\t\t\t<reg name=\"R12\">0x%lx</reg>\n", regs->r12);
    TRACE_SPRINTF("\t\t\t\t<reg name=\"R13\">0x%lx</reg>\n", regs->r13);
    TRACE_SPRINTF("\t\t\t\t<reg name=\"R14\">0x%lx</reg>\n", regs->r14);
    TRACE_SPRINTF("\t\t\t\t<reg name=\"R15\">0x%lx</reg>\n", regs->r15);
//  In cases where a CPU is halted and is woken up from halt by the trace capture IPI 
//  we want to report the RIP as the one pointing to the halt instruction itself 
//  and not the one on the trap frame. This is to avoid the condition where the simulator-run
//  for these halted CPUs ends up running extra cycles (before going back idle) 
//  which would not happen under actual conditions. Problem reported by Jason S.   
////    if(regs->tf_rip == (register_t)ExitIdle)
////        TRACE_SPRINTF("\t\t\t\t<reg name=\"RIP\">0x%lx</reg>\n", regs->ip-1);
////    else
        TRACE_SPRINTF("\t\t\t\t<reg name=\"RIP\">0x%lx</reg>\n", regs->ip);
    TRACE_SPRINTF("\t\t\t\t<reg name=\"RFLAGS\">0x%lx</reg>\n", regs->flags);
    TRACE_SPRINTF("\t\t\t</general>\n");

    //  printk("ending reg dump\n");
}


//------------------------------------------------------------------------------
//  FUNCTION: mictc_capture_segment_reg
//
//  DESCRIPTION:
//  Capture all segment registers.
//
//  PARAMETERS: None
//
//  RETURNS: None
//
//  TODOS:
//
static void
mictc_capture_segment_reg(struct mictc_segment_reg *segment, struct pt_regs *regs)
{
  int i, v;
  struct desc_ptr gdtr;
  struct desc_ptr idtr;
  struct mictc_seg *segreg;

//  printk("Segment registers on cpu %d\n", smp_processor_id());

  // This is only useful during initial development.
  if (!regs) {
    printk("Null pointer found.  cpu %d %s\n", smp_processor_id(), current->comm);
    return;
  }

    segment->cs.selector   = (u16)regs->cs;
    segment->ss.selector   = (u16)regs->ss;
#if 0
    if (ISPL(regs->tf_cs) == SEL_KPL && curthread->td_pcb->pcb_ds == 0x0) {
        // Specifically required for kernel IDLE thread
        segment->ds   = 0x10;
        segment->es   = 0x10;
        segment->fs   = 0x10;
        segment->gs   = 0x10;
    } else {
#endif
    asm("movl %%ds,%0" : "=r" (v)); segment->ds.selector = v;
    asm("movl %%es,%0" : "=r" (v)); segment->es.selector = v;
    segment->fs.selector = current->thread.fs;
    segment->gs.selector = current->thread.gs;
//    }
    mictc_store_tr(&(segment->tr.selector));
    get_tss_desc(smp_processor_id(), &(segment->tr.desc));
    store_gdt(&gdtr);
    store_idt(&idtr);
    mictc_store_ldt(&(segment->ldtr.selector));
    // LDT is not used, so zeros will be printed.

    TRACE_SPRINTF("\t\t\t<segment>\n");
    segreg = (struct mictc_seg *)&(segment->cs);

    for(i=0; i < MAX_SEG_REG; i++) {
        if (strcmp(SegRegNames[i], "GS") == 0) {
            segreg->base = tc_rdmsr(MSR_KGSBASE);
        }
        if (strcmp(SegRegNames[i], "FS") == 0) {
            segreg->base = tc_rdmsr(MSR_FSBASE);
        }

	// Fill in the segment descriptor for cs to gs
	if (i <= 5) {
	  get_seg_desc(smp_processor_id(), segreg->selector, &(segreg->desc));
	}

        TRACE_SPRINTF("\t\t\t\t<reg name=\"%s\">\n",SegRegNames[i]);
	if (i > 5) {		// LDT and TSS
	  struct mictc_tss *segreg1 =(struct mictc_tss *)segreg;

	  TRACE_SPRINTF("\t\t\t\t\t<attr name=\"base\">0x%llx</attr>\n", ((uint64_t)segreg1->desc.base3 << 32) | (uint64_t)((segreg1->desc.base2 << 24) | (segreg1->desc.base1 << 16) | segreg1->desc.base0));
	  TRACE_SPRINTF("\t\t\t\t\t<attr name=\"limit\">0x%x</attr>\n", (segreg1->desc.limit1 << 16) | segreg1->desc.limit0);
	  TRACE_SPRINTF("\t\t\t\t\t<selector>0x%x</selector>\n", segreg1->selector);
	  TRACE_SPRINTF("\t\t\t\t\t<attr name=\"G\">0x%x</attr>\n", segreg1->desc.g);
	  TRACE_SPRINTF("\t\t\t\t\t<attr name=\"DB\">0x%x</attr>\n", 0); // double word of base and limit
	  TRACE_SPRINTF("\t\t\t\t\t<attr name=\"L\">0x%x</attr>\n", 0);
	  TRACE_SPRINTF("\t\t\t\t\t<attr name=\"AVL\">0x0</attr>\n");//AVL bit not populated in the gdt[] array
	  TRACE_SPRINTF("\t\t\t\t\t<attr name=\"P\">0x%x</attr>\n", segreg1->desc.p);
	  TRACE_SPRINTF("\t\t\t\t\t<attr name=\"DPL\">0x%x</attr>\n", segreg1->desc.dpl);
	  TRACE_SPRINTF("\t\t\t\t\t<attr name=\"S\">0x%x</attr>\n", segreg1->desc.type & 0x10 ? 1 : 0); //The S bit (descriptor type) is clubbed along with the ssd_type element.
	  TRACE_SPRINTF("\t\t\t\t\t<attr name=\"TYPE\">0x%x</attr>\n", (segreg1->desc.type & 0xf));
	} else {
	  if (segreg->base) {
	    TRACE_SPRINTF("\t\t\t\t\t<attr name=\"base\">0x%llx</attr>\n", segreg->base);
	  } else {
	    TRACE_SPRINTF("\t\t\t\t\t<attr name=\"base\">0x%x</attr>\n", (segreg->desc.base2 << 24) | (segreg->desc.base1 << 16) |segreg->desc.base0);
	  }
	  if (segreg->desc.l) segreg->desc.a = 0;
	  TRACE_SPRINTF("\t\t\t\t\t<attr name=\"limit\">0x%x</attr>\n", (segreg->desc.limit << 16) | segreg->desc.limit0);
	  TRACE_SPRINTF("\t\t\t\t\t<selector>0x%x</selector>\n", segreg->selector);
	  TRACE_SPRINTF("\t\t\t\t\t<attr name=\"G\">0x%x</attr>\n", segreg->desc.g);
	  TRACE_SPRINTF("\t\t\t\t\t<attr name=\"DB\">0x%x</attr>\n", segreg->desc.a & 1); // double word of base and limit
	  TRACE_SPRINTF("\t\t\t\t\t<attr name=\"L\">0x%x</attr>\n", segreg->desc.l);
	  TRACE_SPRINTF("\t\t\t\t\t<attr name=\"AVL\">0x0</attr>\n");//AVL bit not populated in the gdt[] array
	  TRACE_SPRINTF("\t\t\t\t\t<attr name=\"P\">0x%x</attr>\n", segreg->desc.p);
	  TRACE_SPRINTF("\t\t\t\t\t<attr name=\"DPL\">0x%x</attr>\n", segreg->desc.dpl);
	  TRACE_SPRINTF("\t\t\t\t\t<attr name=\"S\">0x%x</attr>\n", segreg->desc.type & 0x10 ? 1 : 0); //The S bit (descriptor type) is clubbed along with the ssd_type element.
	  TRACE_SPRINTF("\t\t\t\t\t<attr name=\"TYPE\">0x%x</attr>\n", (segreg->desc.type & 0xf));
	}
	TRACE_SPRINTF("\t\t\t\t</reg>\n");
	segreg++;
    }

    TRACE_SPRINTF("\t\t\t\t<reg name=\"GDTR\">\n");
    TRACE_SPRINTF("\t\t\t\t\t<attr name=\"base\">0x%lx</attr>\n", gdtr.address);
    TRACE_SPRINTF("\t\t\t\t\t<attr name=\"limit\">0x%x</attr>\n", gdtr.size);
    TRACE_SPRINTF("\t\t\t\t</reg>\n");
    TRACE_SPRINTF("\t\t\t\t<reg name=\"IDTR\">\n");
    TRACE_SPRINTF("\t\t\t\t\t<attr name=\"base\">0x%lx</attr>\n", idtr.address);
    TRACE_SPRINTF("\t\t\t\t\t<attr name=\"limit\">0x%x</attr>\n", idtr.size);
    TRACE_SPRINTF("\t\t\t\t</reg>\n");

    TRACE_SPRINTF("\t\t\t</segment>\n");

//    printk("End of mictc_capture_segment_reg\n");

}


//------------------------------------------------------------------------------
//  FUNCTION: mictc_capture_debug_reg
//
//  DESCRIPTION:
//  Capture all debug registers.
//
//  PARAMETERS: None
//
//  RETURNS: None
//
//  TODOS:
//
static void
mictc_capture_debug_reg(void)
{
    TRACE_SPRINTF("\t\t\t<debug>\n");
    TRACE_SPRINTF("\t\t\t\t<reg name=\"DR0\">0x%x</reg>\n", get_dr(0));
    TRACE_SPRINTF("\t\t\t\t<reg name=\"DR1\">0x%x</reg>\n", get_dr(1));
    TRACE_SPRINTF("\t\t\t\t<reg name=\"DR2\">0x%x</reg>\n", get_dr(2));
    TRACE_SPRINTF("\t\t\t\t<reg name=\"DR3\">0x%x</reg>\n", get_dr(3));
// These don't exist.
//    TRACE_SPRINTF("\t\t\t\t<reg name=\"DR4\">0x%x</reg>\n", get_dr(4));
//    TRACE_SPRINTF("\t\t\t\t<reg name=\"DR5\">0x%x</reg>\n", get_dr(5));
    TRACE_SPRINTF("\t\t\t\t<reg name=\"DR6\">0x%x</reg>\n", get_dr(6));
    TRACE_SPRINTF("\t\t\t\t<reg name=\"DR7\">0x%x</reg>\n", get_dr(7));
    TRACE_SPRINTF("\t\t\t</debug>\n");
}


//------------------------------------------------------------------------------
//  FUNCTION: mictc_capture_control_reg
//
//  DESCRIPTION:
//  Capture all control registers.
//
//  PARAMETERS: None
//
//  RETURNS: None
//
//  TODOS:
//
static void
mictc_capture_control_reg(void)
{
    TRACE_SPRINTF("\t\t\t<control>\n");
    TRACE_SPRINTF("\t\t\t\t<reg name=\"CR0\">0x%lx</reg>\n", (read_cr0()) & 0xffffffff);
    TRACE_SPRINTF("\t\t\t\t<reg name=\"CR2\">0x%lx</reg>\n", read_cr2());
    TRACE_SPRINTF("\t\t\t\t<reg name=\"CR3\">0x%lx</reg>\n", read_cr3());
    TRACE_SPRINTF("\t\t\t\t<reg name=\"CR4\">0x%lx</reg>\n", (read_cr4()) & 0xffffffff);
    TRACE_SPRINTF("\t\t\t\t<reg name=\"CR8\">0x%lx</reg>\n", read_cr8());
    TRACE_SPRINTF("\t\t\t</control>\n");
}


//------------------------------------------------------------------------------
//  FUNCTION: mictc_capture_SPU_reg
//
//  DESCRIPTION:
//  Capture all SPU registers.
//
//  PARAMETERS: None
//
//  RETURNS: None
//
//  TODOS:
//
static void
mictc_capture_SPU_reg(void)
{
#if 0
  // FIXME - The SPU is not setup currently in Linux

  TRACE_SPRINTF("\t\t\t<spu>\n");
  TRACE_SPRINTF_SPU(SPU_XQ_SIZE);
  TRACE_SPRINTF_SPU(SPU_XQ_BASE);
  TRACE_SPRINTF_SPU(SPU_XQ_INDEX);
  TRACE_SPRINTF_SPU(SPU_CONTROL);
  TRACE_SPRINTF_SPU(SPU_SAMPLER_BASE);
  TRACE_SPRINTF_SPU(SPU_PMU_EVENT_SEL);
  TRACE_SPRINTF_SPU(SPU_CONTROL2);
  TRACE_SPRINTF_SPU(SPU_CONTROL3);
  TRACE_SPRINTF("\t\t\t</spu>\n");
#endif
}


//------------------------------------------------------------------------------
//  FUNCTION: PrintVector
//
//  DESCRIPTION:
//  Prints _m512 vectors
//
//  PARAMETERS: None
//
//  RETURNS: None
//
//  TODOS:
//
static void
PrintVector(u8 *res_mem, int reg_num)
{
    TRACE_SPRINTF("\t\t\t\t<reg name=\"V%d\">0x"
		  "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"
		  "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"
		  "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"
		  "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x</reg>\n",
                  reg_num,
                  res_mem[63], res_mem[62], res_mem[61], res_mem[60], res_mem[59], res_mem[58], res_mem[57], res_mem[56],
                  res_mem[55], res_mem[54], res_mem[53], res_mem[52], res_mem[51], res_mem[50], res_mem[49], res_mem[48],
                  res_mem[47], res_mem[46], res_mem[45], res_mem[44], res_mem[43], res_mem[42], res_mem[41], res_mem[40],
                  res_mem[39], res_mem[38], res_mem[37], res_mem[36], res_mem[35], res_mem[34], res_mem[33], res_mem[32],
                  res_mem[31], res_mem[30], res_mem[29], res_mem[28], res_mem[27], res_mem[26], res_mem[25], res_mem[24],
                  res_mem[23], res_mem[22], res_mem[21], res_mem[20], res_mem[19], res_mem[18], res_mem[17], res_mem[16],
                  res_mem[15], res_mem[14], res_mem[13], res_mem[12], res_mem[11], res_mem[10], res_mem[9], res_mem[8],
                  res_mem[7], res_mem[6], res_mem[5], res_mem[4], res_mem[3], res_mem[2], res_mem[1], res_mem[0]);
}


//------------------------------------------------------------------------------
//  FUNCTION: PrintFPRegister
//
//  DESCRIPTION:
//  Prints 10 byte FP register contents
//
//  PARAMETERS: None
//
//  RETURNS: None
//
//  TODOS:
//
static void
PrintFPRegister(u8 *res_mem, int reg_num)
{
    TRACE_SPRINTF("\t\t\t\t<reg name=\"FR%d\">0x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x</reg>\n",
                  reg_num,
                  res_mem[9],
                  res_mem[8],
                  res_mem[7],
                  res_mem[6],
                  res_mem[5],
                  res_mem[4],
                  res_mem[3],
                  res_mem[2],
                  res_mem[1],
                  res_mem[0]);
}


// VPU Instructions

#ifdef CONFIG_ML1OM
#define VSTORED_DISP32_EAX(v, disp32) " vstored %%v" #v "," #disp32 "(%%rax)\n"

#define VKSTORE_DISP32_EAX(k, disp32)	\
	"	vkmov  %%k" #k ",%%ebx\n" \
	"	movw %%bx, " #disp32 "(%%rax)\n"

#define STVXCSR_DISP32_EAX(disp32) "	stvxcsr " #disp32 "(%%rax)\n"

#else
// For K1OM
#define VSTORED_DISP32_EAX(v, disp32) " vpackstorelps %%zmm" #v "," #disp32 "(%%rax)\n"

#define VKSTORE_DISP32_EAX(k, disp32)	\
	"	kmov  %%k" #k ",%%ebx\n" \
	"	movw %%bx, " #disp32 "(%%rax)\n"

#define STVXCSR_DISP32_EAX(disp32) "	stmxcsr " #disp32 "(%%rax)\n"
#endif

static inline void save_vpu(struct vpustate_struct *vpustate) 
{ 
	asm volatile(
		VSTORED_DISP32_EAX(0, 0x00)
		VSTORED_DISP32_EAX(1, 0x40)
		VSTORED_DISP32_EAX(2, 0x80)
		VSTORED_DISP32_EAX(3, 0xc0)
		VSTORED_DISP32_EAX(4, 0x100)
		VSTORED_DISP32_EAX(5, 0x140)
		VSTORED_DISP32_EAX(6, 0x180)
		VSTORED_DISP32_EAX(7, 0x1c0)
		VSTORED_DISP32_EAX(8, 0x200)
		VSTORED_DISP32_EAX(9, 0x240)
		VSTORED_DISP32_EAX(10, 0x280)
		VSTORED_DISP32_EAX(11, 0x2c0)
		VSTORED_DISP32_EAX(12, 0x300)
		VSTORED_DISP32_EAX(13, 0x340)
		VSTORED_DISP32_EAX(14, 0x380)
		VSTORED_DISP32_EAX(15, 0x3c0)
		VSTORED_DISP32_EAX(16, 0x400)
		VSTORED_DISP32_EAX(17, 0x440)
		VSTORED_DISP32_EAX(18, 0x480)
		VSTORED_DISP32_EAX(19, 0x4c0)
		VSTORED_DISP32_EAX(20, 0x500)
		VSTORED_DISP32_EAX(21, 0x540)
		VSTORED_DISP32_EAX(22, 0x580)
		VSTORED_DISP32_EAX(23, 0x5c0)
		VSTORED_DISP32_EAX(24, 0x600)
		VSTORED_DISP32_EAX(25, 0x640)
		VSTORED_DISP32_EAX(26, 0x680)
		VSTORED_DISP32_EAX(27, 0x6c0)
		VSTORED_DISP32_EAX(28, 0x700)
		VSTORED_DISP32_EAX(29, 0x740)
		VSTORED_DISP32_EAX(30, 0x780)
		VSTORED_DISP32_EAX(31, 0x7c0)
		VKSTORE_DISP32_EAX(0, 0x800)
		VKSTORE_DISP32_EAX(1, 0x802)
		VKSTORE_DISP32_EAX(2, 0x804)
		VKSTORE_DISP32_EAX(3, 0x806)
		VKSTORE_DISP32_EAX(4, 0x808)
		VKSTORE_DISP32_EAX(5, 0x80a)
		VKSTORE_DISP32_EAX(6, 0x80c)
		VKSTORE_DISP32_EAX(7, 0x80e)
		STVXCSR_DISP32_EAX(0x810)
		: "=m" (vpustate) : [fx] "a" (vpustate) : "ebx"
	);
}


//------------------------------------------------------------------------------
//  FUNCTION: mictc_capture_vector_reg
//
//  DESCRIPTION:
//  Capture all vector registers.
//
//  PARAMETERS: None
//
//  RETURNS: None
//
//  TODOS:
//
static void
mictc_capture_vector_reg(struct vpustate_struct *vpustate)
{
  //  printk("vpustate = %p\n", vpustate);

    save_vpu(vpustate);

    TRACE_SPRINTF("\t\t\t<vpu>\n");
    TRACE_SPRINTF("\t\t\t\t<reg name=\"K0\">0x%x</reg>\n", vpustate->k[0]);
    TRACE_SPRINTF("\t\t\t\t<reg name=\"K1\">0x%x</reg>\n", vpustate->k[1]);
    TRACE_SPRINTF("\t\t\t\t<reg name=\"K2\">0x%x</reg>\n", vpustate->k[2]);
    TRACE_SPRINTF("\t\t\t\t<reg name=\"K3\">0x%x</reg>\n", vpustate->k[3]);
    TRACE_SPRINTF("\t\t\t\t<reg name=\"K4\">0x%x</reg>\n", vpustate->k[4]);
    TRACE_SPRINTF("\t\t\t\t<reg name=\"K5\">0x%x</reg>\n", vpustate->k[5]);
    TRACE_SPRINTF("\t\t\t\t<reg name=\"K6\">0x%x</reg>\n", vpustate->k[6]);
    TRACE_SPRINTF("\t\t\t\t<reg name=\"K7\">0x%x</reg>\n", vpustate->k[7]);
    TRACE_SPRINTF_VECTOR(0, vpustate->vector_space[0]);
    TRACE_SPRINTF_VECTOR(1, vpustate->vector_space[16]);
    TRACE_SPRINTF_VECTOR(2, vpustate->vector_space[32]);
    TRACE_SPRINTF_VECTOR(3, vpustate->vector_space[48]);
    TRACE_SPRINTF_VECTOR(4, vpustate->vector_space[64]);
    TRACE_SPRINTF_VECTOR(5, vpustate->vector_space[80]);
    TRACE_SPRINTF_VECTOR(6, vpustate->vector_space[96]);
    TRACE_SPRINTF_VECTOR(7, vpustate->vector_space[112]);
    TRACE_SPRINTF_VECTOR(8, vpustate->vector_space[128]);
    TRACE_SPRINTF_VECTOR(9, vpustate->vector_space[144]);
    TRACE_SPRINTF_VECTOR(10, vpustate->vector_space[160]);
    TRACE_SPRINTF_VECTOR(11, vpustate->vector_space[176]);
    TRACE_SPRINTF_VECTOR(12, vpustate->vector_space[192]);
    TRACE_SPRINTF_VECTOR(13, vpustate->vector_space[208]);
    TRACE_SPRINTF_VECTOR(14, vpustate->vector_space[224]);
    TRACE_SPRINTF_VECTOR(15, vpustate->vector_space[240]);
    TRACE_SPRINTF_VECTOR(16, vpustate->vector_space[256]);
    TRACE_SPRINTF_VECTOR(17, vpustate->vector_space[272]);
    TRACE_SPRINTF_VECTOR(18, vpustate->vector_space[288]);
    TRACE_SPRINTF_VECTOR(19, vpustate->vector_space[304]);
    TRACE_SPRINTF_VECTOR(20, vpustate->vector_space[320]);
    TRACE_SPRINTF_VECTOR(21, vpustate->vector_space[336]);
    TRACE_SPRINTF_VECTOR(22, vpustate->vector_space[352]);
    TRACE_SPRINTF_VECTOR(23, vpustate->vector_space[368]);
    TRACE_SPRINTF_VECTOR(24, vpustate->vector_space[384]);
    TRACE_SPRINTF_VECTOR(25, vpustate->vector_space[400]);
    TRACE_SPRINTF_VECTOR(26, vpustate->vector_space[416]);
    TRACE_SPRINTF_VECTOR(27, vpustate->vector_space[432]);
    TRACE_SPRINTF_VECTOR(28, vpustate->vector_space[448]);
    TRACE_SPRINTF_VECTOR(29, vpustate->vector_space[464]);
    TRACE_SPRINTF_VECTOR(30, vpustate->vector_space[480]);
    TRACE_SPRINTF_VECTOR(31, vpustate->vector_space[496]);
    TRACE_SPRINTF("\t\t\t\t<reg name=\"VXCSR\">0x%x</reg>\n", vpustate->vxcsr);
    TRACE_SPRINTF("\t\t\t</vpu>\n");
}

//------------------------------------------------------------------------------
//  FUNCTION: mictc_capture_FPU_reg
//
//  DESCRIPTION:
//  Capture all FPU registers.
//
//  PARAMETERS: None
//
//  RETURNS: None
//
//  TODOS:
//
static void
mictc_capture_FPU_reg(struct i387_fxsave_struct *fpu)
{

/*
    Get FPU contents from the registers instead of the PCB.
    fxsave on L1OM saves only the x87 FPU registers and not the SSE2 and MMX registers.
    For format of the data below refer Intel 64 and IA-32 Arch. SDM Vol 2A Instr Set Ref A-M
    tables 3-59 & 3-60.
*/
    mictc_fxsave(fpu);

    TRACE_SPRINTF("\t\t\t<fp>\n");
    TRACE_SPRINTF("\t\t\t\t<reg name=\"CW\">0x%x</reg>\n", fpu->cwd);
    TRACE_SPRINTF("\t\t\t\t<reg name=\"SW\">0x%x</reg>\n", fpu->swd);
    TRACE_SPRINTF("\t\t\t\t<reg name=\"TW\">0x%x</reg>\n", (fpu->twd));
    TRACE_SPRINTF("\t\t\t\t<reg name=\"FCS\">0x%x</reg>\n", (fpu->fcs & 0xffff));
    TRACE_SPRINTF("\t\t\t\t<reg name=\"OPCODE\">0x%x</reg>\n", fpu->fop);
    TRACE_SPRINTF("\t\t\t\t<reg name=\"FDS\">0x%x</reg>\n", (fpu->fos & 0xffff));
    TRACE_SPRINTF("\t\t\t\t<reg name=\"FIP\">0x%x</reg>\n", fpu->fip);
    TRACE_SPRINTF("\t\t\t\t<reg name=\"DATAOP\">0x%x</reg>\n", (fpu->foo));
    PrintFPRegister((u8 *)&(fpu->st_space[0]), 0);
    PrintFPRegister((u8 *)&(fpu->st_space[4]), 1);
    PrintFPRegister((u8 *)&(fpu->st_space[8]), 2);
    PrintFPRegister((u8 *)&(fpu->st_space[12]), 3);
    PrintFPRegister((u8 *)&(fpu->st_space[16]), 4);
    PrintFPRegister((u8 *)&(fpu->st_space[20]), 5);
    PrintFPRegister((u8 *)&(fpu->st_space[24]), 6);
    PrintFPRegister((u8 *)&(fpu->st_space[28]), 7);
    TRACE_SPRINTF("\t\t\t</fp>\n");

#if 0
    printk("00 %08x %08x\n", ((u32*)fpu)[0], ((u32*)fpu)[1]);
    printk("08 %08x %08x\n", ((u32*)fpu)[2], ((u32*)fpu)[3]);
    printk("10 %08x %08x\n", ((u32*)fpu)[4], ((u32*)fpu)[5]);
    printk("18 %08x %08x\n", ((u32*)fpu)[6], ((u32*)fpu)[7]);
    printk("20 %08x %08x\n", ((u32*)fpu)[8], ((u32*)fpu)[9]);
    printk("28 %08x %08x\n", ((u32*)fpu)[10], ((u32*)fpu)[11]);
    printk("30 %08x %08x\n", ((u32*)fpu)[12], ((u32*)fpu)[13]);
    printk("38 %08x %08x\n", ((u32*)fpu)[14], ((u32*)fpu)[15]);
    printk("40 %08x %08x\n", ((u32*)fpu)[16], ((u32*)fpu)[17]);
    printk("48 %08x %08x\n", ((u32*)fpu)[18], ((u32*)fpu)[19]);
    printk("50 %08x %08x\n", ((u32*)fpu)[20], ((u32*)fpu)[21]);
    printk("58 %08x %08x\n", ((u32*)fpu)[22], ((u32*)fpu)[23]);
    printk("60 %08x %08x\n", ((u32*)fpu)[24], ((u32*)fpu)[25]);
    printk("68 %08x %08x\n", ((u32*)fpu)[26], ((u32*)fpu)[27]);
    printk("70 %08x %08x\n", ((u32*)fpu)[28], ((u32*)fpu)[29]);
    printk("78 %08x %08x\n", ((u32*)fpu)[30], ((u32*)fpu)[31]);
    printk("80 %08x %08x\n", ((u32*)fpu)[32], ((u32*)fpu)[33]);
    printk("88 %08x %08x\n", ((u32*)fpu)[34], ((u32*)fpu)[35]);
    printk("90 %08x %08x\n", ((u32*)fpu)[36], ((u32*)fpu)[37]);
    printk("98 %08x %08x\n", ((u32*)fpu)[38], ((u32*)fpu)[39]);
#endif
}


//------------------------------------------------------------------------------
//  FUNCTION: mictc_capture_MSR
//
//  DESCRIPTION:
//  Capture all MSR
//
//  PARAMETERS: None
//
//  RETURNS: None
//
//  TODOS:
//
static void
mictc_capture_MSR(void)
{
  //    u32 me_cpu = PCPU_GET(cpuid);
#if 0
    //msr->msrMIC_CR_SPUBASE       = tc_rdmsr(MIC_CR_SPUBASE);
    //msr->msrIA32_CR_MISC         = tc_rdmsr(IA32_CR_MISC);
    //msr->msrWMT_CR_LASTBRANCH_0  = tc_rdmsr(WMT_CR_LASTBRANCH_0);
    //msr->msrWMT_CR_LASTBRANCH_1  = tc_rdmsr(WMT_CR_LASTBRANCH_1);
    msr->msrVMX_MSR_BASE         = tc_rdmsr(VMX_MSR_BASE);
    msr->msrVMX_MSR_BASE_PLUS_1  = tc_rdmsr(VMX_MSR_BASE_PLUS_1);
    msr->msrVMX_MSR_BASE_PLUS_2  = tc_rdmsr(VMX_MSR_BASE_PLUS_2);
    msr->msrVMX_MSR_BASE_PLUS_3  = tc_rdmsr(VMX_MSR_BASE_PLUS_3);
    msr->msrVMX_MSR_BASE_PLUS_4  = tc_rdmsr(VMX_MSR_BASE_PLUS_4);
    msr->msrVMX_MSR_BASE_PLUS_5  = tc_rdmsr(VMX_MSR_BASE_PLUS_5);
    msr->msrVMX_MSR_BASE_PLUS_6  = tc_rdmsr(VMX_MSR_BASE_PLUS_6);
    msr->msrVMX_MSR_BASE_PLUS_7  = tc_rdmsr(VMX_MSR_BASE_PLUS_7);
    msr->msrVMX_MSR_BASE_PLUS_8  = tc_rdmsr(VMX_MSR_BASE_PLUS_8);
    msr->msrVMX_MSR_BASE_PLUS_9  = tc_rdmsr(VMX_MSR_BASE_PLUS_9);
    msr->msrTIME                 = tc_rdmsr(TIME);
    msr->msrPINFO                = tc_rdmsr(PINFO);
#endif
    TRACE_SPRINTF("\t\t\t<msr>\n");
    TRACE_SPRINTF_MSR(P6_CR_TSC);
    TRACE_SPRINTF_MSR(X86_CR_APICBASE);
    TRACE_SPRINTF_MSR(CBOX_SPU_PA_MSR);
    // This is being added since it is included in the ITP dump as well.
    TRACE_SPRINTF("\t\t\t\t<reg addr=\"0x%x\">0x%llx</reg>\n", SPU_BASE, (tc_rdmsr(CBOX_SPU_PA_MSR) & 0x7fffffffffffffff) + 0x1000);
    TRACE_SPRINTF_MSR(CBOX_SPU_SAMPLER_BIND_MSR);
    TRACE_SPRINTF_MSR(X86_CR_MTRRphysMask0);
    TRACE_SPRINTF_MSR(X86_CR_MTRRphysMask1);
    TRACE_SPRINTF_MSR(X86_CR_MTRRphysMask2);
    TRACE_SPRINTF_MSR(X86_CR_MTRRphysMask3);
    TRACE_SPRINTF_MSR(X86_CR_MTRRphysMask4);
    TRACE_SPRINTF_MSR(X86_CR_MTRRphysMask5);
    TRACE_SPRINTF_MSR(X86_CR_MTRRphysMask6);
    TRACE_SPRINTF_MSR(X86_CR_MTRRphysMask7);
    TRACE_SPRINTF_MSR(MSR_EFER & ~0x800); // Force bit 11 to 0
    TRACE_SPRINTF_MSR(MSR_SF_MASK);
    TRACE_SPRINTF_MSR(MSR_FSBASE);
    TRACE_SPRINTF_MSR(MSR_GSBASE);
    TRACE_SPRINTF_MSR(X86_CR_MTRRcap);
    TRACE_SPRINTF_MSR(X86_CR_MTRRdefType);
    TRACE_SPRINTF_MSR(X86_CR_MTRRphysBase2);
    TRACE_SPRINTF_MSR(X86_CR_MTRRphysBase0);
    TRACE_SPRINTF_MSR(X86_CR_MTRRphysBase1);
    TRACE_SPRINTF_MSR(X86_CR_MTRRphysBase3);
    TRACE_SPRINTF_MSR(X86_CR_MTRRphysBase4);
    TRACE_SPRINTF_MSR(X86_CR_MTRRphysBase5);
    TRACE_SPRINTF_MSR(X86_CR_MTRRphysBase6);
    TRACE_SPRINTF_MSR(X86_CR_MTRRphysBase7);
    TRACE_SPRINTF_MSR(STAR);
    TRACE_SPRINTF_MSR(LSTAR);

    // MSR_KGSBASE needs some special handling.
    // On Silicon when a thread transitions from Ring 3->Ring 0 the
    // first instruction it executes is swapgs which swaps the value
    // of the current GSBase (which could be 0x0) with the value in
    // MSR_KGSBASE to get to the per cpu data structure and onwards to the kernel stack.
    // On Silicon, when the same thread transitions from Ring 0->Ring 3 MSR_KGSBASE gets
    // the right value as a result of another swapgs on the way back.
    // Where Trace Capture differs from Silicon is that we take a snapshot while executing
    // in Ring 0 (when MSR_KGSBASE could be 0x0) but the first instruction
    // which executes on LarrySim is a Ring 3 instruction.
    // On the first syscall in LarrySim when it executes a swapgs it sees a MSR_KGSBASE value of 0x0.
    // LarrySim cannot get to the kernel stack and we correctly hit a double fault (Bang!).
    // The correct fix is to ensure that LarrySim sees a correct value of
    // MSR_KGSBASE when it is provided a snapshot.
//FIXME
//    TRACE_SPRINTF("\t\t\t\t<reg addr=\"0x%x\">0x%lx</reg>\n", MSR_KGSBASE, &__pcpu[me_cpu]);

    // The following MSR's are currently ifdef'd out
    // because LarrySim barfs on these.
    // We might need these later.
#if 0
    TRACE_SPRINTF_MSR(X86_CR_MTRRfix64K_00000);
    TRACE_SPRINTF_MSR(X86_CR_MTRRfix16K_80000);
    TRACE_SPRINTF_MSR(X86_CR_MTRRfix16K_A0000);
    TRACE_SPRINTF_MSR(X86_CR_MTRRfix4K_C0000);
    TRACE_SPRINTF_MSR(X86_CR_MTRRfix4K_C8000);
    TRACE_SPRINTF_MSR(X86_CR_MTRRfix4K_D0000);
    TRACE_SPRINTF_MSR(X86_CR_MTRRfix4K_D8000);
    TRACE_SPRINTF_MSR(X86_CR_MTRRfix4K_E0000);
    TRACE_SPRINTF_MSR(X86_CR_MTRRfix4K_E8000);
    TRACE_SPRINTF_MSR(X86_CR_MTRRfix4K_F0000);
    TRACE_SPRINTF_MSR(X86_CR_MTRRfix4K_F8000);
    TRACE_SPRINTF_MSR(P5_MC_ADDR);
    TRACE_SPRINTF_MSR(P5_MC_TYPE);
    TRACE_SPRINTF_MSR(MSR_TR1);
    TRACE_SPRINTF_MSR(MSR_TR2);
    TRACE_SPRINTF_MSR(MSR_TR3);
    TRACE_SPRINTF_MSR(MSR_TR4);
    TRACE_SPRINTF_MSR(MSR_TR5);
    TRACE_SPRINTF_MSR(MSR_TR6);
    TRACE_SPRINTF_MSR(MSR_TR7);
    TRACE_SPRINTF_MSR(MSR_TR9);
    TRACE_SPRINTF_MSR(MSR_TR10);
    TRACE_SPRINTF_MSR(MSR_TR11);
    TRACE_SPRINTF_MSR(MSR_TR12);
    TRACE_SPRINTF_MSR(IA32_APIC_BASE);
    TRACE_SPRINTF_MSR(IA32_TIME_STAMP_COUNTER);
    TRACE_SPRINTF_MSR(IA32_PerfCntr0);
    TRACE_SPRINTF_MSR(IA32_PerfCntr1);
    TRACE_SPRINTF_MSR(IA32_PerfCntr2);
    TRACE_SPRINTF_MSR(IA32_PerfCntr3);
    TRACE_SPRINTF_MSR(PerfFilteredCntr0);
    TRACE_SPRINTF_MSR(PerfFilteredCntr1);
    TRACE_SPRINTF_MSR(PerfFilteredCntr2);
    TRACE_SPRINTF_MSR(PerfFilteredCntr3);
    TRACE_SPRINTF_MSR(IA32_PerfEvtSel0);
    TRACE_SPRINTF_MSR(IA32_PerfEvtSel1);
    TRACE_SPRINTF_MSR(IA32_PerfEvtSel2);
    TRACE_SPRINTF_MSR(IA32_PerfEvtSel3);
    TRACE_SPRINTF_MSR(PerfFilterMask);
    TRACE_SPRINTF_MSR(IA32_PERF_GLOBAL_STATUS);
    TRACE_SPRINTF_MSR(IA32_PERF_GLOBAL_OVF_CONTROL);
    TRACE_SPRINTF_MSR(IA32_PERF_GLOBAL_CTRL);    
    TRACE_SPRINTF_MSR(IA32_MCG_CTL);
    TRACE_SPRINTF_MSR(IA32_MC0_CTRL);
    TRACE_SPRINTF_MSR(IA32_MC0_STAT);
    TRACE_SPRINTF_MSR(IA32_MC0_ADDR);
    TRACE_SPRINTF_MSR(IA32_MC0_MISC);
    TRACE_SPRINTF_MSR(IA32_MC1_CTRL);
    TRACE_SPRINTF_MSR(IA32_MC1_STAT);
    TRACE_SPRINTF_MSR(IA32_MC1_ADDR);
    TRACE_SPRINTF_MSR(IA32_MC1_MISC);
    TRACE_SPRINTF_MSR(SYSCALL_FLAG_MASK);
    TRACE_SPRINTF_MSR(X86_PAT);
#endif
    TRACE_SPRINTF("\t\t\t</msr>\n");
}


//u64 rdtsccount = 0, dmasetuptime = 0, dmacomptime=0, hostacktime=0;

#if MIC_TRACE_CAPTURE_MEMORY_TEST
// Local function to count the number of bytes in a U32
// This is only used for the memory test.
static U32 AddBytes(U32 add)
{
    U32 sum = 0x0;
    for (int i=0; i < sizeof(U32); i++)
    {
        sum += (add & 0xFF);
        add = (add >> 8);
    }
    return sum;
}
#endif


//------------------------------------------------------------------------------
//  FUNCTION: mictc_capture_memory
//
//  DESCRIPTION:
//  Trace Capture IPI Handler
//
//  PARAMETERS: None
//
//  RETURNS: None
//
//  TODOS:
//
static int
mictc_capture_memory(void)
{
	long err;
	long i;
	long delay_count;
	long total_transfered = 0;

	g_sizeXferred = 0;

	// Transfer a full buffer.
	for (i = 0; total_transfered < (max_pfn << PAGE_SHIFT); i++) {
	  printk("before scif_writeto, i = %ld\n", i);

	  // Transfer any remainder
	  if ((max_pfn << PAGE_SHIFT) - total_transfered < MICTC_MEM_BUFFER_SIZE) {
	    long remainder = ((uint64_t)max_pfn <<  PAGE_SHIFT) % MICTC_MEM_BUFFER_SIZE;

	    printk("Writing %ld bytes, max_pfn = %ld\n", remainder, max_pfn);

	    if ((err = scif_writeto(mictc_endp_data, scif_offset_mem + (i * MICTC_MEM_BUFFER_SIZE),
				    remainder, scif_offset_dst, 0)) < 0) {
	      pr_crit("%s:%s:%d scif_writeto failed with error %ld\n", __FILE__, __FUNCTION__, __LINE__, err);
	      return 1;
	    }
	    total_transfered += remainder;
	    g_sizeXferred = remainder;
	  } else {
	    if ((err = scif_writeto(mictc_endp_data, scif_offset_mem + (i * MICTC_MEM_BUFFER_SIZE),
				    MICTC_MEM_BUFFER_SIZE, scif_offset_dst, 0)) < 0) {
	      pr_crit("%s:%s:%d scif_writeto failed with error %ld\n", __FILE__, __FUNCTION__, __LINE__, err);
	      return 1;
	    }
	    total_transfered += MICTC_MEM_BUFFER_SIZE;
	    g_sizeXferred = MICTC_MEM_BUFFER_SIZE;
	  }
	  *g_traceBufferSizeOffset = g_sizeXferred;
	  printk("before fence\n");
	  err = scif_fence_signal(mictc_endp_data, (off_t)scif_offset_xml + TRACE_STATUS_OFFSET,
				  TRACE_PAGE_READY, 0, 0, SCIF_FENCE_INIT_SELF | SCIF_SIGNAL_LOCAL);

	  if (err < 0) {
	    printk("scif_fence_signal failed.  err = %ld\n", err);
	    return 1;
	  }
	  printk("TRACE_PAGE_READY %lld bytes\n", g_sizeXferred);
	  g_sizeXferred = 0;

	  delay_count = 0;
	  printk("waiting for TRACE_HOST_READY\n");

	  while (*g_traceBufferStatusOffset != TRACE_HOST_READY) {
	    cpu_relax();
	    delay_count++;
	    if (delay_count == TRACE_CAPTURE_TIMEOUT) {
	      printk("Memory Dump Timeout. Host did not update @physAddr 0x%lx\n", i << PAGE_SHIFT);
	      return -EBUSY;
	    }
	  }
	}
      *g_traceBufferSizeOffset = 0;
      *g_traceBufferStatusOffset = TRACE_MEM_COMPLETE;

      delay_count = 0;

      while (*g_traceBufferStatusOffset != TRACE_COMPLETE) {
	cpu_relax();
	delay_count++;
	if (delay_count == TRACE_CAPTURE_TIMEOUT) {
	  printk("Trace completion timeout.\n");
	  return -EBUSY;
	}
      }

      return 0;
}


//------------------------------------------------------------------------------
//  FUNCTION: mictc_trace_capture
//
//  DESCRIPTION:
//  Perform all the tasks related to Trace Capture
//  for a particular Hardware Thread.
//  The tasks currently include:
//  	General purpose registers
//	Segment registers
//	Debug registers
//	Control registers
//	VPU registers
//	MSRs
//
//	Note:  The SPU is not setup in Linux.
//
//  PARAMETERS: regs - pointer to the task's registers
//
//  RETURNS: None
//
//  TODOS:
//
static void
mictc_trace_capture(struct pt_regs *regs)
{
    long delay_count;

//    printk("Entering mictc_trace_capture on cpu %d, for process = %s\n", smp_processor_id(), current->comm);

    // Logic to let threads in one by one in order

    while (atomic_read(&cpus_stopped) != smp_processor_id()) {
	    cpu_relax();
//STH	    touch_nmi_watchdog();
    }

    if (smp_processor_id() == 0)
    {
        // CPU0 is responsible for preparing the
        // Trace Capture Header.
	mictc_prep_header();
    }

    TRACE_SPRINTF("\t\t<cpu num=\"%d\">\n", smp_processor_id());
    mictc_capture_general_purpose_reg(regs);
    mictc_capture_segment_reg(&(trace->segment), regs);
    mictc_capture_debug_reg();
    mictc_capture_control_reg();
    mictc_capture_vector_reg(&(trace->vpustate));

//STH    touch_nmi_watchdog();	// Just to be safe

    // The SPU is not setup currently in Linux
    if (always_false) mictc_capture_SPU_reg();

    mictc_capture_FPU_reg(&(trace->fpu));
    mictc_capture_MSR();

//    printk("In mictc_trace_capture on cpu %d, after MSRs\n", smp_processor_id());

    TRACE_SPRINTF("\t\t</cpu>\n");

    // Each core should flush their caches
    // as the initiator is going to take a memory
    // dump soon after.
    // Not required since DMA should snoop the caches.
    //wbinvd();

//    printk("In mictc_trace_capture on cpu %d, before check for last cpu\n", smp_processor_id());

    if (smp_processor_id() == (num_online_cpus() - 1))
    {
        // The last CPU is responsible for preparing the
        // Trace Capture Trailer.
        TRACE_SPRINTF("\t</cpu_state>\n");

        TRACE_SPRINTF("</arch_data>\n");

        // Update the size as the Host App needs this information.
        *g_traceBufferSizeOffset = g_sizeXferred;

        g_sizeXferred = 0;

        // Update the status for the Host App. The CPU register state has been written by all
        // the hardware threads. The host app polls for this status.
        *g_traceBufferStatusOffset = TRACE_REG_COMPLETE;

        printk("Completed Arch Dump. Now Beginning Memory Dump. Be patient (~1 min is ETA)..\n");

	delay_count = 0;

        while (*g_traceBufferStatusOffset != TRACE_GET_FILE)
        {
	    cpu_relax();
            delay_count++;
            if (delay_count == TRACE_CAPTURE_TIMEOUT)
            {
                printk("Arch Dump Timeout. Host did not update status.\n");
                break;
            }
        }
	printk("%s out of wait loop.\n", __FUNCTION__);
    }

//    printk("Exiting mictc_trace_capture on cpu %d\n", smp_processor_id());
}


// Starting point for trace_capture.
static void
mictc_start_capture(void)
{
	long ret;
	long err;
        struct scif_portID portID_data;
	int control_msg = 0;
	int i;
	int found_it = 0;

	spin_lock(&mictc_lock);
	printk("Starting tracecapture on cpu %d.  Taking lock.\n", smp_processor_id());

	if (!(g_traceBufferAllocated = kmalloc(MICTC_XML_BUFFER_SIZE, GFP_KERNEL))) {
		pr_crit("%s:%s:%d kmalloc failed failed with ENOMEM\n", __FILE__, __FUNCTION__, __LINE__);
		goto done0;
	}

	pr_crit("%s:%s:%d kmalloc returned %llx\n", __FILE__, __FUNCTION__, __LINE__, (uint64_t)g_traceBufferAllocated);

	g_traceBufferStatusOffset  = (u64*)((u64)g_traceBufferAllocated + TRACE_STATUS_OFFSET);
	g_traceBufferSizeOffset    = (u64*)((u64)g_traceBufferAllocated + TRACE_SIZE_OFFSET);
	g_traceBufferDataOffset    = (u32*)((u64)g_traceBufferAllocated + TRACE_DATA_OFFSET);
	g_traceBufferTriggerOffset  = (u32*)((u64)g_traceBufferAllocated + TRACE_TRIGGER_OFFSET);

	*g_traceBufferStatusOffset = TRACE_DATA;
#if MIC_TRACE_CAPTURE_MEMORY_TEST
	g_traceBufferChecksumOffset  = (u64*)((u64)g_traceBufferAllocated + TRACE_CHECKSUM_OFFSET);
#endif

	if (!(trace = (struct mictc_trace *)kmalloc(sizeof(struct mictc_trace), GFP_KERNEL))) {
		pr_crit("%s:%s:%d kmalloc failed failed with ENOMEM\n", __FILE__, __FUNCTION__, __LINE__);
		goto done1a;
	}

	pr_crit("%s:%s:%d kmalloc returned %llx\n", __FILE__, __FUNCTION__, __LINE__, (uint64_t)trace);

	memset(trace, 0, sizeof(struct mictc_trace));

	pr_crit("g_traceBufferStatusOffset %llx\n", (uint64_t)g_traceBufferStatusOffset);
	pr_crit("g_traceBufferSizeOffset   %llx\n", (uint64_t)g_traceBufferSizeOffset);
	pr_crit("g_traceBufferDataOffset   %llx\n", (uint64_t)g_traceBufferDataOffset);

	// Data channel
	if (!(mictc_endp_data = scif_open())) {
		pr_crit("%s:%s:%d scif_open failed with ENOMEM\n", __FILE__, __FUNCTION__, __LINE__);
		return;
	}

	if ((ret = scif_bind(mictc_endp_data, MICTC_SCIF_PORT_DATA)) < 0) {
		pr_crit("%s:%s:%d scif_bind failed with error %ld\n", __FILE__, __FUNCTION__, __LINE__, ret);
		goto done1;
	}

	portID_data.node = 0;
	portID_data.port = MICTC_SCIF_PORT_DATA;

	if ((ret = scif_connect(mictc_endp_data, &portID_data)) < 0) {
		pr_crit("%s:%s:%d scif_connect failed with error %ld\n", __FILE__, __FUNCTION__, __LINE__, ret);
		goto done1;
	}

	if ((ret = (long)scif_register(mictc_endp_data,
				       g_traceBufferAllocated,
				       MICTC_XML_BUFFER_SIZE,
				       0,             // suggested_offset,
				       SCIF_PROT_READ | SCIF_PROT_WRITE,
				       SCIF_MAP_KERNEL)) < 0) {
	  if (ret > -300) {
	    pr_crit("%s:%s:%d scif_register failed failed with %ld\n", __FILE__, __FUNCTION__, __LINE__, ret);
	    goto done2;
	  }
	}
	scif_offset_xml = ret;
	pr_crit("%s:%s:%d scif_register scif_offset_xml = %lx\n", __FILE__, __FUNCTION__, __LINE__, scif_offset_xml);

	// Register all of physical memory.
	if ((ret = (long)scif_register(mictc_endp_data,
				       __va(0),	// Physical page 0
				       max_pfn  << PAGE_SHIFT,
				       0, 	// suggested_offset,
				       SCIF_PROT_READ | SCIF_PROT_WRITE,
				       SCIF_MAP_KERNEL)) < 0) {
	  if (ret > -300) {
	    pr_crit("%s:%s:%d scif_register failed failed with %ld\n", __FILE__, __FUNCTION__, __LINE__, ret);
	    goto done2;
	  }
	}
	scif_offset_mem = ret;
	pr_crit("%s:%s:%d scif_register scif_offset_mem = %lx\n", __FILE__, __FUNCTION__, __LINE__, scif_offset_mem);

	BARRIER(mictc_endp_data, "before barrier");

	if ((err = scif_recv(mictc_endp_data, &scif_offset_dst, sizeof(scif_offset_dst), SCIF_RECV_BLOCK)) <= 0) {
	  pr_crit("%s:%s:%d scif_recv failed with err %ld\n", __FILE__, __FUNCTION__, __LINE__, err);
	  goto close;
	}

//	g_traceBufferDataOffset = (u32 *)ret;
//	pr_crit("%s:%s:%d scif_register ret %lx\n", __FILE__, __FUNCTION__, __LINE__, scif_offset);

	if ((err = scif_send(mictc_endp_data, &scif_offset_xml, sizeof(scif_offset_xml), SCIF_SEND_BLOCK)) <= 0) {
	  pr_crit("%s:%s:%d scif_send failed with err %ld\n", __FILE__, __FUNCTION__, __LINE__, err);
	  goto close;
	}

	while (*g_traceBufferStatusOffset != TRACE_HOST_READY)
	  {
	    msleep(100);
	    touch_nmi_watchdog();
	  }

	// Get trigger data.
	for (i = 0; i < TRACE_TRIGGER_MAX; i++) {
	  g_traceTriggers[i] = *g_traceBufferTriggerOffset;
	  printk("Found trace trigger %d\n", g_traceTriggers[i]);
	  g_traceBufferTriggerOffset++;

	  if (g_traceTriggers[i] == TRACE_EOL) break;
	}

	// Is the trigger data empty?  If so, accept everything.
	if (g_traceTriggers[0] == TRACE_EOL) {
		printk("Trace trigger data is empty.\n");
		found_it = 1;
	} else if (g_traceTriggers[0] == TRACE_IGNORE) {
		printk("Ignoring current trace.");
	} else {
		// See if g_traceCurrentTrigger is in the trigger data.
		// If not, abort this trace.
		for (i = 0; i < TRACE_TRIGGER_MAX; i++) {
			if (g_traceTriggers[i] == TRACE_EOL) break;

			if (g_traceTriggers[i] == g_traceCurrentTrigger) {
	  			found_it = 1;
				printk("Matched trace trigger %d\n", g_traceTriggers[i]);
				break;
			}
		}
	}

	if (!found_it) {
		// Abort this trace
		printk("Trace trigger did not match -- aborting.\n");
		*g_traceBufferStatusOffset = TRACE_ABORTED;
		goto done3;
	}

	if (always_false) {
	  // Mmap memory at 0xfee03000 physical.
	  spu_addr = ioremap(0xfee03000, 0x1000);
	  if (! spu_addr) {
	    pr_crit("%s ioremap failed.\n", __FUNCTION__);
	    goto done3;
	  }
	  printk("CPU ioremap %p\n", spu_addr);
	}

	cli;			// Interrupts off
	atomic_set(&cpus_stopped, 0);
	atomic_set(&cpus_released, 0);
	// Send IPI to capture all other cpus.
	apic->send_IPI_allbutself(NMI_VECTOR);
	mictc_trace_capture(task_pt_regs(current));
	atomic_inc(&cpus_stopped);

	pr_debug("start_capture:  Entering wait loop until lock count %d >= %d on cpu %d\n", atomic_read(&cpus_stopped), num_online_cpus() - 1, smp_processor_id());

	{ int ctr = 0;
	  // Wait for every other CPU to finish its trace capture tasks.
	  while (atomic_read(&cpus_stopped) < num_online_cpus()) {
	    cpu_relax();
//STH	    touch_nmi_watchdog();
	    if (ctr++ > 1000000) {
	      ctr = 0;
	      printk("%s:%d *** waiting loop cpus_stopped = %d\n", __FUNCTION__, __LINE__, atomic_read(&cpus_stopped));
	    }
	  }
	}

	printk("%s out of wait loop.\n", __FUNCTION__);

        // Get a memory dump here before exiting.
        err = mictc_capture_memory();

        printk("Completed Memory Dump.\n");
//        printk("Completed Memory Dump. DMASetuptime = %ld , DMATime = %ld, HostAckTime = %ld\n", dmasetuptime, dmacomptime, hostacktime);

	// Now release all cores.
	atomic_set(&cpus_stopped, num_online_cpus() + 1);

	// Wait for every other CPU to be released
	while (atomic_read(&cpus_released) < num_online_cpus() - 1) {
	  //	  msleep(2000);
	  cpu_relax();
	  touch_nmi_watchdog();
	}
	sti;			// Interrupts on

	// FIXME This cleanup probably needs to be checked.
 close:
	if (always_false) {
	  iounmap(spu_addr);
	}
 done3:
//	scif_unregister(mictc_endp_data, scif_offset, MICTC_XML_BUFFER_SIZE);
 done2:
 done1:
	scif_close(mictc_endp_data);
	kfree(trace);
 done1a:
	kfree(g_traceBufferAllocated);
	spin_unlock(&mictc_lock);
 done0:
	printk("Ending tracecapture on cpu %d.  Releasing lock.\n", smp_processor_id());
}
EXPORT_SYMBOL(mictc_start_capture);


/*
 * mictc_handle_exception() - main entry point from a kernel exception
 *
 * Locking hierarchy:
 *	interface locks, if any (begin_session)
 */
int
mictc_handle_exception(int evector, int signo, int ecode, struct pt_regs *regs)
{
	// Interrupts are off.

	//	printk("Entering mictc_handle_exception on cpu %d  pid: %d, name: %s\n", smp_processor_id(), current->pid, current->comm);

	mictc_trace_capture(regs);
	atomic_inc(&cpus_stopped);
	pr_debug("handler:  Entering wait loop until lock count %d >= %d on cpu %d\n", atomic_read(&cpus_stopped), num_online_cpus() - 1, smp_processor_id());
	// Wait for every other CPU to finish its Trace Capture Tasks.
	// This test is for num_online_cpus+1 to hold all threads that are
	// in interrupt context so that the main thread can dump memory.
	while (atomic_read(&cpus_stopped) < num_online_cpus() + 1) {
	  cpu_relax();
//STH	  touch_nmi_watchdog();
	}

	atomic_inc(&cpus_released);

	printk("Exiting mictc_handle_exception on cpu %d %s\n", smp_processor_id(), current->comm);
	return 1;
}


static int __mictc_notify(struct die_args *args, unsigned long cmd)
{
	struct pt_regs *regs = args->regs;
#if 0
	switch (cmd) {
	case DIE_NMI:
		if (atomic_read(&kgdb_active) != -1) {
			/* KGDB CPU roundup */
			kgdb_nmicallback(smp_processor_id(), regs);
			was_in_debug_nmi[smp_processor_id()] = 1;
			touch_nmi_watchdog();
			return NOTIFY_STOP;
		}
		return NOTIFY_DONE;

	case DIE_NMIUNKNOWN:
		if (was_in_debug_nmi[smp_processor_id()]) {
			was_in_debug_nmi[smp_processor_id()] = 0;
			return NOTIFY_STOP;
		}
		return NOTIFY_DONE;

	case DIE_DEBUG:
		if (atomic_read(&kgdb_cpu_doing_single_step) != -1) {
			if (user_mode(regs))
				return single_step_cont(regs, args);
			break;
		} else if (test_thread_flag(TIF_SINGLESTEP))
			/* This means a user thread is single stepping
			 * a system call which should be ignored
			 */
			return NOTIFY_DONE;
		/* fall through */
	default:
		if (user_mode(regs))
			return NOTIFY_DONE;
	}
#endif
	if (cmd == DIE_NMI) {
	  if (mictc_handle_exception(args->trapnr, args->signr, cmd, regs)) {
	    touch_nmi_watchdog();
	    return NOTIFY_STOP;
	  }
	} else {
	  touch_nmi_watchdog();
	  return NOTIFY_DONE;
	}

	/* Must touch watchdog before return to normal operation */
	touch_nmi_watchdog();
	return NOTIFY_STOP;
}


static int
mictc_notify(struct notifier_block *self, unsigned long cmd, void *ptr)
{
	unsigned long flags;
	int ret;

	local_irq_save(flags);
	ret = __mictc_notify(ptr, cmd);
	local_irq_restore(flags);

	return ret;
}


/* 
 * This function is called whenever a process tries to do an ioctl on our
 * device file. We get two extra parameters (additional to the inode and file
 * structures, which all device functions get): the number of the ioctl called
 * and the parameter given to the ioctl function.
 *
 * If the ioctl is write or read/write (meaning output is returned to the
 * calling process), the ioctl call returns the output of this function.
 *
 */
long device_ioctl(
		 struct file *file,	/* ditto */
		 unsigned int ioctl_num,	/* number and param for ioctl */
		 unsigned long ioctl_param)
{
	// Switch according to the ioctl called
	switch (ioctl_num) {
	case MICTC_START_CAPTURE:

		// ioctl_param contains the trace trigger number.
		// Save it to check against the g_traceTrigger array.
		g_traceCurrentTrigger = (u32)ioctl_param;
		printk("IOCTL trace trigger %ld\n", ioctl_param);
		mictc_start_capture();
		break;
	default:
		printk("Invalid ioctl.\n");
		return -ENXIO;
	}
	return 0;
}


/* 
 * This is called whenever a process attempts to open the device file 
 */
static int device_open(struct inode *inode, struct file *file)
{
#ifdef DEBUG
	printk(KERN_INFO "device_open(%p)\n", file);
#endif

	/* 
	 * We don't want to talk to two processes at the same time 
	 */
	if (Device_Open)
		return -EBUSY;

	Device_Open++;
	try_module_get(THIS_MODULE);
	return 0;
}

static int device_release(struct inode *inode, struct file *file)
{
#ifdef DEBUG
	printk(KERN_INFO "device_release(%p,%p)\n", inode, file);
#endif

	/* 
	 * We're now ready for our next caller 
	 */
	Device_Open--;

	module_put(THIS_MODULE);
	return 0;
}


/* 
 * This structure will hold the functions to be called
 * when a process does something to the device we
 * created. Since a pointer to this structure is kept in
 * the devices table, it can't be local to
 * init_module. NULL is for unimplemented functions. 
 */
struct file_operations Fops = {
  //	.read = device_read,
  //	.write = device_write,
	.unlocked_ioctl = device_ioctl,
	.open = device_open,
	.release = device_release,	/* a.k.a. close */
};

static struct notifier_block mictc_notifier = {
	.notifier_call	= mictc_notify,
	.priority = 0x7fffffff /* we need to be notified first */
};


/*
 *	mictc_init - Register our notifier
 *
 */
static
int mictc_init(void)
{
	int ret_val;
	/* 
	 * Register the character device (atleast try) 
	 */
	ret_val = register_chrdev(MICTC_MAJOR_NUM, MICTC_DEVICE_NAME, &Fops);

	/* 
	 * Negative values signify an error 
	 */
	if (ret_val < 0) {
		printk(KERN_ALERT "%s failed with %d\n",
		       "Sorry, registering the character device ", ret_val);
		return ret_val;
	}

	printk(KERN_INFO "%s The major device number is %d.\n",
	       "Registeration is a success", MICTC_MAJOR_NUM);
	printk(KERN_INFO "To use trace capture you'll have to create a device file:\n");
	printk(KERN_INFO "mknod %s c %d 0\n", MICTC_FILE_NAME, MICTC_MAJOR_NUM);

	return register_die_notifier(&mictc_notifier);

}


static
void mictc_exit(void)
{
	return;
}

module_init(mictc_init);
module_exit(mictc_exit);

MODULE_AUTHOR("Intel Corp. 2011 (sth " __DATE__ ") ver " TC_VER);
MODULE_DESCRIPTION("Trace Capture module for K1OM");
MODULE_LICENSE("GPL");
