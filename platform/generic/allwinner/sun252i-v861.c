// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 Han Gao <gaohan@iscas.ac.cn>
 */

#include <platform_override.h>
#include <thead/c9xx_encoding.h>
#include <thead/c9xx_pmu.h>
#include <sbi/riscv_asm.h>
#include <sbi/sbi_bitops.h>
#include <sbi/sbi_hart.h>
#include <sbi/sbi_pmu.h>
#include <sbi/sbi_scratch.h>

static int sun252i_v861_extensions_init(bool cold_boot)
{
	struct sbi_hart_features *hfeatures;
	int rc;

	rc = generic_extensions_init(cold_boot);
	if (rc)
		return rc;

	thead_c9xx_register_pmu_device();

	/* Auto-detection is incomplete on the T-Head C907. */
	hfeatures = sbi_hart_features_ptr(sbi_scratch_thishart_ptr());
	hfeatures->mhpm_mask = 0x0003e3f8;
	hfeatures->mhpm_bits = 64;

	/* Enable cache coherency and the C907 implementation extensions. */
	csr_write(THEAD_C9XX_CSR_MSMPR, BIT(0));
	csr_write(THEAD_C9XX_CSR_MCCR2, 0xa0420002);
	csr_write(THEAD_C9XX_CSR_MXSTATUS, 0x438000);
	csr_write(THEAD_C9XX_CSR_MHINT, 0x3a1aa10c);
	csr_write(THEAD_C9XX_CSR_MHCR, 0x10011bf);

	return 0;
}

static int sun252i_v861_platform_init(const void *fdt, int nodeoff,
				   const struct fdt_match *match)
{
	generic_platform_ops.extensions_init = sun252i_v861_extensions_init;

	return 0;
}

static const struct fdt_match sun252i_v861_match[] = {
	{ .compatible = "avaota,avaota-f2" },
	{ .compatible = "allwinner,sun252i-v861" },
	{ },
};

const struct fdt_driver sun252i_v861 = {
	.match_table = sun252i_v861_match,
	.init = sun252i_v861_platform_init,
};
