/* SPDX-License-Identifier: BSD-2-Clause */
#include <sbi/riscv_asm.h>
#include <sbi/riscv_encoding.h>
#include <thead/c9xx_encoding.h>
#include <thead/th1520.h>

	.section .text
	.balign 64
	.globl th1520_start_warm
th1520_start_warm:
	/* Configure the reset hart without using a stack or shared data. */
	csrw CSR_MIE, zero
	li t1, TH1520_MCOR_INIT
	csrw THEAD_C9XX_CSR_MCOR, t1
	li t1, 1
	csrw THEAD_C9XX_CSR_MSMPR, t1
	/* Shared L2 configuration belongs to the boot hart in SPL. */
	li t1, TH1520_MXSTATUS_INIT
	csrw THEAD_C9XX_CSR_MXSTATUS, t1
	li t1, TH1520_MHINT_INIT
	csrw THEAD_C9XX_CSR_MHINT, t1
	li t1, TH1520_MHCR_INIT
	csrw THEAD_C9XX_CSR_MHCR, t1
	li t1, TH1520_MHINT2_INIT
	csrw THEAD_C9XX_CSR_MHINT2, t1
	li t1, TH1520_MHINT4_INIT
	csrw TH1520_CSR_MHINT4, t1
	fence iorw, iorw
	fence.i

	/* Only now may this hart observe scratch and the requested M-mode PC. */
	csrr t0, CSR_MHARTID
	li t1, TH1520_HART_COUNT
	bgeu t0, t1, .Lpark
	slli t0, t0, LGREG
	lla t1, th1520_hsm_start_addr
	add t1, t1, t0
	REG_L t1, 0(t1)
	beqz t1, .Lpark
	jr t1
.Lpark:
	wfi
	j .Lpark
