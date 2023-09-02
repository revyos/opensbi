/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Authors:
 *   Inochi Amaoto <inochiama@outlook.com>
 *
 */

#include <platform_override.h>
#include <sbi/riscv_barrier.h>
#include <sbi/sbi_const.h>
#include <sbi/sbi_platform.h>
#include <sbi/sbi_scratch.h>
#include <sbi/sbi_string.h>
#include <sbi_utils/fdt/fdt_helper.h>

#define THEAD_QUIRK_SFENCE_FIXUP		BIT(0)

#define thead_get_symbol_local_address(_name)				\
	({								\
		register unsigned long __v;				\
		__asm__ __volatile__ ("lla %0, " STRINGIFY(_name)	\
				     : "=r"(__v)			\
				     :					\
				     : "memory");			\
		(void*)__v;						\
	})

extern const unsigned int thead_patch_sfence_size;

static void thead_fixup_sfence(void)
{
	void* trap_addr = thead_get_symbol_local_address(_trap_handler);
	void* patch_addr = thead_get_symbol_local_address(thead_patch_sfence);

	sbi_memcpy(trap_addr, patch_addr, thead_patch_sfence_size);
	RISCV_FENCE_I;
}

static int thead_generic_early_init(bool cold_boot, const struct fdt_match *match)
{
	unsigned long quirks = (unsigned long)match->data;

	if (cold_boot) {
		if (quirks & THEAD_QUIRK_SFENCE_FIXUP) {
			thead_fixup_sfence();
		}
	}

	return 0;
}

static const struct fdt_match thead_generic_match[] = {
	{ .compatible = "thead,th1520",
	  .data = (void*)THEAD_QUIRK_SFENCE_FIXUP },
	{ .compatible = "thead,light",
	  .data = (void*)THEAD_QUIRK_SFENCE_FIXUP }, /* Compatible with thead vendor kernel  */
	{ },
};

const struct platform_override thead_generic = {
	.match_table	= thead_generic_match,
	.early_init	= thead_generic_early_init,
};
