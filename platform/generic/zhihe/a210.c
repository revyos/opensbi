/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Western Digital Corporation or its affiliates.
 * Original platform framework author: Anup Patel <anup.patel@wdc.com>
 *
 * A210 DEV initialization derived from the ZhiHe vendor OpenSBI platform
 * and reset driver (d9cfcff67e68). This port uses the v1.9 generic runtime.
 */

#include <platform_override.h>
#include <zhihe/a210.h>

static bool a210_cold_boot_allowed(u32 hartid)
{
	/* Cache setup must precede the first stack access, not run here. */
	return hartid == 0;
}

static int a210_platform_init(const void *fdt, int nodeoff,
			      const struct fdt_match *match)
{
	generic_platform_ops.cold_boot_allowed = a210_cold_boot_allowed;
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
