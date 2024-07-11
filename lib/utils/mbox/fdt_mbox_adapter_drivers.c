// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (C) 2021-2024 Alibaba Group Holding Limited.
 *
 * Authors:
 *   XiaoJin Chai <xiaojin.cxj@alibaba-inc.com>
 */

#include <sbi_utils/mbox/fdt_mbox.h>

extern struct fdt_mbox_adapter fdt_mbox_thead;

struct fdt_mbox_adapter *fdt_mbox_adapter_drivers[] = {
	&fdt_mbox_thead,
};

unsigned long fdt_mbox_adapter_drivers_size =
	sizeof(fdt_mbox_adapter_drivers) / sizeof(struct fdt_mbox_adapter *);
