// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 Han Gao <gaohan@iscas.ac.cn>
 */

#include <platform_override.h>
#include <thead/c9xx_encoding.h>
#include <thead/c9xx_pmu.h>
#include <sbi/riscv_asm.h>
#include <sbi/riscv_barrier.h>
#include <sbi/riscv_io.h>
#include <sbi/sbi_bitops.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_hart.h>
#include <sbi/sbi_hsm.h>
#include <sbi/sbi_pmu.h>
#include <sbi/sbi_scratch.h>

#define SUN252I_V861_CCU_BASE		((void *)0x02001000)
#define SUN252I_V861_RISCV_CFG_BGR		0x50c
#define SUN252I_V861_BGR_ENABLE		(BIT(16) | BIT(0))

#define SUN252I_V861_PMC_BASE		((void *)0x08000000)
#define SUN252I_V861_PMC_DOMAIN(n)		(SUN252I_V861_PMC_BASE + 0x1000 * (n))
#define SUN252I_V861_PMC_SW_MODE(n)		(0x100 + 0x4 * (n))
#define SUN252I_V861_PMC_TIMER_MODE		0x600
#define SUN252I_V861_PMC_DELAY_CTRL		0x700

#define SUN252I_V861_RISCV_CFG_BASE		((void *)0x08008000)
#define SUN252I_V861_RESET_ENTRY_LO(n)	(0x100 + 0x80 * (n))
#define SUN252I_V861_RESET_ENTRY_HI(n)	(0x104 + 0x80 * (n))
#define SUN252I_V861_CORE_CFG(n)		(0x108 + 0x80 * (n))
#define SUN252I_V861_CORE_MODE_RV32		BIT(8)
#define SUN252I_V861_CORE_MODE_RV64		BIT(9)

#define SUN252I_V861_HART_COUNT		2

extern void sun252i_v861_secondary_entry(void);
unsigned long sun252i_v861_secondary_target[SUN252I_V861_HART_COUNT];

static void sun252i_v861_set_hart_entry(u32 hartid, unsigned long entry)
{
	void *cfg = SUN252I_V861_RISCV_CFG_BASE + SUN252I_V861_CORE_CFG(hartid);
	u32 value;

	value = readl(cfg);
	value &= ~(SUN252I_V861_CORE_MODE_RV32 | SUN252I_V861_CORE_MODE_RV64);
	value |= __riscv_xlen == 64 ? SUN252I_V861_CORE_MODE_RV64 :
				    SUN252I_V861_CORE_MODE_RV32;
	writel(value, cfg);
	writel(entry, SUN252I_V861_RISCV_CFG_BASE +
	       SUN252I_V861_RESET_ENTRY_LO(hartid));
	writel((u64)entry >> 32, SUN252I_V861_RISCV_CFG_BASE +
	       SUN252I_V861_RESET_ENTRY_HI(hartid));
}

static void sun252i_v861_pmc_init(void)
{
	unsigned int domain;

	/* Allow the cluster to enter its automatic low-power mode. */
	writel(0, SUN252I_V861_PMC_DOMAIN(4) + SUN252I_V861_PMC_SW_MODE(0));
	writel(0, SUN252I_V861_PMC_DOMAIN(4) + SUN252I_V861_PMC_SW_MODE(1));

	for (domain = 0; domain < SUN252I_V861_HART_COUNT; domain++) {
		writel(1, SUN252I_V861_PMC_DOMAIN(domain) +
		       SUN252I_V861_PMC_TIMER_MODE);
		writel(0x01010100, SUN252I_V861_PMC_DOMAIN(domain) +
		       SUN252I_V861_PMC_DELAY_CTRL);
	}

	writel(1, SUN252I_V861_PMC_DOMAIN(4) + SUN252I_V861_PMC_TIMER_MODE);
	writel(0x01010100, SUN252I_V861_PMC_DOMAIN(4) +
	       SUN252I_V861_PMC_DELAY_CTRL);
}

static void sun252i_v861_riscv_cfg_init(void)
{
	unsigned long entry = sbi_scratch_thishart_ptr()->warmboot_addr;

	writel(SUN252I_V861_BGR_ENABLE,
	       SUN252I_V861_CCU_BASE + SUN252I_V861_RISCV_CFG_BGR);
	sun252i_v861_set_hart_entry(current_hartid(), entry);
	csr_set(THEAD_C9XX_CSR_MHINT4, BIT(7));
	sun252i_v861_pmc_init();
}

static int sun252i_v861_hart_start(u32 hartid, unsigned long saddr)
{
	if (hartid >= SUN252I_V861_HART_COUNT)
		return SBI_EINVAL;

	sun252i_v861_secondary_target[hartid] = saddr;
	/* Publish the entry point before releasing the target core. */
	smp_wmb();
	sun252i_v861_set_hart_entry(hartid,
				    (unsigned long)sun252i_v861_secondary_entry);

	/* Select manual mode and request power-on for the target core. */
	writel(0x3 << 3,
	       SUN252I_V861_PMC_DOMAIN(hartid) + SUN252I_V861_PMC_SW_MODE(0));
	writel(0x11,
	       SUN252I_V861_PMC_DOMAIN(hartid) + SUN252I_V861_PMC_SW_MODE(1));

	return 0;
}

static int sun252i_v861_hart_stop(void)
{
	u32 hartid = current_hartid();

	if (hartid >= SUN252I_V861_HART_COUNT)
		return SBI_EINVAL;

	/* Clean caches before removing this core from the coherency domain. */
	csr_write(THEAD_C9XX_CSR_MCOR, 0x22);
	csr_clear(THEAD_C9XX_CSR_MHCR, BIT(1));
	csr_clear(THEAD_C9XX_CSR_MSMPR, BIT(0));

	writel(0x2 << 3,
	       SUN252I_V861_PMC_DOMAIN(hartid) + SUN252I_V861_PMC_SW_MODE(0));
	writel(0x2 << 3,
	       SUN252I_V861_PMC_DOMAIN(hartid) + SUN252I_V861_PMC_SW_MODE(1));
	wfi();

	return 0;
}

static const struct sbi_hsm_device sun252i_v861_hsm = {
	.name = "sun252i-pmc",
	.hart_start = sun252i_v861_hart_start,
	.hart_stop = sun252i_v861_hart_stop,
};

static int sun252i_v861_final_init(bool cold_boot)
{
	if (cold_boot) {
		sun252i_v861_riscv_cfg_init();
		sbi_hsm_set_device(&sun252i_v861_hsm);
	}

	return generic_final_init(cold_boot);
}

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
	generic_platform_ops.final_init = sun252i_v861_final_init;
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
