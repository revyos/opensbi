// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (C) 2021-2024 Alibaba Group Holding Limited.
 *
 * Authors:
 *   XiaoJin Chai <xiaojin.cxj@alibaba-inc.com>
 */

#ifndef __FDT_MBOX_H__
#define __FDT_MBOX_H__

#include <sbi_utils/mbox/mbox.h>

/** FDT based MBOX adapter driver */
struct fdt_mbox_adapter {
	const struct fdt_match *match_table;
	int (*init)(const void *fdt, int nodeoff, const struct fdt_match *match);
};

/** Get MBOX adapter identified by nodeoff */
int fdt_mbox_adapter_get(const void *fdt, int nodeoff,
			 struct mbox_adapter **out_adapter);

#endif
