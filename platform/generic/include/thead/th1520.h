/* SPDX-License-Identifier: BSD-2-Clause */
#ifndef __THEAD_TH1520_H__
#define __THEAD_TH1520_H__

#define TH1520_HART_COUNT		4
#define TH1520_C910_RESET		0xffef014004
#define TH1520_C910_CLUSTER_RST_N	0x1
#define TH1520_C910_CORE_RST_N(hart)	(1U << ((hart) + 1))
#define TH1520_C910_RESET_MASK		0x1f
#define TH1520_CORE_START(hart)		(0xffef018050 + 8 * (hart))

/* C910 cache settings used by the TH1520 SPL reset entry. */
#define TH1520_MCOR_INIT			0x70013
#define TH1520_MXSTATUS_INIT		0x638000
#define TH1520_MHINT_INIT		0x66e30c
#define TH1520_MHCR_INIT			0x17f
#define TH1520_MHINT2_INIT		0x420000
#define TH1520_MHINT4_INIT		0x410
#define TH1520_CSR_MHINT4		0x7ce

#ifndef __ASSEMBLER__
#if __riscv_xlen == 64
int th1520_hsm_init(void);
#else
static inline int th1520_hsm_init(void)
{
	return 0;
}
#endif
#endif

#endif
