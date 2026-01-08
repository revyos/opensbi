/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Authors:
 *   Inochi Amaoto <inochiama@outlook.com>
 *
 */

#include <platform_override.h>
#include <thead/c9xx_errata.h>
#include <thead/c9xx_pmu.h>
#include <sbi/sbi_const.h>
#include <sbi/sbi_platform.h>
#include <sbi/sbi_scratch.h>
#include <sbi/sbi_string.h>
#include <sbi/sbi_tlb.h>
#include <sbi_utils/fdt/fdt_helper.h>

struct thead_generic_quirks {
	u64	errata;
};

static int thead_tlb_flush_early_init(bool cold_boot)
{
	thead_register_tlb_flush_trap_handler();

	return generic_early_init(cold_boot);
}

static int thead_pmu_extensions_init(struct sbi_hart_features *hfeatures)
{
	int rc;

	rc = generic_extensions_init(hfeatures);
	if (rc)
		return rc;

	thead_c9xx_register_pmu_device();

	return 0;
}

static void thead_jtlb_local_sfence_vma(struct sbi_tlb_info *tinfo)
{
	__asm__ __volatile__("sfence.vma");
}

static void thead_jtlb_local_hfence_vvma_asid(struct sbi_tlb_info *tinfo)
{
	unsigned long start = tinfo->start;
	unsigned long size  = tinfo->size;
	unsigned long asid  = tinfo->asid;

	/* Flush entire MM context for a given ASID */
	if ((start == 0 && size == 0) || (size == SBI_TLB_FLUSH_ALL)) {
		__asm__ __volatile__("sfence.vma x0, %0"
				     :
				     : "r"(asid)
				     : "memory");
		return;
	}

	__asm__ __volatile__("sfence.vma x0, %0"
			     :
			     : "r"(asid)
			     : "memory");
}

static int thead_generic_platform_init(const void *fdt, int nodeoff,
				       const struct fdt_match *match)
{
	const struct thead_generic_quirks *quirks = match->data;

	if (quirks->errata & THEAD_QUIRK_ERRATA_TLB_FLUSH)
		generic_platform_ops.early_init = thead_tlb_flush_early_init;
	if (quirks->errata & THEAD_QUIRK_ERRATA_THEAD_PMU)
		generic_platform_ops.extensions_init = thead_pmu_extensions_init;

	if (quirks->errata &THEAD_QUIRK_ERRATA_JTLB) {
		generic_platform_ops.local_sfence_vma = thead_jtlb_local_sfence_vma;
		generic_platform_ops.local_sfence_vma_asid = thead_jtlb_local_hfence_vvma_asid;
	}

	return 0;
}

static const struct thead_generic_quirks thead_th1520_quirks = {
	.errata = THEAD_QUIRK_ERRATA_TLB_FLUSH | THEAD_QUIRK_ERRATA_THEAD_PMU,
};

static const struct thead_generic_quirks thead_pmu_quirks = {
	.errata = THEAD_QUIRK_ERRATA_THEAD_PMU,
};

static const struct thead_generic_quirks thead_pmu_jtlb_quirks = {
	.errata = THEAD_QUIRK_ERRATA_THEAD_PMU | THEAD_QUIRK_ERRATA_JTLB,
};

static const struct fdt_match thead_generic_match[] = {
	{ .compatible = "canaan,kendryte-k230", .data = &thead_pmu_quirks },
	{ .compatible = "sophgo,cv1800b", .data = &thead_pmu_quirks },
	{ .compatible = "sophgo,cv1812h", .data = &thead_pmu_quirks },
	{ .compatible = "sophgo,sg2000", .data = &thead_pmu_quirks },
	{ .compatible = "sophgo,sg2002", .data = &thead_pmu_quirks },
	{ .compatible = "sophgo,sg2044", .data = &thead_pmu_jtlb_quirks },
	{ .compatible = "thead,th1520", .data = &thead_th1520_quirks },
	{ },
};

const struct fdt_driver thead_generic = {
	.match_table		= thead_generic_match,
	.init			= thead_generic_platform_init,
};
