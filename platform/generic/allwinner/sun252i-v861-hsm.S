/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2026 Han Gao <gaohan@iscas.ac.cn>
 */

#include <thead/c9xx_encoding.h>
#include <sbi/riscv_asm.h>

	.section .text
	.align 3
	.globl sun252i_v861_secondary_entry
sun252i_v861_secondary_entry:
	/* Start a reset core with clean local caches and coherency enabled. */
	li	t0, 0x70013
	csrw	THEAD_C9XX_CSR_MCOR, t0
	li	t0, 1
	csrw	THEAD_C9XX_CSR_MSMPR, t0
	fence.i

	/* Each target entry occupies one native word. */
	csrr	t0, CSR_MHARTID
	slli	t0, t0, LGREG
	lla	t1, sun252i_v861_secondary_target
	add	t1, t1, t0
	REG_L	t0, 0(t1)
	jr	t0
