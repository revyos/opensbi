// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (C) 2021-2024 Alibaba Group Holding Limited.
 *
 * Authors:
 *   XiaoJin Chai <xiaojin.cxj@alibaba-inc.com>
 */

#include <libfdt.h>
#include <sbi/sbi_error.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi_utils/mbox/fdt_mbox.h>

/* List of FDT MBOX adapter drivers generated at compile time */
extern struct fdt_mbox_adapter *fdt_mbox_adapter_drivers[];
extern unsigned long fdt_mbox_adapter_drivers_size;

static int fdt_mbox_adapter_init(void *fdt, int nodeoff)
{
	int pos, rc;
	struct fdt_mbox_adapter *drv;
	const struct fdt_match *match;

	/* Try all MBOX drivers one-by-one */
	for (pos = 0; pos < fdt_mbox_adapter_drivers_size; pos++) {
		drv   = fdt_mbox_adapter_drivers[pos];
		match = fdt_match_node(fdt, nodeoff, drv->match_table);
		if (match && drv->init) {
			rc = drv->init(fdt, nodeoff, match);
			if (rc == SBI_ENODEV)
				continue;
			if (rc)
				return rc;
			return 0;
		}
	}

	return SBI_ENOSYS;
}

static int fdt_mbox_adapter_find(void *fdt, int nodeoff,
				 struct mbox_adapter **out_adapter)
{
	int rc;
	struct mbox_adapter *adapter = mbox_adapter_find(nodeoff);

	if (!adapter) {
		/* MBOX adapter not found so initialize matching driver */
		rc = fdt_mbox_adapter_init(fdt, nodeoff);
		if (rc)
			return rc;

		/* Try to find MBOX adapter again */
		adapter = mbox_adapter_find(nodeoff);
		if (!adapter)
			return SBI_ENOSYS;
	}

	if (out_adapter)
		*out_adapter = adapter;

	return 0;
}

/*API for user*/
int fdt_mbox_adapter_get(void *fdt, int nodeoff,
			 struct mbox_adapter **out_adapter)
{
	int rc;
	struct mbox_adapter *adapter;

	if (!fdt || (nodeoff < 0) || !out_adapter)
		return SBI_EINVAL;

	rc = fdt_mbox_adapter_find(fdt, nodeoff, &adapter);
	if (rc)
		return rc;

	*out_adapter = adapter;

	return 0;
}
