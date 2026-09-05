/* SPDX-License-Identifier: BSD-2-Clause */
#ifndef __ZHIHE_A210_H__
#define __ZHIHE_A210_H__

#include <sbi/sbi_const.h>
#include <thead/c9xx_encoding.h>

/* Single-die DEV topology; this code does not describe the D2D variant. */
#define A210_CLUSTER_COUNT		2
#define A210_CORES_PER_CLUSTER		4
#define A210_HART_COUNT			(A210_CLUSTER_COUNT * A210_CORES_PER_CLUSTER)
#define A210_C908_CLUSTER		0
#define A210_C920_CLUSTER		1

/*
 * CPU_SS_SYSREG reset-vector pairs: CLUSTER{0,1}_CORE0_RVBA_L/H.
 * See vendor U-Boot board/zhihe/a210-evb/cpusys/include/
 * AP_MAP_CPU_SS_SYSREG.h. Each core has adjacent 32-bit low/high words.
 */
#define A210_C908_RVBA_BASE		_UL(0x10148040)
#define A210_C920_RVBA_BASE		_UL(0x10148060)
#define A210_RVBA_STRIDE			8
#define A210_RVBA_HIGH_OFFSET		4
#define A210_RVBA_HIGH_SHIFT		32

/*
 * CPU cluster reset controls from the A210 vendor release sequence
 * (OpenSBI d9cfcff67e68, fdt_reset_zhihe.c and its firmware DT).
 * First release the cluster alone, attach its coherent fabric interface,
 * then release the four cores. Keep these hardware values, not a generic
 * DT script describing register writes.
 */
#define A210_C908_RESET_CONTROL		_UL(0x10144004)
#define A210_C920_RESET_CONTROL		_UL(0x10144008)
#define A210_RELEASE_CLUSTER_ONLY	0x01
#define A210_RELEASE_CLUSTER_AND_CORES	0x1f
/* CPU_SS_RSTGEN.html: active-low top reset bit 0, core[0..3] bits [1..4]. */
#define A210_CORE_RESET_SHIFT		1
#define A210_CORE_RESET_RELEASE(core)	(_UL(1) << ((core) + A210_CORE_RESET_SHIFT))
#define A210_CORE_RESET_MASK		(A210_RELEASE_CLUSTER_AND_CORES & \
					 ~A210_RELEASE_CLUSTER_ONLY)

/*
 * NCORE ACE1 is the C920 cluster's coherent-fabric interface on die 0.
 * XAIUTCR bit 9 requests attachment; XAIUTAR bit 5 acknowledges it.
 * These offsets/masks preserve the vendor CLUSTER_ATTACH sequence.
 */
#define A210_NCORE_ACE1			_UL(0x6f801000)
#define A210_XAIUTCR			0x40
#define A210_XAIUTAR			0x44
#define A210_ACE_ATTACH_REQUEST		(_UL(1) << 9)
#define A210_ACE_ATTACH_ACK		(_UL(1) << 5)
/* A software retry budget, not a hardware time unit or a delay in us. */
#define A210_ACE_ATTACH_RETRIES		1000000

/* Hardware IDs used by the vendor to distinguish the two core types. */
#define A210_MARCHID_C908			_ULL(0x8000000009140d00)
#define A210_MARCHID_C920			_ULL(0x80000000090c0d00)

/* A210 uses these CSR numbers with C908/C920 semantics, not old C900 names. */
#define A210_CSR_SMPEN			0x7f3
#define A210_CSR_MHINT4			0x7ce
#define A210_CSR_MCYCLE_EVENT		0x7e0
#define A210_CSR_MINSTRET_EVENT		0x7e1

/*
 * Preserve the vendor cache/prefetch settings, not guessed new tuning.
 * MHINT/MHINT2 configure core-specific hints; MCCR2 and MHCR configure
 * caches. Configure the hints and MCCR2, enable SMP coherency, and only
 * then enable caches through MHCR. Exact tuning fields are revision
 * dependent: the available C920 V3 manual is not the A210 C920v2 manual.
 * Source: vendor platform/generic/zhihe/a210.c at d9cfcff67e68.
 */
#define A210_C908_MHINT_INIT		0x0212a10c
#define A210_C920_MHINT_INIT		0x0316a32c
#define A210_C908_MCCR2_INIT		0xa2490008
#define A210_C920_MCCR2_INIT		0xe2490009
#define A210_C908_MHCR_INIT		0x010011ff
#define A210_C920_MHCR_INIT		0x000011ff
/* Vendor JTAG context-capture enable differs between C908 and C920v2. */
#define A210_C908_MHINT2_DEBUG		(_ULL(1) << 33)
#define A210_C920_MHINT2_INIT		(0x180 | (_ULL(1) << 44))
/* Invalidate caches and branch-prediction state, before any C stack use. */
#define A210_MCOR_RESET_INIT		0x70013
#define A210_SMP_ENABLE			1
/* Vendor MXSTATUS setup, including the T-Head extended execution mode. */
#define A210_MXSTATUS_INIT		0x438000
/* Vendor enables the L2 free-write optimization with MHINT4 bit 13. */
#define A210_MHINT4_L2_FREE_WRITE		(_UL(1) << 13)

/*
 * A210 PMU uses OF bits in event selectors, including the private cycle
 * and instret selectors above. Counter IDs 0 and 2 are architectural;
 * the vendor callback covers programmable counters 3 through 18.
 */
#define A210_CYCLE_COUNTER		0
#define A210_INSTRET_COUNTER		2
#define A210_FIRST_HPM_COUNTER		3
#define A210_LAST_HPM_COUNTER		18
#define A210_EVENT_OVERFLOW		(_UL(1) << 63)
#define A210_MXSTATUS_COUNTER_OVERFLOW	(_UL(1) << 8)
#define A210_COUNTER_WRITE_ENABLE_ALL	_UL(0xffffffff)

#ifndef __ASSEMBLER__
int a210_hsm_init(void);
/* Filled by hart_start before release, consumed by the stackless entry. */
extern unsigned long a210_hsm_start_addr[A210_HART_COUNT];
#endif

#endif
