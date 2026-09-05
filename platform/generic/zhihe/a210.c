/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Western Digital Corporation or its affiliates.
 * Original platform framework author: Anup Patel <anup.patel@wdc.com>
 *
 * A210 DEV initialization derived from the ZhiHe vendor OpenSBI platform
 * and reset driver (d9cfcff67e68). This port uses the v1.9 generic runtime.
 */

#include <libfdt.h>
#include <platform_override.h>
#include <zhihe/a210.h>
#include <sbi/riscv_asm.h>
#include <sbi/riscv_io.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_hart.h>
#include <sbi/sbi_pmu.h>
#include <sbi_utils/fdt/fdt_helper.h>

static bool a210_cold_boot_allowed(u32 hartid)
{
	/* Cache setup must precede the first stack access, not run here. */
	return hartid == 0;
}

static int a210_early_init(bool cold_boot)
{
	int rc;

	rc = generic_early_init(cold_boot);

	if (rc)
		return rc;
	/* Feature detection is cached, but these CSRs need restoring on hotplug. */
	csr_set(THEAD_C9XX_CSR_MXSTATUS, A210_MXSTATUS_COUNTER_OVERFLOW);
	csr_write(THEAD_C9XX_CSR_MCOUNTERWEN, A210_COUNTER_WRITE_ENABLE_ALL);
	if (!cold_boot)
		return 0;
	return a210_hsm_init();
}

/* A210 uses SSCOFPMF-style OF bits, not the older C900 counter IRQ enables. */
static void a210_pmu_enable_irq(uint32_t idx)
{
	unsigned int event;

	if (idx == A210_CYCLE_COUNTER) {
		csr_clear(A210_CSR_MCYCLE_EVENT, A210_EVENT_OVERFLOW);
	} else if (idx == A210_INSTRET_COUNTER) {
		csr_clear(A210_CSR_MINSTRET_EVENT, A210_EVENT_OVERFLOW);
	} else if (idx >= A210_FIRST_HPM_COUNTER && idx <= A210_LAST_HPM_COUNTER) {
		event = CSR_MHPMEVENT3 + idx - A210_FIRST_HPM_COUNTER;
		csr_write_num(event, csr_read_num(event) & ~A210_EVENT_OVERFLOW);
	}
}

static const struct sbi_pmu_device a210_pmu = {
	.name = "zhihe,a210-pmu",
	.hw_counter_enable_irq = a210_pmu_enable_irq,
};

static int a210_extensions_init(bool cold_boot)
{
	int rc = generic_extensions_init(cold_boot);

	if (rc)
		return rc;
	if (cold_boot)
		sbi_pmu_set_device(&a210_pmu);
	return 0;
}

static int a210_platform_init(const void *fdt, int nodeoff,
			      const struct fdt_match *match)
{
	generic_platform_ops.cold_boot_allowed = a210_cold_boot_allowed;
	generic_platform_ops.early_init = a210_early_init;
	generic_platform_ops.extensions_init = a210_extensions_init;
	return 0;
}

static const struct fdt_match a210_match[] = {
	{ .compatible = "zhihe,a210" },
	{ },
};

const struct fdt_driver zhihe_a210 = {
	.match_table = a210_match,
	.init = a210_platform_init,
};
