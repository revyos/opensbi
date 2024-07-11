// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (C) 2021-2024 Alibaba Group Holding Limited.
 *
 * Authors:
 *   XiaoJin Chai <xiaojin.cxj@alibaba-inc.com>
 */

#ifndef __MBOX_H__
#define __MBOX_H__

#include <sbi/sbi_types.h>
#include <sbi/sbi_list.h>

/** Representation of a Mbox adapter */
struct mbox_adapter {
	/** Unique ID of the  adapter assigned by the driver */
	int id;

	/**
	 * Send buffer to given channel
	 *
	 * @return 0 on success and negative error code on failure
	 */
	int (*write)(struct mbox_adapter *ia, uint8_t remote_channel,
		     uint8_t *buffer, int len);

	/**
	 * Read buffer from given address, register
	 *
	 * @return 0 on success and negative error code on failure
	 */
	int (*read)(struct mbox_adapter *ia, uint8_t *src_channel,
		    uint8_t *buffer, int *len, int timeout_us);

	/** List */
	struct sbi_dlist node;
};

static inline struct mbox_adapter *to_mbox_adapter(struct sbi_dlist *node)
{
	return container_of(node, struct mbox_adapter, node);
}

/** Find a registered MBOX adapter */
struct mbox_adapter *mbox_adapter_find(int id);

/** Register MBOX adapter */
int mbox_adapter_add(struct mbox_adapter *ia);

/** Un-register MBOX adapter */
void mbox_adapter_remove(struct mbox_adapter *ia);

/** Send to device on MBOX adapter bus */
int mbox_adapter_write(struct mbox_adapter *ia, uint8_t remote_channel,
		       uint8_t *buffer, int len);

/** Read from device on MBOX adapter bus */
int mbox_adapter_read(struct mbox_adapter *ia, uint8_t *src_channel,
		      uint8_t *buffer, int *len, int timeout_us);

#endif
