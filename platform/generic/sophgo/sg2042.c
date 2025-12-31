/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Authors:
 *   Inochi Amaoto <inochiama@outlook.com>
 *
 */
#include <libfdt.h>
#include <platform_override.h>
#include <thead/c9xx_errata.h>
#include <thead/c9xx_pmu.h>
#include <sbi/sbi_const.h>
#include <sbi/sbi_domain.h>
#include <sbi/sbi_platform.h>
#include <sbi/sbi_scratch.h>
#include <sbi/sbi_string.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi_utils/timer/aclint_mtimer.h>

#define SOPHGO_SG2042_TIMER_BASE	0x70ac000000ULL
#define SOPHGO_SG2042_TIMER_SIZE	0x10000UL

extern struct sbi_platform platform;
static u32 selected_hartid = -1;
static bool force_emulate_time_csr;

static bool mango_cold_boot_allowed(u32 hartid)
{
        if (selected_hartid != -1)
                return (selected_hartid == hartid);

        return (hartid < 64);
}

static int sophgo_sg2042_early_init(bool cold_boot)
{
	int rc;

	rc = generic_early_init(cold_boot);
	if (rc)
		return rc;

	thead_register_tlb_flush_trap_handler();

	/*
	 * Sophgo sg2042 soc use separate 16 timers while initiating,
	 * merge them as a single domain to avoid wasting.
	 */
	if (cold_boot)
		return sbi_domain_root_add_memrange(
					(ulong)SOPHGO_SG2042_TIMER_BASE,
					SOPHGO_SG2042_TIMER_SIZE *
					sbi_platform_hart_count(&platform),
					MTIMER_REGION_ALIGN,
					(SBI_DOMAIN_MEMREGION_MMIO |
					 SBI_DOMAIN_MEMREGION_M_READABLE |
					 SBI_DOMAIN_MEMREGION_M_WRITABLE));


	return 0;
}

static int sophgo_sg2042_extensions_init(struct sbi_hart_features *hfeatures)
{
	int rc;

	rc = generic_extensions_init(hfeatures);
	if (rc)
		return rc;

	thead_c9xx_register_pmu_device();
	hfeatures->mhpm_mask = 0x0003e3f8;
	hfeatures->mhpm_bits = 64;

	return 0;
}

static bool mango_force_emulate_time_csr(void)
{
	return force_emulate_time_csr;
}

static u32 mango_tlb_num_entries(void)
{
	return 128;
}

static int mango_final_init(bool cold_boot)
{
	int noff;
	void *fdt;

	if (!cold_boot)
		return 0;

	fdt = fdt_get_address_rw();

	noff = fdt_node_offset_by_compatible(fdt, 0, "mango,reset");
	if (noff > 0)
		fdt_del_node(fdt, noff);

	noff = fdt_node_offset_by_compatible(fdt, 0, "mango,cpld-poweroff");
	if (noff > 0)
		fdt_del_node(fdt, noff);

	noff = fdt_node_offset_by_compatible(fdt, 0, "mango,cpld-reboot");
	if (noff > 0)
		fdt_del_node(fdt, noff);

	return generic_final_init(cold_boot);
}

static int sophgo_sg2042_platform_init(const void *fdt, int nodeoff, const struct fdt_match *match)
{
	if (platform.hart_stack_size < 16384)
		platform.hart_stack_size = 16384;

	if (fdt_node_offset_by_compatible(fdt, 0, "sophgo,sg2042-global-mtimer") > 0)
		force_emulate_time_csr = true;
	else
		force_emulate_time_csr = false;

	generic_platform_ops.early_init = sophgo_sg2042_early_init;
	generic_platform_ops.extensions_init = sophgo_sg2042_extensions_init;
	generic_platform_ops.cold_boot_allowed = mango_cold_boot_allowed;
	generic_platform_ops.force_emulate_time_csr = mango_force_emulate_time_csr;
	generic_platform_ops.get_tlb_num_entries = mango_tlb_num_entries;
	generic_platform_ops.final_init = mango_final_init;

	return 0;
}


static const struct fdt_match sophgo_sg2042_match[] = {
	{ .compatible = "sophgo,sg2042" },
	{ },
};

const struct fdt_driver sophgo_sg2042 = {
	.match_table		= sophgo_sg2042_match,
	.init			= sophgo_sg2042_platform_init,
};
