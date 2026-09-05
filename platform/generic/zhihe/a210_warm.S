/* SPDX-License-Identifier: BSD-2-Clause */
#include <sbi/riscv_encoding.h>
#include <sbi/riscv_asm.h>
#include <zhihe/a210.h>

	.section .text
	.balign 64
	.globl a210_start_warm
a210_start_warm:
	/* No C code or stack accesses until cache/coherency setup is complete. */
	li t1, A210_MCOR_RESET_INIT
	csrw THEAD_C9XX_CSR_MCOR, t1
	fence rw, rw
	csrr t0, CSR_MARCHID
	li t1, A210_MARCHID_C908
	beq t0, t1, .Lc908
	li t1, A210_MARCHID_C920
	beq t0, t1, .Lc920
.Lunsupported:
	wfi
	j .Lunsupported
.Lc908:
	li t1, A210_C908_MHINT_INIT
	csrw THEAD_C9XX_CSR_MHINT, t1
	li t1, A210_C908_MHINT2_DEBUG
	csrs THEAD_C9XX_CSR_MHINT2, t1
	li t1, A210_MCOR_RESET_INIT
	csrw THEAD_C9XX_CSR_MCOR, t1
	li t1, A210_C908_MCCR2_INIT
	csrw THEAD_C9XX_CSR_MCCR2, t1
	li t1, A210_SMP_ENABLE
	csrw A210_CSR_SMPEN, t1
	li t1, A210_C908_MHCR_INIT
	csrw THEAD_C9XX_CSR_MHCR, t1
	j .Lcommon
.Lc920:
	li t1, A210_C920_MHINT_INIT
	csrw THEAD_C9XX_CSR_MHINT, t1
	li t1, A210_C920_MHINT2_INIT
	csrw THEAD_C9XX_CSR_MHINT2, t1
	li t1, A210_MCOR_RESET_INIT
	csrw THEAD_C9XX_CSR_MCOR, t1
	li t1, A210_C920_MCCR2_INIT
	csrw THEAD_C9XX_CSR_MCCR2, t1
	li t1, A210_SMP_ENABLE
	csrw A210_CSR_SMPEN, t1
	li t1, A210_C920_MHCR_INIT
	csrw THEAD_C9XX_CSR_MHCR, t1
.Lcommon:
	li t1, A210_MXSTATUS_INIT
	csrw THEAD_C9XX_CSR_MXSTATUS, t1
	li t1, A210_MHINT4_L2_FREE_WRITE
	csrs A210_CSR_MHINT4, t1
	fence iorw, iorw
	fence.i
	/* Caches/coherency are ready: obtain the address published by HSM. */
	csrr t0, CSR_MHARTID
	li t1, A210_HART_COUNT
	bgeu t0, t1, .Lunsupported
	slli t0, t0, LGREG /* One machine-word target per hart. */
	lla t1, a210_hsm_start_addr
	add t1, t1, t0
	REG_L t1, 0(t1)
	beqz t1, .Lunsupported
	jr t1
