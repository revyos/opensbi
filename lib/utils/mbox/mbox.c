// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (C) 2021-2024 Alibaba Group Holding Limited.
 *
 * Authors:
 *   XiaoJin Chai <xiaojin.cxj@alibaba-inc.com>
 */

#include <sbi/sbi_error.h>
#include <sbi_utils/mbox/mbox.h>

static SBI_LIST_HEAD(mbox_adapters_list);

struct mbox_adapter *mbox_adapter_find(int id)
{
	struct sbi_dlist *pos;

	sbi_list_for_each(pos, &(mbox_adapters_list))
	{
		struct mbox_adapter *adap = to_mbox_adapter(pos);

		if (adap->id == id)
			return adap;
	}

	return NULL;
}

/* API for fdt_mbox.c and fdt vender, such as fdt_mbox_thead.c*/
int mbox_adapter_add(struct mbox_adapter *ia)
{
	if (!ia)
		return SBI_EINVAL;

	if (mbox_adapter_find(ia->id))
		return SBI_EALREADY;

	sbi_list_add(&(ia->node), &(mbox_adapters_list));

	return 0;
}

void mbox_adapter_remove(struct mbox_adapter *ia)
{
	if (!ia)
		return;

	sbi_list_del(&(ia->node));
}

/*API for user*/
int mbox_adapter_write(struct mbox_adapter *ia, uint8_t remote_channel,
		       uint8_t *buffer, int len)
{
	if (!ia)
		return SBI_EINVAL;
	if (!ia->write)
		return SBI_ENOSYS;

	return ia->write(ia, remote_channel, buffer, len);
}

int mbox_adapter_read(struct mbox_adapter *ia, uint8_t *src_channel,
		      uint8_t *buffer, int *len, int timeout_us)
{
	if (!ia)
		return SBI_EINVAL;
	if (!ia->read)
		return SBI_ENOSYS;

	return ia->read(ia, src_channel, buffer, len, timeout_us);
}
