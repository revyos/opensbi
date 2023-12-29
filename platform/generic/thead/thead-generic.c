/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Authors:
 *   Inochi Amaoto <inochiama@outlook.com>
 *
 */

#include <platform_override.h>
#include <thead/c9xx_encoding.h>
#include <thead/c9xx_errata.h>
#include <thead/c9xx_pmu.h>
#include <sbi/sbi_const.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_platform.h>
#include <sbi/sbi_scratch.h>
#include <sbi/sbi_string.h>
#include <sbi/sbi_hsm.h>
#include <sbi_utils/fdt/fdt_helper.h>

extern const struct sbi_hsm_device light_ppu;
extern struct sbi_ecall_extension ecall_light;
struct thead_generic_quirks {
	u64	errata;
};

static int thead_generic_early_init(bool cold_boot,
				    const struct fdt_match *match)
{
	struct thead_generic_quirks *quirks = (void *)match->data;

	if (quirks->errata & THEAD_QUIRK_ERRATA_TLB_FLUSH)
		thead_register_tlb_flush_trap_handler();

	return 0;
}

static int thead_generic_extensions_init(const struct fdt_match *match,
					 struct sbi_hart_features *hfeatures)
{
	thead_c9xx_register_pmu_device();
	return 0;
}


static void sbi_thead_pmu_init(void)
{
	unsigned long interrupts;

	interrupts = csr_read(CSR_MIDELEG) | (1 << 17);
	csr_write(CSR_MIDELEG, interrupts);

	/* THEAD_C9XX_CSR_MCOUNTERWEN has already been set in mstatus_init() */
	csr_write(THEAD_C9XX_CSR_MCOUNTERWEN, 0xffffffff);
	csr_write(CSR_MHPMEVENT3, 1);
	csr_write(CSR_MHPMEVENT4, 2);
	csr_write(CSR_MHPMEVENT5, 3);
	csr_write(CSR_MHPMEVENT6, 4);
	csr_write(CSR_MHPMEVENT7, 5);
	csr_write(CSR_MHPMEVENT8, 6);
	csr_write(CSR_MHPMEVENT9, 7);
	csr_write(CSR_MHPMEVENT10, 8);
	csr_write(CSR_MHPMEVENT11, 9);
	csr_write(CSR_MHPMEVENT12, 10);
	csr_write(CSR_MHPMEVENT13, 11);
	csr_write(CSR_MHPMEVENT14, 12);
	csr_write(CSR_MHPMEVENT15, 13);
	csr_write(CSR_MHPMEVENT16, 14);
	csr_write(CSR_MHPMEVENT17, 15);
	csr_write(CSR_MHPMEVENT18, 16);
	csr_write(CSR_MHPMEVENT19, 17);
	csr_write(CSR_MHPMEVENT20, 18);
	csr_write(CSR_MHPMEVENT21, 19);
	csr_write(CSR_MHPMEVENT22, 20);
	csr_write(CSR_MHPMEVENT23, 21);
	csr_write(CSR_MHPMEVENT24, 22);
	csr_write(CSR_MHPMEVENT25, 23);
	csr_write(CSR_MHPMEVENT26, 24);
	csr_write(CSR_MHPMEVENT27, 25);
	csr_write(CSR_MHPMEVENT28, 26);
}

static void sbi_thead_pmu_map(unsigned long idx, unsigned long event_id)
{
	switch (idx) {
	case 3:
		csr_write(CSR_MHPMEVENT3, event_id);
		break;
	case 4:
		csr_write(CSR_MHPMEVENT4, event_id);
		break;
	case 5:
		csr_write(CSR_MHPMEVENT5, event_id);
		break;
	case 6:
		csr_write(CSR_MHPMEVENT6, event_id);
		break;
	case 7:
		csr_write(CSR_MHPMEVENT7, event_id);
		break;
	case 8:
		csr_write(CSR_MHPMEVENT8, event_id);
		break;
	case 9:
		csr_write(CSR_MHPMEVENT9, event_id);
		break;
	case 10:
		csr_write(CSR_MHPMEVENT10, event_id);
		break;
	case 11:
		csr_write(CSR_MHPMEVENT11, event_id);
		break;
	case 12:
		csr_write(CSR_MHPMEVENT12, event_id);
		break;
	case 13:
		csr_write(CSR_MHPMEVENT13, event_id);
		break;
	case 14:
		csr_write(CSR_MHPMEVENT14, event_id);
		break;
	case 15:
		csr_write(CSR_MHPMEVENT15, event_id);
		break;
	case 16:
		csr_write(CSR_MHPMEVENT16, event_id);
		break;
	case 17:
		csr_write(CSR_MHPMEVENT17, event_id);
		break;
	case 18:
		csr_write(CSR_MHPMEVENT18, event_id);
		break;
	case 19:
		csr_write(CSR_MHPMEVENT19, event_id);
		break;
	case 20:
		csr_write(CSR_MHPMEVENT20, event_id);
		break;
	case 21:
		csr_write(CSR_MHPMEVENT21, event_id);
		break;
	case 22:
		csr_write(CSR_MHPMEVENT22, event_id);
		break;
	case 23:
		csr_write(CSR_MHPMEVENT23, event_id);
		break;
	case 24:
		csr_write(CSR_MHPMEVENT24, event_id);
		break;
	case 25:
		csr_write(CSR_MHPMEVENT25, event_id);
		break;
	case 26:
		csr_write(CSR_MHPMEVENT26, event_id);
		break;
	case 27:
		csr_write(CSR_MHPMEVENT27, event_id);
		break;
	case 28:
		csr_write(CSR_MHPMEVENT28, event_id);
		break;
	case 29:
		csr_write(CSR_MHPMEVENT29, event_id);
		break;
	case 30:
		csr_write(CSR_MHPMEVENT30, event_id);
		break;
	case 31:
		csr_write(CSR_MHPMEVENT31, event_id);
		break;
	}
}

static void sbi_thead_pmu_set(unsigned long type, unsigned long idx, unsigned long event_id)
{
	switch (type) {
	case 2:
		sbi_thead_pmu_map(idx, event_id);
		break;
	default:
		sbi_thead_pmu_init();
		break;
	}
}

static int thead_vendor_ext_provider(long funcid,
				   struct sbi_trap_regs *regs,
				   struct sbi_ecall_return *out,
				   const struct fdt_match *match)
{
	sbi_thead_pmu_set(regs->a0, regs->a1, regs->a2);
	return 0;
}


static int thead_generic_final_init(bool cold_boot,
	 const struct fdt_match *match)
{
	struct thead_generic_quirks *quirks = (void *)match->data;

	if (cold_boot) {
		if (quirks->errata & THEAD_QUIRK_ERRATA_LOGHT_PPU) {
			sbi_printf("core:%d %s: line:%d enter. cold_boot:%d\n",
				 current_hartid(), __func__, __LINE__, cold_boot);
			sbi_hsm_set_device(&light_ppu);
		}
		if (!sbi_strcmp(match->compatible, "thead,light"))
			sbi_ecall_register_extension(&ecall_light);
	}

	return 0;
}

/* static struct thead_generic_quirks thead_th1520_quirks = {
	.errata = THEAD_QUIRK_ERRATA_TLB_FLUSH,
}; */

static struct thead_generic_quirks thead_light_quirks = {
	.errata = THEAD_QUIRK_ERRATA_TLB_FLUSH | THEAD_QUIRK_ERRATA_LOGHT_PPU,
};

static const struct fdt_match thead_generic_match[] = {
/*	{ .compatible = "thead,th1520", .data = &thead_th1520_quirks }, */
	{ .compatible = "thead,th1520", .data = &thead_light_quirks },
        { .compatible = "thead,light", .data = &thead_light_quirks },
	{ },
};

const struct platform_override thead_generic = {
	.match_table		= thead_generic_match,
	.early_init		= thead_generic_early_init,
	.extensions_init	= thead_generic_extensions_init,
	.vendor_ext_provider = thead_vendor_ext_provider,
	.final_init	= thead_generic_final_init
};
